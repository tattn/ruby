#include "internal.h"
#include "insns.inc"
#include "insns_info.inc"
#include "vm_core.h"
#include "jit.h"

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Transforms/Scalar.h>
#include <llvm-c/BitWriter.h>

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
// #include "llvm/IR/PassManager.h"
#include "llvm/Transforms/Scalar.h"

// #include "llvm/LinkAllPasses.h"
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

	// rb_thread_t
	// rb_control_frame_t
	Type *cfpTy;

	Value *valueOne;

	Value *int32Zero;

	jit_trace_t trace;

	JitCompiler()
	: builder(std::unique_ptr<IRBuilder<>>(new IRBuilder<>(getGlobalContext())))
	{

		valueOne = builder->getInt64(1);
		int32Zero = builder->getInt32(0);

		auto Owner = make_unique<Module>("Ruby LLVM Module", getGlobalContext());
		module = Owner.get();

		std::string error;
		std::unique_ptr<RTDyldMemoryManager> MemMgr(new SectionMemoryManager());

		engine = std::unique_ptr<ExecutionEngine>(EngineBuilder(std::move(Owner))
			.setEngineKind(llvm::EngineKind::JIT)
			.setMCJITMemoryManager(std::move(MemMgr))
			.setErrorStr(&error)
			.create());

		/*
		   VALUE *pc;
		   VALUE *sp
		   rb_iseq_t *iseq;
		   VALUE flag;
		   VALUE self;
		   VALUE klass;
		   VALUE *ep
		   rb_iseq_t *block_iseq;
		   VALUE proc;
		   VALUE *bp_check;
		   */
		cfpTy = StructType::create("rb_control_frame_t",
				valuePtrTy,
				valuePtrTy,
				ptrTy,
				valueTy,
				valueTy,
				valueTy,
				valuePtrTy,
				ptrTy,
				valueTy,
				valuePtrTy, nullptr);

		const char *bitcode = R"STR(
%struct.rb_thread_struct = type { %struct.list_node, i64, %struct.rb_vm_struct*, i64*, i64, %struct.rb_control_frame_struct*, i32, i32, i64, i32, i32, %struct.rb_block_struct*, %struct.rb_method_entry_struct*, %struct.rb_call_info_struct*, i64, i64, %struct.rb_block_struct*, i64*, i64, %struct._opaque_pthread_t*, i32, i32, i32, %struct.native_thread_data_struct, i8*, i64, i64, i64, i64, i64, i32, i32, i64, %struct._opaque_pthread_mutex_t, %struct.rb_thread_cond_struct, %struct.rb_unblock_callback, i64, %struct.rb_mutex_struct*, %struct.rb_vm_tag*, %struct.rb_vm_protect_tag*, i32, i32, %struct.st_table*, i64, i64, %struct.rb_thread_list_struct*, i64, i64, i64 (...)*, %struct.anon.12, i64, %struct.rb_hook_list_struct, %struct.rb_trace_arg_struct*, %struct.rb_fiber_struct*, %struct.rb_fiber_struct*, [37 x i32], %struct.rb_ensure_list*, i32, i32, i8*, i64, i64 }
%struct.list_node = type { %struct.list_node*, %struct.list_node* }
%struct.rb_vm_struct = type { i64, %struct.rb_global_vm_lock_struct, %struct._opaque_pthread_mutex_t, %struct.rb_thread_struct*, %struct.rb_thread_struct*, %struct.list_head, i64, i64, i32, i32, i32, i32, i64, [4 x i64], i64, i64, i64, i64, i64, i64, i64, %struct.st_table*, %struct.st_table*, [32 x %struct.anon.1], %struct.rb_hook_list_struct, %struct.st_table*, %struct.rb_postponed_job_struct*, i32, i32, i64, i64, i64, i64, i64, i64, %struct.rb_objspace*, %struct.RArray, i64*, %struct.st_table*, %struct.anon.5, [22 x i16] }
%struct.rb_global_vm_lock_struct = type { i64, %struct._opaque_pthread_mutex_t, i64, %struct.rb_thread_cond_struct, %struct.rb_thread_cond_struct, %struct.rb_thread_cond_struct, i32, i32 }
%struct.list_head = type { %struct.list_node }
%struct.anon.1 = type { i64, i32 }
%struct.rb_postponed_job_struct = type opaque
%struct.rb_objspace = type opaque
%struct.RArray = type { %struct.RBasic, %union.anon.2 }
%struct.RBasic = type { i64, i64 }
%union.anon.2 = type { %struct.anon.3 }
%struct.anon.3 = type { i64, %union.anon.4, i64* }
%union.anon.4 = type { i64 }
%struct.anon.5 = type { i64, i64, i64, i64 }
%struct.rb_method_entry_struct = type { i64, i64, %struct.rb_method_definition_struct*, i64, i64 }
%struct.rb_method_definition_struct = type { i32, i32, %union.anon.8, i64 }
%union.anon.8 = type { %struct.rb_method_cfunc_struct }
%struct.rb_method_cfunc_struct = type { i64 (...)*, i64 (i64 (...)*, i64, i32, i64*)*, i32 }
%struct.rb_call_info_struct = type { i64, i32, i32, %struct.rb_iseq_struct*, %struct.rb_call_info_kw_arg_struct*, i64, i64, i64, %struct.rb_method_entry_struct*, i64, %struct.rb_block_struct*, i64, i32, %union.anon.9, i64 (%struct.rb_thread_struct*, %struct.rb_control_frame_struct*, %struct.rb_call_info_struct*)* }
%struct.rb_iseq_struct = type { i32, i32, %struct.rb_iseq_location_struct, i64*, i32, i32, i64, i64, %struct.iseq_line_info_entry*, i64*, i32, i32, %union.iseq_inline_storage_entry*, i32, i32, %struct.rb_call_info_struct*, %struct.anon.10, %struct.iseq_catch_table*, %struct.rb_iseq_struct*, %struct.rb_iseq_struct*, i64, i64, i64, i64, i64, %struct.iseq_compile_data*, i64* }
%struct.rb_iseq_location_struct = type { i64, i64, i64, i64, i64 }
%struct.iseq_line_info_entry = type opaque
%union.iseq_inline_storage_entry = type { %struct.iseq_inline_cache_entry }
%struct.iseq_inline_cache_entry = type { i64, %struct.rb_cref_struct*, %union.anon.7 }
%struct.rb_cref_struct = type { i64, i64, i64, %struct.rb_cref_struct*, %struct.rb_scope_visi_struct }
%struct.rb_scope_visi_struct = type { i8, [3 x i8] }
%union.anon.7 = type { i64 }
%struct.anon.10 = type { %struct.anon.11, i32, i32, i32, i32, i32, i32, i32, i64*, %struct.rb_iseq_param_keyword* }
%struct.anon.11 = type { i8, [3 x i8] }
%struct.rb_iseq_param_keyword = type { i32, i32, i32, i32, i64*, i64* }
%struct.iseq_catch_table = type opaque
%struct.iseq_compile_data = type opaque
%struct.rb_call_info_kw_arg_struct = type { i32, [1 x i64] }
%union.anon.9 = type { i32 }
%struct.rb_block_struct = type { i64, i64, i64*, %struct.rb_iseq_struct*, i64 }
%struct._opaque_pthread_t = type { i64, %struct.__darwin_pthread_handler_rec*, [8176 x i8] }
%struct.__darwin_pthread_handler_rec = type { void (i8*)*, i8*, %struct.__darwin_pthread_handler_rec* }
%struct.native_thread_data_struct = type { i8*, %struct.rb_thread_cond_struct }
%struct._opaque_pthread_mutex_t = type { i64, [56 x i8] }
%struct.rb_thread_cond_struct = type { %struct._opaque_pthread_cond_t }
%struct._opaque_pthread_cond_t = type { i64, [40 x i8] }
%struct.rb_unblock_callback = type { void (i8*)*, i8* }
%struct.rb_mutex_struct = type opaque
%struct.rb_vm_tag = type { i64, i64, [37 x i32], %struct.rb_vm_tag* }
%struct.rb_vm_protect_tag = type { %struct.rb_vm_protect_tag* }
%struct.st_table = type { %struct.st_hash_type*, i64, i64, %union.anon }
%struct.st_hash_type = type { i32 (...)*, i64 (...)* }
%union.anon = type { %struct.anon }
%struct.anon = type { %struct.st_table_entry**, %struct.st_table_entry*, %struct.st_table_entry* }
%struct.st_table_entry = type opaque
%struct.rb_thread_list_struct = type { %struct.rb_thread_list_struct*, %struct.rb_thread_struct* }
%struct.anon.12 = type { i64*, i64*, i64, [37 x i32] }
%struct.rb_hook_list_struct = type { %struct.rb_event_hook_struct*, i32, i32 }
%struct.rb_event_hook_struct = type opaque
%struct.rb_trace_arg_struct = type { i32, %struct.rb_thread_struct*, %struct.rb_control_frame_struct*, i64, i64, i64, i64, i32, i32, i64 }
%struct.rb_fiber_struct = type opaque
%struct.rb_ensure_list = type { %struct.rb_ensure_list*, %struct.rb_ensure_entry }
%struct.rb_ensure_entry = type { i64, i64 (...)*, i64 }
%struct.rb_control_frame_struct = type { i64*, i64*, %struct.rb_iseq_struct*, i64, i64, i64, i64*, %struct.rb_iseq_struct*, i64 }


declare void @vm_caller_setup_arg_block(%struct.rb_thread_struct*, %struct.rb_control_frame_struct*, %struct.rb_call_info_struct*, i32) #1
declare void @vm_search_method(%struct.rb_call_info_struct*, i64)
		)STR";

		parseAndLink(bitcode);
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
		legacy::FunctionPassManager fpm(module);
		// fpm.addPass(new DataLayout(*engine->getDataLayout()));
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
	// jit_insn_t *insn = (jit_insn_t*)malloc(sizeof(jit_insn_t));
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
			// insn->operands.push_back(trace.reg_stack.back()); trace.reg_stack.pop_back(); // obj
			// insn->operands.push_back(trace.reg_stack.back()); trace.reg_stack.pop_back(); // recv
			break;
		case BIN(opt_send_without_block):
			CALL_INFO ci = (CALL_INFO)pc[1];
			ci->argc = ci->orig_argc;
			insn->operands.push_back(pc[1]);
			break;
	}
	// if (insn->opecode == BIN(opt_plus)) {
	// 	rb_mJit->engine->finalizeObject();
	// 	Function *jit_opt_plus = rb_mJit->module->getFunction("jit_opt_plus");
	// 	void* pfptr = rb_mJit->engine->getPointerToFunction(jit_opt_plus);
	// 	void (* fptr)(rb_control_frame_t*) = (void (*)(rb_control_frame_t*))pfptr;
	// 	try {
	// 	fptr(cfp);
	// 	} catch(...){}
	// }
}

extern "C"
void
jit_trace_dump(rb_thread_t *th)
{
	JIT_DEBUG_LOG2("=== Start trace dump (length: %d) ===", rb_mJit->trace.insns.size());
	for (auto insn : rb_mJit->trace.insns) {
		JIT_DEBUG_LOG2("trace: %s(%d)", insn_name(insn->opecode), insn->opecode);
		// switch (insn->opecode) {
		// 	case BIN(trace):
		// 		JIT_DEBUG_LOG2("trace: trace(%d)", insn->opecode);
		// 		break;
		// 	case BIN(putself):
		// 		JIT_DEBUG_LOG2("trace: putself(%d)", insn->opecode);
        //
		// 		break;
		// 	case BIN(putobject):
		// 		JIT_DEBUG_LOG2("trace: putobject(%d)", insn->opecode);
        //
		// 		break;
		// 	case BIN(opt_plus):
		// 		JIT_DEBUG_LOG2("trace: opt_plus(%d)", insn->opecode);
        //
		// 		// recv & obj がFixnum なら最適化
		// 		break;
		// 	case BIN(opt_send_without_block):
		// 		JIT_DEBUG_LOG2("trace: opt_send_without_block(%d)", insn->opecode);
        //
		// 		break;
		// 	default:
		// 		JIT_DEBUG_LOG2("trace: unknown[%d(%s)]", insn->opecode, insn_name(insn->opecode));
		// }
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
// #define JIT_LLVM_SET_NAME(v, name)
#endif

	Type *llvm_thread_t = rb_mJit->module->getTypeByName("struct.rb_thread_struct")->getPointerTo();
	Type *llvm_control_frame = rb_mJit->module->getTypeByName("struct.rb_control_frame_struct")->getPointerTo();


	Function *llvm_search_method = rb_mJit->module->getFunction("vm_search_method");
	Function *llvm_caller_setup_arg_block = rb_mJit->module->getFunction("vm_caller_setup_arg_block");


	FunctionType* jit_trace_func_t = FunctionType::get(rb_mJit->voidTy, { llvm_thread_t, llvm_control_frame }, false);
	// Function *jit_trace_func = Function::Create(jit_trace_func_t, GlobalValue::ExternalLinkage, "jit_trace_func1", m);
	Function *jit_trace_func = Function::Create(jit_trace_func_t, GlobalValue::ExternalLinkage, "jit_trace_func1", rb_mJit->module);
	jit_trace_func->setCallingConv(CallingConv::C);

	Function::arg_iterator args = jit_trace_func->arg_begin();
	Argument *arg_th = args++;
	Argument *arg_cfp = args++;


	BasicBlock *entry = BasicBlock::Create(getGlobalContext(), "entry", jit_trace_func);
	rb_mJit->builder->SetInsertPoint(entry);

	std::deque<int> _stack;
	int reg_no = 0;

	std::deque<Value*> __stack;
	Value *self;

	for (auto insn : rb_mJit->trace.insns) {
		switch (insn->opecode) {
			case BIN(trace):
				break;
			case BIN(putself):
				self = rb_mJit->valueVal(th->cfp->self);
				break;
			case BIN(putobject):
				{
					Value *alloca = rb_mJit->builder->CreateAlloca(rb_mJit->valueTy);
					rb_mJit->builder->CreateStore(rb_mJit->valueVal(insn->operands[0]), alloca);
					Value *putVal = rb_mJit->builder->CreateLoad(alloca);
					__stack.push_back(putVal);
				}
				break;
			case BIN(opt_plus):
				{
					Value *recv = __stack.back(); __stack.pop_back();
					Value *obj = __stack.back(); __stack.pop_back();
					Value *obj2 = rb_mJit->builder->CreateAnd(obj, -2);
					Value *result = rb_mJit->builder->CreateAdd(recv, obj2);
					__stack.push_back(result);
				}
				break;
			case BIN(opt_send_without_block):
				{
					Type *llvm_call_info_ptr_t = rb_mJit->module->getTypeByName("struct.rb_call_info_struct")->getPointerTo();
					VALUE rb_ci = insn->operands[0];
					Value *ci = rb_mJit->builder->CreateIntToPtr(rb_mJit->valueVal(rb_ci), llvm_call_info_ptr_t);

					// rb_mJit->builder->CreateCall4(llvm_caller_setup_arg_block, arg_th, arg_cfp, ci, rb_mJit->int32Zero); // ブロック引数時に必要

					Value *sp_elmptr = rb_mJit->builder->CreateStructGEP(arg_cfp, 1);
					Value *sp = rb_mJit->builder->CreateLoad(sp_elmptr);
					rb_mJit->builder->CreateStore(__stack.back(), sp); __stack.pop_back();
					Value *sp_incptr = rb_mJit->builder->CreateGEP(sp, rb_mJit->valueOne);
					rb_mJit->builder->CreateStore(sp_incptr, sp_elmptr);

					// vm_search_method(ci, ci->recv = TOPN(ci->argc));
					rb_mJit->builder->CreateCall2(llvm_search_method, ci, self);

					Value *ci_call_elmptr = rb_mJit->builder->CreateStructGEP(ci, 14);
					Value *ci_call = rb_mJit->builder->CreateLoad(ci_call_elmptr);
					rb_mJit->builder->CreateCall3(ci_call, arg_th, arg_cfp, ci);
				}
				break;
			default:
				JIT_DEBUG_LOG2("trace: unknown[%d(%s)]", insn->opecode, insn_name(insn->opecode));
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

extern "C"
VALUE
jit_testadd(VALUE self, VALUE left, VALUE right)
{
	char *llvm_error = 0;
	LLVMModuleRef module = LLVMModuleCreateWithName("LLVM Ruby module");
	LLVMTypeRef fmain_args[] = { LLVMInt64Type(), LLVMInt64Type() };
	LLVMValueRef fmain = LLVMAddFunction(module, "main", LLVMFunctionType(LLVMInt64Type(), fmain_args, 2, 0));
	LLVMSetFunctionCallConv(fmain, LLVMCCallConv);
	LLVMValueRef arg1 = LLVMGetParam(fmain, 0);
	LLVMValueRef arg2 = LLVMGetParam(fmain, 1);
	LLVMBasicBlockRef entry = LLVMAppendBasicBlock(fmain, "entry");
	LLVMBuilderRef builder = LLVMCreateBuilder();
	LLVMPositionBuilderAtEnd(builder, entry);
	LLVMValueRef add = LLVMBuildAdd(builder, arg1, arg2, "result");
	LLVMBuildRet(builder, add);
	LLVMVerifyModule(module, LLVMAbortProcessAction, &llvm_error);
	LLVMDisposeMessage(llvm_error);

	LLVMExecutionEngineRef engine;
	LLVMModuleProviderRef provider = LLVMCreateModuleProviderForExistingModule(module);
	llvm_error = NULL;
	if(LLVMCreateJITCompiler(&engine, provider, 2, &llvm_error) != 0) {
		fprintf(stderr, "%s\n", llvm_error);
		LLVMDisposeMessage(llvm_error);
		abort();
	}

	LLVMPassManagerRef pass = LLVMCreatePassManager();
	LLVMAddTargetData(LLVMGetExecutionEngineTargetData(engine), pass);
	LLVMAddConstantPropagationPass(pass);
	LLVMAddInstructionCombiningPass(pass);
	LLVMAddPromoteMemoryToRegisterPass(pass);
	LLVMAddGVNPass(pass);
	LLVMAddCFGSimplificationPass(pass);
	LLVMRunPassManager(pass, module);

	LLVMDumpModule(module);


	int l = NUM2INT(left);
	int r = NUM2INT(right);
	printf("%d + %d\n", l, r);
	void *f = LLVMGetPointerToGlobal(engine, fmain);
	int (* fjit_add)(int, int) = (int (*)(int, int))f;
	fprintf(stderr, "%d\n", fjit_add(l, r));

	LLVMDisposePassManager(pass);
	LLVMDisposeBuilder(builder);
	LLVMDisposeExecutionEngine(engine);

	return Qtrue;
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

// extern "C"
// VALUE
// jit_debug_compile(VALUE self, VALUE code)
// {
// }

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
		// rb_mJit->builder->CreateRetVoid();
		// rb_mJit->builder->SetInsertPoint(entry, --entry->end());
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
	rb_define_singleton_method(rb_mJIT, "add", RUBY_METHOD_FUNC(jit_testadd), 2);
	rb_define_singleton_method(rb_mJIT, "compile", RUBY_METHOD_FUNC(jit_compile), 1);
	// rb_define_singleton_method(rb_mJIT, "trace_dump", RUBY_METHOD_FUNC(jit_trace_dump), 0);
}

