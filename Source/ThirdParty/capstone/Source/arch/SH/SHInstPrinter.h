/* Capstone Disassembly Engine */
/* By Yoshinori Sato, 2022 */

#ifndef CS_SHINSTPRINTER_H
#define CS_SHINSTPRINTER_H


#include "capstone/capstone.h"
#include "../../utils.h"
#include "../../MCInst.h"
#include "../../SStream.h"
#include "../../cs_priv.h"
#include "SHDisassembler.h"

struct SStream;

void SH_printInst(MCInst *MI, struct SStream *O, void *Info);
const char* SH_reg_name(csh handle, unsigned int reg);
void SH_get_insn_id(cs_struct* h, cs_insn* insn, unsigned int id);
const char* SH_insn_name(csh handle, unsigned int id);
const char *SH_group_name(csh handle, unsigned int id);

#endif
