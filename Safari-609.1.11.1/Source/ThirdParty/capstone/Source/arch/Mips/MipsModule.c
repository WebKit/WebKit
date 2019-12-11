/* Capstone Disassembly Engine */
/* By Dang Hoang Vu <danghvu@gmail.com> 2013 */

#ifdef CAPSTONE_HAS_MIPS

#include "../../utils.h"
#include "../../MCRegisterInfo.h"
#include "MipsDisassembler.h"
#include "MipsInstPrinter.h"
#include "MipsMapping.h"


static cs_err init(cs_struct *ud)
{
	MCRegisterInfo *mri;

	// verify if requested mode is valid
	if (ud->mode & ~(CS_MODE_LITTLE_ENDIAN | CS_MODE_32 | CS_MODE_64 |
				CS_MODE_MICRO | CS_MODE_MIPS32R6 | CS_MODE_BIG_ENDIAN |
				CS_MODE_MIPS2 | CS_MODE_MIPS3))
		return CS_ERR_MODE;

	mri = cs_mem_malloc(sizeof(*mri));

	Mips_init(mri);
	ud->printer = Mips_printInst;
	ud->printer_info = mri;
	ud->getinsn_info = mri;
	ud->reg_name = Mips_reg_name;
	ud->insn_id = Mips_get_insn_id;
	ud->insn_name = Mips_insn_name;
	ud->group_name = Mips_group_name;

	ud->disasm = Mips_getInstruction;

	return CS_ERR_OK;
}

static cs_err option(cs_struct *handle, cs_opt_type type, size_t value)
{
	if (type == CS_OPT_MODE) {
		handle->mode = (cs_mode)value;
		handle->big_endian = ((handle->mode & CS_MODE_BIG_ENDIAN) != 0);
		return CS_ERR_OK;
	}

	return CS_ERR_OPTION;
}

void Mips_enable(void)
{
	cs_arch_init[CS_ARCH_MIPS] = init;
	cs_arch_option[CS_ARCH_MIPS] = option;

	// support this arch
	all_arch |= (1 << CS_ARCH_MIPS);
}

#endif
