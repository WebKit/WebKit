/* Capstone Disassembly Engine */
/* BPF Backend by david942j <david942j@gmail.com>, 2019 */

#ifndef CS_BPFMAPPING_H
#define CS_BPFMAPPING_H

#include <capstone/capstone.h>

#include "../../cs_priv.h"

#define EBPF_MODE(ud) (((cs_struct*)ud)->mode & CS_MODE_BPF_EXTENDED)

const char *BPF_group_name(csh handle, unsigned int id);
const char *BPF_insn_name(csh handle, unsigned int id);
const char *BPF_reg_name(csh handle, unsigned int reg);
void BPF_get_insn_id(cs_struct *h, cs_insn *insn, unsigned int id);
void BPF_reg_access(const cs_insn *insn,
		cs_regs regs_read, uint8_t *regs_read_count,
		cs_regs regs_write, uint8_t *regs_write_count);

#endif
