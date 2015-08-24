static inline void
jit_dump_insn(jit_insn_t *insn)
{
	JIT_DEBUG_LOG2("pc: %08p, op: %-25s(%d), idx: %3d, len: %d, bb: %03p", insn->pc, insn_name(insn->opecode), insn->opecode, insn->index, insn->len, insn->bb);
}
