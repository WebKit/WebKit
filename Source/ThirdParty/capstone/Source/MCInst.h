//===-- llvm/MC/MCInst.h - MCInst class -------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of the MCInst and MCOperand classes, which
// is the basic representation used to represent low-level machine code
// instructions.
//
//===----------------------------------------------------------------------===//

/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2019 */

#ifndef CS_MCINST_H
#define CS_MCINST_H

#include "include/capstone/capstone.h"
#include "MCInstrDesc.h"
#include "MCRegisterInfo.h"

typedef struct MCInst MCInst;
typedef struct cs_struct cs_struct;
typedef struct MCOperand MCOperand;

/// MCOperand - Instances of this class represent operands of the MCInst class.
/// This is a simple discriminated union.
struct MCOperand {
	enum {
		kInvalid = 0,  ///< Uninitialized.
		kRegister,     ///< Register operand.
		kImmediate,    ///< Immediate operand.
		kFPImmediate,  ///< Floating-point immediate operand.
		kDFPImmediate, ///< Double-Floating-point immediate operand.
		kExpr,	       ///< Relocatable immediate operand.
		kInst	       ///< Sub-instruction operand.

	} MachineOperandType;
	unsigned char Kind;

	union {
		unsigned RegVal;
		int64_t ImmVal;
		double FPImmVal;
	};
};

bool MCOperand_isValid(const MCOperand *op);

bool MCOperand_isReg(const MCOperand *op);

bool MCOperand_isImm(const MCOperand *op);

bool MCOperand_isFPImm(const MCOperand *op);

bool MCOperand_isDFPImm(const MCOperand *op);

bool MCOperand_isExpr(const MCOperand *op);

bool MCOperand_isInst(const MCOperand *op);

/// getReg - Returns the register number.
unsigned MCOperand_getReg(const MCOperand *op);

/// setReg - Set the register number.
void MCOperand_setReg(MCOperand *op, unsigned Reg);

int64_t MCOperand_getImm(MCOperand *op);

void MCOperand_setImm(MCOperand *op, int64_t Val);

double MCOperand_getFPImm(const MCOperand *op);

void MCOperand_setFPImm(MCOperand *op, double Val);

const MCInst *MCOperand_getInst(const MCOperand *op);

void MCOperand_setInst(MCOperand *op, const MCInst *Val);

// create Reg operand in the next slot
void MCOperand_CreateReg0(MCInst *inst, unsigned Reg);

// create Reg operand use the last-unused slot
MCOperand *MCOperand_CreateReg1(MCInst *inst, unsigned Reg);

// create Imm operand in the next slot
void MCOperand_CreateImm0(MCInst *inst, int64_t Val);

// create Imm operand in the last-unused slot
MCOperand *MCOperand_CreateImm1(MCInst *inst, int64_t Val);

#define MAX_MC_OPS 48

/// MCInst - Instances of this class represent a single low-level machine
/// instruction.
struct MCInst {
	unsigned OpcodePub;  // public opcode (<arch>_INS_yyy in header files <arch>.h)
	uint8_t size;	// number of operands
	bool has_imm;	// indicate this instruction has an X86_OP_IMM operand - used for ATT syntax
	uint8_t op1_size; // size of 1st operand - for X86 Intel syntax
	unsigned Opcode;  // private opcode
	MCOperand Operands[MAX_MC_OPS];
	cs_insn *flat_insn;	// insn to be exposed to public
	uint64_t address;	// address of this insn
	cs_struct *csh;	// save the main csh
	uint8_t x86opsize;	// opsize for [mem] operand

	// These flags could be used to pass some info from one target subcomponent
	// to another, for example, from disassembler to asm printer. The values of
	// the flags have any sense on target level only (e.g. prefixes on x86).
	unsigned flags;

	// (Optional) instruction prefix, which can be up to 4 bytes.
	// A prefix byte gets value 0 when irrelevant.
	// This is copied from cs_x86 struct
	uint8_t x86_prefix[4];
	uint8_t imm_size;	// immediate size for X86_OP_IMM operand
	bool writeback;		// writeback for ARM
	int8_t tied_op_idx
		[MAX_MC_OPS]; ///< Tied operand indices. Index = Src op; Value: Dest op
	// operand access index for list of registers sharing the same access right (for ARM)
	uint8_t ac_idx;
	uint8_t popcode_adjust;   // Pseudo X86 instruction adjust
	char assembly[8];	// for special instruction, so that we dont need printer
	unsigned char evm_data[32];	// for EVM PUSH operand
	cs_wasm_op wasm_data;    // for WASM operand
	MCRegisterInfo *MRI;
	uint8_t xAcquireRelease;   // X86 xacquire/xrelease
};

void MCInst_Init(MCInst *inst);

void MCInst_clear(MCInst *inst);

// do not free operand after inserting
void MCInst_insert0(MCInst *inst, int index, MCOperand *Op);

void MCInst_setOpcode(MCInst *inst, unsigned Op);

unsigned MCInst_getOpcode(const MCInst*);

void MCInst_setOpcodePub(MCInst *inst, unsigned Op);

unsigned MCInst_getOpcodePub(const MCInst*);

MCOperand *MCInst_getOperand(MCInst *inst, unsigned i);

unsigned MCInst_getNumOperands(const MCInst *inst);

// This addOperand2 function doesnt free Op
void MCInst_addOperand2(MCInst *inst, MCOperand *Op);

bool MCInst_isPredicable(const MCInstrDesc *MIDesc);

void MCInst_handleWriteback(MCInst *MI, const MCInstrDesc *InstDesc);

bool MCInst_opIsTied(const MCInst *MI, unsigned OpNum);

bool MCInst_opIsTying(const MCInst *MI, unsigned OpNum);

#endif
