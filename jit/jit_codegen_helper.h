#pragma once

// ==============================================
// Type conversions from ruby to llvm
// ==============================================

#define jit_values RB_JIT->values
#define jit_types  RB_JIT->types

#define V(x) jit_values->value(x)
#define PV(x) BUILDER->CreateIntToPtr(V((VALUE)x), jit_types->pvalueT)
#define I(x) jit_values->intV(x)


// ==============================================
// Utilities for codegen
// ==============================================

#define CreateBasicBlock(name) BasicBlock::Create(CONTEXT, (name), codegen_func.jit_trace_func)

#define SetBasicBlock(bb) BUILDER->SetInsertPoint(bb)


// ==============================================
// Stack operations
// ==============================================
#undef TOPN
#define TOPN(x)\
	[&]{\
	Value *sp = BUILDER->CreateLoad(SP_GEP);\
	JIT_LLVM_SET_NAME(sp, "sp");\
	Value *sp_incptr = BUILDER->CreateInBoundsGEP(sp, jit_values->signedValue(-x - 1));\
	JIT_LLVM_SET_NAME(sp_incptr, "sp_minus_" #x "_");\
	return BUILDER->CreateLoad(sp_incptr);}()
#define SET_TOPN(x, val) {\
	Value *sp = BUILDER->CreateLoad(SP_GEP);\
	JIT_LLVM_SET_NAME(sp, "sp");\
	Value *sp_incptr = BUILDER->CreateInBoundsGEP(sp, jit_values->signedValue(-x - 1));\
	JIT_LLVM_SET_NAME(sp_incptr, "sp_minus_" #x "_");\
	BUILDER->CreateStore(val, sp_incptr);}
#undef POPN
#define POPN(x) {\
	Value *sp = BUILDER->CreateLoad(SP_GEP);\
	JIT_LLVM_SET_NAME(sp, "sp");\
	Value *sp_incptr = BUILDER->CreateInBoundsGEP(sp, jit_values->signedValue(-x));\
	JIT_LLVM_SET_NAME(sp_incptr, "sp_minus_" #x "_");\
	BUILDER->CreateStore(sp_incptr, SP_GEP);}
#undef PUSH
#define PUSH(x) {\
	Value *sp = BUILDER->CreateLoad(SP_GEP);\
	JIT_LLVM_SET_NAME(sp, "sp");\
	BUILDER->CreateStore((x), sp);\
	Value *sp_incptr = BUILDER->CreateInBoundsGEP(sp, jit_values->valueOne);\
	JIT_LLVM_SET_NAME(sp_incptr, "sp_plus_1_");\
	BUILDER->CreateStore(sp_incptr, SP_GEP);}
#undef STACK_ADDR_FROM_TOP
#define STACK_ADDR_FROM_TOP(n)\
	[&]{\
	Value *sp = BUILDER->CreateLoad(SP_GEP);\
	JIT_LLVM_SET_NAME(sp, "sp");\
	Value *sp_incptr = BUILDER->CreateInBoundsGEP(sp, jit_values->signedValue(-n));\
	JIT_LLVM_SET_NAME(sp_incptr, "sp_minus_" #n "_");\
	return sp_incptr;}()


