#include <libyasm.h>
#include <libyasm/module.h>

extern yasm_arch_module yasm_dbg_LTX_objfmt;

#ifdef _MSC_VER
__declspec(dllexport)
#endif
void
yasm_init_plugin(void)
{
    yasm_register_module(YASM_MODULE_OBJFMT, "dbg", &yasm_dbg_LTX_objfmt);
}
