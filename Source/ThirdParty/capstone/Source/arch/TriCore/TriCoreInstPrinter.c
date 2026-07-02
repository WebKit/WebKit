//===- TriCoreInstPrinter.cpp - Convert TriCore MCInst to assembly syntax -===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This class prints an TriCore MCInst to a .s file.
//
//===----------------------------------------------------------------------===//

/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2014 */

#ifdef CAPSTONE_HAS_TRICORE

#include <platform.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../MCInst.h"
#include "../../MCRegisterInfo.h"
#include "../../MathExtras.h"
#include "../../SStream.h"
#include "../../utils.h"
#include "TriCoreMapping.h"
#include "TriCoreLinkage.h"

static const char *getRegisterName(unsigned RegNo);

static void printInstruction(MCInst *, uint64_t, SStream *);

static void printOperand(MCInst *MI, int OpNum, SStream *O);

#define GET_INSTRINFO_ENUM

#include "TriCoreGenInstrInfo.inc"

#define GET_REGINFO_ENUM

#include "TriCoreGenRegisterInfo.inc"

static bool fill_mem(MCInst *MI, unsigned int reg, int32_t disp);

static inline void set_mem(cs_tricore_op *op, uint8_t base, int32_t disp)
{
	op->type |= TRICORE_OP_MEM;
	op->mem.base = base;
	op->mem.disp = disp;
}

static inline void fill_reg(MCInst *MI, uint32_t reg)
{
	if (!detail_is_set(MI))
		return;
	cs_tricore_op *op = TriCore_get_detail_op(MI, 0);
	op->type = TRICORE_OP_REG;
	op->reg = reg;
	TriCore_inc_op_count(MI);
}

static inline void fill_imm(MCInst *MI, int32_t imm)
{
	if (!detail_is_set(MI))
		return;
	cs_tricore *tricore = TriCore_get_detail(MI);
	if (tricore->op_count >= 1) {
		cs_tricore_op *op = TriCore_get_detail_op(MI, -1);
		if (op->type == TRICORE_OP_REG && fill_mem(MI, op->reg, imm))
			return;
	}

	cs_tricore_op *op = TriCore_get_detail_op(MI, 0);
	op->type = TRICORE_OP_IMM;
	op->imm = imm;
	tricore->op_count++;
}

static bool fill_mem(MCInst *MI, unsigned int reg, int32_t disp)
{
	if (!detail_is_set(MI))
		return false;
	switch (MI->flat_insn->id) {
	case TRICORE_INS_LDMST:
	case TRICORE_INS_LDLCX:
	case TRICORE_INS_LD_A:
	case TRICORE_INS_LD_B:
	case TRICORE_INS_LD_BU:
	case TRICORE_INS_LD_H:
	case TRICORE_INS_LD_HU:
	case TRICORE_INS_LD_D:
	case TRICORE_INS_LD_DA:
	case TRICORE_INS_LD_W:
	case TRICORE_INS_LD_Q:
	case TRICORE_INS_STLCX:
	case TRICORE_INS_STUCX:
	case TRICORE_INS_ST_A:
	case TRICORE_INS_ST_B:
	case TRICORE_INS_ST_H:
	case TRICORE_INS_ST_D:
	case TRICORE_INS_ST_DA:
	case TRICORE_INS_ST_W:
	case TRICORE_INS_ST_Q:
	case TRICORE_INS_CACHEI_I:
	case TRICORE_INS_CACHEI_W:
	case TRICORE_INS_CACHEI_WI:
	case TRICORE_INS_CACHEA_I:
	case TRICORE_INS_CACHEA_W:
	case TRICORE_INS_CACHEA_WI:
	case TRICORE_INS_CMPSWAP_W:
	case TRICORE_INS_SWAP_A:
	case TRICORE_INS_SWAP_W:
	case TRICORE_INS_SWAPMSK_W:
	case TRICORE_INS_LEA:
	case TRICORE_INS_LHA: {
		switch (MCInst_getOpcode(MI)) {
		case TRICORE_LDMST_abs:
		case TRICORE_LDLCX_abs:
		case TRICORE_LD_A_abs:
		case TRICORE_LD_B_abs:
		case TRICORE_LD_BU_abs:
		case TRICORE_LD_H_abs:
		case TRICORE_LD_HU_abs:
		case TRICORE_LD_D_abs:
		case TRICORE_LD_DA_abs:
		case TRICORE_LD_W_abs:
		case TRICORE_LD_Q_abs:
		case TRICORE_STLCX_abs:
		case TRICORE_STUCX_abs:
		case TRICORE_ST_A_abs:
		case TRICORE_ST_B_abs:
		case TRICORE_ST_H_abs:
		case TRICORE_ST_D_abs:
		case TRICORE_ST_DA_abs:
		case TRICORE_ST_W_abs:
		case TRICORE_ST_Q_abs:
		case TRICORE_SWAP_A_abs:
		case TRICORE_SWAP_W_abs:
		case TRICORE_LEA_abs:
		case TRICORE_LHA_abs: {
			return false;
		}
		}
		cs_tricore_op *op = TriCore_get_detail_op(MI, -1);
		op->type = 0;
		set_mem(op, reg, disp);
		return true;
	}
	}
	return false;
}

static void printOperand(MCInst *MI, int OpNum, SStream *O)
{
	if (OpNum >= MI->size)
		return;

	MCOperand *Op = MCInst_getOperand(MI, OpNum);
	if (MCOperand_isReg(Op)) {
		unsigned reg = MCOperand_getReg(Op);
		SStream_concat0(O, getRegisterName(reg));
		fill_reg(MI, reg);
	} else if (MCOperand_isImm(Op)) {
		int64_t Imm = MCOperand_getImm(Op);
		printInt64Bang(O, Imm);
		fill_imm(MI, (int32_t)Imm);
	}
}

static inline unsigned int get_msb(unsigned int value)
{
	unsigned int msb = 0;
	while (value > 0) {
		value >>= 1; // Shift bits to the right
		msb++;	     // Increment the position of the MSB
	}
	return msb;
}

static inline int32_t sign_ext_n(int32_t imm, unsigned n)
{
	n = get_msb(imm) > n ? get_msb(imm) : n;
	int32_t mask = 1 << (n - 1);
	int32_t sign_extended = (imm ^ mask) - mask;
	return sign_extended;
}

static void print_sign_ext(MCInst *MI, int OpNum, SStream *O, unsigned n)
{
	MCOperand *MO = MCInst_getOperand(MI, OpNum);
	if (MCOperand_isImm(MO)) {
		int32_t imm = (int32_t)MCOperand_getImm(MO);
		imm = sign_ext_n(imm, n);
		printInt32Bang(O, imm);
		fill_imm(MI, imm);
	} else
		printOperand(MI, OpNum, O);
}

static void off4_fixup(MCInst *MI, uint64_t *off4)
{
	switch (MCInst_getOpcode(MI)) {
	case TRICORE_LD_A_slro:
	case TRICORE_LD_A_sro:
	case TRICORE_LD_W_slro:
	case TRICORE_LD_W_sro:
	case TRICORE_ST_A_sro:
	case TRICORE_ST_A_ssro:
	case TRICORE_ST_W_sro:
	case TRICORE_ST_W_ssro: {
		*off4 *= 4;
		break;
	}
	case TRICORE_LD_H_sro:
	case TRICORE_LD_H_slro:
	case TRICORE_ST_H_sro:
	case TRICORE_ST_H_ssro: {
		*off4 *= 2;
		break;
	}
	}
}

static void const8_fixup(MCInst *MI, uint64_t *const8)
{
	switch (MCInst_getOpcode(MI)) {
	case TRICORE_LD_A_sc:
	case TRICORE_ST_A_sc:
	case TRICORE_ST_W_sc:
	case TRICORE_LD_W_sc: {
		*const8 *= 4;
		break;
	}
	}
}

static void print_zero_ext(MCInst *MI, int OpNum, SStream *O, unsigned n)
{
	MCOperand *MO = MCInst_getOperand(MI, OpNum);
	if (MCOperand_isImm(MO)) {
		uint64_t imm = MCOperand_getImm(MO);
		for (unsigned i = n + 1; i < 32; ++i) {
			imm &= ~(1 << i);
		}
		if (n == 4) {
			off4_fixup(MI, &imm);
		}
		if (n == 8) {
			const8_fixup(MI, &imm);
		}

		printInt64Bang(O, imm);
		fill_imm(MI, imm);
	} else
		printOperand(MI, OpNum, O);
}

static void printOff18Imm(MCInst *MI, int OpNum, SStream *O)
{
	MCOperand *MO = MCInst_getOperand(MI, OpNum);
	if (MCOperand_isImm(MO)) {
		uint32_t imm = (uint32_t)MCOperand_getImm(MO);
		imm = ((imm & 0x3C000) << 14) | (imm & 0x3fff);
		printUInt32Bang(O, imm);
		fill_imm(MI, (int32_t)imm);
	} else
		printOperand(MI, OpNum, O);
}

static void printDisp24Imm(MCInst *MI, int OpNum, SStream *O)
{
	MCOperand *MO = MCInst_getOperand(MI, OpNum);
	if (MCOperand_isImm(MO)) {
		uint32_t disp = MCOperand_getImm(MO);
		switch (MCInst_getOpcode(MI)) {
		case TRICORE_CALL_b:
		case TRICORE_FCALL_b: {
			disp = (int32_t)MI->address + sign_ext_n(disp * 2, 24);
			break;
		}
		case TRICORE_CALLA_b:
		case TRICORE_FCALLA_b:
		case TRICORE_JA_b:
		case TRICORE_JLA_b:
			// = {disp24[23:20], 7’b0000000, disp24[19:0], 1’b0};
			disp = ((disp & 0xf00000) << 28) |
			       ((disp & 0xfffff) << 1);
			break;
		case TRICORE_J_b:
		case TRICORE_JL_b:
			disp = (int32_t)MI->address + sign_ext_n(disp, 24) * 2;
			break;
		}

		printUInt32Bang(O, disp);
		fill_imm(MI, disp);
	} else
		printOperand(MI, OpNum, O);
}

static void printDisp15Imm(MCInst *MI, int OpNum, SStream *O)
{
	MCOperand *MO = MCInst_getOperand(MI, OpNum);
	if (MCOperand_isImm(MO)) {
		uint32_t disp = MCOperand_getImm(MO);
		switch (MCInst_getOpcode(MI)) {
		case TRICORE_JEQ_brc:
		case TRICORE_JEQ_brr:
		case TRICORE_JEQ_A_brr:
		case TRICORE_JGE_brc:
		case TRICORE_JGE_brr:
		case TRICORE_JGE_U_brc:
		case TRICORE_JGE_U_brr:
		case TRICORE_JLT_brc:
		case TRICORE_JLT_brr:
		case TRICORE_JLT_U_brc:
		case TRICORE_JLT_U_brr:
		case TRICORE_JNE_brc:
		case TRICORE_JNE_brr:
		case TRICORE_JNE_A_brr:
		case TRICORE_JNED_brc:
		case TRICORE_JNED_brr:
		case TRICORE_JNEI_brc:
		case TRICORE_JNEI_brr:
		case TRICORE_JNZ_A_brr:
		case TRICORE_JNZ_T_brn:
		case TRICORE_JZ_A_brr:
		case TRICORE_JZ_T_brn:
			disp = (int32_t)MI->address + sign_ext_n(disp, 15) * 2;
			break;
		case TRICORE_LOOP_brr:
		case TRICORE_LOOPU_brr:
			disp = (int32_t)MI->address + sign_ext_n(disp * 2, 15);
			break;
		default:
			// handle other cases, if any
			break;
		}

		printUInt32Bang(O, disp);
		fill_imm(MI, disp);
	} else
		printOperand(MI, OpNum, O);
}

static void printDisp8Imm(MCInst *MI, int OpNum, SStream *O)
{
	MCOperand *MO = MCInst_getOperand(MI, OpNum);
	if (MCOperand_isImm(MO)) {
		uint32_t disp = MCOperand_getImm(MO);
		switch (MCInst_getOpcode(MI)) {
		case TRICORE_CALL_sb:
			disp = (int32_t)MI->address + sign_ext_n(2 * disp, 8);
			break;
		case TRICORE_J_sb:
		case TRICORE_JNZ_sb:
		case TRICORE_JZ_sb:
			disp = (int32_t)MI->address + sign_ext_n(disp, 8) * 2;
			break;
		default:
			// handle other cases, if any
			break;
		}

		printUInt32Bang(O, disp);
		fill_imm(MI, disp);
	} else
		printOperand(MI, OpNum, O);
}

static void printDisp4Imm(MCInst *MI, int OpNum, SStream *O)
{
	MCOperand *MO = MCInst_getOperand(MI, OpNum);
	if (MCOperand_isImm(MO)) {
		uint32_t disp = MCOperand_getImm(MO);
		switch (MCInst_getOpcode(MI)) {
		case TRICORE_JEQ_sbc1:
		case TRICORE_JEQ_sbr1:
		case TRICORE_JGEZ_sbr:
		case TRICORE_JGTZ_sbr:
		case TRICORE_JLEZ_sbr:
		case TRICORE_JLTZ_sbr:
		case TRICORE_JNE_sbc1:
		case TRICORE_JNE_sbr1:
		case TRICORE_JNZ_sbr:
		case TRICORE_JNZ_A_sbr:
		case TRICORE_JNZ_T_sbrn:
		case TRICORE_JZ_sbr:
		case TRICORE_JZ_A_sbr:
		case TRICORE_JZ_T_sbrn:
			disp = (int32_t)MI->address + disp * 2;
			break;
		case TRICORE_JEQ_sbc2:
		case TRICORE_JEQ_sbr2:
		case TRICORE_JNE_sbc2:
		case TRICORE_JNE_sbr2:
			disp = (int32_t)MI->address + (disp + 16) * 2;
			break;
		case TRICORE_LOOP_sbr:
			// {27b’111111111111111111111111111, disp4, 0};
			disp = (int32_t)MI->address +
			       ((0x7ffffff << 5) | (disp << 1));
			break;
		default:
			// handle other cases, if any
			break;
		}

		printUInt32Bang(O, disp);
		fill_imm(MI, disp);
	} else
		printOperand(MI, OpNum, O);
}

#define printSExtImm_(n) \
	static void printSExtImm_##n(MCInst *MI, int OpNum, SStream *O) \
	{ \
		print_sign_ext(MI, OpNum, O, n); \
	}

#define printZExtImm_(n) \
	static void printZExtImm_##n(MCInst *MI, int OpNum, SStream *O) \
	{ \
		print_zero_ext(MI, OpNum, O, n); \
	}

// clang-format off

printSExtImm_(16)
printSExtImm_(10)
printSExtImm_(9)
printSExtImm_(4)

printZExtImm_(16)
printZExtImm_(9)
printZExtImm_(8)
printZExtImm_(4)
printZExtImm_(2);

// clang-format on

static void printOExtImm_4(MCInst *MI, int OpNum, SStream *O)
{
	MCOperand *MO = MCInst_getOperand(MI, OpNum);
	if (MCOperand_isImm(MO)) {
		uint32_t imm = MCOperand_getImm(MO);
		// {27b’111111111111111111111111111, disp4, 0};
		int32_t off = (int32_t)(0xffffffe0 | (imm << 1));
		uint32_t target = (int32_t)MI->address + off;

		printUInt32(O, target);
		fill_imm(MI, (int32_t)target);
	} else
		printOperand(MI, OpNum, O);
}

/// Returned by getMnemonic() of the AsmPrinters.
typedef struct {
	const char *first; // Menmonic
	uint64_t second;   // Bits
} MnemonicBitsInfo;

static void set_mem_access(MCInst *MI, unsigned int access)
{
	// TODO: TriCore
}

#include "TriCoreGenAsmWriter.inc"

const char *TriCore_LLVM_getRegisterName(unsigned int id)
{
#ifndef CAPSTONE_DIET
	return getRegisterName(id);
#else
	return NULL;
#endif
}

void TriCore_LLVM_printInst(MCInst *MI, uint64_t Address, SStream *O)
{
	printInstruction(MI, Address, O);
	TriCore_set_access(MI);
}

#endif // CAPSTONE_HAS_TRICORE
