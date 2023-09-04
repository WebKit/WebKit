//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramExecutableMtl.h: Implementation of ProgramExecutableImpl.

#ifndef LIBANGLE_RENDERER_MTL_PROGRAMEXECUTABLEMTL_H_
#define LIBANGLE_RENDERER_MTL_PROGRAMEXECUTABLEMTL_H_

#include "libANGLE/ProgramExecutable.h"
#include "libANGLE/renderer/ProgramExecutableImpl.h"
#include "libANGLE/renderer/metal/mtl_buffer_pool.h"
#include "libANGLE/renderer/metal/mtl_common.h"
#include "libANGLE/renderer/metal/mtl_msl_utils.h"
#include "libANGLE/renderer/metal/mtl_resources.h"
#include "libANGLE/renderer/metal/mtl_state_cache.h"

namespace rx
{
class ContextMtl;

struct UBOConversionInfo
{

    UBOConversionInfo(const std::vector<sh::BlockMemberInfo> &stdInfo,
                      const std::vector<sh::BlockMemberInfo> &metalInfo,
                      size_t stdSize,
                      size_t metalSize)
        : _stdInfo(stdInfo), _metalInfo(metalInfo), _stdSize(stdSize), _metalSize(metalSize)
    {
        _needsConversion = _calculateNeedsConversion();
    }
    const std::vector<sh::BlockMemberInfo> &stdInfo() const { return _stdInfo; }
    const std::vector<sh::BlockMemberInfo> &metalInfo() const { return _metalInfo; }
    size_t stdSize() const { return _stdSize; }
    size_t metalSize() const { return _metalSize; }

    bool needsConversion() const { return _needsConversion; }

  private:
    std::vector<sh::BlockMemberInfo> _stdInfo, _metalInfo;
    size_t _stdSize, _metalSize;
    bool _needsConversion;

    bool _calculateNeedsConversion()
    {
        if (_stdSize != _metalSize)
        {
            return true;
        }
        if (_stdInfo.size() != _metalInfo.size())
        {
            return true;
        }
        for (size_t i = 0; i < _stdInfo.size(); ++i)
        {
            // If the matrix is trasnposed
            if (_stdInfo[i].isRowMajorMatrix)
            {
                return true;
            }
            // If we have a bool
            if (gl::VariableComponentType(_stdInfo[i].type) == GL_BOOL)
            {
                return true;
            }
            // If any offset information is different
            if (!(_stdInfo[i] == _metalInfo[i]))
            {
                return true;
            }
        }
        return false;
    }
};

struct ProgramArgumentBufferEncoderMtl
{
    void reset(ContextMtl *contextMtl);

    mtl::AutoObjCPtr<id<MTLArgumentEncoder>> metalArgBufferEncoder;
    mtl::BufferPool bufferPool;
};

constexpr size_t kFragmentShaderVariants = 4;

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

// State for the default uniform blocks.
struct DefaultUniformBlockMtl final : private angle::NonCopyable
{
    DefaultUniformBlockMtl();
    ~DefaultUniformBlockMtl();

    // Shadow copies of the shader uniform data.
    angle::MemoryBuffer uniformData;

    // Since the default blocks are laid out in std140, this tells us where to write on a call
    // to a setUniform method. They are arranged in uniform location order.
    std::vector<sh::BlockMemberInfo> uniformLayout;
};

class ProgramExecutableMtl : public ProgramExecutableImpl
{
  public:
    ProgramExecutableMtl(const gl::ProgramExecutable *executable);
    ~ProgramExecutableMtl() override;

    void destroy(const gl::Context *context) override;

    angle::Result load(ContextMtl *contextMtl, gl::BinaryInputStream *stream);
    void save(gl::BinaryOutputStream *stream);

    bool hasFlatAttribute() const { return mProgramHasFlatAttributes; }

  private:
    friend class ProgramMtl;

    void reset(ContextMtl *context);

    void saveTranslatedShaders(gl::BinaryOutputStream *stream);
    void loadTranslatedShaders(gl::BinaryInputStream *stream);

    void saveShaderInternalInfo(gl::BinaryOutputStream *stream);
    void loadShaderInternalInfo(gl::BinaryInputStream *stream);

    void saveInterfaceBlockInfo(gl::BinaryOutputStream *stream);
    angle::Result loadInterfaceBlockInfo(gl::BinaryInputStream *stream);

    void saveDefaultUniformBlocksInfo(gl::BinaryOutputStream *stream);
    angle::Result loadDefaultUniformBlocksInfo(mtl::Context *context,
                                               gl::BinaryInputStream *stream);

    void linkUpdateHasFlatAttributes(const gl::SharedCompiledShaderState &vertexShader);

    angle::Result initDefaultUniformBlocks(
        mtl::Context *context,
        const gl::ShaderMap<gl::SharedCompiledShaderState> &shaders);
    angle::Result resizeDefaultUniformBlocksMemory(mtl::Context *context,
                                                   const gl::ShaderMap<size_t> &requiredBufferSize);
    void initUniformBlocksRemapper(const gl::SharedCompiledShaderState &shader);

    bool mProgramHasFlatAttributes;

    gl::ShaderMap<DefaultUniformBlockMtl> mDefaultUniformBlocks;
    std::unordered_map<std::string, UBOConversionInfo> mUniformBlockConversions;

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
    // - Fragment shader: Combinations of sample coverage mask and depth write enabled states.
    std::array<ProgramShaderObjVariantMtl, kFragmentShaderVariants> mFragmentShaderVariants;

    // Cached references of current shader variants.
    gl::ShaderMap<ProgramShaderObjVariantMtl *> mCurrentShaderVariants;

    gl::ShaderBitSet mDefaultUniformBlocksDirty;
    gl::ShaderBitSet mSamplerBindingsDirty;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_MTL_PROGRAMEXECUTABLEMTL_H_
