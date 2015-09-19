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

#include "jit_types.cpp"
#include "jit_funcs.cpp"


int is_jit_tracing = 0;
int trace_stack_size = 0;
int is_top_of_bytecode = 0;

typedef struct jit_insn_struct {
	rb_thread_t *th;
	rb_control_frame_t *cfp;
	VALUE *pc;
	int opecode;
	int index;
	int len = 1;
	BasicBlock *bb;
} jit_insn_t;

typedef struct jit_func_return_struct {
	rb_control_frame_t *exit_cfp;
	//VALUE *exit_pc;
	VALUE ret;
} jit_func_ret_t;

typedef struct jit_trace_struct {
	// std::deque<jit_insn_t*> insns;
	jit_insn_t **insns = nullptr;
	unsigned insns_size = 0;
	unsigned insns_iterator = 0;
	rb_iseq_t *iseq;

	VALUE *first_pc;
	std::unordered_set<VALUE*> pc_set;
	std::function<jit_func_ret_t(rb_thread_t*, rb_control_frame_t*)> jited = nullptr;
} jit_trace_t;

struct jit_codegen_func_t {
	Function *jit_trace_func;
	// Argument *arg_th;
	// Argument *arg_cfp;
	Value *arg_th;
	Value *arg_th_ptr;
	Value *arg_cfp;
	Value *arg_cfp_ptr;
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
	//
	JITTypes *types;
	JITValues *values;
	JITFuncs *funcs;

	Function *llvm_caller_setup_arg_block;

	jit_trace_t *trace = nullptr;
	std::unordered_map<VALUE*, jit_trace_t*> traces;
	std::list<jit_trace_t*> trace_list;

	std::unordered_map<VALUE*, rb_iseq_t*> iseq_list;

	JitCompiler()
	: builder(std::unique_ptr<IRBuilder<>>(new IRBuilder<>(getGlobalContext())))
	{
		types = new JITTypes();
		values = new JITValues(types);
		funcs = nullptr;
	}

	~JitCompiler()
	{
		delete types;
		delete values;
		if (funcs) delete funcs;

		JIT_DEBUG_LOG("==== Destroy JitCompiler ====");
		JIT_DEBUG_LOG2("ExecutionEngine: size=%d\n", engines.size());
		auto begin = engines.begin();
		auto end = engines.end();
		for (auto it = begin; it != end; ++it) {
			it->release();
			// delete *it;
		}
	}


#ifdef JIT_DEBUG_FLAG
	void createPrintf(llvm::Module *mod, llvm::Value* val)
	{
		GlobalVariable* var = mod->getGlobalVariable(".str", true);

		if (!var) {
			Constant* format_const = ConstantDataArray::getString(getGlobalContext(), "DEBUG_IR: %d\n");
			var = new GlobalVariable(*mod, format_const->getType(), true,
						GlobalValue::PrivateLinkage, format_const, ".str");
		}

		std::vector<llvm::Constant*> indices = { values->int32Zero, values->int32Zero };
		Constant *var_ref = ConstantExpr::getGetElementPtr(var, indices);

		builder->CreateCall2(funcs->printf, var_ref, val);
	}
#endif

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


		// if (!llvm_search_method) {
			sys::DynamicLibrary::AddSymbol("_vm_caller_setup_arg_block", (void *)vm_caller_setup_arg_block);

			llvm_caller_setup_arg_block = module->getFunction("vm_caller_setup_arg_block");

			if (funcs) delete funcs;
			funcs = new JITFuncs(module, types);
			// llvm_pop_frame = module->getFunction("vm_pop_frame");
		// }
		// else {
			// 他のモジュールでもAddSymbol+関数ポインタで呼び出しで再利用できるかも
			//
			//
			//
			//
			//
			// void *p = prev_engine->getPointerToFunction(llvm_pop_frame);
			// printf("%p\n", p);
			// sys::DynamicLibrary::AddSymbol("_vm_pop_frame", p);
			// llvm_pop_frame = module->getFunction("vm_pop_frame");
			// FunctionType* pop_frame_t = FunctionType::get(voidTy, { llvm_thread_t }, false);
			// llvm_pop_frame = Function::Create(pop_frame_t, GlobalValue::ExternalLinkage, "vm_pop_frame", module);
		// }


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
	}

	jit_codegen_func_t createJITFunction(Module *module)
	{
		jit_codegen_func_t f;
		FunctionType* jit_trace_func_t = FunctionType::get(types->jit_func_ret_t,
				{ types->rb_thread_t, types->rb_control_frame_t }, false);
		f.jit_trace_func = Function::Create(jit_trace_func_t,
				GlobalValue::ExternalLinkage, "jit_trace_func", module);
		f.jit_trace_func->setCallingConv(CallingConv::C);

		BasicBlock *entry = BasicBlock::Create(getGlobalContext(), "entry", f.jit_trace_func);
		builder->SetInsertPoint(entry);

		f.arg_th_ptr  = builder->CreateAlloca(types->rb_thread_t);
		f.arg_cfp_ptr = builder->CreateAlloca(types->rb_control_frame_t);
		// f.arg_th      = builder->CreateLoad(f.arg_th_ptr);
		// f.arg_cfp     = builder->CreateLoad(f.arg_cfp_ptr);

		Function::arg_iterator args = f.jit_trace_func->arg_begin();
		builder->CreateStore(args++, f.arg_th_ptr);
		builder->CreateStore(args++, f.arg_cfp_ptr);
		// f.arg_th = args++;
		// f.arg_cfp = args++;

		// f.sp_gep = builder->CreateStructGEP(f.arg_cfp, 1); JIT_LLVM_SET_NAME(f.sp_gep, "sp_ptr");
		// f.ep_gep = builder->CreateStructGEP(f.arg_cfp, 6); JIT_LLVM_SET_NAME(f.ep_gep, "ep_ptr");

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
jit_add_symbol(const char* name, void* pfunc)
{
	sys::DynamicLibrary::AddSymbol(name, pfunc);
}

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
	auto size = 0x50;
	auto insns = new jit_insn_t*[size+1];	// +1 is for last basicblock
	// insns[size] = new jit_insn_t;			// for last basicblock
	memset(insns, 0, sizeof(jit_insn_t*) * size);

	trace->insns = insns;
	trace->insns_size = size;
	trace->iseq = iseq;
}

static inline jit_trace_t *
jit_trace_create_trace(rb_control_frame_t *cfp, VALUE *pc)
{
	jit_trace_t *trace = new jit_trace_t;
	jit_init_trace(trace, cfp->iseq);

	// save trace
	RB_JIT->traces[pc] = trace;
	RB_JIT->trace_list.push_back(trace);
	return trace;
}

static inline jit_trace_t *
jit_trace_find_trace(VALUE *pc)
{
	auto &traces = RB_JIT->traces;
	auto it = traces.find(pc);
	if (it == traces.end()) return nullptr;
	return it->second;
}

static inline jit_trace_t *
jit_trace_find_trace_or_create_trace(rb_control_frame_t *cfp, VALUE *pc)
{
	auto &traces = RB_JIT->traces;
	auto it = traces.find(pc);
	if (it == traces.end())
		return jit_trace_create_trace(cfp, pc);
	return it->second;
}

static inline void
jit_switch_trace(jit_trace_t *trace)
{
	RB_JIT->trace = trace;
}

extern "C"
void
jit_trace_start(rb_control_frame_t *cfp)
{
	is_jit_tracing = 1;

	// jit_trace_t *trace = jit_trace_find_trace(cfp->pc);
	// if (!trace) {
	// 	trace = new jit_trace_t;
	// 	jit_init_trace(trace, cfp->iseq);
    //
	// 	// save trace
	// 	RB_JIT->traces[cfp->pc] = trace;
	// 	RB_JIT->trace_list.push_back(trace);
	// }
    //
	// RB_JIT->trace = trace;
	jit_trace_t *trace = jit_trace_find_trace_or_create_trace(cfp, cfp->pc);
	jit_switch_trace(trace);
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
	is_top_of_bytecode = 1;
}

extern "C"
rb_control_frame_t *
jit_pop_trace(rb_control_frame_t *cfp)
{
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

static inline jit_insn_t *
jit_insn_next(jit_trace_t *trace, jit_insn_t *insn, jit_codegen_func_t codegen_func)
{
	int index = insn->index + 1;
	jit_insn_t *next_insn = trace->insns[index];
	if (index >= trace->insns_iterator) {
		jit_insn_init(next_insn, insn->th, insn->cfp, insn->pc + insn->len);
		next_insn->index = index;
		insn->bb = BasicBlock::Create(CONTEXT, "insn", codegen_func.jit_trace_func);
		trace->insns_iterator++;
	}
	return next_insn;
}

static void jit_codegen_trace(rb_thread_t *th, jit_trace_t *trace);

extern "C"
void
jit_trace_insn(rb_thread_t *th, rb_control_frame_t *cfp, VALUE *pc, jit_trace_ret_t *ret)
{
	jit_trace_t *trace = RB_JIT->trace;

	{
		// トレース済み
		if (trace->first_pc == pc) {
			if (!trace->jited) jit_codegen_trace(th, trace);

			JIT_DEBUG_LOG("==== Execute JITed function ====");
			jit_func_ret_t func_ret = trace->jited(th, cfp);
			// JIT_DEBUG_LOG("==== Finished executing JITed function ====");

			auto last_insn = trace->insns[trace->insns_iterator - 1];

			th->cfp = func_ret.exit_cfp; // trace->jited(th, cfp)のcfpが現状では返ってくる
			// th->cfp->pc = last_insn->pc + last_insn->len;

			if (func_ret.ret) {
				ret->jmp = -1;
				ret->retval = func_ret.ret;
				return;
			}

			// JIT_DEBUG_LOG2("@ last_insn: %p(%d)", last_insn, trace->insns_iterator - 1);
			// JIT_DEBUG_LOG2("@ jump pc: (%p - %p)[%d] + %d = %d[%08p]", last_insn->pc, pc, last_insn->pc - pc, last_insn->len, last_insn->pc - pc + last_insn->len, pc + ((last_insn->pc - pc) + last_insn->len));

			if (cfp != th->cfp) {
				// バイトコードが遷移した場合
				RB_JIT->trace = jit_trace_find_trace_or_create_trace(th->cfp, th->cfp->pc);
				ret->jmp = 0;
				cfp->pc += (last_insn->pc - pc) + last_insn->len;
				// JIT_DEBUG_LOG2("%%cfp%% jmp: %d, th->cfp->pc: %p, reg_cfp->pc: %p reg_pc: %p", ret->jmp, th->cfp->pc, cfp->pc, pc);
				jit_trace_start(cfp);
				return;
			}

			ret->jmp = (last_insn->pc - pc) + last_insn->len;
			RB_JIT->trace = jit_trace_find_trace(cfp->pc + ret->jmp);
			cfp->pc += ret->jmp;
			// JIT_DEBUG_LOG2("%% jmp: %d, pc: %p", ret->jmp, cfp->pc);
			jit_trace_start(cfp);
			return; //pc == trace->insns[0]????
		}
	}

	// if (is_top_of_bytecode) {
	// 	jit_trace_t *check_trace = jit_trace_find_trace(pc);
	// 	if (check_trace) {
	// 		if (check_trace != trace) {
	// 			jit_switch_trace(check_trace);   // times 等のブロックで繰り返す命令に対応
	// 			jit_trace_insn(th, cfp, pc, ret);
	// 			return;
	// 		}
	// 	}
	// 	else
	// 		jit_trace_create_trace(cfp, pc); // create a start position of trace
	// 	is_top_of_bytecode = 0;
	// }

	if (trace->insns_iterator == 0) trace->first_pc = pc;

	jit_insn_t *insn = new jit_insn_t;
	jit_insn_init(insn, th, cfp, pc);
	insn->index = trace->insns_iterator;


	// TODO: トレースするかを実行回数などで判定
	// trace->insns[insn->index] = insn;
	trace->insns[trace->insns_iterator++] = insn;

	ret->jmp = 1;
}

static inline jit_insn_t *
jit_trace_last_insn(jit_trace_t *trace)
{
	return trace->insns[trace->insns_iterator - 1];
}

static inline jit_insn_t *
jit_codegen_jump_insn(jit_trace_t *trace, jit_insn_t *insn, int dst)
{
	jit_insn_t **insns = trace->insns;
	unsigned insns_max = trace->insns_iterator - 1;
	int index = insn->index;

	int step;
	if (dst > 0)
		step = 1;
	else {
		step = -1;
		dst = -dst;
	}

	do {
		if (index > insns_max) {
			// トレース外にジャンプ
			JIT_DEBUG_LOG("jump over trace");
			return 0;
		}
		insn = insns[index];
		dst -= insn->len;
		JIT_DEBUG_LOG2("jump[%08p: %s]: %d, %d, (%d/%d)",
				insn->pc, insn_name(insn->opecode), dst * step, insn->len, index, insns_max);

		index += step;
	} while (dst > 0);

	JIT_DEBUG_LOG2("jump_insn: %08p", insn->pc);
	return insn;
}

void
jit_trace_jump(int dest)
{
	jit_trace_t *trace = RB_JIT->trace;
	jit_insn_t *insn = jit_trace_last_insn(trace);

	int cur = insn->index;
	dest += cur;

	jit_trace_ret_t ret;
	while (cur < dest) {
		jit_trace_insn(insn->th, insn->cfp, insn->pc + insn->len, &ret);
		insn = jit_trace_last_insn(trace);
		cur += insn->len;
	}
}

extern "C"
void
jit_trace_dump(rb_thread_t *th)
{
	for (jit_trace_t *trace: RB_JIT->trace_list) {
		jit_insn_t **insns = trace->insns;
		unsigned insns_size = trace->insns_iterator;

		JIT_DEBUG_LOG2("=== Start trace dump (length: %u) ===", insns_size);

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

#include "jit_codegen.cpp"

