/* Capstone Disassembly Engine */
/* BPF Backend by david942j <david942j@gmail.com>, 2019 */

#ifndef CS_BPF_MODULE_H
#define CS_BPF_MODULE_H

#include "../../utils.h"

cs_err BPF_global_init(cs_struct *ud);
cs_err BPF_option(cs_struct *handle, cs_opt_type type, size_t value);

#endif
