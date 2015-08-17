#pragma once
#include "../vm_core.h"

typedef struct jit_insn_struct {
	rb_thread_t *th;
	rb_control_frame_t *cfp;
	VALUE *pc;
	int opecode;
	int index;
	int len; //default = 1
	// BasicBlock *bb;
} jit_insn_t;

typedef struct jit_trace_struct {
	jit_insn_t **insns;
	unsigned insns_size;
	unsigned insns_iterator;
	rb_iseq_t *iseq;

	// std::unordered_set<VALUE*> pc_set;
	// std::function<jit_func_ret_t(rb_thread_t*, rb_control_frame_t*)> jited = nullptr;
} jit_trace_t;

#define JIT_TRACE()


int jit_trace_insn(rb_thread_t *th, rb_control_frame_t *cfp, VALUE *pc);
