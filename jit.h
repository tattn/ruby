#ifndef RUBY_JIT_H
#define RUBY_JIT_H

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* compile.c */
VALUE *rb_iseq_original_iseq(rb_iseq_t *iseq);

/* vm_exec.c */
void vm_search_method(rb_call_info_t *ci, VALUE recv);
void vm_set_eval_stack(rb_thread_t * th, VALUE iseqval, const rb_cref_t *cref, rb_block_t *base_block);
void th_init(rb_thread_t *th, VALUE self);


void ruby_jit_init(void);

void ruby_jit_test(void);


// VALUE rb_jit_compile_node(VALUE self, NODE *node);

VALUE jit_insn_to_llvm(rb_thread_t *th);


extern int is_jit_tracing;

#define JIT_TRACE if (is_jit_tracing) jit_trace_insn(th, reg_cfp, reg_pc)

void jit_trace_insn(rb_thread_t *th, rb_control_frame_t *cfp, VALUE *pc);
void jit_trace_dump(rb_thread_t *th);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* RUBY_JIT_H */
