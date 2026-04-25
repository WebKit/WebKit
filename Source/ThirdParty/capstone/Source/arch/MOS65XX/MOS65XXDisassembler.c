/* Capstone Disassembly Engine */
/* MOS65XX Backend by Sebastian Macke <sebastian@macke.de> 2018 */

#include "capstone/mos65xx.h"
#include "MOS65XXDisassembler.h"
#include "MOS65XXDisassemblerInternals.h"

typedef struct OpInfo {
	mos65xx_insn ins;
	mos65xx_address_mode am;
	int operand_bytes;
} OpInfo;

static const struct OpInfo OpInfoTable[]= {

#include "m6502.inc"
#include "m65c02.inc"
#include "mw65c02.inc"
#include "m65816.inc"

};

static const char* const RegNames[] = {
	"invalid", "A", "X", "Y", "P", "SP", "DP", "B", "K" 
};

#ifndef CAPSTONE_DIET
static const char* const GroupNames[] = {
	NULL,
	"jump",
	"call",
	"ret",
	"int",
	"iret",
	"branch_relative"
};

typedef struct InstructionInfo {
	const char* name;
	mos65xx_group_type group_type;
	mos65xx_reg write, read;
	bool modifies_status;
} InstructionInfo;

static const struct InstructionInfo InstructionInfoTable[]= {

#include "instruction_info.inc"

};
#endif

#ifndef CAPSTONE_DIET
static void fillDetails(MCInst *MI, struct OpInfo opinfo, int cpu_type)
{
	int i;
	cs_detail *detail = MI->flat_insn->detail;

	InstructionInfo insinfo = InstructionInfoTable[opinfo.ins];

	detail->mos65xx.am = opinfo.am;
	detail->mos65xx.modifies_flags = insinfo.modifies_status;
	detail->groups_count = 0;
	detail->regs_read_count = 0;
	detail->regs_write_count = 0;
	detail->mos65xx.op_count = 0;

	if (insinfo.group_type != MOS65XX_GRP_INVALID) {
		detail->groups[detail->groups_count] = insinfo.group_type;
		detail->groups_count++;
	}

	if (opinfo.am == MOS65XX_AM_REL || opinfo.am == MOS65XX_AM_ZP_REL) {
		detail->groups[detail->groups_count] = MOS65XX_GRP_BRANCH_RELATIVE;
		detail->groups_count++;	
	}

	if (insinfo.read != MOS65XX_REG_INVALID) {
		detail->regs_read[detail->regs_read_count++] = insinfo.read;
	} else switch(opinfo.am) {
		case MOS65XX_AM_ACC:
			detail->regs_read[detail->regs_read_count++] = MOS65XX_REG_ACC;
			break;
		case MOS65XX_AM_ZP_Y:
		case MOS65XX_AM_ZP_IND_Y:
		case MOS65XX_AM_ABS_Y:
		case MOS65XX_AM_ZP_IND_LONG_Y:
			detail->regs_read[detail->regs_read_count++] = MOS65XX_REG_Y;
			break;

		case MOS65XX_AM_ZP_X:
		case MOS65XX_AM_ZP_X_IND:
		case MOS65XX_AM_ABS_X:
		case MOS65XX_AM_ABS_X_IND:
		case MOS65XX_AM_ABS_LONG_X:
			detail->regs_read[detail->regs_read_count++] = MOS65XX_REG_X;
			break;

		case MOS65XX_AM_SR:
			detail->regs_read[detail->regs_read_count++] = MOS65XX_REG_SP;
			break;
		case MOS65XX_AM_SR_IND_Y:
			detail->regs_read[detail->regs_read_count++] = MOS65XX_REG_SP;
			detail->regs_read[detail->regs_read_count++] = MOS65XX_REG_Y;
			break;

		default:
			break;
	}

	if (insinfo.write != MOS65XX_REG_INVALID) {
		detail->regs_write[detail->regs_write_count++] = insinfo.write;
	} else if (opinfo.am == MOS65XX_AM_ACC) {
		detail->regs_write[detail->regs_write_count++] = MOS65XX_REG_ACC;
	}


	switch(opinfo.ins) {
		case MOS65XX_INS_ADC:
		case MOS65XX_INS_SBC:
		case MOS65XX_INS_ROL:
		case MOS65XX_INS_ROR:
			/* these read carry flag (and decimal for ADC/SBC) */
			detail->regs_read[detail->regs_read_count++] = MOS65XX_REG_P;
			break;
		/* stack operations */
		case MOS65XX_INS_JSL:
		case MOS65XX_INS_JSR:
		case MOS65XX_INS_PEA:
		case MOS65XX_INS_PEI:
		case MOS65XX_INS_PER:
		case MOS65XX_INS_PHA:
		case MOS65XX_INS_PHB:
		case MOS65XX_INS_PHD:
		case MOS65XX_INS_PHK:
		case MOS65XX_INS_PHP:
		case MOS65XX_INS_PHX:
		case MOS65XX_INS_PHY:
		case MOS65XX_INS_PLA:
		case MOS65XX_INS_PLB:
		case MOS65XX_INS_PLD:
		case MOS65XX_INS_PLP:
		case MOS65XX_INS_PLX:
		case MOS65XX_INS_PLY:
		case MOS65XX_INS_RTI:
		case MOS65XX_INS_RTL:
		case MOS65XX_INS_RTS:
			detail->regs_read[detail->regs_read_count++] = MOS65XX_REG_SP;
			detail->regs_write[detail->regs_write_count++] = MOS65XX_REG_SP;
			break;
		default:
			break;
	}

	if (cpu_type == MOS65XX_CPU_TYPE_65816) {
		switch (opinfo.am) {
			case MOS65XX_AM_ZP:
			case MOS65XX_AM_ZP_X:
			case MOS65XX_AM_ZP_Y:
			case MOS65XX_AM_ZP_IND:
			case MOS65XX_AM_ZP_X_IND:
			case MOS65XX_AM_ZP_IND_Y:
			case MOS65XX_AM_ZP_IND_LONG:
			case MOS65XX_AM_ZP_IND_LONG_Y:
				detail->regs_read[detail->regs_read_count++] = MOS65XX_REG_DP;
				break;
			case MOS65XX_AM_BLOCK:
				detail->regs_read[detail->regs_read_count++] = MOS65XX_REG_ACC;
				detail->regs_read[detail->regs_read_count++] = MOS65XX_REG_X;
				detail->regs_read[detail->regs_read_count++] = MOS65XX_REG_Y;
				detail->regs_write[detail->regs_write_count++] = MOS65XX_REG_ACC;
				detail->regs_write[detail->regs_write_count++] = MOS65XX_REG_X;
				detail->regs_write[detail->regs_write_count++] = MOS65XX_REG_Y;
				detail->regs_write[detail->regs_write_count++] = MOS65XX_REG_B;
				break;
			default:
				break;
		}

		switch (opinfo.am) {
			case MOS65XX_AM_ZP_IND:
			case MOS65XX_AM_ZP_X_IND:
			case MOS65XX_AM_ZP_IND_Y:
			case MOS65XX_AM_ABS:
			case MOS65XX_AM_ABS_X:
			case MOS65XX_AM_ABS_Y:
			case MOS65XX_AM_ABS_X_IND:
				/* these depend on the databank to generate a 24-bit address */
				/* exceptions: PEA, PEI, and JMP (abs) */
				if (opinfo.ins == MOS65XX_INS_PEI || opinfo.ins == MOS65XX_INS_PEA) break;
				detail->regs_read[detail->regs_read_count++] = MOS65XX_REG_B;
				break;
			default:
				break;
		}
	}

	if (insinfo.modifies_status) {
		detail->regs_write[detail->regs_write_count++] = MOS65XX_REG_P;
	}

	switch(opinfo.am) {
		case MOS65XX_AM_IMP:
			break;
		case MOS65XX_AM_IMM:
			detail->mos65xx.operands[detail->mos65xx.op_count].type = MOS65XX_OP_IMM;
			detail->mos65xx.operands[detail->mos65xx.op_count].imm = MI->Operands[0].ImmVal;
			detail->mos65xx.op_count++;
			break;
		case MOS65XX_AM_ACC:
			detail->mos65xx.operands[detail->mos65xx.op_count].type = MOS65XX_OP_REG;
			detail->mos65xx.operands[detail->mos65xx.op_count].reg = MOS65XX_REG_ACC;
			detail->mos65xx.op_count++;
			break;
		case MOS65XX_AM_REL: {
			int value = MI->Operands[0].ImmVal;
			if (MI->op1_size == 1)
				value = 2 + (signed char)value;
			else
				value = 3 + (signed short)value;
			detail->mos65xx.operands[detail->mos65xx.op_count].type = MOS65XX_OP_MEM;
			detail->mos65xx.operands[detail->mos65xx.op_count].mem = (MI->address + value) & 0xffff;
			detail->mos65xx.op_count++;
			break;
		}
		case MOS65XX_AM_ZP_REL: {
			int value =	3 + (signed char)MI->Operands[1].ImmVal;
			/* BBR0, zp, rel  and BBS0, zp, rel */
			detail->mos65xx.operands[detail->mos65xx.op_count].type = MOS65XX_OP_MEM;
			detail->mos65xx.operands[detail->mos65xx.op_count].mem = MI->Operands[0].ImmVal;
			detail->mos65xx.operands[detail->mos65xx.op_count+1].type = MOS65XX_OP_MEM;
			detail->mos65xx.operands[detail->mos65xx.op_count+1].mem = (MI->address + value) & 0xffff;
			detail->mos65xx.op_count+=2;
			break;
		}
		default:
			for (i = 0; i < MI->size; ++i) {
				detail->mos65xx.operands[detail->mos65xx.op_count].type = MOS65XX_OP_MEM;
				detail->mos65xx.operands[detail->mos65xx.op_count].mem = MI->Operands[i].ImmVal;
				detail->mos65xx.op_count++;
			}
			break;
	}
}
#endif

void MOS65XX_printInst(MCInst *MI, struct SStream *O, void *PrinterInfo)
{
#ifndef CAPSTONE_DIET
	unsigned int value;
	unsigned opcode = MCInst_getOpcode(MI);
	mos65xx_info *info = (mos65xx_info *)PrinterInfo;

	OpInfo opinfo = OpInfoTable[opcode];

	const char *prefix = info->hex_prefix ? info->hex_prefix : "0x";

	SStream_concat0(O, InstructionInfoTable[opinfo.ins].name);
	switch (opinfo.ins) {
		/* special case - bit included as part of the instruction name */
		case MOS65XX_INS_BBR:
		case MOS65XX_INS_BBS:
		case MOS65XX_INS_RMB:
		case MOS65XX_INS_SMB:
			SStream_concat(O, "%d", (opcode >> 4) & 0x07);
			break;
		default:
			break;
	}

	value = MI->Operands[0].ImmVal;

	switch (opinfo.am) {
		default:
			break;

		case MOS65XX_AM_IMP:
			break;

		case MOS65XX_AM_ACC:
			SStream_concat0(O, " a");
			break;

		case MOS65XX_AM_IMM:
			if (MI->imm_size == 1)
				SStream_concat(O, " #%s%02x", prefix, value);
			else
				SStream_concat(O, " #%s%04x", prefix, value);
			break;

		case MOS65XX_AM_ZP:
			SStream_concat(O, " %s%02x", prefix, value);
			break;

		case MOS65XX_AM_ABS:
			SStream_concat(O, " %s%04x", prefix, value);
			break;

		case MOS65XX_AM_ABS_LONG_X:
			SStream_concat(O, " %s%06x, x", prefix, value);
			break;

		case MOS65XX_AM_INT:
			SStream_concat(O, " %s%02x", prefix, value);
			break;

		case MOS65XX_AM_ABS_X:
			SStream_concat(O, " %s%04x, x", prefix, value);
			break;

		case MOS65XX_AM_ABS_Y:
			SStream_concat(O, " %s%04x, y", prefix, value);
			break;

		case MOS65XX_AM_ABS_LONG:
			SStream_concat(O, " %s%06x", prefix, value);
			break;

		case MOS65XX_AM_ZP_X:
			SStream_concat(O, " %s%02x, x", prefix, value);
			break;

		case MOS65XX_AM_ZP_Y:
			SStream_concat(O, " %s%02x, y", prefix, value);
			break;

		case MOS65XX_AM_REL:
			if (MI->op1_size == 1)
				value = 2 + (signed char)value;
			else
				value = 3 + (signed short)value;

			SStream_concat(O, " %s%04x", prefix, 
				(MI->address + value) & 0xffff);
			break;

		case MOS65XX_AM_ABS_IND:
			SStream_concat(O, " (%s%04x)", prefix, value);
			break;

		case MOS65XX_AM_ABS_X_IND:
			SStream_concat(O, " (%s%04x, x)", prefix, value);
			break;

		case MOS65XX_AM_ABS_IND_LONG:
			SStream_concat(O, " [%s%04x]", prefix, value);
			break;

		case MOS65XX_AM_ZP_IND:
			SStream_concat(O, " (%s%02x)", prefix, value);
			break;

		case MOS65XX_AM_ZP_X_IND:
			SStream_concat(O, " (%s%02x, x)", prefix, value);
			break;

		case MOS65XX_AM_ZP_IND_Y:
			SStream_concat(O, " (%s%02x), y", prefix, value);
			break;

		case MOS65XX_AM_ZP_IND_LONG:
			SStream_concat(O, " [%s%02x]", prefix, value);
			break;

		case MOS65XX_AM_ZP_IND_LONG_Y:
			SStream_concat(O, " [%s%02x], y", prefix, value);
			break;

		case MOS65XX_AM_SR:
			SStream_concat(O, " %s%02x, s", prefix, value);
			break;

		case MOS65XX_AM_SR_IND_Y:
			SStream_concat(O, " (%s%02x, s), y", prefix, value);
			break;

		case MOS65XX_AM_BLOCK:
			SStream_concat(O, " %s%02x, %s%02x",
				prefix, MI->Operands[0].ImmVal,
				prefix, MI->Operands[1].ImmVal);
			break;

		case MOS65XX_AM_ZP_REL:
			value =	3 + (signed char)MI->Operands[1].ImmVal;
			/* BBR0, zp, rel  and BBS0, zp, rel */
			SStream_concat(O, " %s%02x, %s%04x",
				prefix, MI->Operands[0].ImmVal,
				prefix, (MI->address + value) & 0xffff);
			break;

	}
#endif
}

bool MOS65XX_getInstruction(csh ud, const uint8_t *code, size_t code_len,
							MCInst *MI, uint16_t *size, uint64_t address, void *inst_info)
{
	int i;
	unsigned char opcode;
	unsigned char len;
	unsigned cpu_offset = 0;
	int cpu_type = MOS65XX_CPU_TYPE_6502;
	cs_struct* handle = MI->csh;
	mos65xx_info *info = (mos65xx_info *)handle->printer_info;
	OpInfo opinfo;

	if (code_len == 0) {
		*size = 1;
		return false;
	}

	cpu_type = info->cpu_type;
	cpu_offset = cpu_type * 256;

	opcode = code[0];
	opinfo = OpInfoTable[cpu_offset + opcode];
	if (opinfo.ins == MOS65XX_INS_INVALID) {
		*size = 1;
		return false;
	}

	len = opinfo.operand_bytes + 1;

	if (cpu_type == MOS65XX_CPU_TYPE_65816 && opinfo.am == MOS65XX_AM_IMM) {
		switch(opinfo.ins) {
			case MOS65XX_INS_CPX:
			case MOS65XX_INS_CPY:
			case MOS65XX_INS_LDX:
			case MOS65XX_INS_LDY:
				if (info->long_x) ++len;
				break;
			case MOS65XX_INS_ADC:
			case MOS65XX_INS_AND:
			case MOS65XX_INS_BIT:
			case MOS65XX_INS_CMP:
			case MOS65XX_INS_EOR:
			case MOS65XX_INS_LDA:
			case MOS65XX_INS_ORA:
			case MOS65XX_INS_SBC:
				if (info->long_m) ++len;
				break;
			default:
				break;
		}
	}

	if (code_len < len) {
		*size = 1;
		return false;
	}

	MI->address = address;

	MCInst_setOpcode(MI, cpu_offset + opcode);
	MCInst_setOpcodePub(MI, opinfo.ins);

	*size = len;

	/* needed to differentiate relative vs relative long */
	MI->op1_size = len - 1;
	if (opinfo.ins == MOS65XX_INS_NOP) {
		for (i = 1; i < len; ++i)
			MCOperand_CreateImm0(MI, code[i]);
	}

	switch (opinfo.am) {
		case MOS65XX_AM_ZP_REL:
			MCOperand_CreateImm0(MI, code[1]);
			MCOperand_CreateImm0(MI, code[2]);
			break;
		case MOS65XX_AM_BLOCK:
			MCOperand_CreateImm0(MI, code[2]);
			MCOperand_CreateImm0(MI, code[1]);
			break;
		case MOS65XX_AM_IMP:
		case MOS65XX_AM_ACC:
			break;

		case MOS65XX_AM_IMM:
			MI->has_imm = 1;
			MI->imm_size = len - 1;
			/* 65816 immediate is either 1 or 2 bytes */
			/* drop through */
		default:
			if (len == 2)
				MCOperand_CreateImm0(MI, code[1]);
			else if (len == 3)
				MCOperand_CreateImm0(MI, (code[2]<<8) | code[1]);
			else if (len == 4)
				MCOperand_CreateImm0(MI, (code[3]<<16) | (code[2]<<8) | code[1]);
			break;
	}

#ifndef CAPSTONE_DIET
	if (MI->flat_insn->detail) {
		fillDetails(MI, opinfo, cpu_type);
	}
#endif

	return true;
}

const char *MOS65XX_insn_name(csh handle, unsigned int id)
{
#ifdef CAPSTONE_DIET
	return NULL;
#else
	if (id >= ARR_SIZE(InstructionInfoTable)) {
		return NULL;
	}
	return InstructionInfoTable[id].name;
#endif
}

const char* MOS65XX_reg_name(csh handle, unsigned int reg)
{
#ifdef CAPSTONE_DIET
	return NULL;
#else
	if (reg >= ARR_SIZE(RegNames)) {
		return NULL;
	}
	return RegNames[(int)reg];
#endif
}

void MOS65XX_get_insn_id(cs_struct *h, cs_insn *insn, unsigned int id)
{
	/* id is cpu_offset + opcode */
	if (id < ARR_SIZE(OpInfoTable)) {
		insn->id = OpInfoTable[id].ins;
	}
}

const char *MOS65XX_group_name(csh handle, unsigned int id)
{
#ifdef CAPSTONE_DIET
	return NULL;
#else
	if (id >= ARR_SIZE(GroupNames)) {
		return NULL;
	}
	return GroupNames[(int)id];
#endif
}
