#include "vm_insnhelper.h"
#include <thread>

// vm_exec.h
typedef rb_iseq_t *ISEQ;




static inline void
jit_codegen_optimize(Function& f, Module *module)
{
// #define ADD_PASS(name) static FunctionPass* _##name = name(); fpm.add(_##name)
#define ADD_PASS(name) fpm.add(name())
	//https://github.com/WestleyArgentum/pass-optimizer/blob/master/codegen/pass_setup.cpp
	legacy::FunctionPassManager fpm(module);
	/////// fpm.addPass(new DataLayout(*engine->getDataLayout()));
	ADD_PASS(createTypeBasedAliasAnalysisPass);
	ADD_PASS(createPromoteMemoryToRegisterPass);
	ADD_PASS(createJumpThreadingPass);
	ADD_PASS(createReassociatePass);
	ADD_PASS(createEarlyCSEPass);
	ADD_PASS(createLoopIdiomPass);
	ADD_PASS(createLoopRotatePass);
	ADD_PASS(createLICMPass);
	ADD_PASS(createIndVarSimplifyPass);
	ADD_PASS(createLoopDeletionPass);
	ADD_PASS(createSCCPPass);
	ADD_PASS(createSinkingPass);
	ADD_PASS(createInstructionSimplifierPass);
	ADD_PASS(createDeadStoreEliminationPass);
	ADD_PASS(createScalarizerPass);
	ADD_PASS(createAggressiveDCEPass);
	ADD_PASS(createConstantPropagationPass);
	ADD_PASS(createDeadInstEliminationPass);
	ADD_PASS(createDeadCodeEliminationPass);
	ADD_PASS(createLoopStrengthReducePass);
	ADD_PASS(createTailCallEliminationPass);
	ADD_PASS(createMemCpyOptPass);
	ADD_PASS(createLowerAtomicPass); // SIGSEGV @bm_app_fib.rb
	ADD_PASS(createCorrelatedValuePropagationPass);
	ADD_PASS(createLowerExpectIntrinsicPass);

	ADD_PASS(createBasicAliasAnalysisPass);
	ADD_PASS(createInstructionCombiningPass);
	ADD_PASS(createGVNPass);
	ADD_PASS(createCFGSimplificationPass);
	fpm.doInitialization();
	fpm.run(f);
#undef ADD_PASS
}

static inline void
jit_codegen_core(
		jit_codegen_func_t codegen_func,
		rb_control_frame_t *cfp,
		jit_trace_t *trace,
		int index);

static inline void
jit_codegen_trace(rb_thread_t *th, jit_trace_t *trace);

#if 0
void
jit_codegen(rb_thread_t *th)
{
	int i = 0;
	for (jit_trace_t *trace: RB_JIT->trace_list) {
		JIT_DEBUG_LOG2("=== Start codegen (index: %d) ===", i++);

		jit_codegen_trace(th, trace);

		JIT_DEBUG_LOG("=== End trace dump ===");
	}
}
#endif

static inline void
jit_codegen_make_return(jit_codegen_func_t codegen_func, Value* retval = RB_JIT->values->valueZero)
{
	AllocaInst *ret_alloca = BUILDER->CreateAlloca(RB_JIT->types->jit_func_ret_t);
	Value *ret_cfp_ptr = BUILDER->CreateStructGEP(RB_JIT->types->rb_control_frame_st, ret_alloca, 0);
	Value *ret_ptr = BUILDER->CreateStructGEP(RB_JIT->types->jit_func_ret_t, ret_alloca, 1);
	// arg_th からcfp を取り出す vm_pop_frameでの変更を適用するため
	Value* th_cfp_ptr = BUILDER->CreateStructGEP(RB_JIT->types->rb_thread_st, BUILDER->CreateLoad(codegen_func.arg_th_ptr), 5);
	// Value* th_cfp_ptr = BUILDER->CreateStructGEP(codegen_func.arg_th, 5);
	Value* th_cfp = BUILDER->CreateLoad(th_cfp_ptr);
	// BUILDER->CreateStore(codegen_func.arg_cfp, ret_cfp_ptr);
	BUILDER->CreateStore(th_cfp, ret_cfp_ptr);
	BUILDER->CreateStore(retval, ret_ptr);
	Value *ret = BUILDER->CreateLoad(ret_alloca);
	BUILDER->CreateRet(ret);
}

static inline void
jit_codegen_trace_core(rb_thread_t *th, jit_trace_t *trace)
{
	JIT_DEBUG_LOG("=== Start jit_codegen_trace  ===");
	Module *module = RB_JIT->createModule();
	jit_codegen_func_t codegen_func = RB_JIT->createJITFunction(module);
	codegen_func.module = module;

	jit_insn_t **insns = trace->insns;

	VALUE *first_pc = insns[0]->pc;
	auto *first_cfp = insns[0]->cfp;

	insns[trace->insns_iterator] = new jit_insn_t;

	for (unsigned i = 0; i < trace->insns_iterator + 1; i++) {
	// for (unsigned i = 0, len = 1; i < insns_size + 1; i += len) {
		auto *insn = insns[i];
		// if (!insn) {
		// 	// トレースしそこねた命令(branchlessなどでジャンプした場合に起こる)
		// 	// fprintf(stderr, "not tracing instruction: %d, %08p\n", i, first_pc + i);
		// 	jit_trace_insn(th, first_cfp, first_pc + i);
		// 	insn = insns[i];
		// 	// jit_dump_insn(insn);
		// }
		insn->bb = BasicBlock::Create(CONTEXT, JIT_LLVM_INSN_NAME("insn"), codegen_func.jit_trace_func);
		// len = insn->len;
	}

	BUILDER->CreateBr(insns[0]->bb);

	// for (unsigned i = 0, len; i < insns_size; i+= len) {
	// 	jit_insn_t *insn = insns[i];
	// 	jit_codegen_core(codegen_func, th->cfp, insns, insn);
	// 	len = insn->len;
	// }
#ifdef DUMP_CODEGEN_TRACE
	fprintf(stderr, "==== Dump codegen traces ====\n");
#endif
	for (unsigned i = 0; i < trace->insns_iterator; i++) {
		if (auto *insn = insns[i]) {
#ifdef DUMP_CODEGEN_TRACE
			jit_dump_insn(insn);
#endif
			jit_codegen_core(codegen_func, insn->cfp, trace, i);
		}
	}

	BUILDER->SetInsertPoint(insns[trace->insns_iterator]->bb);
	jit_codegen_make_return(codegen_func);

#ifdef DUMP_LLVM_IR
	fprintf(stderr, "==== JITed instructions ====\n");
	codegen_func.jit_trace_func->dump();
#endif
#ifdef USE_OPT_LLVM_IR
	jit_codegen_optimize(*codegen_func.jit_trace_func, module);
#endif
#ifdef DUMP_OPT_LLVM_IR
	fprintf(,stderr, "==== Optimized JITed instructions ====\n");
	codegen_func.jit_trace_func->dump();
#endif

	JIT_DEBUG_LOG("==== Compile instructions ====");
	trace->jited = RB_JIT->compileFunction(codegen_func.jit_trace_func);

#ifdef USE_THREAD
	trace->compiling = false;
#endif
}

static inline void
jit_codegen_trace(rb_thread_t *th, jit_trace_t *trace)
{
#ifdef USE_THREAD
	trace->compiling = true;
	std::thread codegen_thread(jit_codegen_trace_core, th, trace);
	codegen_thread.detach();
#else
	jit_codegen_trace_core(th, trace);
#endif
}

static inline void
jit_codegen_core(
		jit_codegen_func_t codegen_func,
		rb_control_frame_t *reg_cfp,
		jit_trace_t *trace,
		int index)
{
#define JIT_TRACE_FUNC	codegen_func.jit_trace_func
#define MODULE			codegen_func.module


#ifdef JIT_DEBUG_FLAG
#define PRINT_VAL(val) RB_JIT->createPrintf(MODULE, (val));
#else
#define PRINT_VAL(val)
#endif


#include "jit_codegen_helper.h"


#define OFFSET long

#define JIT_CHECK_STACK_SIZE
#define jit_error(msg) (fprintf(stderr, msg" (%s, %d)", __FILE__, __LINE__), rb_raise(rb_eRuntimeError, "JIT error"), nullptr)

#define _CALL_METHOD(_ci) do { \
	Value *ci_call_elmptr = BUILDER->CreateStructGEP(jit_types->rb_call_info_st, _ci, 14); \
	Value *ci_call = BUILDER->CreateLoad(ci_call_elmptr); \
	Value *v = BUILDER->CreateCall(ci_call, {_GET_TH(), _GET_CFP(), _ci});\
	BasicBlock *bb_cur = GetBasicBlock(); \
	SetNewBasicBlock(bb_then, "then"); \
	_RESTORE_REGS(); \
	SetNewBasicBlock(bb_else, "else"); \
	JIT_DEBUG_LOG("CALL_METHOD else"); \
	_PUSH(v);\
	SetBasicBlock(bb_cur); \
	_IF_EQ2(v, _Qundef, bb_then, bb_else); \
} while (0)


#define _CALL_SIMPLE_METHOD(recv_) do { \
	Value *_ci = PT(ci, rb_call_info_t); \
    ci->blockptr = 0; ci->argc = ci->orig_argc; \
	Value *ci_recv_elmptr = BUILDER->CreateStructGEP(jit_types->rb_call_info_st, _ci, 11); \
	BUILDER->CreateStore(recv_, ci_recv_elmptr); \
	_FCALL(vm_search_method_extern, fnVoid, _ci, V(ci->recv)); \
    _CALL_METHOD(_ci); \
} while (0)


#define GET_SELF() RB_JIT->values->value(reg_cfp->self)
#define _GET_TH() BUILDER->CreateLoad(codegen_func.arg_th_ptr)// JIT_TH
#define _GET_CFP() BUILDER->CreateLoad(codegen_func.arg_cfp_ptr)// JIT_CFP
#define _GET_CFP_PTR() codegen_func.arg_cfp_ptr
// #define _GET_EP() BUILDER->CreateLoad(EP_GEP)
#define _GET_SP_PTR() BUILDER->CreateStructGEP(jit_types->rb_control_frame_st, _GET_CFP(), 1)
#define _GET_SP() BUILDER->CreateLoad(_GET_SP_PTR())
#define _GET_EP_PTR() BUILDER->CreateStructGEP(jit_types->rb_control_frame_st, _GET_CFP(), 6)
#define _GET_EP() BUILDER->CreateLoad(_GET_EP_PTR())

#define _NEXT_SP_PTR(x) [&] {\
	Value *sp_ptr = _GET_SP_PTR(); \
	Value *sp = BUILDER->CreateLoad(sp_ptr);\
	return BUILDER->CreateInBoundsGEP(sp, jit_values->intV(x));}()

#define _INC_SP(x) {\
	Value *sp_ptr = _GET_SP_PTR(); \
	Value *sp = BUILDER->CreateLoad(sp_ptr);\
	Value *sp_incptr = BUILDER->CreateInBoundsGEP(sp, jit_values->intV(x));\
	BUILDER->CreateStore(sp_incptr, sp_ptr);}

// #define _INC_SP(x) {\
// 	Value *sp_ptr = _GET_SP_PTR(); \
// 	Value *sp = BUILDER->CreateLoad(sp_ptr);\
// 	Value *sp_incptr = BUILDER->CreateInBoundsGEP(sp, jit_values->intV(x));\
// 	BUILDER->CreateStore(sp_incptr, sp_ptr);}


#define GET_LEP() (JIT_EP_LEP(GET_EP()))
#undef GET_OPERAND
#define GET_OPERAND(x) (insn->pc[(x)])

#define _RSHIFT(x,y) BUILDER->CreateLShr(x, y)
#define _FIX2LONG(x) (_RSHIFT((x), 1))

#define _INT2FIX(i) BUILDER->CreateOr(BUILDER->CreateShl(i, 1), FIXNUM_FLAG)
#define _LONG2FIX(i) _INT2FIX(i)
#define _LONG2NUM(i) _LONG2FIX(i)

// #define RTEST(v) !(((VALUE)(v) & ~Qnil) == 0)
#define _RTEST(v) (BUILDER->CreateICmpNE(BUILDER->CreateAnd((v), ~Qnil), RB_JIT->values->valueZero))

// #define FIXNUM_2_P(a, b) ((a) & (b) & 1)
#define _FIXNUM_2_P(a, b) (BUILDER->CreateICmpNE(BUILDER->CreateAnd(BUILDER->CreateAnd((a), (b)), 1), jit_values->valueZero))

// #define FLONUM_2_P(a, b) (((((a)^2) | ((b)^2)) & 3) == 0)

#define _RBASIC(v) (BUILDER->CreateBitOrPointerCast((v), jit_types->PRBasic))
#define _RBASIC_CLASS(v) (BUILDER->CreateLoad(BUILDER->CreateStructGEP(jit_types->RBasic, (v), 1)))


#undef  RESTORE_REGS
#define RESTORE_REGS() \
{ \
  REG_CFP = th->cfp; \
  reg_pc  = reg_cfp->pc; \
  JIT_POP_TRACE(th->cfp); \
}
#define _RESTORE_REGS() \
{ \
	Value* th_cfp_ptr = BUILDER->CreateStructGEP(jit_types->rb_thread_st, _GET_TH(), 5); \
	Value* th_cfp = BUILDER->CreateLoad(th_cfp_ptr); \
	BUILDER->CreateStore(th_cfp, _GET_CFP_PTR()); \
}




// #define NEXT_INSN() (insns[insn->index + insn->len])
#define NEXT_INSN() (insns[index + 1])

	unsigned insns_size = trace->insns_iterator;
	jit_insn_t **insns = trace->insns;
	jit_insn_t *insn = insns[index];

	switch (insn->opecode) {
#include "jit_codegen.inc"
	}
}