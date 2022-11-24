//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramD3D.h: Defines the rx::ProgramD3D class which implements rx::ProgramImpl.

#ifndef LIBANGLE_RENDERER_D3D_PROGRAMD3D_H_
#define LIBANGLE_RENDERER_D3D_PROGRAMD3D_H_

#include <string>
#include <vector>

#include "compiler/translator/blocklayoutHLSL.h"
#include "libANGLE/Constants.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/ProgramImpl.h"
#include "libANGLE/renderer/d3d/DynamicHLSL.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"
#include "platform/FeaturesD3D_autogen.h"

namespace rx
{
class RendererD3D;
class UniformStorageD3D;
class ShaderExecutableD3D;

#if !defined(ANGLE_COMPILE_OPTIMIZATION_LEVEL)
// WARNING: D3DCOMPILE_OPTIMIZATION_LEVEL3 may lead to a DX9 shader compiler hang.
// It should only be used selectively to work around specific bugs.
#    define ANGLE_COMPILE_OPTIMIZATION_LEVEL D3DCOMPILE_OPTIMIZATION_LEVEL1
#endif

enum class HLSLRegisterType : uint8_t
{
    None                = 0,
    Texture             = 1,
    UnorderedAccessView = 2
};

// Helper struct representing a single shader uniform
// TODO(jmadill): Make uniform blocks shared between all programs, so we don't need separate
// register indices.
struct D3DUniform : private angle::NonCopyable
{
    D3DUniform(GLenum type,
               HLSLRegisterType reg,
               const std::string &nameIn,
               const std::vector<unsigned int> &arraySizesIn,
               bool defaultBlock);
    ~D3DUniform();

    bool isSampler() const;
    bool isImage() const;
    bool isImage2D() const;
    bool isArray() const { return !arraySizes.empty(); }
    unsigned int getArraySizeProduct() const;
    bool isReferencedByShader(gl::ShaderType shaderType) const;

    const uint8_t *firstNonNullData() const;
    const uint8_t *getDataPtrToElement(size_t elementIndex) const;

    // Duplicated from the GL layer
    const gl::UniformTypeInfo &typeInfo;
    std::string name;  // Names of arrays don't include [0], unlike at the GL layer.
    std::vector<unsigned int> arraySizes;

    // Pointer to a system copies of the data. Separate pointers for each uniform storage type.
    gl::ShaderMap<uint8_t *> mShaderData;

    // Register information.
    HLSLRegisterType regType;
    gl::ShaderMap<unsigned int> mShaderRegisterIndexes;
    unsigned int registerCount;

    // Register "elements" are used for uniform structs in ES3, to appropriately identify single
    // uniforms
    // inside aggregate types, which are packed according C-like structure rules.
    unsigned int registerElement;

    // Special buffer for sampler values.
    std::vector<GLint> mSamplerData;
};

struct D3DInterfaceBlock
{
    D3DInterfaceBlock();
    D3DInterfaceBlock(const D3DInterfaceBlock &other);

    bool activeInShader(gl::ShaderType shaderType) const
    {
        return mShaderRegisterIndexes[shaderType] != GL_INVALID_INDEX;
    }

    gl::ShaderMap<unsigned int> mShaderRegisterIndexes;
};

struct D3DUniformBlock : D3DInterfaceBlock
{
    D3DUniformBlock();
    D3DUniformBlock(const D3DUniformBlock &other);

    gl::ShaderMap<bool> mUseStructuredBuffers;
    gl::ShaderMap<unsigned int> mByteWidths;
    gl::ShaderMap<unsigned int> mStructureByteStrides;
};

struct ShaderStorageBlock
{
    std::string name;
    unsigned int arraySize     = 0;
    unsigned int registerIndex = 0;
};

struct D3DUBOCache
{
    unsigned int registerIndex;
    int binding;
};

struct D3DUBOCacheUseSB : D3DUBOCache
{
    unsigned int byteWidth;
    unsigned int structureByteStride;
};

struct D3DVarying final
{
    D3DVarying();
    D3DVarying(const std::string &semanticNameIn,
               unsigned int semanticIndexIn,
               unsigned int componentCountIn,
               unsigned int outputSlotIn);

    D3DVarying(const D3DVarying &)            = default;
    D3DVarying &operator=(const D3DVarying &) = default;

    std::string semanticName;
    unsigned int semanticIndex;
    unsigned int componentCount;
    unsigned int outputSlot;
};

class ProgramD3DMetadata final : angle::NonCopyable
{
  public:
    ProgramD3DMetadata(RendererD3D *renderer,
                       const gl::ShaderMap<const ShaderD3D *> &attachedShaders,
                       EGLenum clientType);
    ~ProgramD3DMetadata();

    int getRendererMajorShaderModel() const;
    bool usesBroadcast(const gl::State &data) const;
    bool usesSecondaryColor() const;
    bool usesClipDistance() const;
    bool usesCullDistance() const;
    bool usesFragDepth() const;
    bool usesPointCoord() const;
    bool usesFragCoord() const;
    bool usesPointSize() const;
    bool usesInsertedPointCoordValue() const;
    bool usesViewScale() const;
    bool hasANGLEMultiviewEnabled() const;
    bool usesVertexID() const;
    bool usesViewID() const;
    bool canSelectViewInVertexShader() const;
    bool addsPointCoordToVertexShader() const;
    bool usesTransformFeedbackGLPosition() const;
    bool usesSystemValuePointSize() const;
    bool usesMultipleFragmentOuts() const;
    bool usesCustomOutVars() const;
    const ShaderD3D *getFragmentShader() const;

  private:
    const int mRendererMajorShaderModel;
    const std::string mShaderModelSuffix;
    const bool mUsesInstancedPointSpriteEmulation;
    const bool mUsesViewScale;
    const bool mCanSelectViewInVertexShader;
    const gl::ShaderMap<const ShaderD3D *> mAttachedShaders;
    const EGLenum mClientType;
};

using D3DUniformMap = std::map<std::string, D3DUniform *>;

class ProgramD3D : public ProgramImpl
{
  public:
    ProgramD3D(const gl::ProgramState &data, RendererD3D *renderer);
    ~ProgramD3D() override;

    const std::vector<PixelShaderOutputVariable> &getPixelShaderKey() { return mPixelShaderKey; }

    GLint getSamplerMapping(gl::ShaderType type,
                            unsigned int samplerIndex,
                            const gl::Caps &caps) const;
    gl::TextureType getSamplerTextureType(gl::ShaderType type, unsigned int samplerIndex) const;
    gl::RangeUI getUsedSamplerRange(gl::ShaderType type) const;

    enum SamplerMapping
    {
        WasDirty,
        WasClean,
    };

    SamplerMapping updateSamplerMapping();

    GLint getImageMapping(gl::ShaderType type,
                          unsigned int imageIndex,
                          bool readonly,
                          const gl::Caps &caps) const;
    gl::RangeUI getUsedImageRange(gl::ShaderType type, bool readonly) const;

    bool usesPointSize() const { return mUsesPointSize; }
    bool usesPointSpriteEmulation() const;
    bool usesGeometryShader(const gl::State &state, gl::PrimitiveMode drawMode) const;
    bool usesGeometryShaderForPointSpriteEmulation() const;
    bool usesGetDimensionsIgnoresBaseLevel() const;
    bool usesInstancedPointSpriteEmulation() const;

    std::unique_ptr<LinkEvent> load(const gl::Context *context,
                                    gl::BinaryInputStream *stream,
                                    gl::InfoLog &infoLog) override;
    void save(const gl::Context *context, gl::BinaryOutputStream *stream) override;
    void setBinaryRetrievableHint(bool retrievable) override;
    void setSeparable(bool separable) override;

    angle::Result getVertexExecutableForCachedInputLayout(d3d::Context *context,
                                                          ShaderExecutableD3D **outExectuable,
                                                          gl::InfoLog *infoLog);
    angle::Result getGeometryExecutableForPrimitiveType(d3d::Context *errContext,
                                                        const gl::State &state,
                                                        gl::PrimitiveMode drawMode,
                                                        ShaderExecutableD3D **outExecutable,
                                                        gl::InfoLog *infoLog);
    angle::Result getPixelExecutableForCachedOutputLayout(d3d::Context *context,
                                                          ShaderExecutableD3D **outExectuable,
                                                          gl::InfoLog *infoLog);
    angle::Result getComputeExecutableForImage2DBindLayout(const gl::Context *glContext,
                                                           d3d::Context *context,
                                                           ShaderExecutableD3D **outExecutable,
                                                           gl::InfoLog *infoLog);
    std::unique_ptr<LinkEvent> link(const gl::Context *context,
                                    const gl::ProgramLinkedResources &resources,
                                    gl::InfoLog &infoLog,
                                    const gl::ProgramMergedVaryings &mergedVaryings) override;
    GLboolean validate(const gl::Caps &caps, gl::InfoLog *infoLog) override;

    void updateUniformBufferCache(const gl::Caps &caps);

    unsigned int getAtomicCounterBufferRegisterIndex(GLuint binding,
                                                     gl::ShaderType shaderType) const;

    unsigned int getShaderStorageBufferRegisterIndex(GLuint blockIndex,
                                                     gl::ShaderType shaderType) const;
    const std::vector<D3DUBOCache> &getShaderUniformBufferCache(gl::ShaderType shaderType) const;
    const std::vector<D3DUBOCacheUseSB> &getShaderUniformBufferCacheUseSB(
        gl::ShaderType shaderType) const;

    void dirtyAllUniforms();

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

    UniformStorageD3D *getShaderUniformStorage(gl::ShaderType shaderType) const
    {
        return mShaderUniformStorages[shaderType].get();
    }

    unsigned int getSerial() const;

    const AttribIndexArray &getAttribLocationToD3DSemantics() const
    {
        return mAttribLocationToD3DSemantic;
    }

    void updateCachedInputLayout(Serial associatedSerial, const gl::State &state);
    void updateCachedOutputLayout(const gl::Context *context, const gl::Framebuffer *framebuffer);
    void updateCachedComputeImage2DBindLayout(const gl::Context *context);

    bool isSamplerMappingDirty() { return mDirtySamplerMapping; }

    // Checks if we need to recompile certain shaders.
    bool hasVertexExecutableForCachedInputLayout();
    bool hasGeometryExecutableForPrimitiveType(const gl::State &state, gl::PrimitiveMode drawMode);
    bool hasPixelExecutableForCachedOutputLayout();
    bool hasComputeExecutableForCachedImage2DBindLayout();

    bool anyShaderUniformsDirty() const { return mShaderUniformsDirty.any(); }

    bool areShaderUniformsDirty(gl::ShaderType shaderType) const
    {
        return mShaderUniformsDirty[shaderType];
    }
    const std::vector<D3DUniform *> &getD3DUniforms() const { return mD3DUniforms; }
    void markUniformsClean();

    const gl::ProgramState &getState() const { return mState; }

    bool hasShaderStage(gl::ShaderType shaderType) const
    {
        return mState.getExecutable().getLinkedShaderStages()[shaderType];
    }

    void assignImage2DRegisters(gl::ShaderType shaderType,
                                unsigned int startImageIndex,
                                int startLogicalImageUnit,
                                bool readonly);
    bool hasNamedUniform(const std::string &name);

    bool usesVertexID() const { return mUsesVertexID; }

  private:
    // These forward-declared tasks are used for multi-thread shader compiles.
    class GetExecutableTask;
    class GetVertexExecutableTask;
    class GetPixelExecutableTask;
    class GetGeometryExecutableTask;
    class GetComputeExecutableTask;
    class GraphicsProgramLinkEvent;
    class ComputeProgramLinkEvent;

    class LoadBinaryTask;
    class LoadBinaryLinkEvent;

    class VertexExecutable
    {
      public:
        enum HLSLAttribType
        {
            FLOAT,
            UNSIGNED_INT,
            SIGNED_INT,
        };

        typedef std::vector<HLSLAttribType> Signature;

        VertexExecutable(const gl::InputLayout &inputLayout,
                         const Signature &signature,
                         ShaderExecutableD3D *shaderExecutable);
        ~VertexExecutable();

        bool matchesSignature(const Signature &signature) const;
        static void getSignature(RendererD3D *renderer,
                                 const gl::InputLayout &inputLayout,
                                 Signature *signatureOut);

        const gl::InputLayout &inputs() const { return mInputs; }
        const Signature &signature() const { return mSignature; }
        ShaderExecutableD3D *shaderExecutable() const { return mShaderExecutable; }

      private:
        static HLSLAttribType GetAttribType(GLenum type);

        gl::InputLayout mInputs;
        Signature mSignature;
        ShaderExecutableD3D *mShaderExecutable;
    };

    class PixelExecutable
    {
      public:
        PixelExecutable(const std::vector<GLenum> &outputSignature,
                        ShaderExecutableD3D *shaderExecutable);
        ~PixelExecutable();

        bool matchesSignature(const std::vector<GLenum> &signature) const
        {
            return mOutputSignature == signature;
        }

        const std::vector<GLenum> &outputSignature() const { return mOutputSignature; }
        ShaderExecutableD3D *shaderExecutable() const { return mShaderExecutable; }

      private:
        std::vector<GLenum> mOutputSignature;
        ShaderExecutableD3D *mShaderExecutable;
    };

    class ComputeExecutable
    {
      public:
        ComputeExecutable(const gl::ImageUnitTextureTypeMap &signature,
                          std::unique_ptr<ShaderExecutableD3D> shaderExecutable);
        ~ComputeExecutable();

        bool matchesSignature(const gl::ImageUnitTextureTypeMap &signature) const
        {
            return mSignature == signature;
        }

        const gl::ImageUnitTextureTypeMap &signature() const { return mSignature; }
        ShaderExecutableD3D *shaderExecutable() const { return mShaderExecutable.get(); }

      private:
        gl::ImageUnitTextureTypeMap mSignature;
        std::unique_ptr<ShaderExecutableD3D> mShaderExecutable;
    };

    struct Sampler
    {
        Sampler();

        bool active;
        GLint logicalTextureUnit;
        gl::TextureType textureType;
    };

    struct Image
    {
        Image();
        bool active;
        GLint logicalImageUnit;
    };

    void initializeUniformStorage(const gl::ShaderBitSet &availableShaderStages);

    void defineUniformsAndAssignRegisters(const gl::Context *context);
    void defineUniformBase(const gl::Shader *shader,
                           const sh::ShaderVariable &uniform,
                           D3DUniformMap *uniformMap);
    void assignAllSamplerRegisters();
    void assignSamplerRegisters(size_t uniformIndex);

    static void AssignSamplers(unsigned int startSamplerIndex,
                               const gl::UniformTypeInfo &typeInfo,
                               unsigned int samplerCount,
                               std::vector<Sampler> &outSamplers,
                               gl::RangeUI *outUsedRange);

    void assignAllImageRegisters();
    void assignAllAtomicCounterRegisters();
    void assignImageRegisters(size_t uniformIndex);
    static void AssignImages(unsigned int startImageIndex,
                             int startLogicalImageUnit,
                             unsigned int imageCount,
                             std::vector<Image> &outImages,
                             gl::RangeUI *outUsedRange);

    template <typename DestT>
    void getUniformInternal(GLint location, DestT *dataOut) const;

    template <typename T>
    void setUniformImpl(D3DUniform *targetUniform,
                        const gl::VariableLocation &locationInfo,
                        GLsizei count,
                        const T *v,
                        uint8_t *targetData,
                        GLenum uniformType);

    template <typename T>
    void setUniformInternal(GLint location, GLsizei count, const T *v, GLenum uniformType);

    template <int cols, int rows>
    void setUniformMatrixfvInternal(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *value);

    std::unique_ptr<LinkEvent> compileProgramExecutables(const gl::Context *context,
                                                         gl::InfoLog &infoLog);
    std::unique_ptr<LinkEvent> compileComputeExecutable(const gl::Context *context,
                                                        gl::InfoLog &infoLog);

    angle::Result loadBinaryShaderExecutables(d3d::Context *contextD3D,
                                              gl::BinaryInputStream *stream,
                                              gl::InfoLog &infoLog);

    void gatherTransformFeedbackVaryings(const gl::VaryingPacking &varyings,
                                         const BuiltinInfo &builtins);
    D3DUniform *getD3DUniformFromLocation(GLint location);
    const D3DUniform *getD3DUniformFromLocation(GLint location) const;

    void initAttribLocationsToD3DSemantic(const gl::Context *context);

    void reset();
    void initializeUniformBlocks();
    void initializeShaderStorageBlocks(const gl::Context *context);

    void updateCachedInputLayoutFromShader(const gl::Context *context);
    void updateCachedOutputLayoutFromShader();
    void updateCachedImage2DBindLayoutFromShader(gl::ShaderType shaderType);
    void updateCachedVertexExecutableIndex();
    void updateCachedPixelExecutableIndex();
    void updateCachedComputeExecutableIndex();

    void linkResources(const gl::Context *context, const gl::ProgramLinkedResources &resources);

    RendererD3D *mRenderer;
    DynamicHLSL *mDynamicHLSL;

    std::vector<std::unique_ptr<VertexExecutable>> mVertexExecutables;
    std::vector<std::unique_ptr<PixelExecutable>> mPixelExecutables;
    angle::PackedEnumMap<gl::PrimitiveMode, std::unique_ptr<ShaderExecutableD3D>>
        mGeometryExecutables;
    std::vector<std::unique_ptr<ComputeExecutable>> mComputeExecutables;

    gl::ShaderMap<std::string> mShaderHLSL;
    gl::ShaderMap<CompilerWorkaroundsD3D> mShaderWorkarounds;

    bool mUsesFragDepth;
    bool mHasANGLEMultiviewEnabled;
    bool mUsesVertexID;
    bool mUsesViewID;
    std::vector<PixelShaderOutputVariable> mPixelShaderKey;

    // Common code for all dynamic geometry shaders. Consists mainly of the GS input and output
    // structures, built from the linked varying info. We store the string itself instead of the
    // packed varyings for simplicity.
    std::string mGeometryShaderPreamble;

    bool mUsesPointSize;
    bool mUsesFlatInterpolation;

    gl::ShaderMap<std::unique_ptr<UniformStorageD3D>> mShaderUniformStorages;

    gl::ShaderMap<std::vector<Sampler>> mShaderSamplers;
    gl::ShaderMap<gl::RangeUI> mUsedShaderSamplerRanges;
    bool mDirtySamplerMapping;

    gl::ShaderMap<std::vector<Image>> mImages;
    gl::ShaderMap<std::vector<Image>> mReadonlyImages;
    gl::ShaderMap<gl::RangeUI> mUsedImageRange;
    gl::ShaderMap<gl::RangeUI> mUsedReadonlyImageRange;
    gl::ShaderMap<gl::RangeUI> mUsedAtomicCounterRange;

    // Cache for pixel shader output layout to save reallocations.
    std::vector<GLenum> mPixelShaderOutputLayoutCache;
    Optional<size_t> mCachedPixelExecutableIndex;

    AttribIndexArray mAttribLocationToD3DSemantic;

    unsigned int mSerial;

    gl::ShaderMap<std::vector<D3DUBOCache>> mShaderUBOCaches;
    gl::ShaderMap<std::vector<D3DUBOCacheUseSB>> mShaderUBOCachesUseSB;
    VertexExecutable::Signature mCachedVertexSignature;
    gl::InputLayout mCachedInputLayout;
    Optional<size_t> mCachedVertexExecutableIndex;

    std::vector<D3DVarying> mStreamOutVaryings;
    std::vector<D3DUniform *> mD3DUniforms;
    std::map<std::string, int> mImageBindingMap;
    std::map<std::string, int> mAtomicBindingMap;
    std::vector<D3DUniformBlock> mD3DUniformBlocks;
    std::vector<D3DInterfaceBlock> mD3DShaderStorageBlocks;
    gl::ShaderMap<std::vector<ShaderStorageBlock>> mShaderStorageBlocks;
    std::array<unsigned int, gl::IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS>
        mComputeAtomicCounterBufferRegisterIndices;

    gl::ShaderMap<std::vector<sh::ShaderVariable>> mImage2DUniforms;
    gl::ShaderMap<gl::ImageUnitTextureTypeMap> mImage2DBindLayoutCache;
    Optional<size_t> mCachedComputeExecutableIndex;

    gl::ShaderBitSet mShaderUniformsDirty;

    static unsigned int issueSerial();
    static unsigned int mCurrentSerial;

    Serial mCurrentVertexArrayStateSerial;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_D3D_PROGRAMD3D_H_
