//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// CompilerD3D.h: Defines the rx::CompilerD3D class, an implementation of rx::CompilerImpl.

#ifndef LIBANGLE_RENDERER_COMPILERD3D_H_
#define LIBANGLE_RENDERER_COMPILERD3D_H_

#include "libANGLE/renderer/CompilerImpl.h"
#include "libANGLE/Caps.h"

#include "GLSLANG/ShaderLang.h"

namespace gl
{
struct Data;
}

namespace rx
{

class CompilerD3D : public CompilerImpl
{
  public:
    CompilerD3D(const gl::Data &data, ShShaderOutput outputType);
    virtual ~CompilerD3D();

    gl::Error release() override;

    ShHandle getCompilerHandle(GLenum type);

  private:
    ShShaderSpec mSpec;
    ShShaderOutput mOutputType;
    ShBuiltInResources mResources;

    ShHandle mFragmentCompiler;
    ShHandle mVertexCompiler;
};

}

#endif // LIBANGLE_RENDERER_COMPILERD3D_H_
