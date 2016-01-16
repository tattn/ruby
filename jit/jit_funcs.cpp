#include "jit_funcs.h"
#include "llvm/IR/ValueSymbolTable.h"

#undef rb_float_new
#undef rb_float_value

JITFuncs::JITFuncs(Module *module, JITTypes *types) :
t(types),
vm_caller_setup_arg_block_extern(0),
vm_pop_frame_extern(0),
vm_search_method_extern(0),
vm_getspecial(0),
VM_EP_LEP(0),
rb_str_resurrect(0),
rb_gvar_get(0),
rb_obj_as_string(0),
rb_ary_new_from_values(0),
rb_ary_resurrect(0),
vm_expandarray(0),
rb_range_new(0),
vm_getinstancevariable_extern(0),
vm_setinstancevariable_extern(0),
rb_cvar_get(0),
vm_get_cvar_base(0),
rb_vm_get_cref(0),
rb_cvar_set(0),
vm_get_ev_const_extern(0),
vm_check_if_namespace(0),
rb_const_set(0),
rb_gvar_set(0),
vm_defined(0),
rb_funcall(0),
vm_throw(0),
vm_get_cbase(0),
vm_get_const_base(0),
opt_eq_func(0),
rb_float_new(0),
rb_float_value(0),
rb_str_append(0),
rb_ary_entry(0),
rb_hash_aref(0),
rb_ary_store(0),
rb_hash_aset(0)
{
	return;

#if 0
#define DEFINE_FUNC(name, rettype, ...) \
	static FunctionType* name##_type = FunctionType::get((rettype), true); \
	name = Function::Create(name##_type, Function::ExternalLinkage, ("_" #name), module);

#define DEFINE_FUNC_RENAME(name, newname, rettype, ...) \
	static FunctionType* name##_type = FunctionType::get((rettype), true); \
	newname = Function::Create(name##_type, Function::ExternalLinkage, ("_" #name), module);

#define DEFINE_FUNC2(name, rettype, ...) DEFINE_FUNC(name, rettype)

	// void vm_caller_setup_arg_block(const rb_thread_t *th, rb_control_frame_t *reg_cfp, rb_call_info_t *ci, const int is_super)
	DEFINE_FUNC_RENAME(vm_caller_setup_arg_block_extern, vm_caller_setup_arg_block,
			t->voidT, t->rb_thread_t, t->rb_control_frame_t, t->rb_call_info_t, t->intT);

	// void vm_pop_frame(rb_thread_t *th)
	DEFINE_FUNC_RENAME(vm_pop_frame_extern, vm_pop_frame, t->voidT, t->rb_thread_t);

	// void vm_search_method(rb_call_info_t *ci, VALUE recv)
	DEFINE_FUNC_RENAME(vm_search_method_extern, vm_search_method, t->voidT, t->rb_call_info_t, t->valueT);

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
	DEFINE_FUNC_RENAME(vm_getinstancevariable_extern, vm_getinstancevariable, t->valueT, t->valueT, t->valueT, t->pvalueT);

	// void vm_setinstancevariable(VALUE obj, ID id, VALUE val, IC ic)
	DEFINE_FUNC_RENAME(vm_setinstancevariable_extern, vm_setinstancevariable, t->voidT, t->valueT, t->valueT, t->valueT, t->pvalueT);

	//VALUE rb_cvar_get(VALUE klass, ID id)
	DEFINE_FUNC(rb_cvar_get, t->valueT, t->valueT, t->valueT);

	// VALUE vm_get_cvar_base(const rb_cref_t *cref, rb_control_frame_t *cfp)
	DEFINE_FUNC(vm_get_cvar_base, t->valueT, t->pvalueT, t->rb_control_frame_t);

	// rb_cref_t *rb_vm_get_cref(const VALUE *ep)
	DEFINE_FUNC(rb_vm_get_cref, t->pvalueT, t->pvalueT);

	// void rb_cvar_set(VALUE klass, ID id, VALUE val)
	DEFINE_FUNC(rb_cvar_set, t->voidT, t->valueT, t->valueT, t->valueT);

	// VALUE vm_get_ev_const(rb_thread_t *th, VALUE orig_klass, ID id, int is_defined)
	DEFINE_FUNC_RENAME(vm_get_ev_const_extern, vm_get_ev_const, t->valueT, t->rb_thread_t, t->valueT, t->valueT, t->intT);

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

	// VALUE vm_get_cbase(const VALUE *ep)
	DEFINE_FUNC(vm_get_cbase, t->valueT, t->pvalueT);

	// VALUE vm_get_const_base(const VALUE *ep)
	DEFINE_FUNC(vm_get_const_base, t->valueT, t->pvalueT);

	// VALUE opt_eq_func(VALUE recv, VALUE obj, CALL_INFO ci)
	DEFINE_FUNC(opt_eq_func, t->valueT, t->valueT, t->valueT, t->rb_call_info_t);

    // VALUE rb_float_new(double d)
	DEFINE_FUNC(rb_float_new, t->valueT, t->doubleT);

    // double rb_float_value_inline(VALUE v)
	DEFINE_FUNC(rb_float_value, t->doubleT, t->valueT);

	// VALUE rb_str_append(VALUE str, VALUE str2)
	DEFINE_FUNC(rb_str_append, t->valueT, t->valueT, t->valueT);

	// VALUE rb_ary_entry(VALUE ary, long offset)
	DEFINE_FUNC(rb_ary_entry, t->valueT, t->valueT, t->longT);

	// VALUE rb_hash_aref(VALUE hash, VALUE key)
	DEFINE_FUNC(rb_hash_aref, t->valueT, t->valueT, t->valueT);

	// void rb_ary_store(VALUE ary, long idx, VALUE val)
	DEFINE_FUNC(rb_ary_store, t->voidT, t->valueT, t->longT, t->valueT);

	// VALUE rb_hash_aset(VALUE hash, VALUE key, VALUE val)
	DEFINE_FUNC(rb_hash_aset, t->valueT, t->valueT, t->valueT, t->valueT);


#ifdef JIT_DEBUG_FLAG
	DEFINE_FUNC2(printf, t->int32T, t->int8T->getPointerTo());
#endif

#undef DEFINE_FUNC
#undef DEFINE_FUNC2
#endif
}

