struct JITFuncs
{
	JITFuncs(Module *module, JITTypes *types);

	JITTypes *t;

	Function *vm_caller_setup_arg_block;
	Function *vm_pop_frame;
	Function *vm_search_method;
	Function *vm_getspecial;
	Function *VM_EP_LEP;
	Function *rb_str_resurrect;
	Function *rb_gvar_get;
	Function *rb_obj_as_string;
	Function *rb_ary_new_from_values;
	Function *rb_ary_resurrect;
	Function *vm_expandarray;
	Function *rb_range_new;
	Function *vm_getinstancevariable;
	Function *vm_setinstancevariable;
	Function *rb_cvar_get;
	Function *vm_get_cvar_base;
	Function *rb_vm_get_cref;
	Function *rb_cvar_set;
	Function *vm_get_ev_const;
	Function *vm_check_if_namespace;
	Function *rb_const_set;
	Function *rb_gvar_set;
	Function *vm_defined;
	Function *rb_funcall;
	Function *vm_throw;
	Function *vm_get_cbase;
	Function *vm_get_const_base;
	Function *opt_eq_func;
    Function *rb_float_new;
    Function *rb_float_new_inline;
    Function *rb_float_value;
	Function *rb_str_append;
	Function *rb_ary_entry;
	Function *rb_hash_aref;
	Function *rb_ary_store;
	Function *rb_hash_aset;


#ifdef JIT_DEBUG_FLAG
	Function *printf;
#endif
};
