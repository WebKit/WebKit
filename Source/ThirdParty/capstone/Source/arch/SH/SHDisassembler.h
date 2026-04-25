/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh, 2018 */

#ifndef CS_SHDISASSEMBLER_H
#define CS_SHDISASSEMBLER_H

#include "../../MCInst.h"

typedef struct sh_info {
	cs_sh op;
} sh_info;

bool SH_getInstruction(csh ud, const uint8_t *code, size_t code_len,
		MCInst *instr, uint16_t *size, uint64_t address, void *info);

void SH_reg_access(const cs_insn *insn,
		   cs_regs regs_read, uint8_t *regs_read_count,
		   cs_regs regs_write, uint8_t *regs_write_count);
#endif
