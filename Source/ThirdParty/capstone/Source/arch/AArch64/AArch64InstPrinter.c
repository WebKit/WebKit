//==-- AArch64InstPrinter.cpp - Convert AArch64 MCInst to assembly syntax --==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This class prints an AArch64 MCInst to a .s file.
//
//===----------------------------------------------------------------------===//

/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2016 */

#ifdef CAPSTONE_HAS_ARM64

#include <capstone/platform.h>
#include <stdio.h>
#include <stdlib.h>

#include "AArch64InstPrinter.h"
#include "AArch64Disassembler.h"
#include "AArch64BaseInfo.h"
#include "../../utils.h"
#include "../../MCInst.h"
#include "../../SStream.h"
#include "../../MCRegisterInfo.h"
#include "../../MathExtras.h"

#include "AArch64Mapping.h"
#include "AArch64AddressingModes.h"

#define GET_REGINFO_ENUM
#include "AArch64GenRegisterInfo.inc"

#define GET_INSTRINFO_ENUM
#include "AArch64GenInstrInfo.inc"

#include "AArch64GenSubtargetInfo.inc"


static const char *getRegisterName(unsigned RegNo, unsigned AltIdx);
static void printOperand(MCInst *MI, unsigned OpNum, SStream *O);
static bool printSysAlias(MCInst *MI, SStream *O);
static char *printAliasInstr(MCInst *MI, SStream *OS, MCRegisterInfo *MRI);
static void printInstruction(MCInst *MI, SStream *O);
static void printShifter(MCInst *MI, unsigned OpNum, SStream *O);
static void printCustomAliasOperand(MCInst *MI, uint64_t Address, unsigned OpIdx,
		unsigned PrintMethodIdx, SStream *OS);


static cs_ac_type get_op_access(cs_struct *h, unsigned int id, unsigned int index)
{
#ifndef CAPSTONE_DIET
	const uint8_t *arr = AArch64_get_op_access(h, id);

	if (arr[index] == CS_AC_IGNORE)
		return 0;

	return arr[index];
#else
	return 0;
#endif
}

static void op_addImm(MCInst *MI, int v)
{
	if (MI->csh->detail) {
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = v;
		MI->flat_insn->detail->arm64.op_count++;
	}
}

static void set_sme_index(MCInst *MI, bool status)
{
	// Doing SME Index operand
	MI->csh->doing_SME_Index = status;

	if (MI->csh->detail != CS_OPT_ON)
		return;

	if (status) {
		unsigned prevOpNum = MI->flat_insn->detail->arm64.op_count - 1; 
		unsigned Reg = MCOperand_getReg(MCInst_getOperand(MI, prevOpNum));
		// Replace previous SME register operand with an OP_SME_INDEX operand
		MI->flat_insn->detail->arm64.operands[prevOpNum].type = ARM64_OP_SME_INDEX;
		MI->flat_insn->detail->arm64.operands[prevOpNum].sme_index.reg = Reg;
		MI->flat_insn->detail->arm64.operands[prevOpNum].sme_index.base = ARM64_REG_INVALID;
		MI->flat_insn->detail->arm64.operands[prevOpNum].sme_index.disp = 0;
	}
}

static void set_mem_access(MCInst *MI, bool status)
{
	// If status == false, check if this is meant for SME_index
	if(!status && MI->csh->doing_SME_Index) {
		MI->csh->doing_SME_Index = status;
		return;
	}

	// Doing Memory Operation
	MI->csh->doing_mem = status;


	if (MI->csh->detail != CS_OPT_ON)
		return;

	if (status) {
#ifndef CAPSTONE_DIET
		uint8_t access;
		access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
		MI->ac_idx++;
#endif
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_MEM;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].mem.base = ARM64_REG_INVALID;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].mem.index = ARM64_REG_INVALID;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].mem.disp = 0;
	} else {
		// done, create the next operand slot
		MI->flat_insn->detail->arm64.op_count++;
	}
}

void AArch64_printInst(MCInst *MI, SStream *O, void *Info)
{
	// Check for special encodings and print the canonical alias instead.
	unsigned Opcode = MCInst_getOpcode(MI);
	int LSB, Width;
	char *mnem;

	// printf(">>> opcode = %u\n", MCInst_getOpcode(MI));

	if (Opcode == AArch64_SYSxt && printSysAlias(MI, O))
		return;

	// SBFM/UBFM should print to a nicer aliased form if possible.
	if (Opcode == AArch64_SBFMXri || Opcode == AArch64_SBFMWri ||
			Opcode == AArch64_UBFMXri || Opcode == AArch64_UBFMWri) {
		bool IsSigned = (Opcode == AArch64_SBFMXri || Opcode == AArch64_SBFMWri);
		bool Is64Bit = (Opcode == AArch64_SBFMXri || Opcode == AArch64_UBFMXri);

		MCOperand *Op0 = MCInst_getOperand(MI, 0);
		MCOperand *Op1 = MCInst_getOperand(MI, 1);
		MCOperand *Op2 = MCInst_getOperand(MI, 2);
		MCOperand *Op3 = MCInst_getOperand(MI, 3);

		if (MCOperand_isImm(Op2) && MCOperand_getImm(Op2) == 0 && MCOperand_isImm(Op3)) {
			const char *AsmMnemonic = NULL;

			switch (MCOperand_getImm(Op3)) {
				default:
					break;

				case 7:
					if (IsSigned)
						AsmMnemonic = "sxtb";
					else if (!Is64Bit)
						AsmMnemonic = "uxtb";
					break;

				case 15:
					if (IsSigned)
						AsmMnemonic = "sxth";
					else if (!Is64Bit)
						AsmMnemonic = "uxth";
					break;

				case 31:
					// *xtw is only valid for signed 64-bit operations.
					if (Is64Bit && IsSigned)
						AsmMnemonic = "sxtw";
					break;
			}

			if (AsmMnemonic) {
				SStream_concat(O, "%s\t%s, %s", AsmMnemonic,
						getRegisterName(MCOperand_getReg(Op0), AArch64_NoRegAltName),
						getRegisterName(getWRegFromXReg(MCOperand_getReg(Op1)), AArch64_NoRegAltName));

				if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
					uint8_t access;
					access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
					MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
					MI->ac_idx++;
#endif
					MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
					MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = MCOperand_getReg(Op0);
					MI->flat_insn->detail->arm64.op_count++;
#ifndef CAPSTONE_DIET
					access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
					MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
					MI->ac_idx++;
#endif
					MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
					MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = getWRegFromXReg(MCOperand_getReg(Op1));
					MI->flat_insn->detail->arm64.op_count++;
				}

				MCInst_setOpcodePub(MI, AArch64_map_insn(AsmMnemonic));

				return;
			}
		}

		// All immediate shifts are aliases, implemented using the Bitfield
		// instruction. In all cases the immediate shift amount shift must be in
		// the range 0 to (reg.size -1).
		if (MCOperand_isImm(Op2) && MCOperand_isImm(Op3)) {
			const char *AsmMnemonic = NULL;
			int shift = 0;
			int immr = (int)MCOperand_getImm(Op2);
			int imms = (int)MCOperand_getImm(Op3);

			if (Opcode == AArch64_UBFMWri && imms != 0x1F && ((imms + 1) == immr)) {
				AsmMnemonic = "lsl";
				shift = 31 - imms;
			} else if (Opcode == AArch64_UBFMXri && imms != 0x3f &&
					((imms + 1 == immr))) {
				AsmMnemonic = "lsl";
				shift = 63 - imms;
			} else if (Opcode == AArch64_UBFMWri && imms == 0x1f) {
				AsmMnemonic = "lsr";
				shift = immr;
			} else if (Opcode == AArch64_UBFMXri && imms == 0x3f) {
				AsmMnemonic = "lsr";
				shift = immr;
			} else if (Opcode == AArch64_SBFMWri && imms == 0x1f) {
				AsmMnemonic = "asr";
				shift = immr;
			} else if (Opcode == AArch64_SBFMXri && imms == 0x3f) {
				AsmMnemonic = "asr";
				shift = immr;
			}

			if (AsmMnemonic) {
				SStream_concat(O, "%s\t%s, %s, ", AsmMnemonic,
						getRegisterName(MCOperand_getReg(Op0), AArch64_NoRegAltName),
						getRegisterName(MCOperand_getReg(Op1), AArch64_NoRegAltName));

				printInt32Bang(O, shift);

				MCInst_setOpcodePub(MI, AArch64_map_insn(AsmMnemonic));

				if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
					uint8_t access;
					access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
					MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
					MI->ac_idx++;
#endif
					MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
					MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = MCOperand_getReg(Op0);
					MI->flat_insn->detail->arm64.op_count++;
#ifndef CAPSTONE_DIET
					access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
					MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
					MI->ac_idx++;
#endif
					MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
					MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = MCOperand_getReg(Op1);
					MI->flat_insn->detail->arm64.op_count++;
#ifndef CAPSTONE_DIET
					access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
					MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
					MI->ac_idx++;
#endif
					MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
					MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = shift;
					MI->flat_insn->detail->arm64.op_count++;
				}

				return;
			}
		}

		// SBFIZ/UBFIZ aliases
		if (MCOperand_getImm(Op2) > MCOperand_getImm(Op3)) {
			SStream_concat(O, "%s\t%s, %s, ", (IsSigned ? "sbfiz" : "ubfiz"),
					getRegisterName(MCOperand_getReg(Op0), AArch64_NoRegAltName),
					getRegisterName(MCOperand_getReg(Op1), AArch64_NoRegAltName));

			printInt32Bang(O, (int)((Is64Bit ? 64 : 32) - MCOperand_getImm(Op2)));

			SStream_concat0(O, ", ");

			printInt32Bang(O, (int)MCOperand_getImm(Op3) + 1);

			MCInst_setOpcodePub(MI, AArch64_map_insn(IsSigned ? "sbfiz" : "ubfiz"));

			if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
				uint8_t access;
				access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
				MI->ac_idx++;
#endif
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = MCOperand_getReg(Op0);
				MI->flat_insn->detail->arm64.op_count++;
#ifndef CAPSTONE_DIET
				access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
				MI->ac_idx++;
#endif
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = MCOperand_getReg(Op1);
				MI->flat_insn->detail->arm64.op_count++;
#ifndef CAPSTONE_DIET
				access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
				MI->ac_idx++;
#endif
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = (Is64Bit ? 64 : 32) - (int)MCOperand_getImm(Op2);
				MI->flat_insn->detail->arm64.op_count++;
#ifndef CAPSTONE_DIET
				access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
				MI->ac_idx++;
#endif
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = MCOperand_getImm(Op3) + 1;
				MI->flat_insn->detail->arm64.op_count++;
			}

			return;
		}

		// Otherwise SBFX/UBFX is the preferred form
		SStream_concat(O, "%s\t%s, %s, ", (IsSigned ? "sbfx" : "ubfx"),
				getRegisterName(MCOperand_getReg(Op0), AArch64_NoRegAltName),
				getRegisterName(MCOperand_getReg(Op1), AArch64_NoRegAltName));

		printInt32Bang(O, (int)MCOperand_getImm(Op2));
		SStream_concat0(O, ", ");
		printInt32Bang(O, (int)MCOperand_getImm(Op3) - (int)MCOperand_getImm(Op2) + 1);

		MCInst_setOpcodePub(MI, AArch64_map_insn(IsSigned ? "sbfx" : "ubfx"));

		if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
			uint8_t access;
			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = MCOperand_getReg(Op0);
			MI->flat_insn->detail->arm64.op_count++;
#ifndef CAPSTONE_DIET
			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = MCOperand_getReg(Op1);
			MI->flat_insn->detail->arm64.op_count++;
#ifndef CAPSTONE_DIET
			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = MCOperand_getImm(Op2);
			MI->flat_insn->detail->arm64.op_count++;
#ifndef CAPSTONE_DIET
			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = MCOperand_getImm(Op3) - MCOperand_getImm(Op2) + 1;
			MI->flat_insn->detail->arm64.op_count++;
		}

		return;
	}

	if (Opcode == AArch64_BFMXri || Opcode == AArch64_BFMWri) {
		MCOperand *Op0 = MCInst_getOperand(MI, 0); // Op1 == Op0
		MCOperand *Op2 = MCInst_getOperand(MI, 2);
		int ImmR = (int)MCOperand_getImm(MCInst_getOperand(MI, 3));
		int ImmS = (int)MCOperand_getImm(MCInst_getOperand(MI, 4));

		if ((MCOperand_getReg(Op2) == AArch64_WZR || MCOperand_getReg(Op2) == AArch64_XZR) &&
				(ImmR == 0 || ImmS < ImmR)) {
			// BFC takes precedence over its entire range, sligtly differently to BFI.
			int BitWidth = Opcode == AArch64_BFMXri ? 64 : 32;
			int LSB = (BitWidth - ImmR) % BitWidth;
			int Width = ImmS + 1;

			SStream_concat(O, "bfc\t%s, ",
					getRegisterName(MCOperand_getReg(Op0), AArch64_NoRegAltName));

			printInt32Bang(O, LSB);
			SStream_concat0(O, ", ");
			printInt32Bang(O, Width);
			MCInst_setOpcodePub(MI, AArch64_map_insn("bfc"));

			if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
				uint8_t access;
				access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
				MI->ac_idx++;
#endif
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = MCOperand_getReg(Op0);
				MI->flat_insn->detail->arm64.op_count++;

#ifndef CAPSTONE_DIET
				access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
				MI->ac_idx++;
#endif
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = LSB;
				MI->flat_insn->detail->arm64.op_count++;
#ifndef CAPSTONE_DIET
				access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
				MI->ac_idx++;
#endif
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = Width;
				MI->flat_insn->detail->arm64.op_count++;
			}

			return;
		} else if (ImmS < ImmR) {
			// BFI alias
			int BitWidth = Opcode == AArch64_BFMXri ? 64 : 32;
			LSB = (BitWidth - ImmR) % BitWidth;
			Width = ImmS + 1;

			SStream_concat(O, "bfi\t%s, %s, ",
					getRegisterName(MCOperand_getReg(Op0), AArch64_NoRegAltName),
					getRegisterName(MCOperand_getReg(Op2), AArch64_NoRegAltName));

			printInt32Bang(O, LSB);
			SStream_concat0(O, ", ");
			printInt32Bang(O, Width);

			MCInst_setOpcodePub(MI, AArch64_map_insn("bfi"));

			if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
				uint8_t access;
				access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
				MI->ac_idx++;
#endif
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = MCOperand_getReg(Op0);
				MI->flat_insn->detail->arm64.op_count++;
#ifndef CAPSTONE_DIET
				access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
				MI->ac_idx++;
#endif
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = MCOperand_getReg(Op2);
				MI->flat_insn->detail->arm64.op_count++;
#ifndef CAPSTONE_DIET
				access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
				MI->ac_idx++;
#endif
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = LSB;
				MI->flat_insn->detail->arm64.op_count++;
#ifndef CAPSTONE_DIET
				access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
				MI->ac_idx++;
#endif
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = Width;
				MI->flat_insn->detail->arm64.op_count++;
			}

			return;
		}

		LSB = ImmR;
		Width = ImmS - ImmR + 1;
		// Otherwise BFXIL the preferred form
		SStream_concat(O, "bfxil\t%s, %s, ",
				getRegisterName(MCOperand_getReg(Op0), AArch64_NoRegAltName),
				getRegisterName(MCOperand_getReg(Op2), AArch64_NoRegAltName));

		printInt32Bang(O, LSB);
		SStream_concat0(O, ", ");
		printInt32Bang(O, Width);

		MCInst_setOpcodePub(MI, AArch64_map_insn("bfxil"));

		if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
			uint8_t access;
			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = MCOperand_getReg(Op0);
			MI->flat_insn->detail->arm64.op_count++;
#ifndef CAPSTONE_DIET
			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = MCOperand_getReg(Op2);
			MI->flat_insn->detail->arm64.op_count++;
#ifndef CAPSTONE_DIET
			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = LSB;
			MI->flat_insn->detail->arm64.op_count++;
#ifndef CAPSTONE_DIET
			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = Width;
			MI->flat_insn->detail->arm64.op_count++;
		}

		return;
	}

	// MOVZ, MOVN and "ORR wzr, #imm" instructions are aliases for MOV, but their
	// domains overlap so they need to be prioritized. The chain is "MOVZ lsl #0 >
	// MOVZ lsl #N > MOVN lsl #0 > MOVN lsl #N > ORR". The highest instruction
	// that can represent the move is the MOV alias, and the rest get printed
	// normally.
	if ((Opcode == AArch64_MOVZXi || Opcode == AArch64_MOVZWi) &&
			MCOperand_isImm(MCInst_getOperand(MI, 1)) && MCOperand_isImm(MCInst_getOperand(MI, 2))) {
		int RegWidth = Opcode == AArch64_MOVZXi ? 64 : 32;
		int Shift = MCOperand_getImm(MCInst_getOperand(MI, 2));
		uint64_t Value = (uint64_t)MCOperand_getImm(MCInst_getOperand(MI, 1)) << Shift;

		if (isMOVZMovAlias(Value, Shift,
					Opcode == AArch64_MOVZXi ? 64 : 32)) {
			SStream_concat(O, "mov\t%s, ", getRegisterName(MCOperand_getReg(MCInst_getOperand(MI, 0)), AArch64_NoRegAltName));

			printInt64Bang(O, SignExtend64(Value, RegWidth));

			if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
				uint8_t access;
				access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
				MI->ac_idx++;
#endif
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = MCOperand_getReg(MCInst_getOperand(MI, 0));
				MI->flat_insn->detail->arm64.op_count++;

				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = SignExtend64(Value, RegWidth);
				MI->flat_insn->detail->arm64.op_count++;
			}

			MCInst_setOpcodePub(MI, AArch64_map_insn("mov"));

			return;
		}
	}

	if ((Opcode == AArch64_MOVNXi || Opcode == AArch64_MOVNWi) &&
			MCOperand_isImm(MCInst_getOperand(MI, 1)) && MCOperand_isImm(MCInst_getOperand(MI, 2))) {
		int RegWidth = Opcode == AArch64_MOVNXi ? 64 : 32;
		int Shift = MCOperand_getImm(MCInst_getOperand(MI, 2));
		uint64_t Value = ~((uint64_t)MCOperand_getImm(MCInst_getOperand(MI, 1)) << Shift);

		if (RegWidth == 32)
			Value = Value & 0xffffffff;

		if (AArch64_AM_isMOVNMovAlias(Value, Shift, RegWidth)) {
			SStream_concat(O, "mov\t%s, ", getRegisterName(MCOperand_getReg(MCInst_getOperand(MI, 0)), AArch64_NoRegAltName));

			printInt64Bang(O, SignExtend64(Value, RegWidth));

			if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
				uint8_t access;
				access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
				MI->ac_idx++;
#endif
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = MCOperand_getReg(MCInst_getOperand(MI, 0));
				MI->flat_insn->detail->arm64.op_count++;

				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = SignExtend64(Value, RegWidth);
				MI->flat_insn->detail->arm64.op_count++;
			}

			MCInst_setOpcodePub(MI, AArch64_map_insn("mov"));

			return;
		}
	}

	if ((Opcode == AArch64_ORRXri || Opcode == AArch64_ORRWri) &&
			(MCOperand_getReg(MCInst_getOperand(MI, 1)) == AArch64_XZR ||
			 MCOperand_getReg(MCInst_getOperand(MI, 1)) == AArch64_WZR) &&
			MCOperand_isImm(MCInst_getOperand(MI, 2))) {
		int RegWidth = Opcode == AArch64_ORRXri ? 64 : 32;
		uint64_t Value = AArch64_AM_decodeLogicalImmediate(
				MCOperand_getImm(MCInst_getOperand(MI, 2)), RegWidth);
		SStream_concat(O, "mov\t%s, ", getRegisterName(MCOperand_getReg(MCInst_getOperand(MI, 0)), AArch64_NoRegAltName));

		printInt64Bang(O, SignExtend64(Value, RegWidth));

		if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
			uint8_t access;
			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = MCOperand_getReg(MCInst_getOperand(MI, 0));
			MI->flat_insn->detail->arm64.op_count++;

			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = SignExtend64(Value, RegWidth);
			MI->flat_insn->detail->arm64.op_count++;
		}

		MCInst_setOpcodePub(MI, AArch64_map_insn("mov"));

		return;
	}

	// Instruction TSB is specified as a one operand instruction, but 'csync' is
	// not encoded, so for printing it is treated as a special case here:
	if (Opcode == AArch64_TSB) {
		SStream_concat0(O, "tsb\tcsync");
		MCInst_setOpcodePub(MI, AArch64_map_insn("tsb"));
		return;
	}

	MI->MRI = Info;

	mnem = printAliasInstr(MI, O, (MCRegisterInfo *)Info);
	if (mnem) {
		MCInst_setOpcodePub(MI, AArch64_map_insn(mnem));
		cs_mem_free(mnem);

		switch(MCInst_getOpcode(MI)) {
			default: break;
			case AArch64_LD1i8_POST:
				arm64_op_addImm(MI, 1);
				break;
			case AArch64_LD1i16_POST:
				arm64_op_addImm(MI, 2);
				break;
			case AArch64_LD1i32_POST:
				arm64_op_addImm(MI, 4);
				break;
			case AArch64_LD1Onev1d_POST:
			case AArch64_LD1Onev2s_POST:
			case AArch64_LD1Onev4h_POST:
			case AArch64_LD1Onev8b_POST:
			case AArch64_LD1i64_POST:
				arm64_op_addImm(MI, 8);
				break;
			case AArch64_LD1Onev16b_POST:
			case AArch64_LD1Onev2d_POST:
			case AArch64_LD1Onev4s_POST:
			case AArch64_LD1Onev8h_POST:
			case AArch64_LD1Twov1d_POST:
			case AArch64_LD1Twov2s_POST:
			case AArch64_LD1Twov4h_POST:
			case AArch64_LD1Twov8b_POST:
				arm64_op_addImm(MI, 16);
				break;
			case AArch64_LD1Threev1d_POST:
			case AArch64_LD1Threev2s_POST:
			case AArch64_LD1Threev4h_POST:
			case AArch64_LD1Threev8b_POST:
				arm64_op_addImm(MI, 24);
				break;
			case AArch64_LD1Fourv1d_POST:
			case AArch64_LD1Fourv2s_POST:
			case AArch64_LD1Fourv4h_POST:
			case AArch64_LD1Fourv8b_POST:
			case AArch64_LD1Twov16b_POST:
			case AArch64_LD1Twov2d_POST:
			case AArch64_LD1Twov4s_POST:
			case AArch64_LD1Twov8h_POST:
				arm64_op_addImm(MI, 32);
				break;
			case AArch64_LD1Threev16b_POST:
			case AArch64_LD1Threev2d_POST:
			case AArch64_LD1Threev4s_POST:
			case AArch64_LD1Threev8h_POST:
				 arm64_op_addImm(MI, 48);
				 break;
			case AArch64_LD1Fourv16b_POST:
			case AArch64_LD1Fourv2d_POST:
			case AArch64_LD1Fourv4s_POST:
			case AArch64_LD1Fourv8h_POST:
				arm64_op_addImm(MI, 64);
				break;
			case AArch64_UMOVvi64:
				arm64_op_addVectorArrSpecifier(MI, ARM64_VAS_1D);
				break;
			case AArch64_UMOVvi32:
				arm64_op_addVectorArrSpecifier(MI, ARM64_VAS_1S);
				break;
			case AArch64_INSvi8gpr:
			case AArch64_DUP_ZI_B:
			case AArch64_CPY_ZPmI_B:
			case AArch64_CPY_ZPzI_B:
			case AArch64_CPY_ZPmV_B:
			case AArch64_CPY_ZPmR_B:
			case AArch64_DUP_ZR_B:
				if (MI->csh->detail) {
					MI->flat_insn->detail->arm64.operands[0].vas = ARM64_VAS_1B;
				}
				break;
			case AArch64_INSvi16gpr:
			case AArch64_DUP_ZI_H:
			case AArch64_CPY_ZPmI_H:
			case AArch64_CPY_ZPzI_H:
			case AArch64_CPY_ZPmV_H:
			case AArch64_CPY_ZPmR_H:
			case AArch64_DUP_ZR_H:
			case AArch64_FCPY_ZPmI_H:
			case AArch64_FDUP_ZI_H:
				if (MI->csh->detail) {
					MI->flat_insn->detail->arm64.operands[0].vas = ARM64_VAS_1H;
				}
				break;
			case AArch64_INSvi32gpr:
			case AArch64_DUP_ZI_S:
			case AArch64_CPY_ZPmI_S:
			case AArch64_CPY_ZPzI_S:
			case AArch64_CPY_ZPmV_S:
			case AArch64_CPY_ZPmR_S:
			case AArch64_DUP_ZR_S:
			case AArch64_FCPY_ZPmI_S:
			case AArch64_FDUP_ZI_S:
				if (MI->csh->detail) {
					MI->flat_insn->detail->arm64.operands[0].vas = ARM64_VAS_1S;
				}
				break;
			case AArch64_INSvi64gpr:
			case AArch64_DUP_ZI_D:
			case AArch64_CPY_ZPmI_D:
			case AArch64_CPY_ZPzI_D:
			case AArch64_CPY_ZPmV_D:
			case AArch64_CPY_ZPmR_D:
			case AArch64_DUP_ZR_D:
			case AArch64_FCPY_ZPmI_D:
			case AArch64_FDUP_ZI_D:
				if (MI->csh->detail) {
					MI->flat_insn->detail->arm64.operands[0].vas = ARM64_VAS_1D;
				}
				break;
			case AArch64_INSvi8lane:
			case AArch64_ORR_PPzPP:
			case AArch64_ORRS_PPzPP:
				if (MI->csh->detail) {
					MI->flat_insn->detail->arm64.operands[0].vas = ARM64_VAS_1B;
					MI->flat_insn->detail->arm64.operands[1].vas = ARM64_VAS_1B;
				}
				break;
			case AArch64_INSvi16lane:
				if (MI->csh->detail) {
					MI->flat_insn->detail->arm64.operands[0].vas = ARM64_VAS_1H;
					MI->flat_insn->detail->arm64.operands[1].vas = ARM64_VAS_1H;
				}
				 break;
			case AArch64_INSvi32lane:
				if (MI->csh->detail) {
					MI->flat_insn->detail->arm64.operands[0].vas = ARM64_VAS_1S;
					MI->flat_insn->detail->arm64.operands[1].vas = ARM64_VAS_1S;
				}
				break;
			case AArch64_INSvi64lane:
			case AArch64_ORR_ZZZ:
				if (MI->csh->detail) {
					MI->flat_insn->detail->arm64.operands[0].vas = ARM64_VAS_1D;
					MI->flat_insn->detail->arm64.operands[1].vas = ARM64_VAS_1D;
				}
				break;
			case AArch64_ORRv16i8:
			case AArch64_NOTv16i8:
				if (MI->csh->detail) {
					MI->flat_insn->detail->arm64.operands[0].vas = ARM64_VAS_16B;
					MI->flat_insn->detail->arm64.operands[1].vas = ARM64_VAS_16B;
				}
				break;
			case AArch64_ORRv8i8:
			case AArch64_NOTv8i8:
				if (MI->csh->detail) {
					MI->flat_insn->detail->arm64.operands[0].vas = ARM64_VAS_8B;
					MI->flat_insn->detail->arm64.operands[1].vas = ARM64_VAS_8B;
				}
				break;
			case AArch64_AND_PPzPP:
			case AArch64_ANDS_PPzPP:
			case AArch64_EOR_PPzPP:
			case AArch64_EORS_PPzPP:
			case AArch64_SEL_PPPP:
			case AArch64_SEL_ZPZZ_B:
				if (MI->csh->detail) {
					MI->flat_insn->detail->arm64.operands[0].vas = ARM64_VAS_1B;
					MI->flat_insn->detail->arm64.operands[2].vas = ARM64_VAS_1B;
				}
				break;
			case AArch64_SEL_ZPZZ_D:
				if (MI->csh->detail) {
					MI->flat_insn->detail->arm64.operands[0].vas = ARM64_VAS_1D;
					MI->flat_insn->detail->arm64.operands[2].vas = ARM64_VAS_1D;
				}
				break;
			case AArch64_SEL_ZPZZ_H:
				if (MI->csh->detail) {
					MI->flat_insn->detail->arm64.operands[0].vas = ARM64_VAS_1H;
					MI->flat_insn->detail->arm64.operands[2].vas = ARM64_VAS_1H;
				}
				break;
			case AArch64_SEL_ZPZZ_S:
				if (MI->csh->detail) {
					MI->flat_insn->detail->arm64.operands[0].vas = ARM64_VAS_1S;
					MI->flat_insn->detail->arm64.operands[2].vas = ARM64_VAS_1S;
				}
				break;
			case AArch64_DUP_ZZI_B:
				if (MI->csh->detail) {
					MI->flat_insn->detail->arm64.operands[0].vas = ARM64_VAS_1B;
					if (MI->flat_insn->detail->arm64.op_count == 1) {
						arm64_op_addReg(MI, ARM64_REG_B0 + MCOperand_getReg(MCInst_getOperand(MI, 1)) - ARM64_REG_Z0);
					} else {
						MI->flat_insn->detail->arm64.operands[1].vas = ARM64_VAS_1B;
					}
				}
				break;
			case AArch64_DUP_ZZI_D:
				if (MI->csh->detail) {
					MI->flat_insn->detail->arm64.operands[0].vas = ARM64_VAS_1D;
					if (MI->flat_insn->detail->arm64.op_count == 1) {
						arm64_op_addReg(MI, ARM64_REG_D0 + MCOperand_getReg(MCInst_getOperand(MI, 1)) - ARM64_REG_Z0);
					} else {
						MI->flat_insn->detail->arm64.operands[1].vas = ARM64_VAS_1D;
					}
				}
				break;
			case AArch64_DUP_ZZI_H:
				if (MI->csh->detail) {
					MI->flat_insn->detail->arm64.operands[0].vas = ARM64_VAS_1H;
					if (MI->flat_insn->detail->arm64.op_count == 1) {
						arm64_op_addReg(MI, ARM64_REG_H0 + MCOperand_getReg(MCInst_getOperand(MI, 1)) - ARM64_REG_Z0);
					} else {
						MI->flat_insn->detail->arm64.operands[1].vas = ARM64_VAS_1H;
					}
				}
				break;
			case AArch64_DUP_ZZI_Q:
				if (MI->csh->detail) {
					MI->flat_insn->detail->arm64.operands[0].vas = ARM64_VAS_1Q;
					if (MI->flat_insn->detail->arm64.op_count == 1) {
						arm64_op_addReg(MI, ARM64_REG_Q0 + MCOperand_getReg(MCInst_getOperand(MI, 1)) - ARM64_REG_Z0);
					} else {
						MI->flat_insn->detail->arm64.operands[1].vas = ARM64_VAS_1Q;
					}
				 }
				 break;
			case AArch64_DUP_ZZI_S:
				if (MI->csh->detail) {
					MI->flat_insn->detail->arm64.operands[0].vas = ARM64_VAS_1S;
					if (MI->flat_insn->detail->arm64.op_count == 1) {
				 		arm64_op_addReg(MI, ARM64_REG_S0 + MCOperand_getReg(MCInst_getOperand(MI, 1)) - ARM64_REG_Z0);
					} else {
						 MI->flat_insn->detail->arm64.operands[1].vas = ARM64_VAS_1S;
					}
				}
				break;
			// Hacky detail filling of SMSTART and SMSTOP alias'
			case AArch64_MSRpstatesvcrImm1:{
				if(MI->csh->detail){
					MI->flat_insn->detail->arm64.op_count = 2;
#ifndef CAPSTONE_DIET
					MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
					MI->ac_idx++;
					MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
					MI->ac_idx++;
#endif
					MI->flat_insn->detail->arm64.operands[0].type = ARM64_OP_SVCR;
					MI->flat_insn->detail->arm64.operands[0].sys = (unsigned)ARM64_SYSREG_SVCR;
					MI->flat_insn->detail->arm64.operands[0].svcr = lookupSVCRByEncoding(MCOperand_getImm(MCInst_getOperand(MI, 0)))->Encoding;
					MI->flat_insn->detail->arm64.operands[1].type = ARM64_OP_IMM;
					MI->flat_insn->detail->arm64.operands[1].imm = MCOperand_getImm(MCInst_getOperand(MI, 1));
				}
				break;
			}
		}
	} else {
		printInstruction(MI, O);
	}
}

static bool printSysAlias(MCInst *MI, SStream *O)
{
	// unsigned Opcode = MCInst_getOpcode(MI);
	//assert(Opcode == AArch64_SYSxt && "Invalid opcode for SYS alias!");

	const char *Ins;
	uint16_t Encoding;
	bool NeedsReg;
	char Name[64];
	MCOperand *Op1 = MCInst_getOperand(MI, 0);
	MCOperand *Cn = MCInst_getOperand(MI, 1);
	MCOperand *Cm = MCInst_getOperand(MI, 2);
	MCOperand *Op2 = MCInst_getOperand(MI, 3);

	unsigned Op1Val = (unsigned)MCOperand_getImm(Op1);
	unsigned CnVal = (unsigned)MCOperand_getImm(Cn);
	unsigned CmVal = (unsigned)MCOperand_getImm(Cm);
	unsigned Op2Val = (unsigned)MCOperand_getImm(Op2);

	Encoding = Op2Val;
	Encoding |= CmVal << 3;
	Encoding |= CnVal << 7;
	Encoding |= Op1Val << 11;

	if (CnVal == 7) {
		switch (CmVal) {
			default:
				return false;

			// IC aliases
			case 1: case 5: {
				const IC *IC = lookupICByEncoding(Encoding);
				// if (!IC || !IC->haveFeatures(STI.getFeatureBits()))
				if (!IC)
					return false;

				NeedsReg = IC->NeedsReg;
				Ins = "ic";
				strncpy(Name, IC->Name, sizeof(Name) - 1);
			}
			break;

			// DC aliases
			case 4: case 6: case 10: case 11: case 12: case 14: {
				const DC *DC = lookupDCByEncoding(Encoding);
				// if (!DC || !DC->haveFeatures(STI.getFeatureBits()))
				if (!DC)
					return false;

				NeedsReg = true;
				Ins = "dc";
				strncpy(Name, DC->Name, sizeof(Name) - 1);
			}
			break;

			// AT aliases
			case 8: case 9: {
				const AT *AT = lookupATByEncoding(Encoding);
				// if (!AT || !AT->haveFeatures(STI.getFeatureBits()))
				if (!AT)
					return false;

				NeedsReg = true;
				Ins = "at";
				strncpy(Name, AT->Name, sizeof(Name) - 1);
			}
			break;
		}
	} else if (CnVal == 8) {
		// TLBI aliases
		const TLBI *TLBI = lookupTLBIByEncoding(Encoding);
		// if (!TLBI || !TLBI->haveFeatures(STI.getFeatureBits()))
		if (!TLBI)
			return false;

		NeedsReg = TLBI->NeedsReg;
		Ins = "tlbi";
		strncpy(Name, TLBI->Name, sizeof(Name) - 1);
	} else
		return false;

	SStream_concat(O, "%s\t%s", Ins, Name);

	if (NeedsReg) {
		SStream_concat(O, ", %s", getRegisterName(MCOperand_getReg(MCInst_getOperand(MI, 4)), AArch64_NoRegAltName));
	}

	MCInst_setOpcodePub(MI, AArch64_map_insn(Ins));

	if (MI->csh->detail) {
#if 0
#ifndef CAPSTONE_DIET
		uint8_t access;
		access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
		MI->ac_idx++;
#endif
#endif
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_SYS;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].sys = AArch64_map_sys_op(Name);
		MI->flat_insn->detail->arm64.op_count++;

		if (NeedsReg) {
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = MCOperand_getReg(MCInst_getOperand(MI, 4));
			MI->flat_insn->detail->arm64.op_count++;
		}
	}

	return true;
}

static void printOperand(MCInst *MI, unsigned OpNum, SStream *O)
{
	MCOperand *Op = MCInst_getOperand(MI, OpNum);

	if (MCOperand_isReg(Op)) {
		unsigned Reg = MCOperand_getReg(Op);

		SStream_concat0(O, getRegisterName(Reg, AArch64_NoRegAltName));

		if (MI->csh->detail) {
			if (MI->csh->doing_mem) {
				if (MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].mem.base == ARM64_REG_INVALID) {
					MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].mem.base = Reg;
				}
				else if (MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].mem.index == ARM64_REG_INVALID) {
					MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].mem.index = Reg;
				}
			} else if (MI->csh->doing_SME_Index) {
				// Access op_count-1 as We want to add info to previous operand, not create a new one
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count-1].sme_index.base = Reg;
			} else {
#ifndef CAPSTONE_DIET
				uint8_t access;

				access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
				MI->ac_idx++;
#endif
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = Reg;
				MI->flat_insn->detail->arm64.op_count++;
			}
		}
	} else if (MCOperand_isImm(Op)) {
		int64_t imm = MCOperand_getImm(Op);

		if (MI->Opcode == AArch64_ADR) {
			imm += MI->address;
			printUInt64Bang(O, imm);
		} else {
			if (MI->csh->doing_mem) {
				if (MI->csh->imm_unsigned) {
					printUInt64Bang(O, imm);
				} else {
					printInt64Bang(O, imm);
				}
			} else
				printUInt64Bang(O, imm);
		}

		if (MI->csh->detail) {
			if (MI->csh->doing_mem) {
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].mem.disp = (int32_t)imm;
			} else if (MI->csh->doing_SME_Index) {
				// Access op_count-1 as We want to add info to previous operand, not create a new one
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count-1].sme_index.disp = (int32_t)imm; 
			} else {
#ifndef CAPSTONE_DIET
				uint8_t access;

				access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
#endif
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = imm;
				MI->flat_insn->detail->arm64.op_count++;
			}
		}
	}
}

static void printImm(MCInst *MI, unsigned OpNum, SStream *O)
{
	MCOperand *Op = MCInst_getOperand(MI, OpNum);
	printUInt64Bang(O, MCOperand_getImm(Op));

	if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
		uint8_t access;
		access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
		MI->ac_idx++;
#endif
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = MCOperand_getImm(Op);
		MI->flat_insn->detail->arm64.op_count++;
	}
}

static void printImmHex(MCInst *MI, unsigned OpNum, SStream *O)
{
	MCOperand *Op = MCInst_getOperand(MI, OpNum);
	printUInt64Bang(O, MCOperand_getImm(Op));

	if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
		uint8_t access;
		access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
		MI->ac_idx++;
#endif
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = MCOperand_getImm(Op);
		MI->flat_insn->detail->arm64.op_count++;
	}
}

static void printSImm(MCInst *MI, unsigned OpNo, SStream *O, int Size) {
  MCOperand *Op = MCInst_getOperand(MI, OpNo);
  if (Size == 8)
	printInt64Bang(O, (signed char) MCOperand_getImm(Op));
  else if (Size == 16)
	printInt64Bang(O, (signed short) MCOperand_getImm(Op));
  else
    printInt64Bang(O, MCOperand_getImm(Op));

	if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
		uint8_t access;
		access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
		MI->ac_idx++;
#endif
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = MCOperand_getImm(Op);
		MI->flat_insn->detail->arm64.op_count++;
	}
}

static void printPostIncOperand(MCInst *MI, unsigned OpNum, SStream *O,
		unsigned Imm)
{
	MCOperand *Op = MCInst_getOperand(MI, OpNum);

	if (MCOperand_isReg(Op)) {
		unsigned Reg = MCOperand_getReg(Op);
		if (Reg == AArch64_XZR) {
			printInt32Bang(O, Imm);

			if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
				uint8_t access;

				access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
				MI->ac_idx++;
#endif
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = Imm;
				MI->flat_insn->detail->arm64.op_count++;
			}
		} else {
			SStream_concat0(O, getRegisterName(Reg, AArch64_NoRegAltName));

			if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
				uint8_t access;

				access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
				MI->ac_idx++;
#endif
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = Reg;
				MI->flat_insn->detail->arm64.op_count++;
			}
		}
	}
	//llvm_unreachable("unknown operand kind in printPostIncOperand64");
}

static void printVRegOperand(MCInst *MI, unsigned OpNum, SStream *O)
{
	MCOperand *Op = MCInst_getOperand(MI, OpNum);
	//assert(Op.isReg() && "Non-register vreg operand!");
	unsigned Reg = MCOperand_getReg(Op);

	SStream_concat0(O, getRegisterName(Reg, AArch64_vreg));

	if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
		uint8_t access;
		access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
		MI->ac_idx++;
#endif
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = AArch64_map_vregister(Reg);
		MI->flat_insn->detail->arm64.op_count++;
	}
}

static void printSysCROperand(MCInst *MI, unsigned OpNum, SStream *O)
{
	MCOperand *Op = MCInst_getOperand(MI, OpNum);
	//assert(Op.isImm() && "System instruction C[nm] operands must be immediates!");
	SStream_concat(O, "c%u", MCOperand_getImm(Op));

	if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
		uint8_t access;

		access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
		MI->ac_idx++;
#endif
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_CIMM;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = MCOperand_getImm(Op);
		MI->flat_insn->detail->arm64.op_count++;
	}
}

static void printAddSubImm(MCInst *MI, unsigned OpNum, SStream *O)
{
	MCOperand *MO = MCInst_getOperand(MI, OpNum);
	if (MCOperand_isImm(MO)) {
		unsigned Val = (MCOperand_getImm(MO) & 0xfff);
		//assert(Val == MO.getImm() && "Add/sub immediate out of range!");
		unsigned Shift = AArch64_AM_getShiftValue((int)MCOperand_getImm(MCInst_getOperand(MI, OpNum + 1)));

		printInt32Bang(O, Val);

		if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
			uint8_t access;

			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = Val;
			MI->flat_insn->detail->arm64.op_count++;
		}

		if (Shift != 0)
			printShifter(MI, OpNum + 1, O);
	}
}

static void printLogicalImm32(MCInst *MI, unsigned OpNum, SStream *O)
{
	int64_t Val = MCOperand_getImm(MCInst_getOperand(MI, OpNum));

	Val = AArch64_AM_decodeLogicalImmediate(Val, 32);
	printUInt32Bang(O, (int)Val);

	if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
		uint8_t access;

		access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
		MI->ac_idx++;
#endif
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = Val;
		MI->flat_insn->detail->arm64.op_count++;
	}
}

static void printLogicalImm64(MCInst *MI, unsigned OpNum, SStream *O)
{
	int64_t Val = MCOperand_getImm(MCInst_getOperand(MI, OpNum));
	Val = AArch64_AM_decodeLogicalImmediate(Val, 64);

	switch(MI->flat_insn->id) {
		default:
			printInt64Bang(O, Val);
			break;

		case ARM64_INS_ORR:
		case ARM64_INS_AND:
		case ARM64_INS_EOR:
		case ARM64_INS_TST:
			// do not print number in negative form
			if (Val >= 0 && Val <= HEX_THRESHOLD)
				SStream_concat(O, "#%u", (int)Val);
			else
				SStream_concat(O, "#0x%"PRIx64, Val);
			break;
	}

	if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
		uint8_t access;

		access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
		MI->ac_idx++;
#endif
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = (int64_t)Val;
		MI->flat_insn->detail->arm64.op_count++;
	}
}

static void printShifter(MCInst *MI, unsigned OpNum, SStream *O)
{
	unsigned Val = (unsigned)MCOperand_getImm(MCInst_getOperand(MI, OpNum));

	// LSL #0 should not be printed.
	if (AArch64_AM_getShiftType(Val) == AArch64_AM_LSL &&
			AArch64_AM_getShiftValue(Val) == 0)
		return;

	SStream_concat(O, ", %s ", AArch64_AM_getShiftExtendName(AArch64_AM_getShiftType(Val)));
	printInt32BangDec(O, AArch64_AM_getShiftValue(Val));

	if (MI->csh->detail) {
		arm64_shifter shifter = ARM64_SFT_INVALID;

		switch(AArch64_AM_getShiftType(Val)) {
			default:	// never reach
			case AArch64_AM_LSL:
				shifter = ARM64_SFT_LSL;
				break;

			case AArch64_AM_LSR:
				shifter = ARM64_SFT_LSR;
				break;

			case AArch64_AM_ASR:
				shifter = ARM64_SFT_ASR;
				break;

			case AArch64_AM_ROR:
				shifter = ARM64_SFT_ROR;
				break;

			case AArch64_AM_MSL:
				shifter = ARM64_SFT_MSL;
				break;
		}

		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count - 1].shift.type = shifter;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count - 1].shift.value = AArch64_AM_getShiftValue(Val);
	}
}

static void printShiftedRegister(MCInst *MI, unsigned OpNum, SStream *O)
{
	SStream_concat0(O, getRegisterName(MCOperand_getReg(MCInst_getOperand(MI, OpNum)), AArch64_NoRegAltName));

	if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
		uint8_t access;
		access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
		MI->ac_idx++;
#endif
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = MCOperand_getReg(MCInst_getOperand(MI, OpNum));
		MI->flat_insn->detail->arm64.op_count++;
	}

	printShifter(MI, OpNum + 1, O);
}

static void printArithExtend(MCInst *MI, unsigned OpNum, SStream *O)
{
	unsigned Val = (unsigned)MCOperand_getImm(MCInst_getOperand(MI, OpNum));
	AArch64_AM_ShiftExtendType ExtType = AArch64_AM_getArithExtendType(Val);
	unsigned ShiftVal = AArch64_AM_getArithShiftValue(Val);

	// If the destination or first source register operand is [W]SP, print
	// UXTW/UXTX as LSL, and if the shift amount is also zero, print nothing at
	// all.
	if (ExtType == AArch64_AM_UXTW || ExtType == AArch64_AM_UXTX) {
		unsigned Dest = MCOperand_getReg(MCInst_getOperand(MI, 0));
		unsigned Src1 = MCOperand_getReg(MCInst_getOperand(MI, 1));

		if (((Dest == AArch64_SP || Src1 == AArch64_SP) &&
					ExtType == AArch64_AM_UXTX) ||
				((Dest == AArch64_WSP || Src1 == AArch64_WSP) &&
				 ExtType == AArch64_AM_UXTW)) {
			if (ShiftVal != 0) {
				SStream_concat0(O, ", lsl ");
				printInt32Bang(O, ShiftVal);

				if (MI->csh->detail) {
					MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count - 1].shift.type = ARM64_SFT_LSL;
					MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count - 1].shift.value = ShiftVal;
				}
			}

			return;
		}
	}

	SStream_concat(O, ", %s", AArch64_AM_getShiftExtendName(ExtType));

	if (MI->csh->detail) {
		arm64_extender ext = ARM64_EXT_INVALID;
		switch(ExtType) {
			default:	// never reach

			case AArch64_AM_UXTB:
				ext = ARM64_EXT_UXTB;
				break;

			case AArch64_AM_UXTH:
				ext = ARM64_EXT_UXTH;
				break;

			case AArch64_AM_UXTW:
				ext = ARM64_EXT_UXTW;
				break;

			case AArch64_AM_UXTX:
				ext = ARM64_EXT_UXTX;
				break;

			case AArch64_AM_SXTB:
				ext = ARM64_EXT_SXTB;
				break;

			case AArch64_AM_SXTH:
				ext = ARM64_EXT_SXTH;
				break;

			case AArch64_AM_SXTW:
				ext = ARM64_EXT_SXTW;
				break;

			case AArch64_AM_SXTX:
				ext = ARM64_EXT_SXTX;
				break;
		}

		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count - 1].ext = ext;
	}

	if (ShiftVal != 0) {
		SStream_concat0(O, " ");
		printInt32Bang(O, ShiftVal);

		if (MI->csh->detail) {
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count - 1].shift.type = ARM64_SFT_LSL;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count - 1].shift.value = ShiftVal;
		}
	}
}

static void printExtendedRegister(MCInst *MI, unsigned OpNum, SStream *O)
{
	unsigned Reg = MCOperand_getReg(MCInst_getOperand(MI, OpNum));

	SStream_concat0(O, getRegisterName(Reg, AArch64_NoRegAltName));

	if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
		uint8_t access;
		access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
		MI->ac_idx++;
#endif
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = Reg;
		MI->flat_insn->detail->arm64.op_count++;
	}

	printArithExtend(MI, OpNum + 1, O);
}

static void printMemExtendImpl(MCInst *MI, bool SignExtend, bool DoShift, unsigned Width,
			       char SrcRegKind, SStream *O)
{
	// sxtw, sxtx, uxtw or lsl (== uxtx)
	bool IsLSL = !SignExtend && SrcRegKind == 'x';
	if (IsLSL) {
		SStream_concat0(O, "lsl");

		if (MI->csh->detail) {
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].shift.type = ARM64_SFT_LSL;
		}
	} else {
		SStream_concat(O, "%cxt%c", (SignExtend ? 's' : 'u'), SrcRegKind);

		if (MI->csh->detail) {
			if (!SignExtend) {
				switch(SrcRegKind) {
					default: break;
					case 'b':
							 MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].ext = ARM64_EXT_UXTB;
							 break;
					case 'h':
							 MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].ext = ARM64_EXT_UXTH;
							 break;
					case 'w':
							 MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].ext = ARM64_EXT_UXTW;
							 break;
				}
			} else {
					switch(SrcRegKind) {
						default: break;
						case 'b':
							MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].ext = ARM64_EXT_SXTB;
							break;
						case 'h':
							MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].ext = ARM64_EXT_SXTH;
							break;
						case 'w':
							MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].ext = ARM64_EXT_SXTW;
							break;
						case 'x':
							MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].ext = ARM64_EXT_SXTX;
							break;
					}
			}
		}
	}

	if (DoShift || IsLSL) {
		SStream_concat(O, " #%u", Log2_32(Width / 8));

		if (MI->csh->detail) {
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].shift.type = ARM64_SFT_LSL;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].shift.value = Log2_32(Width / 8);
		}
	}
}

static void printMemExtend(MCInst *MI, unsigned OpNum, SStream *O, char SrcRegKind, unsigned Width)
{
	unsigned SignExtend = (unsigned)MCOperand_getImm(MCInst_getOperand(MI, OpNum));
	unsigned DoShift = (unsigned)MCOperand_getImm(MCInst_getOperand(MI, OpNum + 1));

	printMemExtendImpl(MI, SignExtend, DoShift, Width, SrcRegKind, O);
}

static void printRegWithShiftExtend(MCInst *MI, unsigned OpNum, SStream *O,
				    bool SignExtend, int ExtWidth,
				    char SrcRegKind, char Suffix)
{
	bool DoShift;

	printOperand(MI, OpNum, O);

	if (Suffix == 's' || Suffix == 'd')
		SStream_concat(O, ".%c", Suffix);

	DoShift = ExtWidth != 8;
	if (SignExtend || DoShift || SrcRegKind == 'w') {
		SStream_concat0(O, ", ");
		printMemExtendImpl(MI, SignExtend, DoShift, ExtWidth, SrcRegKind, O);
	}
}

static void printCondCode(MCInst *MI, unsigned OpNum, SStream *O)
{
	AArch64CC_CondCode CC = (AArch64CC_CondCode)MCOperand_getImm(MCInst_getOperand(MI, OpNum));
	SStream_concat0(O, getCondCodeName(CC));

	if (MI->csh->detail)
		MI->flat_insn->detail->arm64.cc = (arm64_cc)(CC + 1);
}

static void printInverseCondCode(MCInst *MI, unsigned OpNum, SStream *O)
{
	AArch64CC_CondCode CC = (AArch64CC_CondCode)MCOperand_getImm(MCInst_getOperand(MI, OpNum));
	SStream_concat0(O, getCondCodeName(getInvertedCondCode(CC)));

	if (MI->csh->detail) {
		MI->flat_insn->detail->arm64.cc = (arm64_cc)(getInvertedCondCode(CC) + 1);
	}
}

static void printImmScale(MCInst *MI, unsigned OpNum, SStream *O, int Scale)
{
	int64_t val = Scale * MCOperand_getImm(MCInst_getOperand(MI, OpNum));

	printInt64Bang(O, val);

	if (MI->csh->detail) {
		if (MI->csh->doing_mem) {
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].mem.disp = (int32_t)val;
		} else {
#ifndef CAPSTONE_DIET
			uint8_t access;

			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = val;
			MI->flat_insn->detail->arm64.op_count++;
		}
	}
}

static void printUImm12Offset(MCInst *MI, unsigned OpNum, SStream *O, unsigned Scale)
{
	MCOperand *MO = MCInst_getOperand(MI, OpNum);

	if (MCOperand_isImm(MO)) {
		int64_t val = Scale * MCOperand_getImm(MO);
		printInt64Bang(O, val);

		if (MI->csh->detail) {
			if (MI->csh->doing_mem) {
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].mem.disp = (int32_t)val;
			} else {
#ifndef CAPSTONE_DIET
				uint8_t access;

				access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
				MI->ac_idx++;
#endif
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
				MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = (int)val;
				MI->flat_insn->detail->arm64.op_count++;
			}
		}
	}
}

#if 0
static void printAMIndexedWB(MCInst *MI, unsigned OpNum, SStream *O, unsigned int Scale)
{
	MCOperand *MO = MCInst_getOperand(MI, OpNum + 1);

	SStream_concat(O, "[%s", getRegisterName(MCOperand_getReg(MCInst_getOperand(MI, OpNum)), AArch64_NoRegAltName));

	if (MCOperand_isImm(MO)) {
		int64_t val = Scale * MCOperand_getImm(MCInst_getOperand(MI, OpNum));
		printInt64Bang(O, val);
	// } else {
	//   // assert(MO1.isExpr() && "Unexpected operand type!");
	//   SStream_concat0(O, ", ");
	//   MO1.getExpr()->print(O, &MAI);
	}

	SStream_concat0(O, "]");
}
#endif

// IsSVEPrefetch = false
static void printPrefetchOp(MCInst *MI, unsigned OpNum, SStream *O, bool IsSVEPrefetch)
{
	unsigned prfop = (unsigned)MCOperand_getImm(MCInst_getOperand(MI, OpNum));

	if (IsSVEPrefetch) {
		const SVEPRFM *PRFM = lookupSVEPRFMByEncoding(prfop);
		if (PRFM)
			SStream_concat0(O, PRFM->Name);

		return;
	} else {
		const PRFM *PRFM = lookupPRFMByEncoding(prfop);
		if (PRFM)
			SStream_concat0(O, PRFM->Name);

		return;
	}

	// FIXME: set OpcodePub?

	printInt32Bang(O, prfop);

	if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
		uint8_t access;
		access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
		MI->ac_idx++;
#endif
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = prfop;
		MI->flat_insn->detail->arm64.op_count++;
	}
}

static void printPSBHintOp(MCInst *MI, unsigned OpNum, SStream *O)
{
	MCOperand *Op = MCInst_getOperand(MI, OpNum);
	unsigned int psbhintop = MCOperand_getImm(Op);

	const PSB *PSB = lookupPSBByEncoding(psbhintop);
	if (PSB)
		SStream_concat0(O, PSB->Name);
	else
		printUInt32Bang(O, psbhintop);
}

static void printBTIHintOp(MCInst *MI, unsigned OpNum, SStream *O) {
  unsigned btihintop = MCOperand_getImm(MCInst_getOperand(MI, OpNum)) ^ 32;

  const BTI *BTI = lookupBTIByEncoding(btihintop);
  if (BTI)
	SStream_concat0(O, BTI->Name);
  else
	printUInt32Bang(O, btihintop);
}

static void printFPImmOperand(MCInst *MI, unsigned OpNum, SStream *O)
{
	MCOperand *MO = MCInst_getOperand(MI, OpNum);
	float FPImm = MCOperand_isFPImm(MO) ? MCOperand_getFPImm(MO) : AArch64_AM_getFPImmFloat((int)MCOperand_getImm(MO));

	// 8 decimal places are enough to perfectly represent permitted floats.
#if defined(_KERNEL_MODE)
	// Issue #681: Windows kernel does not support formatting float point
	SStream_concat0(O, "#<float_point_unsupported>");
#else
	SStream_concat(O, "#%.8f", FPImm);
#endif

	if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
		uint8_t access;

		access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
		MI->ac_idx++;
#endif
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_FP;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].fp = FPImm;
		MI->flat_insn->detail->arm64.op_count++;
	}
}

//static unsigned getNextVectorRegister(unsigned Reg, unsigned Stride = 1)
static unsigned getNextVectorRegister(unsigned Reg, unsigned Stride)
{
	while (Stride--) {
		if (Reg >= AArch64_Q0 && Reg <= AArch64_Q30) // AArch64_Q0 .. AArch64_Q30
			Reg += 1;
		else if (Reg == AArch64_Q31) // Vector lists can wrap around.
			Reg = AArch64_Q0;
		else if (Reg >= AArch64_Z0 && Reg <= AArch64_Z30) // AArch64_Z0 .. AArch64_Z30
			Reg += 1;
		else if (Reg == AArch64_Z31) // Vector lists can wrap around.
			Reg = AArch64_Z0;
	}

	return Reg;
}

static void printGPRSeqPairsClassOperand(MCInst *MI, unsigned OpNum, SStream *O, unsigned int size)
{
	// static_assert(size == 64 || size == 32,
	//		"Template parameter must be either 32 or 64");
	unsigned Reg = MCOperand_getReg(MCInst_getOperand(MI, OpNum));
	unsigned Sube = (size == 32) ? AArch64_sube32 : AArch64_sube64;
	unsigned Subo = (size == 32) ? AArch64_subo32 : AArch64_subo64;
	unsigned Even = MCRegisterInfo_getSubReg(MI->MRI, Reg, Sube);
	unsigned Odd = MCRegisterInfo_getSubReg(MI->MRI, Reg, Subo);

	SStream_concat(O, "%s, %s", getRegisterName(Even, AArch64_NoRegAltName),
			getRegisterName(Odd, AArch64_NoRegAltName));

	if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
		uint8_t access;

		access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
		MI->ac_idx++;
#endif

		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = Even;
		MI->flat_insn->detail->arm64.op_count++;

		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = Odd;
		MI->flat_insn->detail->arm64.op_count++;
	}
}

static void printVectorList(MCInst *MI, unsigned OpNum, SStream *O,
		char *LayoutSuffix, MCRegisterInfo *MRI, arm64_vas vas)
{
#define GETREGCLASS_CONTAIN0(_class, _reg) MCRegisterClass_contains(MCRegisterInfo_getRegClass(MRI, _class), _reg)
	unsigned Reg = MCOperand_getReg(MCInst_getOperand(MI, OpNum));
	unsigned NumRegs = 1, FirstReg, i;

	SStream_concat0(O, "{");

	// Work out how many registers there are in the list (if there is an actual
	// list).
	if (GETREGCLASS_CONTAIN0(AArch64_DDRegClassID , Reg) ||
			GETREGCLASS_CONTAIN0(AArch64_ZPR2RegClassID, Reg) ||
			GETREGCLASS_CONTAIN0(AArch64_QQRegClassID, Reg))
		NumRegs = 2;
	else if (GETREGCLASS_CONTAIN0(AArch64_DDDRegClassID, Reg) ||
			GETREGCLASS_CONTAIN0(AArch64_ZPR3RegClassID, Reg) ||
			GETREGCLASS_CONTAIN0(AArch64_QQQRegClassID, Reg))
		NumRegs = 3;
	else if (GETREGCLASS_CONTAIN0(AArch64_DDDDRegClassID, Reg) ||
			GETREGCLASS_CONTAIN0(AArch64_ZPR4RegClassID, Reg) ||
			GETREGCLASS_CONTAIN0(AArch64_QQQQRegClassID, Reg))
		NumRegs = 4;

	// Now forget about the list and find out what the first register is.
	if ((FirstReg = MCRegisterInfo_getSubReg(MRI, Reg, AArch64_dsub0)))
		Reg = FirstReg;
	else if ((FirstReg = MCRegisterInfo_getSubReg(MRI, Reg, AArch64_qsub0)))
		Reg = FirstReg;
	else if ((FirstReg = MCRegisterInfo_getSubReg(MRI, Reg, AArch64_zsub0)))
		Reg = FirstReg;

	// If it's a D-reg, we need to promote it to the equivalent Q-reg before
	// printing (otherwise getRegisterName fails).
	if (GETREGCLASS_CONTAIN0(AArch64_FPR64RegClassID, Reg)) {
		const MCRegisterClass *FPR128RC = MCRegisterInfo_getRegClass(MRI, AArch64_FPR128RegClassID);
		Reg = MCRegisterInfo_getMatchingSuperReg(MRI, Reg, AArch64_dsub, FPR128RC);
	}

	for (i = 0; i < NumRegs; ++i, Reg = getNextVectorRegister(Reg, 1)) {
		bool isZReg = GETREGCLASS_CONTAIN0(AArch64_ZPRRegClassID, Reg);
		if (isZReg)
			SStream_concat(O, "%s%s", getRegisterName(Reg, AArch64_NoRegAltName), LayoutSuffix);
		else
			SStream_concat(O, "%s%s", getRegisterName(Reg, AArch64_vreg), LayoutSuffix);

		if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
			uint8_t access;

			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif
			unsigned regForDetail = isZReg ? Reg : AArch64_map_vregister(Reg);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = regForDetail;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].vas = vas;
			MI->flat_insn->detail->arm64.op_count++;
		}

		if (i + 1 != NumRegs)
			SStream_concat0(O, ", ");
	}

	SStream_concat0(O, "}");
}

static void printTypedVectorList(MCInst *MI, unsigned OpNum, SStream *O, unsigned NumLanes, char LaneKind)
{
	char Suffix[32];
	arm64_vas vas = 0;

	if (NumLanes) {
		cs_snprintf(Suffix, sizeof(Suffix), ".%u%c", NumLanes, LaneKind);

		switch(LaneKind) {
			default: break;
			case 'b':
				switch(NumLanes) {
					default: break;
					case 1:
							 vas = ARM64_VAS_1B;
							 break;
					case 4:
							 vas = ARM64_VAS_4B;
							 break;
					case 8:
							 vas = ARM64_VAS_8B;
							 break;
					case 16:
							 vas = ARM64_VAS_16B;
							 break;
				}
				break;
			case 'h':
				switch(NumLanes) {
					default: break;
					case 1:
							 vas = ARM64_VAS_1H;
							 break;
					case 2:
							 vas = ARM64_VAS_2H;
							 break;
					case 4:
							 vas = ARM64_VAS_4H;
							 break;
					case 8:
							 vas = ARM64_VAS_8H;
							 break;
				}
				break;
			case 's':
				switch(NumLanes) {
					default: break;
					case 1:
							 vas = ARM64_VAS_1S;
							 break;
					case 2:
							 vas = ARM64_VAS_2S;
							 break;
					case 4:
							 vas = ARM64_VAS_4S;
							 break;
				}
				break;
			case 'd':
				switch(NumLanes) {
					default: break;
					case 1:
							 vas = ARM64_VAS_1D;
							 break;
					case 2:
							 vas = ARM64_VAS_2D;
							 break;
				}
				break;
			case 'q':
				switch(NumLanes) {
					default: break;
					case 1:
							 vas = ARM64_VAS_1Q;
							 break;
				}
				break;
		}
	} else {
		cs_snprintf(Suffix, sizeof(Suffix), ".%c", LaneKind);

		switch(LaneKind) {
			default: break;
			case 'b':
					 vas = ARM64_VAS_1B;
					 break;
			case 'h':
					 vas = ARM64_VAS_1H;
					 break;
			case 's':
					 vas = ARM64_VAS_1S;
					 break;
			case 'd':
					 vas = ARM64_VAS_1D;
					 break;
			case 'q':
					 vas = ARM64_VAS_1Q;
					 break;
		}
	}

	printVectorList(MI, OpNum, O, Suffix, MI->MRI, vas);
}

static void printVectorIndex(MCInst *MI, unsigned OpNum, SStream *O)
{
	SStream_concat0(O, "[");
	printInt32(O, (int)MCOperand_getImm(MCInst_getOperand(MI, OpNum)));
	SStream_concat0(O, "]");

	if (MI->csh->detail) {
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count - 1].vector_index = (int)MCOperand_getImm(MCInst_getOperand(MI, OpNum));
	}
}

static void printAlignedLabel(MCInst *MI, unsigned OpNum, SStream *O)
{
	MCOperand *Op = MCInst_getOperand(MI, OpNum);

	// If the label has already been resolved to an immediate offset (say, when
	// we're running the disassembler), just print the immediate.
	if (MCOperand_isImm(Op)) {
		uint64_t imm = (MCOperand_getImm(Op) * 4) + MI->address;
		printUInt64Bang(O, imm);

		if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
			uint8_t access;

			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = imm;
			MI->flat_insn->detail->arm64.op_count++;
		}
	}
}

static void printAdrpLabel(MCInst *MI, unsigned OpNum, SStream *O)
{
	MCOperand *Op = MCInst_getOperand(MI, OpNum);

	if (MCOperand_isImm(Op)) {
		// ADRP sign extends a 21-bit offset, shifts it left by 12
		// and adds it to the value of the PC with its bottom 12 bits cleared
		uint64_t imm = (MCOperand_getImm(Op) * 0x1000) + (MI->address & ~0xfff);
		printUInt64Bang(O, imm);

		if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
			uint8_t access;

			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = imm;
			MI->flat_insn->detail->arm64.op_count++;
		}
	}
}

static void printBarrierOption(MCInst *MI, unsigned OpNum, SStream *O)
{
	unsigned Val = (unsigned)MCOperand_getImm(MCInst_getOperand(MI, OpNum));
	unsigned Opcode = MCInst_getOpcode(MI);
	const char *Name = NULL;

	if (Opcode == AArch64_ISB) {
		const ISB *ISB = lookupISBByEncoding(Val);
		Name = ISB ? ISB->Name : NULL;
	} else if (Opcode == AArch64_TSB) {
		const TSB *TSB = lookupTSBByEncoding(Val);
		Name = TSB ? TSB->Name : NULL;
	} else {
		const DB *DB = lookupDBByEncoding(Val);
		Name = DB ? DB->Name : NULL;
	}

	if (Name) {
		SStream_concat0(O, Name);

		if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
			uint8_t access;

			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_BARRIER;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].barrier = Val;
			MI->flat_insn->detail->arm64.op_count++;
		}
	} else {
		printUInt32Bang(O, Val);

		if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
			uint8_t access;

			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = Val;
			MI->flat_insn->detail->arm64.op_count++;
		}
	}
}

static void printBarriernXSOption(MCInst *MI, unsigned OpNo, SStream *O) {
	unsigned Val = MCOperand_getImm(MCInst_getOperand(MI, OpNo));
	// assert(MI->getOpcode() == AArch64::DSBnXS);

	const char *Name = NULL;
	const DBnXS *DB = lookupDBnXSByEncoding(Val);
	Name = DB ? DB->Name : NULL;

	if (Name) {
		SStream_concat0(O, Name);

		if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
			uint8_t access;

			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_BARRIER;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].barrier = Val;
			MI->flat_insn->detail->arm64.op_count++;
		}
	}
	else {
		printUInt32Bang(O, Val);

		if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
			uint8_t access;

			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = Val;
			MI->flat_insn->detail->arm64.op_count++;
		}
	}
}

static void printMRSSystemRegister(MCInst *MI, unsigned OpNum, SStream *O)
{
	unsigned Val = (unsigned)MCOperand_getImm(MCInst_getOperand(MI, OpNum));
	const SysReg *Reg = lookupSysRegByEncoding(Val);

	// Horrible hack for the one register that has identical encodings but
	// different names in MSR and MRS. Because of this, one of MRS and MSR is
	// going to get the wrong entry
	if (Val == ARM64_SYSREG_DBGDTRRX_EL0) {
		SStream_concat0(O, "dbgdtrrx_el0");

		if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
			uint8_t access;

			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif

			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_SYS;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].sys = Val;
			MI->flat_insn->detail->arm64.op_count++;
		}

		return;
	}

	// Another hack for a register which has an alternative name which is not an alias,
	// and is not in the Armv9-A documentation.
	if( Val == ARM64_SYSREG_VSCTLR_EL2){
		SStream_concat0(O, "ttbr0_el2");

		if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
			uint8_t access;

			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif

			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_SYS;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].sys = Val;
			MI->flat_insn->detail->arm64.op_count++;
		}

		return;
	}

	// if (Reg && Reg->Readable && Reg->haveFeatures(STI.getFeatureBits()))
	if (Reg && Reg->Readable) {
		SStream_concat0(O, Reg->Name);

		if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
			uint8_t access;

			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif

			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_SYS;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].sys = Reg->Encoding;
			MI->flat_insn->detail->arm64.op_count++;
		}
	} else {
		char result[128];

		AArch64SysReg_genericRegisterString(Val, result);
		SStream_concat0(O, result);

		if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
			uint8_t access;
			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG_MRS;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = Val;
			MI->flat_insn->detail->arm64.op_count++;
		}
	}
}

static void printMSRSystemRegister(MCInst *MI, unsigned OpNum, SStream *O)
{
	unsigned Val = (unsigned)MCOperand_getImm(MCInst_getOperand(MI, OpNum));
	const SysReg *Reg = lookupSysRegByEncoding(Val);

	// Horrible hack for the one register that has identical encodings but
	// different names in MSR and MRS. Because of this, one of MRS and MSR is
	// going to get the wrong entry
	if (Val == ARM64_SYSREG_DBGDTRTX_EL0) {
		SStream_concat0(O, "dbgdtrtx_el0");

		if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
			uint8_t access;

			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif

			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_SYS;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].sys = Val;
			MI->flat_insn->detail->arm64.op_count++;
		}

		return;
	}

	// Another hack for a register which has an alternative name which is not an alias,
	// and is not in the Armv9-A documentation.
	if( Val == ARM64_SYSREG_VSCTLR_EL2){
		SStream_concat0(O, "ttbr0_el2");

		if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
			uint8_t access;

			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif

			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_SYS;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].sys = Val;
			MI->flat_insn->detail->arm64.op_count++;
		}

		return;
	}

	// if (Reg && Reg->Writeable && Reg->haveFeatures(STI.getFeatureBits()))
	if (Reg && Reg->Writeable) {
		SStream_concat0(O, Reg->Name);

		if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
			uint8_t access;

			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif

			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_SYS;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].sys = Reg->Encoding;
			MI->flat_insn->detail->arm64.op_count++;
		}
	} else {
		char result[128];

		AArch64SysReg_genericRegisterString(Val, result);
		SStream_concat0(O, result);

		if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
			uint8_t access;
			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG_MRS;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = Val;
			MI->flat_insn->detail->arm64.op_count++;
		}
	}
}

static void printSystemPStateField(MCInst *MI, unsigned OpNum, SStream *O)
{
	unsigned Val = (unsigned)MCOperand_getImm(MCInst_getOperand(MI, OpNum));

	const PState *PState = lookupPStateByEncoding(Val);

	if (PState) {
		SStream_concat0(O, PState->Name);

		if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
			uint8_t access;
			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_PSTATE;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].pstate = Val;
			MI->flat_insn->detail->arm64.op_count++;
		}
	} else {
		printUInt32Bang(O, Val);

		if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
			unsigned char access;

			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif

			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = Val;
			MI->flat_insn->detail->arm64.op_count++;
		}
	}
}

static void printSIMDType10Operand(MCInst *MI, unsigned OpNum, SStream *O)
{
	uint8_t RawVal = (uint8_t)MCOperand_getImm(MCInst_getOperand(MI, OpNum));
	uint64_t Val = AArch64_AM_decodeAdvSIMDModImmType10(RawVal);

	SStream_concat(O, "#%#016llx", Val);

	if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
		unsigned char access;

		access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
		MI->ac_idx++;
#endif
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_IMM;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].imm = Val;
		MI->flat_insn->detail->arm64.op_count++;
	}
}

static void printComplexRotationOp(MCInst *MI, unsigned OpNum, SStream *O, int64_t Angle, int64_t Remainder)
{
	unsigned int Val = MCOperand_getImm(MCInst_getOperand(MI, OpNum));
	printInt64Bang(O, (Val * Angle) + Remainder);
	op_addImm(MI, (Val * Angle) + Remainder);
}

static void printSVCROp(MCInst *MI, unsigned OpNum, SStream *O)
{
	MCOperand *MO = MCInst_getOperand(MI, OpNum);
  	// assert(MCOperand_isImm(MO) && "Unexpected operand type!");
  	unsigned svcrop = MCOperand_getImm(MO);
	const SVCR *svcr = lookupSVCRByEncoding(svcrop);
  	// assert(svcr && "Unexpected SVCR operand!");
	SStream_concat0(O, svcr->Name);

	if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
		uint8_t access;

		access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
		MI->ac_idx++;
#endif

		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_SVCR;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].sys = (unsigned)ARM64_SYSREG_SVCR;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].svcr = svcr->Encoding;
		MI->flat_insn->detail->arm64.op_count++;
	}
}

static void printMatrix(MCInst *MI, unsigned OpNum, SStream *O, int EltSize)
{
	MCOperand *RegOp = MCInst_getOperand(MI, OpNum);
  	// assert(MCOperand_isReg(RegOp) && "Unexpected operand type!");
	unsigned Reg = MCOperand_getReg(RegOp);

	SStream_concat0(O, getRegisterName(Reg, AArch64_NoRegAltName));
	const char *sizeStr = "";
  	switch (EltSize) {
  	case 0:
	  sizeStr = "";
  	  break;
  	case 8:
  	  sizeStr = ".b";
  	  break;
  	case 16:
  	  sizeStr = ".h";
  	  break;
  	case 32:
  	  sizeStr = ".s";
  	  break;
  	case 64:
  	  sizeStr = ".d";
  	  break;
  	case 128:
  	  sizeStr = ".q";
  	  break;
  	default:
	  break;
  	//   llvm_unreachable("Unsupported element size");
  	}
	SStream_concat0(O, sizeStr);

	if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
		uint8_t access;

		access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
		MI->ac_idx++;
#endif

		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = Reg;
		MI->flat_insn->detail->arm64.op_count++;
	}
}

static void printMatrixIndex(MCInst *MI, unsigned OpNum, SStream *O)
{
	int64_t imm = MCOperand_getImm(MCInst_getOperand(MI, OpNum));
	printInt64(O, imm);

	if (MI->csh->detail) {
		if (MI->csh->doing_SME_Index) {
			// Access op_count-1 as We want to add info to previous operand, not create a new one
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count-1].sme_index.disp = imm;
		}
	}
}

static void printMatrixTile(MCInst *MI, unsigned OpNum, SStream *O)
{
	MCOperand *RegOp = MCInst_getOperand(MI, OpNum);
  	// assert(MCOperand_isReg(RegOp) && "Unexpected operand type!");
	unsigned Reg = MCOperand_getReg(RegOp);
  	SStream_concat0(O, getRegisterName(Reg, AArch64_NoRegAltName));

	if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
		uint8_t access;

		access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
		MI->ac_idx++;
#endif

		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = Reg;
		MI->flat_insn->detail->arm64.op_count++;
	}
}

static void printMatrixTileVector(MCInst *MI, unsigned OpNum, SStream *O, bool IsVertical)
{
	MCOperand *RegOp = MCInst_getOperand(MI, OpNum);
  	// assert(MCOperand_isReg(RegOp) && "Unexpected operand type!");
	unsigned Reg = MCOperand_getReg(RegOp);
#ifndef CAPSTONE_DIET
	const char *RegName = getRegisterName(Reg, AArch64_NoRegAltName);

	const size_t strLn = strlen(RegName);
	// +2 for extra chars, + 1 for null char \0
	char *RegNameNew = cs_mem_malloc(sizeof(char) * (strLn + 2 + 1));
	int index = 0, i;
	for (i = 0; i < (strLn + 2); i++){
		if(RegName[i] != '.'){
			RegNameNew[index] = RegName[i];
			index++;
		}
		else{
			RegNameNew[index] = IsVertical ? 'v' : 'h';
			RegNameNew[index + 1] = '.';
			index += 2;
		}
	}
	SStream_concat0(O, RegNameNew);
#endif

	if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
		uint8_t access;

		access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
		MI->ac_idx++;
#endif

		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = Reg;
		MI->flat_insn->detail->arm64.op_count++;
	}
#ifndef CAPSTONE_DIET
	cs_mem_free(RegNameNew);
#endif
}

static const unsigned MatrixZADRegisterTable[] = {
  AArch64_ZAD0, AArch64_ZAD1, AArch64_ZAD2, AArch64_ZAD3,
  AArch64_ZAD4, AArch64_ZAD5, AArch64_ZAD6, AArch64_ZAD7
};

static void printMatrixTileList(MCInst *MI, unsigned OpNum, SStream *O){
	unsigned MaxRegs = 8;
	unsigned RegMask = MCOperand_getImm(MCInst_getOperand(MI, OpNum));

	unsigned NumRegs = 0, I;
	for (I = 0; I < MaxRegs; ++I)
		if ((RegMask & (1 << I)) != 0)
			++NumRegs;

	SStream_concat0(O, "{");
	unsigned Printed = 0, J;
	for (J = 0; J < MaxRegs; ++J) {
		unsigned Reg = RegMask & (1 << J);
		if (Reg == 0)
			continue;
		SStream_concat0(O, getRegisterName(MatrixZADRegisterTable[J], AArch64_NoRegAltName));

		if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
			uint8_t access;

			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif

			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = MatrixZADRegisterTable[J];
			MI->flat_insn->detail->arm64.op_count++;
		}

		if (Printed + 1 != NumRegs)
			SStream_concat0(O, ", ");
		++Printed;
	}
	SStream_concat0(O, "}");
}

static void printSVEPattern(MCInst *MI, unsigned OpNum, SStream *O)
{
	unsigned Val = MCOperand_getImm(MCInst_getOperand(MI, OpNum));

	const SVEPREDPAT *Pat = lookupSVEPREDPATByEncoding(Val);
	if (Pat)
		SStream_concat0(O, Pat->Name);
	else
		printUInt32Bang(O, Val);
}

// default suffix = 0
static void printSVERegOp(MCInst *MI, unsigned OpNum, SStream *O, char suffix)
{
	unsigned int Reg;

#if 0
	switch (suffix) {
		case 0:
		case 'b':
		case 'h':
		case 's':
		case 'd':
		case 'q':
			break;
		default:
			// llvm_unreachable("Invalid kind specifier.");
	}
#endif

	Reg = MCOperand_getReg(MCInst_getOperand(MI, OpNum));

	if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
			uint8_t access;

			access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
			MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
			MI->ac_idx++;
#endif
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = Reg;
		MI->flat_insn->detail->arm64.op_count++;
	}

	SStream_concat0(O, getRegisterName(Reg, AArch64_NoRegAltName));

	if (suffix != '\0')
		SStream_concat(O, ".%c", suffix);
}

static void printImmSVE16(int16_t Val, SStream *O)
{
	printUInt32Bang(O, Val);
}

static void printImmSVE32(int32_t Val, SStream *O)
{
	printUInt32Bang(O, Val);
}

static void printImmSVE64(int64_t Val, SStream *O)
{
	printUInt64Bang(O, Val);
}

static void printImm8OptLsl32(MCInst *MI, unsigned OpNum, SStream *O)
{
	unsigned UnscaledVal = MCOperand_getImm(MCInst_getOperand(MI, OpNum));
	unsigned Shift = MCOperand_getImm(MCInst_getOperand(MI, OpNum + 1));
	uint32_t Val;

	// assert(AArch64_AM::getShiftType(Shift) == AArch64_AM::LSL &&
	// 	"Unexepected shift type!");

	// #0 lsl #8 is never pretty printed
	if ((UnscaledVal == 0) && (AArch64_AM_getShiftValue(Shift) != 0)) {
		printUInt32Bang(O, UnscaledVal);
		printShifter(MI, OpNum + 1, O);
		return;
	}

	Val = UnscaledVal * (1 << AArch64_AM_getShiftValue(Shift));
	printImmSVE32(Val, O);
}

static void printImm8OptLsl64(MCInst *MI, unsigned OpNum, SStream *O)
{
	unsigned UnscaledVal = MCOperand_getImm(MCInst_getOperand(MI, OpNum));
	unsigned Shift = MCOperand_getImm(MCInst_getOperand(MI, OpNum + 1));
	uint64_t Val;

	// assert(AArch64_AM::getShiftType(Shift) == AArch64_AM::LSL &&
	// 	"Unexepected shift type!");

	// #0 lsl #8 is never pretty printed
	if ((UnscaledVal == 0) && (AArch64_AM_getShiftValue(Shift) != 0)) {
		printUInt32Bang(O, UnscaledVal);
		printShifter(MI, OpNum + 1, O);
		return;
	}

	Val = UnscaledVal * (1 << AArch64_AM_getShiftValue(Shift));
	printImmSVE64(Val, O);
}

static void printSVELogicalImm16(MCInst *MI, unsigned OpNum, SStream *O)
{
	uint64_t Val = MCOperand_getImm(MCInst_getOperand(MI, OpNum));
	uint64_t PrintVal = AArch64_AM_decodeLogicalImmediate(Val, 64);

	// Prefer the default format for 16bit values, hex otherwise.
	printImmSVE16(PrintVal, O);
}

static void printSVELogicalImm32(MCInst *MI, unsigned OpNum, SStream *O)
{
	uint64_t Val = MCOperand_getImm(MCInst_getOperand(MI, OpNum));
	uint64_t PrintVal = AArch64_AM_decodeLogicalImmediate(Val, 64);

	// Prefer the default format for 16bit values, hex otherwise.
	if ((uint16_t)PrintVal == (uint32_t)PrintVal)
		printImmSVE16(PrintVal, O);
	else
		printUInt64Bang(O, PrintVal);
}

static void printSVELogicalImm64(MCInst *MI, unsigned OpNum, SStream *O)
{
	uint64_t Val = MCOperand_getImm(MCInst_getOperand(MI, OpNum));
	uint64_t PrintVal = AArch64_AM_decodeLogicalImmediate(Val, 64);

	printImmSVE64(PrintVal, O);
}

static void printZPRasFPR(MCInst *MI, unsigned OpNum, SStream *O, int Width)
{
	unsigned int Base, Reg;

	switch (Width) {
		default: // llvm_unreachable("Unsupported width");
		case 8:   Base = AArch64_B0; break;
		case 16:  Base = AArch64_H0; break;
		case 32:  Base = AArch64_S0; break;
		case 64:  Base = AArch64_D0; break;
		case 128: Base = AArch64_Q0; break;
	}

	Reg = MCOperand_getReg(MCInst_getOperand(MI, OpNum)) - AArch64_Z0 + Base;

	SStream_concat0(O, getRegisterName(Reg, AArch64_NoRegAltName));

	if (MI->csh->detail) {
#ifndef CAPSTONE_DIET
		uint8_t access;

		access = get_op_access(MI->csh, MCInst_getOpcode(MI), MI->ac_idx);
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].access = access;
		MI->ac_idx++;
#endif
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].type = ARM64_OP_REG;
		MI->flat_insn->detail->arm64.operands[MI->flat_insn->detail->arm64.op_count].reg = Reg;
		MI->flat_insn->detail->arm64.op_count++;
	}
}

static void printExactFPImm(MCInst *MI, unsigned OpNum, SStream *O, unsigned ImmIs0, unsigned ImmIs1)
{
	const ExactFPImm *Imm0Desc = lookupExactFPImmByEnum(ImmIs0);
	const ExactFPImm *Imm1Desc = lookupExactFPImmByEnum(ImmIs1);
	unsigned Val = MCOperand_getImm(MCInst_getOperand(MI, OpNum));

	SStream_concat0(O, Val ? Imm1Desc->Repr : Imm0Desc->Repr);
}

static void printGPR64as32(MCInst *MI, unsigned OpNum, SStream *O)
{
	unsigned int Reg = MCOperand_getReg(MCInst_getOperand(MI, OpNum));

	SStream_concat0(O, getRegisterName(getWRegFromXReg(Reg), AArch64_NoRegAltName));
}

static void printGPR64x8(MCInst *MI, unsigned OpNum, SStream *O) 
{
  	unsigned int Reg = MCOperand_getReg(MCInst_getOperand(MI, OpNum));

  	SStream_concat0(O, getRegisterName(MCRegisterInfo_getSubReg(MI->MRI, Reg, AArch64_x8sub_0), AArch64_NoRegAltName));
}

#define PRINT_ALIAS_INSTR
#include "AArch64GenAsmWriter.inc"
#include "AArch64GenRegisterName.inc"

void AArch64_post_printer(csh handle, cs_insn *flat_insn, char *insn_asm, MCInst *mci)
{
	if (((cs_struct *)handle)->detail != CS_OPT_ON)
		return;

	if (mci->csh->detail) {
		unsigned opcode = MCInst_getOpcode(mci);

		switch (opcode) {
			default:
				break;
			case AArch64_LD1Fourv16b_POST:
			case AArch64_LD1Fourv1d_POST:
			case AArch64_LD1Fourv2d_POST:
			case AArch64_LD1Fourv2s_POST:
			case AArch64_LD1Fourv4h_POST:
			case AArch64_LD1Fourv4s_POST:
			case AArch64_LD1Fourv8b_POST:
			case AArch64_LD1Fourv8h_POST:
			case AArch64_LD1Onev16b_POST:
			case AArch64_LD1Onev1d_POST:
			case AArch64_LD1Onev2d_POST:
			case AArch64_LD1Onev2s_POST:
			case AArch64_LD1Onev4h_POST:
			case AArch64_LD1Onev4s_POST:
			case AArch64_LD1Onev8b_POST:
			case AArch64_LD1Onev8h_POST:
			case AArch64_LD1Rv16b_POST:
			case AArch64_LD1Rv1d_POST:
			case AArch64_LD1Rv2d_POST:
			case AArch64_LD1Rv2s_POST:
			case AArch64_LD1Rv4h_POST:
			case AArch64_LD1Rv4s_POST:
			case AArch64_LD1Rv8b_POST:
			case AArch64_LD1Rv8h_POST:
			case AArch64_LD1Threev16b_POST:
			case AArch64_LD1Threev1d_POST:
			case AArch64_LD1Threev2d_POST:
			case AArch64_LD1Threev2s_POST:
			case AArch64_LD1Threev4h_POST:
			case AArch64_LD1Threev4s_POST:
			case AArch64_LD1Threev8b_POST:
			case AArch64_LD1Threev8h_POST:
			case AArch64_LD1Twov16b_POST:
			case AArch64_LD1Twov1d_POST:
			case AArch64_LD1Twov2d_POST:
			case AArch64_LD1Twov2s_POST:
			case AArch64_LD1Twov4h_POST:
			case AArch64_LD1Twov4s_POST:
			case AArch64_LD1Twov8b_POST:
			case AArch64_LD1Twov8h_POST:
			case AArch64_LD1i16_POST:
			case AArch64_LD1i32_POST:
			case AArch64_LD1i64_POST:
			case AArch64_LD1i8_POST:
			case AArch64_LD2Rv16b_POST:
			case AArch64_LD2Rv1d_POST:
			case AArch64_LD2Rv2d_POST:
			case AArch64_LD2Rv2s_POST:
			case AArch64_LD2Rv4h_POST:
			case AArch64_LD2Rv4s_POST:
			case AArch64_LD2Rv8b_POST:
			case AArch64_LD2Rv8h_POST:
			case AArch64_LD2Twov16b_POST:
			case AArch64_LD2Twov2d_POST:
			case AArch64_LD2Twov2s_POST:
			case AArch64_LD2Twov4h_POST:
			case AArch64_LD2Twov4s_POST:
			case AArch64_LD2Twov8b_POST:
			case AArch64_LD2Twov8h_POST:
			case AArch64_LD2i16_POST:
			case AArch64_LD2i32_POST:
			case AArch64_LD2i64_POST:
			case AArch64_LD2i8_POST:
			case AArch64_LD3Rv16b_POST:
			case AArch64_LD3Rv1d_POST:
			case AArch64_LD3Rv2d_POST:
			case AArch64_LD3Rv2s_POST:
			case AArch64_LD3Rv4h_POST:
			case AArch64_LD3Rv4s_POST:
			case AArch64_LD3Rv8b_POST:
			case AArch64_LD3Rv8h_POST:
			case AArch64_LD3Threev16b_POST:
			case AArch64_LD3Threev2d_POST:
			case AArch64_LD3Threev2s_POST:
			case AArch64_LD3Threev4h_POST:
			case AArch64_LD3Threev4s_POST:
			case AArch64_LD3Threev8b_POST:
			case AArch64_LD3Threev8h_POST:
			case AArch64_LD3i16_POST:
			case AArch64_LD3i32_POST:
			case AArch64_LD3i64_POST:
			case AArch64_LD3i8_POST:
			case AArch64_LD4Fourv16b_POST:
			case AArch64_LD4Fourv2d_POST:
			case AArch64_LD4Fourv2s_POST:
			case AArch64_LD4Fourv4h_POST:
			case AArch64_LD4Fourv4s_POST:
			case AArch64_LD4Fourv8b_POST:
			case AArch64_LD4Fourv8h_POST:
			case AArch64_LD4Rv16b_POST:
			case AArch64_LD4Rv1d_POST:
			case AArch64_LD4Rv2d_POST:
			case AArch64_LD4Rv2s_POST:
			case AArch64_LD4Rv4h_POST:
			case AArch64_LD4Rv4s_POST:
			case AArch64_LD4Rv8b_POST:
			case AArch64_LD4Rv8h_POST:
			case AArch64_LD4i16_POST:
			case AArch64_LD4i32_POST:
			case AArch64_LD4i64_POST:
			case AArch64_LD4i8_POST:
			case AArch64_LDRBBpost:
			case AArch64_LDRBpost:
			case AArch64_LDRDpost:
			case AArch64_LDRHHpost:
			case AArch64_LDRHpost:
			case AArch64_LDRQpost:
			case AArch64_LDPDpost:
			case AArch64_LDPQpost:
			case AArch64_LDPSWpost:
			case AArch64_LDPSpost:
			case AArch64_LDPWpost:
			case AArch64_LDPXpost:
			case AArch64_ST1Fourv16b_POST:
			case AArch64_ST1Fourv1d_POST:
			case AArch64_ST1Fourv2d_POST:
			case AArch64_ST1Fourv2s_POST:
			case AArch64_ST1Fourv4h_POST:
			case AArch64_ST1Fourv4s_POST:
			case AArch64_ST1Fourv8b_POST:
			case AArch64_ST1Fourv8h_POST:
			case AArch64_ST1Onev16b_POST:
			case AArch64_ST1Onev1d_POST:
			case AArch64_ST1Onev2d_POST:
			case AArch64_ST1Onev2s_POST:
			case AArch64_ST1Onev4h_POST:
			case AArch64_ST1Onev4s_POST:
			case AArch64_ST1Onev8b_POST:
			case AArch64_ST1Onev8h_POST:
			case AArch64_ST1Threev16b_POST:
			case AArch64_ST1Threev1d_POST:
			case AArch64_ST1Threev2d_POST:
			case AArch64_ST1Threev2s_POST:
			case AArch64_ST1Threev4h_POST:
			case AArch64_ST1Threev4s_POST:
			case AArch64_ST1Threev8b_POST:
			case AArch64_ST1Threev8h_POST:
			case AArch64_ST1Twov16b_POST:
			case AArch64_ST1Twov1d_POST:
			case AArch64_ST1Twov2d_POST:
			case AArch64_ST1Twov2s_POST:
			case AArch64_ST1Twov4h_POST:
			case AArch64_ST1Twov4s_POST:
			case AArch64_ST1Twov8b_POST:
			case AArch64_ST1Twov8h_POST:
			case AArch64_ST1i16_POST:
			case AArch64_ST1i32_POST:
			case AArch64_ST1i64_POST:
			case AArch64_ST1i8_POST:
			case AArch64_ST2GPostIndex:
			case AArch64_ST2Twov16b_POST:
			case AArch64_ST2Twov2d_POST:
			case AArch64_ST2Twov2s_POST:
			case AArch64_ST2Twov4h_POST:
			case AArch64_ST2Twov4s_POST:
			case AArch64_ST2Twov8b_POST:
			case AArch64_ST2Twov8h_POST:
			case AArch64_ST2i16_POST:
			case AArch64_ST2i32_POST:
			case AArch64_ST2i64_POST:
			case AArch64_ST2i8_POST:
			case AArch64_ST3Threev16b_POST:
			case AArch64_ST3Threev2d_POST:
			case AArch64_ST3Threev2s_POST:
			case AArch64_ST3Threev4h_POST:
			case AArch64_ST3Threev4s_POST:
			case AArch64_ST3Threev8b_POST:
			case AArch64_ST3Threev8h_POST:
			case AArch64_ST3i16_POST:
			case AArch64_ST3i32_POST:
			case AArch64_ST3i64_POST:
			case AArch64_ST3i8_POST:
			case AArch64_ST4Fourv16b_POST:
			case AArch64_ST4Fourv2d_POST:
			case AArch64_ST4Fourv2s_POST:
			case AArch64_ST4Fourv4h_POST:
			case AArch64_ST4Fourv4s_POST:
			case AArch64_ST4Fourv8b_POST:
			case AArch64_ST4Fourv8h_POST:
			case AArch64_ST4i16_POST:
			case AArch64_ST4i32_POST:
			case AArch64_ST4i64_POST:
			case AArch64_ST4i8_POST:
			case AArch64_STPDpost:
			case AArch64_STPQpost:
			case AArch64_STPSpost:
			case AArch64_STPWpost:
			case AArch64_STPXpost:
			case AArch64_STRBBpost:
			case AArch64_STRBpost:
			case AArch64_STRDpost:
			case AArch64_STRHHpost:
			case AArch64_STRHpost:
			case AArch64_STRQpost:
			case AArch64_STRSpost:
			case AArch64_STRWpost:
			case AArch64_STRXpost:
			case AArch64_STZ2GPostIndex:
			case AArch64_STZGPostIndex:
			case AArch64_STGPostIndex:
			case AArch64_STGPpost:
			case AArch64_LDRSBWpost:
			case AArch64_LDRSBXpost:
			case AArch64_LDRSHWpost:
			case AArch64_LDRSHXpost:
			case AArch64_LDRSWpost:
			case AArch64_LDRSpost:
			case AArch64_LDRWpost:
			case AArch64_LDRXpost:
				flat_insn->detail->arm64.writeback = true;
			    flat_insn->detail->arm64.post_index = true;
				break;
			case AArch64_LDRAAwriteback:
			case AArch64_LDRABwriteback:
			case AArch64_ST2GPreIndex:
			case AArch64_LDPDpre:
			case AArch64_LDPQpre:
			case AArch64_LDPSWpre:
			case AArch64_LDPSpre:
			case AArch64_LDPWpre:
			case AArch64_LDPXpre:
			case AArch64_LDRBBpre:
			case AArch64_LDRBpre:
			case AArch64_LDRDpre:
			case AArch64_LDRHHpre:
			case AArch64_LDRHpre:
			case AArch64_LDRQpre:
			case AArch64_LDRSBWpre:
			case AArch64_LDRSBXpre:
			case AArch64_LDRSHWpre:
			case AArch64_LDRSHXpre:
			case AArch64_LDRSWpre:
			case AArch64_LDRSpre:
			case AArch64_LDRWpre:
			case AArch64_LDRXpre:
			case AArch64_STGPreIndex:
			case AArch64_STPDpre:
			case AArch64_STPQpre:
			case AArch64_STPSpre:
			case AArch64_STPWpre:
			case AArch64_STPXpre:
			case AArch64_STRBBpre:
			case AArch64_STRBpre:
			case AArch64_STRDpre:
			case AArch64_STRHHpre:
			case AArch64_STRHpre:
			case AArch64_STRQpre:
			case AArch64_STRSpre:
			case AArch64_STRWpre:
			case AArch64_STRXpre:
			case AArch64_STZ2GPreIndex:
			case AArch64_STZGPreIndex:
			case AArch64_STGPpre:
				flat_insn->detail->arm64.writeback = true;
				break;
		}
	}
}

#endif
