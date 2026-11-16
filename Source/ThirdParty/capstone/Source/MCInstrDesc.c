/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2019 */

#include "MCInstrDesc.h"

/// isPredicate - Set if this is one of the operands that made up of
/// the predicate operand that controls an isPredicable() instruction.
bool MCOperandInfo_isPredicate(const MCOperandInfo *m)
{
	return m->Flags & (1 << MCOI_Predicate);
}

/// isOptionalDef - Set if this operand is a optional def.
///
bool MCOperandInfo_isOptionalDef(const MCOperandInfo *m)
{
	return m->Flags & (1 << MCOI_OptionalDef);
}

/// Checks if operand is tied to another one.
bool MCOperandInfo_isTiedToOp(const MCOperandInfo *m)
{
	if (m->Constraints & (1 << MCOI_TIED_TO))
		return true;
	return false;
}

/// Returns the value of the specified operand constraint if
/// it is present. Returns -1 if it is not present.
int MCOperandInfo_getOperandConstraint(const MCInstrDesc *InstrDesc,
				       unsigned OpNum,
				       MCOI_OperandConstraint Constraint)
{
	const MCOperandInfo OpInfo = InstrDesc->OpInfo[OpNum];
	if (OpNum < InstrDesc->NumOperands &&
	    (OpInfo.Constraints & (1 << Constraint))) {
		unsigned ValuePos = 4 + Constraint * 4;
		return (OpInfo.Constraints >> ValuePos) & 0xf;
	}
	return -1;
}