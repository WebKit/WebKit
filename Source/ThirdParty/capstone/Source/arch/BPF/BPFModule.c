/* Capstone Disassembly Engine */
/* BPF Backend by david942j <david942j@gmail.com>, 2019 */

#ifdef CAPSTONE_HAS_BPF

#include "BPFDisassembler.h"
#include "BPFInstPrinter.h"
#include "BPFMapping.h"
#include "BPFModule.h"

cs_err BPF_global_init(cs_struct *ud)
{
	ud->printer = BPF_printInst;
	ud->reg_name = BPF_reg_name;
	ud->insn_id = BPF_get_insn_id;
	ud->insn_name = BPF_insn_name;
	ud->group_name = BPF_group_name;
#ifndef CAPSTONE_DIET
	ud->reg_access = BPF_reg_access;
#endif
	ud->disasm = BPF_getInstruction;

	return CS_ERR_OK;
}

cs_err BPF_option(cs_struct *handle, cs_opt_type type, size_t value)
{
	if (type == CS_OPT_MODE)
		handle->mode = (cs_mode)value;

	return CS_ERR_OK;
}

#endif
