//===------ TriCoreDisassembler.cpp - Disassembler for TriCore --*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2014 */

#ifdef CAPSTONE_HAS_TRICORE

#include <stdio.h> // DEBUG
#include <stdlib.h>
#include <string.h>

#include "../../cs_priv.h"
#include "../../utils.h"

#include "../../MCInst.h"
#include "../../MCInstrDesc.h"
#include "../../MCFixedLenDisassembler.h"
#include "../../MCRegisterInfo.h"
#include "../../MCDisassembler.h"
#include "../../MathExtras.h"

#include "TriCoreDisassembler.h"
#include "TriCoreMapping.h"
#include "TriCoreLinkage.h"

static unsigned getReg(MCRegisterInfo *MRI, unsigned RC, unsigned RegNo)
{
	const MCRegisterClass *rc = MCRegisterInfo_getRegClass(MRI, RC);
	return rc->RegsBegin[RegNo];
}

#define tryDecodeReg(i, x)                                                    \
	status = DecodeRegisterClass(Inst, (x), &desc->OpInfo[(i)], Decoder); \
	if (status != MCDisassembler_Success)                                 \
		return status;

#define decodeImm(x) MCOperand_CreateImm0(Inst, (x));

static DecodeStatus DecodeSBInstruction(MCInst *Inst, unsigned Insn,
					uint64_t Address, void *Decoder);

static DecodeStatus DecodeSBRInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder);

static DecodeStatus DecodeSCInstruction(MCInst *Inst, unsigned Insn,
					uint64_t Address, void *Decoder);

static DecodeStatus DecodeSRInstruction(MCInst *Inst, unsigned Insn,
					uint64_t Address, void *Decoder);

static DecodeStatus DecodeSRCInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder);

static DecodeStatus DecodeSRRInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder);

static DecodeStatus DecodeABSInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder);

static DecodeStatus DecodeBInstruction(MCInst *Inst, unsigned Insn,
				       uint64_t Address, void *Decoder);

static DecodeStatus DecodeBOInstruction(MCInst *Inst, unsigned Insn,
					uint64_t Address, void *Decoder);

static DecodeStatus DecodeBOLInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder);

static DecodeStatus DecodeRCInstruction(MCInst *Inst, unsigned Insn,
					uint64_t Address, void *Decoder);

static DecodeStatus DecodeRCPWInstruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder);

static DecodeStatus DecodeRLCInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder);

static DecodeStatus DecodeRRInstruction(MCInst *Inst, unsigned Insn,
					uint64_t Address, void *Decoder);

static DecodeStatus DecodeRR2Instruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder);

static DecodeStatus DecodeRRPWInstruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder);

static DecodeStatus DecodeSLRInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder);

static DecodeStatus DecodeSLROInstruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder);

static DecodeStatus DecodeSROInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder);

static DecodeStatus DecodeSRRSInstruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder);

static DecodeStatus DecodeSBCInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder);

static DecodeStatus DecodeSBRNInstruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder);

static DecodeStatus DecodeSSRInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder);

static DecodeStatus DecodeSSROInstruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder);

static DecodeStatus DecodeSYSInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder);

static DecodeStatus DecodeRRR2Instruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder);

static DecodeStatus DecodeRRR1Instruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder);

static DecodeStatus DecodeBITInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder);

static DecodeStatus DecodeRR1Instruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder);

static DecodeStatus DecodeRCRInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder);

static DecodeStatus DecodeRRRWInstruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder);

static DecodeStatus DecodeRCRRInstruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder);

static DecodeStatus DecodeRRRRInstruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder);

static DecodeStatus DecodeBRRInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder);

static DecodeStatus DecodeBRCInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder);

static DecodeStatus DecodeRRRInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder);

static DecodeStatus DecodeABSBInstruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder);

static DecodeStatus DecodeRCRWInstruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder);

static DecodeStatus DecodeBRNInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder);

#define GET_SUBTARGETINFO_ENUM

#include "TriCoreGenSubtargetInfo.inc"

bool TriCore_getFeatureBits(unsigned int mode, unsigned int feature)
{
	switch (mode) {
	case CS_MODE_TRICORE_110: {
		return feature == TRICORE_HasV110Ops;
	}
	case CS_MODE_TRICORE_120: {
		return feature == TRICORE_HasV120Ops;
	}
	case CS_MODE_TRICORE_130: {
		return feature == TRICORE_HasV130Ops;
	}
	case CS_MODE_TRICORE_131: {
		return feature == TRICORE_HasV131Ops;
	}
	case CS_MODE_TRICORE_160: {
		return feature == TRICORE_HasV160Ops;
	}
	case CS_MODE_TRICORE_161: {
		return feature == TRICORE_HasV161Ops;
	}
	case CS_MODE_TRICORE_162: {
		return feature == TRICORE_HasV162Ops;
	}
	default:
		return false;
	}
}

#include "TriCoreGenDisassemblerTables.inc"

#define GET_REGINFO_ENUM
#define GET_REGINFO_MC_DESC

#include "TriCoreGenRegisterInfo.inc"

static DecodeStatus DecodeRegisterClass(MCInst *Inst, unsigned RegNo,
					const MCOperandInfo *MCOI,
					void *Decoder)
{
	unsigned Reg;
	unsigned RegHalfNo = RegNo / 2;

	if (!MCOI || MCOI->OperandType != MCOI_OPERAND_REGISTER) {
		return MCDisassembler_Fail;
	}

	if (RegHalfNo > 15)
		return MCDisassembler_Fail;

	if (MCOI->RegClass < 3) {
		Reg = getReg(Decoder, MCOI->RegClass, RegNo);
	} else {
		Reg = getReg(Decoder, MCOI->RegClass, RegHalfNo);
	}

	MCOperand_CreateReg0(Inst, Reg);

	return MCDisassembler_Success;
}

#define GET_INSTRINFO_ENUM
#define GET_INSTRINFO_MC_DESC

#include "TriCoreGenInstrInfo.inc"

static DecodeStatus DecodeSBInstruction(MCInst *Inst, unsigned Insn,
					uint64_t Address, void *Decoder)
{
	unsigned disp8 = fieldFromInstruction_2(Insn, 8, 8);
	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);

	if (is32Bit) // This instruction is 16-bit
		return MCDisassembler_Fail;

	// Decode disp8.
	MCOperand_CreateImm0(Inst, disp8);

	return MCDisassembler_Success;
}

static DecodeStatus DecodeSBRInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder)
{
	DecodeStatus status;
	unsigned s2 = fieldFromInstruction_2(Insn, 12, 4);
	unsigned disp4 = fieldFromInstruction_2(Insn, 8, 4);
	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);

	if (is32Bit) // This instruction is 16-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	status = DecodeRegisterClass(Inst, s2, &desc->OpInfo[0], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode disp4.
	MCOperand_CreateImm0(Inst, disp4);

	return MCDisassembler_Success;
}

static DecodeStatus DecodeSCInstruction(MCInst *Inst, unsigned Insn,
					uint64_t Address, void *Decoder)
{
	unsigned const8 = fieldFromInstruction_2(Insn, 8, 8);
	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);

	if (is32Bit) // This instruction is 16-bit
		return MCDisassembler_Fail;

	// Decode const8.
	MCOperand_CreateImm0(Inst, const8);

	return MCDisassembler_Success;
}

static DecodeStatus DecodeSRInstruction(MCInst *Inst, unsigned Insn,
					uint64_t Address, void *Decoder)
{
	DecodeStatus status;
	unsigned s1_d = fieldFromInstruction_2(Insn, 8, 4);
	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);

	if (is32Bit) // This instruction is 16-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	if (desc->NumOperands > 0) {
		status = DecodeRegisterClass(Inst, s1_d, &desc->OpInfo[0],
					     Decoder);
		if (status != MCDisassembler_Success)
			return status;
	}

	if (desc->NumOperands > 1) {
		status = DecodeRegisterClass(Inst, s1_d, &desc->OpInfo[1],
					     Decoder);
		if (status != MCDisassembler_Success)
			return status;
	}

	return MCDisassembler_Success;
}

static DecodeStatus DecodeSRCInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder)
{
	DecodeStatus status;
	unsigned const4 = fieldFromInstruction_2(Insn, 12, 4);
	unsigned s1_d = fieldFromInstruction_2(Insn, 8, 4);
	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);

	if (is32Bit) // This instruction is 16-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];

	// Decode s1/d.
	status = DecodeRegisterClass(Inst, s1_d, &desc->OpInfo[0], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode const4.
	MCOperand_CreateImm0(Inst, const4);

	return MCDisassembler_Success;
}

static DecodeStatus DecodeSRRInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder)
{
	DecodeStatus status;
	unsigned s2 = fieldFromInstruction_2(Insn, 12, 4);
	unsigned s1_d = fieldFromInstruction_2(Insn, 8, 4);
	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);

	if (is32Bit) // This instruction is 16-bit
		return MCDisassembler_Fail;

	// Decode s1/d.
	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	status = DecodeRegisterClass(Inst, s1_d, &desc->OpInfo[0], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s2.
	status = DecodeRegisterClass(Inst, s2, &desc->OpInfo[1], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	return MCDisassembler_Success;
}

static DecodeStatus DecodeABSInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder)
{
	DecodeStatus status;
	unsigned off18_0 = fieldFromInstruction_4(Insn, 16, 6);
	unsigned off18_1 = fieldFromInstruction_4(Insn, 28, 4);
	unsigned off18_2 = fieldFromInstruction_4(Insn, 22, 4);
	unsigned off18_3 = fieldFromInstruction_4(Insn, 12, 4);
	unsigned off18 = (off18_0 << 0) | (off18_1 << 6) | (off18_2 << 10) |
			 (off18_3 << 14);

	unsigned s1_d = fieldFromInstruction_4(Insn, 8, 4);
	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);

	if (!is32Bit) // This instruction is 32-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];

	if (desc->NumOperands > 1) {
		if (desc->OpInfo[0].OperandType == MCOI_OPERAND_REGISTER) {
			status = DecodeRegisterClass(Inst, s1_d,
						     &desc->OpInfo[0], Decoder);
			if (status != MCDisassembler_Success)
				return status;

			MCOperand_CreateImm0(Inst, off18);
		} else {
			MCOperand_CreateImm0(Inst, off18);
			status = DecodeRegisterClass(Inst, s1_d,
						     &desc->OpInfo[0], Decoder);
			if (status != MCDisassembler_Success)
				return status;
		}
	} else {
		MCOperand_CreateImm0(Inst, off18);
	}

	return MCDisassembler_Success;
}

static DecodeStatus DecodeBInstruction(MCInst *Inst, unsigned Insn,
				       uint64_t Address, void *Decoder)
{
	unsigned disp24_0 = fieldFromInstruction_4(Insn, 16, 16);
	unsigned disp24_1 = fieldFromInstruction_4(Insn, 8, 8);
	unsigned disp24 = (disp24_0 << 0) | (disp24_1 << 16);

	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);

	if (!is32Bit) // This instruction is 32-bit
		return MCDisassembler_Fail;

	// Decode disp24.
	MCOperand_CreateImm0(Inst, disp24);

	return MCDisassembler_Success;
}

static DecodeStatus DecodeBOInstruction(MCInst *Inst, unsigned Insn,
					uint64_t Address, void *Decoder)
{
	DecodeStatus status;
	unsigned off10_0 = fieldFromInstruction_4(Insn, 16, 6);
	unsigned off10_1 = fieldFromInstruction_4(Insn, 28, 4);
	unsigned off10 = (off10_0 << 0) | (off10_1 << 6);
	bool is_store = false;

	unsigned s2 = fieldFromInstruction_4(Insn, 12, 4);
	unsigned s1_d = fieldFromInstruction_4(Insn, 8, 4);

	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);

	if (!is32Bit) // This instruction is 32-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];

	if (desc->NumOperands == 1) {
		return DecodeRegisterClass(Inst, s2, &desc->OpInfo[0], Decoder);
	}

	switch (MCInst_getOpcode(Inst)) {
	case TRICORE_ST_A_bo_r:
	case TRICORE_ST_A_bo_c:
	case TRICORE_ST_B_bo_r:
	case TRICORE_ST_B_bo_c:
	case TRICORE_ST_D_bo_r:
	case TRICORE_ST_D_bo_c:
	case TRICORE_ST_DA_bo_r:
	case TRICORE_ST_DA_bo_c:
	case TRICORE_ST_H_bo_r:
	case TRICORE_ST_H_bo_c:
	case TRICORE_ST_Q_bo_r:
	case TRICORE_ST_Q_bo_c:
	case TRICORE_ST_W_bo_r:
	case TRICORE_ST_W_bo_c:
	case TRICORE_SWAP_W_bo_r:
	case TRICORE_SWAP_W_bo_c:
	case TRICORE_SWAPMSK_W_bo_c:
	case TRICORE_SWAPMSK_W_bo_r: {
		is_store = true;
		break;
	}
	}

	if (desc->NumOperands == 2) {
		if (desc->OpInfo[1].OperandType == MCOI_OPERAND_REGISTER) {
			// we have [reg+r] instruction
			if (is_store) {
				status = DecodeRegisterClass(Inst, s2, &desc->OpInfo[0],
							     Decoder);
				if (status != MCDisassembler_Success)
					return status;
				return DecodeRegisterClass(Inst, s1_d, &desc->OpInfo[1],
							   Decoder);
			} else {
				status = DecodeRegisterClass(Inst, s1_d, &desc->OpInfo[0],
							     Decoder);
				if (status != MCDisassembler_Success)
					return status;
				return DecodeRegisterClass(Inst, s2, &desc->OpInfo[1],
							   Decoder);
			}
		} else {
			// we have one of the CACHE instructions without destination reg
			status = DecodeRegisterClass(Inst, s2, &desc->OpInfo[0],
						     Decoder);
			if (status != MCDisassembler_Success)
				return status;

			MCOperand_CreateImm0(Inst, off10);
		}
		return MCDisassembler_Success;
	}

	if (desc->NumOperands > 2) {
		if (is_store) {
			// we have [reg+c] instruction
			status = DecodeRegisterClass(Inst, s2, &desc->OpInfo[0],
						     Decoder);
			if (status != MCDisassembler_Success)
				return status;

			status = DecodeRegisterClass(Inst, s1_d, &desc->OpInfo[1],
						     Decoder);
			if (status != MCDisassembler_Success)
				return status;
		} else {
			status = DecodeRegisterClass(Inst, s1_d, &desc->OpInfo[0],
						     Decoder);
			if (status != MCDisassembler_Success)
				return status;

			status = DecodeRegisterClass(Inst, s2, &desc->OpInfo[1],
						     Decoder);
			if (status != MCDisassembler_Success)
				return status;
		}
		MCOperand_CreateImm0(Inst, off10);
	}

	return MCDisassembler_Success;
}

static DecodeStatus DecodeBOLInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder)
{
	DecodeStatus status;
	unsigned off16_0 = fieldFromInstruction_4(Insn, 16, 6);
	unsigned off16_1 = fieldFromInstruction_4(Insn, 22, 6);
	unsigned off16_2 = fieldFromInstruction_4(Insn, 28, 4);
	unsigned off16 = (off16_0 << 0) | (off16_1 << 10) | (off16_2 << 6);

	unsigned s2 = fieldFromInstruction_4(Insn, 12, 4);
	unsigned s1_d = fieldFromInstruction_4(Insn, 8, 4);

	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);
	if (!is32Bit) // This instruction is 32-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];

	switch (MCInst_getOpcode(Inst)) {
	case TRICORE_LD_A_bol:
	case TRICORE_LD_B_bol:
	case TRICORE_LD_BU_bol:
	case TRICORE_LD_H_bol:
	case TRICORE_LD_HU_bol:
	case TRICORE_LD_W_bol:
	case TRICORE_LEA_bol: {
		// Decode s1_d.
		status = DecodeRegisterClass(Inst, s1_d, &desc->OpInfo[0],
					     Decoder);
		if (status != MCDisassembler_Success)
			return status;

		// Decode s2.
		status = DecodeRegisterClass(Inst, s2, &desc->OpInfo[1],
					     Decoder);
		if (status != MCDisassembler_Success)
			return status;
		break;
	}
	case TRICORE_ST_A_bol:
	case TRICORE_ST_B_bol:
	case TRICORE_ST_H_bol:
	case TRICORE_ST_W_bol: {
		// Decode s2.
		status = DecodeRegisterClass(Inst, s2, &desc->OpInfo[0],
					     Decoder);
		if (status != MCDisassembler_Success)
			return status;

		// Decode s1_d.
		status = DecodeRegisterClass(Inst, s1_d, &desc->OpInfo[1],
					     Decoder);
		if (status != MCDisassembler_Success)
			return status;
		break;
	}
	default:
		return MCDisassembler_Fail;
	}

	// Decode off16.
	MCOperand_CreateImm0(Inst, off16);

	return MCDisassembler_Success;
}

static DecodeStatus DecodeRCInstruction(MCInst *Inst, unsigned Insn,
					uint64_t Address, void *Decoder)
{
	DecodeStatus status;
	unsigned d = fieldFromInstruction_4(Insn, 28, 4);
	unsigned const9 = fieldFromInstruction_4(Insn, 12, 9);
	unsigned s1 = fieldFromInstruction_4(Insn, 8, 4);

	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);

	if (!is32Bit) // This instruction is 32-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	if (desc->NumOperands > 1) {
		// Decode d.
		status =
			DecodeRegisterClass(Inst, d, &desc->OpInfo[0], Decoder);
		if (status != MCDisassembler_Success)
			return status;

		// Decode s1.
		status = DecodeRegisterClass(Inst, s1, &desc->OpInfo[1],
					     Decoder);
		if (status != MCDisassembler_Success)
			return status;
	}

	// Decode const9.
	MCOperand_CreateImm0(Inst, const9);

	return MCDisassembler_Success;
}

static DecodeStatus DecodeRCPWInstruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder)
{
	DecodeStatus status;
	unsigned d = fieldFromInstruction_4(Insn, 28, 4);
	unsigned pos = fieldFromInstruction_4(Insn, 23, 5);
	unsigned width = fieldFromInstruction_4(Insn, 16, 5);
	unsigned const4 = fieldFromInstruction_4(Insn, 12, 4);
	unsigned s1 = fieldFromInstruction_4(Insn, 8, 4);

	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);

	if (!is32Bit) // This instruction is 32-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	// Decode d.
	status = DecodeRegisterClass(Inst, d, &desc->OpInfo[0], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s1.
	status = DecodeRegisterClass(Inst, s1, &desc->OpInfo[1], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode const4.
	MCOperand_CreateImm0(Inst, const4);

	// Decode pos.
	MCOperand_CreateImm0(Inst, pos);

	// Decode width.
	MCOperand_CreateImm0(Inst, width);

	return MCDisassembler_Success;
}

static DecodeStatus DecodeRLCInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder)
{
	DecodeStatus status;
	unsigned d = fieldFromInstruction_4(Insn, 28, 4);
	unsigned const16 = fieldFromInstruction_4(Insn, 12, 16);
	unsigned s1 = fieldFromInstruction_4(Insn, 8, 4);

	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);

	if (!is32Bit) // This instruction is 32-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	if (desc->NumOperands == 3) {
		status =
			DecodeRegisterClass(Inst, d, &desc->OpInfo[0], Decoder);
		if (status != MCDisassembler_Success)
			return status;

		status = DecodeRegisterClass(Inst, s1, &desc->OpInfo[1],
					     Decoder);
		if (status != MCDisassembler_Success)
			return status;

		MCOperand_CreateImm0(Inst, const16);

		return MCDisassembler_Success;
	}

	if (desc->OpInfo[0].OperandType == MCOI_OPERAND_REGISTER) {
		status =
			DecodeRegisterClass(Inst, d, &desc->OpInfo[0], Decoder);
		if (status != MCDisassembler_Success)
			return status;

		MCOperand_CreateImm0(Inst, const16);
	} else {
		MCOperand_CreateImm0(Inst, const16);
		if (MCInst_getOpcode(Inst) == TRICORE_MTCR_rlc) {
			status =
				DecodeRegisterClass(Inst, s1, &desc->OpInfo[1], Decoder);
		} else {
			status =
				DecodeRegisterClass(Inst, d, &desc->OpInfo[1], Decoder);
		}
		if (status != MCDisassembler_Success)
			return status;
	}
	return MCDisassembler_Success;
}

static DecodeStatus DecodeRRInstruction(MCInst *Inst, unsigned Insn,
					uint64_t Address, void *Decoder)
{
	DecodeStatus status;
	unsigned d = fieldFromInstruction_4(Insn, 28, 4);
	unsigned n = fieldFromInstruction_4(Insn, 16, 2);
	unsigned s2 = fieldFromInstruction_4(Insn, 12, 4);
	unsigned s1 = fieldFromInstruction_4(Insn, 8, 4);

	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);

	if (!is32Bit) // This instruction is 32-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	/// But even if the instruction is in RR format and has only one operand,
	/// we cannot be sure whether the operand is s1 or s2
	if (desc->NumOperands == 1) {
		if (desc->OpInfo[0].OperandType == MCOI_OPERAND_REGISTER) {
			switch (MCInst_getOpcode(Inst)) {
			case TRICORE_CALLI_rr_v110: {
				return DecodeRegisterClass(
					Inst, s2, &desc->OpInfo[0], Decoder);
			}
			default: {
				return DecodeRegisterClass(
					Inst, s1, &desc->OpInfo[0], Decoder);
			}
			}
		}
		return MCDisassembler_Fail;
	}

	if (desc->NumOperands > 0) {
		// Decode d.
		status =
			DecodeRegisterClass(Inst, d, &desc->OpInfo[0], Decoder);
		if (status != MCDisassembler_Success)
			return status;
	}

	if (desc->NumOperands > 1) {
		if (desc->OpInfo[0].OperandType == MCOI_OPERAND_REGISTER) {
			switch (MCInst_getOpcode(Inst)) {
			case TRICORE_ABSS_rr:
			case TRICORE_ABSS_H_rr:
			case TRICORE_ABS_H_rr:
			case TRICORE_ABS_B_rr:
			case TRICORE_ABS_rr: {
				status = DecodeRegisterClass(Inst, s2, &desc->OpInfo[1],
							     Decoder);
				break;
			default:
				status = DecodeRegisterClass(Inst, s1, &desc->OpInfo[1],
							     Decoder);
			}
			if (status != MCDisassembler_Success)
				return status;
			}
		}
	}

	if (desc->NumOperands > 2) {
		status = DecodeRegisterClass(Inst, s2, &desc->OpInfo[2],
					     Decoder);
		if (status != MCDisassembler_Success)
			return status;
	}

	if (desc->NumOperands > 3) {
		MCOperand_CreateImm0(Inst, n);
	}

	return MCDisassembler_Success;
}

static DecodeStatus DecodeRR2Instruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder)
{
	DecodeStatus status;
	unsigned d = fieldFromInstruction_4(Insn, 28, 4);
	unsigned s2 = fieldFromInstruction_4(Insn, 12, 4);
	unsigned s1 = fieldFromInstruction_4(Insn, 8, 4);

	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);

	if (!is32Bit) // This instruction is 32-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	// Decode d.
	status = DecodeRegisterClass(Inst, d, &desc->OpInfo[0], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s1.
	status = DecodeRegisterClass(Inst, s1, &desc->OpInfo[1], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s2.
	status = DecodeRegisterClass(Inst, s2, &desc->OpInfo[2], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	return MCDisassembler_Success;
}

static DecodeStatus DecodeRRPWInstruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder)
{
	DecodeStatus status;
	unsigned d = fieldFromInstruction_4(Insn, 28, 4);
	unsigned pos = fieldFromInstruction_4(Insn, 23, 5);
	unsigned width = fieldFromInstruction_4(Insn, 16, 5);
	unsigned s2 = fieldFromInstruction_4(Insn, 12, 4);
	unsigned s1 = fieldFromInstruction_4(Insn, 8, 4);

	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);

	if (!is32Bit) // This instruction is 32-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	tryDecodeReg(0, d) tryDecodeReg(1, s1) tryDecodeReg(2, s2)
		decodeImm(pos) decodeImm(width)

			return MCDisassembler_Success;
}

static DecodeStatus DecodeSLRInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder)
{
	DecodeStatus status = MCDisassembler_Fail;
	unsigned d = fieldFromInstruction_2(Insn, 8, 4);
	unsigned s2 = fieldFromInstruction_2(Insn, 12, 4);
	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);
	if (is32Bit) // This instruction is 16-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	// Decode d.
	status = DecodeRegisterClass(Inst, d, &desc->OpInfo[0], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s2.
	status = DecodeRegisterClass(Inst, s2, &desc->OpInfo[1], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	return MCDisassembler_Success;
}

static DecodeStatus DecodeSLROInstruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder)
{
	DecodeStatus status = MCDisassembler_Fail;
	unsigned d = fieldFromInstruction_2(Insn, 8, 4);
	unsigned off4 = fieldFromInstruction_2(Insn, 12, 4);
	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);
	if (is32Bit) // This instruction is 16-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	// Decode d.
	status = DecodeRegisterClass(Inst, d, &desc->OpInfo[0], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode off4.
	MCOperand_CreateImm0(Inst, off4);

	return MCDisassembler_Success;
}

static DecodeStatus DecodeSROInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder)
{
	DecodeStatus status = MCDisassembler_Fail;
	unsigned off4 = fieldFromInstruction_2(Insn, 8, 4);
	unsigned s2 = fieldFromInstruction_2(Insn, 12, 4);
	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);
	if (is32Bit) // This instruction is 16-bit
		return MCDisassembler_Fail;

	// Decode s2.
	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	status = DecodeRegisterClass(Inst, s2, &desc->OpInfo[0], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode off4.
	MCOperand_CreateImm0(Inst, off4);

	return MCDisassembler_Success;
}

static DecodeStatus DecodeSRRSInstruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder)
{
	DecodeStatus status = MCDisassembler_Fail;
	unsigned n = fieldFromInstruction_2(Insn, 6, 2);
	unsigned s1_d = fieldFromInstruction_2(Insn, 8, 4);
	unsigned s2 = fieldFromInstruction_2(Insn, 12, 4);
	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);
	if (is32Bit) // This instruction is 16-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];

	// Decode s1_d.
	status = DecodeRegisterClass(Inst, s1_d, &desc->OpInfo[0], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s2.
	status = DecodeRegisterClass(Inst, s2, &desc->OpInfo[1], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode n.
	MCOperand_CreateImm0(Inst, n);

	return MCDisassembler_Success;
}

static DecodeStatus DecodeSBCInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder)
{
	unsigned disp4 = fieldFromInstruction_2(Insn, 8, 4);
	unsigned const4 = fieldFromInstruction_2(Insn, 12, 4);
	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);
	if (is32Bit) // This instruction is 16-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	if (desc->NumOperands != 2) {
		return MCDisassembler_Fail;
	}

	// Decode disp4.
	MCOperand_CreateImm0(Inst, disp4);

	// Decode const4.
	MCOperand_CreateImm0(Inst, const4);

	return MCDisassembler_Success;
}

static DecodeStatus DecodeSBRNInstruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder)
{
	unsigned disp4 = fieldFromInstruction_2(Insn, 8, 4);
	unsigned n = fieldFromInstruction_2(Insn, 12, 4);
	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);
	if (is32Bit) // This instruction is 16-bit
		return MCDisassembler_Fail;

	// Decode n.
	MCOperand_CreateImm0(Inst, n);
	// Decode disp4.
	MCOperand_CreateImm0(Inst, disp4);

	return MCDisassembler_Success;
}

static DecodeStatus DecodeSSRInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder)
{
	DecodeStatus status = MCDisassembler_Fail;
	unsigned s1 = fieldFromInstruction_2(Insn, 8, 4);
	unsigned s2 = fieldFromInstruction_2(Insn, 12, 4);
	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);
	if (is32Bit) // This instruction is 16-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];

	// Decode s2.
	status = DecodeRegisterClass(Inst, s2, &desc->OpInfo[0], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s1.
	status = DecodeRegisterClass(Inst, s1, &desc->OpInfo[1], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	return MCDisassembler_Success;
}

static DecodeStatus DecodeSSROInstruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder)
{
	DecodeStatus status = MCDisassembler_Fail;
	unsigned s1 = fieldFromInstruction_2(Insn, 8, 4);
	unsigned off4 = fieldFromInstruction_2(Insn, 12, 4);
	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);
	if (is32Bit) // This instruction is 16-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	status = DecodeRegisterClass(Inst, s1, &desc->OpInfo[0], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode off4.
	MCOperand_CreateImm0(Inst, off4);

	return MCDisassembler_Success;
}

/// 32-bit Opcode Format

static DecodeStatus DecodeSYSInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder)
{
	DecodeStatus status = MCDisassembler_Fail;
	unsigned s1_d = fieldFromInstruction_4(Insn, 8, 4);

	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);
	if (!is32Bit) // This instruction is 32-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	if (desc->NumOperands > 0) {
		status = DecodeRegisterClass(Inst, s1_d, &desc->OpInfo[0],
					     Decoder);
		if (status != MCDisassembler_Success)
			return status;
	}

	return MCDisassembler_Success;
}

static DecodeStatus DecodeRRR2Instruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder)
{
	DecodeStatus status = MCDisassembler_Fail;
	unsigned s1 = fieldFromInstruction_4(Insn, 8, 4);
	unsigned s2 = fieldFromInstruction_4(Insn, 12, 4);
	unsigned s3 = fieldFromInstruction_4(Insn, 24, 4);
	unsigned d = fieldFromInstruction_4(Insn, 28, 4);

	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);
	if (!is32Bit) // This instruction is 32-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	status = DecodeRegisterClass(Inst, d, &desc->OpInfo[0], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s1.
	status = DecodeRegisterClass(Inst, s1, &desc->OpInfo[1], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s2.
	status = DecodeRegisterClass(Inst, s2, &desc->OpInfo[2], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s3.
	status = DecodeRegisterClass(Inst, s3, &desc->OpInfo[3], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	return MCDisassembler_Success;
}

static DecodeStatus DecodeRRR1Instruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder)
{
	DecodeStatus status = MCDisassembler_Fail;
	unsigned s1 = fieldFromInstruction_4(Insn, 8, 4);
	unsigned s2 = fieldFromInstruction_4(Insn, 12, 4);
	unsigned n = fieldFromInstruction_4(Insn, 16, 2);
	unsigned s3 = fieldFromInstruction_4(Insn, 24, 4);
	unsigned d = fieldFromInstruction_4(Insn, 28, 4);

	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);
	if (!is32Bit) // This instruction is 32-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	status = DecodeRegisterClass(Inst, d, &desc->OpInfo[0], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s1.
	status = DecodeRegisterClass(Inst, s1, &desc->OpInfo[1], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s2.
	status = DecodeRegisterClass(Inst, s2, &desc->OpInfo[2], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s3.
	status = DecodeRegisterClass(Inst, s3, &desc->OpInfo[3], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode n.
	MCOperand_CreateImm0(Inst, n);

	return MCDisassembler_Success;
}

static DecodeStatus DecodeBITInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder)
{
	DecodeStatus status = MCDisassembler_Fail;
	unsigned s1 = fieldFromInstruction_4(Insn, 8, 4);
	unsigned s2 = fieldFromInstruction_4(Insn, 12, 4);
	unsigned pos1 = fieldFromInstruction_4(Insn, 16, 5);
	unsigned pos2 = fieldFromInstruction_4(Insn, 23, 5);
	unsigned d = fieldFromInstruction_4(Insn, 28, 4);

	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);
	if (!is32Bit) // This instruction is 32-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	status = DecodeRegisterClass(Inst, d, &desc->OpInfo[0], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s1.
	status = DecodeRegisterClass(Inst, s1, &desc->OpInfo[1], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s2.
	status = DecodeRegisterClass(Inst, s2, &desc->OpInfo[2], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode pos1.
	MCOperand_CreateImm0(Inst, pos1);

	// Decode pos2.
	MCOperand_CreateImm0(Inst, pos2);

	return MCDisassembler_Success;
}

static DecodeStatus DecodeRR1Instruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder)
{
	DecodeStatus status = MCDisassembler_Fail;
	unsigned s1 = fieldFromInstruction_4(Insn, 8, 4);
	unsigned s2 = fieldFromInstruction_4(Insn, 12, 4);
	unsigned n = fieldFromInstruction_4(Insn, 16, 2);
	unsigned d = fieldFromInstruction_4(Insn, 28, 4);

	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);
	if (!is32Bit) // This instruction is 32-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	status = DecodeRegisterClass(Inst, d, &desc->OpInfo[0], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s1.
	status = DecodeRegisterClass(Inst, s1, &desc->OpInfo[1], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s2.
	status = DecodeRegisterClass(Inst, s2, &desc->OpInfo[2], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode n.
	MCOperand_CreateImm0(Inst, n);

	return MCDisassembler_Success;
}

static DecodeStatus DecodeRCRInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder)
{
	DecodeStatus status = MCDisassembler_Fail;
	unsigned s1 = fieldFromInstruction_4(Insn, 8, 4);
	unsigned const9 = fieldFromInstruction_4(Insn, 12, 9);
	unsigned s3 = fieldFromInstruction_4(Insn, 24, 4);
	unsigned d = fieldFromInstruction_4(Insn, 28, 4);

	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);
	if (!is32Bit) // This instruction is 32-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	status = DecodeRegisterClass(Inst, d, &desc->OpInfo[0], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s1.
	status = DecodeRegisterClass(Inst, s1, &desc->OpInfo[1], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s3.
	status = DecodeRegisterClass(Inst, s3, &desc->OpInfo[2], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode const9.
	MCOperand_CreateImm0(Inst, const9);

	return MCDisassembler_Success;
}

static DecodeStatus DecodeRRRWInstruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder)
{
	DecodeStatus status = MCDisassembler_Fail;
	unsigned s1 = fieldFromInstruction_4(Insn, 8, 4);
	unsigned s2 = fieldFromInstruction_4(Insn, 12, 4);
	unsigned width = fieldFromInstruction_4(Insn, 16, 5);
	unsigned s3 = fieldFromInstruction_4(Insn, 24, 4);
	unsigned d = fieldFromInstruction_4(Insn, 28, 4);

	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);
	if (!is32Bit) // This instruction is 32-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	status = DecodeRegisterClass(Inst, d, &desc->OpInfo[0], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s1.
	status = DecodeRegisterClass(Inst, s1, &desc->OpInfo[1], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s2.
	status = DecodeRegisterClass(Inst, s2, &desc->OpInfo[2], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s3.
	status = DecodeRegisterClass(Inst, s3, &desc->OpInfo[3], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode width.
	MCOperand_CreateImm0(Inst, width);

	return MCDisassembler_Success;
}

static DecodeStatus DecodeRCRRInstruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder)
{
	DecodeStatus status = MCDisassembler_Fail;
	unsigned s1 = fieldFromInstruction_4(Insn, 8, 4);
	unsigned const4 = fieldFromInstruction_4(Insn, 12, 4);
	unsigned s3 = fieldFromInstruction_4(Insn, 24, 4);
	unsigned d = fieldFromInstruction_4(Insn, 28, 4);

	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);
	if (!is32Bit) // This instruction is 32-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	status = DecodeRegisterClass(Inst, d, &desc->OpInfo[0], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s1.
	status = DecodeRegisterClass(Inst, s1, &desc->OpInfo[1], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode const4.
	MCOperand_CreateImm0(Inst, const4);

	// Decode s3.
	status = DecodeRegisterClass(Inst, s3, &desc->OpInfo[3], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	return MCDisassembler_Success;
}

static DecodeStatus DecodeRRRRInstruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder)
{
	DecodeStatus status = MCDisassembler_Fail;
	unsigned s1 = fieldFromInstruction_4(Insn, 8, 4);
	unsigned s2 = fieldFromInstruction_4(Insn, 12, 4);
	unsigned s3 = fieldFromInstruction_4(Insn, 24, 4);
	unsigned d = fieldFromInstruction_4(Insn, 28, 4);

	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);
	if (!is32Bit) // This instruction is 32-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	status = DecodeRegisterClass(Inst, d, &desc->OpInfo[0], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s1.
	status = DecodeRegisterClass(Inst, s1, &desc->OpInfo[1], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	if (desc->NumOperands == 3) {
		switch (MCInst_getOpcode(Inst)) {
		case TRICORE_EXTR_rrrr:
		case TRICORE_EXTR_U_rrrr:
			return DecodeRegisterClass(Inst, s3, &desc->OpInfo[2], Decoder);
		default:
			return DecodeRegisterClass(Inst, s2, &desc->OpInfo[2], Decoder);
		}
	}

	// Decode s2.
	status = DecodeRegisterClass(Inst, s2, &desc->OpInfo[2], Decoder);
	if (status != MCDisassembler_Success)
		return status;
	// Decode s3.
	status = DecodeRegisterClass(Inst, s3, &desc->OpInfo[3], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	return MCDisassembler_Success;
}

static DecodeStatus DecodeBRRInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder)
{
	DecodeStatus status = MCDisassembler_Fail;
	unsigned s1 = fieldFromInstruction_4(Insn, 8, 4);
	unsigned s2 = fieldFromInstruction_4(Insn, 12, 4);
	unsigned disp15 = fieldFromInstruction_4(Insn, 16, 15);

	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);
	if (!is32Bit) // This instruction is 32-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	if (MCInst_getOpcode(Inst) == TRICORE_LOOP_brr) {
		status = DecodeRegisterClass(Inst, s2, &desc->OpInfo[0],
					     Decoder);
		if (status != MCDisassembler_Success)
			return status;

		MCOperand_CreateImm0(Inst, disp15);
		return MCDisassembler_Success;
	}

	if (desc->NumOperands >= 2) {
		status = DecodeRegisterClass(Inst, s1, &desc->OpInfo[0],
					     Decoder);
		if (status != MCDisassembler_Success)
			return status;

		if (desc->NumOperands >= 3) {
			status = DecodeRegisterClass(Inst, s2, &desc->OpInfo[1],
						     Decoder);
			if (status != MCDisassembler_Success)
				return status;
		}
	}

	// Decode disp15.
	MCOperand_CreateImm0(Inst, disp15);

	return MCDisassembler_Success;
}

static DecodeStatus DecodeBRCInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder)
{
	DecodeStatus status = MCDisassembler_Fail;
	unsigned s1 = fieldFromInstruction_4(Insn, 8, 4);
	unsigned const4 = fieldFromInstruction_4(Insn, 12, 4);
	unsigned disp15 = fieldFromInstruction_4(Insn, 16, 15);

	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);
	if (!is32Bit) // This instruction is 32-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	status = DecodeRegisterClass(Inst, s1, &desc->OpInfo[0], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode const4.
	MCOperand_CreateImm0(Inst, const4);

	// Decode disp15.
	MCOperand_CreateImm0(Inst, disp15);

	return MCDisassembler_Success;
}

static DecodeStatus DecodeRRRInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder)
{
	DecodeStatus status = MCDisassembler_Fail;
	unsigned s1 = fieldFromInstruction_4(Insn, 8, 4);
	unsigned s2 = fieldFromInstruction_4(Insn, 12, 4);
	//	unsigned n = fieldFromInstruction_4(Insn, 16, 2);
	unsigned s3 = fieldFromInstruction_4(Insn, 24, 4);
	unsigned d = fieldFromInstruction_4(Insn, 28, 4);

	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);
	if (!is32Bit) // This instruction is 32-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	status = DecodeRegisterClass(Inst, d, &desc->OpInfo[0], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s1.
	status = DecodeRegisterClass(Inst, s1, &desc->OpInfo[1], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s2.
	status = DecodeRegisterClass(Inst, s2, &desc->OpInfo[2], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode s3.
	status = DecodeRegisterClass(Inst, s3, &desc->OpInfo[3], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	return MCDisassembler_Success;
}

static DecodeStatus DecodeABSBInstruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder)
{
	unsigned bpos3 = fieldFromInstruction_4(Insn, 8, 3);
	unsigned b = fieldFromInstruction_4(Insn, 12, 1);

	unsigned off18_0_5 = fieldFromInstruction_4(Insn, 16, 6);
	unsigned off18_6_9 = fieldFromInstruction_4(Insn, 28, 4);
	unsigned off18_10_13 = fieldFromInstruction_4(Insn, 22, 4);
	unsigned off18_14_17 = fieldFromInstruction_4(Insn, 12, 4);
	unsigned off18 = (off18_0_5 << 0) | (off18_6_9 << 6) |
			 (off18_10_13 << 10) | (off18_14_17 << 14);

	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);
	if (!is32Bit) // This instruction is 32-bit
		return MCDisassembler_Fail;

	decodeImm(off18) decodeImm(bpos3) decodeImm(b)

		return MCDisassembler_Success;
}

static DecodeStatus DecodeRCRWInstruction(MCInst *Inst, unsigned Insn,
					  uint64_t Address, void *Decoder)
{
	DecodeStatus status = MCDisassembler_Fail;
	unsigned s1 = fieldFromInstruction_4(Insn, 8, 4);
	unsigned const4 = fieldFromInstruction_4(Insn, 12, 4);
	unsigned width = fieldFromInstruction_4(Insn, 16, 5);
	unsigned s3 = fieldFromInstruction_4(Insn, 24, 4);
	unsigned d = fieldFromInstruction_4(Insn, 28, 4);

	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);
	if (!is32Bit) // This instruction is 32-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	tryDecodeReg(0, d) tryDecodeReg(1, s1) tryDecodeReg(2, s3)
		decodeImm(const4) decodeImm(width)

			return MCDisassembler_Success;
}

static DecodeStatus DecodeBRNInstruction(MCInst *Inst, unsigned Insn,
					 uint64_t Address, void *Decoder)
{
	DecodeStatus status = MCDisassembler_Fail;
	unsigned s1 = fieldFromInstruction_4(Insn, 8, 4);

	unsigned n_0_3 = fieldFromInstruction_4(Insn, 12, 4);
	unsigned n_4 = fieldFromInstruction_4(Insn, 7, 1);
	unsigned n = (n_0_3 << 0) | (n_4 << 4);

	unsigned disp15 = fieldFromInstruction_4(Insn, 16, 15);

	unsigned is32Bit = fieldFromInstruction_4(Insn, 0, 1);
	if (!is32Bit) // This instruction is 32-bit
		return MCDisassembler_Fail;

	const MCInstrDesc *desc = &TriCoreInsts[MCInst_getOpcode(Inst)];
	status = DecodeRegisterClass(Inst, s1, &desc->OpInfo[0], Decoder);
	if (status != MCDisassembler_Success)
		return status;

	// Decode n.
	MCOperand_CreateImm0(Inst, n);

	// Decode disp15.
	MCOperand_CreateImm0(Inst, disp15);

	return MCDisassembler_Success;
}

#define GET_SUBTARGETINFO_ENUM

#include "TriCoreGenInstrInfo.inc"

static inline bool tryGetInstruction16(const uint8_t *code, size_t code_len,
				       MCInst *MI, uint16_t *size,
				       uint64_t address, void *info,
				       const uint8_t *decoderTable16)
{
	if (code_len < 2) {
		return false;
	}
	uint16_t insn16 = readBytes16(MI, code);
	DecodeStatus Result = decodeInstruction_2(decoderTable16, MI, insn16,
						  address, info, 0);
	if (Result != MCDisassembler_Fail) {
		*size = 2;
		return true;
	}
	return false;
}

static inline bool tryGetInstruction32(const uint8_t *code, size_t code_len,
				       MCInst *MI, uint16_t *size,
				       uint64_t address, void *info,
				       const uint8_t *decoderTable32)
{
	if (code_len < 4) {
		return false;
	}
	uint32_t insn32 = readBytes32(MI, code);
	DecodeStatus Result = decodeInstruction_4(decoderTable32, MI, insn32,
						  address, info, 0);
	if (Result != MCDisassembler_Fail) {
		*size = 4;
		return true;
	}
	return false;
}

static bool getInstruction(csh ud, const uint8_t *code, size_t code_len,
			   MCInst *MI, uint16_t *size, uint64_t address,
			   void *info)
{
	if (!ud) {
		return false;
	}

	struct cs_struct *cs = (struct cs_struct *)ud;
	if (MI->flat_insn->detail) {
		memset(MI->flat_insn->detail, 0, sizeof(cs_detail));
	}

	switch (cs->mode) {
	case CS_MODE_TRICORE_110: {
		if (tryGetInstruction16(code, code_len, MI, size, address, info,
					DecoderTablev11016) ||
		    tryGetInstruction32(code, code_len, MI, size, address, info,
					DecoderTablev11032)) {
			return true;
		}
		break;
	}
	case CS_MODE_TRICORE_161: {
		if (tryGetInstruction32(code, code_len, MI, size, address, info,
					DecoderTablev16132)) {
			return true;
		}
		break;
	}
	case CS_MODE_TRICORE_162: {
		if (tryGetInstruction16(code, code_len, MI, size, address, info,
					DecoderTablev16216) ||
		    tryGetInstruction32(code, code_len, MI, size, address, info,
					DecoderTablev16232)) {
			return true;
		}
		break;
	}
	default:
		break;
	}

	return tryGetInstruction16(code, code_len, MI, size, address, info,
				   DecoderTable16) ||
	       tryGetInstruction32(code, code_len, MI, size, address, info,
				   DecoderTable32);
}

bool TriCore_LLVM_getInstruction(csh handle, const uint8_t *Bytes,
				 size_t ByteLen, MCInst *MI, uint16_t *Size,
				 uint64_t Address, void *Info)
{
	bool Result =
		getInstruction(handle, Bytes, ByteLen, MI, Size, Address, Info);
	if (Result) {
		TriCore_set_instr_map_data(MI);
	}
	return Result;
}

void TriCore_init_mri(MCRegisterInfo *MRI)
{
	/*
	InitMCRegisterInfo(TriCoreRegDesc, 45, RA, PC,
			TriCoreMCRegisterClasses, 4,
			TriCoreRegUnitRoots,
			16,
			TriCoreRegDiffLists,
			TriCoreRegStrings,
			TriCoreSubRegIdxLists,
			1,
			TriCoreSubRegIdxRanges,
			TriCoreRegEncodingTable);
	*/

	MCRegisterInfo_InitMCRegisterInfo(
		MRI, TriCoreRegDesc, ARR_SIZE(TriCoreRegDesc), 0, 0,
		TriCoreMCRegisterClasses, ARR_SIZE(TriCoreMCRegisterClasses), 0,
		0, TriCoreRegDiffLists, 0, TriCoreSubRegIdxLists, 1, 0);
}

#endif
