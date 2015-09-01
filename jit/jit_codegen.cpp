static inline void
jit_codegen_optimize(Function& f, Module *module)
{
	//https://github.com/WestleyArgentum/pass-optimizer/blob/master/codegen/pass_setup.cpp
	legacy::FunctionPassManager fpm(module);
	/////// fpm.addPass(new DataLayout(*engine->getDataLayout()));
	fpm.add(createTypeBasedAliasAnalysisPass());
	fpm.add(createPromoteMemoryToRegisterPass());
	fpm.add(createJumpThreadingPass());
	fpm.add(createReassociatePass());
	fpm.add(createEarlyCSEPass());
	fpm.add(createLoopIdiomPass());
	fpm.add(createLoopRotatePass());
	fpm.add(createLICMPass());
	fpm.add(createIndVarSimplifyPass());
	fpm.add(createLoopDeletionPass());
	fpm.add(createSCCPPass());
	fpm.add(createSinkingPass());
	fpm.add(createInstructionSimplifierPass());
	fpm.add(createDeadStoreEliminationPass());
	fpm.add(createScalarizerPass());
	fpm.add(createAggressiveDCEPass());
	fpm.add(createConstantPropagationPass());
	fpm.add(createDeadInstEliminationPass());
	fpm.add(createDeadCodeEliminationPass());
	fpm.add(createLoopStrengthReducePass());
	fpm.add(createTailCallEliminationPass());
	fpm.add(createMemCpyOptPass());
	fpm.add(createLowerAtomicPass());
	fpm.add(createCorrelatedValuePropagationPass());
	fpm.add(createLowerExpectIntrinsicPass());

	fpm.add(createBasicAliasAnalysisPass());
	fpm.add(createInstructionCombiningPass());
	fpm.add(createReassociatePass());
	fpm.add(createGVNPass());
	fpm.add(createCFGSimplificationPass());
	fpm.doInitialization();
	fpm.run(f);
}

static inline void
jit_codegen_core(
		jit_codegen_func_t codegen_func,
		rb_control_frame_t *cfp,
		jit_trace_t *trace,
		int index);

static inline void
jit_codegen_trace(rb_thread_t *th, jit_trace_t *trace);

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

static inline void
jit_codegen_make_return(jit_codegen_func_t codegen_func, int is_ret = 0)
{
	AllocaInst *ret_ptr = BUILDER->CreateAlloca(RB_JIT->types->jit_func_ret_t);
	Value *ret_cfp_ptr = BUILDER->CreateStructGEP(ret_ptr, 0);
	Value *is_ret_ptr = BUILDER->CreateStructGEP(ret_ptr, 1);
	BUILDER->CreateStore(codegen_func.arg_cfp, ret_cfp_ptr);
	BUILDER->CreateStore(RB_JIT->values->intV(is_ret), is_ret_ptr);
	Value *ret = BUILDER->CreateLoad(ret_ptr);
	BUILDER->CreateRet(ret);
}

static inline void
jit_codegen_trace(rb_thread_t *th, jit_trace_t *trace)
{
	jit_trace_dump(th);
	JIT_DEBUG_LOG("=== Start jit_codegen_trace  ===");
	Module *module = RB_JIT->createModule();
	jit_codegen_func_t codegen_func = RB_JIT->createJITFunction(module);
	codegen_func.module = module;

	jit_insn_t **insns = trace->insns;
	unsigned insns_size = trace->insns_iterator;

	VALUE *first_pc = insns[0]->pc;
	auto *first_cfp = insns[0]->cfp;

	insns[insns_size] = new jit_insn_t;

	for (unsigned i = 0; i < insns_size + 1; i++) {
	// for (unsigned i = 0, len = 1; i < insns_size + 1; i += len) {
		auto *insn = insns[i];
		// if (!insn) {
		// 	// トレースしそこねた命令(branchlessなどでジャンプした場合に起こる)
		// 	// fprintf(stderr, "not tracing instruction: %d, %08p\n", i, first_pc + i);
		// 	jit_trace_insn(th, first_cfp, first_pc + i);
		// 	insn = insns[i];
		// 	// jit_dump_insn(insn);
		// }
		insn->bb = BasicBlock::Create(CONTEXT, "insn", codegen_func.jit_trace_func);
		// len = insn->len;
	}

	BUILDER->CreateBr(insns[0]->bb);

	// for (unsigned i = 0, len; i < insns_size; i+= len) {
	// 	jit_insn_t *insn = insns[i];
    //
	// 	jit_codegen_core(codegen_func, th->cfp, insns, insn);
    //
	// 	len = insn->len;
	// }
	for (unsigned i = 0; i < insns_size; i++) {
		if (auto *insn = insns[i]) {
			jit_dump_insn(insn);
			jit_codegen_core(codegen_func, insn->cfp, trace, i);
		}
	}

	BUILDER->SetInsertPoint(insns[insns_size]->bb);
	jit_codegen_make_return(codegen_func);

	// JIT_DEBUG_LOG("==== JITed instructions ====");
	// jit_trace_func->dump();
	//JIT_DEBUG_LOG("==== Optimized JITed instructions ====");
	jit_codegen_optimize(*codegen_func.jit_trace_func, module);
	// RB_JIT->optimizeFunction(*codegen_func.jit_trace_func, module);
	//JIT_DEBUG_RUN(codegen_func.jit_trace_func->dump());

	JIT_DEBUG_LOG("==== Compile instructions ====");
	trace->jited = RB_JIT->compileFunction(codegen_func.jit_trace_func);
}

static inline void
jit_codegen_core(
		jit_codegen_func_t codegen_func,
		rb_control_frame_t *cfp,
		jit_trace_t *trace,
		int index)
{
#define JIT_TRACE_FUNC	codegen_func.jit_trace_func
#define JIT_TH 			codegen_func.arg_th
#define JIT_CFP			codegen_func.arg_cfp
#define SP_GEP			codegen_func.sp_gep
#define EP_GEP			codegen_func.ep_gep
#define MODULE			codegen_func.module


#ifdef JIT_DEBUG_FLAG
#define PRINT_VAL(val) RB_JIT->createPrintf(MODULE, (val));
#else
#define PRINT_VAL(val)
#endif


#define JIT_CHECK_STACK_SIZE
#define jit_error(msg) (fprintf(stderr, msg" (%s, %d)", __FILE__, __LINE__), rb_raise(rb_eRuntimeError, "JIT error"), nullptr)
#undef TOPN
#define TOPN(x)\
	[&]{\
	Value *sp = BUILDER->CreateLoad(SP_GEP);\
	JIT_LLVM_SET_NAME(sp, "sp");\
	Value *sp_incptr = BUILDER->CreateInBoundsGEP(sp, RB_JIT->values->signedValue(-x)); /* -1?*/\
	JIT_LLVM_SET_NAME(sp_incptr, "sp_minus_" #x "_");\
	return BUILDER->CreateLoad(sp_incptr);}()
#undef POPN
#define POPN(x) {\
	Value *sp = BUILDER->CreateLoad(SP_GEP);\
	JIT_LLVM_SET_NAME(sp, "sp");\
	Value *sp_incptr = BUILDER->CreateInBoundsGEP(sp, RB_JIT->values->signedValue(-x));\
	JIT_LLVM_SET_NAME(sp_incptr, "sp_minus_" #x "_");\
	BUILDER->CreateStore(sp_incptr, SP_GEP);}
#undef PUSH
#define PUSH(x) {\
	Value *sp = BUILDER->CreateLoad(SP_GEP);\
	JIT_LLVM_SET_NAME(sp, "sp");\
	BUILDER->CreateStore((x), sp);\
	Value *sp_incptr = BUILDER->CreateInBoundsGEP(sp, RB_JIT->values->valueOne);\
	JIT_LLVM_SET_NAME(sp_incptr, "sp_plus_1_");\
	BUILDER->CreateStore(sp_incptr, SP_GEP);}

#undef CALL_METHOD
#define CALL_METHOD(ci) do { \
	Value *v = BUILDER->CreateCall3(ci_call, JIT_TH, JIT_CFP, ci);\
	Value *cond_v = BUILDER->CreateICmpNE(v, RB_JIT->values->valueQundef, "ifcond");\
    BasicBlock *then_block = BasicBlock::Create(CONTEXT, "then", JIT_TRACE_FUNC);\
    BasicBlock *else_block = BasicBlock::Create(CONTEXT, "else", JIT_TRACE_FUNC);\
    BUILDER->CreateCondBr(cond_v, then_block, else_block);\
	BUILDER->SetInsertPoint(then_block);\
	PUSH(v);\
	BUILDER->CreateBr(else_block);\
	BUILDER->SetInsertPoint(else_block);\
} while (0)
#define GET_SELF() RB_JIT->values->value(cfp->self)
#define GET_EP() BUILDER->CreateLoad(EP_GEP)
#define GET_LEP() (JIT_EP_LEP(GET_EP()))
#undef GET_OPERAND
#define GET_OPERAND(x) (insn->pc[(x)])

#define RSHIFT(x,y) BUILDER->CreateLShr(x, y)
#undef FIX2LONG
#define FIX2LONG(x) (RSHIFT((x), 1))

#undef INT2FIX
#define INT2FIX(i) BUILDER->CreateOr(BUILDER->CreateShl(i, 1), FIXNUM_FLAG)
#undef LONG2FIX
#define LONG2FIX(i) INT2FIX(i)
#undef LONG2NUM
#define LONG2NUM(i) LONG2FIX(i)
#define OFFSET long

// #define RTEST(v) !(((VALUE)(v) & ~Qnil) == 0)
#undef RTEST
#define RTEST(v) (BUILDER->CreateICmpNE(BUILDER->CreateAnd((v), ~Qnil), RB_JIT->values->valueZero))
#undef JIT_CHECK_STACK_SIZE

// #define NEXT_INSN() (insns[insn->index + insn->len])
#define NEXT_INSN() (insns[index + 1])

	unsigned insns_size = trace->insns_iterator;
	jit_insn_t **insns = trace->insns;
	jit_insn_t *insn = insns[index];

	switch (insn->opecode) {
#include "jit_codegen.inc"
	}
}
