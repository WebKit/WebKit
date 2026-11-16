/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2019 */

#ifndef CS_TRICORE_LINKAGE_H
#define CS_TRICORE_LINKAGE_H

// Function defintions to call static LLVM functions.

#include "../../MCDisassembler.h"
#include "../../MCInst.h"
#include "../../MCRegisterInfo.h"
#include "../../SStream.h"
#include "capstone/capstone.h"

bool TriCore_LLVM_getInstruction(csh handle, const uint8_t *Bytes,
				 size_t ByteLen, MCInst *MI, uint16_t *Size,
				 uint64_t Address, void *Info);
const char *TriCore_LLVM_getRegisterName(unsigned RegNo);
void TriCore_LLVM_printInst(MCInst *MI, uint64_t Address, SStream *O);

#endif // CS_TRICORE_LINKAGE_H
