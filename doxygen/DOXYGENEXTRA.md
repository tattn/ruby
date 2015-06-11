@mainpage

CRuby のコードリーディング用の備忘録です。

> 明日の自分は他人だと思え by 梅棹忠夫



## ディレクトリ構成

~~~
Root
 ┣ Ruby の C ファイル群
 ┣ Doxyfile				- Doxygen 設定ファイル
 ┣ DOXYGENEXTRA.md		- このファイル
 ┗ doxygen
     ┣ compile.rb		- 健忘録生成スクリプト
     ┗ other			- 見た目調整ファイル
~~~



## 準備

Mac 以外は読み替えて下さい。

    $ ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
    $ brew install git doxygen graphviz autoconf
    $ cd 任意の開発用ディレクトリ
    $ git clone https://github.com/tattn/ruby.git
    $ cd ruby
    $ git checkout codereading
    $ autoconf
    $ ./configure
    $ make
    $ ruby doxygen/compile.rb -s



## 更新

    $ ruby doxygen/compile.rb


## 機能別クイックリンク

### VM 系

* VM の実行メインループ (vm_exec.c, vm_exec.h)
* VM のメインループ呼び出しとエラーハンドリング (vm.c[vm_exec])


---

Copyright (C) 2015 Tatsuya Tanaka

created at: 2015-06-10 18:28:52 +0900

