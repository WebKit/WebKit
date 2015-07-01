//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ShaderGL.h: Defines the class interface for ShaderGL.

#ifndef LIBANGLE_RENDERER_GL_SHADERGL_H_
#define LIBANGLE_RENDERER_GL_SHADERGL_H_

#include "libANGLE/renderer/ShaderImpl.h"

namespace rx
{

class FunctionsGL;

class ShaderGL : public ShaderImpl
{
  public:
    ShaderGL(GLenum type, const FunctionsGL *functions);
    ~ShaderGL() override;

    bool compile(gl::Compiler *compiler, const std::string &source) override;
    std::string getDebugInfo() const override;

    GLuint getShaderID() const;

  private:
    const FunctionsGL *mFunctions;

    GLenum mType;
    GLuint mShaderID;
};

}

#endif // LIBANGLE_RENDERER_GL_SHADERGL_H_
