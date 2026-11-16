/* Capstone Disassembly Engine */
/* BPF Backend by david942j <david942j@gmail.com>, 2019 */

#ifndef CS_BPF_DISASSEMBLER_H
#define CS_BPF_DISASSEMBLER_H

#include "../../MCInst.h"

typedef struct bpf_internal {
	uint16_t op;
	uint64_t k;
	/* for cBPF */
	uint8_t jt;
	uint8_t jf;
	/* for eBPF */
	uint8_t dst;
	uint8_t src;
	uint16_t offset;

	/* length of this bpf instruction */
	uint8_t insn_size;
} bpf_internal;

bool BPF_getInstruction(csh ud, const uint8_t *code, size_t code_len,
		MCInst *instr, uint16_t *size, uint64_t address, void *info);

#endif
