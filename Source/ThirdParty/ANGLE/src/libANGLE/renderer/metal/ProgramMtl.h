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
#include "libANGLE/renderer/glslang_wrapper_utils.h"
#include "libANGLE/renderer/metal/mtl_command_buffer.h"
#include "libANGLE/renderer/metal/mtl_resources.h"
#include "libANGLE/renderer/metal/mtl_state_cache.h"

namespace rx
{
class ContextMtl;

class ProgramMtl : public ProgramImpl
{
  public:
    ProgramMtl(const gl::ProgramState &state);
    ~ProgramMtl() override;

    void destroy(const gl::Context *context) override;

    std::unique_ptr<LinkEvent> load(const gl::Context *context,
                                    gl::BinaryInputStream *stream,
                                    gl::InfoLog &infoLog) override;
    void save(const gl::Context *context, gl::BinaryOutputStream *stream) override;
    void setBinaryRetrievableHint(bool retrievable) override;
    void setSeparable(bool separable) override;

    std::unique_ptr<LinkEvent> link(const gl::Context *context,
                                    const gl::ProgramLinkedResources &resources,
                                    gl::InfoLog &infoLog) override;
    GLboolean validate(const gl::Caps &caps, gl::InfoLog *infoLog) override;

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

    // Calls this before drawing, changedPipelineDesc is passed when vertex attributes desc and/or
    // shader program changed.
    angle::Result setupDraw(const gl::Context *glContext,
                            mtl::RenderCommandEncoder *cmdEncoder,
                            const Optional<mtl::RenderPipelineDesc> &changedPipelineDesc,
                            bool forceTexturesSetting);

  private:
    template <int cols, int rows>
    void setUniformMatrixfv(GLint location,
                            GLsizei count,
                            GLboolean transpose,
                            const GLfloat *value);
    template <class T>
    void getUniformImpl(GLint location, T *v, GLenum entryPointType) const;

    template <typename T>
    void setUniformImpl(GLint location, GLsizei count, const T *v, GLenum entryPointType);

    angle::Result initDefaultUniformBlocks(const gl::Context *glContext);

    angle::Result commitUniforms(ContextMtl *context, mtl::RenderCommandEncoder *cmdEncoder);
    angle::Result updateTextures(const gl::Context *glContext,
                                 mtl::RenderCommandEncoder *cmdEncoder,
                                 bool forceUpdate);

    void reset(ContextMtl *context);
    void linkResources(const gl::ProgramLinkedResources &resources);
    angle::Result linkImpl(const gl::Context *glContext,
                           const gl::ProgramLinkedResources &resources,
                           gl::InfoLog &infoLog);
    angle::Result convertToMsl(const gl::Context *glContext,
                               gl::ShaderType shaderType,
                               gl::InfoLog &infoLog,
                               std::vector<uint32_t> *sprivCode);

    angle::Result createMslShader(const gl::Context *glContext,
                                  gl::ShaderType shaderType,
                                  gl::InfoLog &infoLog,
                                  const std::string &translatedSource);

    // State for the default uniform blocks.
    struct DefaultUniformBlock final : private angle::NonCopyable
    {
        DefaultUniformBlock();
        ~DefaultUniformBlock();

        // Shadow copies of the shader uniform data.
        angle::MemoryBuffer uniformData;

        // Since the default blocks are laid out in std140, this tells us where to write on a call
        // to a setUniform method. They are arranged in uniform location order.
        std::vector<sh::BlockMemberInfo> uniformLayout;
    };

    gl::ShaderBitSet mDefaultUniformBlocksDirty;
    gl::ShaderBitSet mSamplerBindingsDirty;
    gl::ShaderMap<DefaultUniformBlock> mDefaultUniformBlocks;

    mtl::RenderPipelineCache mMetalRenderPipelineCache;
};

}  // namespace rx

#endif /* LIBANGLE_RENDERER_METAL_PROGRAMMTL_H_ */
