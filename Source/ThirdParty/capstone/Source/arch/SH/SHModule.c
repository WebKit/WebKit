/* Capstone Disassembly Engine */
/* By Yoshinori Sato 2022 */

#ifdef CAPSTONE_HAS_SH

#include "../../cs_priv.h"
#include "SHDisassembler.h"
#include "SHInstPrinter.h"
#include "SHModule.h"

cs_err SH_global_init(cs_struct *ud)
{
	sh_info *info;

	info = cs_mem_malloc(sizeof(sh_info));
	if (!info) {
		return CS_ERR_MEM;
	}

	ud->printer = SH_printInst;
	ud->printer_info = info;
	ud->reg_name = SH_reg_name;
	ud->insn_id = SH_get_insn_id;
	ud->insn_name = SH_insn_name;
	ud->group_name = SH_group_name;
	ud->disasm = SH_getInstruction;
#ifndef CAPSTONE_DIET
        ud->reg_access = SH_reg_access;
#endif

	return CS_ERR_OK;
}

cs_err SH_option(cs_struct *handle, cs_opt_type type, size_t value)
{
	return CS_ERR_OK;
}

#endif
