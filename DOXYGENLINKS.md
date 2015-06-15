@page quick_links クイックリンク

## CFG 別クイックリンク

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
	* ruby_options() [eval.c] - コマンドラインオプションの処理と、ソースのコンパイル
		* ruby_init_stack() [thread_pthread.c]
		* ruby_process_options() [ruby.c]



## 機能別クイックリンク

### VM 系

* VM の実行メインループ (vm_exec.c, vm_exec.h)
* VM のメインループ呼び出しとエラーハンドリング (vm.c[vm_exec()])


### 字句解析・構文解析系

* ファイル名や行数を保存して、コンパイル処理を呼び出す (ext/ripper/ripper.c[yycompile()])


---

Copyright (C) 2015 Tatsuya Tanaka

created at: 2015-06-15 14:28:26 +0900

