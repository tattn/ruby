struct JITFuncs
{
	JITFuncs(Module *module, JITTypes *types);

	JITTypes *t;

	Function *vm_pop_frame;
	Function *vm_search_method;

#ifdef JIT_DEBUG_FLAG
	Function *printf;
#endif
};
