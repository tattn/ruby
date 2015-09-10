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

	// VALUE rb_gvar_get(struct global_entry *entry)
	DEFINE_FUNC(rb_gvar_get, t->valueT, t->pvalueT);

	// VALUE rb_obj_as_string(VALUE obj)
	DEFINE_FUNC(rb_obj_as_string, t->valueT, t->valueT);

	// VALUE rb_ary_new_from_values(long n, const VALUE *elts)
	DEFINE_FUNC(rb_ary_new_from_values, t->valueT, t->longT, t->pvalueT);

	// VALUE rb_ary_resurrect(VALUE ary)
	DEFINE_FUNC(rb_ary_resurrect, t->valueT, t->valueT);

	// void vm_expandarray(rb_control_frame_t *cfp, VALUE ary, rb_num_t num, int flag)
	DEFINE_FUNC(vm_expandarray, t->voidT, t->rb_control_frame_t, t->valueT, t->longT, t->intT);

	// VALUE rb_range_new(VALUE beg, VALUE end, int exclude_end)
	DEFINE_FUNC(rb_range_new, t->voidT, t->valueT, t->valueT, t->intT);

	// VALUE vm_getinstancevariable(VALUE obj, ID id, IC ic)
	DEFINE_FUNC(vm_getinstancevariable, t->valueT, t->valueT, t->valueT, t->pvalueT);

	// void vm_setinstancevariable(VALUE obj, ID id, VALUE val, IC ic)
	DEFINE_FUNC(vm_setinstancevariable, t->voidT, t->valueT, t->valueT, t->valueT, t->pvalueT);

	//VALUE rb_cvar_get(VALUE klass, ID id)
	DEFINE_FUNC(rb_cvar_get, t->valueT, t->valueT, t->valueT);

	// VALUE vm_get_cvar_base(const rb_cref_t *cref, rb_control_frame_t *cfp)
	DEFINE_FUNC(vm_get_cvar_base, t->valueT, t->pvalueT, t->rb_control_frame_t);

	// rb_cref_t *rb_vm_get_cref(const VALUE *ep)
	DEFINE_FUNC(rb_vm_get_cref, t->pvalueT, t->pvalueT);

	// void rb_cvar_set(VALUE klass, ID id, VALUE val)
	DEFINE_FUNC(rb_cvar_set, t->voidT, t->valueT, t->valueT, t->valueT);

	// VALUE vm_get_ev_const(rb_thread_t *th, VALUE orig_klass, ID id, int is_defined)
	DEFINE_FUNC(vm_get_ev_const, t->valueT, t->rb_thread_t, t->valueT, t->valueT, t->intT);

	// void vm_check_if_namespace(VALUE klass)
	DEFINE_FUNC(vm_check_if_namespace, t->voidT, t->valueT);

	// void rb_const_set(VALUE klass, ID id, VALUE val)
	DEFINE_FUNC(rb_const_set, t->voidT, t->valueT, t->valueT, t->valueT);

	// VALUE rb_gvar_set(struct global_entry *entry, VALUE val)
	DEFINE_FUNC(rb_gvar_set, t->valueT, t->pvalueT, t->valueT);

	// VALUE vm_defined(rb_thread_t *th, rb_control_frame_t *reg_cfp, rb_num_t op_type, VALUE obj, VALUE needstr, VALUE v)
	DEFINE_FUNC(vm_defined, t->valueT, t->rb_thread_t, t->rb_control_frame_t, t->longT, t->valueT, t->valueT, t->valueT);

	// VALUE rb_funcall(VALUE recv, ID mid, int n, ...)
	DEFINE_FUNC2(rb_funcall, t->valueT, t->valueT, t->valueT, t->intT);

	// VALUE vm_throw(rb_thread_t *th, rb_control_frame_t *reg_cfp, rb_num_t throw_state, VALUE throwobj)
	DEFINE_FUNC(vm_throw, t->valueT, t->rb_thread_t, t->rb_control_frame_t, t->longT, t->valueT);


#ifdef JIT_DEBUG_FLAG
	DEFINE_FUNC2(printf, t->int32T, t->int8T->getPointerTo());
#endif

#undef DEFINE_FUNC
#undef DEFINE_FUNC2
}

