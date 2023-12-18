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
class RendererGL;
enum class MultiviewImplementationTypeGL;

class ShaderGL : public ShaderImpl
{
  public:
    ShaderGL(const gl::ShaderState &state,
             GLuint shaderID,
             MultiviewImplementationTypeGL multiviewImplementationType,
             const std::shared_ptr<RendererGL> &renderer);
    ~ShaderGL() override;

    void destroy() override;

    std::shared_ptr<ShaderTranslateTask> compile(const gl::Context *context,
                                                 ShCompileOptions *options) override;

    std::string getDebugInfo() const override;

    GLuint getShaderID() const;

  private:
    GLuint mShaderID;
    MultiviewImplementationTypeGL mMultiviewImplementationType;
    std::shared_ptr<RendererGL> mRenderer;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_SHADERGL_H_
