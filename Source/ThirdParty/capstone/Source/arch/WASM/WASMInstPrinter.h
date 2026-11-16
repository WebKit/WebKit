/* Capstone Disassembly Engine */
/* By Spike, xwings 2019 */

#ifndef CS_WASMINSTPRINTER_H
#define CS_WASMINSTPRINTER_H


#include "capstone/capstone.h"
#include "../../MCInst.h"
#include "../../SStream.h"
#include "../../cs_priv.h"

struct SStream;

void WASM_printInst(MCInst *MI, struct SStream *O, void *Info);
void printOperand(MCInst *MI, unsigned OpNo, SStream *O);

#endif
