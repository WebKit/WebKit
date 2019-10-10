#include <libyasm.h>
#include <libyasm/module.h>

extern yasm_arch_module yasm_x86_LTX_arch;

#ifdef _MSC_VER
__declspec(dllexport)
#endif
void
yasm_init_plugin(void)
{
    yasm_register_module(YASM_MODULE_ARCH, "x86", &yasm_x86_LTX_arch);
}
