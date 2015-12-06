#pragma once

struct JITTypes
{
    JITTypes();

	Type *voidT;
	Type *int8T;
	Type *int32T;
	Type *int64T;
	Type *intT;
	Type *longT;
	Type *sizeT;
	Type *doubleT;
	Type *ptrT;
	Type *valueT;
	PointerType *pvalueT;

	PointerType *rb_thread_t;
	PointerType *rb_control_frame_t;
	PointerType *rb_call_info_t;

	StructType *RBasic;
	PointerType *PRBasic;
	StructType *RFloat;
	PointerType *PRFloat;

	StructType *jit_func_ret_t;
};

struct JITValues
{
    JITValues(JITTypes *types);

	JITTypes *types;


	Value *valueZero;
	Value *valueOne;
	Constant *int32Zero;
	Value *nilV;
	Value *undefV;
	Value *trueV;
	Value *falseV;

	Value* value(VALUE val);
	Value* signedValue(int val);

	Value* intV(int val);
};
