
//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramMtl.h:
//    Defines the class interface for ProgramMtl, implementing ProgramImpl.
//

#ifndef LIBANGLE_RENDERER_METAL_PROGRAMMTL_H_
#define LIBANGLE_RENDERER_METAL_PROGRAMMTL_H_

#import <Metal/Metal.h>

#include <array>

#include "common/Optional.h"
#include "common/utilities.h"
#include "libANGLE/renderer/ProgramImpl.h"
#include "libANGLE/renderer/metal/ProgramExecutableMtl.h"
#include "libANGLE/renderer/metal/ShaderMtl.h"
#include "libANGLE/renderer/metal/mtl_command_buffer.h"
#include "libANGLE/renderer/metal/mtl_context_device.h"

namespace rx
{
#define SHADER_ENTRY_NAME @"main0"
class ContextMtl;

class ProgramMtl : public ProgramImpl
{
  public:
    ProgramMtl(const gl::ProgramState &state);
    ~ProgramMtl() override;

    void destroy(const gl::Context *context) override;

    std::unique_ptr<LinkEvent> load(const gl::Context *context,
                                    gl::BinaryInputStream *stream) override;
    void save(const gl::Context *context, gl::BinaryOutputStream *stream) override;
    void setBinaryRetrievableHint(bool retrievable) override;
    void setSeparable(bool separable) override;

    void prepareForLink(const gl::ShaderMap<ShaderImpl *> &shaders) override;
    std::unique_ptr<LinkEvent> link(const gl::Context *context,
                                    const gl::ProgramLinkedResources &resources,
                                    gl::ProgramMergedVaryings &&mergedVaryings) override;
    GLboolean validate(const gl::Caps &caps) override;

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

    angle::Result getSpecializedShader(ContextMtl *context,
                                       gl::ShaderType shaderType,
                                       const mtl::RenderPipelineDesc &renderPipelineDesc,
                                       id<MTLFunction> *shaderOut);

    // Calls this before drawing, changedPipelineDesc is passed when vertex attributes desc and/or
    // shader program changed.
    angle::Result setupDraw(const gl::Context *glContext,
                            mtl::RenderCommandEncoder *cmdEncoder,
                            const mtl::RenderPipelineDesc &pipelineDesc,
                            bool pipelineDescChanged,
                            bool forceTexturesSetting,
                            bool uniformBuffersDirty);

    const ProgramExecutableMtl *getExecutable() const
    {
        return mtl::GetImpl(&mState.getExecutable());
    }
    ProgramExecutableMtl *getExecutable() { return mtl::GetImpl(&mState.getExecutable()); }

  private:
    class ProgramLinkEvent;
    class CompileMslTask;

    template <int cols, int rows>
    void setUniformMatrixfv(GLint location,
                            GLsizei count,
                            GLboolean transpose,
                            const GLfloat *value);
    template <class T>
    void getUniformImpl(GLint location, T *v, GLenum entryPointType) const;

    template <typename T>
    void setUniformImpl(GLint location, GLsizei count, const T *v, GLenum entryPointType);

    angle::Result commitUniforms(ContextMtl *context, mtl::RenderCommandEncoder *cmdEncoder);
    angle::Result updateTextures(const gl::Context *glContext,
                                 mtl::RenderCommandEncoder *cmdEncoder,
                                 bool forceUpdate);

    angle::Result updateUniformBuffers(ContextMtl *context,
                                       mtl::RenderCommandEncoder *cmdEncoder,
                                       const mtl::RenderPipelineDesc &pipelineDesc);
    angle::Result updateXfbBuffers(ContextMtl *context,
                                   mtl::RenderCommandEncoder *cmdEncoder,
                                   const mtl::RenderPipelineDesc &pipelineDesc);
    angle::Result legalizeUniformBufferOffsets(ContextMtl *context,
                                               const std::vector<gl::InterfaceBlock> &blocks);
    angle::Result bindUniformBuffersToDiscreteSlots(ContextMtl *context,
                                                    mtl::RenderCommandEncoder *cmdEncoder,
                                                    const std::vector<gl::InterfaceBlock> &blocks,
                                                    gl::ShaderType shaderType);

    angle::Result encodeUniformBuffersInfoArgumentBuffer(
        ContextMtl *context,
        mtl::RenderCommandEncoder *cmdEncoder,
        const std::vector<gl::InterfaceBlock> &blocks,
        gl::ShaderType shaderType);

    void reset(ContextMtl *context);

    void linkResources(const gl::ProgramLinkedResources &resources);
    std::unique_ptr<LinkEvent> compileMslShaderLibs(const gl::Context *context);

    mtl::BufferPool *getBufferPool(ContextMtl *context);

    gl::ShaderMap<SharedCompiledShaderStateMtl> mAttachedShaders;

    // Scratch data:
    // Legalized buffers and their offsets. For example, uniform buffer's offset=1 is not a valid
    // offset, it will be converted to legal offset and the result is stored in this array.
    std::vector<std::pair<mtl::BufferRef, uint32_t>> mLegalizedOffsetedUniformBuffers;
    // Stores the render stages usage of each uniform buffer. Only used if the buffers are encoded
    // into an argument buffer.
    std::vector<uint32_t> mArgumentBufferRenderStageUsages;

    uint32_t mShadowCompareModes[mtl::kMaxShaderSamplers];

    mtl::BufferPool *mAuxBufferPool;
};

}  // namespace rx

#endif /* LIBANGLE_RENDERER_METAL_PROGRAMMTL_H_ */
