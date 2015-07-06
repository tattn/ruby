
// #define VM_EP_LEP_P(ep)     VM_ENVVAL_BLOCK_PTR_P((ep)[0])
//
// static inline Value *
// JIT_EP_LEP(Value *ep)
// {
//     while (!VM_EP_LEP_P(ep)) {
// 	ep = VM_EP_PREV_EP(ep);
//     }
//     return ep;
// }
//

// static inline Value *
// JIT_INT2FIX(VALUE val)
// {
// 	return JIT_INT2FIX(RB_JIT->valueVal(val));
// }
//
// // template <class T>
// // static inline Value *
// // JIT_INT2FIX(T *val);
//
// // template <>
// static inline Value *
// JIT_INT2FIX(Value *val)
// {
// 	return BUILDER->CreateOr(BUILDER->CreateShl(val, 1), FIXNUM_FLAG);
// }
