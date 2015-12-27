static inline void
jit_dump_insn(jit_insn_t *insn)
{
	fprintf(stderr, "%03d %08p %-25s, len: %d, bb: %03p\n", insn->index, insn->pc, insn_name(insn->opecode), insn->len, insn->bb);
}
