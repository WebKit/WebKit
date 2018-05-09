/* Capstone Disassembly Engine */
/* By Dang Hoang Vu <danghvu@gmail.com> 2013 */

#ifdef CAPSTONE_HAS_ARM

#include "../../cs_priv.h"
#include "../../MCRegisterInfo.h"
#include "ARMDisassembler.h"
#include "ARMInstPrinter.h"
#include "ARMMapping.h"

static cs_err init(cs_struct *ud)
{
	MCRegisterInfo *mri;

	// verify if requested mode is valid
	if (ud->mode & ~(CS_MODE_LITTLE_ENDIAN | CS_MODE_ARM | CS_MODE_V8 |
				CS_MODE_MCLASS | CS_MODE_THUMB | CS_MODE_BIG_ENDIAN))
		return CS_ERR_MODE;

	mri = cs_mem_malloc(sizeof(*mri));

	ARM_init(mri);
	ARM_getRegName(ud, 0);	// use default get_regname

	ud->printer = ARM_printInst;
	ud->printer_info = mri;
	ud->reg_name = ARM_reg_name;
	ud->insn_id = ARM_get_insn_id;
	ud->insn_name = ARM_insn_name;
	ud->group_name = ARM_group_name;
	ud->post_printer = ARM_post_printer;
#ifndef CAPSTONE_DIET
	ud->reg_access = ARM_reg_access;
#endif

	if (ud->mode & CS_MODE_THUMB)
		ud->disasm = Thumb_getInstruction;
	else
		ud->disasm = ARM_getInstruction;

	return CS_ERR_OK;
}

static cs_err option(cs_struct *handle, cs_opt_type type, size_t value)
{
	switch(type) {
		case CS_OPT_MODE:
			if (value & CS_MODE_THUMB)
				handle->disasm = Thumb_getInstruction;
			else
				handle->disasm = ARM_getInstruction;

			handle->mode = (cs_mode)value;
			handle->big_endian = ((handle->mode & CS_MODE_BIG_ENDIAN) != 0);

			break;
		case CS_OPT_SYNTAX:
			ARM_getRegName(handle, (int)value);
			handle->syntax = (int)value;
			break;
		default:
			break;
	}

	return CS_ERR_OK;
}

void ARM_enable(void)
{
	cs_arch_init[CS_ARCH_ARM] = init;
	cs_arch_option[CS_ARCH_ARM] = option;

	// support this arch
	all_arch |= (1 << CS_ARCH_ARM);
}

#endif
