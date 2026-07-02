/* Capstone Disassembly Engine */
/* MOS65XX Backend by Sebastian Macke <sebastian@macke.de> 2018 */

#ifndef CAPSTONE_MOS65XXDISASSEMBLER_H
#define CAPSTONE_MOS65XXDISASSEMBLER_H

#include "../../utils.h"

void MOS65XX_printInst(MCInst *MI, struct SStream *O, void *PrinterInfo);

void MOS65XX_get_insn_id(cs_struct *h, cs_insn *insn, unsigned int id);

const char *MOS65XX_insn_name(csh handle, unsigned int id);

const char *MOS65XX_group_name(csh handle, unsigned int id);

const char* MOS65XX_reg_name(csh handle, unsigned int reg);

bool MOS65XX_getInstruction(csh ud, const uint8_t *code, size_t code_len,
                            MCInst *MI, uint16_t *size, uint64_t address, void *inst_info);

#endif //CAPSTONE_MOS65XXDISASSEMBLER_H
