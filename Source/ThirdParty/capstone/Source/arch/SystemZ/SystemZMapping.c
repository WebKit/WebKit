/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2015 */

#ifdef CAPSTONE_HAS_SYSZ

#include <stdio.h>	// debug
#include <string.h>

#include "../../utils.h"

#include "SystemZMapping.h"

#define GET_INSTRINFO_ENUM
#include "SystemZGenInstrInfo.inc"

#ifndef CAPSTONE_DIET
static const name_map reg_name_maps[] = {
	{ SYSZ_REG_INVALID, NULL },

	{ SYSZ_REG_0, "0" },
	{ SYSZ_REG_1, "1" },
	{ SYSZ_REG_2, "2" },
	{ SYSZ_REG_3, "3" },
	{ SYSZ_REG_4, "4" },
	{ SYSZ_REG_5, "5" },
	{ SYSZ_REG_6, "6" },
	{ SYSZ_REG_7, "7" },
	{ SYSZ_REG_8, "8" },
	{ SYSZ_REG_9, "9" },
	{ SYSZ_REG_10, "10" },
	{ SYSZ_REG_11, "11" },
	{ SYSZ_REG_12, "12" },
	{ SYSZ_REG_13, "13" },
	{ SYSZ_REG_14, "14" },
	{ SYSZ_REG_15, "15" },
	{ SYSZ_REG_CC, "cc"},
	{ SYSZ_REG_F0, "f0" },
	{ SYSZ_REG_F1, "f1" },
	{ SYSZ_REG_F2, "f2" },
	{ SYSZ_REG_F3, "f3" },
	{ SYSZ_REG_F4, "f4" },
	{ SYSZ_REG_F5, "f5" },
	{ SYSZ_REG_F6, "f6" },
	{ SYSZ_REG_F7, "f7" },
	{ SYSZ_REG_F8, "f8" },
	{ SYSZ_REG_F9, "f9" },
	{ SYSZ_REG_F10, "f10" },
	{ SYSZ_REG_F11, "f11" },
	{ SYSZ_REG_F12, "f12" },
	{ SYSZ_REG_F13, "f13" },
	{ SYSZ_REG_F14, "f14" },
	{ SYSZ_REG_F15, "f15" },
	{ SYSZ_REG_R0L, "r0l" },
	{ SYSZ_REG_A0, "a0" },
	{ SYSZ_REG_A1, "a1" },
	{ SYSZ_REG_A2, "a2" },
	{ SYSZ_REG_A3, "a3" },
	{ SYSZ_REG_A4, "a4" },
	{ SYSZ_REG_A5, "a5" },
	{ SYSZ_REG_A6, "a6" },
	{ SYSZ_REG_A7, "a7" },
	{ SYSZ_REG_A8, "a8" },
	{ SYSZ_REG_A9, "a9" },
	{ SYSZ_REG_A10, "a10" },
	{ SYSZ_REG_A11, "a11" },
	{ SYSZ_REG_A12, "a12" },
	{ SYSZ_REG_A13, "a13" },
	{ SYSZ_REG_A14, "a14" },
	{ SYSZ_REG_A15, "a15" },
	{ SYSZ_REG_C0, "c0" },
	{ SYSZ_REG_C1, "c1" },
	{ SYSZ_REG_C2, "c2" },
	{ SYSZ_REG_C3, "c3" },
	{ SYSZ_REG_C4, "c4" },
	{ SYSZ_REG_C5, "c5" },
	{ SYSZ_REG_C6, "c6" },
	{ SYSZ_REG_C7, "c7" },
	{ SYSZ_REG_C8, "c8" },
	{ SYSZ_REG_C9, "c9" },
	{ SYSZ_REG_C10, "c10" },
	{ SYSZ_REG_C11, "c11" },
	{ SYSZ_REG_C12, "c12" },
	{ SYSZ_REG_C13, "c13" },
	{ SYSZ_REG_C14, "c14" },
	{ SYSZ_REG_C15, "c15" },
	{ SYSZ_REG_V0, "v0" },
	{ SYSZ_REG_V1, "v1" },
	{ SYSZ_REG_V2, "v2" },
	{ SYSZ_REG_V3, "v3" },
	{ SYSZ_REG_V4, "v4" },
	{ SYSZ_REG_V5, "v5" },
	{ SYSZ_REG_V6, "v6" },
	{ SYSZ_REG_V7, "v7" },
	{ SYSZ_REG_V8, "v8" },
	{ SYSZ_REG_V9, "v9" },
	{ SYSZ_REG_V10, "v10" },
	{ SYSZ_REG_V11, "v11" },
	{ SYSZ_REG_V12, "v12" },
	{ SYSZ_REG_V13, "v13" },
	{ SYSZ_REG_V14, "v14" },
	{ SYSZ_REG_V15, "v15" },
	{ SYSZ_REG_V16, "v16" },
	{ SYSZ_REG_V17, "v17" },
	{ SYSZ_REG_V18, "v18" },
	{ SYSZ_REG_V19, "v19" },
	{ SYSZ_REG_V20, "v20" },
	{ SYSZ_REG_V21, "v21" },
	{ SYSZ_REG_V22, "v22" },
	{ SYSZ_REG_V23, "v23" },
	{ SYSZ_REG_V24, "v24" },
	{ SYSZ_REG_V25, "v25" },
	{ SYSZ_REG_V26, "v26" },
	{ SYSZ_REG_V27, "v27" },
	{ SYSZ_REG_V28, "v28" },
	{ SYSZ_REG_V29, "v29" },
	{ SYSZ_REG_V30, "v30" },
	{ SYSZ_REG_V31, "v31" },
	{ SYSZ_REG_F16, "f16" },
	{ SYSZ_REG_F17, "f17" },
	{ SYSZ_REG_F18, "f18" },
	{ SYSZ_REG_F19, "f19" },
	{ SYSZ_REG_F20, "f20" },
	{ SYSZ_REG_F21, "f21" },
	{ SYSZ_REG_F22, "f22" },
	{ SYSZ_REG_F23, "f23" },
	{ SYSZ_REG_F24, "f24" },
	{ SYSZ_REG_F25, "f25" },
	{ SYSZ_REG_F26, "f26" },
	{ SYSZ_REG_F27, "f27" },
	{ SYSZ_REG_F28, "f28" },
	{ SYSZ_REG_F29, "f29" },
	{ SYSZ_REG_F30, "f30" },
	{ SYSZ_REG_F31, "f31" },
	{ SYSZ_REG_F0Q, "f0q" },
	{ SYSZ_REG_F4Q, "f4q" },
};
#endif

const char *SystemZ_reg_name(csh handle, unsigned int reg)
{
#ifndef CAPSTONE_DIET
	if (reg >= ARR_SIZE(reg_name_maps))
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
		{ 0 }, { 0 }, { 0 }, 0, 0
#endif
	},

#include "SystemZMappingInsn.inc"
};

// given internal insn id, return public instruction info
void SystemZ_get_insn_id(cs_struct *h, cs_insn *insn, unsigned int id)
{
	unsigned short i;

	i = insn_find(insns, ARR_SIZE(insns), id, &h->insn_cache);
	if (i != 0) {
		insn->id = insns[i].mapid;

		if (h->detail) {
#ifndef CAPSTONE_DIET
			memcpy(insn->detail->regs_read, insns[i].regs_use, sizeof(insns[i].regs_use));
			insn->detail->regs_read_count = (uint8_t)count_positive(insns[i].regs_use);

			memcpy(insn->detail->regs_write, insns[i].regs_mod, sizeof(insns[i].regs_mod));
			insn->detail->regs_write_count = (uint8_t)count_positive(insns[i].regs_mod);

			memcpy(insn->detail->groups, insns[i].groups, sizeof(insns[i].groups));
			insn->detail->groups_count = (uint8_t)count_positive8(insns[i].groups);

			if (insns[i].branch || insns[i].indirect_branch) {
				// this insn also belongs to JUMP group. add JUMP group
				insn->detail->groups[insn->detail->groups_count] = SYSZ_GRP_JUMP;
				insn->detail->groups_count++;
			}
#endif
		}
	}
}

#ifndef CAPSTONE_DIET
static const name_map insn_name_maps[] = {
	{ SYSZ_INS_INVALID, NULL },

#include "SystemZGenInsnNameMaps.inc"
};

// special alias insn
static const name_map alias_insn_names[] = {
	{ 0, NULL }
};
#endif

const char *SystemZ_insn_name(csh handle, unsigned int id)
{
#ifndef CAPSTONE_DIET
	unsigned int i;

	if (id >= SYSZ_INS_ENDING)
		return NULL;

	// handle special alias first
	for (i = 0; i < ARR_SIZE(alias_insn_names); i++) {
		if (alias_insn_names[i].id == id)
			return alias_insn_names[i].name;
	}

	return insn_name_maps[id].name;
#else
	return NULL;
#endif
}

#ifndef CAPSTONE_DIET
static const name_map group_name_maps[] = {
	// generic groups
	{ SYSZ_GRP_INVALID, NULL },
	{ SYSZ_GRP_JUMP, "jump" },

	// architecture-specific groups
	{ SYSZ_GRP_DFPPACKEDCONVERSION, "dfppackedconversion" },
	{ SYSZ_GRP_DFPZONEDCONVERSION, "dfpzonedconversion" },
	{ SYSZ_GRP_DISTINCTOPS, "distinctops" },
	{ SYSZ_GRP_ENHANCEDDAT2, "enhanceddat2" },
	{ SYSZ_GRP_EXECUTIONHINT, "executionhint" },
	{ SYSZ_GRP_FPEXTENSION, "fpextension" },
	{ SYSZ_GRP_GUARDEDSTORAGE, "guardedstorage" },
	{ SYSZ_GRP_HIGHWORD, "highword" },
	{ SYSZ_GRP_INSERTREFERENCEBITSMULTIPLE, "insertreferencebitsmultiple" },
	{ SYSZ_GRP_INTERLOCKEDACCESS1, "interlockedaccess1" },
	{ SYSZ_GRP_LOADANDTRAP, "loadandtrap" },
	{ SYSZ_GRP_LOADANDZERORIGHTMOSTBYTE, "loadandzerorightmostbyte" },
	{ SYSZ_GRP_LOADSTOREONCOND, "loadstoreoncond" },
	{ SYSZ_GRP_LOADSTOREONCOND2, "loadstoreoncond2" },
	{ SYSZ_GRP_MESSAGESECURITYASSIST3, "messagesecurityassist3" },
	{ SYSZ_GRP_MESSAGESECURITYASSIST4, "messagesecurityassist4" },
	{ SYSZ_GRP_MESSAGESECURITYASSIST5, "messagesecurityassist5" },
	{ SYSZ_GRP_MESSAGESECURITYASSIST7, "messagesecurityassist7" },
	{ SYSZ_GRP_MESSAGESECURITYASSIST8, "messagesecurityassist8" },
	{ SYSZ_GRP_MISCELLANEOUSEXTENSIONS, "miscellaneousextensions" },
	{ SYSZ_GRP_MISCELLANEOUSEXTENSIONS2, "miscellaneousextensions2" },
	{ SYSZ_GRP_POPULATIONCOUNT, "populationcount" },
	{ SYSZ_GRP_PROCESSORASSIST, "processorassist" },
	{ SYSZ_GRP_RESETREFERENCEBITSMULTIPLE, "resetreferencebitsmultiple" },
	{ SYSZ_GRP_TRANSACTIONALEXECUTION, "transactionalexecution" },
	{ SYSZ_GRP_VECTOR, "vector" },
	{ SYSZ_GRP_VECTORENHANCEMENTS1, "vectorenhancements1" },
	{ SYSZ_GRP_VECTORPACKEDDECIMAL, "vectorpackeddecimal" },
};
#endif

const char *SystemZ_group_name(csh handle, unsigned int id)
{
#ifndef CAPSTONE_DIET
	return id2name(group_name_maps, ARR_SIZE(group_name_maps), id);
#else
	return NULL;
#endif
}

// map internal raw register to 'public' register
sysz_reg SystemZ_map_register(unsigned int r)
{
	static const unsigned int map[] = { 0,
		/* SystemZ_CC = 1 */ SYSZ_REG_CC,
		/* SystemZ_A0 = 2 */ SYSZ_REG_A0,
		/* SystemZ_A1 = 3 */ SYSZ_REG_A1,
		/* SystemZ_A2 = 4 */ SYSZ_REG_A2,
		/* SystemZ_A3 = 5 */ SYSZ_REG_A3,
		/* SystemZ_A4 = 6 */ SYSZ_REG_A4,
		/* SystemZ_A5 = 7 */ SYSZ_REG_A5,
		/* SystemZ_A6 = 8 */ SYSZ_REG_A6,
		/* SystemZ_A7 = 9 */ SYSZ_REG_A7,
		/* SystemZ_A8 = 10 */ SYSZ_REG_A8,
		/* SystemZ_A9 = 11 */ SYSZ_REG_A9,
		/* SystemZ_A10 = 12 */ SYSZ_REG_A10,
		/* SystemZ_A11 = 13 */ SYSZ_REG_A11,
		/* SystemZ_A12 = 14 */ SYSZ_REG_A12,
		/* SystemZ_A13 = 15 */ SYSZ_REG_A13,
		/* SystemZ_A14 = 16 */ SYSZ_REG_A14,
		/* SystemZ_A15 = 17 */ SYSZ_REG_A15,
		/* SystemZ_C0 = 18 */ SYSZ_REG_C0,
		/* SystemZ_C1 = 19 */ SYSZ_REG_C1,
		/* SystemZ_C2 = 20 */ SYSZ_REG_C2,
		/* SystemZ_C3 = 21 */ SYSZ_REG_C3,
		/* SystemZ_C4 = 22 */ SYSZ_REG_C4,
		/* SystemZ_C5 = 23 */ SYSZ_REG_C5,
		/* SystemZ_C6 = 24 */ SYSZ_REG_C6,
		/* SystemZ_C7 = 25 */ SYSZ_REG_C7,
		/* SystemZ_C8 = 26 */ SYSZ_REG_C8,
		/* SystemZ_C9 = 27 */ SYSZ_REG_C9,
		/* SystemZ_C10 = 28 */ SYSZ_REG_C10,
		/* SystemZ_C11 = 29 */ SYSZ_REG_C11,
		/* SystemZ_C12 = 30 */ SYSZ_REG_C12,
		/* SystemZ_C13 = 31 */ SYSZ_REG_C13,
		/* SystemZ_C14 = 32 */ SYSZ_REG_C14,
		/* SystemZ_C15 = 33 */ SYSZ_REG_C15,
		/* SystemZ_V0 = 34 */ SYSZ_REG_V0,
		/* SystemZ_V1 = 35 */ SYSZ_REG_V1,
		/* SystemZ_V2 = 36 */ SYSZ_REG_V2,
		/* SystemZ_V3 = 37 */ SYSZ_REG_V3,
		/* SystemZ_V4 = 38 */ SYSZ_REG_V4,
		/* SystemZ_V5 = 39 */ SYSZ_REG_V5,
		/* SystemZ_V6 = 40 */ SYSZ_REG_V6,
		/* SystemZ_V7 = 41 */ SYSZ_REG_V7,
		/* SystemZ_V8 = 42 */ SYSZ_REG_V8,
		/* SystemZ_V9 = 43 */ SYSZ_REG_V9,
		/* SystemZ_V10 = 44 */ SYSZ_REG_V10,
		/* SystemZ_V11 = 45 */ SYSZ_REG_V11,
		/* SystemZ_V12 = 46 */ SYSZ_REG_V12,
		/* SystemZ_V13 = 47 */ SYSZ_REG_V13,
		/* SystemZ_V14 = 48 */ SYSZ_REG_V14,
		/* SystemZ_V15 = 49 */ SYSZ_REG_V15,
		/* SystemZ_V16 = 50 */ SYSZ_REG_V16,
		/* SystemZ_V17 = 51 */ SYSZ_REG_V17,
		/* SystemZ_V18 = 52 */ SYSZ_REG_V18,
		/* SystemZ_V19 = 53 */ SYSZ_REG_V19,
		/* SystemZ_V20 = 54 */ SYSZ_REG_V20,
		/* SystemZ_V21 = 55 */ SYSZ_REG_V21,
		/* SystemZ_V22 = 56 */ SYSZ_REG_V22,
		/* SystemZ_V23 = 57 */ SYSZ_REG_V23,
		/* SystemZ_V24 = 58 */ SYSZ_REG_V24,
		/* SystemZ_V25 = 59 */ SYSZ_REG_V25,
		/* SystemZ_V26 = 60 */ SYSZ_REG_V26,
		/* SystemZ_V27 = 61 */ SYSZ_REG_V27,
		/* SystemZ_V28 = 62 */ SYSZ_REG_V28,
		/* SystemZ_V29 = 63 */ SYSZ_REG_V29,
		/* SystemZ_V30 = 64 */ SYSZ_REG_V30,
		/* SystemZ_V31 = 65 */ SYSZ_REG_V31,
		/* SystemZ_F0D = 66 */ SYSZ_REG_F0,
		/* SystemZ_F1D = 67 */ SYSZ_REG_F1,
		/* SystemZ_F2D = 68 */ SYSZ_REG_F2,
		/* SystemZ_F3D = 69 */ SYSZ_REG_F3,
		/* SystemZ_F4D = 70 */ SYSZ_REG_F4,
		/* SystemZ_F5D = 71 */ SYSZ_REG_F5,
		/* SystemZ_F6D = 72 */ SYSZ_REG_F6,
		/* SystemZ_F7D = 73 */ SYSZ_REG_F7,
		/* SystemZ_F8D = 74 */ SYSZ_REG_F8,
		/* SystemZ_F9D = 75 */ SYSZ_REG_F9,
		/* SystemZ_F10D = 76 */ SYSZ_REG_F10,
		/* SystemZ_F11D = 77 */ SYSZ_REG_F11,
		/* SystemZ_F12D = 78 */ SYSZ_REG_F12,
		/* SystemZ_F13D = 79 */ SYSZ_REG_F13,
		/* SystemZ_F14D = 80 */ SYSZ_REG_F14,
		/* SystemZ_F15D = 81 */ SYSZ_REG_F15,
		/* SystemZ_F16D = 82 */ SYSZ_REG_F16,
		/* SystemZ_F17D = 83 */ SYSZ_REG_F17,
		/* SystemZ_F18D = 84 */ SYSZ_REG_F18,
		/* SystemZ_F19D = 85 */ SYSZ_REG_F19,
		/* SystemZ_F20D = 86 */ SYSZ_REG_F20,
		/* SystemZ_F21D = 87 */ SYSZ_REG_F21,
		/* SystemZ_F22D = 88 */ SYSZ_REG_F22,
		/* SystemZ_F23D = 89 */ SYSZ_REG_F23,
		/* SystemZ_F24D = 90 */ SYSZ_REG_F24,
		/* SystemZ_F25D = 91 */ SYSZ_REG_F25,
		/* SystemZ_F26D = 92 */ SYSZ_REG_F26,
		/* SystemZ_F27D = 93 */ SYSZ_REG_F27,
		/* SystemZ_F28D = 94 */ SYSZ_REG_F28,
		/* SystemZ_F29D = 95 */ SYSZ_REG_F29,
		/* SystemZ_F30D = 96 */ SYSZ_REG_F30,
		/* SystemZ_F31D = 97 */ SYSZ_REG_F31,
		/* SystemZ_F0Q = 98 */ SYSZ_REG_F0,
		/* SystemZ_F1Q = 99 */ SYSZ_REG_F1,
		/* SystemZ_F4Q = 100 */ SYSZ_REG_F4,
		/* SystemZ_F5Q = 101 */ SYSZ_REG_F5,
		/* SystemZ_F8Q = 102 */ SYSZ_REG_F8,
		/* SystemZ_F9Q = 103 */ SYSZ_REG_F9,
		/* SystemZ_F12Q = 104 */ SYSZ_REG_F12,
		/* SystemZ_F13Q = 105 */ SYSZ_REG_F13,
		/* SystemZ_F0S = 106 */ SYSZ_REG_F0,
		/* SystemZ_F1S = 107 */ SYSZ_REG_F1,
		/* SystemZ_F2S = 108 */ SYSZ_REG_F2,
		/* SystemZ_F3S = 109 */ SYSZ_REG_F3,
		/* SystemZ_F4S = 110 */ SYSZ_REG_F4,
		/* SystemZ_F5S = 111 */ SYSZ_REG_F5,
		/* SystemZ_F6S = 112 */ SYSZ_REG_F6,
		/* SystemZ_F7S = 113 */ SYSZ_REG_F7,
		/* SystemZ_F8S = 114 */ SYSZ_REG_F8,
		/* SystemZ_F9S = 115 */ SYSZ_REG_F9,
		/* SystemZ_F10S = 116 */ SYSZ_REG_F10,
		/* SystemZ_F11S = 117 */ SYSZ_REG_F11,
		/* SystemZ_F12S = 118 */ SYSZ_REG_F12,
		/* SystemZ_F13S = 119 */ SYSZ_REG_F13,
		/* SystemZ_F14S = 120 */ SYSZ_REG_F14,
		/* SystemZ_F15S = 121 */ SYSZ_REG_F15,
		/* SystemZ_F16S = 122 */ SYSZ_REG_F16,
		/* SystemZ_F17S = 123 */ SYSZ_REG_F17,
		/* SystemZ_F18S = 124 */ SYSZ_REG_F18,
		/* SystemZ_F19S = 125 */ SYSZ_REG_F19,
		/* SystemZ_F20S = 126 */ SYSZ_REG_F20,
		/* SystemZ_F21S = 127 */ SYSZ_REG_F21,
		/* SystemZ_F22S = 128 */ SYSZ_REG_F22,
		/* SystemZ_F23S = 129 */ SYSZ_REG_F23,
		/* SystemZ_F24S = 130 */ SYSZ_REG_F24,
		/* SystemZ_F25S = 131 */ SYSZ_REG_F25,
		/* SystemZ_F26S = 132 */ SYSZ_REG_F26,
		/* SystemZ_F27S = 133 */ SYSZ_REG_F27,
		/* SystemZ_F28S = 134 */ SYSZ_REG_F28,
		/* SystemZ_F29S = 135 */ SYSZ_REG_F29,
		/* SystemZ_F30S = 136 */ SYSZ_REG_F30,
		/* SystemZ_F31S = 137 */ SYSZ_REG_F31,
		/* SystemZ_R0D = 138 */ SYSZ_REG_0,
		/* SystemZ_R1D = 139 */ SYSZ_REG_1,
		/* SystemZ_R2D = 140 */ SYSZ_REG_2,
		/* SystemZ_R3D = 141 */ SYSZ_REG_3,
		/* SystemZ_R4D = 142 */ SYSZ_REG_4,
		/* SystemZ_R5D = 143 */ SYSZ_REG_5,
		/* SystemZ_R6D = 144 */ SYSZ_REG_6,
		/* SystemZ_R7D = 145 */ SYSZ_REG_7,
		/* SystemZ_R8D = 146 */ SYSZ_REG_8,
		/* SystemZ_R9D = 147 */ SYSZ_REG_9,
		/* SystemZ_R10D = 148 */ SYSZ_REG_10,
		/* SystemZ_R11D = 149 */ SYSZ_REG_11,
		/* SystemZ_R12D = 150 */ SYSZ_REG_12,
		/* SystemZ_R13D = 151 */ SYSZ_REG_13,
		/* SystemZ_R14D = 152 */ SYSZ_REG_14,
		/* SystemZ_R15D = 153 */ SYSZ_REG_15,
		/* SystemZ_R0H = 154 */ SYSZ_REG_0,
		/* SystemZ_R1H = 155 */ SYSZ_REG_1,
		/* SystemZ_R2H = 156 */ SYSZ_REG_2,
		/* SystemZ_R3H = 157 */ SYSZ_REG_3,
		/* SystemZ_R4H = 158 */ SYSZ_REG_4,
		/* SystemZ_R5H = 159 */ SYSZ_REG_5,
		/* SystemZ_R6H = 160 */ SYSZ_REG_6,
		/* SystemZ_R7H = 161 */ SYSZ_REG_7,
		/* SystemZ_R8H = 162 */ SYSZ_REG_8,
		/* SystemZ_R9H = 163 */ SYSZ_REG_9,
		/* SystemZ_R10H = 164 */ SYSZ_REG_10,
		/* SystemZ_R11H = 165 */ SYSZ_REG_11,
		/* SystemZ_R12H = 166 */ SYSZ_REG_12,
		/* SystemZ_R13H = 167 */ SYSZ_REG_13,
		/* SystemZ_R14H = 168 */ SYSZ_REG_14,
		/* SystemZ_R15H = 169 */ SYSZ_REG_15,
		/* SystemZ_R0L = 170 */ SYSZ_REG_0,
		/* SystemZ_R1L = 171 */ SYSZ_REG_1,
		/* SystemZ_R2L = 172 */ SYSZ_REG_2,
		/* SystemZ_R3L = 173 */ SYSZ_REG_3,
		/* SystemZ_R4L = 174 */ SYSZ_REG_4,
		/* SystemZ_R5L = 175 */ SYSZ_REG_5,
		/* SystemZ_R6L = 176 */ SYSZ_REG_6,
		/* SystemZ_R7L = 177 */ SYSZ_REG_7,
		/* SystemZ_R8L = 178 */ SYSZ_REG_8,
		/* SystemZ_R9L = 179 */ SYSZ_REG_9,
		/* SystemZ_R10L = 180 */ SYSZ_REG_10,
		/* SystemZ_R11L = 181 */ SYSZ_REG_11,
		/* SystemZ_R12L = 182 */ SYSZ_REG_12,
		/* SystemZ_R13L = 183 */ SYSZ_REG_13,
		/* SystemZ_R14L = 184 */ SYSZ_REG_14,
		/* SystemZ_R15L = 185 */ SYSZ_REG_15,
		/* SystemZ_R0Q = 186 */ SYSZ_REG_0,
		/* SystemZ_R2Q = 187 */ SYSZ_REG_2,
		/* SystemZ_R4Q = 188 */ SYSZ_REG_4,
		/* SystemZ_R6Q = 189 */ SYSZ_REG_6,
		/* SystemZ_R8Q = 190 */ SYSZ_REG_8,
		/* SystemZ_R10Q = 191 */ SYSZ_REG_10,
		/* SystemZ_R12Q = 192 */ SYSZ_REG_12,
		/* SystemZ_R14Q = 193 */ SYSZ_REG_14,
	};

	if (r < ARR_SIZE(map))
		return map[r];

	// cannot find this register
	return 0;
}

#endif
