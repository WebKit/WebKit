//===-- RISCVInstPrinter.h - Convert RISCV MCInst to asm syntax ---*- C++ -*--//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This class prints a RISCV MCInst to a .s file.
//
//===----------------------------------------------------------------------===//

#ifndef CS_RISCVINSTPRINTER_H
#define CS_RISCVINSTPRINTER_H

#include "../../MCInst.h"
#include "../../SStream.h"

void RISCV_printInst(MCInst * MI, SStream * O, void *info);

void RISCV_post_printer(csh ud, cs_insn * insn, char *insn_asm, MCInst * mci);

#endif
