#pragma once

// ==============================================
// Type conversions from ruby to llvm
// ==============================================

#define jit_values RB_JIT->values
#define jit_types  RB_JIT->types
#define jit_funcs  RB_JIT->funcs

#define V(x) jit_values->value(x)
#define PV(x) BUILDER->CreateIntToPtr(V((VALUE)x), jit_types->pvalueT)
#define PT(x, type) BUILDER->CreateIntToPtr(V((VALUE)x), jit_types->type)
#define I(x) jit_values->intV(x)

#define _Qundef jit_values->undefV

// ==============================================
// Utilities for codegen
// ==============================================

#define GetBasicBlock() BUILDER->GetInsertBlock()
#define SetBasicBlock(bb) BUILDER->SetInsertPoint(bb)

#define CreateBasicBlock(name) BasicBlock::Create(CONTEXT, (name), codegen_func.jit_trace_func)
#define SetNewBasicBlock(bb, name) BasicBlock *bb = CreateBasicBlock(name); SetBasicBlock(bb)

#define _FCALL0(name) BUILDER->CreateCall(jit_funcs->name)
#define _FCALL1(name, a) BUILDER->CreateCall(jit_funcs->name, a)
#define _FCALL2(name, a, b) BUILDER->CreateCall2(jit_funcs->name, a, b)
#define _FCALL3(name, a, b, c) BUILDER->CreateCall3(jit_funcs->name, a, b, c)


// if a == b then do bb_then
#define _IF_EQ(a, b, bb_then) do { \
		BasicBlock *bb_cur = GetBasicBlock(); \
		BasicBlock *bb_merge = CreateBasicBlock("merge"); \
		Value *test = BUILDER->CreateICmpEQ(a, b, "if_eq"); \
		BUILDER->CreateCondBr(test, bb_then, bb_merge);\
		SetBasicBlock(bb_then); \
		BUILDER->CreateBr(bb_merge); \
		SetBasicBlock(bb_merge); \
	} while (0)

#define _IF_EQ2(a, b, bb_then, bb_else) do { \
		BasicBlock *bb_merge = CreateBasicBlock("merge2."); \
		Value *test = BUILDER->CreateICmpEQ(a, b, "if_eq2."); \
		BUILDER->CreateCondBr(test, bb_then, bb_else);\
		SetBasicBlock(bb_then); \
		BUILDER->CreateBr(bb_merge); \
		SetBasicBlock(bb_else); \
		BUILDER->CreateBr(bb_merge); \
		SetBasicBlock(bb_merge); \
	} while (0)



// ==============================================
// Stack operations
// ==============================================
#undef TOPN
#define TOPN(x)\
	[&]{\
	Value *sp_ptr = _GET_SP_PTR(); \
	Value *sp = BUILDER->CreateLoad(sp_ptr);\
	JIT_LLVM_SET_NAME(sp, "sp");\
	Value *sp_incptr = BUILDER->CreateInBoundsGEP(sp, jit_values->signedValue(-x - 1));\
	JIT_LLVM_SET_NAME(sp_incptr, "sp_minus_" #x "_");\
	return BUILDER->CreateLoad(sp_incptr);}()
#define SET_TOPN(x, val) {\
	Value *sp_ptr = _GET_SP_PTR(); \
	Value *sp = BUILDER->CreateLoad(sp_ptr);\
	JIT_LLVM_SET_NAME(sp, "sp");\
	Value *sp_incptr = BUILDER->CreateInBoundsGEP(sp, jit_values->signedValue(-x - 1));\
	JIT_LLVM_SET_NAME(sp_incptr, "sp_minus_" #x "_");\
	BUILDER->CreateStore(val, sp_incptr);}
#undef POPN
#define POPN(x) {\
	Value *sp_ptr = _GET_SP_PTR(); \
	Value *sp = BUILDER->CreateLoad(sp_ptr);\
	JIT_LLVM_SET_NAME(sp, "sp");\
	Value *sp_incptr = BUILDER->CreateInBoundsGEP(sp, jit_values->signedValue(-x));\
	JIT_LLVM_SET_NAME(sp_incptr, "sp_minus_" #x "_");\
	BUILDER->CreateStore(sp_incptr, sp_ptr);}
#undef PUSH
#define PUSH(x) {\
	Value *sp_ptr = _GET_SP_PTR(); \
	Value *sp = BUILDER->CreateLoad(sp_ptr);\
	JIT_LLVM_SET_NAME(sp, "sp");\
	BUILDER->CreateStore((x), sp);\
	Value *sp_incptr = BUILDER->CreateInBoundsGEP(sp, jit_values->valueOne);\
	JIT_LLVM_SET_NAME(sp_incptr, "sp_plus_1_");\
	BUILDER->CreateStore(sp_incptr, sp_ptr);}
#undef STACK_ADDR_FROM_TOP
#define STACK_ADDR_FROM_TOP(n)\
	[&]{\
	Value *sp_ptr = _GET_SP_PTR(); \
	Value *sp = BUILDER->CreateLoad(sp_ptr);\
	JIT_LLVM_SET_NAME(sp, "sp");\
	Value *sp_incptr = BUILDER->CreateInBoundsGEP(sp, jit_values->signedValue(-n));\
	JIT_LLVM_SET_NAME(sp_incptr, "sp_minus_" #n "_");\
	return sp_incptr;}()


