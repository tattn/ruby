@page quick_links クイックリンク

## CFG 別クイックリンク

* main() [main.c] - メイン関数
	* ruby_sysinit() [ruby.c] - 引数の初期化
	* RUBY_INIT_STACK [ruby.h]
	* ruby_init()



## 機能別クイックリンク

### VM 系

* VM の実行メインループ (vm_exec.c, vm_exec.h)
* VM のメインループ呼び出しとエラーハンドリング (vm.c[vm_exec()])


### 字句解析・構文解析系

* ファイル名や行数を保存して、コンパイル処理を呼び出す (ext/ripper/ripper.c[yycompile()])


---

Copyright (C) 2015 Tatsuya Tanaka

created at: 2015-06-10 19:51:33 +0900

