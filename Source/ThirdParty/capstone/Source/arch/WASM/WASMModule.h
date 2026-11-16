/* Capstone Disassembly Engine */
/* By Spike, xwings 2019 */

#ifndef CS_WASM_MODULE_H
#define CS_WASM_MODULE_H

#include "../../utils.h"

cs_err WASM_global_init(cs_struct *ud);
cs_err WASM_option(cs_struct *handle, cs_opt_type type, size_t value);

#endif
