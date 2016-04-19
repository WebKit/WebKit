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
#include "libANGLE/renderer/d3d/WorkaroundsD3D.h"

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
struct D3DUniform : angle::NonCopyable
{
    D3DUniform(GLenum typeIn,
               const std::string &nameIn,
               unsigned int arraySizeIn,
               bool defaultBlock);
    ~D3DUniform();

    bool isSampler() const;
    unsigned int elementCount() const { return std::max(1u, arraySize); }
    bool isReferencedByVertexShader() const;
    bool isReferencedByFragmentShader() const;

    // Duplicated from the GL layer
    GLenum type;
    std::string name;
    unsigned int arraySize;

    // Pointer to a system copy of the data.
    // TODO(jmadill): remove this in favor of gl::LinkedUniform::data().
    uint8_t *data;

    // Has the data been updated since the last sync?
    bool dirty;

    // Register information.
    unsigned int vsRegisterIndex;
    unsigned int psRegisterIndex;
    unsigned int registerCount;

    // Register "elements" are used for uniform structs in ES3, to appropriately identify single
    // uniforms
    // inside aggregate types, which are packed according C-like structure rules.
    unsigned int registerElement;
};

struct D3DUniformBlock
{
    D3DUniformBlock() : vsRegisterIndex(GL_INVALID_INDEX), psRegisterIndex(GL_INVALID_INDEX) {}

    bool vertexStaticUse() const { return vsRegisterIndex != GL_INVALID_INDEX; }

    bool fragmentStaticUse() const { return psRegisterIndex != GL_INVALID_INDEX; }

    unsigned int vsRegisterIndex;
    unsigned int psRegisterIndex;
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

class ProgramD3DMetadata : angle::NonCopyable
{
  public:
    ProgramD3DMetadata(int rendererMajorShaderModel,
                       const std::string &shaderModelSuffix,
                       bool usesInstancedPointSpriteEmulation,
                       bool usesViewScale,
                       const ShaderD3D *vertexShader,
                       const ShaderD3D *fragmentShader);

    int getRendererMajorShaderModel() const;
    bool usesBroadcast(const gl::Data &data) const;
    bool usesFragDepth(const gl::Program::Data &programData) const;
    bool usesPointCoord() const;
    bool usesFragCoord() const;
    bool usesPointSize() const;
    bool usesInsertedPointCoordValue() const;
    bool usesViewScale() const;
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
    const ShaderD3D *mVertexShader;
    const ShaderD3D *mFragmentShader;
};

class ProgramD3D : public ProgramImpl
{
  public:
    ProgramD3D(const gl::Program::Data &data, RendererD3D *renderer);
    virtual ~ProgramD3D();

    const std::vector<PixelShaderOutputVariable> &getPixelShaderKey() { return mPixelShaderKey; }

    GLint getSamplerMapping(gl::SamplerType type,
                            unsigned int samplerIndex,
                            const gl::Caps &caps) const;
    GLenum getSamplerTextureType(gl::SamplerType type, unsigned int samplerIndex) const;
    GLuint getUsedSamplerRange(gl::SamplerType type) const;
    void updateSamplerMapping();

    bool usesPointSize() const { return mUsesPointSize; }
    bool usesPointSpriteEmulation() const;
    bool usesGeometryShader(GLenum drawMode) const;
    bool usesInstancedPointSpriteEmulation() const;

    LinkResult load(gl::InfoLog &infoLog, gl::BinaryInputStream *stream) override;
    gl::Error save(gl::BinaryOutputStream *stream) override;
    void setBinaryRetrievableHint(bool retrievable) override;

    gl::Error getPixelExecutableForFramebuffer(const gl::Framebuffer *fbo,
                                               ShaderExecutableD3D **outExectuable);
    gl::Error getPixelExecutableForOutputLayout(const std::vector<GLenum> &outputLayout,
                                                ShaderExecutableD3D **outExectuable,
                                                gl::InfoLog *infoLog);
    gl::Error getVertexExecutableForInputLayout(const gl::InputLayout &inputLayout,
                                                ShaderExecutableD3D **outExectuable,
                                                gl::InfoLog *infoLog);
    gl::Error getGeometryExecutableForPrimitiveType(const gl::Data &data,
                                                    GLenum drawMode,
                                                    ShaderExecutableD3D **outExecutable,
                                                    gl::InfoLog *infoLog);

    LinkResult link(const gl::Data &data, gl::InfoLog &infoLog) override;
    GLboolean validate(const gl::Caps &caps, gl::InfoLog *infoLog) override;

    bool getUniformBlockSize(const std::string &blockName, size_t *sizeOut) const override;
    bool getUniformBlockMemberInfo(const std::string &memberUniformName,
                                   sh::BlockMemberInfo *memberInfoOut) const override;

    void initializeUniformStorage();
    gl::Error applyUniforms(GLenum drawMode);
    gl::Error applyUniformBuffers(const gl::Data &data);
    void dirtyAllUniforms();

    void setUniform1fv(GLint location, GLsizei count, const GLfloat *v);
    void setUniform2fv(GLint location, GLsizei count, const GLfloat *v);
    void setUniform3fv(GLint location, GLsizei count, const GLfloat *v);
    void setUniform4fv(GLint location, GLsizei count, const GLfloat *v);
    void setUniform1iv(GLint location, GLsizei count, const GLint *v);
    void setUniform2iv(GLint location, GLsizei count, const GLint *v);
    void setUniform3iv(GLint location, GLsizei count, const GLint *v);
    void setUniform4iv(GLint location, GLsizei count, const GLint *v);
    void setUniform1uiv(GLint location, GLsizei count, const GLuint *v);
    void setUniform2uiv(GLint location, GLsizei count, const GLuint *v);
    void setUniform3uiv(GLint location, GLsizei count, const GLuint *v);
    void setUniform4uiv(GLint location, GLsizei count, const GLuint *v);
    void setUniformMatrix2fv(GLint location,
                             GLsizei count,
                             GLboolean transpose,
                             const GLfloat *value);
    void setUniformMatrix3fv(GLint location,
                             GLsizei count,
                             GLboolean transpose,
                             const GLfloat *value);
    void setUniformMatrix4fv(GLint location,
                             GLsizei count,
                             GLboolean transpose,
                             const GLfloat *value);
    void setUniformMatrix2x3fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value);
    void setUniformMatrix3x2fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value);
    void setUniformMatrix2x4fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value);
    void setUniformMatrix4x2fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value);
    void setUniformMatrix3x4fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value);
    void setUniformMatrix4x3fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value);

    void setUniformBlockBinding(GLuint uniformBlockIndex, GLuint uniformBlockBinding) override;

    const UniformStorageD3D &getVertexUniformStorage() const { return *mVertexUniformStorage; }
    const UniformStorageD3D &getFragmentUniformStorage() const { return *mFragmentUniformStorage; }

    unsigned int getSerial() const;

    const AttribIndexArray &getAttribLocationToD3DSemantics() const
    {
        return mAttribLocationToD3DSemantic;
    }

    void updateCachedInputLayout(const gl::State &state);
    const gl::InputLayout &getCachedInputLayout() const { return mCachedInputLayout; }

    bool isSamplerMappingDirty() { return mDirtySamplerMapping; }

  private:
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

    void defineUniformsAndAssignRegisters();
    void defineUniformBase(const gl::Shader *shader,
                           const sh::Uniform &uniform,
                           D3DUniformMap *uniformMap);
    void defineUniform(GLenum shaderType,
                       const sh::ShaderVariable &uniform,
                       const std::string &fullName,
                       sh::HLSLBlockEncoder *encoder,
                       D3DUniformMap *uniformMap);
    void assignAllSamplerRegisters();
    void assignSamplerRegisters(D3DUniform *d3dUniform);

    static void AssignSamplers(unsigned int startSamplerIndex,
                               GLenum samplerType,
                               unsigned int samplerCount,
                               std::vector<Sampler> &outSamplers,
                               GLuint *outUsedRange);

    template <typename T>
    void setUniform(GLint location, GLsizei count, const T *v, GLenum targetUniformType);

    template <int cols, int rows>
    void setUniformMatrixfv(GLint location,
                            GLsizei count,
                            GLboolean transpose,
                            const GLfloat *value,
                            GLenum targetUniformType);

    LinkResult compileProgramExecutables(const gl::Data &data, gl::InfoLog &infoLog);

    void gatherTransformFeedbackVaryings(const VaryingPacking &varyings);
    D3DUniform *getD3DUniformByName(const std::string &name);
    D3DUniform *getD3DUniformFromLocation(GLint location);

    void initAttribLocationsToD3DSemantic();

    void reset();
    void assignUniformBlockRegisters();

    void initUniformBlockInfo();
    size_t getUniformBlockInfo(const sh::InterfaceBlock &interfaceBlock);

    RendererD3D *mRenderer;
    DynamicHLSL *mDynamicHLSL;

    std::vector<VertexExecutable *> mVertexExecutables;
    std::vector<PixelExecutable *> mPixelExecutables;
    std::vector<ShaderExecutableD3D *> mGeometryExecutables;

    std::string mVertexHLSL;
    D3DCompilerWorkarounds mVertexWorkarounds;

    std::string mPixelHLSL;
    D3DCompilerWorkarounds mPixelWorkarounds;
    bool mUsesFragDepth;
    std::vector<PixelShaderOutputVariable> mPixelShaderKey;

    // Common code for all dynamic geometry shaders. Consists mainly of the GS input and output
    // structures, built from the linked varying info. We store the string itself instead of the
    // packed varyings for simplicity.
    std::string mGeometryShaderPreamble;

    bool mUsesPointSize;
    bool mUsesFlatInterpolation;

    UniformStorageD3D *mVertexUniformStorage;
    UniformStorageD3D *mFragmentUniformStorage;

    std::vector<Sampler> mSamplersPS;
    std::vector<Sampler> mSamplersVS;
    GLuint mUsedVertexSamplerRange;
    GLuint mUsedPixelSamplerRange;
    bool mDirtySamplerMapping;

    // Cache for getPixelExecutableForFramebuffer
    std::vector<GLenum> mPixelShaderOutputFormatCache;

    AttribIndexArray mAttribLocationToD3DSemantic;

    unsigned int mSerial;

    std::vector<GLint> mVertexUBOCache;
    std::vector<GLint> mFragmentUBOCache;
    VertexExecutable::Signature mCachedVertexSignature;
    gl::InputLayout mCachedInputLayout;

    std::vector<D3DVarying> mStreamOutVaryings;
    std::vector<D3DUniform *> mD3DUniforms;
    std::vector<D3DUniformBlock> mD3DUniformBlocks;

    std::map<std::string, sh::BlockMemberInfo> mBlockInfo;
    std::map<std::string, size_t> mBlockDataSizes;

    static unsigned int issueSerial();
    static unsigned int mCurrentSerial;
};
}

#endif  // LIBANGLE_RENDERER_D3D_PROGRAMD3D_H_
