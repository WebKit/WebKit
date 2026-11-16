//===-- llvm/MC/MCInstrDesc.h - Instruction Descriptors -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the MCOperandInfo and MCInstrDesc classes, which
// are used to describe target instructions and their operands.
//
//===----------------------------------------------------------------------===//

/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2019 */

#ifndef CS_LLVM_MC_MCINSTRDESC_H
#define CS_LLVM_MC_MCINSTRDESC_H

#include "MCRegisterInfo.h"
#include "capstone/platform.h"

//===----------------------------------------------------------------------===//
// Machine Operand Flags and Description
//===----------------------------------------------------------------------===//

/// Operand constraints. These are encoded in 16 bits with one of the
/// low-order 3 bits specifying that a constraint is present and the
/// corresponding high-order hex digit specifying the constraint value.
/// This allows for a maximum of 3 constraints.
typedef enum {
	MCOI_TIED_TO = 0,    // Operand tied to another operand.
	MCOI_EARLY_CLOBBER   // Operand is an early clobber register operand
} MCOI_OperandConstraint;

// Define a macro to produce each constraint value.
#define CONSTRAINT_MCOI_TIED_TO(op) \
  ((1 << MCOI_TIED_TO) | ((op) << (4 + MCOI_TIED_TO * 4)))

#define CONSTRAINT_MCOI_EARLY_CLOBBER \
  (1 << MCOI_EARLY_CLOBBER)

/// OperandFlags - These are flags set on operands, but should be considered
/// private, all access should go through the MCOperandInfo accessors.
/// See the accessors for a description of what these are.
enum MCOI_OperandFlags {
	MCOI_LookupPtrRegClass = 0,
	MCOI_Predicate,
	MCOI_OptionalDef
};

/// Operand Type - Operands are tagged with one of the values of this enum.
enum MCOI_OperandType {
	MCOI_OPERAND_UNKNOWN = 0,
	MCOI_OPERAND_IMMEDIATE = 1,
	MCOI_OPERAND_REGISTER = 2,
	MCOI_OPERAND_MEMORY = 3,
	MCOI_OPERAND_PCREL = 4,

	MCOI_OPERAND_FIRST_GENERIC = 6,
	MCOI_OPERAND_GENERIC_0 = 6,
	MCOI_OPERAND_GENERIC_1 = 7,
	MCOI_OPERAND_GENERIC_2 = 8,
	MCOI_OPERAND_GENERIC_3 = 9,
	MCOI_OPERAND_GENERIC_4 = 10,
	MCOI_OPERAND_GENERIC_5 = 11,
	MCOI_OPERAND_LAST_GENERIC = 11,

	MCOI_OPERAND_FIRST_GENERIC_IMM = 12,
	MCOI_OPERAND_GENERIC_IMM_0 = 12,
	MCOI_OPERAND_LAST_GENERIC_IMM = 12,

	MCOI_OPERAND_FIRST_TARGET = 13,
};


/// MCOperandInfo - This holds information about one operand of a machine
/// instruction, indicating the register class for register operands, etc.
///
typedef struct MCOperandInfo {
	/// This specifies the register class enumeration of the operand
	/// if the operand is a register.  If isLookupPtrRegClass is set, then this is
	/// an index that is passed to TargetRegisterInfo::getPointerRegClass(x) to
	/// get a dynamic register class.
	int16_t RegClass;

	/// These are flags from the MCOI::OperandFlags enum.
	uint8_t Flags;

	/// Information about the type of the operand.
	uint8_t OperandType;

	/// The lower 16 bits are used to specify which constraints are set.
	/// The higher 16 bits are used to specify the value of constraints (4 bits each).
	uint32_t Constraints;
	/// Currently no other information.
} MCOperandInfo;


//===----------------------------------------------------------------------===//
// Machine Instruction Flags and Description
//===----------------------------------------------------------------------===//

/// MCInstrDesc flags - These should be considered private to the
/// implementation of the MCInstrDesc class. Clients should use the predicate
/// methods on MCInstrDesc, not use these directly. These all correspond to
/// bitfields in the MCInstrDesc::Flags field.
enum {
	MCID_Variadic = 0,
	MCID_HasOptionalDef,
	MCID_Pseudo,
	MCID_Return,
	MCID_Call,
	MCID_Barrier,
	MCID_Terminator,
	MCID_Branch,
	MCID_IndirectBranch,
	MCID_Compare,
	MCID_MoveImm,
	MCID_MoveReg,
	MCID_Bitcast,
	MCID_Select,
	MCID_DelaySlot,
	MCID_FoldableAsLoad,
	MCID_MayLoad,
	MCID_MayStore,
	MCID_Predicable,
	MCID_NotDuplicable,
	MCID_UnmodeledSideEffects,
	MCID_Commutable,
	MCID_ConvertibleTo3Addr,
	MCID_UsesCustomInserter,
	MCID_HasPostISelHook,
	MCID_Rematerializable,
	MCID_CheapAsAMove,
	MCID_ExtraSrcRegAllocReq,
	MCID_ExtraDefRegAllocReq,
	MCID_RegSequence,
	MCID_ExtractSubreg,
	MCID_InsertSubreg,
	MCID_Convergent,
	MCID_Add,
	MCID_Trap,
};

/// MCInstrDesc - Describe properties that are true of each instruction in the
/// target description file. This captures information about side effects,
/// register use and many other things. There is one instance of this struct
/// for each target instruction class, and the MachineInstr class points to
/// this struct directly to describe itself.
typedef struct MCInstrDesc {
	unsigned char  NumOperands;   // Num of args (may be more if variable_ops)
	const MCOperandInfo *OpInfo;   // 'NumOperands' entries about operands
} MCInstrDesc;

bool MCOperandInfo_isPredicate(const MCOperandInfo *m);

bool MCOperandInfo_isOptionalDef(const MCOperandInfo *m);

bool MCOperandInfo_isTiedToOp(const MCOperandInfo *m);

int MCOperandInfo_getOperandConstraint(const MCInstrDesc *OpInfo,
				       unsigned OpNum,
				       MCOI_OperandConstraint Constraint);

#endif
