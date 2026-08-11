/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2019 */

#if defined(CAPSTONE_HAS_OSXKERNEL)
#include <Availability.h>
#include <libkern/libkern.h>
#else
#include <stdio.h>
#include <stdlib.h>
#endif
#include <string.h>
#include <assert.h>

#include "MCInst.h"
#include "utils.h"

#define MCINST_CACHE (ARR_SIZE(mcInst->Operands) - 1)

void MCInst_Init(MCInst *inst)
{
	// unnecessary to initialize in loop . its expensive and inst->size shuold be honored
	inst->Operands[0].Kind = kInvalid;
	inst->Operands[0].ImmVal = 0;

	inst->Opcode = 0;
	inst->OpcodePub = 0;
	inst->size = 0;
	inst->has_imm = false;
	inst->op1_size = 0;
	inst->writeback = false;
	inst->ac_idx = 0;
	inst->popcode_adjust = 0;
	inst->assembly[0] = '\0';
	inst->wasm_data.type = WASM_OP_INVALID;
	inst->xAcquireRelease = 0;
	for (int i = 0; i < MAX_MC_OPS; ++i)
		inst->tied_op_idx[i] = -1;
}

void MCInst_clear(MCInst *inst)
{
	inst->size = 0;
}

// does not free @Op
void MCInst_insert0(MCInst *inst, int index, MCOperand *Op)
{
	assert(index < MAX_MC_OPS);
	int i;

	for(i = inst->size; i > index; i--)
		//memcpy(&(inst->Operands[i]), &(inst->Operands[i-1]), sizeof(MCOperand));
		inst->Operands[i] = inst->Operands[i-1];

	inst->Operands[index] = *Op;
	inst->size++;
}

void MCInst_setOpcode(MCInst *inst, unsigned Op)
{
	inst->Opcode = Op;
}

void MCInst_setOpcodePub(MCInst *inst, unsigned Op)
{
	inst->OpcodePub = Op;
}

unsigned MCInst_getOpcode(const MCInst *inst)
{
	return inst->Opcode;
}

unsigned MCInst_getOpcodePub(const MCInst *inst)
{
	return inst->OpcodePub;
}

MCOperand *MCInst_getOperand(MCInst *inst, unsigned i)
{
	assert(i < MAX_MC_OPS);
	return &inst->Operands[i];
}

unsigned MCInst_getNumOperands(const MCInst *inst)
{
	return inst->size;
}

// This addOperand2 function doesnt free Op
void MCInst_addOperand2(MCInst *inst, MCOperand *Op)
{
	assert(inst->size < MAX_MC_OPS);
	inst->Operands[inst->size] = *Op;

	inst->size++;
}

bool MCOperand_isValid(const MCOperand *op)
{
	return op->Kind != kInvalid;
}

bool MCOperand_isReg(const MCOperand *op)
{
	return op->Kind == kRegister;
}

bool MCOperand_isImm(const MCOperand *op)
{
	return op->Kind == kImmediate;
}

bool MCOperand_isFPImm(const MCOperand *op)
{
	return op->Kind == kFPImmediate;
}

bool MCOperand_isDFPImm(const MCOperand *op)
{
	return op->Kind == kDFPImmediate;
}

bool MCOperand_isExpr(const MCOperand *op)
{
	return op->Kind == kExpr;
}

bool MCOperand_isInst(const MCOperand *op)
{
	return op->Kind == kInst;
}

/// getReg - Returns the register number.
unsigned MCOperand_getReg(const MCOperand *op)
{
	return op->RegVal;
}

/// setReg - Set the register number.
void MCOperand_setReg(MCOperand *op, unsigned Reg)
{
	op->RegVal = Reg;
}

int64_t MCOperand_getImm(MCOperand *op)
{
	return op->ImmVal;
}

void MCOperand_setImm(MCOperand *op, int64_t Val)
{
	op->ImmVal = Val;
}

double MCOperand_getFPImm(const MCOperand *op)
{
	return op->FPImmVal;
}

void MCOperand_setFPImm(MCOperand *op, double Val)
{
	op->FPImmVal = Val;
}

MCOperand *MCOperand_CreateReg1(MCInst *mcInst, unsigned Reg)
{
	MCOperand *op = &(mcInst->Operands[MCINST_CACHE]);

	op->MachineOperandType = kRegister;
	op->Kind = kRegister;
	op->RegVal = Reg;

	return op;
}

void MCOperand_CreateReg0(MCInst *mcInst, unsigned Reg)
{
	MCOperand *op = &(mcInst->Operands[mcInst->size]);
	mcInst->size++;

	op->MachineOperandType = kRegister;
	op->Kind = kRegister;
	op->RegVal = Reg;
}

MCOperand *MCOperand_CreateImm1(MCInst *mcInst, int64_t Val)
{
	MCOperand *op = &(mcInst->Operands[MCINST_CACHE]);

	op->MachineOperandType = kImmediate;
	op->Kind = kImmediate;
	op->ImmVal = Val;

	return op;
}

void MCOperand_CreateImm0(MCInst *mcInst, int64_t Val)
{
	assert(mcInst->size < MAX_MC_OPS);
	MCOperand *op = &(mcInst->Operands[mcInst->size]);
	mcInst->size++;

	op->MachineOperandType = kImmediate;
	op->Kind = kImmediate;
	op->ImmVal = Val;
}

/// Check if any operand of the MCInstrDesc is predicable
bool MCInst_isPredicable(const MCInstrDesc *MIDesc)
{
	const MCOperandInfo *OpInfo = MIDesc->OpInfo;
	unsigned NumOps = MIDesc->NumOperands;
	for (unsigned i = 0; i < NumOps; ++i) {
		if (MCOperandInfo_isPredicate(&OpInfo[i])) {
			return true;
		}
	}
	return false;
}

/// Checks if tied operands exist in the instruction and sets
/// - The writeback flag in detail
/// - Saves the indices of the tied destination operands.
void MCInst_handleWriteback(MCInst *MI, const MCInstrDesc *InstDesc)
{
	const MCOperandInfo *OpInfo = InstDesc[MCInst_getOpcode(MI)].OpInfo;
	unsigned short NumOps = InstDesc[MCInst_getOpcode(MI)].NumOperands;

	unsigned i;
	for (i = 0; i < NumOps; ++i) {
		if (MCOperandInfo_isTiedToOp(&OpInfo[i])) {
			int idx = MCOperandInfo_getOperandConstraint(
				&InstDesc[MCInst_getOpcode(MI)], i,
				MCOI_TIED_TO);

			if (idx == -1)
				continue;

			if (i >= MAX_MC_OPS) {
				assert(0 &&
				       "Maximum number of MC operands reached.");
			}
			MI->tied_op_idx[i] = idx;

			if (MI->flat_insn->detail)
				MI->flat_insn->detail->writeback = true;
		}
	}
}

/// Check if operand with OpNum is tied by another operand
/// (operand is tying destination).
bool MCInst_opIsTied(const MCInst *MI, unsigned OpNum)
{
	assert(OpNum < MAX_MC_OPS && "Maximum number of MC operands exceeded.");
	for (int i = 0; i < MAX_MC_OPS; ++i) {
		if (MI->tied_op_idx[i] == OpNum)
			return true;
	}
	return false;
}

/// Check if operand with OpNum is tying another operand
/// (operand is tying src).
bool MCInst_opIsTying(const MCInst *MI, unsigned OpNum)
{
	assert(OpNum < MAX_MC_OPS && "Maximum number of MC operands exceeded.");
	return MI->tied_op_idx[OpNum] != -1;
}
