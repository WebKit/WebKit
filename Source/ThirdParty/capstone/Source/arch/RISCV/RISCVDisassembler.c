//===-- RISCVDisassembler.cpp - Disassembler for RISCV --------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

/* Capstone Disassembly Engine */
/* RISC-V Backend By Rodrigo Cortes Porto <porto703@gmail.com> & 
   Shawn Chang <citypw@gmail.com>, HardenedLinux@2018 */
    
#ifdef CAPSTONE_HAS_RISCV

#include <stdio.h>		// DEBUG
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
#include "RISCVBaseInfo.h"
#include "RISCVDisassembler.h"


/* Need the feature infos define in 
  RISCVGenSubtargetInfo.inc. */
#define GET_SUBTARGETINFO_ENUM
#include "RISCVGenSubtargetInfo.inc"

/* When we specify the RISCV64 mode, It means It is RV64IMAFD.
  Similar, RISCV32 means RV32IMAFD.
*/
static uint64_t getFeatureBits(int mode) 
{
	uint64_t ret = RISCV_FeatureStdExtM | RISCV_FeatureStdExtA |
		       RISCV_FeatureStdExtF | RISCV_FeatureStdExtD ;

	if (mode & CS_MODE_RISCV64)
		ret |= RISCV_Feature64Bit;
	if (mode & CS_MODE_RISCVC)
		ret |= RISCV_FeatureStdExtC;

	return ret;
}

#define GET_REGINFO_ENUM
#define GET_REGINFO_MC_DESC
#include "RISCVGenRegisterInfo.inc"
#define GET_INSTRINFO_ENUM
#include "RISCVGenInstrInfo.inc"

static const unsigned GPRDecoderTable[] = {
  	RISCV_X0,  RISCV_X1,  RISCV_X2,  RISCV_X3,
  	RISCV_X4,  RISCV_X5,  RISCV_X6,  RISCV_X7,
  	RISCV_X8,  RISCV_X9,  RISCV_X10, RISCV_X11,
  	RISCV_X12, RISCV_X13, RISCV_X14, RISCV_X15,
  	RISCV_X16, RISCV_X17, RISCV_X18, RISCV_X19,
  	RISCV_X20, RISCV_X21, RISCV_X22, RISCV_X23,
  	RISCV_X24, RISCV_X25, RISCV_X26, RISCV_X27,
  	RISCV_X28, RISCV_X29, RISCV_X30, RISCV_X31
};

static DecodeStatus DecodeGPRRegisterClass(MCInst *Inst, uint64_t RegNo,
		       	uint64_t Address, const void *Decoder) 
{
  	unsigned Reg = 0;

  	if (RegNo >= ARR_SIZE(GPRDecoderTable))
    		return MCDisassembler_Fail;

  	// We must define our own mapping from RegNo to register identifier.
  	// Accessing index RegNo in the register class will work in the case that
  	// registers were added in ascending order, but not in general.
  	Reg = GPRDecoderTable[RegNo];
  	//Inst.addOperand(MCOperand::createReg(Reg));
  	MCOperand_CreateReg0(Inst, Reg);
  	return MCDisassembler_Success;
}

static const unsigned FPR32DecoderTable[] = {
  	RISCV_F0_32,  RISCV_F1_32,  RISCV_F2_32,  RISCV_F3_32,
  	RISCV_F4_32,  RISCV_F5_32,  RISCV_F6_32,  RISCV_F7_32,
  	RISCV_F8_32,  RISCV_F9_32,  RISCV_F10_32, RISCV_F11_32,
  	RISCV_F12_32, RISCV_F13_32, RISCV_F14_32, RISCV_F15_32,
  	RISCV_F16_32, RISCV_F17_32, RISCV_F18_32, RISCV_F19_32,
  	RISCV_F20_32, RISCV_F21_32, RISCV_F22_32, RISCV_F23_32,
  	RISCV_F24_32, RISCV_F25_32, RISCV_F26_32, RISCV_F27_32,
  	RISCV_F28_32, RISCV_F29_32, RISCV_F30_32, RISCV_F31_32
};

static DecodeStatus DecodeFPR32RegisterClass(MCInst *Inst, uint64_t RegNo,
			 uint64_t Address, const void *Decoder) 
{
  	unsigned Reg = 0;

  	if (RegNo >= ARR_SIZE(FPR32DecoderTable))
    		return MCDisassembler_Fail;

  	// We must define our own mapping from RegNo to register identifier.
  	// Accessing index RegNo in the register class will work in the case that
  	// registers were added in ascending order, but not in general.
  	Reg = FPR32DecoderTable[RegNo];
  	MCOperand_CreateReg0(Inst, Reg);
  	return MCDisassembler_Success;
}

static DecodeStatus DecodeFPR32CRegisterClass(MCInst *Inst, uint64_t RegNo,
                                              uint64_t Address,
                                              const void *Decoder) 
{
  	unsigned Reg = 0;

  	if (RegNo > 8) 
    		return MCDisassembler_Fail;
  	Reg = FPR32DecoderTable[RegNo + 8];
  	MCOperand_CreateReg0(Inst, Reg);
  	return MCDisassembler_Success;
}

static const unsigned FPR64DecoderTable[] = {
  	RISCV_F0_64,  RISCV_F1_64,  RISCV_F2_64,  RISCV_F3_64,
  	RISCV_F4_64,  RISCV_F5_64,  RISCV_F6_64,  RISCV_F7_64,
  	RISCV_F8_64,  RISCV_F9_64,  RISCV_F10_64, RISCV_F11_64,
  	RISCV_F12_64, RISCV_F13_64, RISCV_F14_64, RISCV_F15_64,
  	RISCV_F16_64, RISCV_F17_64, RISCV_F18_64, RISCV_F19_64,
  	RISCV_F20_64, RISCV_F21_64, RISCV_F22_64, RISCV_F23_64,
  	RISCV_F24_64, RISCV_F25_64, RISCV_F26_64, RISCV_F27_64,
  	RISCV_F28_64, RISCV_F29_64, RISCV_F30_64, RISCV_F31_64
};

static DecodeStatus DecodeFPR64RegisterClass(MCInst *Inst, uint64_t RegNo,
			 uint64_t Address, const void *Decoder) 
{
  	unsigned Reg = 0;

  	if (RegNo >= ARR_SIZE(FPR64DecoderTable))
    		return MCDisassembler_Fail;

 	// We must define our own mapping from RegNo to register identifier.
  	// Accessing index RegNo in the register class will work in the case that
  	// registers were added in ascending order, but not in general.
  	Reg = FPR64DecoderTable[RegNo];
  	MCOperand_CreateReg0(Inst, Reg);
  	return MCDisassembler_Success;
}

static DecodeStatus DecodeFPR64CRegisterClass(MCInst *Inst, uint64_t RegNo,
                                              uint64_t Address,
                                              const void *Decoder) 
{
  	unsigned Reg = 0;

  	if (RegNo > 8)
    		return MCDisassembler_Fail;
  	Reg = FPR64DecoderTable[RegNo + 8];
  	MCOperand_CreateReg0(Inst, Reg);
  	return MCDisassembler_Success;
}

static DecodeStatus DecodeGPRNoX0RegisterClass(MCInst *Inst, uint64_t RegNo,
                                               uint64_t Address,
                                               const void *Decoder) 
{
  	if (RegNo == 0)
    		return MCDisassembler_Fail;
  	return DecodeGPRRegisterClass(Inst, RegNo, Address, Decoder);
}

static DecodeStatus DecodeGPRNoX0X2RegisterClass(MCInst *Inst, uint64_t RegNo,
                                                 uint64_t Address,
                                                 const void *Decoder) 
{
  	if (RegNo == 2)
    		return MCDisassembler_Fail;
  	return DecodeGPRNoX0RegisterClass(Inst, RegNo, Address, Decoder);
}

static DecodeStatus DecodeGPRCRegisterClass(MCInst *Inst, uint64_t RegNo,
                                            uint64_t Address,
                                            const void *Decoder) 
{
  	unsigned Reg = 0;

  	if (RegNo > 8)
    		return MCDisassembler_Fail;

  	Reg = GPRDecoderTable[RegNo + 8];
  	MCOperand_CreateReg0(Inst, Reg);
  	return MCDisassembler_Success;
}

// Add implied SP operand for instructions *SP compressed instructions. The SP
// operand isn't explicitly encoded in the instruction.
static void addImplySP(MCInst *Inst, int64_t Address, const void *Decoder) 
{
  	if (MCInst_getOpcode(Inst) == RISCV_C_LWSP || 
      	    MCInst_getOpcode(Inst) == RISCV_C_SWSP ||
      	    MCInst_getOpcode(Inst) == RISCV_C_LDSP || 
      	    MCInst_getOpcode(Inst) == RISCV_C_SDSP ||
            MCInst_getOpcode(Inst) == RISCV_C_FLWSP ||
            MCInst_getOpcode(Inst) == RISCV_C_FSWSP ||
            MCInst_getOpcode(Inst) == RISCV_C_FLDSP ||
            MCInst_getOpcode(Inst) == RISCV_C_FSDSP ||
            MCInst_getOpcode(Inst) == RISCV_C_ADDI4SPN) {
		DecodeGPRRegisterClass(Inst, 2, Address, Decoder);
  	}

  	if (MCInst_getOpcode(Inst) == RISCV_C_ADDI16SP) {
    		DecodeGPRRegisterClass(Inst, 2, Address, Decoder);
    		DecodeGPRRegisterClass(Inst, 2, Address, Decoder);
  	}
}

static DecodeStatus decodeUImmOperand(MCInst *Inst, uint64_t Imm,
                                      int64_t Address, const void *Decoder,
				      unsigned N) 
{
  	//CS_ASSERT(isUInt<N>(Imm) && "Invalid immediate");
  	addImplySP(Inst, Address, Decoder);
  	//Inst.addOperand(MCOperand::createImm(Imm));
  	MCOperand_CreateImm0(Inst, Imm);
  	return MCDisassembler_Success;
}

static DecodeStatus decodeUImmNonZeroOperand(MCInst *Inst, uint64_t Imm,
                                             int64_t Address,
                                             const void *Decoder, 
					     unsigned N) 
{
  	if (Imm == 0)
    		return MCDisassembler_Fail;
  	return decodeUImmOperand(Inst, Imm, Address, Decoder, N);
}

static DecodeStatus decodeSImmOperand(MCInst *Inst, uint64_t Imm,
                                      int64_t Address, const void *Decoder,
				      unsigned N) 
{
  	//CS_ASSERT(isUInt<N>(Imm) && "Invalid immediate");
  	addImplySP(Inst, Address, Decoder);
  	// Sign-extend the number in the bottom N bits of Imm
  	//Inst.addOperand(MCOperand::createImm(SignExtend64<N>(Imm)));
  	MCOperand_CreateImm0(Inst, SignExtend64(Imm, N));
  	return MCDisassembler_Success;
}

static DecodeStatus decodeSImmNonZeroOperand(MCInst *Inst, uint64_t Imm,
                                             int64_t Address,
                                             const void *Decoder,
					     unsigned N) 
{
  	if (Imm == 0)
    		return MCDisassembler_Fail;
  	return decodeSImmOperand(Inst, Imm, Address, Decoder, N);
}

static DecodeStatus decodeSImmOperandAndLsl1(MCInst *Inst, uint64_t Imm,
                                             int64_t Address,
                                             const void *Decoder,
					     unsigned N) 
{
  	//CS_ASSERT(isUInt<N>(Imm) && "Invalid immediate");
  	// Sign-extend the number in the bottom N bits of Imm after accounting for
  	// the fact that the N bit immediate is stored in N-1 bits (the LSB is
  	// always zero)
  	//Inst.addOperand(MCOperand::createImm(SignExtend64<N>(Imm << 1)));
  	MCOperand_CreateImm0(Inst, SignExtend64(Imm << 1, N));
  	return MCDisassembler_Success;
}

static DecodeStatus decodeCLUIImmOperand(MCInst *Inst, uint64_t Imm,
                                         int64_t Address,
                                         const void *Decoder) 
{
  	//CS_ASSERT(isUInt<6>(Imm) && "Invalid immediate");
  	if (Imm > 31) {
    		Imm = (SignExtend64(Imm, 6) & 0xfffff);
  	}
  	//Inst.addOperand(MCOperand::createImm(Imm));
  	MCOperand_CreateImm0(Inst, Imm);
  	return MCDisassembler_Success;
}

static DecodeStatus decodeFRMArg(MCInst *Inst, uint64_t Imm,
                                 int64_t Address,
                                 const void *Decoder) 
{
  	//CS_ASSERT(isUInt<3>(Imm) && "Invalid immediate");
  	if (!RISCVFPRndMode_isValidRoundingMode(Imm))
    		return MCDisassembler_Fail;

  	//Inst.addOperand(MCOperand::createImm(Imm));
  	MCOperand_CreateImm0(Inst, Imm);
  	return MCDisassembler_Success;
}


#include "RISCVGenDisassemblerTables.inc"

static void init_MI_insn_detail(MCInst *MI) 
{
  	if (MI->flat_insn->detail) {
    		memset(MI->flat_insn->detail, 0, sizeof(cs_detail));
  	}

  	return;
}

// mark the load/store instructions through the opcode.
static void markLSInsn(MCInst *MI, uint32_t in)
{
	/* 
	   I   ld 0000011 = 0x03
	       st 0100011 = 0x23
	   F/D ld 0000111 = 0x07
	       st 0100111 = 0x27
	*/
#define MASK_LS_INSN 0x0000007f
	uint32_t opcode = in & MASK_LS_INSN;
	if (0 == (opcode ^ 0x03) || 0 == (opcode ^ 0x07) ||
	    0 == (opcode ^ 0x23) || 0 == (opcode ^ 0x27))
		MI->flat_insn->detail->riscv.need_effective_addr = true;
#undef MASK_LS_INSN
	return;
}

static DecodeStatus RISCVDisassembler_getInstruction(int mode, MCInst *MI,
				 const uint8_t *code, size_t code_len,
				 uint16_t *Size, uint64_t Address,
				 MCRegisterInfo *MRI) 
{
  	// TODO: This will need modification when supporting instruction set
  	// extensions with instructions > 32-bits (up to 176 bits wide).
  	uint32_t Inst = 0;
  	DecodeStatus Result;

  	// It's a 32 bit instruction if bit 0 and 1 are 1.
  	if ((code[0] & 0x3) == 0x3) {
      		if (code_len < 4) {
        		*Size = 0;
        		return MCDisassembler_Fail;
      		}

      		*Size = 4;
      		// Get the four bytes of the instruction.
      		//Encoded as little endian 32 bits.
      		Inst = code[0] | (code[1] << 8) | (code[2] << 16) | ((uint32_t)code[3] << 24);
		init_MI_insn_detail(MI);
		// Now we need mark what instruction need fix effective address output.
    		if (MI->csh->detail) 
			markLSInsn(MI, Inst);
      		Result = decodeInstruction(DecoderTable32, MI, Inst, Address, MRI, mode);
  	} else {
    		if (code_len < 2) {
      			*Size = 0;
      			return MCDisassembler_Fail;
    		}

		// If not b4bit.
    		if (! (getFeatureBits(mode) & ((uint64_t)RISCV_Feature64Bit))) {
      			// Trying RISCV32Only_16 table (16-bit Instruction)
      			Inst = code[0] | (code[1] << 8);
      			init_MI_insn_detail(MI);
      			Result = decodeInstruction(DecoderTableRISCV32Only_16, MI, Inst, Address,
                                 	   	   MRI, mode);
      			if (Result != MCDisassembler_Fail) {
        			*Size = 2;
        			return Result;
      			}
    		}
    
    		// Trying RISCV_C table (16-bit Instruction)
    		Inst = code[0] | (code[1] << 8);
    		init_MI_insn_detail(MI);
    		// Calling the auto-generated decoder function.
    		Result = decodeInstruction(DecoderTable16, MI, Inst, Address, MRI, mode);
    		*Size = 2;
  	}

  	return Result;
}

bool RISCV_getInstruction(csh ud, const uint8_t *code, size_t code_len,
		          MCInst *instr, uint16_t *size, uint64_t address,
		          void *info) 
{
  	cs_struct *handle = (cs_struct *)(uintptr_t)ud;

  	return MCDisassembler_Success == 
	   	RISCVDisassembler_getInstruction(handle->mode, instr,
				            	 code, code_len,
			                    	 size, address,
			                    	 (MCRegisterInfo *)info);

}

void RISCV_init(MCRegisterInfo * MRI) 
{
  	/*
  	InitMCRegisterInfo(RISCVRegDesc, 97, RA, PC,
                     RISCVMCRegisterClasses, 11,
                     RISCVRegUnitRoots,
                     64,
                     RISCVRegDiffLists,
                     RISCVLaneMaskLists,
                     RISCVRegStrings,
                     RISCVRegClassStrings,
                     RISCVSubRegIdxLists,
                     2,
                     RISCVSubRegIdxRanges,
                     RISCVRegEncodingTable);
  	*/

  	MCRegisterInfo_InitMCRegisterInfo(MRI, RISCVRegDesc, 97, 0, 0,
				    	  RISCVMCRegisterClasses, 11,
				          0, 
				          0,
				          RISCVRegDiffLists,
				          0, 
				          RISCVSubRegIdxLists, 
				          2, 
				          0);
}

#endif
