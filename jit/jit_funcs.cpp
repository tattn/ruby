#include "jit_funcs.h"

extern "C" {
void vm_search_method(rb_call_info_t *ci, VALUE recv);
void vm_caller_setup_arg_block(const rb_thread_t *th, rb_control_frame_t *reg_cfp, rb_call_info_t *ci, const int is_super);
void vm_pop_frame(rb_thread_t *th);
}


JITFuncs::JITFuncs(Module *module, JITTypes *types)
: t(types)
{
#define DEFINE_FUNC(name, rettype, ...) \
	type = FunctionType::get((rettype), {__VA_ARGS__}, false); \
	name = Function::Create(type, Function::ExternalLinkage, (#name), module);

#define DEFINE_FUNC2(name, rettype, ...) \
	type = FunctionType::get((rettype), {__VA_ARGS__}, true); \
	name = Function::Create(type, Function::ExternalLinkage, (#name), module);

	FunctionType *type;

	// void vm_pop_frame(rb_thread_t *th)
	DEFINE_FUNC(vm_pop_frame, t->voidT, t->rb_thread_t);

	// void vm_search_method(rb_call_info_t *ci, VALUE recv)
	DEFINE_FUNC(vm_search_method, t->voidT, t->rb_call_info_t, t->valueT);

	// VALUE vm_getspecial(rb_thread_t *th, VALUE *lep, rb_num_t key, rb_num_t type)
	DEFINE_FUNC(vm_getspecial, t->valueT, t->rb_thread_t, t->pvalueT, t->longT, t->longT);

	// VALUE *VM_EP_LEP(VALUE *ep)
	DEFINE_FUNC(VM_EP_LEP, t->pvalueT, t->pvalueT);

	// VALUE rb_str_resurrect(VALUE str)
	DEFINE_FUNC(rb_str_resurrect, t->valueT, t->valueT);



#ifdef JIT_DEBUG_FLAG
	DEFINE_FUNC2(printf, t->int32T, t->int8T->getPointerTo());
#endif

#undef DEFINE_FUNC
#undef DEFINE_FUNC2
}

