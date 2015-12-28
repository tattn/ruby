#ifndef RUBY_JIT_H
#define RUBY_JIT_H

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

typedef struct jit_trace_result_struct {
	VALUE retval;
	int jmp;
	VALUE *pc;
} jit_trace_ret_t;

/* compile.c */
VALUE *rb_iseq_original_iseq(rb_iseq_t *iseq);

#define _ADD_PC(x) (reg_pc += (x))

// VM_FRAME_TYPE を2回計算していて冗長?
// IFUNC: Iterator function?, frame of block when call yield on the method with block
//        cfp->iseq is not rb_iseq_t, but NODE(AST).
// DUMMY: http://svn.ruby-lang.org/cgi-bin/viewvc.cgi?revision=50701&view=revision 
#define JIT_IS_PASS(cfp) (VM_FRAME_TYPE(cfp) == VM_FRAME_MAGIC_IFUNC || VM_FRAME_TYPE(cfp) == VM_FRAME_MAGIC_DUMMY)

// #define JIT_TRACE \
// if (is_jit_tracing) { static jit_trace_ret_t ret; jit_trace_insn(th, reg_cfp, reg_pc, &ret); if (ret.jmp == -1) { return ret.retval; } else if (ret.jmp != 1) { #<{(| _ADD_PC(ret.jmp); |)}># RESTORE_REGS();  goto *(void const *)GET_CURRENT_INSN(); } }

#define JIT_TRACE \
 { static jit_trace_ret_t ret; jit_trace_insn(th, reg_cfp, reg_pc, &ret); if (ret.jmp == -1) { return ret.retval; } else if (ret.jmp != 1) { /* _ADD_PC(ret.jmp); */ RESTORE_REGS();  goto *(void const *)GET_CURRENT_INSN(); } }

// cfp->iseq == 0 is CFUNC
// CFUNCはバイトコードに遷移せずに値を返すのでトレースを継続する
#define JIT_NEW_TRACE(cfp) if (!JIT_IS_PASS(cfp) && cfp->iseq) jit_push_new_trace(cfp)
#define JIT_POP_TRACE(cfp) if (!JIT_IS_PASS(cfp) && cfp->iseq) jit_push_new_trace(cfp)
#define JIT_SET_CFP(cfp)   if (!JIT_IS_PASS(cfp) && cfp->iseq) jit_push_new_trace(cfp)

// #define JIT_NEW_TRACE(cfp) JIT_SET_CFP_WITHOUT_CHECK(cfp)
// #define JIT_POP_TRACE(cfp) JIT_SET_CFP_WITHOUT_CHECK(cfp)
// #define JIT_SET_CFP(cfp)   JIT_SET_CFP_WITHOUT_CHECK(cfp)
#define JIT_SET_CFP_WITHOUT_CHECK(cfp) if (cfp->iseq) jit_push_new_trace(cfp)

void ruby_jit_init(void);
void jit_add_symbol(const char* name, void* pfunc);
void jit_add_iseq(rb_iseq_t *iseq);
void jit_push_new_trace(rb_control_frame_t *cfp);
void jit_trace_insn(rb_thread_t *th, rb_control_frame_t *cfp, VALUE *pc, jit_trace_ret_t *ret);
void jit_trace_dump(rb_thread_t *th);
void jit_trace_jump(int dest);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* RUBY_JIT_H */
