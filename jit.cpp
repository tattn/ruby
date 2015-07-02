#include "internal.h"
#include "insns.inc"
#include "insns_info.inc"
#include "vm_core.h"
#include "jit.h"

#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/ExecutionEngine/RTDyldMemoryManager.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Transforms/Scalar.h"

#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Linker/Linker.h"
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <sstream>

using namespace llvm;

#define JIT_DEBUG_LOG(format) do{ fprintf(stderr, format "\n"); }while(0)
#define JIT_DEBUG_LOG2(format, ...) do{ fprintf(stderr, format "\n", __VA_ARGS__); }while(0)

int is_jit_tracing = 0;

typedef struct jit_insn_struct {
	rb_thread_t *th;
	rb_control_frame_t *cfp;
	VALUE *pc;
	int opecode;
	std::vector<VALUE> operands;
} jit_insn_t;

typedef struct jit_trace_struct {
	std::deque<jit_insn_t*> insns;
} jit_trace_t;

class JitCompiler
{
public:
	std::unique_ptr<IRBuilder<>> builder;
	std::unique_ptr<ExecutionEngine> engine;
	Module *module;

	Type *voidTy = Type::getVoidTy(getGlobalContext());
	Type *int8Ty = Type::getInt8Ty(getGlobalContext());
	Type *int64Ty = Type::getInt64Ty(getGlobalContext());
	Type *ptrTy = PointerType::getUnqual(int8Ty);

	Type *valueTy = int64Ty;
	Type *valuePtrTy = PointerType::getUnqual(int64Ty);

	Value* valueVal(VALUE val) { return builder->getInt64(val); }
	Value *signedVal(int val) { return llvm::ConstantInt::getSigned(int64Ty, val); }

	Value *valueOne;

	Value *int32Zero;

	Value *valueQundef;

	jit_trace_t trace;

	JitCompiler()
	: builder(std::unique_ptr<IRBuilder<>>(new IRBuilder<>(getGlobalContext())))
	{

		valueOne = builder->getInt64(1);
		int32Zero = builder->getInt32(0);
		valueQundef = builder->getInt64(Qundef);

		auto Owner = make_unique<Module>("Ruby LLVM Module", getGlobalContext());
		module = Owner.get();

		std::string error;
		std::unique_ptr<RTDyldMemoryManager> MemMgr(new SectionMemoryManager());

		engine = std::unique_ptr<ExecutionEngine>(EngineBuilder(std::move(Owner))
			.setEngineKind(llvm::EngineKind::JIT)
			.setMCJITMemoryManager(std::move(MemMgr))
			.setErrorStr(&error)
			.create());

		const char *ruby_module =
			#include "tool/jit/jit_typedef.inc"
		parseAndLink(ruby_module);
	}

	~JitCompiler()
	{

	}

	void dump()
	{
		module->dump();
	}

	void parseAndLink(const char *bitcode)
	{
		std::unique_ptr<MemoryBuffer> buffer = MemoryBuffer::getMemBuffer(bitcode);

		SMDiagnostic err;
		std::unique_ptr<Module> newModule = parseIR(buffer->getMemBufferRef(), err, getGlobalContext());
		if (!newModule) {
			JIT_DEBUG_LOG2("ERROR: %s", err.getMessage().str().c_str());
			return;
		}

		if (Linker::LinkModules(module, newModule.get())) {
			JIT_DEBUG_LOG("Module link error");
		}
	}

	void optimizeFunction(Function& f)
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
		// fpm.add(createLowerSimdLoopPass());
		fpm.add(createLICMPass());
		fpm.add(createIndVarSimplifyPass());
		fpm.add(createLoopDeletionPass());
		// fpm.add(createLoopVectorizePass());
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


};

std::unique_ptr<JitCompiler> rb_mJit;

static void
jit_insn_init(jit_insn_t *insn, rb_thread_t *th, rb_control_frame_t *cfp, VALUE *pc)
{
	rb_iseq_t *iseq = cfp->iseq;
	VALUE *iseq_encoded = rb_iseq_original_iseq(iseq);
	int offset = pc - iseq->iseq_encoded;
	insn->opecode = (int)iseq_encoded[offset];
	insn->th = th;
	insn->cfp = cfp;
	insn->pc = pc;
}

extern "C"
void
jit_trace_insn(rb_thread_t *th, rb_control_frame_t *cfp, VALUE *pc)
{
	jit_insn_t *insn = new jit_insn_t;
	jit_insn_init(insn, th, cfp, pc);
	// TODO: トレースするかを実行回数などで判定
	jit_trace_t &trace = rb_mJit->trace;
	trace.insns.push_back(insn);

	switch (insn->opecode) {
		case BIN(putobject):
			insn->operands.push_back(pc[1]);
			break;
		case BIN(opt_plus):
			insn->operands.push_back(pc[1]);
			break;
		case BIN(opt_send_without_block):
			insn->operands.push_back(pc[1]);
			break;
	}
}

extern "C"
void
jit_trace_dump(rb_thread_t *th)
{
	JIT_DEBUG_LOG2("=== Start trace dump (length: %lu) ===", rb_mJit->trace.insns.size());
	for (auto insn : rb_mJit->trace.insns) {
		JIT_DEBUG_LOG2("trace: %s(%d)", insn_name(insn->opecode), insn->opecode);
	}
	JIT_DEBUG_LOG("=== End trace dump ===");
	void jit_codegen(rb_thread_t *th);
	jit_codegen(th);
}

void
jit_codegen(rb_thread_t *th)
{
#define JIT_DEBUG_SET_NAME_MODE
#ifdef  JIT_DEBUG_SET_NAME_MODE
#define JIT_LLVM_SET_NAME(v, name) do { v->setName(name); } while(0)
#else
#define JIT_LLVM_SET_NAME(v, name)
#endif

	Type *llvm_thread_t = rb_mJit->module->getTypeByName("struct.rb_thread_struct")->getPointerTo();
	Type *llvm_control_frame = rb_mJit->module->getTypeByName("struct.rb_control_frame_struct")->getPointerTo();


	Function *llvm_search_method = rb_mJit->module->getFunction("vm_search_method");
	Function *llvm_caller_setup_arg_block = rb_mJit->module->getFunction("vm_caller_setup_arg_block");


	FunctionType* jit_trace_func_t = FunctionType::get(rb_mJit->voidTy, { llvm_thread_t, llvm_control_frame }, false);
	Function *jit_trace_func = Function::Create(jit_trace_func_t, GlobalValue::ExternalLinkage, "jit_trace_func1", rb_mJit->module);
	jit_trace_func->setCallingConv(CallingConv::C);

	Function::arg_iterator args = jit_trace_func->arg_begin();
	Argument *arg_th = args++;
	Argument *arg_cfp = args++;


	BasicBlock *entry = BasicBlock::Create(getGlobalContext(), "entry", jit_trace_func);
	rb_mJit->builder->SetInsertPoint(entry);

	std::deque<Value*> __stack;
#define JIT_STACK __stack

#define JIT_CHECK_STACK_SIZE
#define jit_error(msg) (fprintf(stderr, msg" (%s, %d)", __FILE__, __LINE__), rb_raise(rb_eRuntimeError, "JIT error"), nullptr)
#undef TOPN
#define TOPN(x)\
	[&]{\
	Value *sp_elmptr = rb_mJit->builder->CreateStructGEP(arg_cfp, 1);\
	JIT_LLVM_SET_NAME(sp_elmptr, "sp_ptr");\
	Value *sp = rb_mJit->builder->CreateLoad(sp_elmptr);\
	JIT_LLVM_SET_NAME(sp, "sp");\
	Value *sp_incptr = rb_mJit->builder->CreateInBoundsGEP(sp, rb_mJit->signedVal(-x));\
	JIT_LLVM_SET_NAME(sp_incptr, "sp_minus_" #x "_");\
	return rb_mJit->builder->CreateLoad(sp_incptr);}()
// #ifdef JIT_CHECK_STACK_SIZE
// #define TOPN(x) (JIT_STACK.size() > (x)? JIT_STACK[x] : jit_error("Out of range..."))
// #else
// #define TOPN(x) (JIT_STACK[x])
// #endif
#undef POPN
#define POPN(x) {\
	Value *sp_elmptr = rb_mJit->builder->CreateStructGEP(arg_cfp, 1);\
	JIT_LLVM_SET_NAME(sp_elmptr, "sp_ptr");\
	Value *sp = rb_mJit->builder->CreateLoad(sp_elmptr);\
	JIT_LLVM_SET_NAME(sp, "sp");\
	Value *sp_incptr = rb_mJit->builder->CreateInBoundsGEP(sp, rb_mJit->signedVal(-x));\
	JIT_LLVM_SET_NAME(sp_incptr, "sp_minus_" #x "_");\
	rb_mJit->builder->CreateStore(sp_incptr, sp_elmptr);}
// #ifdef JIT_CHECK_STACK_SIZE
// #define POPN(x) do { for (int i=0; i<(x); i++) if (JIT_STACK.size() > 0) JIT_STACK.pop_front(); else jit_error("Out of range..."); } while(0)
// #else
// #define POPN(x) do { for (int i=0; i<(x); i++) JIT_STACK.pop_front(); } while(0)
// #endif
#undef PUSH
#define PUSH(x) {\
	Value *sp_elmptr = rb_mJit->builder->CreateStructGEP(arg_cfp, 1);\
	JIT_LLVM_SET_NAME(sp_elmptr, "sp_ptr");\
	Value *sp = rb_mJit->builder->CreateLoad(sp_elmptr);\
	JIT_LLVM_SET_NAME(sp, "sp");\
	rb_mJit->builder->CreateStore((x), sp);\
	Value *sp_incptr = rb_mJit->builder->CreateInBoundsGEP(sp, rb_mJit->valueOne);\
	JIT_LLVM_SET_NAME(sp_incptr, "sp_plus_1_");\
	rb_mJit->builder->CreateStore(sp_incptr, sp_elmptr);}
// #define PUSH(x) JIT_STACK.push_front(x)

#define POP2VM() \
	Value *sp_elmptr = rb_mJit->builder->CreateStructGEP(arg_cfp, 1);\
	JIT_LLVM_SET_NAME(sp_elmptr, "sp_ptr");\
	Value *sp = rb_mJit->builder->CreateLoad(sp_elmptr);\
	JIT_LLVM_SET_NAME(sp, "sp");\
	rb_mJit->builder->CreateStore(JIT_STACK.front(), sp); JIT_STACK.pop_front();\
	Value *sp_incptr = rb_mJit->builder->CreateGEP(sp, rb_mJit->valueOne);\
	rb_mJit->builder->CreateStore(sp_incptr, sp_elmptr);

#undef CALL_METHOD
#define CALL_METHOD(ci) do { \
	Value *v = rb_mJit->builder->CreateCall3(ci_call, arg_th, arg_cfp, ci); \
	Value *cond_v = rb_mJit->builder->CreateICmpNE(v, rb_mJit->valueQundef, "ifcond");\
    BasicBlock *then_block = BasicBlock::Create(getGlobalContext(), "then", jit_trace_func);\
    BasicBlock *else_block = BasicBlock::Create(getGlobalContext(), "else", jit_trace_func);\
    rb_mJit->builder->CreateCondBr(cond_v, then_block, else_block);\
	rb_mJit->builder->SetInsertPoint(then_block);\
	PUSH(v);\
	rb_mJit->builder->CreateBr(else_block);\
	rb_mJit->builder->SetInsertPoint(else_block);\
} while (0)
#undef JIT_CHECK_STACK_SIZE


	for (auto insn : rb_mJit->trace.insns) {
		switch (insn->opecode) {
#include "jit_codegen.inc"
		}
	}

	rb_mJit->builder->CreateRetVoid();
	JIT_DEBUG_LOG("==== JITed instructions ====");
	jit_trace_func->dump();
	JIT_DEBUG_LOG("==== Optimized JITed instructions ====");
	rb_mJit->optimizeFunction(*jit_trace_func);
	jit_trace_func->dump();

	sys::DynamicLibrary::AddSymbol("_vm_search_method", (void *)vm_search_method);
	// sys::DynamicLibrary::AddSymbol("_vm_caller_setup_arg_block", (void *)vm_caller_setup_arg_block);
	rb_mJit->engine->finalizeObject();
	rb_mJit->engine->addGlobalMapping(llvm_search_method, (void *)vm_search_method);
	void* pfptr = rb_mJit->engine->getPointerToFunction(jit_trace_func);
	void (* fptr)(rb_thread_t *th, rb_control_frame_t*) = (void (*)(rb_thread_t *th, rb_control_frame_t*))pfptr;
	JIT_DEBUG_LOG("==== Execute JITed function ====");
	fptr(th, th->cfp);
}

extern "C"
void
ruby_jit_init(void)
{
    LLVMInitializeX86AsmPrinter();
    LLVMInitializeX86Disassembler();
	LLVMLinkInMCJIT();
	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();
	InitializeNativeTargetAsmParser();
	rb_mJit = make_unique<JitCompiler>();
	// printf("Initialized LLVM!\n");
}

extern "C"
void
ruby_jit_test(void)
{
}

// extern "C"
// VALUE
// rb_jit_compile_node(VALUE self, NODE *node)
// {
// 	// Node to LLVM が必要なら実装
// 	assert("Not implementation");
// }

extern "C"
VALUE
jit_insn_to_llvm(rb_thread_t *th)
{
	return Qtrue;
	rb_iseq_t *iseq;
	iseq = th->cfp->iseq;

#if OPT_DIRECT_THREADED_CODE || OPT_CALL_THREADED_CODE
	unsigned int i;

	VALUE* iseq_no_threaded = rb_iseq_original_iseq(iseq);

	for (i = 0; i < iseq->iseq_size; /* */ ) {
		int insn = (int)iseq_no_threaded[i];

		if (insn == BIN(opt_plus)) {
			fprintf(stderr, "足し算");
		}

		int len = insn_len(insn);
		i += len;
	}
#endif
}

static VALUE
jit_compile(VALUE self, VALUE code)
{
	static VALUE name = rb_str_new2("<internal:jit>");
	static VALUE line = INT2FIX(1);
	VALUE iseqval = rb_iseq_compile_with_option(code, name, Qnil, line, 0, Qtrue);

	rb_iseq_t *iseq;
	GetISeqPtr(iseqval, iseq);

#if OPT_DIRECT_THREADED_CODE || OPT_CALL_THREADED_CODE
	VALUE *iseq_encoded = rb_iseq_original_iseq(iseq);
#else
	VALUE *iseq_encoded = iseq->iseq_encoded;
#endif

	VALUE str = rb_iseq_disasm(iseq->self);
	printf("disasm[self]: %s\n", StringValueCStr(str));

	std::deque<VALUE> vm_stack;
	// std::deque<Value*> vm_stack;

    // rb_thread_t * th = (rb_thread_t*)ruby_mimmalloc(sizeof(*th));
    // MEMZERO(th, rb_thread_t, 1);
    // th_init(th, 0);
    //
    // VALUE toplevel_binding = rb_const_get(rb_cObject, rb_intern("TOPLEVEL_BINDING"));
    // rb_binding_t *bind;
    // GetBindingPtr(toplevel_binding, bind);
    //
    // rb_env_t *env;
    // GetEnvPtr(bind->env, env);
	// env->block->self;
	// printf("%p, %p\n", &env->block, th->cfp);

    // vm_set_eval_stack(th, iseqval, 0, &env->block);
	// printf("debug line 2\n");

	rb_thread_t *th = ruby_current_thread;

#undef GET_OPERAND
#define GET_OPERAND(x) (iseq_encoded[i+(x)])
#undef TOPN
#define TOPN(x) (vm_stack[vm_stack.size() - (x)])
#undef PUSH
#define PUSH(x) (*(th->cfp->sp++) = (x))


	Function *jit_main_func = rb_mJit->module->getFunction("jit_main_func");
	if (!jit_main_func) {
		auto jit_main_func_type = FunctionType::get(rb_mJit->voidTy, {}, false);
		jit_main_func = Function::Create(jit_main_func_type, Function::ExternalLinkage,
				"jit_main_func", rb_mJit->module);

		BasicBlock *entry = BasicBlock::Create(getGlobalContext(), "entry", jit_main_func);
		rb_mJit->builder->SetInsertPoint(entry);
	}

	for (unsigned int i = 0; i < iseq->iseq_size; /* */ ) {
		int insn = (int)iseq_encoded[i];

		switch (insn) {
			case BIN(putself):
				JIT_DEBUG_LOG2("log: putself[%s]", rb_obj_classname(th->cfp->self));
				vm_stack.push_back(th->cfp->self);
				break;
			case BIN(putobject):
				JIT_DEBUG_LOG("log: putobject");
				vm_stack.push_back(iseq_encoded[i+1]);
				break;
			case BIN(opt_plus): {
				VALUE rb_r = vm_stack.back(); vm_stack.pop_back();
				VALUE rb_l = vm_stack.back(); vm_stack.pop_back();
				// VALUE rb_arg = iseq_encoded[i+1];
				// if (FIXNUM_2_P(rb_r, rb_l) &&
				// 		BASIC_OP_UNREDEFINED_P(BOP_PLUS,FIXNUM_REDEFINED_OP_FLAG)) {
					int l = FIX2INT(rb_l);
					int r = FIX2INT(rb_r);
					// Value *ret = rb_mJit->builder->CreateAdd(arg1, arg2);

				// }

				// Function *opt_plus = rb_mJit->module->getFunction("opt_plus");
				// if (!opt_plus) {
				// 	Type *intTy = rb_mJit->int64Ty;
				// 	auto opt_plus_type = FunctionType::get(intTy, { intTy, intTy }, false);
				// 	opt_plus = Function::Create(opt_plus_type, Function::ExternalLinkage,
				// 			"opt_plus", rb_mJit->module);
                //
				// 	BasicBlock *entry = BasicBlock::Create(getGlobalContext(), "entry", opt_plus);
				// 	rb_mJit->builder->SetInsertPoint(entry);
                //
				// 	Function::arg_iterator args = opt_plus->arg_begin();
				// 	Argument *arg1 = args++;
				// 	Argument *arg2 = args++;
                //
				// 	Value *ret = rb_mJit->builder->CreateAdd(arg1, arg2);
				// 	rb_mJit->builder->CreateRet(ret);
				// 	opt_plus->dump();
				// 	rb_mJit->engine->finalizeObject();
				// }
                //
				// void* fptr = rb_mJit->engine->getPointerToFunction(opt_plus);
				// int (* jit_opt_plus)(int, int) = (int (*)(int, int))fptr;
                //
				// int result = jit_opt_plus(l, r);
				// // fprintf(stderr, "Result: %d (%d + %d)\n", result, l, r);
				// PUSH(INT2FIX(result));
				break;
								}
			case BIN(opt_send_without_block): {
				CALL_INFO ci = (CALL_INFO)GET_OPERAND(1);
				ci->argc = ci->orig_argc;

				vm_search_method(ci, ci->recv = TOPN(ci->argc)); //segv

				rb_control_frame_t *cfp = th->cfp;

				// CALL_METHOD(ci);
				JIT_DEBUG_LOG2("log: send[%s]", rb_id2name(ci->mid));
				VALUE v = (*(ci)->call)(th, cfp, ci);
				break;
											  }
		}

		int len = insn_len(insn);
		i += len;
	}

	rb_mJit->builder->CreateRetVoid();
	jit_main_func->dump();
	rb_mJit->engine->finalizeObject();
	void* pfptr = rb_mJit->engine->getPointerToFunction(jit_main_func);
	void (* fptr)() = (void (*)())pfptr;
	fptr();

	return Qtrue;
}



extern "C"
void
Init_JIT(void)
{
	ruby_jit_init();
	VALUE rb_mJIT = rb_define_module("JIT");
	rb_define_singleton_method(rb_mJIT, "compile", RUBY_METHOD_FUNC(jit_compile), 1);
	// rb_define_singleton_method(rb_mJIT, "trace_dump", RUBY_METHOD_FUNC(jit_trace_dump), 0);
}

