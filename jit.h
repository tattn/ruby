#ifndef RUBY_JIT_H
#define RUBY_JIT_H

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* compile.c */
VALUE *rb_iseq_original_iseq(rb_iseq_t *iseq);


void ruby_jit_init(void);

void ruby_jit_test(void);


// VALUE rb_jit_compile_node(VALUE self, NODE *node);

VALUE jit_insn_to_llvm(rb_thread_t *th);


extern int is_jit_tracing;

#define JIT_TRACE if (is_jit_tracing) jit_trace_insn(th, reg_cfp, reg_pc)
#define JIT_NEW_TRACE(cfp) if (is_jit_tracing) jit_push_new_trace(cfp)


void jit_trace_start(rb_thread_t *th);
void jit_push_new_trace(rb_control_frame_t *cfp);
void jit_pop_trace();
void jit_trace_insn(rb_thread_t *th, rb_control_frame_t *cfp, VALUE *pc);
void jit_trace_dump(rb_thread_t *th);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* RUBY_JIT_H */
