#ifndef RUBY_JIT_H
#define RUBY_JIT_H

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* compile.c */
VALUE *rb_iseq_original_iseq(rb_iseq_t *iseq);


void ruby_jit_init(void);


// VALUE rb_jit_compile_node(VALUE self, NODE *node);

VALUE jit_insn_to_llvm(rb_thread_t *th);


extern int is_jit_tracing;



// #define JIT_IS_IFUNC(type) ((int)(type & VM_FRAME_MAGIC_MASK) == VM_FRAME_MAGIC_IFUNC)
// VM_FRAME_TYPE を2回計算していて冗長
#define JIT_IS_PASS(cfp) (VM_FRAME_TYPE(cfp) == VM_FRAME_MAGIC_IFUNC || VM_FRAME_TYPE(cfp) == VM_FRAME_MAGIC_DUMMY)

#define JIT_TRACE \
if (is_jit_tracing) { int jmp = jit_trace_insn(th, reg_cfp, reg_pc); if (jmp) { /* RESTORE_REGS(); */ ADD_PC(jmp); goto *(void const *)GET_CURRENT_INSN(); } }

#define JIT_NEW_TRACE(cfp) if (is_jit_tracing && !JIT_IS_PASS(cfp)) jit_push_new_trace(cfp)
// #define JIT_NEW_TRACE(cfp)
#define JIT_POP_TRACE(cfp) if (is_jit_tracing && !JIT_IS_PASS(cfp)) jit_pop_trace(cfp)
// #define JIT_POP_TRACE(cfp)

#define JIT_SET_CFP(cfp) if (is_jit_tracing && !JIT_IS_PASS(cfp)) jit_pop_trace(cfp)
// #define JIT_SET_CFP(cfp)

void jit_add_iseq(rb_iseq_t *iseq);

void jit_trace_start(rb_control_frame_t *cfp);
void jit_push_new_trace(rb_control_frame_t *cfp);
rb_control_frame_t *jit_pop_trace(rb_control_frame_t *cfp);
int jit_trace_insn(rb_thread_t *th, rb_control_frame_t *cfp, VALUE *pc);
void jit_trace_dump(rb_thread_t *th);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* RUBY_JIT_H */
