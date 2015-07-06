#include "internal.h"
#include "insns.inc"
#include "insns_info.inc"
#include "vm_core.h"
#include "jit.h"

#include "llvm/ExecutionEngine/ExecutionEngine.h"
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

extern "C" {
void vm_search_method(rb_call_info_t *ci, VALUE recv);
}

#define JIT_DEBUG_LOG(format) do{ fprintf(stderr, format "\n"); }while(0)
#define JIT_DEBUG_LOG2(format, ...) do{ fprintf(stderr, format "\n", __VA_ARGS__); }while(0)

int is_jit_tracing = 0;

typedef struct jit_insn_struct {
	rb_thread_t *th;
	rb_control_frame_t *cfp;
	VALUE *pc;
	int opecode;
	int index;
	int len;
	BasicBlock *bb;
} jit_insn_t;

typedef struct jit_trace_struct {
	std::deque<jit_insn_t*> insns;
	// jit_insn_t **insns = nullptr;
	unsigned insns_size = 0;
	// unsigned insns_iterator = 0;
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

	Value *valueZero;
	Value *valueOne;
	Value *int32Zero;
	Value *valueQundef;

	Type *llvm_thread_t;
	Type *llvm_control_frame_t;
	Type *llvm_call_info_t;

	Function *llvm_search_method;

	jit_trace_t trace;

	JitCompiler()
	: builder(std::unique_ptr<IRBuilder<>>(new IRBuilder<>(getGlobalContext())))
	{

		valueZero = builder->getInt64(0);
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

		llvm_thread_t = module->getTypeByName("struct.rb_thread_struct")->getPointerTo();
		llvm_control_frame_t = module->getTypeByName("struct.rb_control_frame_struct")->getPointerTo();
		llvm_call_info_t = module->getTypeByName("struct.rb_call_info_struct")->getPointerTo();

		llvm_search_method = module->getFunction("vm_search_method");
		sys::DynamicLibrary::AddSymbol("_vm_search_method", (void *)vm_search_method); // addGlobalMapping was failed in MCJIT
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
#define RB_JIT rb_mJit

extern "C"
void
jit_trace_start(rb_thread_t *th)
{
	RB_JIT->trace.insns_size = th->cfp->iseq->iseq_size;
	is_jit_tracing = 1;
	// if (RB_JIT->trace.insns) delete RB_JIT->trace.insns; // TODO:delete children
	// RB_JIT->trace.insns = new jit_insn_t*[RB_JIT->trace.insns_size];
}

extern "C"
void
jit_push_new_trace(rb_control_frame_t *cfp)
{
	if (!cfp->iseq) return; // CFUNC
	is_jit_tracing = 0;
}

extern "C"
void
jit_pop_trace()
{
}

static void
jit_insn_init(jit_insn_t *insn, rb_thread_t *th, rb_control_frame_t *cfp, VALUE *pc)
{
	rb_iseq_t *iseq = cfp->iseq;
	VALUE *iseq_encoded = rb_iseq_original_iseq(iseq);

	insn->index = pc - iseq->iseq_encoded;
	insn->opecode = (int)iseq_encoded[insn->index];
	insn->len = insn_len(insn->opecode);
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
	for (int i=1, len=insn_len(insn->opecode); i<len; i++)
		trace.insns.push_back(nullptr);
	// trace.insns[trace.insns_iterator] = insn;
	// trace.insns_iterator += insn_len(insn->opecode);
}

extern "C"
void
jit_trace_dump(rb_thread_t *th)
{
	// JIT_DEBUG_LOG2("=== Start trace dump (length: %lu) ===", rb_mJit->trace.insns_size);
	JIT_DEBUG_LOG2("=== Start trace dump (length: %lu) ===", rb_mJit->trace.insns.size());
	// for (auto insn : rb_mJit->trace.insns) {
	unsigned i = 0;
	while (i < RB_JIT->trace.insns.size()) {
	// while (i < RB_JIT->trace.insns_size) {
		jit_insn_t *insn = RB_JIT->trace.insns[i];
		JIT_DEBUG_LOG2("%08p: %s(%d)", insn->pc, insn_name(insn->opecode), insn->opecode);
		i += insn->len;
	}
	JIT_DEBUG_LOG("=== End trace dump ===");
	void jit_codegen(rb_thread_t *th);
	jit_codegen(th);
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
}

extern "C"
void
ruby_jit_test(void)
{
}

extern "C"
VALUE
jit_insn_to_llvm(rb_thread_t *th)
{
	return Qtrue;
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


#define CONTEXT getGlobalContext()
#define MODULE RB_JIT->module
#define BUILDER RB_JIT->builder
#define ENGINE RB_JIT->engine

#include "jit_core.h"

void
jit_codegen(rb_thread_t *th)
{
#define JIT_DEBUG_SET_NAME_MODE
#ifdef  JIT_DEBUG_SET_NAME_MODE
#define JIT_LLVM_SET_NAME(v, name) do { v->setName(name); } while(0)
#else
#define JIT_LLVM_SET_NAME(v, name)
#endif

	FunctionType* jit_trace_func_t = FunctionType::get(RB_JIT->voidTy,
			{ RB_JIT->llvm_thread_t, RB_JIT->llvm_control_frame_t }, false);
	Function *jit_trace_func = Function::Create(jit_trace_func_t,
			GlobalValue::ExternalLinkage, "jit_trace_func1", MODULE);
	jit_trace_func->setCallingConv(CallingConv::C);

	BasicBlock *entry = BasicBlock::Create(CONTEXT, "entry", jit_trace_func);
	BUILDER->SetInsertPoint(entry);

	Function::arg_iterator args = jit_trace_func->arg_begin();
	Argument *arg_th = args++, *arg_cfp = args++;

	Value *sp_gep = BUILDER->CreateStructGEP(arg_cfp, 1); JIT_LLVM_SET_NAME(sp_gep, "sp_ptr");
	Value *ep_gep = BUILDER->CreateStructGEP(arg_cfp, 6); JIT_LLVM_SET_NAME(ep_gep, "ep_ptr");

#define JIT_TRACE_FUNC jit_trace_func

#define JIT_TH arg_th
#define JIT_CFP arg_cfp
#define SP_GEP sp_gep
#define EP_GEP ep_gep

	std::deque<Value*> __stack;
#define JIT_STACK __stack

#define JIT_CHECK_STACK_SIZE
#define jit_error(msg) (fprintf(stderr, msg" (%s, %d)", __FILE__, __LINE__), rb_raise(rb_eRuntimeError, "JIT error"), nullptr)
#undef TOPN
#define TOPN(x)\
	[&]{\
	Value *sp = BUILDER->CreateLoad(SP_GEP);\
	JIT_LLVM_SET_NAME(sp, "sp");\
	Value *sp_incptr = BUILDER->CreateInBoundsGEP(sp, RB_JIT->signedVal(-x));\
	JIT_LLVM_SET_NAME(sp_incptr, "sp_minus_" #x "_");\
	return BUILDER->CreateLoad(sp_incptr);}()
// #ifdef JIT_CHECK_STACK_SIZE
// #define TOPN(x) (JIT_STACK.size() > (x)? JIT_STACK[x] : jit_error("Out of range..."))
// #else
// #define TOPN(x) (JIT_STACK[x])
// #endif
#undef POPN
#define POPN(x) {\
	Value *sp = BUILDER->CreateLoad(SP_GEP);\
	JIT_LLVM_SET_NAME(sp, "sp");\
	Value *sp_incptr = BUILDER->CreateInBoundsGEP(sp, RB_JIT->signedVal(-x));\
	JIT_LLVM_SET_NAME(sp_incptr, "sp_minus_" #x "_");\
	BUILDER->CreateStore(sp_incptr, SP_GEP);}
// #ifdef JIT_CHECK_STACK_SIZE
// #define POPN(x) do { for (int i=0; i<(x); i++) if (JIT_STACK.size() > 0) JIT_STACK.pop_front(); else jit_error("Out of range..."); } while(0)
// #else
// #define POPN(x) do { for (int i=0; i<(x); i++) JIT_STACK.pop_front(); } while(0)
// #endif
#undef PUSH
#define PUSH(x) {\
	Value *sp = BUILDER->CreateLoad(SP_GEP);\
	JIT_LLVM_SET_NAME(sp, "sp");\
	BUILDER->CreateStore((x), sp);\
	Value *sp_incptr = BUILDER->CreateInBoundsGEP(sp, RB_JIT->valueOne);\
	JIT_LLVM_SET_NAME(sp_incptr, "sp_plus_1_");\
	BUILDER->CreateStore(sp_incptr, SP_GEP);}
// #define PUSH(x) JIT_STACK.push_front(x)

#define POP2VM() \
	Value *sp = BUILDER->CreateLoad(SP_GEP);\
	JIT_LLVM_SET_NAME(sp, "sp");\
	BUILDER->CreateStore(JIT_STACK.front(), sp); JIT_STACK.pop_front();\
	Value *sp_incptr = BUILDER->CreateGEP(sp, RB_JIT->valueOne);\
	BUILDER->CreateStore(sp_incptr, SP_GEP);

#undef CALL_METHOD
#define CALL_METHOD(ci) do { \
	Value *v = BUILDER->CreateCall3(ci_call, JIT_TH, JIT_CFP, ci); \
	Value *cond_v = BUILDER->CreateICmpNE(v, RB_JIT->valueQundef, "ifcond");\
    BasicBlock *then_block = BasicBlock::Create(CONTEXT, "then", jit_trace_func);\
    BasicBlock *else_block = BasicBlock::Create(CONTEXT, "else", jit_trace_func);\
    BUILDER->CreateCondBr(cond_v, then_block, else_block);\
	BUILDER->SetInsertPoint(then_block);\
	PUSH(v);\
	BUILDER->CreateBr(else_block);\
	BUILDER->SetInsertPoint(else_block);\
} while (0)
#define GET_SELF() RB_JIT->valueVal(th->cfp->self)
#define GET_EP() BUILDER->CreateLoad(EP_GEP)
#define GET_LEP() (JIT_EP_LEP(GET_EP()))
#undef GET_OPERAND
#define GET_OPERAND(x) (insn->pc[(x)])

#define RSHIFT(x,y) BUILDER->CreateLShr(x, y)
#define FIX2LONG(x) (RSHIFT((x), 1))

#define INT2FIX(i) BUILDER->CreateOr(BUILDER->CreateShl(i, 1), FIXNUM_FLAG)
#define LONG2FIX(i) INT2FIX(i)
#define LONG2NUM(i) LONG2FIX(i)
typedef long OFFSET;

// #define RTEST(v) !(((VALUE)(v) & ~Qnil) == 0)
#define RTEST(v) (BUILDER->CreateICmpNE(BUILDER->CreateAnd((v), ~Qnil), RB_JIT->valueZero))
#undef JIT_CHECK_STACK_SIZE

	// for (auto insn : RB_JIT->trace.insns) {
	//

	unsigned i = 0;
	// while (i < RB_JIT->trace.insns_size) {
	while (i < RB_JIT->trace.insns.size()) {
		auto &insns = RB_JIT->trace.insns;
		jit_insn_t *insn = insns[i];

		switch (insn->opecode) {
#include "jit_codegen.inc"
		}

		i += insn->len;
	}
	BUILDER->CreateRetVoid();
	// JIT_DEBUG_LOG("==== JITed instructions ====");
	// jit_trace_func->dump();
	JIT_DEBUG_LOG("==== Optimized JITed instructions ====");
	RB_JIT->optimizeFunction(*jit_trace_func);
	jit_trace_func->dump();

	ENGINE->finalizeObject();
	void* pfptr = ENGINE->getPointerToFunction(jit_trace_func);
	void (* fptr)(rb_thread_t *th, rb_control_frame_t*) = (void (*)(rb_thread_t *th, rb_control_frame_t*))pfptr;
	JIT_DEBUG_LOG("==== Execute JITed function ====");
	fptr(th, th->cfp);
}

