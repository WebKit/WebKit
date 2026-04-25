/* Capstone Disassembly Engine */
/* By Sebastian Macke <sebastian@macke.de>, 2018 */

#ifndef CS_MOS65XX_MODULE_H
#define CS_MOS65XX_MODULE_H

#include "../../utils.h"

cs_err MOS65XX_global_init(cs_struct *ud);
cs_err MOS65XX_option(cs_struct *handle, cs_opt_type type, size_t value);

#endif
