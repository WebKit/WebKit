
#ifdef CAPSTONE_HAS_RISCV

#include <stdio.h>		// debug
#include <string.h>

#include "../../utils.h"

#include "RISCVMapping.h"
#include "RISCVInstPrinter.h"

#define GET_INSTRINFO_ENUM
#include "RISCVGenInstrInfo.inc"

#ifndef CAPSTONE_DIET
static const name_map reg_name_maps[] = {
	{ RISCV_REG_INVALID, NULL },

	{ RISCV_REG_X0, "zero" },
	{ RISCV_REG_X1, "ra" },
	{ RISCV_REG_X2, "sp" },
	{ RISCV_REG_X3, "gp" },
	{ RISCV_REG_X4, "tp" },
	{ RISCV_REG_X5, "t0" },
	{ RISCV_REG_X6, "t1" },
	{ RISCV_REG_X7, "t2" },
	{ RISCV_REG_X8, "s0" },
	{ RISCV_REG_X9, "s1" },
	{ RISCV_REG_X10, "a0" },
	{ RISCV_REG_X11, "a1" },
	{ RISCV_REG_X12, "a2" },
	{ RISCV_REG_X13, "a3" },
	{ RISCV_REG_X14, "a4" },
	{ RISCV_REG_X15, "a5" },
	{ RISCV_REG_X16, "a6" },
	{ RISCV_REG_X17, "a7" },
	{ RISCV_REG_X18, "s2" },
	{ RISCV_REG_X19, "s3" },
	{ RISCV_REG_X20, "s4" },
	{ RISCV_REG_X21, "s5" },
	{ RISCV_REG_X22, "s6" },
	{ RISCV_REG_X23, "s7" },
	{ RISCV_REG_X24, "s8" },
	{ RISCV_REG_X25, "s9" },
	{ RISCV_REG_X26, "s10" },
	{ RISCV_REG_X27, "s11" },
	{ RISCV_REG_X28, "t3" },
	{ RISCV_REG_X29, "t4" },
	{ RISCV_REG_X30, "t5" },
	{ RISCV_REG_X31, "t6" },

	{ RISCV_REG_F0_32, "ft0" },
	{ RISCV_REG_F0_64, "ft0" },
	{ RISCV_REG_F1_32, "ft1" },
	{ RISCV_REG_F1_64, "ft1" },
	{ RISCV_REG_F2_32, "ft2" },
	{ RISCV_REG_F2_64, "ft2" },
	{ RISCV_REG_F3_32, "ft3" },
	{ RISCV_REG_F3_64, "ft3" },
	{ RISCV_REG_F4_32, "ft4" },
	{ RISCV_REG_F4_64, "ft4" },
	{ RISCV_REG_F5_32, "ft5" },
	{ RISCV_REG_F5_64, "ft5" },
	{ RISCV_REG_F6_32, "ft6" },
	{ RISCV_REG_F6_64, "ft6" },
	{ RISCV_REG_F7_32, "ft7" },
	{ RISCV_REG_F7_64, "ft7" },
	{ RISCV_REG_F8_32, "fs0" },
	{ RISCV_REG_F8_64, "fs0" },
	{ RISCV_REG_F9_32, "fs1" },
	{ RISCV_REG_F9_64, "fs1" },
	{ RISCV_REG_F10_32, "fa0" },
	{ RISCV_REG_F10_64, "fa0" },
	{ RISCV_REG_F11_32, "fa1" },
	{ RISCV_REG_F11_64, "fa1" },
	{ RISCV_REG_F12_32, "fa2" },
	{ RISCV_REG_F12_64, "fa2" },
	{ RISCV_REG_F13_32, "fa3" },
	{ RISCV_REG_F13_64, "fa3" },
	{ RISCV_REG_F14_32, "fa4" },
	{ RISCV_REG_F14_64, "fa4" },
	{ RISCV_REG_F15_32, "fa5" },
	{ RISCV_REG_F15_64, "fa5" },
	{ RISCV_REG_F16_32, "fa6" },
	{ RISCV_REG_F16_64, "fa6" },
	{ RISCV_REG_F17_32, "fa7" },
	{ RISCV_REG_F17_64, "fa7" },
	{ RISCV_REG_F18_32, "fs2" },
	{ RISCV_REG_F18_64, "fs2" },
	{ RISCV_REG_F19_32, "fs3" },
	{ RISCV_REG_F19_64, "fs3" },
	{ RISCV_REG_F20_32, "fs4" },
	{ RISCV_REG_F20_64, "fs4" },
	{ RISCV_REG_F21_32, "fs5" },
	{ RISCV_REG_F21_64, "fs5" },
	{ RISCV_REG_F22_32, "fs6" },
	{ RISCV_REG_F22_64, "fs6" },
	{ RISCV_REG_F23_32, "fs7" },
	{ RISCV_REG_F23_64, "fs7" },
	{ RISCV_REG_F24_32, "fs8" },
	{ RISCV_REG_F24_64, "fs8" },
	{ RISCV_REG_F25_32, "fs9" },
	{ RISCV_REG_F25_64, "fs9" },
	{ RISCV_REG_F26_32, "fs10" },
	{ RISCV_REG_F26_64, "fs10" },
	{ RISCV_REG_F27_32, "fs11" },
	{ RISCV_REG_F27_64, "fs11" },
	{ RISCV_REG_F28_32, "ft8" },
	{ RISCV_REG_F28_64, "ft8" },
	{ RISCV_REG_F29_32, "ft9" },
	{ RISCV_REG_F29_64, "ft9" },
	{ RISCV_REG_F30_32, "ft10" },
	{ RISCV_REG_F30_64, "ft10" },
	{ RISCV_REG_F31_32, "ft11" },
	{ RISCV_REG_F31_64, "ft11" },
};
#endif

const char *RISCV_reg_name(csh handle, unsigned int reg)
{
#ifndef CAPSTONE_DIET
	if (reg >= RISCV_REG_ENDING)
		return NULL;
	return reg_name_maps[reg].name;
#else
	return NULL;
#endif
}

static const insn_map insns[] = {
	// dummy item
	{
	 0, 0,
#ifndef CAPSTONE_DIET
	 {0}, {0}, {0}, 0, 0
#endif
	 },

#include "RISCVMappingInsn.inc"
};

// given internal insn id, return public instruction info
void RISCV_get_insn_id(cs_struct * h, cs_insn * insn, unsigned int id) 
{
  	unsigned int i;

  	i = insn_find(insns, ARR_SIZE(insns), id, &h->insn_cache);
  	if (i != 0) {
    		insn->id = insns[i].mapid;

    		if (h->detail) {
#ifndef CAPSTONE_DIET
      			memcpy(insn->detail->regs_read,
      			insns[i].regs_use, sizeof(insns[i].regs_use));
      			insn->detail->regs_read_count = (uint8_t)count_positive(insns[i].regs_use);

      			memcpy(insn->detail->regs_write, insns[i].regs_mod, sizeof(insns[i].regs_mod));
      			insn->detail->regs_write_count = (uint8_t)count_positive(insns[i].regs_mod);

     			memcpy(insn->detail->groups, insns[i].groups, sizeof(insns[i].groups));
      			insn->detail->groups_count = (uint8_t)count_positive8(insns[i].groups);

      			if (insns[i].branch || insns[i].indirect_branch) {
        			// this insn also belongs to JUMP group. add JUMP group
        			insn->detail->groups[insn->detail->groups_count] = RISCV_GRP_JUMP;
        			insn->detail->groups_count++;
      			}
#endif
    		}
  	}
}

static const name_map insn_name_maps[] = {
  	{RISCV_INS_INVALID, NULL},

#include "RISCVGenInsnNameMaps.inc"
};

const char *RISCV_insn_name(csh handle, unsigned int id) 
{
#ifndef CAPSTONE_DIET
  	if (id >= RISCV_INS_ENDING)
    		return NULL;

  	return insn_name_maps[id].name;
#else
  	return NULL;
#endif
}

#ifndef CAPSTONE_DIET
static const name_map group_name_maps[] = {
  	// generic groups
  	{ RISCV_GRP_INVALID,    NULL },
  	{ RISCV_GRP_JUMP,       "jump" },
  	{ RISCV_GRP_CALL,       "call" },
  	{ RISCV_GRP_RET,        "ret" },
  	{ RISCV_GRP_INT,        "int" },
  	{ RISCV_GRP_IRET,       "iret" },
  	{ RISCV_GRP_PRIVILEGE,  "privileged" },
  	{ RISCV_GRP_BRANCH_RELATIVE, "branch_relative" },
  
  	// architecture specific
  	{ RISCV_GRP_ISRV32,     "isrv32" },
  	{ RISCV_GRP_ISRV64,     "isrv64" },
  	{ RISCV_GRP_HASSTDEXTA, "hasStdExtA" },
  	{ RISCV_GRP_HASSTDEXTC, "hasStdExtC" },
  	{ RISCV_GRP_HASSTDEXTD, "hasStdExtD" },
  	{ RISCV_GRP_HASSTDEXTF, "hasStdExtF" },
  	{ RISCV_GRP_HASSTDEXTM, "hasStdExtM" },
  
  	/*
  	{ RISCV_GRP_ISRVA,      "isrva" },
  	{ RISCV_GRP_ISRVC,      "isrvc" },
  	{ RISCV_GRP_ISRVD,      "isrvd" },
  	{ RISCV_GRP_ISRVCD,     "isrvcd" },
  	{ RISCV_GRP_ISRVF,      "isrvf" },
  	{ RISCV_GRP_ISRV32C,    "isrv32c" },
  	{ RISCV_GRP_ISRV32CF,   "isrv32cf" },
  	{ RISCV_GRP_ISRVM,      "isrvm" },
  	{ RISCV_GRP_ISRV64A,    "isrv64a" },
  	{ RISCV_GRP_ISRV64C,    "isrv64c" },
  	{ RISCV_GRP_ISRV64D,    "isrv64d" },
  	{ RISCV_GRP_ISRV64F,    "isrv64f" },
  	{ RISCV_GRP_ISRV64M,    "isrv64m" }
  	*/
  	{ RISCV_GRP_ENDING,     NULL }
};
#endif

const char *RISCV_group_name(csh handle, unsigned int id)
{
#ifndef CAPSTONE_DIET
	// verify group id
	if (id >= RISCV_GRP_ENDING || 
            (id > RISCV_GRP_BRANCH_RELATIVE && id < RISCV_GRP_ISRV32))
		return NULL;
	return id2name(group_name_maps, ARR_SIZE(group_name_maps), id);
#else
	return NULL;
#endif
}

// map instruction name to public instruction ID
riscv_reg RISCV_map_insn(const char *name)
{
	// handle special alias first
	unsigned int i;

	// NOTE: skip first NULL name in insn_name_maps
	i = name2id(&insn_name_maps[1], ARR_SIZE(insn_name_maps) - 1, name);

	return (i != -1) ? i : RISCV_REG_INVALID;
}

// map internal raw register to 'public' register
riscv_reg RISCV_map_register(unsigned int r)
{
	static const unsigned int map[] = { 0,
		RISCV_REG_X0,
		RISCV_REG_X1,
		RISCV_REG_X2,
		RISCV_REG_X3,
		RISCV_REG_X4,
		RISCV_REG_X5,
		RISCV_REG_X6,
		RISCV_REG_X7,
		RISCV_REG_X8,
		RISCV_REG_X9,
		RISCV_REG_X10,
		RISCV_REG_X11,
		RISCV_REG_X12,
		RISCV_REG_X13,
		RISCV_REG_X14,
		RISCV_REG_X15,
		RISCV_REG_X16,
		RISCV_REG_X17,
		RISCV_REG_X18,
		RISCV_REG_X19,
		RISCV_REG_X20,
		RISCV_REG_X21,
		RISCV_REG_X22,
		RISCV_REG_X23,
		RISCV_REG_X24,
		RISCV_REG_X25,
		RISCV_REG_X26,
		RISCV_REG_X27,
		RISCV_REG_X28,
		RISCV_REG_X29,
		RISCV_REG_X30,
		RISCV_REG_X31,

		RISCV_REG_F0_32,
		RISCV_REG_F0_64,
		RISCV_REG_F1_32,
		RISCV_REG_F1_64,
		RISCV_REG_F2_32,
		RISCV_REG_F2_64,
		RISCV_REG_F3_32,
		RISCV_REG_F3_64,
		RISCV_REG_F4_32,
		RISCV_REG_F4_64,
		RISCV_REG_F5_32,
		RISCV_REG_F5_64,
		RISCV_REG_F6_32,
		RISCV_REG_F6_64,
		RISCV_REG_F7_32,
		RISCV_REG_F7_64,
		RISCV_REG_F8_32,
		RISCV_REG_F8_64,
		RISCV_REG_F9_32,
		RISCV_REG_F9_64,
		RISCV_REG_F10_32,
		RISCV_REG_F10_64,
		RISCV_REG_F11_32,
		RISCV_REG_F11_64,
		RISCV_REG_F12_32,
		RISCV_REG_F12_64,
		RISCV_REG_F13_32,
		RISCV_REG_F13_64,
		RISCV_REG_F14_32,
		RISCV_REG_F14_64,
		RISCV_REG_F15_32,
		RISCV_REG_F15_64,
		RISCV_REG_F16_32,
		RISCV_REG_F16_64,
		RISCV_REG_F17_32,
		RISCV_REG_F17_64,
		RISCV_REG_F18_32,
		RISCV_REG_F18_64,
		RISCV_REG_F19_32,
		RISCV_REG_F19_64,
		RISCV_REG_F20_32,
		RISCV_REG_F20_64,
		RISCV_REG_F21_32,
		RISCV_REG_F21_64,
		RISCV_REG_F22_32,
		RISCV_REG_F22_64,
		RISCV_REG_F23_32,
		RISCV_REG_F23_64,
		RISCV_REG_F24_32,
		RISCV_REG_F24_64,
		RISCV_REG_F25_32,
		RISCV_REG_F25_64,
		RISCV_REG_F26_32,
		RISCV_REG_F26_64,
		RISCV_REG_F27_32,
		RISCV_REG_F27_64,
		RISCV_REG_F28_32,
		RISCV_REG_F28_64,
		RISCV_REG_F29_32,
		RISCV_REG_F29_64,
		RISCV_REG_F30_32,
		RISCV_REG_F30_64,
		RISCV_REG_F31_32,
		RISCV_REG_F31_64,
	};

	if (r < ARR_SIZE(map))
		return map[r];

	// cannot find this register
	return 0;
}

#endif
