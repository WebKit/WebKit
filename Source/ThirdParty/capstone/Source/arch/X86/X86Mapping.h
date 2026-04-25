/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2019 */

#ifndef CS_X86_MAP_H
#define CS_X86_MAP_H

#include "capstone/capstone.h"
#include "../../cs_priv.h"

// map instruction to its characteristics
typedef struct insn_map_x86 {
	unsigned short id;
	unsigned short mapid;
	unsigned char is64bit;
#ifndef CAPSTONE_DIET
	uint16_t regs_use[12]; // list of implicit registers used by this instruction
	uint16_t regs_mod[20]; // list of implicit registers modified by this instruction
	unsigned char groups[8]; // list of group this instruction belong to
	bool branch;	// branch instruction?
	bool indirect_branch;	// indirect branch instruction?
#endif
} insn_map_x86;

extern const insn_map_x86 insns[];

// map sib_base to x86_reg
x86_reg x86_map_sib_base(int r);

// map sib_index to x86_reg
x86_reg x86_map_sib_index(int r);

// map seg_override to x86_reg
x86_reg x86_map_segment(int r);

// return name of regiser in friendly string
const char *X86_reg_name(csh handle, unsigned int reg);

// given internal insn id, return public instruction info
void X86_get_insn_id(cs_struct *h, cs_insn *insn, unsigned int id);

// return insn name, given insn id
const char *X86_insn_name(csh handle, unsigned int id);

// return group name, given group id
const char *X86_group_name(csh handle, unsigned int id);

// return register of given instruction id
// return 0 if not found
// this is to handle instructions embedding accumulate registers into AsmStrs[]
x86_reg X86_insn_reg_intel(unsigned int id, enum cs_ac_type *access);
x86_reg X86_insn_reg_att(unsigned int id, enum cs_ac_type *access);
bool X86_insn_reg_intel2(unsigned int id, x86_reg *reg1, enum cs_ac_type *access1, x86_reg *reg2, enum cs_ac_type *access2);
bool X86_insn_reg_att2(unsigned int id, x86_reg *reg1, enum cs_ac_type *access1, x86_reg *reg2, enum cs_ac_type *access2);

extern const uint64_t arch_masks[9];

// handle LOCK/REP/REPNE prefixes
// return True if we patch mnemonic, like in MULPD case
bool X86_lockrep(MCInst *MI, SStream *O);

// map registers to sizes
extern const uint8_t regsize_map_32[];
extern const uint8_t regsize_map_64[];

void op_addReg(MCInst *MI, int reg);
void op_addImm(MCInst *MI, int v);

void op_addAvxBroadcast(MCInst *MI, x86_avx_bcast v);

void op_addXopCC(MCInst *MI, int v);
void op_addSseCC(MCInst *MI, int v);
void op_addAvxCC(MCInst *MI, int v);

void op_addAvxZeroOpmask(MCInst *MI);

void op_addAvxSae(MCInst *MI);

void op_addAvxRoundingMode(MCInst *MI, int v);

// given internal insn id, return operand access info
const uint8_t *X86_get_op_access(cs_struct *h, unsigned int id, uint64_t *eflags);

void X86_reg_access(const cs_insn *insn,
		cs_regs regs_read, uint8_t *regs_read_count,
		cs_regs regs_write, uint8_t *regs_write_count);

// given the instruction id, return the size of its immediate operand (or 0)
uint8_t X86_immediate_size(unsigned int id, uint8_t *enc_size);

unsigned short X86_register_map(unsigned short id);

unsigned int find_insn(unsigned int id);

void X86_postprinter(csh handle, cs_insn *insn, char *mnem, MCInst *mci);

#endif
