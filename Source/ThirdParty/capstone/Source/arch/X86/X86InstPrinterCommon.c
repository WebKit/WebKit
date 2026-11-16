//===--- X86InstPrinterCommon.cpp - X86 assembly instruction printing -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file includes common code for rendering MCInst instances as Intel-style
// and Intel-style assembly.
//
//===----------------------------------------------------------------------===//

/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2019 */

#if defined (WIN32) || defined (WIN64) || defined (_WIN32) || defined (_WIN64)
#pragma warning(disable:4996)			// disable MSVC's warning on strncpy()
#pragma warning(disable:28719)		// disable MSVC's warning on strncpy()
#endif

#if !defined(CAPSTONE_HAS_OSXKERNEL)
#include <ctype.h>
#endif
#include <capstone/platform.h>

#if defined(CAPSTONE_HAS_OSXKERNEL)
#include <Availability.h>
#include <libkern/libkern.h>
#else
#include <stdio.h>
#include <stdlib.h>
#endif

#include <string.h>

#include "../../utils.h"
#include "../../MCInst.h"
#include "../../SStream.h"

#include "X86InstPrinterCommon.h"
#include "X86Mapping.h"

#ifndef CAPSTONE_X86_REDUCE
void printSSEAVXCC(MCInst *MI, unsigned Op, SStream *O)
{
	uint8_t Imm = (uint8_t)(MCOperand_getImm(MCInst_getOperand(MI, Op)) & 0x1f);
	switch (Imm) {
		default: break;//printf("Invalid avxcc argument!\n"); break;
		case    0: SStream_concat0(O, "eq"); op_addAvxCC(MI, X86_AVX_CC_EQ); break;
		case    1: SStream_concat0(O, "lt"); op_addAvxCC(MI, X86_AVX_CC_LT); break;
		case    2: SStream_concat0(O, "le"); op_addAvxCC(MI, X86_AVX_CC_LE); break;
		case    3: SStream_concat0(O, "unord"); op_addAvxCC(MI, X86_AVX_CC_UNORD); break;
		case    4: SStream_concat0(O, "neq"); op_addAvxCC(MI, X86_AVX_CC_NEQ); break;
		case    5: SStream_concat0(O, "nlt"); op_addAvxCC(MI, X86_AVX_CC_NLT); break;
		case    6: SStream_concat0(O, "nle"); op_addAvxCC(MI, X86_AVX_CC_NLE); break;
		case    7: SStream_concat0(O, "ord"); op_addAvxCC(MI, X86_AVX_CC_ORD); break;
		case    8: SStream_concat0(O, "eq_uq"); op_addAvxCC(MI, X86_AVX_CC_EQ_UQ); break;
		case    9: SStream_concat0(O, "nge"); op_addAvxCC(MI, X86_AVX_CC_NGE); break;
		case  0xa: SStream_concat0(O, "ngt"); op_addAvxCC(MI, X86_AVX_CC_NGT); break;
		case  0xb: SStream_concat0(O, "false"); op_addAvxCC(MI, X86_AVX_CC_FALSE); break;
		case  0xc: SStream_concat0(O, "neq_oq"); op_addAvxCC(MI, X86_AVX_CC_NEQ_OQ); break;
		case  0xd: SStream_concat0(O, "ge"); op_addAvxCC(MI, X86_AVX_CC_GE); break;
		case  0xe: SStream_concat0(O, "gt"); op_addAvxCC(MI, X86_AVX_CC_GT); break;
		case  0xf: SStream_concat0(O, "true"); op_addAvxCC(MI, X86_AVX_CC_TRUE); break;
		case 0x10: SStream_concat0(O, "eq_os"); op_addAvxCC(MI, X86_AVX_CC_EQ_OS); break;
		case 0x11: SStream_concat0(O, "lt_oq"); op_addAvxCC(MI, X86_AVX_CC_LT_OQ); break;
		case 0x12: SStream_concat0(O, "le_oq"); op_addAvxCC(MI, X86_AVX_CC_LE_OQ); break;
		case 0x13: SStream_concat0(O, "unord_s"); op_addAvxCC(MI, X86_AVX_CC_UNORD_S); break;
		case 0x14: SStream_concat0(O, "neq_us"); op_addAvxCC(MI, X86_AVX_CC_NEQ_US); break;
		case 0x15: SStream_concat0(O, "nlt_uq"); op_addAvxCC(MI, X86_AVX_CC_NLT_UQ); break;
		case 0x16: SStream_concat0(O, "nle_uq"); op_addAvxCC(MI, X86_AVX_CC_NLE_UQ); break;
		case 0x17: SStream_concat0(O, "ord_s"); op_addAvxCC(MI, X86_AVX_CC_ORD_S); break;
		case 0x18: SStream_concat0(O, "eq_us"); op_addAvxCC(MI, X86_AVX_CC_EQ_US); break;
		case 0x19: SStream_concat0(O, "nge_uq"); op_addAvxCC(MI, X86_AVX_CC_NGE_UQ); break;
		case 0x1a: SStream_concat0(O, "ngt_uq"); op_addAvxCC(MI, X86_AVX_CC_NGT_UQ); break;
		case 0x1b: SStream_concat0(O, "false_os"); op_addAvxCC(MI, X86_AVX_CC_FALSE_OS); break;
		case 0x1c: SStream_concat0(O, "neq_os"); op_addAvxCC(MI, X86_AVX_CC_NEQ_OS); break;
		case 0x1d: SStream_concat0(O, "ge_oq"); op_addAvxCC(MI, X86_AVX_CC_GE_OQ); break;
		case 0x1e: SStream_concat0(O, "gt_oq"); op_addAvxCC(MI, X86_AVX_CC_GT_OQ); break;
		case 0x1f: SStream_concat0(O, "true_us"); op_addAvxCC(MI, X86_AVX_CC_TRUE_US); break;
	}

	MI->popcode_adjust = Imm + 1;
}

void printXOPCC(MCInst *MI, unsigned Op, SStream *O)
{
	int64_t Imm = MCOperand_getImm(MCInst_getOperand(MI, Op));

	switch (Imm) {
		default: // llvm_unreachable("Invalid xopcc argument!");
		case 0: SStream_concat0(O, "lt"); op_addXopCC(MI, X86_XOP_CC_LT); break;
		case 1: SStream_concat0(O, "le"); op_addXopCC(MI, X86_XOP_CC_LE); break;
		case 2: SStream_concat0(O, "gt"); op_addXopCC(MI, X86_XOP_CC_GT); break;
		case 3: SStream_concat0(O, "ge"); op_addXopCC(MI, X86_XOP_CC_GE); break;
		case 4: SStream_concat0(O, "eq"); op_addXopCC(MI, X86_XOP_CC_EQ); break;
		case 5: SStream_concat0(O, "neq"); op_addXopCC(MI, X86_XOP_CC_NEQ); break;
		case 6: SStream_concat0(O, "false"); op_addXopCC(MI, X86_XOP_CC_FALSE); break;
		case 7: SStream_concat0(O, "true"); op_addXopCC(MI, X86_XOP_CC_TRUE); break;
	}
}

void printRoundingControl(MCInst *MI, unsigned Op, SStream *O)
{
	int64_t Imm = MCOperand_getImm(MCInst_getOperand(MI, Op)) & 0x3;
	switch (Imm) {
		case 0: SStream_concat0(O, "{rn-sae}"); op_addAvxSae(MI); op_addAvxRoundingMode(MI, X86_AVX_RM_RN); break;
		case 1: SStream_concat0(O, "{rd-sae}"); op_addAvxSae(MI); op_addAvxRoundingMode(MI, X86_AVX_RM_RD); break;
		case 2: SStream_concat0(O, "{ru-sae}"); op_addAvxSae(MI); op_addAvxRoundingMode(MI, X86_AVX_RM_RU); break;
		case 3: SStream_concat0(O, "{rz-sae}"); op_addAvxSae(MI); op_addAvxRoundingMode(MI, X86_AVX_RM_RZ); break;
		default: break;	// never reach
	}
}
#endif
