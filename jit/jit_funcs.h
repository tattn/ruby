struct JITFuncs
{
	JITFuncs(Module *module, JITTypes *types);

	JITTypes *t;

	Function *vm_pop_frame;
	Function *vm_search_method;
	Function *vm_getspecial;
	Function *VM_EP_LEP;
	Function *rb_str_resurrect;

#ifdef JIT_DEBUG_FLAG
	Function *printf;
#endif
};
