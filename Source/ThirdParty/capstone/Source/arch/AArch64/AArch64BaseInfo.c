//===-- AArch64BaseInfo.cpp - AArch64 Base encoding information------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file provides basic encoding and assembly information for AArch64.
//
//===----------------------------------------------------------------------===//

/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2019 */

#ifdef CAPSTONE_HAS_ARM64

#if defined (WIN32) || defined (WIN64) || defined (_WIN32) || defined (_WIN64)
#pragma warning(disable:4996)			// disable MSVC's warning on strcpy()
#pragma warning(disable:28719)		// disable MSVC's warning on strcpy()
#endif

#include "../../utils.h"

#include <stdio.h>
#include <stdlib.h>

#include "AArch64BaseInfo.h"

#include "AArch64GenSystemOperands.inc"

// return a string representing the number X
// NOTE: result must be big enough to contain the data
static void utostr(uint64_t X, bool isNeg, char *result)
{
	char Buffer[22];
	char *BufPtr = Buffer + 21;

	Buffer[21] = '\0';
	if (X == 0) *--BufPtr = '0';  // Handle special case...

	while (X) {
		*--BufPtr = X % 10 + '0';
		X /= 10;
	}

	if (isNeg) *--BufPtr = '-';   // Add negative sign...

	// suppose that result is big enough
	strncpy(result, BufPtr, sizeof(Buffer));
}

// NOTE: result must be big enough to contain the result
void AArch64SysReg_genericRegisterString(uint32_t Bits, char *result)
{
	// assert(Bits < 0x10000);
	char Op0Str[32], Op1Str[32], CRnStr[32], CRmStr[32], Op2Str[32];
	int dummy;
	uint32_t Op0 = (Bits >> 14) & 0x3;
	uint32_t Op1 = (Bits >> 11) & 0x7;
	uint32_t CRn = (Bits >> 7) & 0xf;
	uint32_t CRm = (Bits >> 3) & 0xf;
	uint32_t Op2 = Bits & 0x7;

	utostr(Op0, false, Op0Str);
	utostr(Op1, false, Op1Str);
	utostr(Op2, false, Op2Str);
	utostr(CRn, false, CRnStr);
	utostr(CRm, false, CRmStr);

	dummy = cs_snprintf(result, 128, "s%s_%s_c%s_c%s_%s",
			Op0Str, Op1Str, CRnStr, CRmStr, Op2Str);
	(void)dummy;
}

#endif
