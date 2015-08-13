; ModuleID = 'cruby_type2llvm.c'
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-darwin14.4.0"

%struct.rb_thread_struct = type { %struct.list_node, i64, %struct.rb_vm_struct*, i64*, i64, %struct.rb_control_frame_struct*, i32, i32, i64, i32, i32, %struct.rb_block_struct*, %struct.rb_method_entry_struct*, %struct.rb_call_info_struct*, i64, i64, %struct.rb_block_struct*, i64*, i64, %struct._opaque_pthread_t*, i32, i32, i32, %struct.native_thread_data_struct, i8*, i64, i64, i64, i64, i64, i32, i32, i64, %struct._opaque_pthread_mutex_t, %struct.rb_thread_cond_struct, %struct.rb_unblock_callback, i64, %struct.rb_mutex_struct*, %struct.rb_vm_tag*, %struct.rb_vm_protect_tag*, i32, i32, %struct.st_table*, i64, i64, %struct.rb_thread_list_struct*, i64, i64, i64 (...)*, %struct.anon.12, i64, %struct.rb_hook_list_struct, %struct.rb_trace_arg_struct*, %struct.rb_fiber_struct*, %struct.rb_fiber_struct*, [37 x i32], %struct.rb_ensure_list*, i32, i32, i8*, i64, i64 }
%struct.list_node = type { %struct.list_node*, %struct.list_node* }
%struct.rb_vm_struct = type { i64, %struct.rb_global_vm_lock_struct, %struct._opaque_pthread_mutex_t, %struct.rb_thread_struct*, %struct.rb_thread_struct*, %struct.list_head, i64, i64, i32, i32, i32, i32, i64, [4 x i64], i64, i64, i64, i64, i64, i64, i64, %struct.st_table*, %struct.st_table*, [32 x %struct.anon.1], %struct.rb_hook_list_struct, %struct.st_table*, %struct.rb_postponed_job_struct*, i32, i32, i64, i64, i64, i64, i64, i64, %struct.rb_objspace*, %struct.RArray, i64*, %struct.st_table*, %struct.anon.5, [22 x i16] }
%struct.rb_global_vm_lock_struct = type { i64, %struct._opaque_pthread_mutex_t, i64, %struct.rb_thread_cond_struct, %struct.rb_thread_cond_struct, %struct.rb_thread_cond_struct, i32, i32 }
%struct.list_head = type { %struct.list_node }
%struct.anon.1 = type { i64, i32 }
%struct.rb_postponed_job_struct = type opaque
%struct.rb_objspace = type opaque
%struct.RArray = type { %struct.RBasic, %union.anon.2 }
%struct.RBasic = type { i64, i64 }
%union.anon.2 = type { %struct.anon.3 }
%struct.anon.3 = type { i64, %union.anon.4, i64* }
%union.anon.4 = type { i64 }
%struct.anon.5 = type { i64, i64, i64, i64 }
%struct.rb_method_entry_struct = type { i64, i64, %struct.rb_method_definition_struct*, i64, i64 }
%struct.rb_method_definition_struct = type { i32, i32, %union.anon.8, i64 }
%union.anon.8 = type { %struct.rb_method_cfunc_struct }
%struct.rb_method_cfunc_struct = type { i64 (...)*, i64 (i64 (...)*, i64, i32, i64*)*, i32 }
%struct.rb_call_info_struct = type { i64, i32, i32, %struct.rb_iseq_struct*, %struct.rb_call_info_kw_arg_struct*, i64, i64, i64, %struct.rb_method_entry_struct*, i64, %struct.rb_block_struct*, i64, i32, %union.anon.9, i64 (%struct.rb_thread_struct*, %struct.rb_control_frame_struct*, %struct.rb_call_info_struct*)* }
%struct.rb_iseq_struct = type { i32, i32, %struct.rb_iseq_location_struct, i64*, i32, i32, i64, i64, %struct.iseq_line_info_entry*, i64*, i32, i32, %union.iseq_inline_storage_entry*, i32, i32, %struct.rb_call_info_struct*, %struct.anon.10, %struct.iseq_catch_table*, %struct.rb_iseq_struct*, %struct.rb_iseq_struct*, i64, i64, i64, i64, i64, %struct.iseq_compile_data*, i64* }
%struct.rb_iseq_location_struct = type { i64, i64, i64, i64, i64 }
%struct.iseq_line_info_entry = type opaque
%union.iseq_inline_storage_entry = type { %struct.iseq_inline_cache_entry }
%struct.iseq_inline_cache_entry = type { i64, %struct.rb_cref_struct*, %union.anon.7 }
%struct.rb_cref_struct = type { i64, i64, i64, %struct.rb_cref_struct*, %struct.rb_scope_visi_struct }
%struct.rb_scope_visi_struct = type { i8, [3 x i8] }
%union.anon.7 = type { i64 }
%struct.anon.10 = type { %struct.anon.11, i32, i32, i32, i32, i32, i32, i32, i64*, %struct.rb_iseq_param_keyword* }
%struct.anon.11 = type { i8, [3 x i8] }
%struct.rb_iseq_param_keyword = type { i32, i32, i32, i32, i64*, i64* }
%struct.iseq_catch_table = type opaque
%struct.iseq_compile_data = type opaque
%struct.rb_call_info_kw_arg_struct = type { i32, [1 x i64] }
%union.anon.9 = type { i32 }
%struct.rb_block_struct = type { i64, i64, i64*, %struct.rb_iseq_struct*, i64 }
%struct._opaque_pthread_t = type { i64, %struct.__darwin_pthread_handler_rec*, [8176 x i8] }
%struct.__darwin_pthread_handler_rec = type { void (i8*)*, i8*, %struct.__darwin_pthread_handler_rec* }
%struct.native_thread_data_struct = type { i8*, %struct.rb_thread_cond_struct }
%struct._opaque_pthread_mutex_t = type { i64, [56 x i8] }
%struct.rb_thread_cond_struct = type { %struct._opaque_pthread_cond_t }
%struct._opaque_pthread_cond_t = type { i64, [40 x i8] }
%struct.rb_unblock_callback = type { void (i8*)*, i8* }
%struct.rb_mutex_struct = type opaque
%struct.rb_vm_tag = type { i64, i64, [37 x i32], %struct.rb_vm_tag* }
%struct.rb_vm_protect_tag = type { %struct.rb_vm_protect_tag* }
%struct.st_table = type { %struct.st_hash_type*, i64, i64, %union.anon }
%struct.st_hash_type = type { i32 (...)*, i64 (...)* }
%union.anon = type { %struct.anon }
%struct.anon = type { %struct.st_table_entry**, %struct.st_table_entry*, %struct.st_table_entry* }
%struct.st_table_entry = type opaque
%struct.rb_thread_list_struct = type { %struct.rb_thread_list_struct*, %struct.rb_thread_struct* }
%struct.anon.12 = type { i64*, i64*, i64, [37 x i32] }
%struct.rb_hook_list_struct = type { %struct.rb_event_hook_struct*, i32, i32 }
%struct.rb_event_hook_struct = type opaque
%struct.rb_trace_arg_struct = type { i32, %struct.rb_thread_struct*, %struct.rb_control_frame_struct*, i64, i64, i64, i64, i32, i32, i64 }
%struct.rb_fiber_struct = type opaque
%struct.rb_ensure_list = type { %struct.rb_ensure_list*, %struct.rb_ensure_entry }
%struct.rb_ensure_entry = type { i64, i64 (...)*, i64 }
%struct.rb_control_frame_struct = type { i64*, i64*, %struct.rb_iseq_struct*, i64, i64, i64, i64*, %struct.rb_iseq_struct*, i64 }

; Function Attrs: nounwind
define void @function(%struct.rb_thread_struct* %th, %struct.rb_control_frame_struct* %cfp) #0 {
  %1 = alloca %struct.rb_thread_struct*, align 8
  %2 = alloca %struct.rb_control_frame_struct*, align 8
  %idx = alloca i64, align 8
  store %struct.rb_thread_struct* %th, %struct.rb_thread_struct** %1, align 8
  store %struct.rb_control_frame_struct* %cfp, %struct.rb_control_frame_struct** %2, align 8
  call void @vm_caller_setup_arg_block(%struct.rb_thread_struct* null, %struct.rb_control_frame_struct* null, %struct.rb_call_info_struct* null, i32 0)
  call void @vm_search_method(%struct.rb_call_info_struct* null, i64 0)
  call void @vm_pop_frame(%struct.rb_thread_struct* null)
  store i64 6, i64* %idx, align 8
  %3 = load %struct.rb_control_frame_struct** %2, align 8
  %4 = getelementptr inbounds %struct.rb_control_frame_struct* %3, i32 0, i32 6
  %5 = load i64** %4, align 8
  %6 = load i64* %idx, align 8
  %7 = sub i64 0, %6
  %8 = getelementptr inbounds i64* %5, i64 %7
  store i64 100, i64* %8, align 8
  ret void
}

declare void @vm_caller_setup_arg_block(%struct.rb_thread_struct*, %struct.rb_control_frame_struct*, %struct.rb_call_info_struct*, i32) #1

declare void @vm_search_method(%struct.rb_call_info_struct*, i64) #1

declare void @vm_pop_frame(%struct.rb_thread_struct*) #1

attributes #0 = { nounwind "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-realign-stack" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-realign-stack" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.ident = !{!0}

!0 = metadata !{metadata !"Apple LLVM version 6.1.0 (clang-602.0.53) (based on LLVM 3.6.0svn)"}
