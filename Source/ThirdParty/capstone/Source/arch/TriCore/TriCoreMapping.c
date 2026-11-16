/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2014 */

#ifdef CAPSTONE_HAS_TRICORE

#include <stdio.h> // debug
#include <string.h>
#include <assert.h>

#include "../../utils.h"
#include "../../cs_simple_types.h"

#include "TriCoreMapping.h"
#include "TriCoreLinkage.h"

#define GET_INSTRINFO_ENUM

#include "TriCoreGenInstrInfo.inc"

static const insn_map insns[] = {
	// dummy item
	{ 0,
	  0,
#ifndef CAPSTONE_DIET
	  { 0 },
	  { 0 },
	  { 0 },
	  0,
	  0
#endif
	},

#include "TriCoreGenCSMappingInsn.inc"
};

void TriCore_get_insn_id(cs_struct *h, cs_insn *insn, unsigned int id)
{
	// Not used. Information is set after disassembly.
}

#ifndef CAPSTONE_DIET
static const tricore_reg flag_regs[] = { TRICORE_REG_PSW };
#endif // CAPSTONE_DIET

static inline void check_updates_flags(MCInst *MI)
{
#ifndef CAPSTONE_DIET
	if (!MI->flat_insn->detail)
		return;
	cs_detail *detail = MI->flat_insn->detail;
	for (int i = 0; i < detail->regs_write_count; ++i) {
		if (detail->regs_write[i] == 0)
			return;
		for (int j = 0; j < ARR_SIZE(flag_regs); ++j) {
			if (detail->regs_write[i] == flag_regs[j]) {
				detail->tricore.update_flags = true;
				return;
			}
		}
	}
#endif // CAPSTONE_DIET
}

void TriCore_set_instr_map_data(MCInst *MI)
{
	map_cs_id(MI, insns, ARR_SIZE(insns));
	map_implicit_reads(MI, insns);
	map_implicit_writes(MI, insns);
	check_updates_flags(MI);
	map_groups(MI, insns);
}

#ifndef CAPSTONE_DIET

static const char * const insn_names[] = {
	NULL,

#include "TriCoreGenCSMappingInsnName.inc"
};

// special alias insn
static const name_map alias_insn_names[] = { { 0, NULL } };
#endif

const char *TriCore_insn_name(csh handle, unsigned int id)
{
#ifndef CAPSTONE_DIET
	unsigned int i;

	if (id >= TRICORE_INS_ENDING)
		return NULL;

	// handle special alias first
	for (i = 0; i < ARR_SIZE(alias_insn_names); i++) {
		if (alias_insn_names[i].id == id)
			return alias_insn_names[i].name;
	}

	return insn_names[id];
#else
	return NULL;
#endif
}

#ifndef CAPSTONE_DIET
static const name_map group_name_maps[] = {
	{ TRICORE_GRP_INVALID, NULL },
	{ TRICORE_GRP_CALL, "call" },
	{ TRICORE_GRP_JUMP, "jump" },
};
#endif

const char *TriCore_group_name(csh handle, unsigned int id)
{
#ifndef CAPSTONE_DIET
	if (id >= TRICORE_GRP_ENDING)
		return NULL;

	return group_name_maps[id].name;
#else
	return NULL;
#endif
}

#ifndef CAPSTONE_DIET
/// A LLVM<->CS Mapping entry of an operand.
typedef struct insn_op {
	uint8_t /* cs_op_type */ type;	 ///< Operand type (e.g.: reg, imm, mem)
	uint8_t /* cs_ac_type */ access; ///< The access type (read, write)
	uint8_t				 /* cs_data_type */
		dtypes[10]; ///< List of op types. Terminated by CS_DATA_TYPE_LAST
} insn_op;

///< Operands of an instruction.
typedef struct {
	insn_op ops[16]; ///< NULL terminated array of operands.
} insn_ops;

const insn_ops insn_operands[] = {
#include "TriCoreGenCSMappingInsnOp.inc"
};
#endif

void TriCore_set_access(MCInst *MI)
{
#ifndef CAPSTONE_DIET
	if (!(MI->csh->detail == CS_OPT_ON && MI->flat_insn->detail))
		return;

	assert(MI->Opcode < ARR_SIZE(insn_operands));

	cs_detail *detail = MI->flat_insn->detail;
	cs_tricore *tc = &(detail->tricore);
	for (int i = 0; i < tc->op_count; ++i) {
		cs_ac_type ac = map_get_op_access(MI, i);
		cs_tricore_op *op = &tc->operands[i];
		op->access = ac;
		cs_op_type op_type = map_get_op_type(MI, i);
		if (op_type != CS_OP_REG) {
			continue;
		}
		if (ac & CS_AC_READ) {
			detail->regs_read[detail->regs_read_count++] = op->reg;
		}
		if (ac & CS_AC_WRITE) {
			detail->regs_write[detail->regs_write_count++] =
				op->reg;
		}
	}
#endif
}

void TriCore_reg_access(const cs_insn *insn, cs_regs regs_read,
			uint8_t *regs_read_count, cs_regs regs_write,
			uint8_t *regs_write_count)
{
#ifndef CAPSTONE_DIET
	uint8_t read_count, write_count;
	cs_detail *detail = insn->detail;
	read_count = detail->regs_read_count;
	write_count = detail->regs_write_count;

	// implicit registers
	memcpy(regs_read, detail->regs_read,
	       read_count * sizeof(detail->regs_read[0]));
	memcpy(regs_write, detail->regs_write,
	       write_count * sizeof(detail->regs_write[0]));

	// explicit registers
	cs_tricore *tc = &detail->tricore;
	for (uint8_t i = 0; i < tc->op_count; i++) {
		cs_tricore_op *op = &(tc->operands[i]);
		switch ((int)op->type) {
		case TRICORE_OP_REG:
			if ((op->access & CS_AC_READ) &&
			    !arr_exist(regs_read, read_count, op->reg)) {
				regs_read[read_count] = (uint16_t)op->reg;
				read_count++;
			}
			if ((op->access & CS_AC_WRITE) &&
			    !arr_exist(regs_write, write_count, op->reg)) {
				regs_write[write_count] = (uint16_t)op->reg;
				write_count++;
			}
			break;
		case TRICORE_OP_MEM:
			// registers appeared in memory references always being read
			if ((op->mem.base != ARM_REG_INVALID) &&
			    !arr_exist(regs_read, read_count, op->mem.base)) {
				regs_read[read_count] = (uint16_t)op->mem.base;
				read_count++;
			}
		default:
			break;
		}
	}

	*regs_read_count = read_count;
	*regs_write_count = write_count;
#endif
}

bool TriCore_getInstruction(csh handle, const uint8_t *Bytes, size_t ByteLen,
			    MCInst *MI, uint16_t *Size, uint64_t Address,
			    void *Info)
{
	return TriCore_LLVM_getInstruction(handle, Bytes, ByteLen, MI, Size,
					   Address, Info);
}

void TriCore_printInst(MCInst *MI, SStream *O, void *Info)
{
	TriCore_LLVM_printInst(MI, MI->address, O);
}

const char *TriCore_getRegisterName(csh handle, unsigned int RegNo)
{
	return TriCore_LLVM_getRegisterName(RegNo);
}

#endif // CAPSTONE_HAS_TRICORE
