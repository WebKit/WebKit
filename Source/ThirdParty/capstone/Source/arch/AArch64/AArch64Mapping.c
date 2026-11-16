/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2019 */

#ifdef CAPSTONE_HAS_ARM64

#include <stdio.h>	// debug
#include <string.h>

#include "../../utils.h"

#include "AArch64Mapping.h"

#define GET_INSTRINFO_ENUM
#include "AArch64GenInstrInfo.inc"

#ifndef CAPSTONE_DIET
// NOTE: this reg_name_maps[] reflects the order of registers in arm64_reg
static const char * const reg_name_maps[] = {
	NULL, /* ARM64_REG_INVALID */

	"ffr",
  	"fp",
  	"lr",
  	"nzcv",
  	"sp",
  	"vg",
  	"wsp",
  	"wzr",
  	"xzr",

  	"za",

  	"b0",
  	"b1",
  	"b2",
  	"b3",
  	"b4",
  	"b5",
  	"b6",
  	"b7",
  	"b8",
  	"b9",
  	"b10",
  	"b11",
  	"b12",
  	"b13",
  	"b14",
  	"b15",
  	"b16",
  	"b17",
  	"b18",
  	"b19",
  	"b20",
  	"b21",
  	"b22",
  	"b23",
  	"b24",
  	"b25",
  	"b26",
  	"b27",
  	"b28",
  	"b29",
  	"b30",
  	"b31",

  	"d0",
  	"d1",
  	"d2",
  	"d3",
  	"d4",
  	"d5",
  	"d6",
  	"d7",
  	"d8",
  	"d9",
  	"d10",
  	"d11",
  	"d12",
  	"d13",
  	"d14",
  	"d15",
  	"d16",
  	"d17",
  	"d18",
  	"d19",
  	"d20",
  	"d21",
  	"d22",
  	"d23",
  	"d24",
  	"d25",
  	"d26",
  	"d27",
  	"d28",
  	"d29",
  	"d30",
  	"d31",

  	"h0",
  	"h1",
  	"h2",
  	"h3",
  	"h4",
  	"h5",
  	"h6",
  	"h7",
  	"h8",
  	"h9",
  	"h10",
  	"h11",
  	"h12",
  	"h13",
  	"h14",
  	"h15",
  	"h16",
  	"h17",
  	"h18",
  	"h19",
  	"h20",
  	"h21",
  	"h22",
  	"h23",
  	"h24",
  	"h25",
  	"h26",
  	"h27",
  	"h28",
  	"h29",
  	"h30",
  	"h31",

  	"p0",
  	"p1",
  	"p2",
  	"p3",
  	"p4",
  	"p5",
  	"p6",
  	"p7",
  	"p8",
  	"p9",
  	"p10",
  	"p11",
  	"p12",
  	"p13",
  	"p14",
  	"p15",

  	"q0",
  	"q1",
  	"q2",
  	"q3",
  	"q4",
  	"q5",
  	"q6",
  	"q7",
  	"q8",
  	"q9",
  	"q10",
  	"q11",
  	"q12",
  	"q13",
  	"q14",
  	"q15",
  	"q16",
  	"q17",
  	"q18",
  	"q19",
  	"q20",
  	"q21",
  	"q22",
  	"q23",
  	"q24",
  	"q25",
  	"q26",
  	"q27",
  	"q28",
  	"q29",
  	"q30",
  	"q31",

  	"s0",
  	"s1",
  	"s2",
  	"s3",
  	"s4",
  	"s5",
  	"s6",
  	"s7",
  	"s8",
  	"s9",
  	"s10",
  	"s11",
  	"s12",
  	"s13",
  	"s14",
  	"s15",
  	"s16",
  	"s17",
  	"s18",
  	"s19",
  	"s20",
  	"s21",
  	"s22",
  	"s23",
  	"s24",
  	"s25",
  	"s26",
  	"s27",
  	"s28",
  	"s29",
  	"s30",
  	"s31",

  	"w0",
  	"w1",
  	"w2",
  	"w3",
  	"w4",
  	"w5",
  	"w6",
  	"w7",
  	"w8",
  	"w9",
  	"w10",
  	"w11",
  	"w12",
  	"w13",
  	"w14",
  	"w15",
  	"w16",
  	"w17",
  	"w18",
  	"w19",
  	"w20",
  	"w21",
  	"w22",
  	"w23",
  	"w24",
  	"w25",
  	"w26",
  	"w27",
  	"w28",
  	"w29",
  	"w30",

  	"x0",
  	"x1",
  	"x2",
  	"x3",
  	"x4",
  	"x5",
  	"x6",
  	"x7",
  	"x8",
  	"x9",
  	"x10",
  	"x11",
  	"x12",
  	"x13",
  	"x14",
  	"x15",
  	"x16",
  	"x17",
  	"x18",
  	"x19",
  	"x20",
  	"x21",
  	"x22",
  	"x23",
  	"x24",
  	"x25",
  	"x26",
  	"x27",
  	"x28",

  	"z0",
  	"z1",
  	"z2",
  	"z3",
  	"z4",
  	"z5",
  	"z6",
  	"z7",
  	"z8",
  	"z9",
  	"z10",
  	"z11",
  	"z12",
  	"z13",
  	"z14",
  	"z15",
  	"z16",
  	"z17",
  	"z18",
  	"z19",
  	"z20",
  	"z21",
  	"z22",
  	"z23",
  	"z24",
  	"z25",
  	"z26",
  	"z27",
  	"z28",
  	"z29",
  	"z30",
  	"z31",

  	"zab0",

  	"zad0",
  	"zad1",
  	"zad2",
  	"zad3",
  	"zad4",
  	"zad5",
  	"zad6",
  	"zad7",

  	"zah0",
  	"zah1",

  	"zaq0",
  	"zaq1",
  	"zaq2",
  	"zaq3",
  	"zaq4",
  	"zaq5",
  	"zaq6",
  	"zaq7",
  	"zaq8",
  	"zaq9",
  	"zaq10",
  	"zaq11",
  	"zaq12",
  	"zaq13",
  	"zaq14",
  	"zaq15",

  	"zas0",
  	"zas1",
  	"zas2",
  	"zas3",

  	"v0",
  	"v1",
  	"v2",
  	"v3",
  	"v4",
  	"v5",
  	"v6",
  	"v7",
  	"v8",
  	"v9",
  	"v10",
  	"v11",
  	"v12",
  	"v13",
  	"v14",
  	"v15",
  	"v16",
  	"v17",
  	"v18",
  	"v19",
  	"v20",
  	"v21",
  	"v22",
  	"v23",
  	"v24",
  	"v25",
  	"v26",
  	"v27",
  	"v28",
  	"v29",
  	"v30",
  	"v31",
};
#endif

const char *AArch64_reg_name(csh handle, unsigned int reg)
{
#ifndef CAPSTONE_DIET
	if (reg >= ARR_SIZE(reg_name_maps))
		return NULL;

	return reg_name_maps[reg];
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

#include "AArch64MappingInsn.inc"
};

// given internal insn id, return public instruction info
void AArch64_get_insn_id(cs_struct *h, cs_insn *insn, unsigned int id)
{
	int i = insn_find(insns, ARR_SIZE(insns), id, &h->insn_cache);
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

			insn->detail->arm64.update_flags = cs_reg_write((csh)&handle, insn, ARM64_REG_NZCV);
#endif
		}
	}
}

static const char * const insn_name_maps[] = {
	NULL, // ARM64_INS_INVALID
#include "AArch64MappingInsnName.inc"
	"sbfiz",
	"ubfiz",
	"sbfx",
	"ubfx",
	"bfi",
	"bfxil",
	"ic",
	"dc",
	"at",
	"tlbi",
	"smstart",
  	"smstop",
};

const char *AArch64_insn_name(csh handle, unsigned int id)
{
#ifndef CAPSTONE_DIET
	if (id >= ARM64_INS_ENDING)
		return NULL;

	if (id < ARR_SIZE(insn_name_maps))
		return insn_name_maps[id];

	// not found
	return NULL;
#else
	return NULL;
#endif
}

#ifndef CAPSTONE_DIET
static const name_map group_name_maps[] = {
	// generic groups
	{ ARM64_GRP_INVALID, NULL },
	{ ARM64_GRP_JUMP, "jump" },
	{ ARM64_GRP_CALL, "call" },
	{ ARM64_GRP_RET, "return" },
	{ ARM64_GRP_PRIVILEGE, "privilege" },
	{ ARM64_GRP_INT, "int" },
	{ ARM64_GRP_BRANCH_RELATIVE, "branch_relative" },
	{ ARM64_GRP_PAC, "pointer authentication" },

	// architecture-specific groups
	{ ARM64_GRP_CRYPTO, "crypto" },
	{ ARM64_GRP_FPARMV8, "fparmv8" },
	{ ARM64_GRP_NEON, "neon" },
	{ ARM64_GRP_CRC, "crc" },

	{ ARM64_GRP_AES, "aes" },
	{ ARM64_GRP_DOTPROD, "dotprod" },
	{ ARM64_GRP_FULLFP16, "fullfp16" },
	{ ARM64_GRP_LSE, "lse" },
	{ ARM64_GRP_RCPC, "rcpc" },
	{ ARM64_GRP_RDM, "rdm" },
	{ ARM64_GRP_SHA2, "sha2" },
	{ ARM64_GRP_SHA3, "sha3" },
	{ ARM64_GRP_SM4, "sm4" },
	{ ARM64_GRP_SVE, "sve" },
	{ ARM64_GRP_SVE2, "sve2" },
  	{ ARM64_GRP_SVE2AES, "sve2-aes" },
  	{ ARM64_GRP_SVE2BitPerm, "sve2-bitperm" },
  	{ ARM64_GRP_SVE2SHA3, "sve2-sha3" },
  	{ ARM64_GRP_SVE2SM4, "sve2-sm4" },
  	{ ARM64_GRP_SME, "sme" },
  	{ ARM64_GRP_SMEF64, "sme-f64" },
  	{ ARM64_GRP_SMEI64, "sme-i64" },
  	{ ARM64_GRP_MatMulFP32, "f32mm" },
  	{ ARM64_GRP_MatMulFP64, "f64mm" },
  	{ ARM64_GRP_MatMulInt8, "i8mm" },
	{ ARM64_GRP_V8_1A, "v8_1a" },
	{ ARM64_GRP_V8_3A, "v8_3a" },
	{ ARM64_GRP_V8_4A, "v8_4a" },
};
#endif

const char *AArch64_group_name(csh handle, unsigned int id)
{
#ifndef CAPSTONE_DIET
	return id2name(group_name_maps, ARR_SIZE(group_name_maps), id);
#else
	return NULL;
#endif
}

// map instruction name to public instruction ID
arm64_insn AArch64_map_insn(const char *name)
{
	unsigned int i;

	for(i = 1; i < ARR_SIZE(insn_name_maps); i++) {
		if (!strcmp(name, insn_name_maps[i]))
			return i;
	}

	// not found
	return ARM64_INS_INVALID;
}

// map internal raw vregister to 'public' register
arm64_reg AArch64_map_vregister(unsigned int r)
{
	static const unsigned short RegAsmOffsetvreg[] = {
#include "AArch64GenRegisterV.inc"
	};

	if (r < ARR_SIZE(RegAsmOffsetvreg))
		return RegAsmOffsetvreg[r - 1];

	// cannot find this register
	return 0;
}

static const name_map sys_op_name_map[] = {
	{ ARM64_TLBI_ALLE1, "alle1"} ,
  	{ ARM64_TLBI_ALLE1IS, "alle1is"} ,
  	{ ARM64_TLBI_ALLE1ISNXS, "alle1isnxs"} ,
  	{ ARM64_TLBI_ALLE1NXS, "alle1nxs"} ,
  	{ ARM64_TLBI_ALLE1OS, "alle1os"} ,
  	{ ARM64_TLBI_ALLE1OSNXS, "alle1osnxs"} ,
  	{ ARM64_TLBI_ALLE2, "alle2"} ,
  	{ ARM64_TLBI_ALLE2IS, "alle2is"} ,
  	{ ARM64_TLBI_ALLE2ISNXS, "alle2isnxs"} ,
  	{ ARM64_TLBI_ALLE2NXS, "alle2nxs"} ,
  	{ ARM64_TLBI_ALLE2OS, "alle2os"} ,
  	{ ARM64_TLBI_ALLE2OSNXS, "alle2osnxs"} ,
  	{ ARM64_TLBI_ALLE3, "alle3"} ,
  	{ ARM64_TLBI_ALLE3IS, "alle3is"} ,
  	{ ARM64_TLBI_ALLE3ISNXS, "alle3isnxs"} ,
  	{ ARM64_TLBI_ALLE3NXS, "alle3nxs"} ,
  	{ ARM64_TLBI_ALLE3OS, "alle3os"} ,
  	{ ARM64_TLBI_ALLE3OSNXS, "alle3osnxs"} ,
  	{ ARM64_TLBI_ASIDE1, "aside1"} ,
  	{ ARM64_TLBI_ASIDE1IS, "aside1is"} ,
  	{ ARM64_TLBI_ASIDE1ISNXS, "aside1isnxs"} ,
  	{ ARM64_TLBI_ASIDE1NXS, "aside1nxs"} ,
  	{ ARM64_TLBI_ASIDE1OS, "aside1os"} ,
  	{ ARM64_TLBI_ASIDE1OSNXS, "aside1osnxs"} ,
  	{ ARM64_TLBI_IPAS2E1, "ipas2e1"} ,
  	{ ARM64_TLBI_IPAS2E1IS, "ipas2e1is"} ,
  	{ ARM64_TLBI_IPAS2E1ISNXS, "ipas2e1isnxs"} ,
  	{ ARM64_TLBI_IPAS2E1NXS, "ipas2e1nxs"} ,
  	{ ARM64_TLBI_IPAS2E1OS, "ipas2e1os"} ,
  	{ ARM64_TLBI_IPAS2E1OSNXS, "ipas2e1osnxs"} ,
  	{ ARM64_TLBI_IPAS2LE1, "ipas2le1"} ,
  	{ ARM64_TLBI_IPAS2LE1IS, "ipas2le1is"} ,
  	{ ARM64_TLBI_IPAS2LE1ISNXS, "ipas2le1isnxs"} ,
  	{ ARM64_TLBI_IPAS2LE1NXS, "ipas2le1nxs"} ,
  	{ ARM64_TLBI_IPAS2LE1OS, "ipas2le1os"} ,
  	{ ARM64_TLBI_IPAS2LE1OSNXS, "ipas2le1osnxs"} ,
  	{ ARM64_TLBI_PAALL, "paall"} ,
  	{ ARM64_TLBI_PAALLNXS, "paallnxs"} ,
  	{ ARM64_TLBI_PAALLOS, "paallos"} ,
  	{ ARM64_TLBI_PAALLOSNXS, "paallosnxs"} ,
  	{ ARM64_TLBI_RIPAS2E1, "ripas2e1"} ,
  	{ ARM64_TLBI_RIPAS2E1IS, "ripas2e1is"} ,
  	{ ARM64_TLBI_RIPAS2E1ISNXS, "ripas2e1isnxs"} ,
  	{ ARM64_TLBI_RIPAS2E1NXS, "ripas2e1nxs"} ,
  	{ ARM64_TLBI_RIPAS2E1OS, "ripas2e1os"} ,
  	{ ARM64_TLBI_RIPAS2E1OSNXS, "ripas2e1osnxs"} ,
  	{ ARM64_TLBI_RIPAS2LE1, "ripas2le1"} ,
  	{ ARM64_TLBI_RIPAS2LE1IS, "ripas2le1is"} ,
  	{ ARM64_TLBI_RIPAS2LE1ISNXS, "ripas2le1isnxs"} ,
  	{ ARM64_TLBI_RIPAS2LE1NXS, "ripas2le1nxs"} ,
  	{ ARM64_TLBI_RIPAS2LE1OS, "ripas2le1os"} ,
  	{ ARM64_TLBI_RIPAS2LE1OSNXS, "ripas2le1osnxs"} ,
  	{ ARM64_TLBI_RPALOS, "rpalos"} ,
  	{ ARM64_TLBI_RPALOSNXS, "rpalosnxs"} ,
  	{ ARM64_TLBI_RPAOS, "rpaos"} ,
  	{ ARM64_TLBI_RPAOSNXS, "rpaosnxs"} ,
  	{ ARM64_TLBI_RVAAE1, "rvaae1"} ,
  	{ ARM64_TLBI_RVAAE1IS, "rvaae1is"} ,
  	{ ARM64_TLBI_RVAAE1ISNXS, "rvaae1isnxs"} ,
  	{ ARM64_TLBI_RVAAE1NXS, "rvaae1nxs"} ,
  	{ ARM64_TLBI_RVAAE1OS, "rvaae1os"} ,
  	{ ARM64_TLBI_RVAAE1OSNXS, "rvaae1osnxs"} ,
  	{ ARM64_TLBI_RVAALE1, "rvaale1"} ,
  	{ ARM64_TLBI_RVAALE1IS, "rvaale1is"} ,
  	{ ARM64_TLBI_RVAALE1ISNXS, "rvaale1isnxs"} ,
  	{ ARM64_TLBI_RVAALE1NXS, "rvaale1nxs"} ,
  	{ ARM64_TLBI_RVAALE1OS, "rvaale1os"} ,
  	{ ARM64_TLBI_RVAALE1OSNXS, "rvaale1osnxs"} ,
  	{ ARM64_TLBI_RVAE1, "rvae1"} ,
  	{ ARM64_TLBI_RVAE1IS, "rvae1is"} ,
  	{ ARM64_TLBI_RVAE1ISNXS, "rvae1isnxs"} ,
  	{ ARM64_TLBI_RVAE1NXS, "rvae1nxs"} ,
  	{ ARM64_TLBI_RVAE1OS, "rvae1os"} ,
  	{ ARM64_TLBI_RVAE1OSNXS, "rvae1osnxs"} ,
  	{ ARM64_TLBI_RVAE2, "rvae2"} ,
  	{ ARM64_TLBI_RVAE2IS, "rvae2is"} ,
  	{ ARM64_TLBI_RVAE2ISNXS, "rvae2isnxs"} ,
  	{ ARM64_TLBI_RVAE2NXS, "rvae2nxs"} ,
  	{ ARM64_TLBI_RVAE2OS, "rvae2os"} ,
  	{ ARM64_TLBI_RVAE2OSNXS, "rvae2osnxs"} ,
  	{ ARM64_TLBI_RVAE3, "rvae3"} ,
  	{ ARM64_TLBI_RVAE3IS, "rvae3is"} ,
  	{ ARM64_TLBI_RVAE3ISNXS, "rvae3isnxs"} ,
  	{ ARM64_TLBI_RVAE3NXS, "rvae3nxs"} ,
  	{ ARM64_TLBI_RVAE3OS, "rvae3os"} ,
  	{ ARM64_TLBI_RVAE3OSNXS, "rvae3osnxs"} ,
  	{ ARM64_TLBI_RVALE1, "rvale1"} ,
  	{ ARM64_TLBI_RVALE1IS, "rvale1is"} ,
  	{ ARM64_TLBI_RVALE1ISNXS, "rvale1isnxs"} ,
  	{ ARM64_TLBI_RVALE1NXS, "rvale1nxs"} ,
  	{ ARM64_TLBI_RVALE1OS, "rvale1os"} ,
  	{ ARM64_TLBI_RVALE1OSNXS, "rvale1osnxs"} ,
  	{ ARM64_TLBI_RVALE2, "rvale2"} ,
  	{ ARM64_TLBI_RVALE2IS, "rvale2is"} ,
  	{ ARM64_TLBI_RVALE2ISNXS, "rvale2isnxs"} ,
  	{ ARM64_TLBI_RVALE2NXS, "rvale2nxs"} ,
  	{ ARM64_TLBI_RVALE2OS, "rvale2os"} ,
  	{ ARM64_TLBI_RVALE2OSNXS, "rvale2osnxs"} ,
  	{ ARM64_TLBI_RVALE3, "rvale3"} ,
  	{ ARM64_TLBI_RVALE3IS, "rvale3is"} ,
  	{ ARM64_TLBI_RVALE3ISNXS, "rvale3isnxs"} ,
  	{ ARM64_TLBI_RVALE3NXS, "rvale3nxs"} ,
  	{ ARM64_TLBI_RVALE3OS, "rvale3os"} ,
  	{ ARM64_TLBI_RVALE3OSNXS, "rvale3osnxs"} ,
  	{ ARM64_TLBI_VAAE1, "vaae1"} ,
  	{ ARM64_TLBI_VAAE1IS, "vaae1is"} ,
  	{ ARM64_TLBI_VAAE1ISNXS, "vaae1isnxs"} ,
  	{ ARM64_TLBI_VAAE1NXS, "vaae1nxs"} ,
  	{ ARM64_TLBI_VAAE1OS, "vaae1os"} ,
  	{ ARM64_TLBI_VAAE1OSNXS, "vaae1osnxs"} ,
  	{ ARM64_TLBI_VAALE1, "vaale1"} ,
  	{ ARM64_TLBI_VAALE1IS, "vaale1is"} ,
  	{ ARM64_TLBI_VAALE1ISNXS, "vaale1isnxs"} ,
  	{ ARM64_TLBI_VAALE1NXS, "vaale1nxs"} ,
  	{ ARM64_TLBI_VAALE1OS, "vaale1os"} ,
  	{ ARM64_TLBI_VAALE1OSNXS, "vaale1osnxs"} ,
  	{ ARM64_TLBI_VAE1, "vae1"} ,
  	{ ARM64_TLBI_VAE1IS, "vae1is"} ,
  	{ ARM64_TLBI_VAE1ISNXS, "vae1isnxs"} ,
  	{ ARM64_TLBI_VAE1NXS, "vae1nxs"} ,
  	{ ARM64_TLBI_VAE1OS, "vae1os"} ,
  	{ ARM64_TLBI_VAE1OSNXS, "vae1osnxs"} ,
  	{ ARM64_TLBI_VAE2, "vae2"} ,
  	{ ARM64_TLBI_VAE2IS, "vae2is"} ,
  	{ ARM64_TLBI_VAE2ISNXS, "vae2isnxs"} ,
  	{ ARM64_TLBI_VAE2NXS, "vae2nxs"} ,
  	{ ARM64_TLBI_VAE2OS, "vae2os"} ,
  	{ ARM64_TLBI_VAE2OSNXS, "vae2osnxs"} ,
  	{ ARM64_TLBI_VAE3, "vae3"} ,
  	{ ARM64_TLBI_VAE3IS, "vae3is"} ,
  	{ ARM64_TLBI_VAE3ISNXS, "vae3isnxs"} ,
  	{ ARM64_TLBI_VAE3NXS, "vae3nxs"} ,
  	{ ARM64_TLBI_VAE3OS, "vae3os"} ,
  	{ ARM64_TLBI_VAE3OSNXS, "vae3osnxs"} ,
  	{ ARM64_TLBI_VALE1, "vale1"} ,
  	{ ARM64_TLBI_VALE1IS, "vale1is"} ,
  	{ ARM64_TLBI_VALE1ISNXS, "vale1isnxs"} ,
  	{ ARM64_TLBI_VALE1NXS, "vale1nxs"} ,
  	{ ARM64_TLBI_VALE1OS, "vale1os"} ,
  	{ ARM64_TLBI_VALE1OSNXS, "vale1osnxs"} ,
  	{ ARM64_TLBI_VALE2, "vale2"} ,
  	{ ARM64_TLBI_VALE2IS, "vale2is"} ,
  	{ ARM64_TLBI_VALE2ISNXS, "vale2isnxs"} ,
  	{ ARM64_TLBI_VALE2NXS, "vale2nxs"} ,
  	{ ARM64_TLBI_VALE2OS, "vale2os"} ,
  	{ ARM64_TLBI_VALE2OSNXS, "vale2osnxs"} ,
  	{ ARM64_TLBI_VALE3, "vale3"} ,
  	{ ARM64_TLBI_VALE3IS, "vale3is"} ,
  	{ ARM64_TLBI_VALE3ISNXS, "vale3isnxs"} ,
  	{ ARM64_TLBI_VALE3NXS, "vale3nxs"} ,
  	{ ARM64_TLBI_VALE3OS, "vale3os"} ,
  	{ ARM64_TLBI_VALE3OSNXS, "vale3osnxs"} ,
  	{ ARM64_TLBI_VMALLE1, "vmalle1"} ,
  	{ ARM64_TLBI_VMALLE1IS, "vmalle1is"} ,
  	{ ARM64_TLBI_VMALLE1ISNXS, "vmalle1isnxs"} ,
  	{ ARM64_TLBI_VMALLE1NXS, "vmalle1nxs"} ,
  	{ ARM64_TLBI_VMALLE1OS, "vmalle1os"} ,
  	{ ARM64_TLBI_VMALLE1OSNXS, "vmalle1osnxs"} ,
  	{ ARM64_TLBI_VMALLS12E1, "vmalls12e1"} ,
  	{ ARM64_TLBI_VMALLS12E1IS, "vmalls12e1is"} ,
  	{ ARM64_TLBI_VMALLS12E1ISNXS, "vmalls12e1isnxs"} ,
  	{ ARM64_TLBI_VMALLS12E1NXS, "vmalls12e1nxs"} ,
  	{ ARM64_TLBI_VMALLS12E1OS, "vmalls12e1os"} ,
  	{ ARM64_TLBI_VMALLS12E1OSNXS, "vmalls12e1osnxs"} ,
  	{ ARM64_AT_S1E1R, "s1e1r"} ,
  	{ ARM64_AT_S1E2R, "s1e2r"} ,
  	{ ARM64_AT_S1E3R, "s1e3r"} ,
  	{ ARM64_AT_S1E1W, "s1e1w"} ,
  	{ ARM64_AT_S1E2W, "s1e2w"} ,
  	{ ARM64_AT_S1E3W, "s1e3w"} ,
  	{ ARM64_AT_S1E0R, "s1e0r"} ,
  	{ ARM64_AT_S1E0W, "s1e0w"} ,
  	{ ARM64_AT_S12E1R, "s12e1r"} ,
  	{ ARM64_AT_S12E1W, "s12e1w"} ,
  	{ ARM64_AT_S12E0R, "s12e0r"} ,
  	{ ARM64_AT_S12E0W, "s12e0w"} ,
  	{ ARM64_AT_S1E1RP, "s1e1rp"} ,
  	{ ARM64_AT_S1E1WP, "s1e1wp"} ,
  	{ ARM64_DC_CGDSW, "cgdsw"} ,
  	{ ARM64_DC_CGDVAC, "cgdvac"} ,
  	{ ARM64_DC_CGDVADP, "cgdvadp"} ,
  	{ ARM64_DC_CGDVAP, "cgdvap"} ,
  	{ ARM64_DC_CGSW, "cgsw"} ,
  	{ ARM64_DC_CGVAC, "cgvac"} ,
  	{ ARM64_DC_CGVADP, "cgvadp"} ,
  	{ ARM64_DC_CGVAP, "cgvap"} ,
  	{ ARM64_DC_CIGDSW, "cigdsw"} ,
  	{ ARM64_DC_CIGDVAC, "cigdvac"} ,
  	{ ARM64_DC_CIGSW, "cigsw"} ,
  	{ ARM64_DC_CIGVAC, "cigvac"} ,
  	{ ARM64_DC_CISW, "cisw"} ,
  	{ ARM64_DC_CIVAC, "civac"} ,
  	{ ARM64_DC_CSW, "csw"} ,
  	{ ARM64_DC_CVAC, "cvac"} ,
  	{ ARM64_DC_CVADP, "cvadp"} ,
  	{ ARM64_DC_CVAP, "cvap"} ,
  	{ ARM64_DC_CVAU, "cvau"} ,
  	{ ARM64_DC_GVA, "gva"} ,
  	{ ARM64_DC_GZVA, "gzva"} ,
  	{ ARM64_DC_IGDSW, "igdsw"} ,
  	{ ARM64_DC_IGDVAC, "igdvac"} ,
  	{ ARM64_DC_IGSW, "igsw"} ,
  	{ ARM64_DC_IGVAC, "igvac"} ,
  	{ ARM64_DC_ISW, "isw"} ,
  	{ ARM64_DC_IVAC, "ivac"} ,
  	{ ARM64_DC_ZVA, "zva"} ,
  	{ ARM64_IC_IALLUIS, "ialluis"} ,
  	{ ARM64_IC_IALLU, "iallu"} ,
  	{ ARM64_IC_IVAU, "ivau"} ,
};

arm64_sys_op AArch64_map_sys_op(const char *name)
{
	int result = name2id(sys_op_name_map, ARR_SIZE(sys_op_name_map), name);
	if (result == -1) {
		return ARM64_SYS_INVALID;
	}
	return result;
}

void arm64_op_addReg(MCInst *MI, int reg)
{
	if (MI->csh->detail) {
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = reg;
		MI->flat_insn->detail->arm64.op_count++;
	}
}

void arm64_op_addVectorArrSpecifier(MCInst * MI, int sp)
{
	if (MI->csh->detail) {
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count - 1].vas = sp;
	}
}

void arm64_op_addFP(MCInst *MI, float fp)
{
	if (MI->csh->detail) {
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_FP;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].fp = fp;
		MI->flat_insn->detail->arm64.op_count++;
	}
}

void arm64_op_addImm(MCInst *MI, int64_t imm)
{
	if (MI->csh->detail) {
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = (int)imm;
		MI->flat_insn->detail->arm64.op_count++;
	}
}

#ifndef CAPSTONE_DIET

// map instruction to its characteristics
typedef struct insn_op {
	unsigned int eflags_update;	// how this instruction update status flags
	uint8_t access[5];
} insn_op;

static const insn_op insn_ops[] = {
    {
         /* NULL item */
        0, { 0 }
    },

#include "AArch64MappingInsnOp.inc"
};

// given internal insn id, return operand access info
const uint8_t *AArch64_get_op_access(cs_struct *h, unsigned int id)
{
	int i = insn_find(insns, ARR_SIZE(insns), id, &h->insn_cache);
	if (i != 0) {
		return insn_ops[i].access;
	}

	return NULL;
}

void AArch64_reg_access(const cs_insn *insn,
		cs_regs regs_read, uint8_t *regs_read_count,
		cs_regs regs_write, uint8_t *regs_write_count)
{
	uint8_t i;
	uint8_t read_count, write_count;
	cs_arm64 *arm64 = &(insn->detail->arm64);

	read_count = insn->detail->regs_read_count;
	write_count = insn->detail->regs_write_count;

	// implicit registers
	memcpy(regs_read, insn->detail->regs_read, read_count * sizeof(insn->detail->regs_read[0]));
	memcpy(regs_write, insn->detail->regs_write, write_count * sizeof(insn->detail->regs_write[0]));

	// explicit registers
	for (i = 0; i < arm64->op_count; i++) {
		cs_arm64_op *op = &(arm64->operands[i]);
		switch((int)op->type) {
			case ARM64_OP_REG:
				if ((op->access & CS_AC_READ) && !arr_exist(regs_read, read_count, op->reg)) {
					regs_read[read_count] = (uint16_t)op->reg;
					read_count++;
				}
				if ((op->access & CS_AC_WRITE) && !arr_exist(regs_write, write_count, op->reg)) {
					regs_write[write_count] = (uint16_t)op->reg;
					write_count++;
				}
				break;
			case ARM_OP_MEM:
				// registers appeared in memory references always being read
				if ((op->mem.base != ARM64_REG_INVALID) && !arr_exist(regs_read, read_count, op->mem.base)) {
					regs_read[read_count] = (uint16_t)op->mem.base;
					read_count++;
				}
				if ((op->mem.index != ARM64_REG_INVALID) && !arr_exist(regs_read, read_count, op->mem.index)) {
					regs_read[read_count] = (uint16_t)op->mem.index;
					read_count++;
				}
				if ((arm64->writeback) && (op->mem.base != ARM64_REG_INVALID) && !arr_exist(regs_write, write_count, op->mem.base)) {
					regs_write[write_count] = (uint16_t)op->mem.base;
					write_count++;
				}
			default:
				break;
		}
	}

	*regs_read_count = read_count;
	*regs_write_count = write_count;
}
#endif

#endif
