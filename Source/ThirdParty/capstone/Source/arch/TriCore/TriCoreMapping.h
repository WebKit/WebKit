/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2014 */

#ifndef CS_TRICORE_MAP_H
#define CS_TRICORE_MAP_H

#include <capstone/capstone.h>

// given internal insn id, return public instruction info
void TriCore_get_insn_id(cs_struct *h, cs_insn *insn, unsigned int id);

const char *TriCore_insn_name(csh handle, unsigned int id);

const char *TriCore_group_name(csh handle, unsigned int id);

void TriCore_reg_access(const cs_insn *insn, cs_regs regs_read,
			uint8_t *regs_read_count, cs_regs regs_write,
			uint8_t *regs_write_count);

void TriCore_set_access(MCInst *MI);

void TriCore_set_instr_map_data(MCInst *MI);

bool TriCore_getInstruction(csh handle, const uint8_t *Bytes, size_t ByteLen,
			    MCInst *MI, uint16_t *Size, uint64_t Address,
			    void *Info);

void TriCore_printInst(MCInst *MI, SStream *O, void *Info);

const char *TriCore_getRegisterName(csh handle, unsigned int RegNo);

#endif
