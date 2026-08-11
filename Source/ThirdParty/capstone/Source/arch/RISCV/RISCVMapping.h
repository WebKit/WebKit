
#ifndef CS_RISCV_MAP_H
#define CS_RISCV_MAP_H

#include "../../include/capstone/capstone.h"

// given internal insn id, return public instruction info
void RISCV_get_insn_id(cs_struct * h, cs_insn * insn, unsigned int id);

const char *RISCV_insn_name(csh handle, unsigned int id);

const char *RISCV_group_name(csh handle, unsigned int id);

const char *RISCV_reg_name(csh handle, unsigned int reg);

// map instruction name to instruction ID
riscv_reg RISCV_map_insn(const char *name);

// map internal raw register to 'public' register
riscv_reg RISCV_map_register(unsigned int r);

#endif
