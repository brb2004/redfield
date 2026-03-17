#include "jit.h"

extern "C" {
#include "common.h"
#include "value.h"
#include "chunk.h"
#include "obj.h"
#include "table.h"
#include "vm.h"
#include "memory.h"
void runtimeError(const char* format, ...);
}

using RFValue = Value;
#ifdef __cplusplus
static inline RFValue makeRFNil()           { RFValue v; v.type = VAL_NIL;    v.as.number  = 0; return v; }
static inline RFValue makeRFNumber(double n){ RFValue v; v.type = VAL_NUMBER; v.as.number  = n; return v; }
static inline RFValue makeRFBool(bool b)    { RFValue v; v.type = VAL_BOOL;   v.as.boolean = b; return v; }
#undef NIL_VAL
#undef NUMBER_VAL
#undef BOOL_VAL
#define NIL_VAL       makeRFNil()
#define NUMBER_VAL(n) makeRFNumber(n)
#define BOOL_VAL(b)   makeRFBool(b)
#endif

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <stdio.h>

using namespace llvm;

static LLVMContext* gContext = nullptr;

struct LLVMTypes {
    StructType*  rfValueType;
    Type*        doubleType;
    Type*        i32Type;
    Type*        i64Type;
    Type*        voidType;
    PointerType* i8PtrType;
};
static LLVMTypes gTypes;

static void initTypes(LLVMContext& ctx) {
    gTypes.doubleType  = Type::getDoubleTy(ctx);
    gTypes.i32Type     = Type::getInt32Ty(ctx);
    gTypes.i64Type     = Type::getInt64Ty(ctx);
    gTypes.voidType    = Type::getVoidTy(ctx);
    gTypes.i8PtrType   = PointerType::get(Type::getInt8Ty(ctx), 0);
    gTypes.rfValueType = StructType::create(ctx, "RFValue");
    gTypes.rfValueType->setBody({gTypes.i32Type, gTypes.doubleType});
}

static llvm::Value* makeIRNumber(double n) {
    return ConstantStruct::get(gTypes.rfValueType, {
        ConstantInt::get(gTypes.i32Type, VAL_NUMBER),
        ConstantFP::get(gTypes.doubleType, n)
    });
}
static llvm::Value* makeIRBool(bool v) {
    return ConstantStruct::get(gTypes.rfValueType, {
        ConstantInt::get(gTypes.i32Type, VAL_BOOL),
        ConstantFP::get(gTypes.doubleType, v ? 1.0 : 0.0)
    });
}
static llvm::Value* makeIRNil() {
    return ConstantStruct::get(gTypes.rfValueType, {
        ConstantInt::get(gTypes.i32Type, VAL_NIL),
        ConstantFP::get(gTypes.doubleType, 0.0)
    });
}
static llvm::Value* extractNum(IRBuilder<>& b, llvm::Value* val) {
    return b.CreateExtractValue(val, {1}, "num");
}
static llvm::Value* rfToIR(RFValue rfVal) {
    switch (rfVal.type) {
        case VAL_NUMBER: return makeIRNumber(rfVal.as.number);
        case VAL_BOOL:   return makeIRBool(rfVal.as.boolean);
        default:         return makeIRNil();
    }
}
static llvm::Value* objStrPtr(LLVMContext& ctx, ObjString* str) {
    return ConstantExpr::getIntToPtr(
        ConstantInt::get(Type::getInt64Ty(ctx), (uint64_t)(uintptr_t)str),
        gTypes.i8PtrType);
}

static bool canJIT(ObjFunction* function) {
    if (!function) return false;
    Chunk* chunk = &function->chunk;
    int ip = 0;
    while (ip < chunk->count) {
        uint8_t op = chunk->code[ip];
        switch (op) {
            case OP_CONSTANT:
            case OP_GET_LOCAL: case OP_SET_LOCAL:
            case OP_GET_GLOBAL: case OP_SET_GLOBAL: case OP_DEFINE_GLOBAL:
                ip += 2; break;
            case OP_JUMP: case OP_JUMP_IF_FALSE: case OP_LOOP:
                ip += 3; break;
            case OP_NIL: case OP_TRUE: case OP_FALSE: case OP_POP:
            case OP_ADD: case OP_SUBTRACT: case OP_MULTIPLY: case OP_DIVIDE:
            case OP_NEGATE: case OP_NOT:
            case OP_EQUAL: case OP_GREATER: case OP_LESS:
            case OP_PRINT: case OP_RETURN:
                ip += 1; break;
            default:
                return false;
        }
    }
    return true;
}

class Codegen {
public:
    LLVMContext&              ctx;
    Module*                   mod;
    IRBuilder<>               builder;
    ObjFunction*              rfFunc;
    std::vector<llvm::Value*> stack;
    std::vector<llvm::Value*> locals;
    Function*                 llvmFn;
    std::unordered_map<int, BasicBlock*> blocks;

    FunctionCallee fnPrintValue;
    FunctionCallee fnPrintf;
    FunctionCallee fnDefineGlobal;
    FunctionCallee fnGetGlobal;
    FunctionCallee fnSetGlobal;

    Codegen(LLVMContext& ctx, Module* mod, ObjFunction* fn)
        : ctx(ctx), mod(mod), builder(ctx), rfFunc(fn) {}

    void declareExternals() {
        fnPrintValue = mod->getOrInsertFunction("printValue",
            FunctionType::get(gTypes.voidType, {gTypes.rfValueType}, false));
        fnPrintf = mod->getOrInsertFunction("printf",
            FunctionType::get(gTypes.i32Type, {gTypes.i8PtrType}, true));
        fnDefineGlobal = mod->getOrInsertFunction("jitDefineGlobal",
            FunctionType::get(gTypes.voidType,
                {gTypes.i8PtrType, gTypes.rfValueType}, false));
        fnGetGlobal = mod->getOrInsertFunction("jitGetGlobal",
            FunctionType::get(gTypes.rfValueType, {gTypes.i8PtrType}, false));
        fnSetGlobal = mod->getOrInsertFunction("jitSetGlobal",
            FunctionType::get(gTypes.voidType,
                {gTypes.i8PtrType, gTypes.rfValueType}, false));
    }

    void irPush(llvm::Value* val) { stack.push_back(val); }
    llvm::Value* irPop() {
        if (!stack.empty()) { auto v = stack.back(); stack.pop_back(); return v; }
        return makeIRNil();
    }
    llvm::Value* irPeek() {
        return stack.empty() ? makeIRNil() : stack.back();
    }
    BasicBlock* getBlock(int offset) {
        auto it = blocks.find(offset);
        if (it != blocks.end()) return it->second;
        BasicBlock* bb = BasicBlock::Create(ctx,
            "ip" + std::to_string(offset), llvmFn);
        blocks[offset] = bb;
        return bb;
    }
    void createBlocks(Chunk* chunk) {
        int ip = 0;
        while (ip < chunk->count) {
            uint8_t op = chunk->code[ip];
            if (op == OP_JUMP || op == OP_JUMP_IF_FALSE) {
                uint16_t offset = (uint16_t)((chunk->code[ip+1] << 8) | chunk->code[ip+2]);
                int target = ip + 3 + (int)offset;
                getBlock(target);
                getBlock(ip + 3); 
            } else if (op == OP_LOOP) {
                uint16_t offset = (uint16_t)((chunk->code[ip+1] << 8) | chunk->code[ip+2]);
                int target = ip + 3 - (int)offset;
                getBlock(target);
                getBlock(ip + 3);
            }
            // advance ip
            switch (op) {
                case OP_CONSTANT: case OP_GET_LOCAL: case OP_SET_LOCAL:
                case OP_GET_GLOBAL: case OP_SET_GLOBAL: case OP_DEFINE_GLOBAL:
                    ip += 2; break;
                case OP_JUMP: case OP_JUMP_IF_FALSE: case OP_LOOP:
                    ip += 3; break;
                default: ip += 1; break;
            }
        }
    }

    Function* build() {
        declareExternals();
        std::string name = rfFunc->name ? rfFunc->name->chars : "__script__";
        llvmFn = Function::Create(
            FunctionType::get(gTypes.voidType, {}, false),
            Function::ExternalLinkage, name, mod);

        BasicBlock* entry = BasicBlock::Create(ctx, "entry", llvmFn);
        builder.SetInsertPoint(entry);

        // Allocate local slots
        locals.resize(256);
        for (int i = 0; i < 256; i++) {
            locals[i] = builder.CreateAlloca(gTypes.rfValueType,
                nullptr, "s" + std::to_string(i));
            builder.CreateStore(makeIRNil(), locals[i]);
        }

        Chunk* chunk = &rfFunc->chunk;
        createBlocks(chunk);

        int ip = 0;
        while (ip >= 0 && ip < chunk->count) {
            auto it = blocks.find(ip);
            if (it != blocks.end()) {
                BasicBlock* bb = it->second;
                if (!builder.GetInsertBlock()->getTerminator())
                    builder.CreateBr(bb);
                builder.SetInsertPoint(bb);
            }
            ip = emit(chunk, ip);
        }

        if (!builder.GetInsertBlock()->getTerminator())
            builder.CreateRetVoid();

        std::string err;
        raw_string_ostream errOS(err);
        if (verifyFunction(*llvmFn, &errOS)) {
            fprintf(stderr, "JIT verify: %s\n", err.c_str());
            llvmFn->eraseFromParent();
            return nullptr;
        }
        return llvmFn;
    }
    llvm::Value* isFalsey(llvm::Value* val) {
        llvm::Value* tag     = builder.CreateExtractValue(val, {0});
        llvm::Value* payload = builder.CreateExtractValue(val, {1});
        llvm::Value* isNil   = builder.CreateICmpEQ(tag,
            ConstantInt::get(gTypes.i32Type, VAL_NIL));
        llvm::Value* isBool  = builder.CreateICmpEQ(tag,
            ConstantInt::get(gTypes.i32Type, VAL_BOOL));
        llvm::Value* isFalse = builder.CreateFCmpOEQ(payload,
            ConstantFP::get(gTypes.doubleType, 0.0));
        return builder.CreateOr(isNil, builder.CreateAnd(isBool, isFalse));
    }

    llvm::Value* buildBinop(char op) {
        llvm::Value* b = irPop(), *a = irPop();
        llvm::Value* na = extractNum(builder, a);
        llvm::Value* nb = extractNum(builder, b);
        llvm::Value* r;
        switch (op) {
            case '+': r = builder.CreateFAdd(na, nb); break;
            case '-': r = builder.CreateFSub(na, nb); break;
            case '*': r = builder.CreateFMul(na, nb); break;
            case '/': r = builder.CreateFDiv(na, nb); break;
            default:  r = na;
        }
        llvm::Value* out = builder.CreateInsertValue(makeIRNumber(0), r, {1});
        return builder.CreateInsertValue(out,
            ConstantInt::get(gTypes.i32Type, VAL_NUMBER), {0});
    }

    llvm::Value* buildCmp(llvm::CmpInst::Predicate pred) {
        llvm::Value* b = irPop(), *a = irPop();
        llvm::Value* cmp = builder.CreateFCmp(pred,
            extractNum(builder,a), extractNum(builder,b));
        llvm::Value* ext = builder.CreateUIToFP(cmp, gTypes.doubleType);
        llvm::Value* out = builder.CreateInsertValue(makeIRNil(), ext, {1});
        return builder.CreateInsertValue(out,
            ConstantInt::get(gTypes.i32Type, VAL_BOOL), {0});
    }

    int emit(Chunk* chunk, int ip) {
        uint8_t op = chunk->code[ip];
        switch (op) {
            case OP_CONSTANT:
                irPush(rfToIR(chunk->constants.values[chunk->code[ip+1]]));
                return ip+2;
            case OP_NIL:   irPush(makeIRNil());      return ip+1;
            case OP_TRUE:  irPush(makeIRBool(true));  return ip+1;
            case OP_FALSE: irPush(makeIRBool(false)); return ip+1;
            case OP_POP:   irPop();                   return ip+1;

            case OP_ADD:      irPush(buildBinop('+')); return ip+1;
            case OP_SUBTRACT: irPush(buildBinop('-')); return ip+1;
            case OP_MULTIPLY: irPush(buildBinop('*')); return ip+1;
            case OP_DIVIDE:   irPush(buildBinop('/')); return ip+1;

            case OP_NEGATE: {
                llvm::Value* a = irPop();
                llvm::Value* r = builder.CreateFNeg(extractNum(builder,a));
                llvm::Value* out = builder.CreateInsertValue(makeIRNumber(0), r, {1});
                out = builder.CreateInsertValue(out,
                    ConstantInt::get(gTypes.i32Type, VAL_NUMBER), {0});
                irPush(out); return ip+1;
            }
            case OP_NOT: {
                llvm::Value* a = irPop();
                llvm::Value* falsy = isFalsey(a);
                llvm::Value* ext = builder.CreateUIToFP(falsy, gTypes.doubleType);
                llvm::Value* out = builder.CreateInsertValue(makeIRNil(), ext, {1});
                out = builder.CreateInsertValue(out,
                    ConstantInt::get(gTypes.i32Type, VAL_BOOL), {0});
                irPush(out); return ip+1;
            }
            case OP_EQUAL: {
                llvm::Value* b = irPop(), *a = irPop();
                llvm::Value* tagA = builder.CreateExtractValue(a, {0});
                llvm::Value* tagB = builder.CreateExtractValue(b, {0});
                llvm::Value* tagsEq = builder.CreateICmpEQ(tagA, tagB);
                llvm::Value* valsEq = builder.CreateFCmpOEQ(
                    extractNum(builder,a), extractNum(builder,b));
                llvm::Value* eq = builder.CreateAnd(tagsEq, valsEq);
                llvm::Value* ext = builder.CreateUIToFP(eq, gTypes.doubleType);
                llvm::Value* out = builder.CreateInsertValue(makeIRNil(), ext, {1});
                out = builder.CreateInsertValue(out,
                    ConstantInt::get(gTypes.i32Type, VAL_BOOL), {0});
                irPush(out); return ip+1;
            }
            case OP_GREATER:
                irPush(buildCmp(llvm::CmpInst::FCMP_OGT)); return ip+1;
            case OP_LESS:
                irPush(buildCmp(llvm::CmpInst::FCMP_OLT)); return ip+1;

            case OP_PRINT: {
                llvm::Value* val = irPop();
                builder.CreateCall(fnPrintValue, {val});
                builder.CreateCall(fnPrintf,
                    {builder.CreateGlobalStringPtr("\n")});
                return ip+1;
            }

            case OP_GET_LOCAL: {
                uint8_t slot = chunk->code[ip+1];
                irPush(builder.CreateLoad(gTypes.rfValueType, locals[slot]));
                return ip+2;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = chunk->code[ip+1];
                builder.CreateStore(irPeek(), locals[slot]);
                return ip+2;
            }
            case OP_DEFINE_GLOBAL: {
                uint8_t idx = chunk->code[ip+1];
                ObjString* name = AS_STRING(chunk->constants.values[idx]);
                builder.CreateCall(fnDefineGlobal,
                    {objStrPtr(ctx, name), irPop()});
                return ip+2;
            }
            case OP_GET_GLOBAL: {
                uint8_t idx = chunk->code[ip+1];
                ObjString* name = AS_STRING(chunk->constants.values[idx]);
                irPush(builder.CreateCall(fnGetGlobal, {objStrPtr(ctx, name)}));
                return ip+2;
            }
            case OP_SET_GLOBAL: {
                uint8_t idx = chunk->code[ip+1];
                ObjString* name = AS_STRING(chunk->constants.values[idx]);
                builder.CreateCall(fnSetGlobal,
                    {objStrPtr(ctx, name), irPeek()});
                return ip+2;
            }

            case OP_JUMP: {
                uint16_t offset = (uint16_t)((chunk->code[ip+1] << 8) | chunk->code[ip+2]);
                int target = ip + 3 + (int)offset;
                builder.CreateBr(getBlock(target));
                return ip+3;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = (uint16_t)((chunk->code[ip+1] << 8) | chunk->code[ip+2]);
                int target  = ip + 3 + (int)offset;
                int fallthru = ip + 3;
                llvm::Value* cond = isFalsey(irPeek()); 
                builder.CreateCondBr(cond,
                    getBlock(target), getBlock(fallthru));
                return ip+3;
            }
            case OP_LOOP: {
                uint16_t offset = (uint16_t)((chunk->code[ip+1] << 8) | chunk->code[ip+2]);
                int target = ip + 3 - (int)offset;
                builder.CreateBr(getBlock(target));
                return ip+3;
            }

            case OP_RETURN:
                if (!stack.empty()) irPop();
                builder.CreateRetVoid();
                return -1;

            default:
                fprintf(stderr, "JIT: unhandled opcode %d\n", op);
                builder.CreateRetVoid();
                return -1;
        }
    }
};

extern "C" {

void jitDefineGlobal(ObjString* name, RFValue value) {
    tableSet(&vm.globals, name, value);
}

RFValue jitGetGlobal(ObjString* name) {
    RFValue value;
    if (!tableGet(&vm.globals, name, &value)) {
        runtimeError("Undefined variable '%s'.", name->chars);
        value.type = VAL_NIL;
        value.as.number = 0;
    }
    return value;
}

void jitSetGlobal(ObjString* name, RFValue value) {
    tableSet(&vm.globals, name, value);
}

} 

void jitInit() {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();
    gContext = new LLVMContext();
    initTypes(*gContext);
}

void jitShutdown() {
    delete gContext;
    gContext = nullptr;
}

int jitExecute(ObjFunction* function) {
    if (!gContext) return 0;
    if (!function->name) return 0; 
    if (!canJIT(function)) return 0;

    auto mod = std::make_unique<Module>("rf_jit", *gContext);
    Codegen cg(*gContext, mod.get(), function);
    Function* fn = cg.build();
    if (!fn) return 0;

    std::string errStr;
    ExecutionEngine* ee = EngineBuilder(std::move(mod))
        .setErrorStr(&errStr)
        .setEngineKind(EngineKind::JIT)
        .create();
    if (!ee) {
        fprintf(stderr, "JIT engine: %s\n", errStr.c_str());
        return 0;
    }

    ee->addGlobalMapping("jitDefineGlobal", (uint64_t)(void*)jitDefineGlobal);
    ee->addGlobalMapping("jitGetGlobal",    (uint64_t)(void*)jitGetGlobal);
    ee->addGlobalMapping("jitSetGlobal",    (uint64_t)(void*)jitSetGlobal);
    ee->finalizeObject();

    std::string name = function->name->chars;
    auto fnPtr = (void(*)())ee->getFunctionAddress(name);
    if (!fnPtr) { delete ee; return 0; }
    fnPtr();
    delete ee;
    return 1;
}