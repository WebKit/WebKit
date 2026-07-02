/* Capstone Disassembly Engine */
/* RISC-V Backend By Rodrigo Cortes Porto <porto703@gmail.com> & 
   Shawn Chang <citypw@gmail.com>, HardenedLinux@2018 */
    
#ifndef CS_RISCVDISASSEMBLER_H
#define CS_RISCVDISASSEMBLER_H

#include "../../include/capstone/capstone.h"
#include "../../MCRegisterInfo.h"
#include "../../MCInst.h"

void RISCV_init(MCRegisterInfo *MRI);

bool RISCV_getInstruction(csh ud, const uint8_t *code, size_t code_len,
		          MCInst *instr, uint16_t *size, uint64_t address,
		          void *info);

#endif
