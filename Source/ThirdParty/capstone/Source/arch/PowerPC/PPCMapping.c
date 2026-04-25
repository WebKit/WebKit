/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2015 */

#ifdef CAPSTONE_HAS_POWERPC

#include <stdio.h>	// debug
#include <string.h>

#include "../../utils.h"

#include "PPCMapping.h"

#define GET_INSTRINFO_ENUM
#include "PPCGenInstrInfo.inc"

// NOTE: this reg_name_maps[] reflects the order of registers in ppc_reg
static const name_map reg_name_maps[] = {
	{ PPC_REG_INVALID, NULL },

	{ PPC_REG_CARRY, "ca" },
	{ PPC_REG_CTR, "ctr" },
	{ PPC_REG_LR, "lr" },
	{ PPC_REG_RM, "rm" },
	{ PPC_REG_VRSAVE, "vrsave" },
	{ PPC_REG_XER, "xer" },
	{ PPC_REG_ZERO, "zero" },
	{ PPC_REG_CR0, "cr0" },
	{ PPC_REG_CR1, "cr1" },
	{ PPC_REG_CR2, "cr2" },
	{ PPC_REG_CR3, "cr3" },
	{ PPC_REG_CR4, "cr4" },
	{ PPC_REG_CR5, "cr5" },
	{ PPC_REG_CR6, "cr6" },
	{ PPC_REG_CR7, "cr7" },
	{ PPC_REG_CTR8, "ctr8" },
	{ PPC_REG_F0, "f0" },
	{ PPC_REG_F1, "f1" },
	{ PPC_REG_F2, "f2" },
	{ PPC_REG_F3, "f3" },
	{ PPC_REG_F4, "f4" },
	{ PPC_REG_F5, "f5" },
	{ PPC_REG_F6, "f6" },
	{ PPC_REG_F7, "f7" },
	{ PPC_REG_F8, "f8" },
	{ PPC_REG_F9, "f9" },
	{ PPC_REG_F10, "f10" },
	{ PPC_REG_F11, "f11" },
	{ PPC_REG_F12, "f12" },
	{ PPC_REG_F13, "f13" },
	{ PPC_REG_F14, "f14" },
	{ PPC_REG_F15, "f15" },
	{ PPC_REG_F16, "f16" },
	{ PPC_REG_F17, "f17" },
	{ PPC_REG_F18, "f18" },
	{ PPC_REG_F19, "f19" },
	{ PPC_REG_F20, "f20" },
	{ PPC_REG_F21, "f21" },
	{ PPC_REG_F22, "f22" },
	{ PPC_REG_F23, "f23" },
	{ PPC_REG_F24, "f24" },
	{ PPC_REG_F25, "f25" },
	{ PPC_REG_F26, "f26" },
	{ PPC_REG_F27, "f27" },
	{ PPC_REG_F28, "f28" },
	{ PPC_REG_F29, "f29" },
	{ PPC_REG_F30, "f30" },
	{ PPC_REG_F31, "f31" },
	{ PPC_REG_LR8, "lr8" },

	{ PPC_REG_Q0, "q0" },
	{ PPC_REG_Q1, "q1" },
	{ PPC_REG_Q2, "q2" },
	{ PPC_REG_Q3, "q3" },
	{ PPC_REG_Q4, "q4" },
	{ PPC_REG_Q5, "q5" },
	{ PPC_REG_Q6, "q6" },
	{ PPC_REG_Q7, "q7" },
	{ PPC_REG_Q8, "q8" },
	{ PPC_REG_Q9, "q9" },
	{ PPC_REG_Q10, "q10" },
	{ PPC_REG_Q11, "q11" },
	{ PPC_REG_Q12, "q12" },
	{ PPC_REG_Q13, "q13" },
	{ PPC_REG_Q14, "q14" },
	{ PPC_REG_Q15, "q15" },
	{ PPC_REG_Q16, "q16" },
	{ PPC_REG_Q17, "q17" },
	{ PPC_REG_Q18, "q18" },
	{ PPC_REG_Q19, "q19" },
	{ PPC_REG_Q20, "q20" },
	{ PPC_REG_Q21, "q21" },
	{ PPC_REG_Q22, "q22" },
	{ PPC_REG_Q23, "q23" },
	{ PPC_REG_Q24, "q24" },
	{ PPC_REG_Q25, "q25" },
	{ PPC_REG_Q26, "q26" },
	{ PPC_REG_Q27, "q27" },
	{ PPC_REG_Q28, "q28" },
	{ PPC_REG_Q29, "q29" },
	{ PPC_REG_Q30, "q30" },
	{ PPC_REG_Q31, "q31" },
	{ PPC_REG_R0, "r0" },
	{ PPC_REG_R1, "r1" },
	{ PPC_REG_R2, "r2" },
	{ PPC_REG_R3, "r3" },
	{ PPC_REG_R4, "r4" },
	{ PPC_REG_R5, "r5" },
	{ PPC_REG_R6, "r6" },
	{ PPC_REG_R7, "r7" },
	{ PPC_REG_R8, "r8" },
	{ PPC_REG_R9, "r9" },
	{ PPC_REG_R10, "r10" },
	{ PPC_REG_R11, "r11" },
	{ PPC_REG_R12, "r12" },
	{ PPC_REG_R13, "r13" },
	{ PPC_REG_R14, "r14" },
	{ PPC_REG_R15, "r15" },
	{ PPC_REG_R16, "r16" },
	{ PPC_REG_R17, "r17" },
	{ PPC_REG_R18, "r18" },
	{ PPC_REG_R19, "r19" },
	{ PPC_REG_R20, "r20" },
	{ PPC_REG_R21, "r21" },
	{ PPC_REG_R22, "r22" },
	{ PPC_REG_R23, "r23" },
	{ PPC_REG_R24, "r24" },
	{ PPC_REG_R25, "r25" },
	{ PPC_REG_R26, "r26" },
	{ PPC_REG_R27, "r27" },
	{ PPC_REG_R28, "r28" },
	{ PPC_REG_R29, "r29" },
	{ PPC_REG_R30, "r30" },
	{ PPC_REG_R31, "r31" },
	{ PPC_REG_V0, "v0" },
	{ PPC_REG_V1, "v1" },
	{ PPC_REG_V2, "v2" },
	{ PPC_REG_V3, "v3" },
	{ PPC_REG_V4, "v4" },
	{ PPC_REG_V5, "v5" },
	{ PPC_REG_V6, "v6" },
	{ PPC_REG_V7, "v7" },
	{ PPC_REG_V8, "v8" },
	{ PPC_REG_V9, "v9" },
	{ PPC_REG_V10, "v10" },
	{ PPC_REG_V11, "v11" },
	{ PPC_REG_V12, "v12" },
	{ PPC_REG_V13, "v13" },
	{ PPC_REG_V14, "v14" },
	{ PPC_REG_V15, "v15" },
	{ PPC_REG_V16, "v16" },
	{ PPC_REG_V17, "v17" },
	{ PPC_REG_V18, "v18" },
	{ PPC_REG_V19, "v19" },
	{ PPC_REG_V20, "v20" },
	{ PPC_REG_V21, "v21" },
	{ PPC_REG_V22, "v22" },
	{ PPC_REG_V23, "v23" },
	{ PPC_REG_V24, "v24" },
	{ PPC_REG_V25, "v25" },
	{ PPC_REG_V26, "v26" },
	{ PPC_REG_V27, "v27" },
	{ PPC_REG_V28, "v28" },
	{ PPC_REG_V29, "v29" },
	{ PPC_REG_V30, "v30" },
	{ PPC_REG_V31, "v31" },
	{ PPC_REG_VS0, "vs0" },
	{ PPC_REG_VS1, "vs1" },
	{ PPC_REG_VS2, "vs2" },
	{ PPC_REG_VS3, "vs3" },
	{ PPC_REG_VS4, "vs4" },
	{ PPC_REG_VS5, "vs5" },
	{ PPC_REG_VS6, "vs6" },
	{ PPC_REG_VS7, "vs7" },
	{ PPC_REG_VS8, "vs8" },
	{ PPC_REG_VS9, "vs9" },
	{ PPC_REG_VS10, "vs10" },
	{ PPC_REG_VS11, "vs11" },
	{ PPC_REG_VS12, "vs12" },
	{ PPC_REG_VS13, "vs13" },
	{ PPC_REG_VS14, "vs14" },
	{ PPC_REG_VS15, "vs15" },
	{ PPC_REG_VS16, "vs16" },
	{ PPC_REG_VS17, "vs17" },
	{ PPC_REG_VS18, "vs18" },
	{ PPC_REG_VS19, "vs19" },
	{ PPC_REG_VS20, "vs20" },
	{ PPC_REG_VS21, "vs21" },
	{ PPC_REG_VS22, "vs22" },
	{ PPC_REG_VS23, "vs23" },
	{ PPC_REG_VS24, "vs24" },
	{ PPC_REG_VS25, "vs25" },
	{ PPC_REG_VS26, "vs26" },
	{ PPC_REG_VS27, "vs27" },
	{ PPC_REG_VS28, "vs28" },
	{ PPC_REG_VS29, "vs29" },
	{ PPC_REG_VS30, "vs30" },
	{ PPC_REG_VS31, "vs31" },

	{ PPC_REG_VS32, "vs32" },
	{ PPC_REG_VS33, "vs33" },
	{ PPC_REG_VS34, "vs34" },
	{ PPC_REG_VS35, "vs35" },
	{ PPC_REG_VS36, "vs36" },
	{ PPC_REG_VS37, "vs37" },
	{ PPC_REG_VS38, "vs38" },
	{ PPC_REG_VS39, "vs39" },
	{ PPC_REG_VS40, "vs40" },
	{ PPC_REG_VS41, "vs41" },
	{ PPC_REG_VS42, "vs42" },
	{ PPC_REG_VS43, "vs43" },
	{ PPC_REG_VS44, "vs44" },
	{ PPC_REG_VS45, "vs45" },
	{ PPC_REG_VS46, "vs46" },
	{ PPC_REG_VS47, "vs47" },
	{ PPC_REG_VS48, "vs48" },
	{ PPC_REG_VS49, "vs49" },
	{ PPC_REG_VS50, "vs50" },
	{ PPC_REG_VS51, "vs51" },
	{ PPC_REG_VS52, "vs52" },
	{ PPC_REG_VS53, "vs53" },
	{ PPC_REG_VS54, "vs54" },
	{ PPC_REG_VS55, "vs55" },
	{ PPC_REG_VS56, "vs56" },
	{ PPC_REG_VS57, "vs57" },
	{ PPC_REG_VS58, "vs58" },
	{ PPC_REG_VS59, "vs59" },
	{ PPC_REG_VS60, "vs60" },
	{ PPC_REG_VS61, "vs61" },
	{ PPC_REG_VS62, "vs62" },
	{ PPC_REG_VS63, "vs63" },

    { PPC_REG_CR0EQ, "cr0eq" },
    { PPC_REG_CR1EQ, "cr1eq" },
    { PPC_REG_CR2EQ, "cr2eq" },
    { PPC_REG_CR3EQ, "cr3eq" },
    { PPC_REG_CR4EQ, "cr4eq" },
    { PPC_REG_CR5EQ, "cr5eq" },
    { PPC_REG_CR6EQ, "cr6eq" },
    { PPC_REG_CR7EQ, "cr7eq" },
    { PPC_REG_CR0GT, "cr0gt" },
    { PPC_REG_CR1GT, "cr1gt" },
    { PPC_REG_CR2GT, "cr2gt" },
    { PPC_REG_CR3GT, "cr3gt" },
    { PPC_REG_CR4GT, "cr4gt" },
    { PPC_REG_CR5GT, "cr5gt" },
    { PPC_REG_CR6GT, "cr6gt" },
    { PPC_REG_CR7GT, "cr7gt" },
    { PPC_REG_CR0LT, "cr0lt" },
    { PPC_REG_CR1LT, "cr1lt" },
    { PPC_REG_CR2LT, "cr2lt" },
    { PPC_REG_CR3LT, "cr3lt" },
    { PPC_REG_CR4LT, "cr4lt" },
    { PPC_REG_CR5LT, "cr5lt" },
    { PPC_REG_CR6LT, "cr6lt" },
    { PPC_REG_CR7LT, "cr7lt" },
    { PPC_REG_CR0UN, "cr0un" },
    { PPC_REG_CR1UN, "cr1un" },
    { PPC_REG_CR2UN, "cr2un" },
    { PPC_REG_CR3UN, "cr3un" },
    { PPC_REG_CR4UN, "cr4un" },
    { PPC_REG_CR5UN, "cr5un" },
    { PPC_REG_CR6UN, "cr6un" },
    { PPC_REG_CR7UN, "cr7un" },
};

const char *PPC_reg_name(csh handle, unsigned int reg)
{
    // binary searching since the IDs are sorted in order
    unsigned int left, right, m;
    unsigned int max = ARR_SIZE(reg_name_maps);

    right = max - 1;

    if (reg < reg_name_maps[0].id || reg > reg_name_maps[right].id)
        // not found
        return NULL;

    left = 0;

    while(left <= right) {
        m = (left + right) / 2;
        if (reg == reg_name_maps[m].id) {
            return reg_name_maps[m].name;
        }

        if (reg < reg_name_maps[m].id)
            right = m - 1;
        else
            left = m + 1;
    }

    // not found
    return NULL;
}

ppc_reg PPC_name_reg(const char *name)
{
	unsigned int i;

	for(i = 1; i < ARR_SIZE(reg_name_maps); i++) {
		if (!strcmp(name, reg_name_maps[i].name))
			return reg_name_maps[i].id;
	}

	// not found
	return 0;
}

static const insn_map insns[] = {
	// dummy item
	{
		0, 0,
#ifndef CAPSTONE_DIET
		{ 0 }, { 0 }, { 0 }, 0, 0
#endif
	},

#include "PPCMappingInsn.inc"
};

// given internal insn id, return public instruction info
void PPC_get_insn_id(cs_struct *h, cs_insn *insn, unsigned int id)
{
	int i;

	i = insn_find(insns, ARR_SIZE(insns), id, &h->insn_cache);
	if (i != 0) {
		insn->id = insns[i].mapid;

		if (h->detail) {
#ifndef CAPSTONE_DIET
			cs_struct handle;
			handle.detail = h->detail;

			memcpy(insn->detail->regs_read, insns[i].regs_use, sizeof(insns[i].regs_use));
			insn->detail->regs_read_count = (uint8_t)count_positive(insns[i].regs_use);

			memcpy(insn->detail->regs_write, insns[i].regs_mod, sizeof(insns[i].regs_mod));
			insn->detail->regs_write_count = (uint8_t)count_positive(insns[i].regs_mod);

			memcpy(insn->detail->groups, insns[i].groups, sizeof(insns[i].groups));
			insn->detail->groups_count = (uint8_t)count_positive8(insns[i].groups);

			if (insns[i].branch || insns[i].indirect_branch) {
				// this insn also belongs to JUMP group. add JUMP group
				insn->detail->groups[insn->detail->groups_count] = PPC_GRP_JUMP;
				insn->detail->groups_count++;
			}

			insn->detail->ppc.update_cr0 = cs_reg_write((csh)&handle, insn, PPC_REG_CR0);
#endif
		}
	}
}

static const char * const insn_name_maps[] = {
    NULL, // PPC_INS_BCT
#include "PPCMappingInsnName.inc"
};

const char *PPC_insn_name(csh handle, unsigned int id)
{
#ifndef CAPSTONE_DIET
	if (id >= PPC_INS_ENDING)
		return NULL;

	return insn_name_maps[id];
#else
	return NULL;
#endif
}

// map instruction name to public instruction ID
ppc_insn PPC_map_insn(const char *name)
{
	unsigned int i;

	for(i = 1; i < ARR_SIZE(insn_name_maps); i++) {
		if (!strcmp(name, insn_name_maps[i]))
			return i;
	}

	// not found
	return PPC_INS_INVALID;
}

#ifndef CAPSTONE_DIET
static const name_map group_name_maps[] = {
	// generic groups
	{ PPC_GRP_INVALID, NULL },
	{ PPC_GRP_JUMP,	"jump" },

	// architecture-specific groups
	{ PPC_GRP_ALTIVEC, "altivec" },
	{ PPC_GRP_MODE32, "mode32" },
	{ PPC_GRP_MODE64, "mode64" },
	{ PPC_GRP_BOOKE, "booke" },
	{ PPC_GRP_NOTBOOKE, "notbooke" },
	{ PPC_GRP_SPE, "spe" },
	{ PPC_GRP_VSX, "vsx" },
	{ PPC_GRP_E500, "e500" },
	{ PPC_GRP_PPC4XX, "ppc4xx" },
	{ PPC_GRP_PPC6XX, "ppc6xx" },
	{ PPC_GRP_ICBT, "icbt" },
	{ PPC_GRP_P8ALTIVEC, "p8altivec" },
	{ PPC_GRP_P8VECTOR, "p8vector" },
	{ PPC_GRP_QPX, "qpx" },
	{ PPC_GRP_PS, "ps" },
};
#endif

const char *PPC_group_name(csh handle, unsigned int id)
{
#ifndef CAPSTONE_DIET
	return id2name(group_name_maps, ARR_SIZE(group_name_maps), id);
#else
	return NULL;
#endif
}

static const struct ppc_alias alias_insn_name_maps[] = {
	//{ PPC_INS_BTA, "bta" },
	{ PPC_INS_B, PPC_BC_LT, "blt" },
	{ PPC_INS_B, PPC_BC_LE, "ble" },
	{ PPC_INS_B, PPC_BC_EQ, "beq" },
	{ PPC_INS_B, PPC_BC_GE, "bge" },
	{ PPC_INS_B, PPC_BC_GT, "bgt" },
	{ PPC_INS_B, PPC_BC_NE, "bne" },
	{ PPC_INS_B, PPC_BC_UN, "bun" },
	{ PPC_INS_B, PPC_BC_NU, "bnu" },
	{ PPC_INS_B, PPC_BC_SO, "bso" },
	{ PPC_INS_B, PPC_BC_NS, "bns" },

	{ PPC_INS_BA, PPC_BC_LT, "blta" },
	{ PPC_INS_BA, PPC_BC_LE, "blea" },
	{ PPC_INS_BA, PPC_BC_EQ, "beqa" },
	{ PPC_INS_BA, PPC_BC_GE, "bgea" },
	{ PPC_INS_BA, PPC_BC_GT, "bgta" },
	{ PPC_INS_BA, PPC_BC_NE, "bnea" },
	{ PPC_INS_BA, PPC_BC_UN, "buna" },
	{ PPC_INS_BA, PPC_BC_NU, "bnua" },
	{ PPC_INS_BA, PPC_BC_SO, "bsoa" },
	{ PPC_INS_BA, PPC_BC_NS, "bnsa" },

	{ PPC_INS_BCTR, PPC_BC_LT, "bltctr" },
	{ PPC_INS_BCTR, PPC_BC_LE, "blectr" },
	{ PPC_INS_BCTR, PPC_BC_EQ, "beqctr" },
	{ PPC_INS_BCTR, PPC_BC_GE, "bgectr" },
	{ PPC_INS_BCTR, PPC_BC_GT, "bgtctr" },
	{ PPC_INS_BCTR, PPC_BC_NE, "bnectr" },
	{ PPC_INS_BCTR, PPC_BC_UN, "bunctr" },
	{ PPC_INS_BCTR, PPC_BC_NU, "bnuctr" },
	{ PPC_INS_BCTR, PPC_BC_SO, "bsoctr" },
	{ PPC_INS_BCTR, PPC_BC_NS, "bnsctr" },

	{ PPC_INS_BCTRL, PPC_BC_LT, "bltctrl" },
	{ PPC_INS_BCTRL, PPC_BC_LE, "blectrl" },
	{ PPC_INS_BCTRL, PPC_BC_EQ, "beqctrl" },
	{ PPC_INS_BCTRL, PPC_BC_GE, "bgectrl" },
	{ PPC_INS_BCTRL, PPC_BC_GT, "bgtctrl" },
	{ PPC_INS_BCTRL, PPC_BC_NE, "bnectrl" },
	{ PPC_INS_BCTRL, PPC_BC_UN, "bunctrl" },
	{ PPC_INS_BCTRL, PPC_BC_NU, "bnuctrl" },
	{ PPC_INS_BCTRL, PPC_BC_SO, "bsoctrl" },
	{ PPC_INS_BCTRL, PPC_BC_NS, "bnsctrl" },

	{ PPC_INS_BL, PPC_BC_LT, "bltl" },
	{ PPC_INS_BL, PPC_BC_LE, "blel" },
	{ PPC_INS_BL, PPC_BC_EQ, "beql" },
	{ PPC_INS_BL, PPC_BC_GE, "bgel" },
	{ PPC_INS_BL, PPC_BC_GT, "bgtl" },
	{ PPC_INS_BL, PPC_BC_NE, "bnel" },
	{ PPC_INS_BL, PPC_BC_UN, "bunl" },
	{ PPC_INS_BL, PPC_BC_NU, "bnul" },
	{ PPC_INS_BL, PPC_BC_SO, "bsol" },
	{ PPC_INS_BL, PPC_BC_NS, "bnsl" },

	{ PPC_INS_BLA, PPC_BC_LT, "bltla" },
	{ PPC_INS_BLA, PPC_BC_LE, "blela" },
	{ PPC_INS_BLA, PPC_BC_EQ, "beqla" },
	{ PPC_INS_BLA, PPC_BC_GE, "bgela" },
	{ PPC_INS_BLA, PPC_BC_GT, "bgtla" },
	{ PPC_INS_BLA, PPC_BC_NE, "bnela" },
	{ PPC_INS_BLA, PPC_BC_UN, "bunla" },
	{ PPC_INS_BLA, PPC_BC_NU, "bnula" },
	{ PPC_INS_BLA, PPC_BC_SO, "bsola" },
	{ PPC_INS_BLA, PPC_BC_NS, "bnsla" },

	{ PPC_INS_BLR, PPC_BC_LT, "bltlr" },
	{ PPC_INS_BLR, PPC_BC_LE, "blelr" },
	{ PPC_INS_BLR, PPC_BC_EQ, "beqlr" },
	{ PPC_INS_BLR, PPC_BC_GE, "bgelr" },
	{ PPC_INS_BLR, PPC_BC_GT, "bgtlr" },
	{ PPC_INS_BLR, PPC_BC_NE, "bnelr" },
	{ PPC_INS_BLR, PPC_BC_UN, "bunlr" },
	{ PPC_INS_BLR, PPC_BC_NU, "bnulr" },
	{ PPC_INS_BLR, PPC_BC_SO, "bsolr" },
	{ PPC_INS_BLR, PPC_BC_NS, "bnslr" },

	{ PPC_INS_BLRL, PPC_BC_LT, "bltlrl" },
	{ PPC_INS_BLRL, PPC_BC_LE, "blelrl" },
	{ PPC_INS_BLRL, PPC_BC_EQ, "beqlrl" },
	{ PPC_INS_BLRL, PPC_BC_GE, "bgelrl" },
	{ PPC_INS_BLRL, PPC_BC_GT, "bgtlrl" },
	{ PPC_INS_BLRL, PPC_BC_NE, "bnelrl" },
	{ PPC_INS_BLRL, PPC_BC_UN, "bunlrl" },
	{ PPC_INS_BLRL, PPC_BC_NU, "bnulrl" },
	{ PPC_INS_BLRL, PPC_BC_SO, "bsolrl" },
	{ PPC_INS_BLRL, PPC_BC_NS, "bnslrl" },
};

// given alias mnemonic, return instruction ID & CC
bool PPC_alias_insn(const char *name, struct ppc_alias *alias)
{
	size_t i;

	alias->cc = PPC_BC_INVALID;

	for(i = 0; i < ARR_SIZE(alias_insn_name_maps); i++) {
		if (!strcmp(name, alias_insn_name_maps[i].mnem)) {
			// alias->id = alias_insn_name_maps[i].id;
			alias->cc = alias_insn_name_maps[i].cc;
			return true;
		}
	}

	// not found
	return false;
}

// check if this insn is relative branch
bool PPC_abs_branch(cs_struct *h, unsigned int id)
{
	unsigned int i;
	// list all absolute branch instructions
	static const unsigned int insn_abs[] = {
		PPC_BA,
		PPC_BCCA,
		PPC_BCCLA,
		PPC_BDNZA,
		PPC_BDNZAm,
		PPC_BDNZAp,
		PPC_BDNZLA,
		PPC_BDNZLAm,
		PPC_BDNZLAp,
		PPC_BDZA,
		PPC_BDZAm,
		PPC_BDZAp,
		PPC_BDZLAm,
		PPC_BDZLAp,
		PPC_BLA,
		PPC_gBCA,
		PPC_gBCLA,
		PPC_BDZLA,
		0
	};

	// printf("opcode: %u\n", id);

	for (i = 0; insn_abs[i]; i++) {
		if (id == insn_abs[i]) {
			return true;
		}
	}

	// not found
	return false;
}

#endif
