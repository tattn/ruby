#include "jit_types.h"

using namespace llvm;

JITTypes::JITTypes()
{
	voidT = Type::getVoidTy(getGlobalContext());
	int8T = Type::getInt8Ty(getGlobalContext());
	int32T = Type::getInt32Ty(getGlobalContext());
	int64T = Type::getInt64Ty(getGlobalContext());
	intT = Type::getIntNTy(getGlobalContext(), sizeof(int) * 8);

	ptrT = PointerType::getUnqual(int8T);
	valueT = int64T;
	pvalueT = PointerType::getUnqual(int64T);

	StructType *newStruct;
	std::vector<llvm::Type*> elements;

// typedef struct rb_control_frame_struct {
//     VALUE *pc;			#<{(| cfp[0] |)}>#
//     VALUE *sp;			#<{(| cfp[1] |)}>#
//     rb_iseq_t *iseq;		#<{(| cfp[2] |)}>#
//     VALUE flag;			#<{(| cfp[3] |)}>#
//     VALUE self;			#<{(| cfp[4] / block[0] |)}>#
//     VALUE klass;		#<{(| cfp[5] / block[1] |)}>#
//     VALUE *ep;			#<{(| cfp[6] / block[2] |)}>#
//     rb_iseq_t *block_iseq;	#<{(| cfp[7] / block[3] |)}>#
//     VALUE proc;			#<{(| cfp[8] / block[4] |)}>#
//
// #if VM_DEBUG_BP_CHECK
//     VALUE *bp_check;		#<{(| cfp[9] |)}>#
// #endif
// } rb_control_frame_t;
	newStruct = StructType::create(getGlobalContext(), "struct.rb_control_frame_struct");
	elements.clear();
	elements.push_back(pvalueT);
	elements.push_back(pvalueT);
	elements.push_back(pvalueT);
	elements.push_back(valueT);
	elements.push_back(valueT);
	elements.push_back(valueT);
	elements.push_back(pvalueT);
	elements.push_back(pvalueT);
	elements.push_back(valueT);
	newStruct->setBody(elements);
	this->rb_control_frame_t = newStruct->getPointerTo();

	// sorry
	this->rb_thread_t = pvalueT;

// typedef struct rb_call_info_struct {
//     #<{(| fixed at compile time |)}>#
//     ID mid;
//
//     unsigned int flag;
//     int orig_argc;
//     rb_iseq_t *blockiseq;
//     rb_call_info_kw_arg_t *kw_arg;
//
//     #<{(| inline cache: keys |)}>#
//     rb_serial_t method_state;
//     rb_serial_t class_serial;
//     VALUE klass;
//
//     #<{(| inline cache: values |)}>#
//     const rb_method_entry_t *me;
//     VALUE defined_class;
//
//     #<{(| temporary values for method calling |)}>#
//     struct rb_block_struct *blockptr;
//     VALUE recv;
//     int argc;
//     union {
// 	int opt_pc; #<{(| used by iseq |)}>#
// 	int index; #<{(| used by ivar |)}>#
// 	enum method_missing_reason method_missing_reason; #<{(| used by method_missing |)}>#
// 	int inc_sp; #<{(| used by cfunc |)}>#
//     } aux;
//
//     VALUE (*call)(struct rb_thread_struct *th, struct rb_control_frame_struct *cfp, struct rb_call_info_struct *ci);
// } rb_call_info_t;
	newStruct = StructType::create(getGlobalContext(), "struct.rb_call_info_struct");
	this->rb_call_info_t = newStruct->getPointerTo();
	elements.clear();
	elements.push_back(valueT);		// mid
	elements.push_back(intT);		// flag
	elements.push_back(intT);		// orig_argc
	elements.push_back(pvalueT);	// blockiseq
	elements.push_back(pvalueT);	// kw_arg
	elements.push_back(int64T);		// method_state
	elements.push_back(int64T);		// class_serial
	elements.push_back(valueT);		// klass
	elements.push_back(pvalueT);	// me
	elements.push_back(valueT);		// defined_class
	elements.push_back(pvalueT);	// blockptr
	elements.push_back(valueT);		// recv
	elements.push_back(intT);		// argc
	elements.push_back(intT);		// aux

	std::vector<Type*> pFnTypes;
	pFnTypes.push_back(this->rb_thread_t);
	pFnTypes.push_back(this->rb_control_frame_t);
	pFnTypes.push_back(this->rb_call_info_t);
	FunctionType* FuncTy = FunctionType::get(valueT, pFnTypes, false);
	PointerType* pFnT = PointerType::get(FuncTy, 0);

	elements.push_back(pFnT);		// call

	newStruct->setBody(elements);

}


JITValues::JITValues(JITTypes *types)
: types(types)
{
	valueZero = ConstantInt::get(types->int64T, 0);
	valueOne = ConstantInt::get(types->int64T, 1);
	int32Zero = ConstantInt::get(types->int32T, 0);
	valueQundef = ConstantInt::get(types->int64T, Qundef);
}

Value* JITValues::value(VALUE val)
{
	return ConstantInt::get(types->int64T, val);
}

Value* JITValues::signedValue(int val)
{
	return llvm::ConstantInt::getSigned(types->int64T, val);
}
