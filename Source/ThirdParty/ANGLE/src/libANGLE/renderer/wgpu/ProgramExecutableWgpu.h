//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramExecutableWgpu.h: Implementation of ProgramExecutableImpl.

#ifndef LIBANGLE_RENDERER_WGPU_PROGRAMEXECUTABLEWGPU_H_
#define LIBANGLE_RENDERER_WGPU_PROGRAMEXECUTABLEWGPU_H_

#include "libANGLE/ProgramExecutable.h"
#include "libANGLE/renderer/ProgramExecutableImpl.h"
#include "libANGLE/renderer/wgpu/wgpu_pipeline_state.h"

#include <dawn/webgpu_cpp.h>

namespace rx
{
struct TranslatedWGPUShaderModule
{
    wgpu::ShaderModule module;
};

class ProgramExecutableWgpu : public ProgramExecutableImpl
{
  public:
    ProgramExecutableWgpu(const gl::ProgramExecutable *executable);
    ~ProgramExecutableWgpu() override;

    void destroy(const gl::Context *context) override;

    void setUniform1fv(GLint location, GLsizei count, const GLfloat *v) override;
    void setUniform2fv(GLint location, GLsizei count, const GLfloat *v) override;
    void setUniform3fv(GLint location, GLsizei count, const GLfloat *v) override;
    void setUniform4fv(GLint location, GLsizei count, const GLfloat *v) override;
    void setUniform1iv(GLint location, GLsizei count, const GLint *v) override;
    void setUniform2iv(GLint location, GLsizei count, const GLint *v) override;
    void setUniform3iv(GLint location, GLsizei count, const GLint *v) override;
    void setUniform4iv(GLint location, GLsizei count, const GLint *v) override;
    void setUniform1uiv(GLint location, GLsizei count, const GLuint *v) override;
    void setUniform2uiv(GLint location, GLsizei count, const GLuint *v) override;
    void setUniform3uiv(GLint location, GLsizei count, const GLuint *v) override;
    void setUniform4uiv(GLint location, GLsizei count, const GLuint *v) override;
    void setUniformMatrix2fv(GLint location,
                             GLsizei count,
                             GLboolean transpose,
                             const GLfloat *value) override;
    void setUniformMatrix3fv(GLint location,
                             GLsizei count,
                             GLboolean transpose,
                             const GLfloat *value) override;
    void setUniformMatrix4fv(GLint location,
                             GLsizei count,
                             GLboolean transpose,
                             const GLfloat *value) override;
    void setUniformMatrix2x3fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value) override;
    void setUniformMatrix3x2fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value) override;
    void setUniformMatrix2x4fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value) override;
    void setUniformMatrix4x2fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value) override;
    void setUniformMatrix3x4fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value) override;
    void setUniformMatrix4x3fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value) override;

    void getUniformfv(const gl::Context *context, GLint location, GLfloat *params) const override;
    void getUniformiv(const gl::Context *context, GLint location, GLint *params) const override;
    void getUniformuiv(const gl::Context *context, GLint location, GLuint *params) const override;

    TranslatedWGPUShaderModule &getShaderModule(gl::ShaderType type);

    angle::Result getRenderPipeline(ContextWgpu *context,
                                    const webgpu::RenderPipelineDesc &desc,
                                    wgpu::RenderPipeline *pipelineOut);

  private:
    gl::ShaderMap<TranslatedWGPUShaderModule> mShaderModules;
    webgpu::PipelineCache mPipelineCache;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_WGPU_PROGRAMEXECUTABLEWGPU_H_
