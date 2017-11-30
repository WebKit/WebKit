//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
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
#include "platform/WorkaroundsD3D.h"

namespace rx
{
class RendererD3D;
class UniformStorageD3D;
class ShaderExecutableD3D;

#if !defined(ANGLE_COMPILE_OPTIMIZATION_LEVEL)
// WARNING: D3DCOMPILE_OPTIMIZATION_LEVEL3 may lead to a DX9 shader compiler hang.
// It should only be used selectively to work around specific bugs.
#define ANGLE_COMPILE_OPTIMIZATION_LEVEL D3DCOMPILE_OPTIMIZATION_LEVEL1
#endif

// Helper struct representing a single shader uniform
// TODO(jmadill): Make uniform blocks shared between all programs, so we don't need separate
// register indices.
struct D3DUniform : private angle::NonCopyable
{
    D3DUniform(GLenum type,
               const std::string &nameIn,
               const std::vector<unsigned int> &arraySizesIn,
               bool defaultBlock);
    ~D3DUniform();

    bool isSampler() const;

    bool isArray() const { return !arraySizes.empty(); }
    unsigned int getArraySizeProduct() const;

    bool isReferencedByVertexShader() const;
    bool isReferencedByFragmentShader() const;
    bool isReferencedByComputeShader() const;

    const uint8_t *firstNonNullData() const;
    const uint8_t *getDataPtrToElement(size_t elementIndex) const;

    // Duplicated from the GL layer
    const gl::UniformTypeInfo &typeInfo;
    std::string name;  // Names of arrays don't include [0], unlike at the GL layer.
    std::vector<unsigned int> arraySizes;

    // Pointer to a system copies of the data. Separate pointers for each uniform storage type.
    uint8_t *vsData;
    uint8_t *psData;
    uint8_t *csData;

    // Register information.
    unsigned int vsRegisterIndex;
    unsigned int psRegisterIndex;
    unsigned int csRegisterIndex;
    unsigned int registerCount;

    // Register "elements" are used for uniform structs in ES3, to appropriately identify single
    // uniforms
    // inside aggregate types, which are packed according C-like structure rules.
    unsigned int registerElement;

    // Special buffer for sampler values.
    std::vector<GLint> mSamplerData;
};

struct D3DUniformBlock
{
    D3DUniformBlock()
        : vsRegisterIndex(GL_INVALID_INDEX),
          psRegisterIndex(GL_INVALID_INDEX),
          csRegisterIndex(GL_INVALID_INDEX)
    {
    }

    bool vertexStaticUse() const { return vsRegisterIndex != GL_INVALID_INDEX; }

    bool fragmentStaticUse() const { return psRegisterIndex != GL_INVALID_INDEX; }

    bool computeStaticUse() const { return csRegisterIndex != GL_INVALID_INDEX; }

    unsigned int vsRegisterIndex;
    unsigned int psRegisterIndex;
    unsigned int csRegisterIndex;
};

struct D3DVarying final
{
    D3DVarying();
    D3DVarying(const std::string &semanticNameIn,
               unsigned int semanticIndexIn,
               unsigned int componentCountIn,
               unsigned int outputSlotIn);

    D3DVarying(const D3DVarying &) = default;
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
                       const ShaderD3D *vertexShader,
                       const ShaderD3D *fragmentShader);

    int getRendererMajorShaderModel() const;
    bool usesBroadcast(const gl::ContextState &data) const;
    bool usesFragDepth() const;
    bool usesPointCoord() const;
    bool usesFragCoord() const;
    bool usesPointSize() const;
    bool usesInsertedPointCoordValue() const;
    bool usesViewScale() const;
    bool hasANGLEMultiviewEnabled() const;
    bool usesViewID() const;
    bool canSelectViewInVertexShader() const;
    bool addsPointCoordToVertexShader() const;
    bool usesTransformFeedbackGLPosition() const;
    bool usesSystemValuePointSize() const;
    bool usesMultipleFragmentOuts() const;
    GLint getMajorShaderVersion() const;
    const ShaderD3D *getFragmentShader() const;

  private:
    const int mRendererMajorShaderModel;
    const std::string mShaderModelSuffix;
    const bool mUsesInstancedPointSpriteEmulation;
    const bool mUsesViewScale;
    const bool mHasANGLEMultiviewEnabled;
    const bool mUsesViewID;
    const bool mCanSelectViewInVertexShader;
    const ShaderD3D *mVertexShader;
    const ShaderD3D *mFragmentShader;
};

class ProgramD3D : public ProgramImpl
{
  public:
    ProgramD3D(const gl::ProgramState &data, RendererD3D *renderer);
    ~ProgramD3D() override;

    const std::vector<PixelShaderOutputVariable> &getPixelShaderKey() { return mPixelShaderKey; }

    GLint getSamplerMapping(gl::SamplerType type,
                            unsigned int samplerIndex,
                            const gl::Caps &caps) const;
    GLenum getSamplerTextureType(gl::SamplerType type, unsigned int samplerIndex) const;
    GLuint getUsedSamplerRange(gl::SamplerType type) const;

    enum SamplerMapping
    {
        WasDirty,
        WasClean,
    };

    SamplerMapping updateSamplerMapping();

    bool usesPointSize() const { return mUsesPointSize; }
    bool usesPointSpriteEmulation() const;
    bool usesGeometryShader(GLenum drawMode) const;
    bool usesGeometryShaderForPointSpriteEmulation() const;
    bool usesInstancedPointSpriteEmulation() const;

    gl::LinkResult load(const gl::Context *context,
                        gl::InfoLog &infoLog,
                        gl::BinaryInputStream *stream) override;
    void save(const gl::Context *context, gl::BinaryOutputStream *stream) override;
    void setBinaryRetrievableHint(bool retrievable) override;
    void setSeparable(bool separable) override;

    gl::Error getVertexExecutableForCachedInputLayout(ShaderExecutableD3D **outExectuable,
                                                      gl::InfoLog *infoLog);
    gl::Error getGeometryExecutableForPrimitiveType(const gl::Context *context,
                                                    GLenum drawMode,
                                                    ShaderExecutableD3D **outExecutable,
                                                    gl::InfoLog *infoLog);
    gl::Error getPixelExecutableForCachedOutputLayout(ShaderExecutableD3D **outExectuable,
                                                      gl::InfoLog *infoLog);
    gl::Error getComputeExecutable(ShaderExecutableD3D **outExecutable);
    gl::LinkResult link(const gl::Context *context,
                        const gl::ProgramLinkedResources &resources,
                        gl::InfoLog &infoLog) override;
    GLboolean validate(const gl::Caps &caps, gl::InfoLog *infoLog) override;

    void setPathFragmentInputGen(const std::string &inputName,
                                 GLenum genMode,
                                 GLint components,
                                 const GLfloat *coeffs) override;

    void initializeUniformStorage();
    void updateUniformBufferCache(const gl::Caps &caps,
                                  unsigned int reservedVertex,
                                  unsigned int reservedFragment);
    const std::vector<GLint> &getVertexUniformBufferCache() const;
    const std::vector<GLint> &getFragmentUniformBufferCache() const;

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

    void setUniformBlockBinding(GLuint uniformBlockIndex, GLuint uniformBlockBinding) override;

    UniformStorageD3D &getVertexUniformStorage() const { return *mVertexUniformStorage.get(); }
    UniformStorageD3D &getFragmentUniformStorage() const { return *mFragmentUniformStorage.get(); }
    UniformStorageD3D &getComputeUniformStorage() const { return *mComputeUniformStorage.get(); }

    unsigned int getSerial() const;

    const AttribIndexArray &getAttribLocationToD3DSemantics() const
    {
        return mAttribLocationToD3DSemantic;
    }

    void updateCachedInputLayout(Serial associatedSerial, const gl::State &state);
    void updateCachedOutputLayout(const gl::Context *context, const gl::Framebuffer *framebuffer);

    bool isSamplerMappingDirty() { return mDirtySamplerMapping; }

    // Checks if we need to recompile certain shaders.
    bool hasVertexExecutableForCachedInputLayout();
    bool hasGeometryExecutableForPrimitiveType(GLenum drawMode);
    bool hasPixelExecutableForCachedOutputLayout();

    bool areVertexUniformsDirty() const { return mVertexUniformsDirty; }
    bool areFragmentUniformsDirty() const { return mFragmentUniformsDirty; }
    bool areComputeUniformsDirty() const { return mComputeUniformsDirty; }
    const std::vector<D3DUniform *> &getD3DUniforms() const { return mD3DUniforms; }
    void markUniformsClean();

  private:
    // These forward-declared tasks are used for multi-thread shader compiles.
    class GetExecutableTask;
    class GetVertexExecutableTask;
    class GetPixelExecutableTask;
    class GetGeometryExecutableTask;

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

    struct Sampler
    {
        Sampler();

        bool active;
        GLint logicalTextureUnit;
        GLenum textureType;
    };

    typedef std::map<std::string, D3DUniform *> D3DUniformMap;

    void defineUniformsAndAssignRegisters(const gl::Context *context);
    void defineUniformBase(const gl::Shader *shader,
                           const sh::Uniform &uniform,
                           D3DUniformMap *uniformMap);
    void defineStructUniformFields(GLenum shaderType,
                                   const std::vector<sh::ShaderVariable> &fields,
                                   const std::string &namePrefix,
                                   sh::HLSLBlockEncoder *encoder,
                                   D3DUniformMap *uniformMap);
    void defineArrayOfStructsUniformFields(GLenum shaderType,
                                           const sh::ShaderVariable &uniform,
                                           unsigned int arrayNestingIndex,
                                           const std::string &prefix,
                                           sh::HLSLBlockEncoder *encoder,
                                           D3DUniformMap *uniformMap);
    void defineArrayUniformElements(GLenum shaderType,
                                    const sh::ShaderVariable &uniform,
                                    const std::string &fullName,
                                    sh::HLSLBlockEncoder *encoder,
                                    D3DUniformMap *uniformMap);
    void defineUniform(GLenum shaderType,
                       const sh::ShaderVariable &uniform,
                       const std::string &fullName,
                       sh::HLSLBlockEncoder *encoder,
                       D3DUniformMap *uniformMap);
    void assignAllSamplerRegisters();
    void assignSamplerRegisters(size_t uniformIndex);

    static void AssignSamplers(unsigned int startSamplerIndex,
                               const gl::UniformTypeInfo &typeInfo,
                               unsigned int samplerCount,
                               std::vector<Sampler> &outSamplers,
                               GLuint *outUsedRange);

    template <typename DestT>
    void getUniformInternal(GLint location, DestT *dataOut) const;

    template <typename T>
    void setUniformImpl(const gl::VariableLocation &locationInfo,
                        GLsizei count,
                        const T *v,
                        uint8_t *targetData,
                        GLenum uniformType);

    template <typename T>
    void setUniformInternal(GLint location, GLsizei count, const T *v, GLenum uniformType);

    template <int cols, int rows>
    bool setUniformMatrixfvImpl(GLint location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLfloat *value,
                                uint8_t *targetData,
                                GLenum targetUniformType);

    template <int cols, int rows>
    void setUniformMatrixfvInternal(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *value,
                                    GLenum targetUniformType);

    gl::LinkResult compileProgramExecutables(const gl::Context *context, gl::InfoLog &infoLog);
    gl::LinkResult compileComputeExecutable(const gl::Context *context, gl::InfoLog &infoLog);

    void gatherTransformFeedbackVaryings(const gl::VaryingPacking &varyings,
                                         const BuiltinInfo &builtins);
    D3DUniform *getD3DUniformByName(const std::string &name);
    D3DUniform *getD3DUniformFromLocation(GLint location);
    const D3DUniform *getD3DUniformFromLocation(GLint location) const;

    void initAttribLocationsToD3DSemantic(const gl::Context *context);

    void reset();
    void initializeUniformBlocks();

    void updateCachedInputLayoutFromShader(const gl::Context *context);
    void updateCachedOutputLayoutFromShader();
    void updateCachedVertexExecutableIndex();
    void updateCachedPixelExecutableIndex();

    void linkResources(const gl::Context *context, const gl::ProgramLinkedResources &resources);

    RendererD3D *mRenderer;
    DynamicHLSL *mDynamicHLSL;

    std::vector<std::unique_ptr<VertexExecutable>> mVertexExecutables;
    std::vector<std::unique_ptr<PixelExecutable>> mPixelExecutables;
    std::vector<std::unique_ptr<ShaderExecutableD3D>> mGeometryExecutables;
    std::unique_ptr<ShaderExecutableD3D> mComputeExecutable;

    std::string mVertexHLSL;
    angle::CompilerWorkaroundsD3D mVertexWorkarounds;

    std::string mPixelHLSL;
    angle::CompilerWorkaroundsD3D mPixelWorkarounds;
    bool mUsesFragDepth;
    bool mHasANGLEMultiviewEnabled;
    bool mUsesViewID;
    std::vector<PixelShaderOutputVariable> mPixelShaderKey;

    // Common code for all dynamic geometry shaders. Consists mainly of the GS input and output
    // structures, built from the linked varying info. We store the string itself instead of the
    // packed varyings for simplicity.
    std::string mGeometryShaderPreamble;

    bool mUsesPointSize;
    bool mUsesFlatInterpolation;

    std::unique_ptr<UniformStorageD3D> mVertexUniformStorage;
    std::unique_ptr<UniformStorageD3D> mFragmentUniformStorage;
    std::unique_ptr<UniformStorageD3D> mComputeUniformStorage;

    std::vector<Sampler> mSamplersPS;
    std::vector<Sampler> mSamplersVS;
    std::vector<Sampler> mSamplersCS;
    GLuint mUsedVertexSamplerRange;
    GLuint mUsedPixelSamplerRange;
    GLuint mUsedComputeSamplerRange;
    bool mDirtySamplerMapping;

    // Cache for pixel shader output layout to save reallocations.
    std::vector<GLenum> mPixelShaderOutputLayoutCache;
    Optional<size_t> mCachedPixelExecutableIndex;

    AttribIndexArray mAttribLocationToD3DSemantic;

    unsigned int mSerial;

    std::vector<GLint> mVertexUBOCache;
    std::vector<GLint> mFragmentUBOCache;
    VertexExecutable::Signature mCachedVertexSignature;
    gl::InputLayout mCachedInputLayout;
    Optional<size_t> mCachedVertexExecutableIndex;

    std::vector<D3DVarying> mStreamOutVaryings;
    std::vector<D3DUniform *> mD3DUniforms;
    std::vector<D3DUniformBlock> mD3DUniformBlocks;

    bool mVertexUniformsDirty;
    bool mFragmentUniformsDirty;
    bool mComputeUniformsDirty;

    static unsigned int issueSerial();
    static unsigned int mCurrentSerial;

    Serial mCurrentVertexArrayStateSerial;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_D3D_PROGRAMD3D_H_
