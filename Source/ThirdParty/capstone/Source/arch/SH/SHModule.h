/* Capstone Disassembly Engine */
/* By Yoshinori Sato, 2022 */

#ifndef CS_SH_MODULE_H
#define CS_SH_MODULE_H

#include "../../utils.h"

cs_err SH_global_init(cs_struct *ud);
cs_err SH_option(cs_struct *handle, cs_opt_type type, size_t value);

#endif
