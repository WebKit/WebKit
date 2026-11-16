/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2019 */

#ifndef CS_ARMDISASSEMBLER_H
#define CS_ARMDISASSEMBLER_H

#include "capstone/capstone.h"
#include "../../MCRegisterInfo.h"

void ARM_init(MCRegisterInfo *MRI);

bool ARM_getInstruction(csh handle, const uint8_t *code, size_t code_len, MCInst *instr, uint16_t *size, uint64_t address, void *info);

bool Thumb_getInstruction(csh handle, const uint8_t *code, size_t code_len, MCInst *instr, uint16_t *size, uint64_t address, void *info);

bool ARM_getFeatureBits(unsigned int mode, unsigned int feature);

#endif
