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
#include "llvm/Support/Signals.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Linker/Linker.h"
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <unordered_map>
#include <list>
#include <unordered_set>
#include <sstream>


using namespace llvm;

extern "C" {
void vm_search_method(rb_call_info_t *ci, VALUE recv);
void vm_caller_setup_arg_block(const rb_thread_t *th, rb_control_frame_t *reg_cfp, rb_call_info_t *ci, const int is_super);
void vm_pop_frame(rb_thread_t *th);
}

#define JIT_DEBUG_FLAG

#ifdef JIT_DEBUG_FLAG
#define JIT_DEBUG_RUN(stmt) stmt
#define JIT_DEBUG_LOG(format) do{ fprintf(stderr, format "\n"); }while(0)
#define JIT_DEBUG_LOG2(format, ...) do{ fprintf(stderr, format "\n", __VA_ARGS__); }while(0)
#define JIT_LLVM_SET_NAME(v, name) do { v->setName(name); } while(0)
#else
#define JIT_DEBUG_RUN(stmt)
#define JIT_DEBUG_LOG(format)
#define JIT_DEBUG_LOG2(format, ...)
#define JIT_LLVM_SET_NAME(v, name)
#endif

int is_jit_tracing = 0;
int trace_stack_size = 0;

typedef struct jit_insn_struct {
	rb_thread_t *th;
	rb_control_frame_t *cfp;
	VALUE *pc;
	int opecode;
	int index;
	int len = 1;
	BasicBlock *bb;
} jit_insn_t;

typedef struct jit_func_result_struct {
	rb_control_frame_t *exit_cfp;
} jit_func_ret_t;

typedef struct jit_trace_struct {
	// std::deque<jit_insn_t*> insns;
	jit_insn_t **insns = nullptr;
	unsigned insns_size = 0;
	unsigned insns_iterator = 0;
	rb_iseq_t *iseq;

	std::unordered_set<VALUE*> pc_set;
	std::function<jit_func_ret_t(rb_thread_t*, rb_control_frame_t*)> jited = nullptr;
} jit_trace_t;

struct jit_codegen_func_t {
	Function *jit_trace_func;
	Argument *arg_th;
	Argument *arg_cfp;
	Value *sp_gep;
	Value *ep_gep;
	Module *module;
};

class JitCompiler
{
public:
	std::unique_ptr<IRBuilder<>> builder;
	std::list<std::unique_ptr<ExecutionEngine>> engines;
	// std::list<ExecutionEngine*> engines;

	Type *voidTy = Type::getVoidTy(getGlobalContext());
	Type *int8Ty = Type::getInt8Ty(getGlobalContext());
	Type *int32Ty = Type::getInt32Ty(getGlobalContext());
	Type *int64Ty = Type::getInt64Ty(getGlobalContext());
	Type *ptrTy = PointerType::getUnqual(int8Ty);
	Type *valueTy = int64Ty;
	Type *valuePtrTy = PointerType::getUnqual(int64Ty);

	Value* valueVal(VALUE val) { return builder->getInt64(val); }
	Value *signedVal(int val) { return llvm::ConstantInt::getSigned(int64Ty, val); }

	Value *valueZero = ConstantInt::get(int64Ty, 0);
	Value *valueOne = ConstantInt::get(int64Ty, 1);
	Constant *int32Zero = ConstantInt::get(int32Ty, 0);
	Value *valueQundef = ConstantInt::get(int64Ty, Qundef);

	Type *llvm_thread_t;
	Type *llvm_control_frame_t;
	Type *llvm_call_info_t;

	StructType *llvm_jit_func_ret_t;

	Function *llvm_search_method = nullptr;
	Function *llvm_caller_setup_arg_block;
	Function *llvm_pop_frame;

	jit_trace_t *trace = nullptr;
	std::unordered_map<VALUE*, jit_trace_t*> traces;
	std::list<jit_trace_t*> trace_list;

	std::unordered_map<VALUE*, rb_iseq_t*> iseq_list;

	int size;

	JitCompiler()
	: builder(std::unique_ptr<IRBuilder<>>(new IRBuilder<>(getGlobalContext())))
	{
		size = 0;
	}

	~JitCompiler()
	{
		JIT_DEBUG_LOG("==== Destroy JitCompiler ====");
		JIT_DEBUG_LOG2("ExecutionEngine: size=%d\n", engines.size());
		auto begin = engines.begin();
		auto end = engines.end();
		for (auto it = begin; it != end; ++it) {
			it->release();
			// delete *it;
		}
	}

	void createPrintf(llvm::Module *mod, llvm::Value* val)
	{
		Function *func = mod->getFunction("printf");
		GlobalVariable* var = mod->getGlobalVariable(".str", true);

		if (!func) {
			FunctionType *printf_type = FunctionType::get(
					int32Ty, { int8Ty->getPointerTo() }, true);

			func = Function::Create(
					printf_type, Function::ExternalLinkage,
					Twine("printf"),
					mod
					);
			func->setCallingConv(CallingConv::C);

			Constant* format_const = ConstantDataArray::getString(getGlobalContext(), "DEBUG_IR: %d\n");
			var = new GlobalVariable(*mod, format_const->getType(), true,
						GlobalValue::PrivateLinkage, format_const, ".str");
		}

		std::vector<llvm::Constant*> indices = { int32Zero, int32Zero };
		Constant *var_ref = ConstantExpr::getGetElementPtr(var, indices);

		builder->CreateCall2(func, var_ref, val);
	}

	Module *createModule()
	{
		auto Owner = make_unique<Module>("Ruby LLVM Module", getGlobalContext());
		Module* module = Owner.get();

		std::string error;
		std::unique_ptr<RTDyldMemoryManager> MemMgr(new SectionMemoryManager());

		// auto engine = EngineBuilder(std::move(Owner))
		auto engine = std::unique_ptr<ExecutionEngine>(EngineBuilder(std::move(Owner))
			.setEngineKind(llvm::EngineKind::JIT)
			.setMCJITMemoryManager(std::move(MemMgr))
			.setErrorStr(&error)
			.create());

		auto prev_engine = engines.back().get();
		// engines.push_back(engine);
		engines.push_back(std::move(engine));

		const char *ruby_module =
			#include "tool/jit/jit_typedef.inc"
		parseAndLink(ruby_module, module);

#define GET_RUBY_STRUCT(name) module->getTypeByName("struct." name)->getPointerTo()
		llvm_thread_t = GET_RUBY_STRUCT("rb_thread_struct");
		llvm_control_frame_t = GET_RUBY_STRUCT("rb_control_frame_struct");
		llvm_call_info_t = GET_RUBY_STRUCT("rb_call_info_struct");
#undef GET_RUBY_STRUCT

		if (!llvm_search_method) {
			sys::DynamicLibrary::AddSymbol("_vm_search_method", (void *)vm_search_method); // addGlobalMapping was failed in MCJIT

			sys::DynamicLibrary::AddSymbol("_vm_caller_setup_arg_block", (void *)vm_caller_setup_arg_block);

			sys::DynamicLibrary::AddSymbol("_vm_pop_frame", (void *)vm_pop_frame);

			llvm_search_method = module->getFunction("vm_search_method");
			llvm_caller_setup_arg_block = module->getFunction("vm_caller_setup_arg_block");
			llvm_pop_frame = module->getFunction("vm_pop_frame");
		}
		else {
			// 他のモジュールでもAddSymbol+関数ポインタで呼び出しで再利用できるかも
			//
			//
			//
			//
			//
			// void *p = prev_engine->getPointerToFunction(llvm_pop_frame);
			// printf("%p\n", p);
			// sys::DynamicLibrary::AddSymbol("_vm_pop_frame", p);
			llvm_pop_frame = module->getFunction("vm_pop_frame");
			// FunctionType* pop_frame_t = FunctionType::get(voidTy, { llvm_thread_t }, false);
			// llvm_pop_frame = Function::Create(pop_frame_t, GlobalValue::ExternalLinkage, "vm_pop_frame", module);
		}


		llvm_jit_func_ret_t = StructType::get(llvm_control_frame_t, 0);

		sys::PrintStackTraceOnErrorSignal();

		return module;
	}

	void parseAndLink(const char *bitcode, Module *module)
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
		// static std::unique_ptr<Module> newModule;
		// if (!newModule) {
		// 	newModule = parseIR(buffer->getMemBufferRef(), err, getGlobalContext());
		// 	if (!newModule) {
		// 		JIT_DEBUG_LOG2("ERROR: %s", err.getMessage().str().c_str());
		// 		return;
		// 	}
		// }
        //
		// if (Linker::LinkModules(module, newModule.get())) {
		// 	JIT_DEBUG_LOG("Module link error");
		// }
	}

	// void optimizeFunction(Function& f, Module *module)
	void optimizeFunction(Function& f, Module *module)
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

	jit_codegen_func_t createJITFunction(Module *module)
	{
		jit_codegen_func_t f;
		FunctionType* jit_trace_func_t = FunctionType::get(llvm_jit_func_ret_t,
				{ llvm_thread_t, llvm_control_frame_t }, false);
		f.jit_trace_func = Function::Create(jit_trace_func_t,
				GlobalValue::ExternalLinkage, "jit_trace_func", module);
		f.jit_trace_func->setCallingConv(CallingConv::C);

		BasicBlock *entry = BasicBlock::Create(getGlobalContext(), "entry", f.jit_trace_func);
		builder->SetInsertPoint(entry);

		Function::arg_iterator args = f.jit_trace_func->arg_begin();
		f.arg_th = args++;
		f.arg_cfp = args++;

		f.sp_gep = builder->CreateStructGEP(f.arg_cfp, 1); JIT_LLVM_SET_NAME(f.sp_gep, "sp_ptr");
		f.ep_gep = builder->CreateStructGEP(f.arg_cfp, 6); JIT_LLVM_SET_NAME(f.ep_gep, "ep_ptr");

		return f;
	}

	std::function<jit_func_ret_t(rb_thread_t*, rb_control_frame_t*)> compileFunction(Function *function)
	{
		static int flag = 0;
		void* pfptr;
		// if (flag++ == 1) {
		// 	engine2->finalizeObject();
        //
		// 	pfptr = engine2->getPointerToFunction(function);
		// 	JIT_DEBUG_LOG2("compile: %p", pfptr);
		// }
		// else {
			// engine->finalizeObject();

			// pfptr = engine->getPointerToFunction(function);
			// pfptr = (void *)engine->getFunctionAddress(function->getName());
			pfptr = (void *)engines.back()->getFunctionAddress(function->getName());
			JIT_DEBUG_LOG2("compile: %p", pfptr);
		// }
		return (jit_func_ret_t (*)(rb_thread_t *th, rb_control_frame_t*))pfptr;
	}
};

static std::unique_ptr<JitCompiler> rb_mJit;
#define RB_JIT rb_mJit

#define CONTEXT getGlobalContext()
// #define MODULE RB_JIT->module
#define BUILDER RB_JIT->builder
#define ENGINE RB_JIT->engine

#include "jit_dump.h"

extern "C"
void
jit_add_iseq(rb_iseq_t *iseq)
{
	return;
	if (!iseq) return;

	RB_JIT->iseq_list[iseq->iseq_encoded] = iseq;
}

static inline void
jit_init_trace(jit_trace_t *trace, rb_iseq_t *iseq)
{
	// auto size = iseq->iseq_size;
	auto size = 0x100;
	auto insns = new jit_insn_t*[size+1];	// +1 is for last basicblock
	// insns[size] = new jit_insn_t;			// for last basicblock
	memset(insns, 0, sizeof(jit_insn_t*) * size);

	trace->insns = insns;
	trace->insns_size = size;
	trace->iseq = iseq;
}

extern "C"
void
jit_trace_start(rb_control_frame_t *cfp)
{
	jit_trace_t *trace;

	is_jit_tracing = 1;

	auto &traces = RB_JIT->traces;
	// auto it = traces.find(cfp->iseq->iseq_encoded);
	auto it = traces.find(cfp->pc);
	if (it != traces.end()) {
		trace = it->second;
	}
	else {
		trace = new jit_trace_t;
		jit_init_trace(trace, cfp->iseq);

		// save trace
		// RB_JIT->traces[cfp->iseq->iseq_encoded] = trace;
		RB_JIT->traces[cfp->pc] = trace;
		RB_JIT->trace_list.push_back(trace);
	}

	RB_JIT->trace = trace;
}

extern "C"
void
jit_push_new_trace(rb_control_frame_t *cfp)
{
	if (!cfp->iseq) return; // CFUNC

	// ブロック呼び出しも push と pop はよばれる
	// vm_yield_with_cfuncでifuncをプッシュした時は cfp->iseq->origが0以外で、sizeも未設定

	// rb_iseq_location_t &loc = cfp->iseq->location;
	// char *path = RSTRING_PTR(loc.path);
	// char *base_label = RSTRING_PTR(loc.base_label);
	// char *label = RSTRING_PTR(loc.label);
	// JIT_DEBUG_LOG2("jit_push_new_trace: %s, %s, %s", path, base_label, label);

	jit_trace_start(cfp);
}

extern "C"
rb_control_frame_t *
jit_pop_trace(rb_control_frame_t *cfp)
{
	// push_new_trace と pop_trace の呼び出し回数が合わない pop が多くなる???
	// if (cfp->iseq) {
	// 	// --trace_stack_size;
	// 	auto it = RB_JIT->traces.find(cfp->iseq->iseq_encoded);
	// 	if (it == RB_JIT->traces.end()) {
	// 		// installing default ripper libraries
	// 		// checking ../.././parse.y and ../.././ext/ripper/eventids2.c
	// 		is_jit_tracing = 0;
	// 		JIT_DEBUG_LOG2("@@@@ jit_pop_trace @@@@   %p, %d, %d", cfp->pc, cfp->iseq->type, cfp->flag);
	// 	}
	// 	else {
	// 		RB_JIT->trace = it->second;
	// 	}
	// 	// JIT_DEBUG_LOG("@@@@ jit_pop_trace @@@@   ???????????");

	// }
	jit_push_new_trace(cfp);

	return cfp;
}

static inline void
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
	insn->bb = nullptr;
}

static void jit_codegen_trace(rb_thread_t *th, jit_trace_t *trace);

extern "C"
int
jit_trace_insn(rb_thread_t *th, rb_control_frame_t *cfp, VALUE *pc)
{
	jit_trace_t *trace = RB_JIT->trace;

	// if (cfp->iseq->iseq_encoded != trace->iseq->iseq_encoded) {
	// 	// th->cfp の切り替えの検知に失敗
	// 	JIT_DEBUG_LOG2("*** jit_trace_insn ***  : %p, %p, %d, %d", cfp->iseq, trace->iseq, cfp->flag, cfp->iseq->type);
	// 	jit_pop_trace(cfp);
	// 	trace = RB_JIT->trace;
	// }

	{
		// トレース済み
		// int index = pc - cfp->iseq->iseq_encoded;
		// if (trace->insns[index]) return;

		// if (trace->pc_set.find(pc) == trace->pc_set.end()) {
		// 	trace->pc_set.insert(pc);
		// }
		if (trace->insns_iterator > 0 && trace->insns[0]->pc == pc) {
		// else {
			int size = trace->insns_iterator;
			// JIT_DEBUG_LOG2("trace yet: %d", size);
			// JIT_DEBUG_LOG2("%p, %p", trace->insns[size - 1]->pc, pc + (trace->insns[size - 1]->pc - pc));

			if (!trace->jited) jit_codegen_trace(th, trace);

			// JIT_DEBUG_LOG("==== Execute JITed function ====");
			jit_func_ret_t ret = trace->jited(th, cfp);

			// JIT_DEBUG_LOG("==== Finish to execute JITed function ====");
			auto last_insn = trace->insns[trace->insns_iterator - 1];

			th->cfp = ret.exit_cfp; // trace->jited(th, cfp)のcfpが現状では返ってくる
			th->cfp->pc = last_insn->pc + last_insn->len;

			return last_insn->pc - pc; //pc == trace->insns[0]????
		}
	}

	jit_insn_t *insn = new jit_insn_t;

	jit_insn_init(insn, th, cfp, pc);
	// TODO: トレースするかを実行回数などで判定
	// trace->insns[insn->index] = insn;
	trace->insns[trace->insns_iterator++] = insn;

	return 0;
}

extern "C"
void
jit_trace_dump(rb_thread_t *th)
{
	// for (auto trace_map: RB_JIT->traces) {
	// 	jit_trace_t *trace = trace_map.second;

	for (jit_trace_t *trace: RB_JIT->trace_list) {
		jit_insn_t **insns = trace->insns;
		// unsigned insns_size = trace->insns_size;
		unsigned insns_size = trace->insns_iterator;

		JIT_DEBUG_LOG2("=== Start trace dump (length: %lu) ===", insns_size);

		for (unsigned i = 0; i < insns_size; i++) {
			if (jit_insn_t *insn = insns[i]) {
				jit_dump_insn(insn);
			}
		}

		JIT_DEBUG_LOG("=== End trace dump ===");
	}

	// jit_trace_t *trace = RB_JIT->trace;
	// jit_insn_t **insns = trace->insns;
	// unsigned insns_size = trace->insns_iterator;
    //
	// JIT_DEBUG_LOG2("=== Start trace dump (length: %lu) ===", insns_size);
    //
	// for (unsigned i = 0; i < insns_size; i++) {
	// 	if (jit_insn_t *insn = insns[i]) {
	// 		jit_dump_insn(insn);
	// 	}
	// }
    //
	// JIT_DEBUG_LOG("=== End trace dump ===");
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

// static VALUE
// jit_compile(VALUE self, VALUE code)
// {
// 	static VALUE name = rb_str_new2("<internal:jit>");
// 	static VALUE line = INT2FIX(1);
// 	VALUE iseqval = rb_iseq_compile_with_option(code, name, Qnil, line, 0, Qtrue);
//
// 	rb_iseq_t *iseq;
// 	GetISeqPtr(iseqval, iseq);
//
// #if OPT_DIRECT_THREADED_CODE || OPT_CALL_THREADED_CODE
// 	VALUE *iseq_encoded = rb_iseq_original_iseq(iseq);
// #else
// 	VALUE *iseq_encoded = iseq->iseq_encoded;
// #endif
//
// 	VALUE str = rb_iseq_disasm(iseq->self);
// 	printf("disasm[self]: %s\n", StringValueCStr(str));
//
// 	return Qtrue;
// }

static inline void
jit_compile(rb_control_frame_t *cfp)
{
	rb_iseq_t *iseq = cfp->iseq;

	if (!iseq) return; // CFUNC

	VALUE *iseq_encoded = rb_iseq_original_iseq(iseq);

	unsigned size = iseq->iseq_size;

	// insn->index = pc - iseq->iseq_encoded;
	// insn->opecode = (int)iseq_encoded[insn->index];
	// insn->len = insn_len(insn->opecode);
	// insn->th = th;
	// insn->cfp = cfp;
	// insn->pc = pc;
	// insn->bb = nullptr;

	for (unsigned i = 0, len = 0; i < size; i += len) {
		VALUE *pc = &iseq_encoded[i];
		int op = pc[0];
		len = insn_len(op);

	}
}


extern "C"
void
Init_JIT(void)
{
	ruby_jit_init();
	VALUE rb_mJIT = rb_define_module("JIT");
	// rb_define_singleton_method(rb_mJIT, "compile", RUBY_METHOD_FUNC(jit_compile), 1);
	// rb_define_singleton_method(rb_mJIT, "trace_dump", RUBY_METHOD_FUNC(jit_trace_dump), 0);
}


#include "jit_core.h"


static inline void
jit_codegen_core(
		jit_codegen_func_t codegen_func,
		rb_control_frame_t *cfp,
		jit_insn_t **insns,
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
jit_codegen_trace(rb_thread_t *th, jit_trace_t *trace)
{
	JIT_DEBUG_LOG("=== Start jit_codegen_trace  ===");
	Module *module = RB_JIT->createModule();
	jit_codegen_func_t codegen_func = RB_JIT->createJITFunction(module);
	codegen_func.module = module;

	jit_insn_t **insns = trace->insns;
	unsigned insns_size = trace->insns_iterator;

	VALUE *first_pc = insns[0]->pc;
	auto *first_cfp = insns[0]->cfp;

	insns[insns_size] = new jit_insn_t;

	// for (unsigned i = 0; i < insns_size + 1; i++) {
	for (unsigned i = 0, len = 1; i < insns_size + 1; i += len) {
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
			jit_codegen_core(codegen_func, insn->cfp, insns, i);
		}
	}

	BUILDER->SetInsertPoint(insns[insns_size]->bb);
	// BUILDER->CreateRetVoid();
	AllocaInst *ret_ptr = BUILDER->CreateAlloca(RB_JIT->llvm_jit_func_ret_t);
	Value *ret_cfp_ptr = BUILDER->CreateStructGEP(ret_ptr, 0);
	BUILDER->CreateStore(codegen_func.arg_cfp, ret_cfp_ptr);
	Value *ret = BUILDER->CreateLoad(ret_ptr);
	BUILDER->CreateRet(ret);


	// JIT_DEBUG_LOG("==== JITed instructions ====");
	// jit_trace_func->dump();
	JIT_DEBUG_LOG("==== Optimized JITed instructions ====");
	RB_JIT->optimizeFunction(*codegen_func.jit_trace_func, module);
	JIT_DEBUG_RUN(codegen_func.jit_trace_func->dump());

	JIT_DEBUG_LOG("==== Compile instructions ====");
	trace->jited = RB_JIT->compileFunction(codegen_func.jit_trace_func);
}

static inline void
jit_codegen_core(
		jit_codegen_func_t codegen_func,
		rb_control_frame_t *cfp,
		jit_insn_t **insns,
		int index)
{
#define JIT_TRACE_FUNC	codegen_func.jit_trace_func
#define JIT_TH 			codegen_func.arg_th
#define JIT_CFP			codegen_func.arg_cfp
#define SP_GEP			codegen_func.sp_gep
#define EP_GEP			codegen_func.ep_gep
#define MODULE			codegen_func.module

#define PRINT_VAL(val) RB_JIT->createPrintf(MODULE, (val));


#define JIT_CHECK_STACK_SIZE
#define jit_error(msg) (fprintf(stderr, msg" (%s, %d)", __FILE__, __LINE__), rb_raise(rb_eRuntimeError, "JIT error"), nullptr)
#undef TOPN
#define TOPN(x)\
	[&]{\
	Value *sp = BUILDER->CreateLoad(SP_GEP);\
	JIT_LLVM_SET_NAME(sp, "sp");\
	Value *sp_incptr = BUILDER->CreateInBoundsGEP(sp, RB_JIT->signedVal(-x - 1));\
	JIT_LLVM_SET_NAME(sp_incptr, "sp_minus_" #x "_");\
	return BUILDER->CreateLoad(sp_incptr);}()
#undef POPN
#define POPN(x) {\
	Value *sp = BUILDER->CreateLoad(SP_GEP);\
	JIT_LLVM_SET_NAME(sp, "sp");\
	Value *sp_incptr = BUILDER->CreateInBoundsGEP(sp, RB_JIT->signedVal(-x));\
	JIT_LLVM_SET_NAME(sp_incptr, "sp_minus_" #x "_");\
	BUILDER->CreateStore(sp_incptr, SP_GEP);}
#undef PUSH
#define PUSH(x) {\
	Value *sp = BUILDER->CreateLoad(SP_GEP);\
	JIT_LLVM_SET_NAME(sp, "sp");\
	BUILDER->CreateStore((x), sp);\
	Value *sp_incptr = BUILDER->CreateInBoundsGEP(sp, RB_JIT->valueOne);\
	JIT_LLVM_SET_NAME(sp_incptr, "sp_plus_1_");\
	BUILDER->CreateStore(sp_incptr, SP_GEP);}

#undef CALL_METHOD
#define CALL_METHOD(ci) do { \
	Value *v = BUILDER->CreateCall3(ci_call, JIT_TH, JIT_CFP, ci);\
	Value *cond_v = BUILDER->CreateICmpNE(v, RB_JIT->valueQundef, "ifcond");\
    BasicBlock *then_block = BasicBlock::Create(CONTEXT, "then", JIT_TRACE_FUNC);\
    BasicBlock *else_block = BasicBlock::Create(CONTEXT, "else", JIT_TRACE_FUNC);\
    BUILDER->CreateCondBr(cond_v, then_block, else_block);\
	BUILDER->SetInsertPoint(then_block);\
	PUSH(v);\
	BUILDER->CreateBr(else_block);\
	BUILDER->SetInsertPoint(else_block);\
} while (0)
#define GET_SELF() RB_JIT->valueVal(cfp->self)
#define GET_EP() BUILDER->CreateLoad(EP_GEP)
#define GET_LEP() (JIT_EP_LEP(GET_EP()))
#undef GET_OPERAND
#define GET_OPERAND(x) (insn->pc[(x)])

#define RSHIFT(x,y) BUILDER->CreateLShr(x, y)
#define FIX2LONG(x) (RSHIFT((x), 1))

#define INT2FIX(i) BUILDER->CreateOr(BUILDER->CreateShl(i, 1), FIXNUM_FLAG)
#define LONG2FIX(i) INT2FIX(i)
#define LONG2NUM(i) LONG2FIX(i)
#define OFFSET long

// #define RTEST(v) !(((VALUE)(v) & ~Qnil) == 0)
#define RTEST(v) (BUILDER->CreateICmpNE(BUILDER->CreateAnd((v), ~Qnil), RB_JIT->valueZero))
#undef JIT_CHECK_STACK_SIZE

// #define NEXT_INSN() (insns[insn->index + insn->len])
#define NEXT_INSN() (insns[index + 1])

	jit_insn_t *insn = insns[index];
	switch (insn->opecode) {
#include "jit_codegen.inc"
	}
}



