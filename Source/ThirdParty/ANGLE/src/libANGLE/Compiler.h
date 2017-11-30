//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Compiler.h: Defines the gl::Compiler class, abstracting the ESSL compiler
// that a GL context holds.

#ifndef LIBANGLE_COMPILER_H_
#define LIBANGLE_COMPILER_H_

#include "GLSLANG/ShaderLang.h"
#include "libANGLE/Error.h"
#include "libANGLE/RefCountObject.h"

namespace rx
{
class CompilerImpl;
class GLImplFactory;
}

namespace gl
{
class ContextState;

class Compiler final : public RefCountObjectNoID
{
  public:
    Compiler(rx::GLImplFactory *implFactory, const ContextState &data);

    ShHandle getCompilerHandle(GLenum type);
    ShShaderOutput getShaderOutputType() const { return mOutputType; }
    const std::string &getBuiltinResourcesString(GLenum type);

  private:
    ~Compiler() override;
    std::unique_ptr<rx::CompilerImpl> mImplementation;
    ShShaderSpec mSpec;
    ShShaderOutput mOutputType;
    ShBuiltInResources mResources;

    ShHandle mFragmentCompiler;
    ShHandle mVertexCompiler;
    ShHandle mComputeCompiler;
    ShHandle mGeometryCompiler;
};

}  // namespace gl

#endif // LIBANGLE_COMPILER_H_
