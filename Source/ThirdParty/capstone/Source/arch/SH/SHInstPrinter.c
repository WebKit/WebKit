/* Capstone Disassembly Engine */
/* By Yoshinori Sato, 2022 */

#include <string.h>
#include "SHInstPrinter.h"


#ifndef CAPSTONE_DIET
static const char* const s_reg_names[] = {
	"invalid",
	"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
	"r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
	"r0_bank", "r1_bank", "r2_bank", "r3_bank",
	"r4_bank", "r5_bank", "r6_bank", "r7_bank",
	"fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7",
	"fr8", "fr9", "fr10", "fr11", "fr12", "fr13", "fr14", "fr15",
	"dr0", "dr2", "dr4", "dr6", "dr8", "dr10", "dr12", "dr14",
	"xd0", "xd2", "xd4", "xd6", "xd8", "xd10", "xd12", "xd14",
	"xf0", "xf1", "xf2", "xf3", "xf4", "xf5", "xf6", "xf7",
	"xf8", "xf9", "xf10", "xf11", "xf12", "xf13", "xf14", "xf15",
	"fv0", "fv4", "fv8", "fv12",
	"xmtrx",
	"pc", "pr", "mach", "macl",
	"sr", "gbr", "ssr", "spc", "sgr", "dbr", "vbr", "tbr",
	"rs", "re", "mod",
	"fpul", "fpscr",
	"x0", "x1", "y0", "y1", "a0", "a1", "a0g", "a1g", "m0", "m1",
	"dsr",
	"0x0", "0x1", "0x2", "0x3", "0x4", "0x5", "0x6", "0x7",
	"0x8", "0x9", "0xa", "0xb", "0xc", "0xd", "0xe", "0xf",
};
#endif

const char* SH_reg_name(csh handle, unsigned int reg)
{
#ifdef CAPSTONE_DIET
	return NULL;
#else
	if (reg >= ARR_SIZE(s_reg_names)) {
		return NULL;
	}
	return s_reg_names[(int)reg];
#endif
}


void SH_get_insn_id(cs_struct* h, cs_insn* insn, unsigned int id)
{
	insn->id = id; // These id's matches for sh
}

#ifndef CAPSTONE_DIET
static const char* const s_insn_names[] = {
	"unknwon",
	"add", "add", "addc", "addv", "and",
	"band", "bandnot", "bclr",
	"bf", "bf/s", "bld", "bldnot", "bor", "bornot", "bra", "braf",
	"bset", "bsr", "bsrf", "bst", "bt", "bt/s", "bxor",
	"clips", "clipu",
	"clrdmxy",
	"clrmac", "clrs", "clrt",
	"cmp/eq", "cmp/ge", "cmp/gt", "cmp/hi", "cmp/hs", "cmp/pl",
	"cmp/pz", "cmp/str",
	"div0s", "div0u", "div1",
	"divs", "divu",
	"dmuls.l", "dmulu.l",
	"dt",
	"exts", "exts", "extu", "extu",
	"fabs", "fadd", "fcmp/eq", "fcmp/gt",
	"fcnvds", "fcnvsd", "fdiv",
	"fipr", "fldi0", "fldi1", "flds", "float",
	"fmac", "fmov", "fmul", "fneg", "fpchg",
	"frchg", "fsca", "fschg", "fsqrt", "fsrra",
	"fsts", "fsub", "ftrc", "ftrv",
	"icbi",
	"jmp", "jsr", "jsr/n",
	"ldbank",
	"ldc", "ldrc", "ldre", "ldrs", "lds",
	"ldtlb",
	"mac.l", "mac.w",
	"mov", "mova", "movca", "movco", "movi20", "movi20s",
	"movli", "movml", "movmu", "movrt", "movt", "movu", "movua",
	"mul.l", "mulr", "muls", "mulu",
	"neg", "negc",
	"nop",
	"not", "nott",
	"ocbi", "ocbp", "ocbwb",
	"or",
	"pref", "prefi",
	"resbank",
	"rotcl", "rotcr", "rotl", "rotr",
	"rte", "rts", "rts/n", "rtv/n",
	"setdmx", "setdmy", "setrc",
	"sets", "sett",
	"shad", "shal", "shar", "shld", "shll",
	"shll16", "shll2", "shll8",
	"shlr", "shlr16", "shlr2", "shlr8",
	"sleep",
	"stbank",
	"stc", "sts",
	"sub", "subc", "subv",
	"swap", "swap",
	"synco",
	"tas",
	"trapa",
	"tst",
	"xor",
	"xtrct",
};
#endif

const char* SH_insn_name(csh handle, unsigned int id)
{
#ifdef CAPSTONE_DIET
	return NULL;
#else
	if (id >= ARR_SIZE(s_insn_names)) {
		return NULL;
	}
	return s_insn_names[id];
#endif
}

#ifndef CAPSTONE_DIET
#endif

#ifndef CAPSTONE_DIET
static void print_dsp_double(SStream *O, sh_info *info, int xy)
{
	char suffix_xy = 'x' + xy;
	int i;
	if (info->op.operands[xy].dsp.insn == SH_INS_DSP_NOP) {
		if ((info->op.operands[0].dsp.insn == SH_INS_DSP_NOP) &&
		    (info->op.operands[1].dsp.insn == SH_INS_DSP_NOP)) {
			SStream_concat(O, "nop%c", suffix_xy);
		}
	} else {
		SStream_concat(O, "mov%c", suffix_xy);
		switch(info->op.operands[xy].dsp.size) {
		case 16:
			SStream_concat0(O, ".w ");
			break;
		case 32:
			SStream_concat0(O, ".l ");
			break;
		}
		
		for (i = 0; i < 2; i++) {
			switch(info->op.operands[xy].dsp.operand[i]) {
			default:
				break;
			case SH_OP_DSP_REG_IND:
				SStream_concat(O, "@%s", s_reg_names[info->op.operands[xy].dsp.r[i]]);
				break;
			case SH_OP_DSP_REG_POST:
				SStream_concat(O, "@%s+", s_reg_names[info->op.operands[xy].dsp.r[i]]);
				break;
			case SH_OP_DSP_REG_INDEX:
				SStream_concat(O, "@%s+%s", s_reg_names[info->op.operands[xy].dsp.r[i]], s_reg_names[SH_REG_R8 + xy]);
				break;
			case SH_OP_DSP_REG:
				SStream_concat(O, "%s", s_reg_names[info->op.operands[xy].dsp.r[i]]);
				break;
			}
			if (i == 0)
				SStream_concat0(O, ",");
		}
	}
	if (xy == 0)
		SStream_concat0(O, " ");
}

static const char *s_dsp_insns[] = {
	"invalid",
	"nop",
	"mov",
	"pshl",
	"psha",
	"pmuls",
	"pclr_pmuls",
	"psub_pmuls",
	"padd_pmuls",
	"psubc",
	"paddc",
	"pcmp",
	"pabs",
	"prnd",
	"psub",
	"psub",
	"padd",
	"pand",
	"pxor",
	"por",
	"pdec",
	"pinc",
	"pclr",
	"pdmsb",
	"pneg", 
	"pcopy",
	"psts",
	"plds",
	"pswap",
	"pwad",
	"pwsb",
};

static void print_dsp(SStream *O, sh_info *info)
{
	int i;
	switch(info->op.op_count) {
	case 1:
		// single transfer
		SStream_concat0(O, "movs");
		switch(info->op.operands[0].dsp.size) {
		case 16:
			SStream_concat0(O, ".w ");
			break;
		case 32:
			SStream_concat0(O, ".l ");
			break;
		}
		for (i = 0; i < 2; i++) {
			switch(info->op.operands[0].dsp.operand[i]) {
			default:
				break;
			case SH_OP_DSP_REG_PRE:
				SStream_concat(O, "@-%s", s_reg_names[info->op.operands[0].dsp.r[i]]);
				break;
			case SH_OP_DSP_REG_IND:
				SStream_concat(O, "@%s", s_reg_names[info->op.operands[0].dsp.r[i]]);
				break;
			case SH_OP_DSP_REG_POST:
				SStream_concat(O, "@%s+", s_reg_names[info->op.operands[0].dsp.r[i]]);
				break;
			case SH_OP_DSP_REG_INDEX:
				SStream_concat(O, "@%s+%s", s_reg_names[info->op.operands[0].dsp.r[i]],s_reg_names[SH_REG_R8]);
				break;
			case SH_OP_DSP_REG:
				SStream_concat(O, "%s", s_reg_names[info->op.operands[0].dsp.r[i]]);
			}
			if (i == 0)
				SStream_concat0(O, ",");
		}
		break;
	case 2: // Double transfer
		print_dsp_double(O, info, 0);
		print_dsp_double(O, info, 1);
		break;
	case 3: // Parallel
		switch(info->op.operands[2].dsp.cc) {
		default:
			break;
		case SH_DSP_CC_DCT:
			SStream_concat0(O,"dct ");
			break;
		case SH_DSP_CC_DCF:
			SStream_concat0(O,"dcf ");
			break;
		}
		switch(info->op.operands[2].dsp.insn) {
		case SH_INS_DSP_PSUB_PMULS:
		case SH_INS_DSP_PADD_PMULS:
			switch(info->op.operands[2].dsp.insn) {
			default:
				break;
			case SH_INS_DSP_PSUB_PMULS:
				SStream_concat0(O, "psub ");
				break;
			case SH_INS_DSP_PADD_PMULS:
				SStream_concat0(O, "padd ");
				break;
			}
			for (i = 0; i < 6; i++) {
				SStream_concat(O, "%s", s_reg_names[info->op.operands[2].dsp.r[i]]);
				if ((i % 3) < 2)
					SStream_concat0(O, ",");
				if (i == 2)
					SStream_concat(O, " %s ", s_dsp_insns[SH_INS_DSP_PMULS]);
			}
			break;
		case SH_INS_DSP_PCLR_PMULS:
			SStream_concat0(O, s_dsp_insns[SH_INS_DSP_PCLR]);
			SStream_concat(O, " %s ", s_reg_names[info->op.operands[2].dsp.r[3]]);
			SStream_concat(O, "%s ", s_dsp_insns[SH_INS_DSP_PMULS]);
			for (i = 0; i < 3; i++) {
				SStream_concat(O, "%s", s_reg_names[info->op.operands[2].dsp.r[i]]);
				if (i < 2)
					SStream_concat0(O, ",");
			}
			break;
			
		default:
			SStream_concat0(O, s_dsp_insns[info->op.operands[2].dsp.insn]);
			SStream_concat0(O, " ");
			for (i = 0; i < 3; i++) {
				if (info->op.operands[2].dsp.r[i] == SH_REG_INVALID) {
					if (i == 0) {
						SStream_concat(O, "#%d", info->op.operands[2].dsp.imm);
					}
				} else
					SStream_concat(O, "%s", s_reg_names[info->op.operands[2].dsp.r[i]]);
				if (i < 2 && info->op.operands[2].dsp.r[i + 1] != SH_REG_INVALID)
					SStream_concat0(O, ",");
			}
		}
		
		if (info->op.operands[0].dsp.insn != SH_INS_DSP_NOP) {
			SStream_concat0(O, " ");
			print_dsp_double(O, info, 0);
		}
		if (info->op.operands[1].dsp.insn != SH_INS_DSP_NOP) {
			SStream_concat0(O, " ");
			print_dsp_double(O, info, 1);
		}
		break;
	}
}

static void PrintMemop(SStream *O, sh_op_mem *op) {
	switch(op->address) {
	case SH_OP_MEM_INVALID:
		break;
	case SH_OP_MEM_REG_IND:
		SStream_concat(O, "@%s", s_reg_names[op->reg]);
		break;
	case SH_OP_MEM_REG_POST:
		SStream_concat(O, "@%s+", s_reg_names[op->reg]);
		break;
	case SH_OP_MEM_REG_PRE:
		SStream_concat(O, "@-%s", s_reg_names[op->reg]);
		break;
	case SH_OP_MEM_REG_DISP:
		SStream_concat(O, "@(%d,%s)", op->disp, s_reg_names[op->reg]);
		break;
	case SH_OP_MEM_REG_R0:    /// <= R0 indexed
		SStream_concat(O, "@(%s,%s)",
				s_reg_names[SH_REG_R0],	s_reg_names[op->reg]);
		break;
	case SH_OP_MEM_GBR_DISP:  /// <= GBR based displaysment
		SStream_concat(O, "@(%d,%s)",
				op->disp, s_reg_names[SH_REG_GBR]);
		break;
	case SH_OP_MEM_GBR_R0:    /// <= GBR based R0 indexed
		SStream_concat(O, "@(%s,%s)",
				s_reg_names[SH_REG_R0], s_reg_names[SH_REG_GBR]);
		break;
	case SH_OP_MEM_PCR:       /// <= PC relative
		SStream_concat(O, "0x%x", op->disp);
		break;
	case SH_OP_MEM_TBR_DISP:  /// <= GBR based displaysment
		SStream_concat(O, "@@(%d,%s)",
				op->disp, s_reg_names[SH_REG_TBR]);
		break;
	}
}
#endif

void SH_printInst(MCInst* MI, SStream* O, void* PrinterInfo)
{
#ifndef CAPSTONE_DIET
	sh_info *info = (sh_info *)PrinterInfo;
	int i;
	int imm;

	if (MI->Opcode ==  SH_INS_DSP) {
		print_dsp(O, info);
		return;
	}
	
	SStream_concat0(O, (char*)s_insn_names[MI->Opcode]);
	switch(info->op.size) {
	case 8:
		SStream_concat0(O, ".b");
		break;
	case 16:
		SStream_concat0(O, ".w");
		break;
	case 32:
		SStream_concat0(O, ".l");
		break;
	case 64:
		SStream_concat0(O, ".d");
		break;
	}
	SStream_concat0(O, " ");
	for (i = 0; i < info->op.op_count; i++) {
		switch(info->op.operands[i].type) {
		case SH_OP_INVALID:
			break;
		case SH_OP_REG:
			SStream_concat0(O, s_reg_names[info->op.operands[i].reg]);
			break;
		case SH_OP_IMM:
			imm = info->op.operands[i].imm;
			SStream_concat(O, "#%d", imm);
			break;
		case SH_OP_MEM:
			PrintMemop(O, &info->op.operands[i].mem);
			break;
		}
		if (i < (info->op.op_count - 1)) {
			SStream_concat0(O, ",");
		}
	}
#endif
}

#ifndef CAPSTONE_DIET
static const name_map group_name_maps[] = {
	{ SH_GRP_INVALID , NULL },
	{ SH_GRP_JUMP, "jump" },
	{ SH_GRP_CALL, "call" },
	{ SH_GRP_INT,  "int" },
	{ SH_GRP_RET , "ret" },
	{ SH_GRP_IRET, "iret" },
        { SH_GRP_PRIVILEGE, "privilege" },
	{ SH_GRP_BRANCH_RELATIVE, "branch_relative" },
	{ SH_GRP_SH2, "SH2" },
	{ SH_GRP_SH2E, "SH2E" },
	{ SH_GRP_SH2DSP, "SH2-DSP" },
	{ SH_GRP_SH2A, "SH2A" },
	{ SH_GRP_SH2AFPU, "SH2A-FPU" },
	{ SH_GRP_SH3, "SH3" },
	{ SH_GRP_SH3DSP, "SH3-DSP" },
	{ SH_GRP_SH4, "SH4" },
	{ SH_GRP_SH4A, "SH4A" },
};
#endif

const char *SH_group_name(csh handle, unsigned int id)
{
#ifndef CAPSTONE_DIET
	return id2name(group_name_maps, ARR_SIZE(group_name_maps), id);
#else
	return NULL;
#endif
}

