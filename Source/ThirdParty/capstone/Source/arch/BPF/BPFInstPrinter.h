/* Capstone Disassembly Engine */
/* BPF Backend by david942j <david942j@gmail.com>, 2019 */

#ifndef CS_BPFINSTPRINTER_H
#define CS_BPFINSTPRINTER_H

#include <capstone/capstone.h>

#include "../../MCInst.h"
#include "../../SStream.h"

struct SStream;

void BPF_printInst(MCInst *MI, struct SStream *O, void *Info);

#endif
