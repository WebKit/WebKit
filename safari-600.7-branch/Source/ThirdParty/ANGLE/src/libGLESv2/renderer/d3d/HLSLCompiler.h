#ifndef LIBGLESV2_RENDERER_HLSL_D3DCOMPILER_H_
#define LIBGLESV2_RENDERER_HLSL_D3DCOMPILER_H_

#include "common/angleutils.h"

#include <windows.h>

namespace gl
{
class InfoLog;
}

namespace rx
{

typedef void* ShaderBlob;
typedef void(*CompileFuncPtr)();

class HLSLCompiler
{
  public:
    HLSLCompiler();
    ~HLSLCompiler();

    bool initialize();
    void release();

    ShaderBlob *compileToBinary(gl::InfoLog &infoLog, const char *hlsl, const char *profile,
                                unsigned int optimizationFlags, bool alternateFlags) const;

  private:
    DISALLOW_COPY_AND_ASSIGN(HLSLCompiler);

    HMODULE mD3DCompilerModule;
    CompileFuncPtr mD3DCompileFunc;
};

}

#endif // LIBGLESV2_RENDERER_HLSL_D3DCOMPILER_H_
