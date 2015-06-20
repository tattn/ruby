@page microscope Ruby のしくみ

Ruby のしくみ



### VM 関係

#### reg_pc, REG_PC()

Direct Threaded では vm.inc に記述されている命令へのラベルのポインタが入っている。

ラベルのポインタは gcc の拡張文法で

~~~
    void *ptr;

    label: ...

    ptr = &&label;

    ...

    goto *ptr;
~~~

のようにラベルのポインタを利用した goto 文によるジャンプが出来る。(https://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html)
- - -

#### rb_iseq_t

命令列が格納されている構造体。

特に、rb_iseq_t::iseq_encoded は重要で、この中に reg_pc の中身が入っている。

* iseq_encoded の参照： vm_set_eval_stack()
* cfp->pc    への代入： vm_push_frame()
* reg_pc     への代入： vm_exec_core()

iseq_encoded は iseq_set_sequence() [compile.c] で代入されているが、この時は

* iseq_encoded[i] に命令のID
* iseq_encoded[i + 1] にオペランド1
* iseq_encoded[i + x] にオペランドx

が入っている。その後、rb_iseq_translate_threaded_code() [compile.c] で

* iseq_encoded[i] に命令へのラベルのポインタ

を上書きで代入しなおしている。
- - -

#### insn_len()

１命令の大きさを取得できる関数。中身は insn_len_info 配列の値を返している。

insns_info.inc.tmpl で自動生成されている。
- - -





---

Copyright (C) 2015 Tatsuya Tanaka

created at: 2015-06-21 16:45:27 +0900

