/* Capstone Disassembly Engine */
/* MOS65XX Backend by Sebastian Macke <sebastian@macke.de> 2018 */

#ifdef CAPSTONE_HAS_MOS65XX

#include "../../utils.h"
#include "../../MCRegisterInfo.h"
#include "MOS65XXDisassembler.h"
#include "MOS65XXDisassemblerInternals.h"
#include "MOS65XXModule.h"

cs_err MOS65XX_global_init(cs_struct *ud)
{
	mos65xx_info *info;

	info = cs_mem_malloc(sizeof(*info));
	info->hex_prefix = NULL;
	info->cpu_type = MOS65XX_CPU_TYPE_6502;
	info->long_m = 0;
	info->long_x = 0;


	ud->printer = MOS65XX_printInst;
	ud->printer_info = info;
	ud->insn_id = MOS65XX_get_insn_id;
	ud->insn_name = MOS65XX_insn_name;
	ud->group_name = MOS65XX_group_name;
	ud->disasm = MOS65XX_getInstruction;
	ud->reg_name = MOS65XX_reg_name;

	if (ud->mode) {
		MOS65XX_option(ud, CS_OPT_MODE, ud->mode);
	}

	return CS_ERR_OK;
}

cs_err MOS65XX_option(cs_struct *handle, cs_opt_type type, size_t value)
{
	mos65xx_info *info = (mos65xx_info *)handle->printer_info;
	switch(type) {
		default:
			break;
		case CS_OPT_MODE:

			if (value & CS_MODE_MOS65XX_6502)
				info->cpu_type = MOS65XX_CPU_TYPE_6502;
			if (value & CS_MODE_MOS65XX_65C02)
				info->cpu_type = MOS65XX_CPU_TYPE_65C02;
			if (value & CS_MODE_MOS65XX_W65C02)
				info->cpu_type = MOS65XX_CPU_TYPE_W65C02;
			if (value & (CS_MODE_MOS65XX_65816|CS_MODE_MOS65XX_65816_LONG_M|CS_MODE_MOS65XX_65816_LONG_X))
				info->cpu_type = MOS65XX_CPU_TYPE_65816;

			info->long_m = value & CS_MODE_MOS65XX_65816_LONG_M ? 1 : 0;
			info->long_x = value & CS_MODE_MOS65XX_65816_LONG_X ? 1 : 0;

			handle->mode = (cs_mode)value;
			break;
		case CS_OPT_SYNTAX:
			switch(value) {
				default:
					// wrong syntax value
					handle->errnum = CS_ERR_OPTION;
					return CS_ERR_OPTION;
				case CS_OPT_SYNTAX_DEFAULT:
					info->hex_prefix = NULL;
					break;
				case CS_OPT_SYNTAX_MOTOROLA:
					info->hex_prefix = "$";
					break;
			}
			handle->syntax = (int)value;
			break;
	}
	return CS_ERR_OK;
}

#endif
