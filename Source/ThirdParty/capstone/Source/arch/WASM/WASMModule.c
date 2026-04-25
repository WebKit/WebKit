/* Capstone Disassembly Engine */
/* By Spike, xwings 2019 */

#ifdef CAPSTONE_HAS_WASM

#include "../../cs_priv.h"
#include "WASMDisassembler.h"
#include "WASMInstPrinter.h"
#include "WASMMapping.h"
#include "WASMModule.h"

cs_err WASM_global_init(cs_struct *ud)
{
	// verify if requested mode is valid
	if (ud->mode)
		return CS_ERR_MODE;

	ud->printer = WASM_printInst;
	ud->printer_info = NULL;
	ud->insn_id = WASM_get_insn_id;
	ud->insn_name = WASM_insn_name;
	ud->group_name = WASM_group_name;
	ud->disasm = WASM_getInstruction;

	return CS_ERR_OK;
}

cs_err WASM_option(cs_struct *handle, cs_opt_type type, size_t value)
{
	return CS_ERR_OPTION;
}

#endif
