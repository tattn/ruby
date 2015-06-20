@page quick_links クイックリンク

## 呼び出し順クイックリンク

* main() [main.c] - メイン関数
	* ruby_sysinit() [ruby.c] - 引数の初期化
	* RUBY_INIT_STACK [ruby.h]
	* ruby_init() [eval.c]
		* ruby_setup() [eval.c] - Ruby 全般の初期化
			* ruby_init_stack() [thread_pthread.c]
			* Init_BareVM() [vm.c] - vm や thread のメモリ確保など
			* Init_heap() [gc.c] - ObjectSpace の初期化
			* Init_vm_objects() [vm.c] - vm の内部オブジェクトの確保
			* Init_frozen_strings() [eval.c] - frozen string の初期化
			* rb_call_inits() [eval.c] - 組み込みクラスのイニシャライザ呼び出し
			* ruby_prog_init() [ruby.c] - 組み込み変数の作成
	* ruby_options() [eval.c] - コマンドラインオプションの処理と、ソースのパース
		* ruby_init_stack() [thread_pthread.c]
		* ruby_process_options() [ruby.c] - スクリプト名の設定とパース
			* cmdline_options_init() [ruby.c] - コマンドライン引数の初期化
			* process_options() [ruby.c] - 実際のコマンドライン引数の処理と抽象木の作成
				* e オプションが付いている時
					* rb_parser_compile_string()
				* e オプションが付いていない時
					* load_file() [ruby.c]
						* rb_ensure() [eval.c]
							* load_file_internal() [ruby.c] - OS ごとに合わせたファイル読み込み
								* rb_protect() [eval.c] - 第１引数の関数実行中にエラーが起きた場合に復帰できる
									* load_file_internal2() [ruby.c] - ファイル読み込みとパース (エンコード関係が複雑すぎる)
										* rb_parser_compile_string_path() [ripper.c]
											* parser_compile_string() [ripper.c] - パースして AST を返す。
				* rb_iseq_new_main() [iseq.c]
					* rb_iseq_new_with_opt() [iseq.c]
						* prepare_iseq_build() [iseq.c] - iseq の初期化
						* rb_iseq_compile_node() [compile.c] - ノードの種類と self に合わせてコンパイルの準備を行う
							* iseq_compile_each() [compile.c]
							* iseq_setup() [compile.c] - node を rb_iseq_t に変換、その際に最適化を適用
								* iseq_optimize() [compile.c] - 最適化の適用
									* iseq_peephole_optimize()
									* iseq_specialized_instruction()
									* insn_operands_unification()
								* iseq_insns_unification() [compile.c]
								* iseq_set_sequence_stackcaching() [compile.c]
								* iseq_set_sequence() [compile.c] - node のリストを rb_iseq_t::iseq_encoded に変換
								* iseq_set_exception_table() [compile.c] - 例外処理を表引き法で行うための設定
								* iseq_set_optargs_table() [compile.c] - 名前付き引数のために引数テーブルにインデックスを設定
								* rb_iseq_translate_threaded_code() [compile.c] - iseq_encoded に格納された命令 ID を命令へのラベルのポインタに変換


	* ruby_run_node() [main.c]
		* ruby_executable_node() [eval.c] - パースがうまく行ったかを調べる
			* ruby_exec_node() [eval.c]
				* ruby_exec_internal() [eval.c] - エラー処理と実行
					* rb_iseq_eval_main() [vm.c] - スタックフレームのセットアップ
					* vm_exec() [vm.c] - VM のループとエラーハンドリング
						* vm_exec_core() [vm_exec.c]  - VM の実行!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!




## 機能別クイックリンク


### Makefile 系

* 自動生成用 Makefile [common.mk, Makefile.in]
* 環境チェックと自動設定 [configure.in]


### VM 系

* VM の実行メインループ [vm_exec.c, vm_exec.h]
* VM のメインループ呼び出しとエラーハンドリング [vm_exec() in vm.c]
* バイトコードの命令の定義 [insns.def]
* 実際に実行するバイトコードの命令 [vm.inc]



### 字句解析・構文解析系

* ファイル名や行数を保存して、コンパイル処理を呼び出す [yycompile() in ext/ripper/ripper.c]


---

Copyright (C) 2015 Tatsuya Tanaka

created at: 2015-06-21 16:45:27 +0900

