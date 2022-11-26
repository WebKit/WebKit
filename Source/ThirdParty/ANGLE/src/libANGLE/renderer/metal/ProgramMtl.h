
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
#include "libANGLE/renderer/ShaderInterfaceVariableInfoMap.h"
#include "libANGLE/renderer/glslang_wrapper_utils.h"
#include "libANGLE/renderer/metal/mtl_buffer_pool.h"
#include "libANGLE/renderer/metal/mtl_command_buffer.h"
#include "libANGLE/renderer/metal/mtl_common.h"
#include "libANGLE/renderer/metal/mtl_context_device.h"
#include "libANGLE/renderer/metal/mtl_glslang_mtl_utils.h"
#include "libANGLE/renderer/metal/mtl_resources.h"
#include "libANGLE/renderer/metal/mtl_state_cache.h"

namespace rx
{
#define SHADER_ENTRY_NAME @"main0"
class ContextMtl;

struct ProgramArgumentBufferEncoderMtl
{
    void reset(ContextMtl *contextMtl);

    mtl::AutoObjCPtr<id<MTLArgumentEncoder>> metalArgBufferEncoder;
    mtl::BufferPool bufferPool;
};

// Represents a specialized shader variant. For example, a shader variant with fragment coverage
// mask enabled and a shader variant without.
struct ProgramShaderObjVariantMtl
{
    void reset(ContextMtl *contextMtl);

    mtl::AutoObjCPtr<id<MTLFunction>> metalShader;
    // UBO's argument buffer encoder. Used when number of UBOs used exceeds number of allowed
    // discrete slots, and thus needs to encode all into one argument buffer.
    ProgramArgumentBufferEncoderMtl uboArgBufferEncoder;

    // Store reference to the TranslatedShaderInfo to easy querying mapped textures/UBO/XFB
    // bindings.
    const mtl::TranslatedShaderInfo *translatedSrcInfo;
};

class ProgramMtl : public ProgramImpl, public mtl::RenderPipelineCacheSpecializeShaderFactory
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
                                    gl::InfoLog &infoLog,
                                    const gl::ProgramMergedVaryings &mergedVaryings) override;
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

    // Override mtl::RenderPipelineCacheSpecializeShaderFactory
    angle::Result getSpecializedShader(ContextMtl *context,
                                       gl::ShaderType shaderType,
                                       const mtl::RenderPipelineDesc &renderPipelineDesc,
                                       id<MTLFunction> *shaderOut) override;
    bool hasSpecializedShader(gl::ShaderType shaderType,
                              const mtl::RenderPipelineDesc &renderPipelineDesc) override;

    angle::Result createMslShaderLib(
        ContextMtl *context,
        gl::ShaderType shaderType,
        gl::InfoLog &infoLog,
        mtl::TranslatedShaderInfo *translatedMslInfo,
        NSDictionary<NSString *, NSObject *> *subtitutionDictionary = @{});
    // Calls this before drawing, changedPipelineDesc is passed when vertex attributes desc and/or
    // shader program changed.
    angle::Result setupDraw(const gl::Context *glContext,
                            mtl::RenderCommandEncoder *cmdEncoder,
                            const mtl::RenderPipelineDesc &pipelineDesc,
                            bool pipelineDescChanged,
                            bool forceTexturesSetting,
                            bool uniformBuffersDirty);

    std::string getTranslatedShaderSource(const gl::ShaderType shaderType) const
    {
        return mMslShaderTranslateInfo[shaderType].metalShaderSource;
    }

    mtl::TranslatedShaderInfo getTranslatedShaderInfo(const gl::ShaderType shaderType) const
    {
        return mMslShaderTranslateInfo[shaderType];
    }

    bool hasFlatAttribute() const { return mProgramHasFlatAttributes; }

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
    angle::Result resizeDefaultUniformBlocksMemory(const gl::Context *glContext,
                                                   const gl::ShaderMap<size_t> &requiredBufferSize);
    void saveDefaultUniformBlocksInfo(gl::BinaryOutputStream *stream);
    angle::Result loadDefaultUniformBlocksInfo(const gl::Context *glContext,
                                               gl::BinaryInputStream *stream);

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

    void saveTranslatedShaders(gl::BinaryOutputStream *stream);
    void loadTranslatedShaders(gl::BinaryInputStream *stream);

    void saveShaderInternalInfo(gl::BinaryOutputStream *stream);
    void loadShaderInternalInfo(gl::BinaryInputStream *stream);

    void linkUpdateHasFlatAttributes(const gl::Context *context);

    angle::Result linkImplDirect(const gl::Context *glContext,
                                 const gl::ProgramLinkedResources &resources,
                                 gl::InfoLog &infoLog);

    void linkResources(const gl::Context *context, const gl::ProgramLinkedResources &resources);
    angle::Result linkImpl(const gl::Context *glContext,
                           const gl::ProgramLinkedResources &resources,
                           gl::InfoLog &infoLog);

    angle::Result linkTranslatedShaders(const gl::Context *glContext,
                                        gl::BinaryInputStream *stream,
                                        gl::InfoLog &infoLog);

    mtl::BufferPool *getBufferPool(ContextMtl *context);

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

    bool mProgramHasFlatAttributes;
    gl::ShaderBitSet mDefaultUniformBlocksDirty;
    gl::ShaderBitSet mSamplerBindingsDirty;
    gl::ShaderMap<DefaultUniformBlock> mDefaultUniformBlocks;

    // Translated metal shaders:
    gl::ShaderMap<mtl::TranslatedShaderInfo> mMslShaderTranslateInfo;

    // Translated metal version for transform feedback only vertex shader:
    // - Metal doesn't allow vertex shader to write to both buffers and to stage output
    // (gl_Position). Need a special version of vertex shader that only writes to transform feedback
    // buffers.
    mtl::TranslatedShaderInfo mMslXfbOnlyVertexShaderInfo;

    // Compiled native shader object variants:
    // - Vertex shader: One with emulated rasterization discard, one with true rasterization
    // discard, one without.
    mtl::RenderPipelineRasterStateMap<ProgramShaderObjVariantMtl> mVertexShaderVariants;
    // - Fragment shader: One with sample coverage mask enabled, one with it disabled.
    std::array<ProgramShaderObjVariantMtl, 2> mFragmentShaderVariants;

    // Cached references of current shader variants.
    gl::ShaderMap<ProgramShaderObjVariantMtl *> mCurrentShaderVariants;

    ShaderInterfaceVariableInfoMap mVariableInfoMap;
    // Scratch data:
    // Legalized buffers and their offsets. For example, uniform buffer's offset=1 is not a valid
    // offset, it will be converted to legal offset and the result is stored in this array.
    std::vector<std::pair<mtl::BufferRef, uint32_t>> mLegalizedOffsetedUniformBuffers;
    // Stores the render stages usage of each uniform buffer. Only used if the buffers are encoded
    // into an argument buffer.
    std::vector<uint32_t> mArgumentBufferRenderStageUsages;

    uint32_t mShadowCompareModes[mtl::kMaxShaderSamplers];

    mtl::RenderPipelineCache mMetalRenderPipelineCache;
    mtl::BufferPool *mAuxBufferPool;
};

}  // namespace rx

#endif /* LIBANGLE_RENDERER_METAL_PROGRAMMTL_H_ */
