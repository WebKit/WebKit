//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramD3D.cpp: Defines the rx::ProgramD3D class which implements rx::ProgramImpl.

#include "libANGLE/renderer/d3d/ProgramD3D.h"

#include "common/bitset_utils.h"
#include "common/string_utils.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/Program.h"
#include "libANGLE/ProgramLinkedResources.h"
#include "libANGLE/Uniform.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/features.h"
#include "libANGLE/queryconversions.h"
#include "libANGLE/renderer/ContextImpl.h"
#include "libANGLE/renderer/d3d/DynamicHLSL.h"
#include "libANGLE/renderer/d3d/FramebufferD3D.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"
#include "libANGLE/renderer/d3d/ShaderD3D.h"
#include "libANGLE/renderer/d3d/ShaderExecutableD3D.h"
#include "libANGLE/renderer/d3d/VertexDataManager.h"

using namespace angle;

namespace rx
{

namespace
{

void GetDefaultInputLayoutFromShader(const gl::Context *context,
                                     gl::Shader *vertexShader,
                                     gl::InputLayout *inputLayoutOut)
{
    inputLayoutOut->clear();

    for (const sh::Attribute &shaderAttr : vertexShader->getActiveAttributes(context))
    {
        if (shaderAttr.type != GL_NONE)
        {
            GLenum transposedType = gl::TransposeMatrixType(shaderAttr.type);

            for (size_t rowIndex = 0;
                 static_cast<int>(rowIndex) < gl::VariableRowCount(transposedType); ++rowIndex)
            {
                GLenum componentType = gl::VariableComponentType(transposedType);
                GLuint components    = static_cast<GLuint>(gl::VariableColumnCount(transposedType));
                bool pureInt = (componentType != GL_FLOAT);
                gl::VertexFormatType defaultType =
                    gl::GetVertexFormatType(componentType, GL_FALSE, components, pureInt);

                inputLayoutOut->push_back(defaultType);
            }
        }
    }
}

void GetDefaultOutputLayoutFromShader(
    const std::vector<PixelShaderOutputVariable> &shaderOutputVars,
    std::vector<GLenum> *outputLayoutOut)
{
    outputLayoutOut->clear();

    if (!shaderOutputVars.empty())
    {
        outputLayoutOut->push_back(GL_COLOR_ATTACHMENT0 +
                                   static_cast<unsigned int>(shaderOutputVars[0].outputIndex));
    }
}

template <typename T, int cols, int rows>
bool TransposeExpandMatrix(T *target, const GLfloat *value)
{
    constexpr int targetWidth  = 4;
    constexpr int targetHeight = rows;
    constexpr int srcWidth     = rows;
    constexpr int srcHeight    = cols;

    constexpr int copyWidth  = std::min(targetHeight, srcWidth);
    constexpr int copyHeight = std::min(targetWidth, srcHeight);

    T staging[targetWidth * targetHeight] = {0};

    for (int x = 0; x < copyWidth; x++)
    {
        for (int y = 0; y < copyHeight; y++)
        {
            staging[x * targetWidth + y] = static_cast<T>(value[y * srcWidth + x]);
        }
    }

    if (memcmp(target, staging, targetWidth * targetHeight * sizeof(T)) == 0)
    {
        return false;
    }

    memcpy(target, staging, targetWidth * targetHeight * sizeof(T));
    return true;
}

template <typename T, int cols, int rows>
bool ExpandMatrix(T *target, const GLfloat *value)
{
    constexpr int targetWidth  = 4;
    constexpr int targetHeight = rows;
    constexpr int srcWidth = cols;
    constexpr int srcHeight = rows;

    constexpr int copyWidth  = std::min(targetWidth, srcWidth);
    constexpr int copyHeight = std::min(targetHeight, srcHeight);

    T staging[targetWidth * targetHeight] = {0};

    for (int y = 0; y < copyHeight; y++)
    {
        for (int x = 0; x < copyWidth; x++)
        {
            staging[y * targetWidth + x] = static_cast<T>(value[y * srcWidth + x]);
        }
    }

    if (memcmp(target, staging, targetWidth * targetHeight * sizeof(T)) == 0)
    {
        return false;
    }

    memcpy(target, staging, targetWidth * targetHeight * sizeof(T));
    return true;
}

gl::PrimitiveType GetGeometryShaderTypeFromDrawMode(GLenum drawMode)
{
    switch (drawMode)
    {
        // Uses the point sprite geometry shader.
        case GL_POINTS:
            return gl::PRIMITIVE_POINTS;

        // All line drawing uses the same geometry shader.
        case GL_LINES:
        case GL_LINE_STRIP:
        case GL_LINE_LOOP:
            return gl::PRIMITIVE_LINES;

        // The triangle fan primitive is emulated with strips in D3D11.
        case GL_TRIANGLES:
        case GL_TRIANGLE_FAN:
            return gl::PRIMITIVE_TRIANGLES;

        // Special case for triangle strips.
        case GL_TRIANGLE_STRIP:
            return gl::PRIMITIVE_TRIANGLE_STRIP;

        default:
            UNREACHABLE();
            return gl::PRIMITIVE_TYPE_MAX;
    }
}

bool FindFlatInterpolationVarying(const std::vector<sh::Varying> &varyings)
{
    // Note: this assumes nested structs can only be packed with one interpolation.
    for (const auto &varying : varyings)
    {
        if (varying.interpolation == sh::INTERPOLATION_FLAT)
        {
            return true;
        }
    }

    return false;
}

// Helper method to de-tranpose a matrix uniform for an API query.
void GetMatrixUniform(GLint columns, GLint rows, GLfloat *dataOut, const GLfloat *source)
{
    for (GLint col = 0; col < columns; ++col)
    {
        for (GLint row = 0; row < rows; ++row)
        {
            GLfloat *outptr      = dataOut + ((col * rows) + row);
            const GLfloat *inptr = source + ((row * 4) + col);
            *outptr              = *inptr;
        }
    }
}

template <typename NonFloatT>
void GetMatrixUniform(GLint columns, GLint rows, NonFloatT *dataOut, const NonFloatT *source)
{
    UNREACHABLE();
}

class UniformBlockInfo final : angle::NonCopyable
{
  public:
    UniformBlockInfo() {}

    void getShaderBlockInfo(const gl::Context *context, gl::Shader *shader);

    bool getBlockSize(const std::string &name, const std::string &mappedName, size_t *sizeOut);
    bool getBlockMemberInfo(const std::string &name,
                            const std::string &mappedName,
                            sh::BlockMemberInfo *infoOut);

  private:
    size_t getBlockInfo(const sh::InterfaceBlock &interfaceBlock);

    std::map<std::string, size_t> mBlockSizes;
    sh::BlockLayoutMap mBlockLayout;
};

void UniformBlockInfo::getShaderBlockInfo(const gl::Context *context, gl::Shader *shader)
{
    for (const sh::InterfaceBlock &interfaceBlock : shader->getUniformBlocks(context))
    {
        if (!interfaceBlock.staticUse && interfaceBlock.layout == sh::BLOCKLAYOUT_PACKED)
            continue;

        if (mBlockSizes.count(interfaceBlock.name) > 0)
            continue;

        size_t dataSize                  = getBlockInfo(interfaceBlock);
        mBlockSizes[interfaceBlock.name] = dataSize;
    }
}

size_t UniformBlockInfo::getBlockInfo(const sh::InterfaceBlock &interfaceBlock)
{
    ASSERT(interfaceBlock.staticUse || interfaceBlock.layout != sh::BLOCKLAYOUT_PACKED);

    // define member uniforms
    sh::Std140BlockEncoder std140Encoder;
    sh::HLSLBlockEncoder hlslEncoder(sh::HLSLBlockEncoder::ENCODE_PACKED, false);
    sh::BlockLayoutEncoder *encoder = nullptr;

    if (interfaceBlock.layout == sh::BLOCKLAYOUT_STD140)
    {
        encoder = &std140Encoder;
    }
    else
    {
        encoder = &hlslEncoder;
    }

    sh::GetUniformBlockInfo(interfaceBlock.fields, interfaceBlock.fieldPrefix(), encoder,
                            interfaceBlock.isRowMajorLayout, &mBlockLayout);

    return encoder->getBlockSize();
}

bool UniformBlockInfo::getBlockSize(const std::string &name,
                                    const std::string &mappedName,
                                    size_t *sizeOut)
{
    size_t nameLengthWithoutArrayIndex;
    gl::ParseArrayIndex(name, &nameLengthWithoutArrayIndex);
    std::string baseName = name.substr(0u, nameLengthWithoutArrayIndex);
    auto sizeIter        = mBlockSizes.find(baseName);
    if (sizeIter == mBlockSizes.end())
    {
        *sizeOut = 0;
        return false;
    }

    *sizeOut = sizeIter->second;
    return true;
};

bool UniformBlockInfo::getBlockMemberInfo(const std::string &name,
                                          const std::string &mappedName,
                                          sh::BlockMemberInfo *infoOut)
{
    auto infoIter = mBlockLayout.find(name);
    if (infoIter == mBlockLayout.end())
    {
        *infoOut = sh::BlockMemberInfo::getDefaultBlockInfo();
        return false;
    }

    *infoOut = infoIter->second;
    return true;
};

}  // anonymous namespace

// D3DUniform Implementation

D3DUniform::D3DUniform(GLenum type,
                       const std::string &nameIn,
                       const std::vector<unsigned int> &arraySizesIn,
                       bool defaultBlock)
    : typeInfo(gl::GetUniformTypeInfo(type)),
      name(nameIn),
      arraySizes(arraySizesIn),
      vsData(nullptr),
      psData(nullptr),
      csData(nullptr),
      vsRegisterIndex(GL_INVALID_INDEX),
      psRegisterIndex(GL_INVALID_INDEX),
      csRegisterIndex(GL_INVALID_INDEX),
      registerCount(0),
      registerElement(0)
{
    // We use data storage for default block uniforms to cache values that are sent to D3D during
    // rendering
    // Uniform blocks/buffers are treated separately by the Renderer (ES3 path only)
    if (defaultBlock)
    {
        // Use the row count as register count, will work for non-square matrices.
        registerCount = typeInfo.rowCount * getArraySizeProduct();
    }
}

D3DUniform::~D3DUniform()
{
}

unsigned int D3DUniform::getArraySizeProduct() const
{
    return gl::ArraySizeProduct(arraySizes);
}

const uint8_t *D3DUniform::getDataPtrToElement(size_t elementIndex) const
{
    ASSERT((!isArray() && elementIndex == 0) ||
           (isArray() && elementIndex < getArraySizeProduct()));

    if (isSampler())
    {
        return reinterpret_cast<const uint8_t *>(&mSamplerData[elementIndex]);
    }

    return firstNonNullData() + (elementIndex > 0 ? (typeInfo.internalSize * elementIndex) : 0u);
}

bool D3DUniform::isSampler() const
{
    return typeInfo.isSampler;
}

bool D3DUniform::isReferencedByVertexShader() const
{
    return vsRegisterIndex != GL_INVALID_INDEX;
}

bool D3DUniform::isReferencedByFragmentShader() const
{
    return psRegisterIndex != GL_INVALID_INDEX;
}

bool D3DUniform::isReferencedByComputeShader() const
{
    return csRegisterIndex != GL_INVALID_INDEX;
}

const uint8_t *D3DUniform::firstNonNullData() const
{
    ASSERT(vsData || psData || csData || !mSamplerData.empty());

    if (!mSamplerData.empty())
    {
        return reinterpret_cast<const uint8_t *>(mSamplerData.data());
    }

    return vsData ? vsData : (psData ? psData : csData);
}

// D3DVarying Implementation

D3DVarying::D3DVarying() : semanticIndex(0), componentCount(0), outputSlot(0)
{
}

D3DVarying::D3DVarying(const std::string &semanticNameIn,
                       unsigned int semanticIndexIn,
                       unsigned int componentCountIn,
                       unsigned int outputSlotIn)
    : semanticName(semanticNameIn),
      semanticIndex(semanticIndexIn),
      componentCount(componentCountIn),
      outputSlot(outputSlotIn)
{
}

// ProgramD3DMetadata Implementation

ProgramD3DMetadata::ProgramD3DMetadata(RendererD3D *renderer,
                                       const ShaderD3D *vertexShader,
                                       const ShaderD3D *fragmentShader)
    : mRendererMajorShaderModel(renderer->getMajorShaderModel()),
      mShaderModelSuffix(renderer->getShaderModelSuffix()),
      mUsesInstancedPointSpriteEmulation(
          renderer->getWorkarounds().useInstancedPointSpriteEmulation),
      mUsesViewScale(renderer->presentPathFastEnabled()),
      mHasANGLEMultiviewEnabled(vertexShader->hasANGLEMultiviewEnabled()),
      mUsesViewID(fragmentShader->usesViewID()),
      mCanSelectViewInVertexShader(renderer->canSelectViewInVertexShader()),
      mVertexShader(vertexShader),
      mFragmentShader(fragmentShader)
{
}

int ProgramD3DMetadata::getRendererMajorShaderModel() const
{
    return mRendererMajorShaderModel;
}

bool ProgramD3DMetadata::usesBroadcast(const gl::ContextState &data) const
{
    return (mFragmentShader->usesFragColor() && mFragmentShader->usesMultipleRenderTargets() &&
            data.getClientMajorVersion() < 3);
}

bool ProgramD3DMetadata::usesFragDepth() const
{
    return mFragmentShader->usesFragDepth();
}

bool ProgramD3DMetadata::usesPointCoord() const
{
    return mFragmentShader->usesPointCoord();
}

bool ProgramD3DMetadata::usesFragCoord() const
{
    return mFragmentShader->usesFragCoord();
}

bool ProgramD3DMetadata::usesPointSize() const
{
    return mVertexShader->usesPointSize();
}

bool ProgramD3DMetadata::usesInsertedPointCoordValue() const
{
    return (!usesPointSize() || !mUsesInstancedPointSpriteEmulation) && usesPointCoord() &&
           mRendererMajorShaderModel >= 4;
}

bool ProgramD3DMetadata::usesViewScale() const
{
    return mUsesViewScale;
}

bool ProgramD3DMetadata::hasANGLEMultiviewEnabled() const
{
    return mHasANGLEMultiviewEnabled;
}

bool ProgramD3DMetadata::usesViewID() const
{
    return mUsesViewID;
}

bool ProgramD3DMetadata::canSelectViewInVertexShader() const
{
    return mCanSelectViewInVertexShader;
}

bool ProgramD3DMetadata::addsPointCoordToVertexShader() const
{
    // PointSprite emulation requiress that gl_PointCoord is present in the vertex shader
    // VS_OUTPUT structure to ensure compatibility with the generated PS_INPUT of the pixel shader.
    // Even with a geometry shader, the app can render triangles or lines and reference
    // gl_PointCoord in the fragment shader, requiring us to provide a dummy value. For
    // simplicity, we always add this to the vertex shader when the fragment shader
    // references gl_PointCoord, even if we could skip it in the geometry shader.
    return (mUsesInstancedPointSpriteEmulation && usesPointCoord()) ||
           usesInsertedPointCoordValue();
}

bool ProgramD3DMetadata::usesTransformFeedbackGLPosition() const
{
    // gl_Position only needs to be outputted from the vertex shader if transform feedback is
    // active. This isn't supported on D3D11 Feature Level 9_3, so we don't output gl_Position from
    // the vertex shader in this case. This saves us 1 output vector.
    return !(mRendererMajorShaderModel >= 4 && mShaderModelSuffix != "");
}

bool ProgramD3DMetadata::usesSystemValuePointSize() const
{
    return !mUsesInstancedPointSpriteEmulation && usesPointSize();
}

bool ProgramD3DMetadata::usesMultipleFragmentOuts() const
{
    return mFragmentShader->usesMultipleRenderTargets();
}

GLint ProgramD3DMetadata::getMajorShaderVersion() const
{
    return mVertexShader->getData().getShaderVersion();
}

const ShaderD3D *ProgramD3DMetadata::getFragmentShader() const
{
    return mFragmentShader;
}

// ProgramD3D Implementation

ProgramD3D::VertexExecutable::VertexExecutable(const gl::InputLayout &inputLayout,
                                               const Signature &signature,
                                               ShaderExecutableD3D *shaderExecutable)
    : mInputs(inputLayout), mSignature(signature), mShaderExecutable(shaderExecutable)
{
}

ProgramD3D::VertexExecutable::~VertexExecutable()
{
    SafeDelete(mShaderExecutable);
}

// static
ProgramD3D::VertexExecutable::HLSLAttribType ProgramD3D::VertexExecutable::GetAttribType(
    GLenum type)
{
    switch (type)
    {
        case GL_INT:
            return HLSLAttribType::SIGNED_INT;
        case GL_UNSIGNED_INT:
            return HLSLAttribType::UNSIGNED_INT;
        case GL_SIGNED_NORMALIZED:
        case GL_UNSIGNED_NORMALIZED:
        case GL_FLOAT:
            return HLSLAttribType::FLOAT;
        default:
            UNREACHABLE();
            return HLSLAttribType::FLOAT;
    }
}

// static
void ProgramD3D::VertexExecutable::getSignature(RendererD3D *renderer,
                                                const gl::InputLayout &inputLayout,
                                                Signature *signatureOut)
{
    signatureOut->assign(inputLayout.size(), HLSLAttribType::FLOAT);

    for (size_t index = 0; index < inputLayout.size(); ++index)
    {
        gl::VertexFormatType vertexFormatType = inputLayout[index];
        if (vertexFormatType == gl::VERTEX_FORMAT_INVALID)
            continue;

        VertexConversionType conversionType = renderer->getVertexConversionType(vertexFormatType);
        if ((conversionType & VERTEX_CONVERT_GPU) == 0)
            continue;

        GLenum componentType = renderer->getVertexComponentType(vertexFormatType);
        (*signatureOut)[index] = GetAttribType(componentType);
    }
}

bool ProgramD3D::VertexExecutable::matchesSignature(const Signature &signature) const
{
    size_t limit = std::max(mSignature.size(), signature.size());
    for (size_t index = 0; index < limit; ++index)
    {
        // treat undefined indexes as FLOAT
        auto a = index < signature.size() ? signature[index] : HLSLAttribType::FLOAT;
        auto b = index < mSignature.size() ? mSignature[index] : HLSLAttribType::FLOAT;
        if (a != b)
            return false;
    }

    return true;
}

ProgramD3D::PixelExecutable::PixelExecutable(const std::vector<GLenum> &outputSignature,
                                             ShaderExecutableD3D *shaderExecutable)
    : mOutputSignature(outputSignature), mShaderExecutable(shaderExecutable)
{
}

ProgramD3D::PixelExecutable::~PixelExecutable()
{
    SafeDelete(mShaderExecutable);
}

ProgramD3D::Sampler::Sampler() : active(false), logicalTextureUnit(0), textureType(GL_TEXTURE_2D)
{
}

unsigned int ProgramD3D::mCurrentSerial = 1;

ProgramD3D::ProgramD3D(const gl::ProgramState &state, RendererD3D *renderer)
    : ProgramImpl(state),
      mRenderer(renderer),
      mDynamicHLSL(nullptr),
      mGeometryExecutables(gl::PRIMITIVE_TYPE_MAX),
      mComputeExecutable(nullptr),
      mUsesPointSize(false),
      mUsesFlatInterpolation(false),
      mVertexUniformStorage(nullptr),
      mFragmentUniformStorage(nullptr),
      mComputeUniformStorage(nullptr),
      mUsedVertexSamplerRange(0),
      mUsedPixelSamplerRange(0),
      mUsedComputeSamplerRange(0),
      mDirtySamplerMapping(true),
      mSerial(issueSerial()),
      mVertexUniformsDirty(true),
      mFragmentUniformsDirty(true),
      mComputeUniformsDirty(true)
{
    mDynamicHLSL = new DynamicHLSL(renderer);
}

ProgramD3D::~ProgramD3D()
{
    reset();
    SafeDelete(mDynamicHLSL);
}

bool ProgramD3D::usesPointSpriteEmulation() const
{
    return mUsesPointSize && mRenderer->getMajorShaderModel() >= 4;
}

bool ProgramD3D::usesGeometryShaderForPointSpriteEmulation() const
{
    return usesPointSpriteEmulation() && !usesInstancedPointSpriteEmulation();
}

bool ProgramD3D::usesGeometryShader(GLenum drawMode) const
{
    if (mHasANGLEMultiviewEnabled && !mRenderer->canSelectViewInVertexShader())
    {
        return true;
    }
    if (drawMode != GL_POINTS)
    {
        return mUsesFlatInterpolation;
    }
    return usesGeometryShaderForPointSpriteEmulation();
}

bool ProgramD3D::usesInstancedPointSpriteEmulation() const
{
    return mRenderer->getWorkarounds().useInstancedPointSpriteEmulation;
}

GLint ProgramD3D::getSamplerMapping(gl::SamplerType type,
                                    unsigned int samplerIndex,
                                    const gl::Caps &caps) const
{
    GLint logicalTextureUnit = -1;

    switch (type)
    {
        case gl::SAMPLER_PIXEL:
            ASSERT(samplerIndex < caps.maxTextureImageUnits);
            if (samplerIndex < mSamplersPS.size() && mSamplersPS[samplerIndex].active)
            {
                logicalTextureUnit = mSamplersPS[samplerIndex].logicalTextureUnit;
            }
            break;
        case gl::SAMPLER_VERTEX:
            ASSERT(samplerIndex < caps.maxVertexTextureImageUnits);
            if (samplerIndex < mSamplersVS.size() && mSamplersVS[samplerIndex].active)
            {
                logicalTextureUnit = mSamplersVS[samplerIndex].logicalTextureUnit;
            }
            break;
        case gl::SAMPLER_COMPUTE:
            ASSERT(samplerIndex < caps.maxComputeTextureImageUnits);
            if (samplerIndex < mSamplersCS.size() && mSamplersCS[samplerIndex].active)
            {
                logicalTextureUnit = mSamplersCS[samplerIndex].logicalTextureUnit;
            }
            break;
        default:
            UNREACHABLE();
    }

    if (logicalTextureUnit >= 0 &&
        logicalTextureUnit < static_cast<GLint>(caps.maxCombinedTextureImageUnits))
    {
        return logicalTextureUnit;
    }

    return -1;
}

// Returns the texture type for a given Direct3D 9 sampler type and
// index (0-15 for the pixel shader and 0-3 for the vertex shader).
GLenum ProgramD3D::getSamplerTextureType(gl::SamplerType type, unsigned int samplerIndex) const
{
    switch (type)
    {
        case gl::SAMPLER_PIXEL:
            ASSERT(samplerIndex < mSamplersPS.size());
            ASSERT(mSamplersPS[samplerIndex].active);
            return mSamplersPS[samplerIndex].textureType;
        case gl::SAMPLER_VERTEX:
            ASSERT(samplerIndex < mSamplersVS.size());
            ASSERT(mSamplersVS[samplerIndex].active);
            return mSamplersVS[samplerIndex].textureType;
        case gl::SAMPLER_COMPUTE:
            ASSERT(samplerIndex < mSamplersCS.size());
            ASSERT(mSamplersCS[samplerIndex].active);
            return mSamplersCS[samplerIndex].textureType;
        default:
            UNREACHABLE();
    }

    return GL_TEXTURE_2D;
}

GLuint ProgramD3D::getUsedSamplerRange(gl::SamplerType type) const
{
    switch (type)
    {
        case gl::SAMPLER_PIXEL:
            return mUsedPixelSamplerRange;
        case gl::SAMPLER_VERTEX:
            return mUsedVertexSamplerRange;
        case gl::SAMPLER_COMPUTE:
            return mUsedComputeSamplerRange;
        default:
            UNREACHABLE();
            return 0u;
    }
}

ProgramD3D::SamplerMapping ProgramD3D::updateSamplerMapping()
{
    if (!mDirtySamplerMapping)
    {
        return SamplerMapping::WasClean;
    }

    mDirtySamplerMapping = false;

    // Retrieve sampler uniform values
    for (const D3DUniform *d3dUniform : mD3DUniforms)
    {
        if (!d3dUniform->isSampler())
            continue;

        int count = d3dUniform->getArraySizeProduct();

        if (d3dUniform->isReferencedByFragmentShader())
        {
            unsigned int firstIndex = d3dUniform->psRegisterIndex;

            for (int i = 0; i < count; i++)
            {
                unsigned int samplerIndex = firstIndex + i;

                if (samplerIndex < mSamplersPS.size())
                {
                    ASSERT(mSamplersPS[samplerIndex].active);
                    mSamplersPS[samplerIndex].logicalTextureUnit = d3dUniform->mSamplerData[i];
                }
            }
        }

        if (d3dUniform->isReferencedByVertexShader())
        {
            unsigned int firstIndex = d3dUniform->vsRegisterIndex;

            for (int i = 0; i < count; i++)
            {
                unsigned int samplerIndex = firstIndex + i;

                if (samplerIndex < mSamplersVS.size())
                {
                    ASSERT(mSamplersVS[samplerIndex].active);
                    mSamplersVS[samplerIndex].logicalTextureUnit = d3dUniform->mSamplerData[i];
                }
            }
        }

        if (d3dUniform->isReferencedByComputeShader())
        {
            unsigned int firstIndex = d3dUniform->csRegisterIndex;

            for (int i = 0; i < count; i++)
            {
                unsigned int samplerIndex = firstIndex + i;

                if (samplerIndex < mSamplersCS.size())
                {
                    ASSERT(mSamplersCS[samplerIndex].active);
                    mSamplersCS[samplerIndex].logicalTextureUnit = d3dUniform->mSamplerData[i];
                }
            }
        }
    }

    return SamplerMapping::WasDirty;
}

gl::LinkResult ProgramD3D::load(const gl::Context *context,
                                gl::InfoLog &infoLog,
                                gl::BinaryInputStream *stream)
{
    // TODO(jmadill): Use Renderer from contextImpl.

    reset();

    DeviceIdentifier binaryDeviceIdentifier = {0};
    stream->readBytes(reinterpret_cast<unsigned char *>(&binaryDeviceIdentifier),
                      sizeof(DeviceIdentifier));

    DeviceIdentifier identifier = mRenderer->getAdapterIdentifier();
    if (memcmp(&identifier, &binaryDeviceIdentifier, sizeof(DeviceIdentifier)) != 0)
    {
        infoLog << "Invalid program binary, device configuration has changed.";
        return false;
    }

    int compileFlags = stream->readInt<int>();
    if (compileFlags != ANGLE_COMPILE_OPTIMIZATION_LEVEL)
    {
        infoLog << "Mismatched compilation flags.";
        return false;
    }

    for (int &index : mAttribLocationToD3DSemantic)
    {
        stream->readInt(&index);
    }

    const unsigned int psSamplerCount = stream->readInt<unsigned int>();
    for (unsigned int i = 0; i < psSamplerCount; ++i)
    {
        Sampler sampler;
        stream->readBool(&sampler.active);
        stream->readInt(&sampler.logicalTextureUnit);
        stream->readInt(&sampler.textureType);
        mSamplersPS.push_back(sampler);
    }
    const unsigned int vsSamplerCount = stream->readInt<unsigned int>();
    for (unsigned int i = 0; i < vsSamplerCount; ++i)
    {
        Sampler sampler;
        stream->readBool(&sampler.active);
        stream->readInt(&sampler.logicalTextureUnit);
        stream->readInt(&sampler.textureType);
        mSamplersVS.push_back(sampler);
    }

    const unsigned int csSamplerCount = stream->readInt<unsigned int>();
    for (unsigned int i = 0; i < csSamplerCount; ++i)
    {
        Sampler sampler;
        stream->readBool(&sampler.active);
        stream->readInt(&sampler.logicalTextureUnit);
        stream->readInt(&sampler.textureType);
        mSamplersCS.push_back(sampler);
    }

    stream->readInt(&mUsedVertexSamplerRange);
    stream->readInt(&mUsedPixelSamplerRange);
    stream->readInt(&mUsedComputeSamplerRange);

    const unsigned int uniformCount = stream->readInt<unsigned int>();
    if (stream->error())
    {
        infoLog << "Invalid program binary.";
        return false;
    }

    const auto &linkedUniforms = mState.getUniforms();
    ASSERT(mD3DUniforms.empty());
    for (unsigned int uniformIndex = 0; uniformIndex < uniformCount; uniformIndex++)
    {
        const gl::LinkedUniform &linkedUniform = linkedUniforms[uniformIndex];

        D3DUniform *d3dUniform =
            new D3DUniform(linkedUniform.type, linkedUniform.name, linkedUniform.arraySizes,
                           linkedUniform.isInDefaultBlock());
        stream->readInt(&d3dUniform->psRegisterIndex);
        stream->readInt(&d3dUniform->vsRegisterIndex);
        stream->readInt(&d3dUniform->csRegisterIndex);
        stream->readInt(&d3dUniform->registerCount);
        stream->readInt(&d3dUniform->registerElement);

        mD3DUniforms.push_back(d3dUniform);
    }

    const unsigned int blockCount = stream->readInt<unsigned int>();
    if (stream->error())
    {
        infoLog << "Invalid program binary.";
        return false;
    }

    ASSERT(mD3DUniformBlocks.empty());
    for (unsigned int blockIndex = 0; blockIndex < blockCount; ++blockIndex)
    {
        D3DUniformBlock uniformBlock;
        stream->readInt(&uniformBlock.psRegisterIndex);
        stream->readInt(&uniformBlock.vsRegisterIndex);
        stream->readInt(&uniformBlock.csRegisterIndex);
        mD3DUniformBlocks.push_back(uniformBlock);
    }

    const unsigned int streamOutVaryingCount = stream->readInt<unsigned int>();
    mStreamOutVaryings.resize(streamOutVaryingCount);
    for (unsigned int varyingIndex = 0; varyingIndex < streamOutVaryingCount; ++varyingIndex)
    {
        D3DVarying *varying = &mStreamOutVaryings[varyingIndex];

        stream->readString(&varying->semanticName);
        stream->readInt(&varying->semanticIndex);
        stream->readInt(&varying->componentCount);
        stream->readInt(&varying->outputSlot);
    }

    stream->readString(&mVertexHLSL);
    stream->readBytes(reinterpret_cast<unsigned char *>(&mVertexWorkarounds),
                      sizeof(angle::CompilerWorkaroundsD3D));
    stream->readString(&mPixelHLSL);
    stream->readBytes(reinterpret_cast<unsigned char *>(&mPixelWorkarounds),
                      sizeof(angle::CompilerWorkaroundsD3D));
    stream->readBool(&mUsesFragDepth);
    stream->readBool(&mHasANGLEMultiviewEnabled);
    stream->readBool(&mUsesViewID);
    stream->readBool(&mUsesPointSize);
    stream->readBool(&mUsesFlatInterpolation);

    const size_t pixelShaderKeySize = stream->readInt<unsigned int>();
    mPixelShaderKey.resize(pixelShaderKeySize);
    for (size_t pixelShaderKeyIndex = 0; pixelShaderKeyIndex < pixelShaderKeySize;
         pixelShaderKeyIndex++)
    {
        stream->readInt(&mPixelShaderKey[pixelShaderKeyIndex].type);
        stream->readString(&mPixelShaderKey[pixelShaderKeyIndex].name);
        stream->readString(&mPixelShaderKey[pixelShaderKeyIndex].source);
        stream->readInt(&mPixelShaderKey[pixelShaderKeyIndex].outputIndex);
    }

    stream->readString(&mGeometryShaderPreamble);

    const unsigned char *binary = reinterpret_cast<const unsigned char *>(stream->data());

    bool separateAttribs = (mState.getTransformFeedbackBufferMode() == GL_SEPARATE_ATTRIBS);

    const unsigned int vertexShaderCount = stream->readInt<unsigned int>();
    for (unsigned int vertexShaderIndex = 0; vertexShaderIndex < vertexShaderCount;
         vertexShaderIndex++)
    {
        size_t inputLayoutSize = stream->readInt<size_t>();
        gl::InputLayout inputLayout(inputLayoutSize, gl::VERTEX_FORMAT_INVALID);

        for (size_t inputIndex = 0; inputIndex < inputLayoutSize; inputIndex++)
        {
            inputLayout[inputIndex] = stream->readInt<gl::VertexFormatType>();
        }

        unsigned int vertexShaderSize             = stream->readInt<unsigned int>();
        const unsigned char *vertexShaderFunction = binary + stream->offset();

        ShaderExecutableD3D *shaderExecutable = nullptr;

        ANGLE_TRY(mRenderer->loadExecutable(vertexShaderFunction, vertexShaderSize,
                                            gl::SHADER_VERTEX, mStreamOutVaryings, separateAttribs,
                                            &shaderExecutable));

        if (!shaderExecutable)
        {
            infoLog << "Could not create vertex shader.";
            return false;
        }

        // generated converted input layout
        VertexExecutable::Signature signature;
        VertexExecutable::getSignature(mRenderer, inputLayout, &signature);

        // add new binary
        mVertexExecutables.push_back(std::unique_ptr<VertexExecutable>(
            new VertexExecutable(inputLayout, signature, shaderExecutable)));

        stream->skip(vertexShaderSize);
    }

    const size_t pixelShaderCount = stream->readInt<unsigned int>();
    for (size_t pixelShaderIndex = 0; pixelShaderIndex < pixelShaderCount; pixelShaderIndex++)
    {
        const size_t outputCount = stream->readInt<unsigned int>();
        std::vector<GLenum> outputs(outputCount);
        for (size_t outputIndex = 0; outputIndex < outputCount; outputIndex++)
        {
            stream->readInt(&outputs[outputIndex]);
        }

        const size_t pixelShaderSize             = stream->readInt<unsigned int>();
        const unsigned char *pixelShaderFunction = binary + stream->offset();
        ShaderExecutableD3D *shaderExecutable    = nullptr;

        ANGLE_TRY(mRenderer->loadExecutable(pixelShaderFunction, pixelShaderSize,
                                            gl::SHADER_FRAGMENT, mStreamOutVaryings,
                                            separateAttribs, &shaderExecutable));

        if (!shaderExecutable)
        {
            infoLog << "Could not create pixel shader.";
            return false;
        }

        // add new binary
        mPixelExecutables.push_back(
            std::unique_ptr<PixelExecutable>(new PixelExecutable(outputs, shaderExecutable)));

        stream->skip(pixelShaderSize);
    }

    for (unsigned int geometryExeIndex = 0; geometryExeIndex < gl::PRIMITIVE_TYPE_MAX;
         ++geometryExeIndex)
    {
        unsigned int geometryShaderSize = stream->readInt<unsigned int>();
        if (geometryShaderSize == 0)
        {
            continue;
        }

        const unsigned char *geometryShaderFunction = binary + stream->offset();

        ShaderExecutableD3D *geometryExecutable = nullptr;
        ANGLE_TRY(mRenderer->loadExecutable(geometryShaderFunction, geometryShaderSize,
                                            gl::SHADER_GEOMETRY, mStreamOutVaryings,
                                            separateAttribs, &geometryExecutable));

        if (!geometryExecutable)
        {
            infoLog << "Could not create geometry shader.";
            return false;
        }

        mGeometryExecutables[geometryExeIndex].reset(geometryExecutable);

        stream->skip(geometryShaderSize);
    }

    unsigned int computeShaderSize = stream->readInt<unsigned int>();
    if (computeShaderSize > 0)
    {
        const unsigned char *computeShaderFunction = binary + stream->offset();

        ShaderExecutableD3D *computeExecutable = nullptr;
        ANGLE_TRY(mRenderer->loadExecutable(computeShaderFunction, computeShaderSize,
                                            gl::SHADER_COMPUTE, std::vector<D3DVarying>(), false,
                                            &computeExecutable));

        if (!computeExecutable)
        {
            infoLog << "Could not create compute shader.";
            return false;
        }

        mComputeExecutable.reset(computeExecutable);
    }

    initializeUniformStorage();

    return true;
}

void ProgramD3D::save(const gl::Context *context, gl::BinaryOutputStream *stream)
{
    // Output the DeviceIdentifier before we output any shader code
    // When we load the binary again later, we can validate the device identifier before trying to
    // compile any HLSL
    DeviceIdentifier binaryIdentifier = mRenderer->getAdapterIdentifier();
    stream->writeBytes(reinterpret_cast<unsigned char *>(&binaryIdentifier),
                       sizeof(DeviceIdentifier));

    stream->writeInt(ANGLE_COMPILE_OPTIMIZATION_LEVEL);

    for (int d3dSemantic : mAttribLocationToD3DSemantic)
    {
        stream->writeInt(d3dSemantic);
    }

    stream->writeInt(mSamplersPS.size());
    for (unsigned int i = 0; i < mSamplersPS.size(); ++i)
    {
        stream->writeInt(mSamplersPS[i].active);
        stream->writeInt(mSamplersPS[i].logicalTextureUnit);
        stream->writeInt(mSamplersPS[i].textureType);
    }

    stream->writeInt(mSamplersVS.size());
    for (unsigned int i = 0; i < mSamplersVS.size(); ++i)
    {
        stream->writeInt(mSamplersVS[i].active);
        stream->writeInt(mSamplersVS[i].logicalTextureUnit);
        stream->writeInt(mSamplersVS[i].textureType);
    }

    stream->writeInt(mSamplersCS.size());
    for (unsigned int i = 0; i < mSamplersCS.size(); ++i)
    {
        stream->writeInt(mSamplersCS[i].active);
        stream->writeInt(mSamplersCS[i].logicalTextureUnit);
        stream->writeInt(mSamplersCS[i].textureType);
    }

    stream->writeInt(mUsedVertexSamplerRange);
    stream->writeInt(mUsedPixelSamplerRange);
    stream->writeInt(mUsedComputeSamplerRange);

    stream->writeInt(mD3DUniforms.size());
    for (const D3DUniform *uniform : mD3DUniforms)
    {
        // Type, name and arraySize are redundant, so aren't stored in the binary.
        stream->writeIntOrNegOne(uniform->psRegisterIndex);
        stream->writeIntOrNegOne(uniform->vsRegisterIndex);
        stream->writeIntOrNegOne(uniform->csRegisterIndex);
        stream->writeInt(uniform->registerCount);
        stream->writeInt(uniform->registerElement);
    }

    stream->writeInt(mD3DUniformBlocks.size());
    for (const D3DUniformBlock &uniformBlock : mD3DUniformBlocks)
    {
        stream->writeIntOrNegOne(uniformBlock.psRegisterIndex);
        stream->writeIntOrNegOne(uniformBlock.vsRegisterIndex);
        stream->writeIntOrNegOne(uniformBlock.csRegisterIndex);
    }

    stream->writeInt(mStreamOutVaryings.size());
    for (const auto &varying : mStreamOutVaryings)
    {
        stream->writeString(varying.semanticName);
        stream->writeInt(varying.semanticIndex);
        stream->writeInt(varying.componentCount);
        stream->writeInt(varying.outputSlot);
    }

    stream->writeString(mVertexHLSL);
    stream->writeBytes(reinterpret_cast<unsigned char *>(&mVertexWorkarounds),
                       sizeof(angle::CompilerWorkaroundsD3D));
    stream->writeString(mPixelHLSL);
    stream->writeBytes(reinterpret_cast<unsigned char *>(&mPixelWorkarounds),
                       sizeof(angle::CompilerWorkaroundsD3D));
    stream->writeInt(mUsesFragDepth);
    stream->writeInt(mHasANGLEMultiviewEnabled);
    stream->writeInt(mUsesViewID);
    stream->writeInt(mUsesPointSize);
    stream->writeInt(mUsesFlatInterpolation);

    const std::vector<PixelShaderOutputVariable> &pixelShaderKey = mPixelShaderKey;
    stream->writeInt(pixelShaderKey.size());
    for (size_t pixelShaderKeyIndex = 0; pixelShaderKeyIndex < pixelShaderKey.size();
         pixelShaderKeyIndex++)
    {
        const PixelShaderOutputVariable &variable = pixelShaderKey[pixelShaderKeyIndex];
        stream->writeInt(variable.type);
        stream->writeString(variable.name);
        stream->writeString(variable.source);
        stream->writeInt(variable.outputIndex);
    }

    stream->writeString(mGeometryShaderPreamble);

    stream->writeInt(mVertexExecutables.size());
    for (size_t vertexExecutableIndex = 0; vertexExecutableIndex < mVertexExecutables.size();
         vertexExecutableIndex++)
    {
        VertexExecutable *vertexExecutable = mVertexExecutables[vertexExecutableIndex].get();

        const auto &inputLayout = vertexExecutable->inputs();
        stream->writeInt(inputLayout.size());

        for (size_t inputIndex = 0; inputIndex < inputLayout.size(); inputIndex++)
        {
            stream->writeInt(static_cast<unsigned int>(inputLayout[inputIndex]));
        }

        size_t vertexShaderSize = vertexExecutable->shaderExecutable()->getLength();
        stream->writeInt(vertexShaderSize);

        const uint8_t *vertexBlob = vertexExecutable->shaderExecutable()->getFunction();
        stream->writeBytes(vertexBlob, vertexShaderSize);
    }

    stream->writeInt(mPixelExecutables.size());
    for (size_t pixelExecutableIndex = 0; pixelExecutableIndex < mPixelExecutables.size();
         pixelExecutableIndex++)
    {
        PixelExecutable *pixelExecutable = mPixelExecutables[pixelExecutableIndex].get();

        const std::vector<GLenum> outputs = pixelExecutable->outputSignature();
        stream->writeInt(outputs.size());
        for (size_t outputIndex = 0; outputIndex < outputs.size(); outputIndex++)
        {
            stream->writeInt(outputs[outputIndex]);
        }

        size_t pixelShaderSize = pixelExecutable->shaderExecutable()->getLength();
        stream->writeInt(pixelShaderSize);

        const uint8_t *pixelBlob = pixelExecutable->shaderExecutable()->getFunction();
        stream->writeBytes(pixelBlob, pixelShaderSize);
    }

    for (auto const &geometryExecutable : mGeometryExecutables)
    {
        if (!geometryExecutable)
        {
            stream->writeInt(0);
            continue;
        }

        size_t geometryShaderSize = geometryExecutable->getLength();
        stream->writeInt(geometryShaderSize);
        stream->writeBytes(geometryExecutable->getFunction(), geometryShaderSize);
    }

    if (mComputeExecutable)
    {
        size_t computeShaderSize = mComputeExecutable->getLength();
        stream->writeInt(computeShaderSize);
        stream->writeBytes(mComputeExecutable->getFunction(), computeShaderSize);
    }
    else
    {
        stream->writeInt(0);
    }
}

void ProgramD3D::setBinaryRetrievableHint(bool /* retrievable */)
{
}

void ProgramD3D::setSeparable(bool /* separable */)
{
}

gl::Error ProgramD3D::getPixelExecutableForCachedOutputLayout(ShaderExecutableD3D **outExecutable,
                                                              gl::InfoLog *infoLog)
{
    if (mCachedPixelExecutableIndex.valid())
    {
        *outExecutable = mPixelExecutables[mCachedPixelExecutableIndex.value()]->shaderExecutable();
        return gl::NoError();
    }

    std::string finalPixelHLSL = mDynamicHLSL->generatePixelShaderForOutputSignature(
        mPixelHLSL, mPixelShaderKey, mUsesFragDepth, mPixelShaderOutputLayoutCache);

    // Generate new pixel executable
    ShaderExecutableD3D *pixelExecutable = nullptr;

    gl::InfoLog tempInfoLog;
    gl::InfoLog *currentInfoLog = infoLog ? infoLog : &tempInfoLog;

    ANGLE_TRY(mRenderer->compileToExecutable(
        *currentInfoLog, finalPixelHLSL, gl::SHADER_FRAGMENT, mStreamOutVaryings,
        (mState.getTransformFeedbackBufferMode() == GL_SEPARATE_ATTRIBS), mPixelWorkarounds,
        &pixelExecutable));

    if (pixelExecutable)
    {
        mPixelExecutables.push_back(std::unique_ptr<PixelExecutable>(
            new PixelExecutable(mPixelShaderOutputLayoutCache, pixelExecutable)));
        mCachedPixelExecutableIndex = mPixelExecutables.size() - 1;
    }
    else if (!infoLog)
    {
        ERR() << "Error compiling dynamic pixel executable:" << std::endl
              << tempInfoLog.str() << std::endl;
    }

    *outExecutable = pixelExecutable;
    return gl::NoError();
}

gl::Error ProgramD3D::getVertexExecutableForCachedInputLayout(ShaderExecutableD3D **outExectuable,
                                                              gl::InfoLog *infoLog)
{
    if (mCachedVertexExecutableIndex.valid())
    {
        *outExectuable =
            mVertexExecutables[mCachedVertexExecutableIndex.value()]->shaderExecutable();
        return gl::NoError();
    }

    // Generate new dynamic layout with attribute conversions
    std::string finalVertexHLSL = mDynamicHLSL->generateVertexShaderForInputLayout(
        mVertexHLSL, mCachedInputLayout, mState.getAttributes());

    // Generate new vertex executable
    ShaderExecutableD3D *vertexExecutable = nullptr;

    gl::InfoLog tempInfoLog;
    gl::InfoLog *currentInfoLog = infoLog ? infoLog : &tempInfoLog;

    ANGLE_TRY(mRenderer->compileToExecutable(
        *currentInfoLog, finalVertexHLSL, gl::SHADER_VERTEX, mStreamOutVaryings,
        (mState.getTransformFeedbackBufferMode() == GL_SEPARATE_ATTRIBS), mVertexWorkarounds,
        &vertexExecutable));

    if (vertexExecutable)
    {
        mVertexExecutables.push_back(std::unique_ptr<VertexExecutable>(
            new VertexExecutable(mCachedInputLayout, mCachedVertexSignature, vertexExecutable)));
        mCachedVertexExecutableIndex = mVertexExecutables.size() - 1;
    }
    else if (!infoLog)
    {
        ERR() << "Error compiling dynamic vertex executable:" << std::endl
              << tempInfoLog.str() << std::endl;
    }

    *outExectuable = vertexExecutable;
    return gl::NoError();
}

gl::Error ProgramD3D::getGeometryExecutableForPrimitiveType(const gl::Context *context,
                                                            GLenum drawMode,
                                                            ShaderExecutableD3D **outExecutable,
                                                            gl::InfoLog *infoLog)
{
    if (outExecutable)
    {
        *outExecutable = nullptr;
    }

    // Return a null shader if the current rendering doesn't use a geometry shader
    if (!usesGeometryShader(drawMode))
    {
        return gl::NoError();
    }

    gl::PrimitiveType geometryShaderType = GetGeometryShaderTypeFromDrawMode(drawMode);

    if (mGeometryExecutables[geometryShaderType])
    {
        if (outExecutable)
        {
            *outExecutable = mGeometryExecutables[geometryShaderType].get();
        }
        return gl::NoError();
    }

    std::string geometryHLSL = mDynamicHLSL->generateGeometryShaderHLSL(
        context, geometryShaderType, mState, mRenderer->presentPathFastEnabled(),
        mHasANGLEMultiviewEnabled, mRenderer->canSelectViewInVertexShader(),
        usesGeometryShaderForPointSpriteEmulation(), mGeometryShaderPreamble);

    gl::InfoLog tempInfoLog;
    gl::InfoLog *currentInfoLog = infoLog ? infoLog : &tempInfoLog;

    ShaderExecutableD3D *geometryExecutable = nullptr;
    gl::Error error                         = mRenderer->compileToExecutable(
        *currentInfoLog, geometryHLSL, gl::SHADER_GEOMETRY, mStreamOutVaryings,
        (mState.getTransformFeedbackBufferMode() == GL_SEPARATE_ATTRIBS),
        angle::CompilerWorkaroundsD3D(), &geometryExecutable);

    if (!infoLog && error.isError())
    {
        ERR() << "Error compiling dynamic geometry executable:" << std::endl
              << tempInfoLog.str() << std::endl;
    }

    if (geometryExecutable != nullptr)
    {
        mGeometryExecutables[geometryShaderType].reset(geometryExecutable);
    }

    if (outExecutable)
    {
        *outExecutable = mGeometryExecutables[geometryShaderType].get();
    }
    return error;
}

class ProgramD3D::GetExecutableTask : public Closure
{
  public:
    GetExecutableTask(ProgramD3D *program)
        : mProgram(program), mError(gl::NoError()), mInfoLog(), mResult(nullptr)
    {
    }

    virtual gl::Error run() = 0;

    void operator()() override { mError = run(); }

    const gl::Error &getError() const { return mError; }
    const gl::InfoLog &getInfoLog() const { return mInfoLog; }
    ShaderExecutableD3D *getResult() { return mResult; }

  protected:
    ProgramD3D *mProgram;
    gl::Error mError;
    gl::InfoLog mInfoLog;
    ShaderExecutableD3D *mResult;
};

class ProgramD3D::GetVertexExecutableTask : public ProgramD3D::GetExecutableTask
{
  public:
    GetVertexExecutableTask(ProgramD3D *program, const gl::Context *context)
        : GetExecutableTask(program), mContext(context)
    {
    }
    gl::Error run() override
    {
        mProgram->updateCachedInputLayoutFromShader(mContext);

        ANGLE_TRY(mProgram->getVertexExecutableForCachedInputLayout(&mResult, &mInfoLog));

        return gl::NoError();
    }

  private:
    const gl::Context *mContext;
};

void ProgramD3D::updateCachedInputLayoutFromShader(const gl::Context *context)
{
    GetDefaultInputLayoutFromShader(context, mState.getAttachedVertexShader(), &mCachedInputLayout);
    VertexExecutable::getSignature(mRenderer, mCachedInputLayout, &mCachedVertexSignature);
    updateCachedVertexExecutableIndex();
}

class ProgramD3D::GetPixelExecutableTask : public ProgramD3D::GetExecutableTask
{
  public:
    GetPixelExecutableTask(ProgramD3D *program) : GetExecutableTask(program) {}
    gl::Error run() override
    {
        mProgram->updateCachedOutputLayoutFromShader();

        ANGLE_TRY(mProgram->getPixelExecutableForCachedOutputLayout(&mResult, &mInfoLog));

        return gl::NoError();
    }
};

void ProgramD3D::updateCachedOutputLayoutFromShader()
{
    GetDefaultOutputLayoutFromShader(mPixelShaderKey, &mPixelShaderOutputLayoutCache);
    updateCachedPixelExecutableIndex();
}

class ProgramD3D::GetGeometryExecutableTask : public ProgramD3D::GetExecutableTask
{
  public:
    GetGeometryExecutableTask(ProgramD3D *program, const gl::Context *context)
        : GetExecutableTask(program), mContext(context)
    {
    }

    gl::Error run() override
    {
        // Auto-generate the geometry shader here, if we expect to be using point rendering in
        // D3D11.
        if (mProgram->usesGeometryShader(GL_POINTS))
        {
            ANGLE_TRY(mProgram->getGeometryExecutableForPrimitiveType(mContext, GL_POINTS, &mResult,
                                                                      &mInfoLog));
        }

        return gl::NoError();
    }

  private:
    const gl::Context *mContext;
};

gl::Error ProgramD3D::getComputeExecutable(ShaderExecutableD3D **outExecutable)
{
    if (outExecutable)
    {
        *outExecutable = mComputeExecutable.get();
    }

    return gl::NoError();
}

gl::LinkResult ProgramD3D::compileProgramExecutables(const gl::Context *context,
                                                     gl::InfoLog &infoLog)
{
    // Ensure the compiler is initialized to avoid race conditions.
    ANGLE_TRY(mRenderer->ensureHLSLCompilerInitialized());

    WorkerThreadPool *workerPool = mRenderer->getWorkerThreadPool();

    GetVertexExecutableTask vertexTask(this, context);
    GetPixelExecutableTask pixelTask(this);
    GetGeometryExecutableTask geometryTask(this, context);

    std::array<WaitableEvent, 3> waitEvents = {{workerPool->postWorkerTask(&vertexTask),
                                                workerPool->postWorkerTask(&pixelTask),
                                                workerPool->postWorkerTask(&geometryTask)}};

    WaitableEvent::WaitMany(&waitEvents);

    infoLog << vertexTask.getInfoLog().str();
    infoLog << pixelTask.getInfoLog().str();
    infoLog << geometryTask.getInfoLog().str();

    ANGLE_TRY(vertexTask.getError());
    ANGLE_TRY(pixelTask.getError());
    ANGLE_TRY(geometryTask.getError());

    ShaderExecutableD3D *defaultVertexExecutable = vertexTask.getResult();
    ShaderExecutableD3D *defaultPixelExecutable  = pixelTask.getResult();
    ShaderExecutableD3D *pointGS                 = geometryTask.getResult();

    const ShaderD3D *vertexShaderD3D = GetImplAs<ShaderD3D>(mState.getAttachedVertexShader());

    if (usesGeometryShader(GL_POINTS) && pointGS)
    {
        // Geometry shaders are currently only used internally, so there is no corresponding shader
        // object at the interface level. For now the geometry shader debug info is prepended to
        // the vertex shader.
        vertexShaderD3D->appendDebugInfo("// GEOMETRY SHADER BEGIN\n\n");
        vertexShaderD3D->appendDebugInfo(pointGS->getDebugInfo());
        vertexShaderD3D->appendDebugInfo("\nGEOMETRY SHADER END\n\n\n");
    }

    if (defaultVertexExecutable)
    {
        vertexShaderD3D->appendDebugInfo(defaultVertexExecutable->getDebugInfo());
    }

    if (defaultPixelExecutable)
    {
        const ShaderD3D *fragmentShaderD3D =
            GetImplAs<ShaderD3D>(mState.getAttachedFragmentShader());
        fragmentShaderD3D->appendDebugInfo(defaultPixelExecutable->getDebugInfo());
    }

    return (defaultVertexExecutable && defaultPixelExecutable &&
            (!usesGeometryShader(GL_POINTS) || pointGS));
}

gl::LinkResult ProgramD3D::compileComputeExecutable(const gl::Context *context,
                                                    gl::InfoLog &infoLog)
{
    // Ensure the compiler is initialized to avoid race conditions.
    ANGLE_TRY(mRenderer->ensureHLSLCompilerInitialized());

    std::string computeShader = mDynamicHLSL->generateComputeShaderLinkHLSL(context, mState);

    ShaderExecutableD3D *computeExecutable = nullptr;
    ANGLE_TRY(mRenderer->compileToExecutable(infoLog, computeShader, gl::SHADER_COMPUTE,
                                             std::vector<D3DVarying>(), false,
                                             angle::CompilerWorkaroundsD3D(), &computeExecutable));

    if (computeExecutable == nullptr)
    {
        ERR() << "Error compiling dynamic compute executable:" << std::endl
              << infoLog.str() << std::endl;
    }
    else
    {
        const ShaderD3D *computeShaderD3D = GetImplAs<ShaderD3D>(mState.getAttachedComputeShader());
        computeShaderD3D->appendDebugInfo(computeExecutable->getDebugInfo());
        mComputeExecutable.reset(computeExecutable);
    }

    return mComputeExecutable.get() != nullptr;
}

gl::LinkResult ProgramD3D::link(const gl::Context *context,
                                const gl::ProgramLinkedResources &resources,
                                gl::InfoLog &infoLog)
{
    const auto &data = context->getContextState();

    reset();

    gl::Shader *computeShader = mState.getAttachedComputeShader();
    if (computeShader)
    {
        mSamplersCS.resize(data.getCaps().maxComputeTextureImageUnits);

        defineUniformsAndAssignRegisters(context);

        gl::LinkResult result = compileComputeExecutable(context, infoLog);
        if (result.isError())
        {
            infoLog << result.getError().getMessage();
            return result;
        }
        else if (!result.getResult())
        {
            infoLog << "Failed to create D3D compute shader.";
            return result;
        }
    }
    else
    {
        gl::Shader *vertexShader   = mState.getAttachedVertexShader();
        gl::Shader *fragmentShader = mState.getAttachedFragmentShader();

        const ShaderD3D *vertexShaderD3D   = GetImplAs<ShaderD3D>(vertexShader);
        const ShaderD3D *fragmentShaderD3D = GetImplAs<ShaderD3D>(fragmentShader);

        mSamplersVS.resize(data.getCaps().maxVertexTextureImageUnits);
        mSamplersPS.resize(data.getCaps().maxTextureImageUnits);

        vertexShaderD3D->generateWorkarounds(&mVertexWorkarounds);
        fragmentShaderD3D->generateWorkarounds(&mPixelWorkarounds);

        if (mRenderer->getNativeLimitations().noFrontFacingSupport)
        {
            if (fragmentShaderD3D->usesFrontFacing())
            {
                infoLog << "The current renderer doesn't support gl_FrontFacing";
                return false;
            }
        }

        // TODO(jmadill): Implement more sophisticated component packing in D3D9.
        // We can fail here because we use one semantic per GLSL varying. D3D11 can pack varyings
        // intelligently, but D3D9 assumes one semantic per register.
        if (mRenderer->getRendererClass() == RENDERER_D3D9 &&
            resources.varyingPacking.getMaxSemanticIndex() > data.getCaps().maxVaryingVectors)
        {
            infoLog << "Cannot pack these varyings on D3D9.";
            return false;
        }

        ProgramD3DMetadata metadata(mRenderer, vertexShaderD3D, fragmentShaderD3D);
        BuiltinVaryingsD3D builtins(metadata, resources.varyingPacking);

        mDynamicHLSL->generateShaderLinkHLSL(context, mState, metadata, resources.varyingPacking,
                                             builtins, &mPixelHLSL, &mVertexHLSL);

        mUsesPointSize = vertexShaderD3D->usesPointSize();
        mDynamicHLSL->getPixelShaderOutputKey(data, mState, metadata, &mPixelShaderKey);
        mUsesFragDepth = metadata.usesFragDepth();
        mUsesViewID               = metadata.usesViewID();
        mHasANGLEMultiviewEnabled = metadata.hasANGLEMultiviewEnabled();

        // Cache if we use flat shading
        mUsesFlatInterpolation =
            (FindFlatInterpolationVarying(fragmentShader->getInputVaryings(context)) ||
             FindFlatInterpolationVarying(vertexShader->getOutputVaryings(context)));

        if (mRenderer->getMajorShaderModel() >= 4)
        {
            mGeometryShaderPreamble = mDynamicHLSL->generateGeometryShaderPreamble(
                resources.varyingPacking, builtins, mHasANGLEMultiviewEnabled,
                metadata.canSelectViewInVertexShader());
        }

        initAttribLocationsToD3DSemantic(context);

        defineUniformsAndAssignRegisters(context);

        gatherTransformFeedbackVaryings(resources.varyingPacking, builtins[gl::SHADER_VERTEX]);

        gl::LinkResult result = compileProgramExecutables(context, infoLog);
        if (result.isError())
        {
            infoLog << result.getError().getMessage();
            return result;
        }
        else if (!result.getResult())
        {
            infoLog << "Failed to create D3D shaders.";
            return result;
        }
    }

    linkResources(context, resources);

    return true;
}

GLboolean ProgramD3D::validate(const gl::Caps & /*caps*/, gl::InfoLog * /*infoLog*/)
{
    // TODO(jmadill): Do something useful here?
    return GL_TRUE;
}

void ProgramD3D::initializeUniformBlocks()
{
    if (mState.getUniformBlocks().empty())
    {
        return;
    }

    ASSERT(mD3DUniformBlocks.empty());

    // Assign registers and update sizes.
    const ShaderD3D *vertexShaderD3D = SafeGetImplAs<ShaderD3D>(mState.getAttachedVertexShader());
    const ShaderD3D *fragmentShaderD3D =
        SafeGetImplAs<ShaderD3D>(mState.getAttachedFragmentShader());
    const ShaderD3D *computeShaderD3D = SafeGetImplAs<ShaderD3D>(mState.getAttachedComputeShader());

    for (const gl::InterfaceBlock &uniformBlock : mState.getUniformBlocks())
    {
        unsigned int uniformBlockElement = uniformBlock.isArray ? uniformBlock.arrayElement : 0;

        D3DUniformBlock d3dUniformBlock;

        if (uniformBlock.vertexStaticUse)
        {
            ASSERT(vertexShaderD3D != nullptr);
            unsigned int baseRegister = vertexShaderD3D->getUniformBlockRegister(uniformBlock.name);
            d3dUniformBlock.vsRegisterIndex = baseRegister + uniformBlockElement;
        }

        if (uniformBlock.fragmentStaticUse)
        {
            ASSERT(fragmentShaderD3D != nullptr);
            unsigned int baseRegister =
                fragmentShaderD3D->getUniformBlockRegister(uniformBlock.name);
            d3dUniformBlock.psRegisterIndex = baseRegister + uniformBlockElement;
        }

        if (uniformBlock.computeStaticUse)
        {
            ASSERT(computeShaderD3D != nullptr);
            unsigned int baseRegister =
                computeShaderD3D->getUniformBlockRegister(uniformBlock.name);
            d3dUniformBlock.csRegisterIndex = baseRegister + uniformBlockElement;
        }

        mD3DUniformBlocks.push_back(d3dUniformBlock);
    }
}

void ProgramD3D::initializeUniformStorage()
{
    // Compute total default block size
    unsigned int vertexRegisters   = 0;
    unsigned int fragmentRegisters = 0;
    unsigned int computeRegisters  = 0;
    for (const D3DUniform *d3dUniform : mD3DUniforms)
    {
        if (!d3dUniform->isSampler())
        {
            if (d3dUniform->isReferencedByVertexShader())
            {
                vertexRegisters = std::max(vertexRegisters,
                                           d3dUniform->vsRegisterIndex + d3dUniform->registerCount);
            }
            if (d3dUniform->isReferencedByFragmentShader())
            {
                fragmentRegisters = std::max(
                    fragmentRegisters, d3dUniform->psRegisterIndex + d3dUniform->registerCount);
            }
            if (d3dUniform->isReferencedByComputeShader())
            {
                computeRegisters = std::max(
                    computeRegisters, d3dUniform->csRegisterIndex + d3dUniform->registerCount);
            }
        }
    }

    mVertexUniformStorage =
        std::unique_ptr<UniformStorageD3D>(mRenderer->createUniformStorage(vertexRegisters * 16u));
    mFragmentUniformStorage = std::unique_ptr<UniformStorageD3D>(
        mRenderer->createUniformStorage(fragmentRegisters * 16u));
    mComputeUniformStorage =
        std::unique_ptr<UniformStorageD3D>(mRenderer->createUniformStorage(computeRegisters * 16u));

    // Iterate the uniforms again to assign data pointers to default block uniforms.
    for (D3DUniform *d3dUniform : mD3DUniforms)
    {
        if (d3dUniform->isSampler())
        {
            d3dUniform->mSamplerData.resize(d3dUniform->getArraySizeProduct(), 0);
            continue;
        }

        if (d3dUniform->isReferencedByVertexShader())
        {
            d3dUniform->vsData = mVertexUniformStorage->getDataPointer(d3dUniform->vsRegisterIndex,
                                                                       d3dUniform->registerElement);
        }

        if (d3dUniform->isReferencedByFragmentShader())
        {
            d3dUniform->psData = mFragmentUniformStorage->getDataPointer(
                d3dUniform->psRegisterIndex, d3dUniform->registerElement);
        }

        if (d3dUniform->isReferencedByComputeShader())
        {
            d3dUniform->csData = mComputeUniformStorage->getDataPointer(
                d3dUniform->csRegisterIndex, d3dUniform->registerElement);
        }
    }
}

void ProgramD3D::updateUniformBufferCache(const gl::Caps &caps,
                                          unsigned int reservedVertex,
                                          unsigned int reservedFragment)
{
    if (mState.getUniformBlocks().empty())
    {
        return;
    }

    mVertexUBOCache.clear();
    mFragmentUBOCache.clear();

    for (unsigned int uniformBlockIndex = 0; uniformBlockIndex < mD3DUniformBlocks.size();
         uniformBlockIndex++)
    {
        const D3DUniformBlock &uniformBlock = mD3DUniformBlocks[uniformBlockIndex];
        GLuint blockBinding                 = mState.getUniformBlockBinding(uniformBlockIndex);

        // Unnecessary to apply an unreferenced standard or shared UBO
        if (!uniformBlock.vertexStaticUse() && !uniformBlock.fragmentStaticUse())
        {
            continue;
        }

        if (uniformBlock.vertexStaticUse())
        {
            unsigned int registerIndex = uniformBlock.vsRegisterIndex - reservedVertex;
            ASSERT(registerIndex < caps.maxVertexUniformBlocks);

            if (mVertexUBOCache.size() <= registerIndex)
            {
                mVertexUBOCache.resize(registerIndex + 1, -1);
            }

            ASSERT(mVertexUBOCache[registerIndex] == -1);
            mVertexUBOCache[registerIndex] = blockBinding;
        }

        if (uniformBlock.fragmentStaticUse())
        {
            unsigned int registerIndex = uniformBlock.psRegisterIndex - reservedFragment;
            ASSERT(registerIndex < caps.maxFragmentUniformBlocks);

            if (mFragmentUBOCache.size() <= registerIndex)
            {
                mFragmentUBOCache.resize(registerIndex + 1, -1);
            }

            ASSERT(mFragmentUBOCache[registerIndex] == -1);
            mFragmentUBOCache[registerIndex] = blockBinding;
        }
    }
}

const std::vector<GLint> &ProgramD3D::getVertexUniformBufferCache() const
{
    return mVertexUBOCache;
}

const std::vector<GLint> &ProgramD3D::getFragmentUniformBufferCache() const
{
    return mFragmentUBOCache;
}

void ProgramD3D::dirtyAllUniforms()
{
    mVertexUniformsDirty   = true;
    mFragmentUniformsDirty = true;
    mComputeUniformsDirty  = true;
}

void ProgramD3D::markUniformsClean()
{
    mVertexUniformsDirty   = false;
    mFragmentUniformsDirty = false;
    mComputeUniformsDirty  = false;
}

void ProgramD3D::setUniform1fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformInternal(location, count, v, GL_FLOAT);
}

void ProgramD3D::setUniform2fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformInternal(location, count, v, GL_FLOAT_VEC2);
}

void ProgramD3D::setUniform3fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformInternal(location, count, v, GL_FLOAT_VEC3);
}

void ProgramD3D::setUniform4fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformInternal(location, count, v, GL_FLOAT_VEC4);
}

void ProgramD3D::setUniformMatrix2fv(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value)
{
    setUniformMatrixfvInternal<2, 2>(location, count, transpose, value, GL_FLOAT_MAT2);
}

void ProgramD3D::setUniformMatrix3fv(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value)
{
    setUniformMatrixfvInternal<3, 3>(location, count, transpose, value, GL_FLOAT_MAT3);
}

void ProgramD3D::setUniformMatrix4fv(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value)
{
    setUniformMatrixfvInternal<4, 4>(location, count, transpose, value, GL_FLOAT_MAT4);
}

void ProgramD3D::setUniformMatrix2x3fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfvInternal<2, 3>(location, count, transpose, value, GL_FLOAT_MAT2x3);
}

void ProgramD3D::setUniformMatrix3x2fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfvInternal<3, 2>(location, count, transpose, value, GL_FLOAT_MAT3x2);
}

void ProgramD3D::setUniformMatrix2x4fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfvInternal<2, 4>(location, count, transpose, value, GL_FLOAT_MAT2x4);
}

void ProgramD3D::setUniformMatrix4x2fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfvInternal<4, 2>(location, count, transpose, value, GL_FLOAT_MAT4x2);
}

void ProgramD3D::setUniformMatrix3x4fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfvInternal<3, 4>(location, count, transpose, value, GL_FLOAT_MAT3x4);
}

void ProgramD3D::setUniformMatrix4x3fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfvInternal<4, 3>(location, count, transpose, value, GL_FLOAT_MAT4x3);
}

void ProgramD3D::setUniform1iv(GLint location, GLsizei count, const GLint *v)
{
    setUniformInternal(location, count, v, GL_INT);
}

void ProgramD3D::setUniform2iv(GLint location, GLsizei count, const GLint *v)
{
    setUniformInternal(location, count, v, GL_INT_VEC2);
}

void ProgramD3D::setUniform3iv(GLint location, GLsizei count, const GLint *v)
{
    setUniformInternal(location, count, v, GL_INT_VEC3);
}

void ProgramD3D::setUniform4iv(GLint location, GLsizei count, const GLint *v)
{
    setUniformInternal(location, count, v, GL_INT_VEC4);
}

void ProgramD3D::setUniform1uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformInternal(location, count, v, GL_UNSIGNED_INT);
}

void ProgramD3D::setUniform2uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformInternal(location, count, v, GL_UNSIGNED_INT_VEC2);
}

void ProgramD3D::setUniform3uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformInternal(location, count, v, GL_UNSIGNED_INT_VEC3);
}

void ProgramD3D::setUniform4uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformInternal(location, count, v, GL_UNSIGNED_INT_VEC4);
}

void ProgramD3D::setUniformBlockBinding(GLuint /*uniformBlockIndex*/,
                                        GLuint /*uniformBlockBinding*/)
{
}

void ProgramD3D::defineUniformsAndAssignRegisters(const gl::Context *context)
{
    D3DUniformMap uniformMap;
    gl::Shader *computeShader = mState.getAttachedComputeShader();
    if (computeShader)
    {
        for (const sh::Uniform &computeUniform : computeShader->getUniforms(context))
        {
            if (computeUniform.staticUse)
            {
                defineUniformBase(computeShader, computeUniform, &uniformMap);
            }
        }
    }
    else
    {
        gl::Shader *vertexShader = mState.getAttachedVertexShader();
        for (const sh::Uniform &vertexUniform : vertexShader->getUniforms(context))
        {
            if (vertexUniform.staticUse)
            {
                defineUniformBase(vertexShader, vertexUniform, &uniformMap);
            }
        }

        gl::Shader *fragmentShader = mState.getAttachedFragmentShader();
        for (const sh::Uniform &fragmentUniform : fragmentShader->getUniforms(context))
        {
            if (fragmentUniform.staticUse)
            {
                defineUniformBase(fragmentShader, fragmentUniform, &uniformMap);
            }
        }
    }

    // Initialize the D3DUniform list to mirror the indexing of the GL layer.
    for (const gl::LinkedUniform &glUniform : mState.getUniforms())
    {
        if (!glUniform.isInDefaultBlock())
            continue;

        std::string name = glUniform.name;
        if (glUniform.isArray())
        {
            // In the program state, array uniform names include [0] as in the program resource
            // spec. Here we don't include it.
            // TODO(oetuaho@nvidia.com): consider using the same uniform naming here as in the GL
            // layer.
            ASSERT(angle::EndsWith(name, "[0]"));
            name.resize(name.length() - 3);
        }
        auto mapEntry = uniformMap.find(name);
        ASSERT(mapEntry != uniformMap.end());
        mD3DUniforms.push_back(mapEntry->second);
    }

    assignAllSamplerRegisters();
    initializeUniformStorage();
}

void ProgramD3D::defineUniformBase(const gl::Shader *shader,
                                   const sh::Uniform &uniform,
                                   D3DUniformMap *uniformMap)
{
    // Samplers get their registers assigned in assignAllSamplerRegisters.
    if (uniform.isBuiltIn() || gl::IsSamplerType(uniform.type))
    {
        defineUniform(shader->getType(), uniform, uniform.name, nullptr, uniformMap);
        return;
    }

    const ShaderD3D *shaderD3D = GetImplAs<ShaderD3D>(shader);

    unsigned int startRegister = shaderD3D->getUniformRegister(uniform.name);
    ShShaderOutput outputType = shaderD3D->getCompilerOutputType();
    sh::HLSLBlockEncoder encoder(sh::HLSLBlockEncoder::GetStrategyFor(outputType), true);
    encoder.skipRegisters(startRegister);

    defineUniform(shader->getType(), uniform, uniform.name, &encoder, uniformMap);
}

D3DUniform *ProgramD3D::getD3DUniformByName(const std::string &name)
{
    for (D3DUniform *d3dUniform : mD3DUniforms)
    {
        if (d3dUniform->name == name)
        {
            return d3dUniform;
        }
    }

    return nullptr;
}

void ProgramD3D::defineStructUniformFields(GLenum shaderType,
                                           const std::vector<sh::ShaderVariable> &fields,
                                           const std::string &namePrefix,
                                           sh::HLSLBlockEncoder *encoder,
                                           D3DUniformMap *uniformMap)
{
    if (encoder)
        encoder->enterAggregateType();

    for (size_t fieldIndex = 0; fieldIndex < fields.size(); fieldIndex++)
    {
        const sh::ShaderVariable &field  = fields[fieldIndex];
        const std::string &fieldFullName = (namePrefix + "." + field.name);

        // Samplers get their registers assigned in assignAllSamplerRegisters.
        // Also they couldn't use the same encoder as the rest of the struct, since they are
        // extracted out of the struct by the shader translator.
        if (gl::IsSamplerType(field.type))
        {
            defineUniform(shaderType, field, fieldFullName, nullptr, uniformMap);
        }
        else
        {
            defineUniform(shaderType, field, fieldFullName, encoder, uniformMap);
        }
    }

    if (encoder)
        encoder->exitAggregateType();
}

void ProgramD3D::defineArrayOfStructsUniformFields(GLenum shaderType,
                                                   const sh::ShaderVariable &uniform,
                                                   unsigned int arrayNestingIndex,
                                                   const std::string &prefix,
                                                   sh::HLSLBlockEncoder *encoder,
                                                   D3DUniformMap *uniformMap)
{
    // Nested arrays are processed starting from outermost (arrayNestingIndex 0u) and ending at the
    // innermost.
    const unsigned int currentArraySize = uniform.getNestedArraySize(arrayNestingIndex);
    for (unsigned int arrayElement = 0u; arrayElement < currentArraySize; ++arrayElement)
    {
        const std::string &elementString = prefix + ArrayString(arrayElement);
        if (arrayNestingIndex + 1u < uniform.arraySizes.size())
        {
            defineArrayOfStructsUniformFields(shaderType, uniform, arrayNestingIndex + 1u,
                                              elementString, encoder, uniformMap);
        }
        else
        {
            defineStructUniformFields(shaderType, uniform.fields, elementString, encoder,
                                      uniformMap);
        }
    }
}

void ProgramD3D::defineArrayUniformElements(GLenum shaderType,
                                            const sh::ShaderVariable &uniform,
                                            const std::string &fullName,
                                            sh::HLSLBlockEncoder *encoder,
                                            D3DUniformMap *uniformMap)
{
    if (encoder)
        encoder->enterAggregateType();

    sh::ShaderVariable uniformElement = uniform;
    uniformElement.arraySizes.pop_back();
    for (unsigned int arrayIndex = 0u; arrayIndex < uniform.getOutermostArraySize(); ++arrayIndex)
    {
        std::string elementFullName = fullName + ArrayString(arrayIndex);
        defineUniform(shaderType, uniformElement, elementFullName, encoder, uniformMap);
    }

    if (encoder)
        encoder->exitAggregateType();
}

void ProgramD3D::defineUniform(GLenum shaderType,
                               const sh::ShaderVariable &uniform,
                               const std::string &fullName,
                               sh::HLSLBlockEncoder *encoder,
                               D3DUniformMap *uniformMap)
{
    if (uniform.isStruct())
    {
        if (uniform.isArray())
        {
            defineArrayOfStructsUniformFields(shaderType, uniform, 0u, fullName, encoder,
                                              uniformMap);
        }
        else
        {
            defineStructUniformFields(shaderType, uniform.fields, fullName, encoder, uniformMap);
        }
        return;
    }
    if (uniform.isArrayOfArrays())
    {
        defineArrayUniformElements(shaderType, uniform, fullName, encoder, uniformMap);
        return;
    }

    // Not a struct. Arrays are treated as aggregate types.
    if (uniform.isArray() && encoder)
    {
        encoder->enterAggregateType();
    }

    // Advance the uniform offset, to track registers allocation for structs
    sh::BlockMemberInfo blockInfo =
        encoder ? encoder->encodeType(uniform.type, uniform.arraySizes, false)
                : sh::BlockMemberInfo::getDefaultBlockInfo();

    auto uniformMapEntry   = uniformMap->find(fullName);
    D3DUniform *d3dUniform = nullptr;

    if (uniformMapEntry != uniformMap->end())
    {
        d3dUniform = uniformMapEntry->second;
    }
    else
    {
        d3dUniform              = new D3DUniform(uniform.type, fullName, uniform.arraySizes, true);
        (*uniformMap)[fullName] = d3dUniform;
    }

    if (encoder)
    {
        d3dUniform->registerElement =
            static_cast<unsigned int>(sh::HLSLBlockEncoder::getBlockRegisterElement(blockInfo));
        unsigned int reg =
            static_cast<unsigned int>(sh::HLSLBlockEncoder::getBlockRegister(blockInfo));
        if (shaderType == GL_FRAGMENT_SHADER)
        {
            d3dUniform->psRegisterIndex = reg;
        }
        else if (shaderType == GL_VERTEX_SHADER)
        {
            d3dUniform->vsRegisterIndex = reg;
        }
        else
        {
            ASSERT(shaderType == GL_COMPUTE_SHADER);
            d3dUniform->csRegisterIndex = reg;
        }

        // Arrays are treated as aggregate types
        if (uniform.isArray())
        {
            encoder->exitAggregateType();
        }
    }
}

// Assume count is already clamped.
template <typename T>
void ProgramD3D::setUniformImpl(const gl::VariableLocation &locationInfo,
                                GLsizei count,
                                const T *v,
                                uint8_t *targetData,
                                GLenum uniformType)
{
    D3DUniform *targetUniform = mD3DUniforms[locationInfo.index];
    const int components      = targetUniform->typeInfo.componentCount;
    const unsigned int arrayElementOffset = locationInfo.arrayIndex;

    if (targetUniform->typeInfo.type == uniformType)
    {
        T *dest         = reinterpret_cast<T *>(targetData) + arrayElementOffset * 4;
        const T *source = v;

        for (GLint i = 0; i < count; i++, dest += 4, source += components)
        {
            memcpy(dest, source, components * sizeof(T));
        }
    }
    else
    {
        ASSERT(targetUniform->typeInfo.type == gl::VariableBoolVectorType(uniformType));
        GLint *boolParams = reinterpret_cast<GLint *>(targetData) + arrayElementOffset * 4;

        for (GLint i = 0; i < count; i++)
        {
            GLint *dest     = boolParams + (i * 4);
            const T *source = v + (i * components);

            for (int c = 0; c < components; c++)
            {
                dest[c] = (source[c] == static_cast<T>(0)) ? GL_FALSE : GL_TRUE;
            }
        }
    }
}

template <typename T>
void ProgramD3D::setUniformInternal(GLint location, GLsizei count, const T *v, GLenum uniformType)
{
    const gl::VariableLocation &locationInfo = mState.getUniformLocations()[location];
    D3DUniform *targetUniform                = mD3DUniforms[locationInfo.index];

    if (targetUniform->typeInfo.isSampler)
    {
        ASSERT(uniformType == GL_INT);
        size_t size = count * sizeof(T);
        GLint *dest = &targetUniform->mSamplerData[locationInfo.arrayIndex];
        if (memcmp(dest, v, size) != 0)
        {
            memcpy(dest, v, size);
            mDirtySamplerMapping = true;
        }
        return;
    }

    if (targetUniform->vsData)
    {
        setUniformImpl(locationInfo, count, v, targetUniform->vsData, uniformType);
        mVertexUniformsDirty = true;
    }

    if (targetUniform->psData)
    {
        setUniformImpl(locationInfo, count, v, targetUniform->psData, uniformType);
        mFragmentUniformsDirty = true;
    }

    if (targetUniform->csData)
    {
        setUniformImpl(locationInfo, count, v, targetUniform->csData, uniformType);
        mComputeUniformsDirty = true;
    }
}

template <int cols, int rows>
bool ProgramD3D::setUniformMatrixfvImpl(GLint location,
                                        GLsizei countIn,
                                        GLboolean transpose,
                                        const GLfloat *value,
                                        uint8_t *targetData,
                                        GLenum targetUniformType)
{
    D3DUniform *targetUniform = getD3DUniformFromLocation(location);

    unsigned int elementCount       = targetUniform->getArraySizeProduct();
    unsigned int arrayElementOffset = mState.getUniformLocations()[location].arrayIndex;
    unsigned int count =
        std::min(elementCount - arrayElementOffset, static_cast<unsigned int>(countIn));

    const unsigned int targetMatrixStride = (4 * rows);
    GLfloat *target                       = reinterpret_cast<GLfloat *>(
        targetData + arrayElementOffset * sizeof(GLfloat) * targetMatrixStride);

    bool dirty = false;

    for (unsigned int i = 0; i < count; i++)
    {
        // Internally store matrices as transposed versions to accomodate HLSL matrix indexing
        if (transpose == GL_FALSE)
        {
            dirty = TransposeExpandMatrix<GLfloat, cols, rows>(target, value) || dirty;
        }
        else
        {
            dirty = ExpandMatrix<GLfloat, cols, rows>(target, value) || dirty;
        }
        target += targetMatrixStride;
        value += cols * rows;
    }

    return dirty;
}

template <int cols, int rows>
void ProgramD3D::setUniformMatrixfvInternal(GLint location,
                                            GLsizei countIn,
                                            GLboolean transpose,
                                            const GLfloat *value,
                                            GLenum targetUniformType)
{
    D3DUniform *targetUniform = getD3DUniformFromLocation(location);

    if (targetUniform->vsData)
    {
        if (setUniformMatrixfvImpl<cols, rows>(location, countIn, transpose, value,
                                               targetUniform->vsData, targetUniformType))
        {
            mVertexUniformsDirty = true;
        }
    }

    if (targetUniform->psData)
    {
        if (setUniformMatrixfvImpl<cols, rows>(location, countIn, transpose, value,
                                               targetUniform->psData, targetUniformType))
        {
            mFragmentUniformsDirty = true;
        }
    }

    if (targetUniform->csData)
    {
        if (setUniformMatrixfvImpl<cols, rows>(location, countIn, transpose, value,
                                               targetUniform->csData, targetUniformType))
        {
            mComputeUniformsDirty = true;
        }
    }
}

void ProgramD3D::assignAllSamplerRegisters()
{
    for (size_t uniformIndex = 0; uniformIndex < mD3DUniforms.size(); ++uniformIndex)
    {
        if (mD3DUniforms[uniformIndex]->isSampler())
        {
            assignSamplerRegisters(uniformIndex);
        }
    }
}

void ProgramD3D::assignSamplerRegisters(size_t uniformIndex)
{
    D3DUniform *d3dUniform = mD3DUniforms[uniformIndex];
    ASSERT(d3dUniform->isSampler());
    // If the uniform is an array of arrays, then we have separate entries for each inner array in
    // mD3DUniforms. However, the sampler register info is stored in the shader only for the
    // outermost array.
    std::vector<unsigned int> subscripts;
    const std::string baseName  = gl::ParseResourceName(d3dUniform->name, &subscripts);
    unsigned int registerOffset = mState.getUniforms()[uniformIndex].flattenedOffsetInParentArrays *
                                  d3dUniform->getArraySizeProduct();

    const gl::Shader *computeShader = mState.getAttachedComputeShader();
    if (computeShader)
    {
        const ShaderD3D *computeShaderD3D = GetImplAs<ShaderD3D>(mState.getAttachedComputeShader());
        ASSERT(computeShaderD3D->hasUniform(baseName));
        d3dUniform->csRegisterIndex =
            computeShaderD3D->getUniformRegister(baseName) + registerOffset;
        ASSERT(d3dUniform->csRegisterIndex != GL_INVALID_INDEX);
        AssignSamplers(d3dUniform->csRegisterIndex, d3dUniform->typeInfo,
                       d3dUniform->getArraySizeProduct(), mSamplersCS, &mUsedComputeSamplerRange);
    }
    else
    {
        const ShaderD3D *vertexShaderD3D = GetImplAs<ShaderD3D>(mState.getAttachedVertexShader());
        const ShaderD3D *fragmentShaderD3D =
            GetImplAs<ShaderD3D>(mState.getAttachedFragmentShader());
        ASSERT(vertexShaderD3D->hasUniform(baseName) || fragmentShaderD3D->hasUniform(baseName));
        if (vertexShaderD3D->hasUniform(baseName))
        {
            d3dUniform->vsRegisterIndex =
                vertexShaderD3D->getUniformRegister(baseName) + registerOffset;
            ASSERT(d3dUniform->vsRegisterIndex != GL_INVALID_INDEX);
            AssignSamplers(d3dUniform->vsRegisterIndex, d3dUniform->typeInfo,
                           d3dUniform->getArraySizeProduct(), mSamplersVS,
                           &mUsedVertexSamplerRange);
        }
        if (fragmentShaderD3D->hasUniform(baseName))
        {
            d3dUniform->psRegisterIndex =
                fragmentShaderD3D->getUniformRegister(baseName) + registerOffset;
            ASSERT(d3dUniform->psRegisterIndex != GL_INVALID_INDEX);
            AssignSamplers(d3dUniform->psRegisterIndex, d3dUniform->typeInfo,
                           d3dUniform->getArraySizeProduct(), mSamplersPS, &mUsedPixelSamplerRange);
        }
    }
}

// static
void ProgramD3D::AssignSamplers(unsigned int startSamplerIndex,
                                const gl::UniformTypeInfo &typeInfo,
                                unsigned int samplerCount,
                                std::vector<Sampler> &outSamplers,
                                GLuint *outUsedRange)
{
    unsigned int samplerIndex = startSamplerIndex;

    do
    {
        ASSERT(samplerIndex < outSamplers.size());
        Sampler *sampler            = &outSamplers[samplerIndex];
        sampler->active             = true;
        sampler->textureType        = typeInfo.samplerTextureType;
        sampler->logicalTextureUnit = 0;
        *outUsedRange               = std::max(samplerIndex + 1, *outUsedRange);
        samplerIndex++;
    } while (samplerIndex < startSamplerIndex + samplerCount);
}

void ProgramD3D::reset()
{
    mVertexExecutables.clear();
    mPixelExecutables.clear();

    for (auto &geometryExecutable : mGeometryExecutables)
    {
        geometryExecutable.reset(nullptr);
    }

    mComputeExecutable.reset(nullptr);

    mVertexHLSL.clear();
    mVertexWorkarounds = angle::CompilerWorkaroundsD3D();

    mPixelHLSL.clear();
    mPixelWorkarounds = angle::CompilerWorkaroundsD3D();
    mUsesFragDepth = false;
    mHasANGLEMultiviewEnabled = false;
    mUsesViewID               = false;
    mPixelShaderKey.clear();
    mUsesPointSize = false;
    mUsesFlatInterpolation = false;

    SafeDeleteContainer(mD3DUniforms);
    mD3DUniformBlocks.clear();

    mVertexUniformStorage.reset(nullptr);
    mFragmentUniformStorage.reset(nullptr);
    mComputeUniformStorage.reset(nullptr);

    mSamplersPS.clear();
    mSamplersVS.clear();
    mSamplersCS.clear();

    mUsedVertexSamplerRange = 0;
    mUsedPixelSamplerRange  = 0;
    mUsedComputeSamplerRange = 0;
    mDirtySamplerMapping    = true;

    mAttribLocationToD3DSemantic.fill(-1);

    mStreamOutVaryings.clear();

    mGeometryShaderPreamble.clear();

    dirtyAllUniforms();

    mCachedPixelExecutableIndex.reset();
    mCachedVertexExecutableIndex.reset();
}

unsigned int ProgramD3D::getSerial() const
{
    return mSerial;
}

unsigned int ProgramD3D::issueSerial()
{
    return mCurrentSerial++;
}

void ProgramD3D::initAttribLocationsToD3DSemantic(const gl::Context *context)
{
    gl::Shader *vertexShader = mState.getAttachedVertexShader();
    ASSERT(vertexShader != nullptr);

    // Init semantic index
    int semanticIndex = 0;
    for (const sh::Attribute &attribute : vertexShader->getActiveAttributes(context))
    {
        int regCount    = gl::VariableRegisterCount(attribute.type);
        GLuint location = mState.getAttributeLocation(attribute.name);
        ASSERT(location != std::numeric_limits<GLuint>::max());

        for (int reg = 0; reg < regCount; ++reg)
        {
            mAttribLocationToD3DSemantic[location + reg] = semanticIndex++;
        }
    }
}

void ProgramD3D::updateCachedInputLayout(Serial associatedSerial, const gl::State &state)
{
    if (mCurrentVertexArrayStateSerial == associatedSerial)
    {
        return;
    }

    mCurrentVertexArrayStateSerial = associatedSerial;
    mCachedInputLayout.clear();

    const auto &vertexAttributes = state.getVertexArray()->getVertexAttributes();

    for (size_t locationIndex : mState.getActiveAttribLocationsMask())
    {
        int d3dSemantic = mAttribLocationToD3DSemantic[locationIndex];

        if (d3dSemantic != -1)
        {
            if (mCachedInputLayout.size() < static_cast<size_t>(d3dSemantic + 1))
            {
                mCachedInputLayout.resize(d3dSemantic + 1, gl::VERTEX_FORMAT_INVALID);
            }
            mCachedInputLayout[d3dSemantic] =
                GetVertexFormatType(vertexAttributes[locationIndex],
                                    state.getVertexAttribCurrentValue(locationIndex).Type);
        }
    }

    VertexExecutable::getSignature(mRenderer, mCachedInputLayout, &mCachedVertexSignature);

    updateCachedVertexExecutableIndex();
}

void ProgramD3D::updateCachedOutputLayout(const gl::Context *context,
                                          const gl::Framebuffer *framebuffer)
{
    mPixelShaderOutputLayoutCache.clear();

    FramebufferD3D *fboD3D   = GetImplAs<FramebufferD3D>(framebuffer);
    const auto &colorbuffers = fboD3D->getColorAttachmentsForRender(context);

    for (size_t colorAttachment = 0; colorAttachment < colorbuffers.size(); ++colorAttachment)
    {
        const gl::FramebufferAttachment *colorbuffer = colorbuffers[colorAttachment];

        if (colorbuffer)
        {
            auto binding = colorbuffer->getBinding() == GL_BACK ? GL_COLOR_ATTACHMENT0
                                                                : colorbuffer->getBinding();
            mPixelShaderOutputLayoutCache.push_back(binding);
        }
        else
        {
            mPixelShaderOutputLayoutCache.push_back(GL_NONE);
        }
    }

    updateCachedPixelExecutableIndex();
}

void ProgramD3D::gatherTransformFeedbackVaryings(const gl::VaryingPacking &varyingPacking,
                                                 const BuiltinInfo &builtins)
{
    const std::string &varyingSemantic =
        GetVaryingSemantic(mRenderer->getMajorShaderModel(), usesPointSize());

    // Gather the linked varyings that are used for transform feedback, they should all exist.
    mStreamOutVaryings.clear();

    const auto &tfVaryingNames = mState.getTransformFeedbackVaryingNames();
    for (unsigned int outputSlot = 0; outputSlot < static_cast<unsigned int>(tfVaryingNames.size());
         ++outputSlot)
    {
        const auto &tfVaryingName = tfVaryingNames[outputSlot];
        if (tfVaryingName == "gl_Position")
        {
            if (builtins.glPosition.enabled)
            {
                mStreamOutVaryings.push_back(D3DVarying(builtins.glPosition.semantic,
                                                        builtins.glPosition.index, 4, outputSlot));
            }
        }
        else if (tfVaryingName == "gl_FragCoord")
        {
            if (builtins.glFragCoord.enabled)
            {
                mStreamOutVaryings.push_back(D3DVarying(builtins.glFragCoord.semantic,
                                                        builtins.glFragCoord.index, 4, outputSlot));
            }
        }
        else if (tfVaryingName == "gl_PointSize")
        {
            if (builtins.glPointSize.enabled)
            {
                mStreamOutVaryings.push_back(D3DVarying("PSIZE", 0, 1, outputSlot));
            }
        }
        else
        {
            std::vector<unsigned int> subscripts;
            std::string baseName = gl::ParseResourceName(tfVaryingName, &subscripts);
            size_t subscript     = GL_INVALID_INDEX;
            if (!subscripts.empty())
            {
                subscript = subscripts.back();
            }
            for (const auto &registerInfo : varyingPacking.getRegisterList())
            {
                const auto &varying   = *registerInfo.packedVarying->varying;
                GLenum transposedType = gl::TransposeMatrixType(varying.type);
                int componentCount = gl::VariableColumnCount(transposedType);
                ASSERT(!varying.isBuiltIn());

                // Transform feedback for varying structs is underspecified.
                // See Khronos bug 9856.
                // TODO(jmadill): Figure out how to be spec-compliant here.
                if (registerInfo.packedVarying->isStructField() || varying.isStruct())
                    continue;

                // There can be more than one register assigned to a particular varying, and each
                // register needs its own stream out entry.
                if (baseName == registerInfo.packedVarying->varying->name &&
                    (subscript == GL_INVALID_INDEX || subscript == registerInfo.varyingArrayIndex))
                {
                    mStreamOutVaryings.push_back(D3DVarying(
                        varyingSemantic, registerInfo.semanticIndex, componentCount, outputSlot));
                }
            }
        }
    }
}

D3DUniform *ProgramD3D::getD3DUniformFromLocation(GLint location)
{
    return mD3DUniforms[mState.getUniformLocations()[location].index];
}

const D3DUniform *ProgramD3D::getD3DUniformFromLocation(GLint location) const
{
    return mD3DUniforms[mState.getUniformLocations()[location].index];
}

void ProgramD3D::setPathFragmentInputGen(const std::string &inputName,
                                         GLenum genMode,
                                         GLint components,
                                         const GLfloat *coeffs)
{
    UNREACHABLE();
}

bool ProgramD3D::hasVertexExecutableForCachedInputLayout()
{
    return mCachedVertexExecutableIndex.valid();
}

bool ProgramD3D::hasGeometryExecutableForPrimitiveType(GLenum drawMode)
{
    if (!usesGeometryShader(drawMode))
    {
        // No shader necessary mean we have the required (null) executable.
        return true;
    }

    gl::PrimitiveType geometryShaderType = GetGeometryShaderTypeFromDrawMode(drawMode);
    return mGeometryExecutables[geometryShaderType].get() != nullptr;
}

bool ProgramD3D::hasPixelExecutableForCachedOutputLayout()
{
    return mCachedPixelExecutableIndex.valid();
}

template <typename DestT>
void ProgramD3D::getUniformInternal(GLint location, DestT *dataOut) const
{
    const gl::VariableLocation &locationInfo = mState.getUniformLocations()[location];
    const gl::LinkedUniform &uniform         = mState.getUniforms()[locationInfo.index];

    const D3DUniform *targetUniform = getD3DUniformFromLocation(location);
    const uint8_t *srcPointer       = targetUniform->getDataPtrToElement(locationInfo.arrayIndex);

    if (gl::IsMatrixType(uniform.type))
    {
        GetMatrixUniform(gl::VariableColumnCount(uniform.type), gl::VariableRowCount(uniform.type),
                         dataOut, reinterpret_cast<const DestT *>(srcPointer));
    }
    else
    {
        memcpy(dataOut, srcPointer, uniform.getElementSize());
    }
}

void ProgramD3D::getUniformfv(const gl::Context *context, GLint location, GLfloat *params) const
{
    getUniformInternal(location, params);
}

void ProgramD3D::getUniformiv(const gl::Context *context, GLint location, GLint *params) const
{
    getUniformInternal(location, params);
}

void ProgramD3D::getUniformuiv(const gl::Context *context, GLint location, GLuint *params) const
{
    getUniformInternal(location, params);
}

void ProgramD3D::updateCachedVertexExecutableIndex()
{
    mCachedVertexExecutableIndex.reset();
    for (size_t executableIndex = 0; executableIndex < mVertexExecutables.size(); executableIndex++)
    {
        if (mVertexExecutables[executableIndex]->matchesSignature(mCachedVertexSignature))
        {
            mCachedVertexExecutableIndex = executableIndex;
            break;
        }
    }
}

void ProgramD3D::updateCachedPixelExecutableIndex()
{
    mCachedPixelExecutableIndex.reset();
    for (size_t executableIndex = 0; executableIndex < mPixelExecutables.size(); executableIndex++)
    {
        if (mPixelExecutables[executableIndex]->matchesSignature(mPixelShaderOutputLayoutCache))
        {
            mCachedPixelExecutableIndex = executableIndex;
            break;
        }
    }
}

void ProgramD3D::linkResources(const gl::Context *context,
                               const gl::ProgramLinkedResources &resources)
{
    UniformBlockInfo uniformBlockInfo;

    if (mState.getAttachedVertexShader())
    {
        uniformBlockInfo.getShaderBlockInfo(context, mState.getAttachedVertexShader());
    }

    if (mState.getAttachedFragmentShader())
    {
        uniformBlockInfo.getShaderBlockInfo(context, mState.getAttachedFragmentShader());
    }

    if (mState.getAttachedComputeShader())
    {
        uniformBlockInfo.getShaderBlockInfo(context, mState.getAttachedComputeShader());
    }

    // Gather interface block info.
    auto getUniformBlockSize = [&uniformBlockInfo](const std::string &name,
                                                   const std::string &mappedName, size_t *sizeOut) {
        return uniformBlockInfo.getBlockSize(name, mappedName, sizeOut);
    };

    auto getUniformBlockMemberInfo = [&uniformBlockInfo](const std::string &name,
                                                         const std::string &mappedName,
                                                         sh::BlockMemberInfo *infoOut) {
        return uniformBlockInfo.getBlockMemberInfo(name, mappedName, infoOut);
    };

    resources.uniformBlockLinker.linkBlocks(getUniformBlockSize, getUniformBlockMemberInfo);
    initializeUniformBlocks();

    // TODO(jiajia.qin@intel.com): Determine correct shader storage block info.
    auto getShaderStorageBlockSize = [](const std::string &name, const std::string &mappedName,
                                        size_t *sizeOut) {
        *sizeOut = 0;
        return true;
    };

    auto getShaderStorageBlockMemberInfo =
        [](const std::string &name, const std::string &mappedName, sh::BlockMemberInfo *infoOut) {
            *infoOut = sh::BlockMemberInfo::getDefaultBlockInfo();
            return true;
        };

    resources.shaderStorageBlockLinker.linkBlocks(getShaderStorageBlockSize,
                                                  getShaderStorageBlockMemberInfo);
}

}  // namespace rx
