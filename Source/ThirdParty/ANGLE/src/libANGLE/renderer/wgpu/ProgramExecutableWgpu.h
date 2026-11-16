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
#include "libANGLE/renderer/renderer_utils.h"
#include "libANGLE/renderer/wgpu/wgpu_pipeline_state.h"

#include <webgpu/webgpu.h>

namespace rx
{
struct TranslatedWGPUShaderModule
{
    webgpu::ShaderModuleHandle module;
};

class ProgramExecutableWgpu : public ProgramExecutableImpl
{
  public:
    ProgramExecutableWgpu(const gl::ProgramExecutable *executable);
    ~ProgramExecutableWgpu() override;

    void destroy(const gl::Context *context) override;

    angle::Result updateUniformsAndGetBindGroup(ContextWgpu *context,
                                                webgpu::BindGroupHandle *outBindGroup);
    angle::Result getSamplerAndTextureBindGroup(ContextWgpu *contextWgpu,
                                                webgpu::BindGroupHandle *outBindGroup);

    angle::Result resizeUniformBlockMemory(const gl::ShaderMap<size_t> &requiredBufferSize);

    std::shared_ptr<BufferAndLayout> &getSharedDefaultUniformBlock(gl::ShaderType shaderType)
    {
        return mDefaultUniformBlocks[shaderType];
    }

    void markDefaultUniformsDirty();
    bool checkDirtyUniforms() { return mDefaultUniformBlocksDirty.any(); }
    void markSamplerBindingsDirty() { mSamplerBindingsDirty = true; }
    bool hasDirtySamplerBindings() { return mSamplerBindingsDirty; }

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
                                    webgpu::RenderPipelineHandle *pipelineOut);

  private:
    angle::CheckedNumeric<size_t> getDefaultUniformAlignedSize(ContextWgpu *context,
                                                               gl::ShaderType shaderType) const;
    angle::CheckedNumeric<size_t> calcUniformUpdateRequiredSpace(
        ContextWgpu *context,
        gl::ShaderMap<uint64_t> *uniformOffsets) const;
    // The layout of the resource bind groups (numbering for buffers, textures, samplers) can be
    // determined once the program is linked, and should be passed in pipeline creation. Fills in
    // `mPipelineLayout` and `mDefaultBindGroupLayout` if they haven't been already.
    void genBindingLayoutIfNecessary(ContextWgpu *context);

    gl::ShaderMap<TranslatedWGPUShaderModule> mShaderModules;
    webgpu::PipelineCache mPipelineCache;
    // Holds the binding layout of resources (buffers, textures, samplers) required by the linked
    // shaders.
    webgpu::PipelineLayoutHandle mPipelineLayout;
    // Holds the binding group layout for the default bind group.
    webgpu::BindGroupLayoutHandle mDefaultBindGroupLayout;
    // Holds the most recent BindGroup. Note there may be others in the command buffer.
    webgpu::BindGroupHandle mDefaultBindGroup;

    // Holds layout info for basic GL uniforms, which needs to be laid out in a buffer for WGSL
    // similarly to a UBO.
    DefaultUniformBlockMap mDefaultUniformBlocks;
    gl::ShaderBitSet mDefaultUniformBlocksDirty;

    // Tracks when a sampler binding has been changed with glUniform1i(). Starts true to ensure the
    // bind group is created the first time around.
    bool mSamplerBindingsDirty = true;
    // Holds the binding group layout for the samplers and textures.
    webgpu::BindGroupLayoutHandle mSamplersAndTexturesBindGroupLayout;
    // Holds the most recent samplers and textures BindGroup. Note there may be others in the
    // command buffer.
    webgpu::BindGroupHandle mSamplersAndTexturesBindGroup;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_WGPU_PROGRAMEXECUTABLEWGPU_H_
