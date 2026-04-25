/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2014 */

#ifndef CS_TRICOREDISASSEMBLER_H
#define CS_TRICOREDISASSEMBLER_H

#if !defined(_MSC_VER) || !defined(_KERNEL_MODE)
#include <stdint.h>
#endif

#include <capstone/capstone.h>
#include "../../MCRegisterInfo.h"
#include "../../MCInst.h"

void TriCore_init_mri(MCRegisterInfo *MRI);
bool TriCore_getFeatureBits(unsigned int mode, unsigned int feature);

#endif
