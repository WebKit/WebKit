/* Capstone Disassembly Engine */
/* By Yoshinori Sato, 2022 */

#include <string.h>
#include <stdarg.h>
#include "../../cs_priv.h"
#include "../../MCInst.h"
#include "../../MCDisassembler.h"
#include "../../utils.h"
#include "SHDisassembler.h"
#include "capstone/sh.h"

#define regs_read(_detail, _reg)					\
	if (_detail)							\
		_detail->regs_read[_detail->regs_read_count++] = _reg
#define regs_write(_detail, _reg)					\
	if (_detail)							\
		_detail->regs_write[_detail->regs_write_count++] = _reg

enum direction {read, write};

static void regs_rw(cs_detail *detail, enum direction rw, sh_reg reg)
{
	switch(rw) {
	case read:
		regs_read(detail, reg);
		break;
	case write:
		regs_write(detail, reg);
		break;
	}
}

static void set_reg_n(sh_info *info, sh_reg reg, int pos,
		      enum direction rw, cs_detail *detail)
{
	info->op.operands[pos].type = SH_OP_REG;
	info->op.operands[pos].reg = reg;
	regs_rw(detail, rw, reg);
}

static void set_reg(sh_info *info, sh_reg reg, enum direction rw,
		    cs_detail *detail)
{
	set_reg_n(info, reg, info->op.op_count, rw, detail);
	info->op.op_count++;
}

static void set_mem_n(sh_info *info, sh_op_mem_type address,
		      sh_reg reg, uint32_t disp, int sz, int pos,
		      cs_detail *detail)
{
	info->op.operands[pos].type = SH_OP_MEM;
	info->op.operands[pos].mem.address = address;
	info->op.operands[pos].mem.reg = reg;
	info->op.operands[pos].mem.disp = disp;
	if (sz > 0)
		info->op.size = sz;
	switch (address) {
	case SH_OP_MEM_REG_POST:
	case SH_OP_MEM_REG_PRE:
		regs_write(detail, reg);
		break;
	case SH_OP_MEM_GBR_R0:
		regs_read(detail, SH_REG_GBR);
		regs_read(detail, SH_REG_R0);
		break;
	case SH_OP_MEM_REG_R0:
		regs_read(detail, SH_REG_R0);
		regs_read(detail, reg);
		break;
	case SH_OP_MEM_PCR:
		break;
	default:
		regs_read(detail, reg);
		break;
	}
}

static void set_mem(sh_info *info, sh_op_mem_type address,
		    sh_reg reg, uint32_t disp, int sz, cs_detail *detail)
{
	set_mem_n(info, address, reg, disp, sz, info->op.op_count, detail);
	info->op.op_count++;
}

static void set_imm(sh_info *info, int sign, uint64_t imm)
{
	info->op.operands[info->op.op_count].type = SH_OP_IMM;
	if (sign && imm >= 128)
		imm = -256 + imm;
	info->op.operands[info->op.op_count].imm = imm;
	info->op.op_count++;
}

static void set_groups(cs_detail *detail, int n, ...)
{
	va_list g;
	va_start(g, n);
	while (n > 0) {
		sh_insn_group grp;
		grp = va_arg(g, sh_insn_group);
		if (detail) {
			detail->groups[detail->groups_count] = grp;
			detail->groups_count++;
		}
		n--;
	}
	va_end(g);
}

enum {
	ISA_ALL = 1,
	ISA_SH2 = 2,
	ISA_SH2A = 3,
	ISA_SH3 = 4,
	ISA_SH4 = 5,
	ISA_SH4A = 6,
	ISA_MAX = 7,
};

static int isalevel(cs_mode mode)
{
	int level;
	mode >>= 1; /* skip endian */
	for (level = 2; level < ISA_MAX; level++) {
		if (mode & 1)
			return level;
		mode >>= 1;
	}
	return ISA_ALL;
}

enum co_processor {none, shfpu, shdsp};
typedef union reg_insn {
	sh_reg reg;
	sh_insn insn;
} reg_insn;
struct ri_list {
	int no;
	int /* reg_insn */ri;
	int level;
	enum co_processor cp;
};

static const struct ri_list ldc_stc_regs[] = {
		{0, SH_REG_SR, ISA_ALL, none},
		{1, SH_REG_GBR, ISA_ALL, none},
		{2, SH_REG_VBR, ISA_ALL, none},
		{3, SH_REG_SSR, ISA_SH3, none},
		{4, SH_REG_SPC, ISA_SH3, none},
		{5, SH_REG_MOD, ISA_ALL, shdsp},
		{6, SH_REG_RS, ISA_ALL, shdsp},
		{7, SH_REG_RE, ISA_ALL, shdsp},
		{8, SH_REG_R0_BANK, ISA_SH3, none},
		{9, SH_REG_R1_BANK, ISA_SH3, none},
		{10, SH_REG_R2_BANK, ISA_SH3, none},
		{11, SH_REG_R3_BANK, ISA_SH3, none},
		{12, SH_REG_R4_BANK, ISA_SH3, none},
		{13, SH_REG_R5_BANK, ISA_SH3, none},
		{14, SH_REG_R6_BANK, ISA_SH3, none},
		{15, SH_REG_R7_BANK, ISA_SH3, none},
		{-1, SH_REG_INVALID, ISA_ALL, none},
};

static sh_insn lookup_insn(const struct ri_list *list,
			     int no, cs_mode mode)
{
	int level = isalevel(mode);
	sh_insn error = SH_INS_INVALID;
	for(; list->no >= 0; list++) {
		if (no != list->no)
			continue;
		if (((level >= 0) && (level < list->level)) ||
		    ((level < 0) && (-(level) != list->level)))
			continue;
		if ((list->cp == none) ||
		    ((list->cp == shfpu) && (mode & CS_MODE_SHFPU)) ||
		    ((list->cp == shdsp) && (mode & CS_MODE_SHDSP))) {
			return list->ri;
		}
	}
	return error;
}

static sh_reg lookup_regs(const struct ri_list *list,
			     int no, cs_mode mode)
{
	int level = isalevel(mode);
	sh_reg error = SH_REG_INVALID;
	for(; list->no >= 0; list++) {
		if (no != list->no)
			continue;
		if (((level >= 0) && (level < list->level)) ||
		    ((level < 0) && (-(level) != list->level)))
			continue;
		if ((list->cp == none) ||
		    ((list->cp == shfpu) && (mode & CS_MODE_SHFPU)) ||
		    ((list->cp == shdsp) && (mode & CS_MODE_SHDSP))) {
			return list->ri;
		}
	}
	return error;
}

// #define lookup_regs(list, no, mode) ((reg_insn)(lookup(reg, list, no, mode).reg))
// #define lookup_insn(list, no, mode) ((sh_insn)(lookup(insn, list, no, mode).insn))

static sh_reg opSTCsrc(uint16_t code, MCInst *MI, cs_mode mode,
		       sh_info *info, cs_detail *detail)
{
	int s = (code >> 4) & 0x0f;
	int d = (code >> 8) & 0x0f;
	sh_reg sreg;
	MCInst_setOpcode(MI, SH_INS_STC);
	sreg = lookup_regs(ldc_stc_regs, s, mode);
	if (sreg != SH_REG_INVALID) {
		set_reg(info, sreg, read, detail);
		return SH_REG_R0 + d;
	} else {
		return SH_REG_INVALID;
	}
}

static bool opSTC(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		  sh_info *info, cs_detail *detail)
{
	sh_reg d;
	d = opSTCsrc(code, MI, mode, info, detail);
	if (d != SH_REG_INVALID) {
		set_reg(info, d, write, detail);
		return MCDisassembler_Success;
	} else {
		return MCDisassembler_Fail;
	}		
}

static bool op0xx3(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	int r = (code >> 8) & 0x0f;
	int insn_code = (code >> 4) & 0x0f;
	static const struct ri_list list[] = {
		{0, SH_INS_BSRF, ISA_SH2, none},
		{2, SH_INS_BRAF, ISA_SH2, none},
		{6, SH_INS_MOVLI, ISA_SH4A, none},
		{7, SH_INS_MOVCO, ISA_SH4A, none},
		{8, SH_INS_PREF, ISA_SH2A, none},
		{9, SH_INS_OCBI, ISA_SH4, none},
		{10, SH_INS_OCBP, ISA_SH4, none},
		{11, SH_INS_OCBWB, ISA_SH4, none},
		{12, SH_INS_MOVCA, ISA_SH4, none},
		{13, SH_INS_PREFI, ISA_SH4A, none},
		{14, SH_INS_ICBI, ISA_SH4A, none},
		{-1, SH_INS_INVALID, ISA_ALL, none},
	};
	sh_insn insn = lookup_insn(list, insn_code, mode);

	if (insn != SH_INS_INVALID) {
		MCInst_setOpcode(MI, insn);
		switch (insn_code) {
		case 0: /// bsrf Rn
		case 2: /// braf Rn
			set_reg(info, SH_REG_R0 + r, read, detail);
			if (detail)
				set_groups(detail, 2,
					   SH_GRP_JUMP,
					   SH_GRP_BRANCH_RELATIVE);
			break;
		case 8: /// pref @Rn
		case 9: /// ocbi @Rn
		case 10: /// ocbp @Rn
		case 11: /// ocbwb @Rn
		case 13: /// prefi @Rn
		case 14: /// icbi @Rn
			set_mem(info, SH_OP_MEM_REG_IND,
				SH_REG_R0 + r, 0, 0, detail);
			break;
		case 6: /// movli @Rn, R0
			set_mem(info, SH_OP_MEM_REG_IND,
				SH_REG_R0 + r, 0, 32, detail);
			set_reg(info, SH_REG_R0, write, detail);
			break;
		case 7: /// movco R0,@Rn
		case 12: /// movca R0,@Rn
			set_reg(info, SH_REG_R0, read, detail);
			set_mem(info, SH_OP_MEM_REG_IND,
				SH_REG_R0 + r, 0, 32, detail);
			break;
		}
		return MCDisassembler_Success;
	} else {
		return MCDisassembler_Fail;
	}		
}

#define nm(code, dir)				\
	int m, n;				\
	m = (code >> (4 * (dir + 1))) & 0x0f;	\
	n = (code >> (8 - 4 * dir)) & 0x0f

static bool opMOVx(uint16_t code, uint64_t address, MCInst *MI,
		   cs_mode mode, int size, sh_info *info, cs_detail *detail)
{
	int ad = ((code >> 10) & 0x3c) | ((code >> 2) & 0x03);
	enum direction rw;
	MCInst_setOpcode(MI, SH_INS_MOV);
	switch (ad) {
	case 0x01: /// mov.X Rs,@(R0, Rd)
	case 0x03: /// mov.X @(R0, Rs), Rd
		rw = (ad >> 1);
		{
			nm(code, rw);
			set_reg_n(info, SH_REG_R0 + m, rw, rw, detail);
			set_mem_n(info, SH_OP_MEM_REG_R0, SH_REG_R0 + n,
				  0, size, 1 - rw, detail);
			info->op.op_count = 2;
		}
		break;
	case 0x20: /// mov.X Rs,@-Rd
	case 0x60: /// mov.X @Rs+,Rd
		rw = (ad >> 6) & 1;
		{
			nm(code, rw);
			set_reg_n(info, SH_REG_R0 + m, rw, rw, detail);
			set_mem_n(info, SH_OP_MEM_REG_PRE, SH_REG_R0 + n,
				  0, size, 1 - rw, detail);
		}
		break;
	default:
		return MCDisassembler_Fail;
	}
	return MCDisassembler_Success;
}

static bool opMOV_B(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		    sh_info *info, cs_detail *detail)
{
	return opMOVx(code, address, MI, mode, 8, info, detail);
}

static bool opMOV_W(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		    sh_info *info, cs_detail *detail)
{
	return opMOVx(code, address, MI, mode, 16, info, detail);
}

static bool opMOV_L(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		    sh_info *info, cs_detail *detail)
{
	return opMOVx(code, address, MI, mode, 32, info, detail);
}

static bool opRRfn(uint16_t code, MCInst *MI, sh_insn insn, cs_mode mode,
		   int size, int level, sh_info *info, cs_detail *detail)
{
	int m = (code >> 4) & 0x0f;
	int n = (code >> 8) & 0x0f;
	if (level > isalevel(mode))
		return MCDisassembler_Fail;
	MCInst_setOpcode(MI, insn);
	set_reg(info, SH_REG_R0 + m, read, detail);
	set_reg(info, SH_REG_R0 + n, write, detail);
	info->op.size = size;
	return MCDisassembler_Success;
}

#define opRR(level, __insn, __size)					\
static bool op##__insn(uint16_t code, uint64_t address, MCInst *MI, \
		       cs_mode mode, sh_info *info, cs_detail *detail)	\
{									\
	return opRRfn(code, MI, SH_INS_##__insn, mode, __size, level,	\
		      info, detail);					\
}

/* mul.l - SH2 */
opRR(ISA_SH2, MUL_L, 0)

static bool op0xx8(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	int insn_code = (code >> 4) & 0xf;
	static const struct ri_list list[] = {
		{0, SH_INS_CLRT, ISA_ALL, none},
		{1, SH_INS_SETT, ISA_ALL, none},
		{2, SH_INS_CLRMAC, ISA_ALL, none},
		{3, SH_INS_LDTLB, ISA_SH3, none},
		{4, SH_INS_CLRS, ISA_SH3, none},
		{5, SH_INS_SETS, ISA_SH3, none},
		{6, SH_INS_NOTT, -(ISA_SH2A), none},
		{8, SH_INS_CLRDMXY, ISA_SH4A, shdsp},
		{9, SH_INS_SETDMX, ISA_SH4A, shdsp},
		{12, SH_INS_SETDMY, ISA_SH4A, shdsp},
		{-1, SH_INS_INVALID, ISA_ALL, none},
	};
	
	sh_insn insn = lookup_insn(list, insn_code, mode);
	if (code & 0x0f00)
		return MCDisassembler_Fail;
		
	if (insn != SH_INS_INVALID) {
		MCInst_setOpcode(MI, insn);
		return MCDisassembler_Success;
	} else {
		return MCDisassembler_Fail;
	}
}

static bool op0xx9(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	int insn_code = (code >> 4) & 0x0f;
	int r = (code >> 8) & 0x0f;
	static const struct ri_list list[] = {
		{0, SH_INS_NOP, ISA_ALL, none},
		{1, SH_INS_DIV0U, ISA_ALL, none},
		{2, SH_INS_MOVT, ISA_ALL, none},
		{3, SH_INS_MOVRT, -(ISA_SH2A), none},
		{-1, SH_INS_INVALID, ISA_ALL, none},
	};
	sh_insn insn = lookup_insn(list, insn_code, mode);
	if (insn != SH_INS_INVALID) {
		if (insn_code >= 2) {
			/// movt / movrt Rn
			set_reg(info, SH_REG_R0 + r, write, detail);
		} else if (r > 0) {
			insn = SH_INS_INVALID;
		}
	}
	if (insn != SH_INS_INVALID) {
		MCInst_setOpcode(MI, insn);
		return MCDisassembler_Success;
	} else {
		return MCDisassembler_Fail;
	}
}

static const struct ri_list sts_lds_regs[] = {
	{0, SH_REG_MACH, ISA_ALL, none},
	{1, SH_REG_MACL, ISA_ALL, none},
	{2, SH_REG_PR, ISA_ALL, none},
	{3, SH_REG_SGR, ISA_SH4, none},
	{4, SH_REG_TBR, -(ISA_SH2A), none},
	{5, SH_REG_FPUL, ISA_ALL, shfpu},
	{6, SH_REG_FPSCR, ISA_ALL, shfpu},
	{6, SH_REG_DSP_DSR, ISA_ALL, shdsp},
	{7, SH_REG_DSP_A0, ISA_ALL, shdsp},
	{8, SH_REG_DSP_X0, ISA_ALL, shdsp},
	{9, SH_REG_DSP_X1, ISA_ALL, shdsp},
	{10, SH_REG_DSP_Y0, ISA_ALL, shdsp},
	{11, SH_REG_DSP_Y1, ISA_ALL, shdsp},
	{15, SH_REG_DBR, ISA_SH4, none},
	{-1, SH_REG_INVALID, ISA_ALL, none},
};

static sh_reg opSTCSTS(uint16_t code, MCInst *MI, cs_mode mode, sh_info *info,
		       cs_detail *detail)
{
	int s = (code >> 4) & 0x0f;
	int d = (code >> 8) & 0x0f;
	sh_reg reg;
	sh_insn insn;

	reg = lookup_regs(sts_lds_regs, s, mode);
	if (reg != SH_REG_INVALID) {
		if (s == 3 || s == 4 || s == 15) {
			insn = SH_INS_STC;
		} else {
			insn = SH_INS_STS;
		}
		MCInst_setOpcode(MI, insn);
		set_reg(info, reg, read, detail);
		return SH_REG_R0 + d;
	} else {
		return SH_REG_INVALID;
	}
}

static bool op0xxa(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	sh_reg r = opSTCSTS(code, MI, mode, info, detail);
	if (r != SH_REG_INVALID) {
		set_reg(info, r, write, detail);
		return MCDisassembler_Success;
	} else
		return MCDisassembler_Fail;
}

static bool op0xxb(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	int insn_code = (code >> 4) & 0x0f;
	int r = (code >> 8) & 0x0f;
	static const struct ri_list list[] = {
		{0, SH_INS_RTS, ISA_ALL, none},
		{1, SH_INS_SLEEP, ISA_ALL, none},
		{2, SH_INS_RTE, ISA_ALL, none},
		{5, SH_INS_RESBANK, -(ISA_SH2A), none},
		{6, SH_INS_RTS_N, -(ISA_SH2A), none},
		{7, SH_INS_RTV_N, -(ISA_SH2A), none},
		{10, SH_INS_SYNCO, -(ISA_SH4A), none},
		{-1, SH_INS_INVALID, ISA_ALL, none},
	};

	sh_insn insn = lookup_insn(list, insn_code, mode);
	if (insn_code == 7) {
		set_reg(info, SH_REG_R0 + r, read, detail);
		regs_write(detail, SH_REG_R0);
	} else if (r > 0) {
		insn = SH_INS_INVALID;
	}
	if (insn != SH_INS_INVALID) {
		MCInst_setOpcode(MI, insn);
		return MCDisassembler_Success;
	} else {
		return MCDisassembler_Fail;
	}		
}

static bool opMAC(uint16_t code, sh_insn op, MCInst *MI, sh_info *info,
		  cs_detail *detail)
{
	nm(code, 0);
	MCInst_setOpcode(MI, op);
	set_mem(info, SH_OP_MEM_REG_POST, SH_REG_R0 + m, 0, 0, detail);
	set_mem(info, SH_OP_MEM_REG_POST, SH_REG_R0 + n, 0, 0, detail);
	return MCDisassembler_Success;
}

/// mac.l - sh2+
static bool opMAC_L(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		    sh_info *info, cs_detail *detail)
{
	if (isalevel(mode) < ISA_SH2)
		return MCDisassembler_Fail;
	return opMAC(code, SH_INS_MAC_L, MI, info, detail);
}

static bool opMAC_W(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		    sh_info *info, cs_detail *detail)
{
	return opMAC(code, SH_INS_MAC_W, MI, info, detail);
}

static bool opMOV_L_dsp(uint16_t code, uint64_t address, MCInst *MI,
			cs_mode mode, sh_info *info, cs_detail *detail)
{
	int dsp = (code & 0x0f) * 4;
	int rw = (code >> 14) & 1;
	nm(code, rw);
	MCInst_setOpcode(MI, SH_INS_MOV);
	set_mem_n(info, SH_OP_MEM_REG_DISP, SH_REG_R0 + n, dsp,
		  32, 1 - rw, detail);
	set_reg_n(info, SH_REG_R0 + m, rw, rw, detail);
	info->op.op_count = 2;
	return MCDisassembler_Success;
}

static bool opMOV_rind(uint16_t code, uint64_t address, MCInst *MI,
		       cs_mode mode, sh_info *info, cs_detail *detail)
{
	int sz = (code & 0x03);
	int rw = (code >> 14) & 1;
	nm(code, rw);
	MCInst_setOpcode(MI, SH_INS_MOV);
	sz = 8 << sz;
	set_mem_n(info, SH_OP_MEM_REG_IND, SH_REG_R0 + n, 0,
		  sz, 1 - rw, detail);
	set_reg_n(info, SH_REG_R0 + m, rw, rw, detail);
	info->op.op_count = 2;
	return MCDisassembler_Success;
}

static bool opMOV_rpd(uint16_t code, uint64_t address, MCInst *MI,
		      cs_mode mode, sh_info *info, cs_detail *detail)
{
	nm(code, 0);
	int sz = (code & 0x03);
	MCInst_setOpcode(MI, SH_INS_MOV);
	set_reg(info, SH_REG_R0 + m, read, detail);
	set_mem(info, SH_OP_MEM_REG_PRE, SH_REG_R0 + n, 0, 8 << sz, detail);
	return MCDisassembler_Success;
}

opRR(ISA_ALL, TST, 0)
opRR(ISA_ALL, AND, 0)
opRR(ISA_ALL, XOR, 0)
opRR(ISA_ALL, OR, 0)
opRR(ISA_ALL, CMP_STR, 0)
opRR(ISA_ALL, XTRCT, 0)
opRR(ISA_ALL, MULU_W, 16)
opRR(ISA_ALL, MULS_W, 16)
opRR(ISA_ALL, CMP_EQ, 0)
opRR(ISA_ALL, CMP_HI, 0)
opRR(ISA_ALL, CMP_HS, 0)
opRR(ISA_ALL, CMP_GE, 0)
opRR(ISA_ALL, CMP_GT, 0)
opRR(ISA_ALL, SUB, 0)
opRR(ISA_ALL, SUBC, 0)
opRR(ISA_ALL, SUBV, 0)
opRR(ISA_ALL, ADD_r, 0)
opRR(ISA_ALL, ADDC, 0)
opRR(ISA_ALL, ADDV, 0)
opRR(ISA_ALL, DIV0S, 0)
opRR(ISA_ALL, DIV1, 0)
/// DMULS / DMULU - SH2
opRR(ISA_SH2, DMULS_L, 0)
opRR(ISA_SH2, DMULU_L, 0)

static bool op4xx0(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	int insn_code = (code >> 4) & 0x0f;
	int r = (code >> 8) & 0x0f;
	static const struct ri_list list[] = {
		{0, SH_INS_SHLL, ISA_ALL, none},
		{1, SH_INS_DT, ISA_SH2, none},
		{2, SH_INS_SHAL, ISA_ALL, none},
		{8, SH_INS_MULR, -(ISA_SH2A), none},
		{15, SH_INS_MOVMU, -(ISA_SH2A), none},
		{-1, SH_INS_INVALID, ISA_ALL, none},
	};
	sh_insn insn = lookup_insn(list, insn_code,mode);
	if (insn != SH_INS_INVALID) {
		MCInst_setOpcode(MI, insn);
		if (insn_code < 8) {
			set_reg(info, SH_REG_R0 + r, write, detail);
		} else {
			switch(insn_code) {
			case 0x08:
				set_reg(info, SH_REG_R0, read, detail);
				set_reg(info, SH_REG_R0 + r, write, detail);
				break;
			case 0x0f:
				set_reg(info, SH_REG_R0 + r, read, detail);
				set_mem(info, SH_OP_MEM_REG_PRE, SH_REG_R15, 0, 32, detail);
				break;
			}
		}
		return MCDisassembler_Success;
	} else {
		return MCDisassembler_Fail;
	}
}

static bool op4xx1(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	int insn_code = (code >> 4) & 0x0f;
	int r = (code >> 8) & 0x0f;
	static const struct ri_list list[] = {
		{0, SH_INS_SHLR, ISA_ALL, none},
		{1, SH_INS_CMP_PZ, ISA_ALL, none},
		{2, SH_INS_SHAR, ISA_ALL, none},
		{8, SH_INS_CLIPU, -(ISA_SH2A), none},
		{9, SH_INS_CLIPS, -(ISA_SH2A), none},
		{14, SH_INS_STBANK, -(ISA_SH2A), none},
		{15, SH_INS_MOVML, -(ISA_SH2A), none},
		{-1, SH_INS_INVALID, ISA_ALL, none},
	};
	sh_insn insn = lookup_insn(list, insn_code,mode);
	if (insn != SH_INS_INVALID) {
		MCInst_setOpcode(MI, insn);
		switch(insn_code) {
		case 14:
			set_reg(info, SH_REG_R0, read, detail);
			set_mem(info, SH_OP_MEM_REG_IND, SH_REG_R0 + r, 0,
				0, detail);
			break;
		case 15:
			set_reg(info, SH_REG_R0 + r, read, detail);
			set_mem(info, SH_OP_MEM_REG_PRE, SH_REG_R15, 0,
				32, detail);
			break;
		default:
			set_reg(info, SH_REG_R0 + r, write, detail);
			if (insn_code >= 8)
				info->op.size = 8;
			break;
		}
		return MCDisassembler_Success;
	} else {
		return MCDisassembler_Fail;
	}
}

static bool op4xx2(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	sh_reg r = opSTCSTS(code, MI, mode, info, detail);
	if (r != SH_REG_INVALID) {
		set_mem(info, SH_OP_MEM_REG_PRE, r, 0, 32, detail);
		return MCDisassembler_Success;
	} else {
		return MCDisassembler_Fail;
	}
}

static bool opSTC_L(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		    sh_info *info, cs_detail *detail)
{
	sh_reg r = opSTCsrc(code, MI, mode, info, detail);
	if (r != SH_REG_INVALID) {
		set_mem(info, SH_OP_MEM_REG_PRE, r, 0, 32, detail);
		return MCDisassembler_Success;
	} else {
		return MCDisassembler_Fail;
	}		
}

static bool op4xx4(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	int r = (code >> 8) & 0x0f;
	int insn_code = (code >> 4) & 0x0f;
	static const struct ri_list list[] = {
		{0, SH_INS_ROTL, ISA_ALL, none},
		{1, SH_INS_SETRC, ISA_ALL, shdsp},
		{2, SH_INS_ROTCL, ISA_ALL, none},
		{3, SH_INS_LDRC, ISA_ALL, shdsp},
		{8, SH_INS_DIVU, -(ISA_SH2A), none},
		{9, SH_INS_DIVS, -(ISA_SH2A), none},
		{15, SH_INS_MOVMU, -(ISA_SH2A), none},
		{-1, SH_INS_INVALID, ISA_ALL, none},
	};
	sh_insn insn = lookup_insn(list, insn_code, mode);
	if (insn != SH_INS_INVALID) {
		MCInst_setOpcode(MI, insn);
		switch(insn_code) {
		case 8:
		case 9:
			set_reg(info, SH_REG_R0, read, detail);
			break;
		case 15:
			set_mem(info, SH_OP_MEM_REG_POST, SH_REG_R15, 0,
				32, detail);
			set_reg(info, SH_REG_R0 + r, read, detail);
			return MCDisassembler_Success;
		}
		set_reg(info, SH_REG_R0 + r, write, detail);
		return MCDisassembler_Success;
	} else {
		return MCDisassembler_Fail;
	}
}

static bool op4xx5(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	int r = (code >> 8) & 0x0f;
	enum direction rw = read;
	static const struct ri_list list[] = {
		{0, SH_INS_ROTR, ISA_ALL, none},
		{1, SH_INS_CMP_PL, ISA_ALL, none},
		{2, SH_INS_ROTCR, ISA_ALL, none},
		{8, SH_INS_CLIPU, -(ISA_SH2A), none},
		{9, SH_INS_CLIPS, -(ISA_SH2A), none},
		{14, SH_INS_LDBANK, -(ISA_SH2A), none},
		{15, SH_INS_MOVML, -(ISA_SH2A), none},
		{-1, SH_INS_INVALID, ISA_ALL, none},
	};
	int insn_code = (code >> 4) & 0x0f;
	sh_insn insn = lookup_insn(list, insn_code,mode);
	if (insn != SH_INS_INVALID) {
		MCInst_setOpcode(MI, insn);
		switch (insn_code) {
		case 0:
		case 2:
			rw = write;
			break;
		case 1:
			rw = read;
			break;
		case 8:
		case 9:
			info->op.size = 16;
			rw = write;
			break;
		case 0x0e:
			set_mem(info, SH_OP_MEM_REG_IND, SH_REG_R0 + r, 0,
				0, detail);
			set_reg(info, SH_REG_R0, write, detail);
			return MCDisassembler_Success;
		case 0x0f:
			set_mem(info, SH_OP_MEM_REG_POST, SH_REG_R15, 0,
				32, detail);
			set_reg(info, SH_REG_R0 + r, write, detail);
			return MCDisassembler_Success;
		}
		set_reg(info, SH_REG_R0 + r, rw, detail);
		return MCDisassembler_Success;
	} else {
		return MCDisassembler_Fail;
	}
}

static bool opLDCLDS(uint16_t code, MCInst *MI, cs_mode mode,
		     sh_info *info, cs_detail *detail)
{
	int d = (code >> 4) & 0x0f;
	sh_reg reg = lookup_regs(sts_lds_regs, d, mode);
	sh_insn insn;
	if (reg != SH_REG_INVALID) {
		if (d == 3 || d == 4 || d == 15) {
			insn = SH_INS_LDC;
		} else {
			insn = SH_INS_LDS;
		}
		MCInst_setOpcode(MI, insn);
		set_reg(info, reg, write, detail);
		return MCDisassembler_Success;
	} else {
		return MCDisassembler_Fail;
	}
}

static bool op4xx6(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	int r = (code >> 8) & 0x0f;
	set_mem(info, SH_OP_MEM_REG_POST, SH_REG_R0 + r, 0, 32, detail);
	return opLDCLDS(code, MI, mode, info, detail);
}

static bool opLDCdst(uint16_t code, MCInst *MI, cs_mode mode,
		     sh_info *info, cs_detail *detail)
{
	int d = (code >> 4) & 0x0f;
	sh_reg dreg = lookup_regs(ldc_stc_regs, d, mode);
	if (dreg == SH_REG_INVALID)
		return MCDisassembler_Fail;
	MCInst_setOpcode(MI, SH_INS_LDC);
	set_reg(info, dreg, write, detail);
	return MCDisassembler_Success;
}

static bool opLDC_L(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		    sh_info *info, cs_detail *detail)
{
	int s = (code >> 8) & 0x0f;
	set_mem(info,  SH_OP_MEM_REG_POST, SH_REG_R0 + s, 0, 32, detail);
	return opLDCdst(code, MI, mode, info, detail);
	
}

static bool op4xx8(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	int r = (code >> 8) & 0x0f;
	sh_insn insn[] = { SH_INS_SHLL2, SH_INS_SHLL8, SH_INS_SHLL16};
	int size = (code >> 4) & 0x0f;
	if (size >= ARR_SIZE(insn)) {
		return MCDisassembler_Fail;
	}
	MCInst_setOpcode(MI, insn[size]);
	set_reg(info, SH_REG_R0 + r, write, detail);
	return MCDisassembler_Success;
}

static bool op4xx9(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	int r = (code >> 8) & 0x0f;
	static const struct ri_list list[] = {
		{0, SH_INS_SHLR2, ISA_ALL, none},
		{1, SH_INS_SHLR8, ISA_ALL, none},
		{2, SH_INS_SHLR16, ISA_ALL, none},
		{10, SH_INS_MOVUA, -(ISA_SH4A), none},
		{14, SH_INS_MOVUA, -(ISA_SH4A), none},
		{-1, SH_INS_INVALID, ISA_ALL, none},
	};
	int op = (code >> 4) & 0x0f;
	sh_insn insn = lookup_insn(list, op, mode);
	sh_op_mem_type memop = SH_OP_MEM_INVALID;
	if (insn != SH_INS_INVALID) {
		MCInst_setOpcode(MI, insn);
		if (op < 8) {
			set_reg(info, SH_REG_R0 + r, write, detail);
		} else {
			memop = (op&4)?SH_OP_MEM_REG_POST:SH_OP_MEM_REG_IND;
			set_mem(info, memop, SH_REG_R0 + r, 0, 32, detail);
			set_reg(info, SH_REG_R0, write, detail);
		}
		return MCDisassembler_Success;
	} else {
		return MCDisassembler_Fail;
	}
}

static bool op4xxa(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	int r = (code >> 8) & 0x0f;
	set_reg(info, SH_REG_R0 + r, read, detail);
	return opLDCLDS(code, MI, mode, info, detail);
}

static bool op4xxb(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	int r = (code >> 8) & 0x0f;
	int insn_code = (code >> 4) & 0x0f;
	int sz = 0;
	int grp = SH_GRP_INVALID;
	sh_op_mem_type memop = SH_OP_MEM_INVALID;
	enum direction rw = read;
	static const struct ri_list list[] = {
		{0, SH_INS_JSR, ISA_ALL, none},
		{1, SH_INS_TAS, ISA_ALL, none},
		{2, SH_INS_JMP, ISA_ALL, none},
		{4, SH_INS_JSR_N, -(ISA_SH2A), none},
		{8, SH_INS_MOV, -(ISA_SH2A), none},
		{9, SH_INS_MOV, -(ISA_SH2A), none},
		{10, SH_INS_MOV, -(ISA_SH2A), none},
		{12, SH_INS_MOV, -(ISA_SH2A), none},
		{13, SH_INS_MOV, -(ISA_SH2A), none},
		{14, SH_INS_MOV, -(ISA_SH2A), none},
		{-1, SH_INS_INVALID, ISA_ALL, none},
	};
	sh_insn insn = lookup_insn(list, insn_code, mode);
	if (insn != SH_INS_INVALID) {
		MCInst_setOpcode(MI, insn);
		sz = 8 << ((code >> 4) & 3);
		switch (insn_code) {
		case 0:
		case 4:
			memop = SH_OP_MEM_REG_IND;
			grp = SH_GRP_CALL;
			break;
		case 1:
			memop = SH_OP_MEM_REG_IND;
			sz = 8;
			rw = write;
			break;
		case 2:
			insn = SH_INS_JMP;
			grp = SH_GRP_JUMP;
			break;
		case 8:
		case 9:
		case 10:
			memop = SH_OP_MEM_REG_POST;
			rw = read;
			break;
		case 12:
		case 13:
		case 14:
			memop = SH_OP_MEM_REG_PRE;
			rw = write;
			break;
		}
		if (grp != SH_GRP_INVALID) {
			set_mem(info, SH_OP_MEM_REG_IND, SH_REG_R0 + r, 0,
				0, detail);
			if (detail)
				set_groups(detail, 1, grp);
		} else {
			if (insn_code != 1) {
				set_reg_n(info, SH_REG_R0, rw, rw, detail);
				info->op.op_count++;
			}
			set_mem_n(info, memop, SH_REG_R0 + r, 0, sz,
				  1 - rw, detail);
			info->op.op_count++;
		}
		return MCDisassembler_Success;
	} else {
		return MCDisassembler_Fail;
	}
}

/* SHAD / SHLD - SH2A */
opRR(ISA_SH2A, SHAD, 0)
opRR(ISA_SH2A, SHLD, 0)
	
static bool opLDC(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		  sh_info *info, cs_detail *detail)
{
	int s = (code >> 8) & 0x0f;
	set_reg(info, SH_REG_R0 + s, read, detail);
	return opLDCdst(code, MI, mode, info, detail);
}

opRR(ISA_ALL, MOV, 0)

static bool opMOV_rpi(uint16_t code, uint64_t address, MCInst *MI,
		      cs_mode mode, sh_info *info, cs_detail *detail)
{
	int sz = (code & 0x03);
	nm(code, 0);
	MCInst_setOpcode(MI, SH_INS_MOV);
	set_mem(info, SH_OP_MEM_REG_POST, SH_REG_R0 + m, 0, 8 << sz, detail);
	set_reg(info, SH_REG_R0 + n, write, detail);
	return MCDisassembler_Success;
}

opRR(ISA_ALL, NOT, 0)
opRR(ISA_ALL, SWAP_B, 8)
opRR(ISA_ALL, SWAP_W, 16)
opRR(ISA_ALL, NEGC, 0)
opRR(ISA_ALL, NEG, 0)
opRR(ISA_ALL, EXTU_B, 8)
opRR(ISA_ALL, EXTU_W, 16)
opRR(ISA_ALL, EXTS_B, 8)
opRR(ISA_ALL, EXTS_W, 16)

static bool opADD_i(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		    sh_info *info, cs_detail *detail)
{
	int r = (code >> 8) & 0x0f;
	MCInst_setOpcode(MI, SH_INS_ADD);
	set_imm(info, 1, code & 0xff);
	set_reg(info, SH_REG_R0 + r, write, detail);
	return MCDisassembler_Success;
	
}
	
static bool opMOV_BW_dsp(uint16_t code, uint64_t address, MCInst *MI,
			 cs_mode mode, sh_info *info, cs_detail *detail)
{
	int dsp = (code & 0x0f);
	int r = (code >> 4) & 0x0f;
	int size = 1 + ((code >> 8) & 1);
	int rw = (code >> 10) & 1;
	MCInst_setOpcode(MI, SH_INS_MOV);
	set_mem_n(info, SH_OP_MEM_REG_DISP, SH_REG_R0 + r, dsp * size,
		  8 * size, 1 - rw, detail);
	set_reg_n(info, SH_REG_R0, rw, rw, detail);
	info->op.op_count = 2;
	return MCDisassembler_Success;
}

static bool opSETRC(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		    sh_info *info, cs_detail *detail)
{
	int imm = code & 0xff;
	if (!(mode & CS_MODE_SHDSP))
		return MCDisassembler_Fail;
	MCInst_setOpcode(MI, SH_INS_SETRC);
	set_imm(info, 0, imm);
	return MCDisassembler_Success;
}

static bool opJSR_N(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		    sh_info *info, cs_detail *detail)
{
	int dsp = code & 0xff;
	if (isalevel(mode) != ISA_SH2A)
		return MCDisassembler_Fail;
	MCInst_setOpcode(MI, SH_INS_JSR_N);
	set_mem(info, SH_OP_MEM_TBR_DISP, SH_REG_INVALID, dsp * 4, 0, detail);
	return MCDisassembler_Success;
}

#define boperand(_code, _op, _imm, _reg)		\
	int _op = (code >> 3) & 1;			\
	int _imm = code & 7;				\
	int _reg = (code >> 4) & 0x0f

static bool op86xx(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	static const sh_insn bop[] = {SH_INS_BCLR, SH_INS_BSET};
	boperand(code, op, imm, reg);
	if (isalevel(mode) != ISA_SH2A)
		return MCDisassembler_Fail;
	MCInst_setOpcode(MI, bop[op]);
	set_imm(info, 0, imm);
	set_reg(info, SH_REG_R0 + reg, write, detail);
	return MCDisassembler_Success;
}

static bool op87xx(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	static const sh_insn bop[] = {SH_INS_BST, SH_INS_BLD};
	boperand(code, op, imm, reg);
	if (isalevel(mode) != ISA_SH2A)
		return MCDisassembler_Fail;
	MCInst_setOpcode(MI, bop[op]);
	set_imm(info, 0, imm);
	set_reg(info, SH_REG_R0 + reg, op?read:write, detail);
	return MCDisassembler_Success;
}

static bool opCMP_EQi(uint16_t code, uint64_t address, MCInst *MI,
		      cs_mode mode, sh_info *info, cs_detail *detail)
{
	MCInst_setOpcode(MI, SH_INS_CMP_EQ);
	set_imm(info, 1, code & 0x00ff);
	set_reg(info, SH_REG_R0, read, detail);
	return MCDisassembler_Success;
}

#define opBranch(level, insn)						\
static bool op##insn(uint16_t code, uint64_t address, MCInst *MI, \
		     cs_mode mode, sh_info *info, cs_detail *detail)	\
{									\
	int dsp = code & 0x00ff;					\
	if (level > isalevel(mode))					\
		return MCDisassembler_Fail;				\
	if (dsp >= 0x80)						\
		dsp = -256 + dsp;					\
	MCInst_setOpcode(MI, SH_INS_##insn);				\
	set_mem(info, SH_OP_MEM_PCR, SH_REG_INVALID, address + 4 + dsp * 2, \
		0, detail);						\
	if (detail)							\
		set_groups(detail, 2, SH_GRP_JUMP, SH_GRP_BRANCH_RELATIVE); \
	return MCDisassembler_Success;					\
}

opBranch(ISA_ALL, BT)
opBranch(ISA_ALL, BF)
/* bt/s / bf/s - SH2 */
opBranch(ISA_SH2, BT_S)
opBranch(ISA_SH2, BF_S)

#define opLDRSE(insn)							\
static bool op##insn(uint16_t code, uint64_t address, MCInst *MI, \
		     cs_mode mode, sh_info *info, cs_detail *detail)	\
{									\
	int dsp = code & 0xff;						\
	if (!(mode & CS_MODE_SHDSP))					\
		return MCDisassembler_Fail;				\
	MCInst_setOpcode(MI, SH_INS_##insn);				\
	set_mem(info, SH_OP_MEM_PCR, SH_REG_INVALID, address + 4 + dsp * 2, \
		0, detail);						\
	return MCDisassembler_Success;\
}

static bool opLDRC(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	int imm = code & 0xff;
	if (!(mode & CS_MODE_SHDSP) || isalevel(mode) != ISA_SH4A)
		return MCDisassembler_Fail;
	MCInst_setOpcode(MI, SH_INS_LDRC);
	set_imm(info, 0, imm);
	return MCDisassembler_Success;
}

opLDRSE(LDRS)
opLDRSE(LDRE)
	
#define opImmR0(insn) \
static bool op##insn##_i(uint16_t code, uint64_t address, MCInst *MI, \
			 cs_mode mode, sh_info *info, cs_detail *detail) \
{									\
	MCInst_setOpcode(MI, SH_INS_##insn);				\
	set_imm(info, 0, code & 0xff);					\
	set_reg(info, SH_REG_R0, write, detail);			\
	return MCDisassembler_Success;					\
}

opImmR0(TST)
opImmR0(AND)
opImmR0(XOR)
opImmR0(OR)
	
#define opImmMem(insn) \
static bool op##insn##_B(uint16_t code, uint64_t address, MCInst *MI, \
			 cs_mode mode, sh_info *info, cs_detail *detail) \
{									\
	MCInst_setOpcode(MI, SH_INS_##insn);				\
	set_imm(info, 0, code & 0xff);					\
	set_mem(info, SH_OP_MEM_GBR_R0, SH_REG_R0, 0, 8, detail);	\
	return MCDisassembler_Success;					\
}

opImmMem(TST)
opImmMem(AND)
opImmMem(XOR)
opImmMem(OR)
	
static bool opMOV_pc(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		     sh_info *info, cs_detail *detail)
{
	int sz = 16 << ((code >> 14) & 1);
	int dsp = (code & 0x00ff) * (sz / 8);
	int r = (code >> 8) & 0x0f;
	MCInst_setOpcode(MI, SH_INS_MOV);
	if (sz == 32)
		address &= ~3;
	set_mem(info, SH_OP_MEM_PCR, SH_REG_INVALID, address + 4 + dsp,
		sz, detail);
	set_reg(info, SH_REG_R0 + r, write, detail);
	return MCDisassembler_Success;
}

#define opBxx(insn, grp)						\
static bool op##insn(uint16_t code, uint64_t address, MCInst *MI, \
		     cs_mode mode, sh_info *info, cs_detail *detail)	\
{									\
	int dsp = (code & 0x0fff);					\
	if (dsp >= 0x800)						\
		dsp = -0x1000 + dsp;					\
	MCInst_setOpcode(MI, SH_INS_##insn);				\
	set_mem(info, SH_OP_MEM_PCR, SH_REG_INVALID, address + 4 + dsp * 2, \
		0, detail);						\
	if (detail)							\
		set_groups(detail, 2, grp, SH_GRP_BRANCH_RELATIVE);	\
	return MCDisassembler_Success;					\
}

opBxx(BRA, SH_GRP_JUMP)
opBxx(BSR, SH_GRP_CALL)

static bool opMOV_gbr(uint16_t code, uint64_t address, MCInst *MI,
		      cs_mode mode, sh_info *info, cs_detail *detail)
{
	int sz = 8 << ((code >> 8) & 0x03);
	int dsp = (code & 0x00ff) * (sz / 8);
	int rw = (code >> 10) & 1;
	MCInst_setOpcode(MI, SH_INS_MOV);
	set_mem_n(info, SH_OP_MEM_GBR_DISP, SH_REG_GBR, dsp, sz,
		  1 - rw, detail);
	set_reg_n(info, SH_REG_R0, rw, rw, detail);
	info->op.op_count = 2;
	return MCDisassembler_Success;
}

static bool opTRAPA(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		    sh_info *info, cs_detail *detail)
{
	MCInst_setOpcode(MI, SH_INS_TRAPA);
	set_imm(info, 0, code & 0xff);
	if (detail)
		set_groups(detail, 1,  SH_GRP_INT);
	return MCDisassembler_Success;
}

static bool opMOVA(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	int dsp = (code & 0x00ff) * 4;
	MCInst_setOpcode(MI, SH_INS_MOVA);
	set_mem(info, SH_OP_MEM_PCR, SH_REG_INVALID, (address & ~3) + 4 + dsp,
		0, detail);
	set_reg(info, SH_REG_R0, write, detail);
	return MCDisassembler_Success;
}

static bool opMOV_i(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		    sh_info *info, cs_detail *detail)
{
	int imm = (code & 0x00ff);
	int r = (code >> 8) & 0x0f;
	MCInst_setOpcode(MI, SH_INS_MOV);
	set_imm(info, 1, imm);
	set_reg(info, SH_REG_R0 + r, write, detail);
	return MCDisassembler_Success;
}

/* FPU instructions */
#define opFRR(insn)							\
static bool op##insn(uint16_t code, uint64_t address, MCInst *MI, \
		     cs_mode mode, sh_info *info, cs_detail *detail)	\
{									\
	int m = (code >> 4) & 0x0f;					\
	int n = (code >> 8) & 0x0f;					\
	MCInst_setOpcode(MI, SH_INS_##insn);				\
	set_reg(info, SH_REG_FR0 + m, read, detail);			\
	set_reg(info, SH_REG_FR0 + n, write, detail);			\
	return MCDisassembler_Success;					\
}

#define opFRRcmp(insn)							\
static bool op##insn(uint16_t code, uint64_t address, MCInst *MI, \
		     cs_mode mode, sh_info *info, cs_detail *detail)	\
{									\
	int m = (code >> 4) & 0x0f;					\
	int n = (code >> 8) & 0x0f;					\
	MCInst_setOpcode(MI, SH_INS_##insn);				\
	set_reg(info, SH_REG_FR0 + m, read, detail);			\
	set_reg(info, SH_REG_FR0 + n, read, detail);			\
	return MCDisassembler_Success;					\
}

opFRR(FADD)
opFRR(FSUB)
opFRR(FMUL)
opFRR(FDIV)
opFRRcmp(FCMP_EQ)
opFRRcmp(FCMP_GT)

static bool opFMOVm(MCInst *MI, enum direction rw, uint16_t code,
		    sh_op_mem_type address, sh_info *info, cs_detail *detail)
{
	nm(code, (1 - rw));
	MCInst_setOpcode(MI, SH_INS_FMOV);
	set_mem_n(info, address, SH_REG_R0 + m, 0, 0, 1 - rw, detail);
	set_reg_n(info, SH_REG_FR0 + n, rw, rw, detail);
	info->op.op_count = 2;
	return MCDisassembler_Success;
}

static bool opfxx6(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	return opFMOVm(MI, write, code, SH_OP_MEM_REG_R0, info, detail);
}
	
static bool opfxx7(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	return opFMOVm(MI, read, code, SH_OP_MEM_REG_R0, info, detail);
}
	
static bool opfxx8(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	return opFMOVm(MI, write, code, SH_OP_MEM_REG_IND, info, detail);
}
	
static bool opfxx9(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	return opFMOVm(MI, write, code, SH_OP_MEM_REG_POST, info, detail);
}
	
static bool opfxxa(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	return opFMOVm(MI, read, code, SH_OP_MEM_REG_IND, info, detail);
}
	
static bool opfxxb(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	return opFMOVm(MI, read, code, SH_OP_MEM_REG_PRE, info, detail);
}
	
static bool opFMOV(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	nm(code, 0);
	MCInst_setOpcode(MI, SH_INS_FMOV);
	set_reg(info, SH_REG_FR0 + m, read, detail);
	set_reg(info, SH_REG_FR0 + n, write, detail);
	return MCDisassembler_Success;
}
	
static bool opfxxd(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	int fr = (code >> 8) & 0x0f;
	int dr = (code >> 9) & 0x07;
	int fvn = (code >> 10) & 0x03;
	int fvm = (code >> 8) & 0x03;
	sh_insn insn = SH_INS_INVALID;
	sh_reg s, d;
	static const struct ri_list list[] = {
		{0, SH_INS_FSTS, ISA_ALL, shfpu},
		{1, SH_INS_FLDS, ISA_ALL, shfpu},
		{2, SH_INS_FLOAT, ISA_ALL, shfpu},
		{3, SH_INS_FTRC, ISA_ALL, shfpu},
		{4, SH_INS_FNEG, ISA_ALL, shfpu},
		{5, SH_INS_FABS, ISA_ALL, shfpu},
		{6, SH_INS_FSQRT, ISA_ALL, shfpu},
		{7, SH_INS_FSRRA, ISA_ALL, shfpu},
		{8, SH_INS_FLDI0, ISA_ALL, shfpu},
		{9, SH_INS_FLDI1, ISA_ALL, shfpu},
		{10, SH_INS_FCNVSD, ISA_SH4A, shfpu},
		{11, SH_INS_FCNVDS, ISA_SH4A, shfpu},
		{14, SH_INS_FIPR, ISA_SH4A, shfpu},
		{-1, SH_INS_INVALID, ISA_ALL, none},
	};
	static const sh_insn chg[] = {
		SH_INS_FSCHG, SH_INS_FPCHG, SH_INS_FRCHG, SH_INS_INVALID
	};
	insn = lookup_insn(list, (code >> 4) & 0x0f, mode);
	s = d = SH_REG_FPUL;
	if (insn != SH_INS_INVALID) {
		switch((code >> 4) & 0x0f) {
		case 0:
		case 2:
			d = SH_REG_FR0 + fr;
			break;
		case 1:
		case 3:
			s = SH_REG_FR0 + fr;
			break;
		case 10:
			d = SH_REG_DR0 + dr;
			break;
		case 11:
			s = SH_REG_DR0 + dr;
			break;
		case 14:
			s = SH_REG_FV0 + fvm;
			d = SH_REG_FV0 + fvn;
			break;
		default:
			s = SH_REG_FR0 + fr;
			d = SH_REG_INVALID;
			break;
		}
	} else if ((code & 0x00f0) == 0x00f0) {
		if ((code & 0x01ff) == 0x00fd) {
			insn = SH_INS_FSCA;
			d = SH_REG_DR0 + dr;
		}
		if ((code & 0x03ff) == 0x01fd) {
			insn = SH_INS_FTRV;
			s = SH_REG_XMATRX;
			d = SH_REG_FV0 + fvn;
		}
		if ((code & 0x03ff) == 0x03fd) {
			insn = chg[(code >> 10) & 3];
			s = d = SH_REG_INVALID;
		}
	}
	if (insn == SH_INS_INVALID) {
		return MCDisassembler_Fail;
	}
	MCInst_setOpcode(MI, insn);
	if (s != SH_REG_INVALID) {
		set_reg(info, s, read, detail);
	}
	if (d != SH_REG_INVALID) {
		set_reg(info, d, write, detail);
	}
	return MCDisassembler_Success;
}

static bool opFMAC(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode,
		   sh_info *info, cs_detail *detail)
{
	int m = (code >> 4) & 0x0f;
	int n = (code >> 8) & 0x0f;
	MCInst_setOpcode(MI, SH_INS_FMAC);
	set_reg(info, SH_REG_FR0, read, detail);
	set_reg(info, SH_REG_FR0 + m, read, detail);
	set_reg(info, SH_REG_FR0 + n, write, detail);
	return MCDisassembler_Success;
}

#include "SHInsnTable.inc"

static bool decode_long(uint32_t code, uint64_t address, MCInst *MI,
			sh_info *info, cs_detail *detail)
{
	uint32_t imm;
	sh_insn insn = SH_INS_INVALID;
	int m,n;
	int dsp;
	int sz;
	static const sh_insn bop[] = {
		SH_INS_BCLR, SH_INS_BSET, SH_INS_BST, SH_INS_BLD,
		SH_INS_BAND, SH_INS_BOR, SH_INS_BXOR, SH_INS_INVALID,
		SH_INS_INVALID, SH_INS_INVALID, SH_INS_INVALID, SH_INS_BLDNOT,
		SH_INS_BANDNOT, SH_INS_BORNOT, SH_INS_INVALID, SH_INS_INVALID,
	};
	switch (code >> 28) {
	case 0x0:
		imm = ((code >> 4) & 0x000f0000) | (code & 0xffff);
		n = (code >> 24) & 0x0f;
		if (code & 0x00010000) {
			// movi20s #imm,
			imm <<= 8;
			if (imm & (1 << (28 - 1)))
				imm |= ~((1 << 28) - 1);
			insn = SH_INS_MOVI20S;
		} else {
			// MOVI20
			if (imm & (1 << (28 - 1)))
				imm |= ~((1 << 20) - 1);
			insn = SH_INS_MOVI20;
		}
		set_imm(info, 0, imm);
		set_reg(info, SH_REG_R0 + n, write, detail);
		break;
	case 0x3:
		n = (code >> 24) & 0x0f;
		m = (code >> 20) & 0x0f;
		sz = (code >> 12) & 0x03;
		dsp = code & 0xfff;
		if (!(code & 0x80000)) {
			dsp <<= sz;
			switch((code >> 14) & 0x3) {
			case 0: // mov.[bwl] Rm,@(disp,Rn)
				// fmov.s DRm,@(disp,Rn)
				if (sz < 3) {
					insn = SH_INS_MOV;
					set_reg(info, SH_REG_R0 + m,
						read, detail);
				} else {
					insn = SH_INS_FMOV;
					set_reg(info, SH_REG_DR0 + (m >> 1),
						read, detail);
				}
				set_mem(info, SH_OP_MEM_REG_DISP,
					SH_REG_R0 + n, dsp, 8 << sz, detail);
				break;
			case 1: // mov.[bwl] @(disp,Rm),Rn
				// fmov.s @(disp,Rm),DRn
				set_mem(info, SH_OP_MEM_REG_DISP,
					SH_REG_R0 + m, dsp, 8 << sz, detail);
				if (sz < 3) {
					insn = SH_INS_MOV;
					set_reg(info, SH_REG_R0 + n,
						write, detail);
				} else {
					insn = SH_INS_FMOV;
					set_reg(info, SH_REG_DR0 + (n >> 1),
						write, detail);
				}
				break;
			case 2: // movu.[bwl] @(disp,Rm),Rn
				if (sz < 2) {
					insn = SH_INS_MOVU;
					set_mem(info, SH_OP_MEM_REG_DISP,
						SH_REG_R0 + m, dsp,
						8 << sz, detail);
					set_reg(info, SH_REG_R0 + n,
						write, detail);
				}
				break;
			}
		} else {
			// bitop #imm,@(disp,Rn)
			insn = bop[(code >> 12) & 0x0f];
			set_imm(info, 0, m & 7);
			set_mem(info, SH_OP_MEM_REG_DISP, SH_REG_R0 + n,
				dsp, 8, detail);
		}
	}
	if (insn != SH_INS_INVALID) {
		MCInst_setOpcode(MI, insn);
		return MCDisassembler_Success;
	} else {
		return MCDisassembler_Fail;
	}
}

static const sh_reg dsp_areg[2][4] = {
	{SH_REG_R4, SH_REG_R0, SH_REG_R5, SH_REG_R1},
	{SH_REG_R6, SH_REG_R7, SH_REG_R2, SH_REG_R3},
};

static bool decode_dsp_xy(sh_info *info, int xy, uint16_t code,
			  cs_detail *detail)
{
	int a = (code >> 8) & 3;
	int d = (code >> 6) & 3;
	int dir;
	int sz;
	int op;

	static const sh_reg dreg[4][4] = {
		{SH_REG_DSP_A0, SH_REG_DSP_X0, SH_REG_DSP_A1, SH_REG_DSP_X1},
		{SH_REG_DSP_A0, SH_REG_DSP_A1, SH_REG_DSP_Y0, SH_REG_DSP_Y1},
		{SH_REG_DSP_X0, SH_REG_DSP_Y0, SH_REG_DSP_X1, SH_REG_DSP_Y1},
		{SH_REG_DSP_Y0, SH_REG_DSP_Y1, SH_REG_DSP_X0, SH_REG_DSP_X1},
	};
	
	if (xy) {
		op = code & 3;
		dir = 1 - ((code >> 4) & 1);
		sz = (code >> 5) & 1;
		if (code & 0x0c) {
			info->op.operands[xy].dsp.insn = SH_INS_DSP_NOP;
			return MCDisassembler_Success;
		}
	} else {
		op = (code >> 2) & 3;
		dir = 1 - ((code >> 5) & 1);
		sz = (code >> 4) & 1;
		if (code & 0x03) {
			info->op.operands[xy].dsp.insn = SH_INS_DSP_NOP;
			return MCDisassembler_Success;
		}
	}
	info->op.operands[xy].dsp.size = 16 << sz;
	info->op.operands[xy].dsp.insn = SH_INS_DSP_MOV;
	info->op.operands[xy].dsp.operand[1 - dir] =
		SH_OP_DSP_REG_IND + (op - 1);
	info->op.operands[xy].dsp.operand[dir] = SH_OP_DSP_REG;
	info->op.operands[xy].dsp.r[1 - dir] = dsp_areg[xy][a];
	info->op.operands[xy].dsp.size = 16 << sz;
	regs_rw(detail, dir,
		info->op.operands[xy].dsp.r[dir] = dreg[xy * 2 + dir][d]);
	switch(op) {
	case 0x03:
		regs_read(detail, SH_REG_R8 + xy);
		// Fail through
	case 0x02:
		regs_write(detail, dsp_areg[xy][a]);
		break;
	case 0x01:
		regs_read(detail, dsp_areg[xy][a]);
		break;
	default:
		return MCDisassembler_Fail;
	}
	return MCDisassembler_Success;
}

static bool set_dsp_move_d(sh_info *info, int xy, uint16_t code, cs_mode mode, cs_detail *detail)
{
	int a;
	int d;
	int dir;
	int op;
	static const sh_reg base[] = {SH_REG_DSP_A0, SH_REG_DSP_X0};
	switch (xy) {
	case 0:
		op = (code >> 2) & 3;
		dir = 1 - ((code >> 5) & 1);
		d = (code >> 7) & 1;
		a = (code >> 9) & 1;
		break;
	case 1:
		op = (code >> 0) & 3;
		dir = 1 - ((code >> 4) & 1);
		d = (code >> 6) & 1;
		a = (code >> 8) & 1;
		break;
	}
	if (op == 0x00) {
		if ((a || d || dir) && !(code & 0x0f))
			return MCDisassembler_Fail;
		info->op.operands[xy].dsp.insn = SH_INS_DSP_NOP;
	} else {
		info->op.operands[xy].dsp.insn = SH_INS_DSP_MOV;
		info->op.operands[xy].dsp.operand[1 - dir] =
			SH_OP_DSP_REG_IND + (op - 1);
		info->op.operands[xy].dsp.operand[dir] = SH_OP_DSP_REG;
		info->op.operands[xy].dsp.r[1 - dir] = SH_REG_R4 + xy * 2 + a;
		info->op.operands[xy].dsp.size = 16;
		regs_rw(detail, dir,
			info->op.operands[xy].dsp.r[dir] =
			base[dir] + d + dir?(xy * 2):0);
		switch(op) {
		case 0x03:
			regs_read(detail, SH_REG_R8 + a);
			// Fail through
		case 0x02:
			regs_write(detail, SH_REG_R4 + xy * 2 + a);
			break;
		case 0x01:
			regs_read(detail, SH_REG_R4 + xy * 2 + a);
			break;
		}
	}
	return MCDisassembler_Success;
}

static bool decode_dsp_d(const uint16_t code, MCInst *MI, cs_mode mode,
			 sh_info *info, cs_detail *detail)
{
	bool ret, dsp_long;
	MCInst_setOpcode(MI, SH_INS_DSP);
	if ((code & 0x3ff) == 0) {
		info->op.operands[0].dsp.insn = 
			info->op.operands[1].dsp.insn = SH_INS_DSP_NOP;
		info->op.op_count = 2;
		return MCDisassembler_Success;
	}
	dsp_long = false;
	if (isalevel(mode) == ISA_SH4A) {
		if (!(code & 0x03) && (code & 0x0f) >= 0x04) {
			ret = decode_dsp_xy(info, 0, code, detail);
			ret &= set_dsp_move_d(info, 1, code, mode, detail);
			dsp_long |= true;
		}
		if ((code & 0x0f) <= 0x03 && (code & 0xff)) {
			ret = decode_dsp_xy(info, 1, code, detail);
			ret &= set_dsp_move_d(info, 0, code, mode, detail);
			dsp_long |= true;
		}
	}
	if (!dsp_long) {
		/* X op */
		ret = set_dsp_move_d(info, 0, code, mode, detail);
		/* Y op */
		ret &= set_dsp_move_d(info, 1, code, mode, detail);
	}

	info->op.op_count = 2;
	return ret;
}

static bool decode_dsp_s(const uint16_t code, MCInst *MI,
			 sh_info *info, cs_detail *detail)
{
	int d = code & 1;
	int s = (code >> 1) & 1;
	int opr = (code >> 2) & 3;
	int as = (code >> 8) & 3;
	int ds = (code >> 4) & 0x0f;
	static const sh_reg regs[] = {
		SH_REG_DSP_RSV0, SH_REG_DSP_RSV1, SH_REG_DSP_RSV2,
		SH_REG_DSP_RSV3,
		SH_REG_DSP_RSV4, SH_REG_DSP_A1, SH_REG_DSP_RSV6, SH_REG_DSP_A0,
		SH_REG_DSP_X0, SH_REG_DSP_X1, SH_REG_DSP_Y0, SH_REG_DSP_Y1,
		SH_REG_DSP_M0, SH_REG_DSP_A1G, SH_REG_DSP_M1, SH_REG_DSP_A0G,
	};

	if (regs[ds] == SH_REG_INVALID)
		return MCDisassembler_Fail;
		
	MCInst_setOpcode(MI, SH_INS_DSP);
	info->op.operands[0].dsp.insn = SH_INS_DSP_MOV;
	info->op.operands[0].dsp.operand[1 - d] = SH_OP_DSP_REG;
	info->op.operands[0].dsp.operand[d] = SH_OP_DSP_REG_PRE + opr;
	info->op.operands[0].dsp.r[1 - d] = regs[ds];
	info->op.operands[0].dsp.r[d] = SH_REG_R2 + ((as < 2)?(as+2):(as-2));
	switch (opr) {
	case 3:
		regs_read(detail, SH_REG_R8);
		/* Fail through */
	case 1:
		regs_read(detail, info->op.operands[0].dsp.r[d]);
		break;
	case 0:
	case 2:
		regs_write(detail,  info->op.operands[0].dsp.r[d]);
	}
	regs_rw(detail, d, regs[ds]);
	info->op.operands[0].dsp.size = 16 << s;
	info->op.op_count = 1;
	return MCDisassembler_Success;
}

static const sh_reg dsp_reg_sd[6][4] = {
	{SH_REG_DSP_X0, SH_REG_DSP_X1, SH_REG_DSP_Y0, SH_REG_DSP_A1},
	{SH_REG_DSP_Y0, SH_REG_DSP_Y1, SH_REG_DSP_X0, SH_REG_DSP_A1},
	{SH_REG_DSP_X0, SH_REG_DSP_X1, SH_REG_DSP_A0, SH_REG_DSP_A1},
	{SH_REG_DSP_Y0, SH_REG_DSP_Y1, SH_REG_DSP_M0, SH_REG_DSP_M1},
	{SH_REG_DSP_M0, SH_REG_DSP_M1, SH_REG_DSP_A0, SH_REG_DSP_A1},
	{SH_REG_DSP_X0, SH_REG_DSP_Y0, SH_REG_DSP_A0, SH_REG_DSP_A1},
};
typedef enum {f_se, f_sf, f_sx, f_sy, f_dg, f_du} dsp_reg_opr;
static void set_reg_dsp_read(sh_info *info, int pos, dsp_reg_opr f, int r,
			     cs_detail *detail)
{
	info->op.operands[2].dsp.r[pos] = dsp_reg_sd[f][r];
	regs_read(detail, dsp_reg_sd[f][r]);
}	

static void set_reg_dsp_write_gu(sh_info *info, int pos, dsp_reg_opr f, int r,
				 cs_detail *detail)
{
	info->op.operands[2].dsp.r[pos] = dsp_reg_sd[f][r];
	regs_write(detail, dsp_reg_sd[f][r]);
}	

static const sh_reg regs_dz[] = {
	SH_REG_DSP_RSV0, SH_REG_DSP_RSV1, SH_REG_DSP_RSV2, SH_REG_DSP_RSV3,
	SH_REG_DSP_RSV4, SH_REG_DSP_A1, SH_REG_DSP_RSV6, SH_REG_DSP_A0,
	SH_REG_DSP_X0, SH_REG_DSP_X1, SH_REG_DSP_Y0, SH_REG_DSP_Y1,
	SH_REG_DSP_M0, SH_REG_DSP_A1G, SH_REG_DSP_M1, SH_REG_DSP_A0G,
};

static void set_reg_dsp_write_z(sh_info *info, int pos, int r,
				cs_detail *detail)
{
	info->op.operands[2].dsp.r[pos] = regs_dz[r];
	regs_write(detail, regs_dz[r]);
}	

static bool dsp_op_cc_3opr(uint32_t code, sh_info *info, sh_dsp_insn insn,
			   sh_dsp_insn_type insn2, cs_detail *detail)
{
	info->op.operands[2].dsp.cc = (code >> 8) & 3;
	if (info->op.operands[2].dsp.cc > 0) {
		info->op.operands[2].dsp.insn = insn;
	} else {
		if (insn2 != SH_INS_DSP_INVALID)
			info->op.operands[2].dsp.insn = (sh_dsp_insn) insn2;
		else
			return MCDisassembler_Fail;
	}
	if (info->op.operands[2].dsp.insn != SH_INS_DSP_PSUBr) {
		set_reg_dsp_read(info, 0, f_sx, (code >> 6) & 3, detail);
		set_reg_dsp_read(info, 1, f_sy, (code >> 4) & 3, detail);
	} else {
		set_reg_dsp_read(info, 1, f_sx, (code >> 6) & 3, detail);
		set_reg_dsp_read(info, 0, f_sy, (code >> 4) & 3, detail);
	}
	set_reg_dsp_write_z(info, 2, code & 0x0f, detail);
	info->op.op_count = 3;
	return MCDisassembler_Success;
}

static bool dsp_op_cc_2opr(uint32_t code, sh_info *info, sh_dsp_insn insn,
			   int xy, int b, cs_detail *detail)
{
	if (((code >> 8) & 3) == 0)
		return MCDisassembler_Fail;
	info->op.operands[2].dsp.insn = (sh_dsp_insn) insn;
	set_reg_dsp_read(info, 0, xy, (code >> b) & 3, detail);
	set_reg_dsp_write_z(info, 2, code & 0x0f, detail);
	info->op.operands[2].dsp.cc = (code >> 8) & 3;
	info->op.op_count = 3;
	return MCDisassembler_Success;
}
	
static bool dsp_op_cc0_2opr(uint32_t code, sh_info *info, sh_dsp_insn insn,
			    int xy, int b, cs_detail *detail)
{
	info->op.operands[2].dsp.insn = (sh_dsp_insn) insn;
	set_reg_dsp_read(info, 0, xy, (code >> b) & 3, detail);
	set_reg_dsp_write_z(info, 2, code & 0x0f, detail);
	info->op.operands[2].dsp.cc = (code >> 8) & 3;	
	if (info->op.operands[2].dsp.cc == 1)
		return MCDisassembler_Fail;
	if (info->op.operands[2].dsp.cc == 0)
		info->op.operands[2].dsp.cc = SH_DSP_CC_NONE;
	info->op.op_count = 3;
	return MCDisassembler_Success;
}
	
static bool decode_dsp_3op(const uint32_t code, sh_info *info,
			   cs_detail *detail)
{
	int cc = (code >> 8) & 3;
	int sx = (code >> 6) & 3;
	int sy = (code >> 4) & 3;
	int dz = (code >> 0) & 0x0f;

	if ((code & 0xef00) == 0x8000)
		return MCDisassembler_Fail;
	switch((code >> 10) & 0x1f) {
	case 0x00:
		return dsp_op_cc_3opr(code, info,
				      SH_INS_DSP_PSHL, SH_INS_DSP_INVALID,
				      detail);
	case 0x01:
		if (cc == 0) {
			info->op.operands[2].dsp.insn = SH_INS_DSP_PCMP;
			set_reg_dsp_read(info, 0, f_sx, sx, detail);
			set_reg_dsp_read(info, 1, f_sy, sy, detail);
			info->op.op_count = 3;
			return MCDisassembler_Success;
		} else {
			return dsp_op_cc_3opr(code, info,
					      SH_INS_DSP_PSUBr,
					      SH_INS_DSP_INVALID, detail);
		}
	case 0x02:
		switch (sy) {
		case 0:
			if(cc == 0) {
				info->op.operands[2].dsp.insn = SH_INS_DSP_PABS;
				set_reg_dsp_read(info, 0, f_sx, sx, detail);
				set_reg_dsp_write_z(info, 1, dz, detail);
				info->op.op_count = 3;
				return MCDisassembler_Success;
			} else {
				return dsp_op_cc_2opr(code, info,
						      SH_INS_DSP_PDEC,
						      f_sx, 6, detail);
			}
		case 1:
			return dsp_op_cc0_2opr(code, info,
					       SH_INS_DSP_PABS,
					       f_sx, 6, detail);
		default:
			return MCDisassembler_Fail;
		}			
	case 0x03:
		if (cc != 0) {
			info->op.operands[2].dsp.insn = SH_INS_DSP_PCLR;
			info->op.operands[2].dsp.cc = cc;
			set_reg_dsp_write_z(info, 0, dz, detail);
			info->op.op_count = 3;
			return MCDisassembler_Success;
		} else
			return MCDisassembler_Fail;
	case 0x04:
		return dsp_op_cc_3opr(code, info,
				      SH_INS_DSP_PSHA, SH_INS_DSP_INVALID,
				      detail);
	case 0x05:
		return dsp_op_cc_3opr(code, info,
				      SH_INS_DSP_PAND, SH_INS_DSP_INVALID,
				      detail);
	case 0x06:
		switch (sy) {
		case 0:
			if (cc == 0) {
				info->op.operands[2].dsp.insn = SH_INS_DSP_PRND;
				set_reg_dsp_read(info, 0, f_sx, sx, detail);
				set_reg_dsp_write_z(info, 1, dz, detail);
				info->op.op_count = 3;
				return MCDisassembler_Success;
			} else {
				return dsp_op_cc_2opr(code, info,
						      SH_INS_DSP_PINC,
						      f_sx, 6, detail);
			}
		case 1:
			return dsp_op_cc0_2opr(code, info,
					       SH_INS_DSP_PRND,
					       f_sx, 6, detail);
		default:
			return MCDisassembler_Fail;
		}
	case 0x07:
		switch(sy) {
		case 0:
			return dsp_op_cc_2opr(code, info,
					      SH_INS_DSP_PDMSB,
					      f_sx, 6, detail);
		case 1:
			return dsp_op_cc_2opr(code, info,
					      SH_INS_DSP_PSWAP,
					      f_sx, 6, detail);
		default:
			return MCDisassembler_Fail;
		}
	case 0x08:
		return dsp_op_cc_3opr(code, info,
				      SH_INS_DSP_PSUB, (sh_dsp_insn_type) SH_INS_DSP_PSUBC,
				      detail);
	case 0x09:
		return dsp_op_cc_3opr(code, info,
				      SH_INS_DSP_PXOR, (sh_dsp_insn_type) SH_INS_DSP_PWSB,
				      detail);
	case 0x0a:
		switch(sx) {
		case 0:
			if (cc == 0) {
				info->op.operands[2].dsp.insn = SH_INS_DSP_PABS;
				set_reg_dsp_read(info, 0, f_sy, sy, detail);
				set_reg_dsp_write_z(info, 1, dz, detail);
				info->op.op_count = 3;
				return MCDisassembler_Success;
			} else {
				return dsp_op_cc_2opr(code, info,
						      SH_INS_DSP_PDEC,
						      f_sy, 4, detail);
			}
		case 1:
			return dsp_op_cc_2opr(code, info,
					      SH_INS_DSP_PABS,
					      f_sy, 4, detail);
		default:
			return MCDisassembler_Fail;
		}
	case 0x0c:
		if (cc == 0) {
				info->op.operands[2].dsp.insn
					= SH_INS_DSP_PADDC;
				set_reg_dsp_read(info, 0, f_sx, sx, detail);
				set_reg_dsp_read(info, 1, f_sy, sy, detail);
				set_reg_dsp_write_z(info, 2, dz, detail);
				info->op.op_count = 3;
				return MCDisassembler_Success;
		} else {
			return dsp_op_cc_3opr(code, info,
					      SH_INS_DSP_PADD,
					      SH_INS_DSP_INVALID, detail);
		}
	case 0x0d:
		return dsp_op_cc_3opr(code, info,
								SH_INS_DSP_POR,
								(sh_dsp_insn_type) SH_INS_DSP_PWAD,
								detail);
	case 0x0e:
		if (cc == 0) {
			if (sx != 0)
				return MCDisassembler_Fail;
			info->op.operands[2].dsp.insn = SH_INS_DSP_PRND;
			set_reg_dsp_read(info, 0, f_sy, sy, detail);
			set_reg_dsp_write_z(info, 1, dz, detail);
			info->op.op_count = 3;
			return MCDisassembler_Success;
		} else {
			switch(sx) {
			case 0:
				return dsp_op_cc_2opr(code, info,
						      SH_INS_DSP_PINC,
						      f_sy, 4, detail);
			case 1:
				return dsp_op_cc_2opr(code, info,
						      SH_INS_DSP_PRND,
						      f_sy, 4, detail);
			default:
				return MCDisassembler_Fail;
 			}
		}
	case 0x0f:
		switch(sx) {
		case 0:
			return dsp_op_cc_2opr(code, info,
					      SH_INS_DSP_PDMSB,
					      f_sy, 4, detail);
		case 1:
			return dsp_op_cc_2opr(code, info,
					      SH_INS_DSP_PSWAP,
					      f_sy, 4, detail);
		default:
			return MCDisassembler_Fail;
		}
	case 0x12:
		return dsp_op_cc_2opr(code, info,
				      SH_INS_DSP_PNEG, f_sx, 6, detail);
	case 0x13:
	case 0x17:
		if (cc > 0) {
			info->op.operands[2].dsp.insn = SH_INS_DSP_PSTS;
			info->op.operands[2].dsp.cc = cc;
			regs_read(detail, 
				  info->op.operands[2].dsp.r[0]
				  = SH_REG_MACH + ((code >> 12) & 1));
			set_reg_dsp_write_z(info, 1, dz, detail);
			info->op.op_count = 3;
			return MCDisassembler_Success;
		} else {
			return MCDisassembler_Fail;
		}
	case 0x16:
		return dsp_op_cc_2opr(code, info,
				      SH_INS_DSP_PCOPY, f_sx, 6, detail);
	case 0x1a:
		return dsp_op_cc_2opr(code, info,
				      SH_INS_DSP_PNEG, f_sy, 4, detail);
	case 0x1b:
	case 0x1f:
		if (cc > 0) {
			info->op.operands[2].dsp.insn = SH_INS_DSP_PLDS;
			info->op.operands[2].dsp.cc = cc;
			info->op.operands[2].dsp.r[0] = regs_dz[dz];
			regs_read(detail, regs_dz[dz]);
			regs_write(detail, 
				   info->op.operands[2].dsp.r[1]
				   = SH_REG_MACH + ((code >> 12) & 1));
			info->op.op_count = 3;
			return MCDisassembler_Success;
		} else {
			return MCDisassembler_Fail;
		}
	case 0x1e:
		return dsp_op_cc_2opr(code, info, SH_INS_DSP_PCOPY, f_sy, 4, detail);
	default:
		return MCDisassembler_Fail;
	}		
}

static bool decode_dsp_p(const uint32_t code, MCInst *MI, cs_mode mode,
			 sh_info *info, cs_detail *detail)
{
	int dz = code & 0x0f;
	MCInst_setOpcode(MI, SH_INS_DSP);
	if (!decode_dsp_d(code >> 16, MI, mode, info, detail))
		return MCDisassembler_Fail;
		
	switch((code >> 12) & 0x0f) {
	case 0x00:
	case 0x01:
		if ((code >> 11) & 1)
			return MCDisassembler_Fail;
		info->op.operands[2].dsp.insn
			= SH_INS_DSP_PSHL + ((code >> 12) & 1);
		info->op.operands[2].dsp.imm = (code >> 4) & 0x7f;
		set_reg_dsp_write_z(info, 1, dz, detail);
		info->op.op_count = 3;
		return MCDisassembler_Success;
	case 0x04:
		if ((((code >> 4) & 1) && isalevel(mode) != ISA_SH4A) ||
		    (!((code >> 4) & 1) && (code &3)) ||
		    ((code >> 4) & 0x0f) >= 2)
			return MCDisassembler_Fail;
			
		info->op.operands[2].dsp.insn
			= SH_INS_DSP_PMULS + ((code >> 4) & 1);
		set_reg_dsp_read(info, 0, f_se, (code >> 10) & 3, detail);
		set_reg_dsp_read(info, 1, f_sf, (code >> 8) & 3, detail);
		set_reg_dsp_write_gu(info, 2, f_dg, (code >> 2) & 3, detail);
		if ((code >> 4) & 1)
			set_reg_dsp_write_gu(info, 3, f_du,
					     (code >> 0) & 3, detail);
		info->op.op_count = 3;
		return MCDisassembler_Success;
	case 0x06:
	case 0x07:
		info->op.operands[2].dsp.insn
			= SH_INS_DSP_PSUB_PMULS + ((code >> 12) & 1);
		set_reg_dsp_read(info, 0, f_sx, (code >> 6) & 3, detail);
		set_reg_dsp_read(info, 1, f_sy, (code >> 4) & 3, detail);
		set_reg_dsp_write_gu(info, 2, f_du, (code >> 0) & 3, detail);
		set_reg_dsp_read(info, 3, f_se, (code >> 10) & 3, detail);
		set_reg_dsp_read(info, 4, f_sf, (code >> 8) & 3, detail);
		set_reg_dsp_write_gu(info, 5, f_dg, (code >> 2) & 3, detail);
		info->op.op_count = 3;
		return MCDisassembler_Success;
	default:
		if ((code >> 15) & 1)
			return decode_dsp_3op(code, info, detail);
	}
	return MCDisassembler_Fail;
}

static bool sh_disassemble(const uint8_t *code, MCInst *MI, uint64_t address,
			   cs_mode mode, uint16_t *size, int code_len,
			   sh_info *info, cs_detail *detail)
{
	int idx;
	uint32_t insn;
	bool dsp_result;
	if (MODE_IS_BIG_ENDIAN(mode)) {
		insn = code[0] << 8 | code[1];
	} else {
		insn = code[1] << 8 | code[0];
	}
	if (mode & CS_MODE_SH2A) {
		/* SH2A 32bit instrcution test */
		if (((insn & 0xf007) == 0x3001 ||
		     (insn & 0xf00e) == 0x0000)) {
			if (code_len < 4)
				return MCDisassembler_Fail;
			*size = 4;
			// SH2A is only BIG ENDIAN.
			insn <<= 16;
			insn |= code[2] << 8 | code[3];
			if (decode_long(insn, address,	MI, info, detail))
				return MCDisassembler_Success;
		}
	}
	/* Co-processor instructions */
	if ((insn & 0xf000) == 0xf000) {
		if (mode & CS_MODE_SHDSP) {
			dsp_result = MCDisassembler_Fail;
			switch(insn >> 10 & 3) {
			case 0:
				*size = 2;
				dsp_result = decode_dsp_d(insn, MI, mode,
							  info, detail);
				break;
			case 1:
				*size = 2;
				dsp_result = decode_dsp_s(insn, MI,
							  info, detail);
				break;
			case 2:
				if (code_len < 4)
					return MCDisassembler_Fail;
				*size = 4;
				if (MODE_IS_BIG_ENDIAN(mode)) {
					insn <<= 16;
					insn |= code[2] << 8 | code[3];
				} else
					insn |= (code[3] << 24)
						| (code[2] << 16);
				dsp_result = decode_dsp_p(insn, MI, mode,
							  info, detail);
				break;
			}
			return dsp_result;
		}
		if ((mode & CS_MODE_SHFPU) == 0)
			return MCDisassembler_Fail;
	}
	
	*size = 2;
	if ((insn & 0xf000) >= 0x8000 && (insn & 0xf000) < 0xf000) {
		idx = insn >> 8;
	} else {
		idx = ((insn >> 8) & 0xf0) | (insn & 0x000f);
	}

	if (decode[idx]) {
		return decode[idx](insn, address, MI, mode, info, detail);
	} else {
		return MCDisassembler_Fail;
	}
}
		  
bool SH_getInstruction(csh ud, const uint8_t *code, size_t code_len,
	MCInst *MI, uint16_t *size, uint64_t address, void *inst_info)
{
	
	cs_struct* handle = (cs_struct *)ud;
	sh_info *info = (sh_info *)handle->printer_info;
	cs_detail *detail = MI->flat_insn->detail;

	if (code_len < 2) {
		*size = 0;
		return MCDisassembler_Fail;
	}

	if (detail) {
		memset(detail, 0, offsetof(cs_detail, sh)+sizeof(cs_sh));
	}
	memset(info, 0, sizeof(sh_info));
	if (sh_disassemble(code, MI, address, handle->mode,
			   size, code_len, info, detail) == MCDisassembler_Fail) {
		*size = 0;
		return MCDisassembler_Fail;
	} else {
		if (detail)
			detail->sh = info->op;
		return MCDisassembler_Success;
	}		
}

#ifndef CAPSTONE_DIET
void SH_reg_access(const cs_insn *insn,
		   cs_regs regs_read, uint8_t *regs_read_count,
		   cs_regs regs_write, uint8_t *regs_write_count)
{
        if (insn->detail == NULL) {
                *regs_read_count = 0;
                *regs_write_count = 0;
        }
        else {
                *regs_read_count = insn->detail->regs_read_count;
                *regs_write_count = insn->detail->regs_write_count;

                memcpy(regs_read, insn->detail->regs_read,
                        *regs_read_count * sizeof(insn->detail->regs_read[0]));
                memcpy(regs_write, insn->detail->regs_write,
                        *regs_write_count *
                        sizeof(insn->detail->regs_write[0]));
        }
}
#endif


