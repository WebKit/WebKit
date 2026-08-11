//===-- SystemZMCTargetDesc.cpp - SystemZ target descriptions -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2015 */

#ifdef CAPSTONE_HAS_SYSZ

#include <capstone/platform.h>
#include "SystemZMCTargetDesc.h"

#define GET_REGINFO_ENUM
#include "SystemZGenRegisterInfo.inc"

const unsigned SystemZMC_GR32Regs[16] = {
	SystemZ_R0L, SystemZ_R1L, SystemZ_R2L, SystemZ_R3L,
	SystemZ_R4L, SystemZ_R5L, SystemZ_R6L, SystemZ_R7L,
	SystemZ_R8L, SystemZ_R9L, SystemZ_R10L, SystemZ_R11L,
	SystemZ_R12L, SystemZ_R13L, SystemZ_R14L, SystemZ_R15L
};

const unsigned SystemZMC_GRH32Regs[16] = {
	SystemZ_R0H, SystemZ_R1H, SystemZ_R2H, SystemZ_R3H,
	SystemZ_R4H, SystemZ_R5H, SystemZ_R6H, SystemZ_R7H,
	SystemZ_R8H, SystemZ_R9H, SystemZ_R10H, SystemZ_R11H,
	SystemZ_R12H, SystemZ_R13H, SystemZ_R14H, SystemZ_R15H
};

const unsigned SystemZMC_GR64Regs[16] = {
	SystemZ_R0D, SystemZ_R1D, SystemZ_R2D, SystemZ_R3D,
	SystemZ_R4D, SystemZ_R5D, SystemZ_R6D, SystemZ_R7D,
	SystemZ_R8D, SystemZ_R9D, SystemZ_R10D, SystemZ_R11D,
	SystemZ_R12D, SystemZ_R13D, SystemZ_R14D, SystemZ_R15D
};

const unsigned SystemZMC_GR128Regs[16] = {
	SystemZ_R0Q, 0, SystemZ_R2Q, 0,
	SystemZ_R4Q, 0, SystemZ_R6Q, 0,
	SystemZ_R8Q, 0, SystemZ_R10Q, 0,
	SystemZ_R12Q, 0, SystemZ_R14Q, 0
};

const unsigned SystemZMC_FP32Regs[16] = {
	SystemZ_F0S, SystemZ_F1S, SystemZ_F2S, SystemZ_F3S,
	SystemZ_F4S, SystemZ_F5S, SystemZ_F6S, SystemZ_F7S,
	SystemZ_F8S, SystemZ_F9S, SystemZ_F10S, SystemZ_F11S,
	SystemZ_F12S, SystemZ_F13S, SystemZ_F14S, SystemZ_F15S
};

const unsigned SystemZMC_FP64Regs[16] = {
	SystemZ_F0D, SystemZ_F1D, SystemZ_F2D, SystemZ_F3D,
	SystemZ_F4D, SystemZ_F5D, SystemZ_F6D, SystemZ_F7D,
	SystemZ_F8D, SystemZ_F9D, SystemZ_F10D, SystemZ_F11D,
	SystemZ_F12D, SystemZ_F13D, SystemZ_F14D, SystemZ_F15D
};

const unsigned SystemZMC_FP128Regs[16] = {
	SystemZ_F0Q, SystemZ_F1Q, 0, 0,
	SystemZ_F4Q, SystemZ_F5Q, 0, 0,
	SystemZ_F8Q, SystemZ_F9Q, 0, 0,
	SystemZ_F12Q, SystemZ_F13Q, 0, 0
};

const unsigned SystemZMC_VR32Regs[32] = {
  SystemZ_F0S, SystemZ_F1S, SystemZ_F2S, SystemZ_F3S,
  SystemZ_F4S, SystemZ_F5S, SystemZ_F6S, SystemZ_F7S,
  SystemZ_F8S, SystemZ_F9S, SystemZ_F10S, SystemZ_F11S,
  SystemZ_F12S, SystemZ_F13S, SystemZ_F14S, SystemZ_F15S,
  SystemZ_F16S, SystemZ_F17S, SystemZ_F18S, SystemZ_F19S,
  SystemZ_F20S, SystemZ_F21S, SystemZ_F22S, SystemZ_F23S,
  SystemZ_F24S, SystemZ_F25S, SystemZ_F26S, SystemZ_F27S,
  SystemZ_F28S, SystemZ_F29S, SystemZ_F30S, SystemZ_F31S
};

const unsigned SystemZMC_VR64Regs[32] = {
  SystemZ_F0D, SystemZ_F1D, SystemZ_F2D, SystemZ_F3D,
  SystemZ_F4D, SystemZ_F5D, SystemZ_F6D, SystemZ_F7D,
  SystemZ_F8D, SystemZ_F9D, SystemZ_F10D, SystemZ_F11D,
  SystemZ_F12D, SystemZ_F13D, SystemZ_F14D, SystemZ_F15D,
  SystemZ_F16D, SystemZ_F17D, SystemZ_F18D, SystemZ_F19D,
  SystemZ_F20D, SystemZ_F21D, SystemZ_F22D, SystemZ_F23D,
  SystemZ_F24D, SystemZ_F25D, SystemZ_F26D, SystemZ_F27D,
  SystemZ_F28D, SystemZ_F29D, SystemZ_F30D, SystemZ_F31D
};

const unsigned SystemZMC_VR128Regs[32] = {
  SystemZ_V0, SystemZ_V1, SystemZ_V2, SystemZ_V3,
  SystemZ_V4, SystemZ_V5, SystemZ_V6, SystemZ_V7,
  SystemZ_V8, SystemZ_V9, SystemZ_V10, SystemZ_V11,
  SystemZ_V12, SystemZ_V13, SystemZ_V14, SystemZ_V15,
  SystemZ_V16, SystemZ_V17, SystemZ_V18, SystemZ_V19,
  SystemZ_V20, SystemZ_V21, SystemZ_V22, SystemZ_V23,
  SystemZ_V24, SystemZ_V25, SystemZ_V26, SystemZ_V27,
  SystemZ_V28, SystemZ_V29, SystemZ_V30, SystemZ_V31
};

const unsigned SystemZMC_AR32Regs[16] = {
  SystemZ_A0, SystemZ_A1, SystemZ_A2, SystemZ_A3,
  SystemZ_A4, SystemZ_A5, SystemZ_A6, SystemZ_A7,
  SystemZ_A8, SystemZ_A9, SystemZ_A10, SystemZ_A11,
  SystemZ_A12, SystemZ_A13, SystemZ_A14, SystemZ_A15
};

const unsigned SystemZMC_CR64Regs[16] = {
  SystemZ_C0, SystemZ_C1, SystemZ_C2, SystemZ_C3,
  SystemZ_C4, SystemZ_C5, SystemZ_C6, SystemZ_C7,
  SystemZ_C8, SystemZ_C9, SystemZ_C10, SystemZ_C11,
  SystemZ_C12, SystemZ_C13, SystemZ_C14, SystemZ_C15
};

/* All register classes that have 0-15.  */
#define DEF_REG16(N) \
    [SystemZ_R ## N ## L] = N, \
    [SystemZ_R ## N ## H] = N, \
    [SystemZ_R ## N ## D] = N, \
    [SystemZ_F ## N ## S] = N, \
    [SystemZ_F ## N ## D] = N, \
    [SystemZ_V ## N] = N, \
    [SystemZ_A ## N] = N, \
    [SystemZ_C ## N] = N

/* All register classes that (also) have 16-31.  */
#define DEF_REG32(N) \
    [SystemZ_F ## N ## S] = N, \
    [SystemZ_F ## N ## D] = N, \
    [SystemZ_V ## N] = N

static const uint8_t Map[SystemZ_NUM_TARGET_REGS] = {
    DEF_REG16(0),
    DEF_REG16(1),
    DEF_REG16(2),
    DEF_REG16(3),
    DEF_REG16(4),
    DEF_REG16(5),
    DEF_REG16(6),
    DEF_REG16(8),
    DEF_REG16(9),
    DEF_REG16(10),
    DEF_REG16(11),
    DEF_REG16(12),
    DEF_REG16(13),
    DEF_REG16(14),
    DEF_REG16(15),

    DEF_REG32(16),
    DEF_REG32(17),
    DEF_REG32(18),
    DEF_REG32(19),
    DEF_REG32(20),
    DEF_REG32(21),
    DEF_REG32(22),
    DEF_REG32(23),
    DEF_REG32(24),
    DEF_REG32(25),
    DEF_REG32(26),
    DEF_REG32(27),
    DEF_REG32(28),
    DEF_REG32(29),
    DEF_REG32(30),
    DEF_REG32(31),

    /* The float Q registers are non-sequential.  */
    [SystemZ_F0Q] = 0,
    [SystemZ_F1Q] = 1,
    [SystemZ_F4Q] = 4,
    [SystemZ_F5Q] = 5,
    [SystemZ_F8Q] = 8,
    [SystemZ_F9Q] = 9,
    [SystemZ_F12Q] = 12,
    [SystemZ_F13Q] = 13,

    /* The integer Q registers are all even.  */
    [SystemZ_R0Q] = 0,
    [SystemZ_R2Q] = 2,
    [SystemZ_R4Q] = 4,
    [SystemZ_R6Q] = 6,
    [SystemZ_R8Q] = 8,
    [SystemZ_R10Q] = 10,
    [SystemZ_R12Q] = 12,
    [SystemZ_R14Q] = 14,
};

unsigned SystemZMC_getFirstReg(unsigned Reg)
{
	// assert(Reg < SystemZ_NUM_TARGET_REGS);
	return Map[Reg];
}

#endif
