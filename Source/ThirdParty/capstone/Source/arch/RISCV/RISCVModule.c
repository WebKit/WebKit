/* Capstone Disassembly Engine */
/* RISC-V Backend By Rodrigo Cortes Porto <porto703@gmail.com> & 
   Shawn Chang <citypw@gmail.com>, HardenedLinux@2018 */

#ifdef CAPSTONE_HAS_RISCV

#include "../../utils.h"
#include "../../MCRegisterInfo.h"
#include "RISCVDisassembler.h"
#include "RISCVInstPrinter.h"
#include "RISCVMapping.h"
#include "RISCVModule.h"

cs_err RISCV_global_init(cs_struct * ud)
{
	MCRegisterInfo *mri;
	mri = cs_mem_malloc(sizeof(*mri));

	RISCV_init(mri);
	ud->printer = RISCV_printInst;
	ud->printer_info = mri;
	ud->getinsn_info = mri;
	ud->disasm = RISCV_getInstruction;
	ud->post_printer = NULL;

	ud->reg_name = RISCV_reg_name;
	ud->insn_id = RISCV_get_insn_id;
	ud->insn_name = RISCV_insn_name;
	ud->group_name = RISCV_group_name;

	return CS_ERR_OK;
}

cs_err RISCV_option(cs_struct * handle, cs_opt_type type, size_t value)
{
	if (type == CS_OPT_SYNTAX)
		handle->syntax = (int)value;

	return CS_ERR_OK;
}

#endif
