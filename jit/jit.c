#include "jit.h"
#include <libgccjit.h>

static void
create_code (gcc_jit_context *ctxt)
{
  /* Let's try to inject the equivalent of:
     void
     greet (const char *name)
     {
        printf ("hello %s\n", name);
     }
  */
  gcc_jit_type *void_type =
    gcc_jit_context_get_type (ctxt, GCC_JIT_TYPE_VOID);
  gcc_jit_type *const_char_ptr_type =
    gcc_jit_context_get_type (ctxt, GCC_JIT_TYPE_CONST_CHAR_PTR);
  gcc_jit_param *param_name =
    gcc_jit_context_new_param (ctxt, NULL, const_char_ptr_type, "name");
  gcc_jit_function *func =
    gcc_jit_context_new_function (ctxt, NULL,
                                  GCC_JIT_FUNCTION_EXPORTED,
                                  void_type,
                                  "greet",
                                  1, &param_name,
                                  0);

  gcc_jit_param *param_format =
    gcc_jit_context_new_param (ctxt, NULL, const_char_ptr_type, "format");
  gcc_jit_function *printf_func =
    gcc_jit_context_new_function (ctxt, NULL,
				  GCC_JIT_FUNCTION_IMPORTED,
				  gcc_jit_context_get_type (
				     ctxt, GCC_JIT_TYPE_INT),
				  "printf",
				  1, &param_format,
				  1);
  gcc_jit_rvalue *args[2];
  args[0] = gcc_jit_context_new_string_literal (ctxt, "hello %s\n");
  args[1] = gcc_jit_param_as_rvalue (param_name);

  gcc_jit_block *block = gcc_jit_function_new_block (func, NULL);

  gcc_jit_block_add_eval (
    block, NULL,
    gcc_jit_context_new_call (ctxt,
                              NULL,
                              printf_func,
                              2, args));
  gcc_jit_block_end_with_void_return (block, NULL);
}

static gcc_jit_context *ctxt;
static gcc_jit_result *result; // sorry

void jit_init(void)
{
	/* Get a "context" object for working with the library.  */
	ctxt = gcc_jit_context_acquire();
	if (!ctxt) {
		fprintf(stderr, "Failed to create context for JIT...");
		exit(1);
	}

	/* Set some options on the context.
	   Let's see the code being generated, in assembler form.  */
	gcc_jit_context_set_bool_option (
			ctxt,
			GCC_JIT_BOOL_OPTION_DUMP_GENERATED_CODE,
			0);
}

void jit_release(void)
{
	gcc_jit_context_release(ctxt);
	gcc_jit_result_release(result);
}

typedef void (*jit_func_type) (const char *);

void* jit_compile_context(gcc_jit_context* ctx)
{
	/* Compile the code.  */
	result = gcc_jit_context_compile(ctx);
	if (!result) {
		fprintf(stderr, "Failed to compile context of JIT...");
		exit(1);
	}

	/* Extract the generated code from "result".  */
	jit_func_type greet =
		(jit_func_type)gcc_jit_result_get_code(result, "greet");
	if (!greet) {
		fprintf(stderr, "Failed to get a compiled function...");
		exit(1);
	}

	return greet;
}

void jit_test(void)
{
  jit_init();

  /* Populate the context.  */
  create_code(ctxt);

  jit_func_type greet = jit_compile_context(ctxt);

  /* Now call the generated function: */
  greet("world");
  fflush(stdout);

  jit_release();
}

/**TODO:
 *
 * C言語用ハッシュマップ/リストライブラリを探す
 * トレースしてダンプする
 * puts 1 + 2をトレースしてコンパイルして実行
 */
