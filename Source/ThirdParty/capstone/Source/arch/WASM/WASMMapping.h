/* Capstone Disassembly Engine */
/* By Spike, xwings 2019 */

#include <capstone/capstone.h>

void WASM_get_insn_id(cs_struct *h, cs_insn *insn, unsigned int id);
const char *WASM_insn_name(csh handle, unsigned int id);
const char *WASM_group_name(csh handle, unsigned int id);
const char *WASM_kind_name(unsigned int id);
