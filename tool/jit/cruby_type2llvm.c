#include "internal.h"
#include "insns.inc"
#include "insns_info.inc"
#include "vm_core.h"
#include <stdio.h>
#include <stdint.h>

void vm_caller_setup_arg_block(const rb_thread_t *th, rb_control_frame_t *reg_cfp, rb_call_info_t *ci, const int is_super);
void vm_search_method(rb_call_info_t *ci, VALUE recv);
void vm_pop_frame(rb_thread_t *th);

void function(rb_thread_t *th, rb_control_frame_t *cfp) {
	vm_caller_setup_arg_block(0, 0, 0, 0);
	vm_search_method(0, 0);
	vm_pop_frame(0);

	VALUE idx = 6;

    *(cfp->ep - idx) = 100;
}
