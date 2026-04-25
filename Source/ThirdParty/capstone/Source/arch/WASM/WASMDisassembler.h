/* Capstone Disassembly Engine */
/* By Spike, xwings 2019 */

#ifndef CS_WASMDISASSEMBLER_H
#define CS_WASMDISASSEMBLER_H

#include "../../MCInst.h"

bool WASM_getInstruction(csh ud, const uint8_t *code, size_t code_len,
		MCInst *instr, uint16_t *size, uint64_t address, void *info);

#endif
