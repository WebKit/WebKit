//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramD3D.cpp: Defines the rx::ProgramD3D class which implements rx::ProgramImpl.

#include "libANGLE/renderer/d3d/ProgramD3D.h"

#include "common/bitset_utils.h"
#include "common/utilities.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/Program.h"
#include "libANGLE/Uniform.h"
#include "libANGLE/VaryingPacking.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/features.h"
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

gl::InputLayout GetDefaultInputLayoutFromShader(const gl::Shader *vertexShader)
{
    gl::InputLayout defaultLayout;
    for (const sh::Attribute &shaderAttr : vertexShader->getActiveAttributes())
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

                defaultLayout.push_back(defaultType);
            }
        }
    }

    return defaultLayout;
}

std::vector<GLenum> GetDefaultOutputLayoutFromShader(
    const std::vector<PixelShaderOutputVariable> &shaderOutputVars)
{
    std::vector<GLenum> defaultPixelOutput;

    if (!shaderOutputVars.empty())
    {
        defaultPixelOutput.push_back(GL_COLOR_ATTACHMENT0 +
                                     static_cast<unsigned int>(shaderOutputVars[0].outputIndex));
    }

    return defaultPixelOutput;
}

bool IsRowMajorLayout(const sh::InterfaceBlockField &var)
{
    return var.isRowMajorLayout;
}

bool IsRowMajorLayout(const sh::ShaderVariable &var)
{
    return false;
}

template <typename VarT>
void GetUniformBlockInfo(const std::vector<VarT> &fields,
                         const std::string &prefix,
                         sh::BlockLayoutEncoder *encoder,
                         bool inRowMajorLayout,
                         std::map<std::string, sh::BlockMemberInfo> *blockInfoOut)
{
    for (const VarT &field : fields)
    {
        const std::string &fieldName = (prefix.empty() ? field.name : prefix + "." + field.name);

        if (field.isStruct())
        {
            bool rowMajorLayout = (inRowMajorLayout || IsRowMajorLayout(field));

            for (unsigned int arrayElement = 0; arrayElement < field.elementCount(); arrayElement++)
            {
                encoder->enterAggregateType();

                const std::string uniformElementName =
                    fieldName + (field.isArray() ? ArrayString(arrayElement) : "");
                GetUniformBlockInfo(field.fields, uniformElementName, encoder, rowMajorLayout,
                                    blockInfoOut);

                encoder->exitAggregateType();
            }
        }
        else
        {
            bool isRowMajorMatrix = (gl::IsMatrixType(field.type) && inRowMajorLayout);
            (*blockInfoOut)[fieldName] =
                encoder->encodeType(field.type, field.arraySize, isRowMajorMatrix);
        }
    }
}

template <typename T>
static inline void SetIfDirty(T *dest, const T &source, bool *dirtyFlag)
{
    ASSERT(dest != NULL);
    ASSERT(dirtyFlag != NULL);

    *dirtyFlag = *dirtyFlag || (memcmp(dest, &source, sizeof(T)) != 0);
    *dest      = source;
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

}  // anonymous namespace

// D3DUniform Implementation

D3DUniform::D3DUniform(GLenum typeIn,
                       const std::string &nameIn,
                       unsigned int arraySizeIn,
                       bool defaultBlock)
    : type(typeIn),
      name(nameIn),
      arraySize(arraySizeIn),
      data(nullptr),
      dirty(true),
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
        size_t bytes = gl::VariableInternalSize(type) * elementCount();
        data = new uint8_t[bytes];
        memset(data, 0, bytes);

        // Use the row count as register count, will work for non-square matrices.
        registerCount = gl::VariableRowCount(type) * elementCount();
    }
}

D3DUniform::~D3DUniform()
{
    SafeDeleteArray(data);
}

bool D3DUniform::isSampler() const
{
    return gl::IsSamplerType(type);
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
    return (mFragmentShader->usesFragColor() && data.getClientMajorVersion() < 3);
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
      mSerial(issueSerial())
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

bool ProgramD3D::usesGeometryShader(GLenum drawMode) const
{
    if (drawMode != GL_POINTS)
    {
        return mUsesFlatInterpolation;
    }

    return usesPointSpriteEmulation() && !usesInstancedPointSpriteEmulation();
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

void ProgramD3D::updateSamplerMapping()
{
    if (!mDirtySamplerMapping)
    {
        return;
    }

    mDirtySamplerMapping = false;

    // Retrieve sampler uniform values
    for (const D3DUniform *d3dUniform : mD3DUniforms)
    {
        if (!d3dUniform->dirty)
            continue;

        if (!d3dUniform->isSampler())
            continue;

        int count = d3dUniform->elementCount();
        const GLint(*v)[4] = reinterpret_cast<const GLint(*)[4]>(d3dUniform->data);

        if (d3dUniform->isReferencedByFragmentShader())
        {
            unsigned int firstIndex = d3dUniform->psRegisterIndex;

            for (int i = 0; i < count; i++)
            {
                unsigned int samplerIndex = firstIndex + i;

                if (samplerIndex < mSamplersPS.size())
                {
                    ASSERT(mSamplersPS[samplerIndex].active);
                    mSamplersPS[samplerIndex].logicalTextureUnit = v[i][0];
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
                    mSamplersVS[samplerIndex].logicalTextureUnit = v[i][0];
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
                    mSamplersCS[samplerIndex].logicalTextureUnit = v[i][0];
                }
            }
        }
    }
}

LinkResult ProgramD3D::load(const ContextImpl *contextImpl,
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
            new D3DUniform(linkedUniform.type, linkedUniform.name, linkedUniform.arraySize,
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

        ANGLE_TRY(mRenderer->loadExecutable(vertexShaderFunction, vertexShaderSize, SHADER_VERTEX,
                                            mStreamOutVaryings, separateAttribs,
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

        ANGLE_TRY(mRenderer->loadExecutable(pixelShaderFunction, pixelShaderSize, SHADER_PIXEL,
                                            mStreamOutVaryings, separateAttribs,
                                            &shaderExecutable));

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
                                            SHADER_GEOMETRY, mStreamOutVaryings, separateAttribs,
                                            &geometryExecutable));

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
                                            SHADER_COMPUTE, std::vector<D3DVarying>(), false,
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

gl::Error ProgramD3D::save(gl::BinaryOutputStream *stream)
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

    // Ensure we init the uniform block structure data if we should.
    // http://anglebug.com/1637
    ensureUniformBlocksInitialized();

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

    return gl::NoError();
}

void ProgramD3D::setBinaryRetrievableHint(bool /* retrievable */)
{
}

void ProgramD3D::setSeparable(bool /* separable */)
{
}

gl::Error ProgramD3D::getPixelExecutableForFramebuffer(const gl::Framebuffer *fbo,
                                                       ShaderExecutableD3D **outExecutable)
{
    mPixelShaderOutputFormatCache.clear();

    const FramebufferD3D *fboD3D = GetImplAs<FramebufferD3D>(fbo);
    const gl::AttachmentList &colorbuffers = fboD3D->getColorAttachmentsForRender();

    for (size_t colorAttachment = 0; colorAttachment < colorbuffers.size(); ++colorAttachment)
    {
        const gl::FramebufferAttachment *colorbuffer = colorbuffers[colorAttachment];

        if (colorbuffer)
        {
            mPixelShaderOutputFormatCache.push_back(colorbuffer->getBinding() == GL_BACK
                                                        ? GL_COLOR_ATTACHMENT0
                                                        : colorbuffer->getBinding());
        }
        else
        {
            mPixelShaderOutputFormatCache.push_back(GL_NONE);
        }
    }

    return getPixelExecutableForOutputLayout(mPixelShaderOutputFormatCache, outExecutable, nullptr);
}

gl::Error ProgramD3D::getPixelExecutableForOutputLayout(const std::vector<GLenum> &outputSignature,
                                                        ShaderExecutableD3D **outExectuable,
                                                        gl::InfoLog *infoLog)
{
    for (size_t executableIndex = 0; executableIndex < mPixelExecutables.size(); executableIndex++)
    {
        if (mPixelExecutables[executableIndex]->matchesSignature(outputSignature))
        {
            *outExectuable = mPixelExecutables[executableIndex]->shaderExecutable();
            return gl::NoError();
        }
    }

    std::string finalPixelHLSL = mDynamicHLSL->generatePixelShaderForOutputSignature(
        mPixelHLSL, mPixelShaderKey, mUsesFragDepth, outputSignature);

    // Generate new pixel executable
    ShaderExecutableD3D *pixelExecutable = NULL;

    gl::InfoLog tempInfoLog;
    gl::InfoLog *currentInfoLog = infoLog ? infoLog : &tempInfoLog;

    ANGLE_TRY(mRenderer->compileToExecutable(
        *currentInfoLog, finalPixelHLSL, SHADER_PIXEL, mStreamOutVaryings,
        (mState.getTransformFeedbackBufferMode() == GL_SEPARATE_ATTRIBS), mPixelWorkarounds,
        &pixelExecutable));

    if (pixelExecutable)
    {
        mPixelExecutables.push_back(std::unique_ptr<PixelExecutable>(
            new PixelExecutable(outputSignature, pixelExecutable)));
    }
    else if (!infoLog)
    {
        ERR() << "Error compiling dynamic pixel executable:" << std::endl
              << tempInfoLog.str() << std::endl;
    }

    *outExectuable = pixelExecutable;
    return gl::NoError();
}

gl::Error ProgramD3D::getVertexExecutableForInputLayout(const gl::InputLayout &inputLayout,
                                                        ShaderExecutableD3D **outExectuable,
                                                        gl::InfoLog *infoLog)
{
    VertexExecutable::getSignature(mRenderer, inputLayout, &mCachedVertexSignature);

    for (size_t executableIndex = 0; executableIndex < mVertexExecutables.size(); executableIndex++)
    {
        if (mVertexExecutables[executableIndex]->matchesSignature(mCachedVertexSignature))
        {
            *outExectuable = mVertexExecutables[executableIndex]->shaderExecutable();
            return gl::NoError();
        }
    }

    // Generate new dynamic layout with attribute conversions
    std::string finalVertexHLSL = mDynamicHLSL->generateVertexShaderForInputLayout(
        mVertexHLSL, inputLayout, mState.getAttributes());

    // Generate new vertex executable
    ShaderExecutableD3D *vertexExecutable = NULL;

    gl::InfoLog tempInfoLog;
    gl::InfoLog *currentInfoLog = infoLog ? infoLog : &tempInfoLog;

    ANGLE_TRY(mRenderer->compileToExecutable(
        *currentInfoLog, finalVertexHLSL, SHADER_VERTEX, mStreamOutVaryings,
        (mState.getTransformFeedbackBufferMode() == GL_SEPARATE_ATTRIBS), mVertexWorkarounds,
        &vertexExecutable));

    if (vertexExecutable)
    {
        mVertexExecutables.push_back(std::unique_ptr<VertexExecutable>(
            new VertexExecutable(inputLayout, mCachedVertexSignature, vertexExecutable)));
    }
    else if (!infoLog)
    {
        ERR() << "Error compiling dynamic vertex executable:" << std::endl
              << tempInfoLog.str() << std::endl;
    }

    *outExectuable = vertexExecutable;
    return gl::NoError();
}

gl::Error ProgramD3D::getGeometryExecutableForPrimitiveType(const gl::ContextState &data,
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
        geometryShaderType, data, mState, mRenderer->presentPathFastEnabled(),
        mGeometryShaderPreamble);

    gl::InfoLog tempInfoLog;
    gl::InfoLog *currentInfoLog = infoLog ? infoLog : &tempInfoLog;

    ShaderExecutableD3D *geometryExecutable = nullptr;
    gl::Error error                         = mRenderer->compileToExecutable(
        *currentInfoLog, geometryHLSL, SHADER_GEOMETRY, mStreamOutVaryings,
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
        : mProgram(program), mError(GL_NO_ERROR), mInfoLog(), mResult(nullptr)
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
    GetVertexExecutableTask(ProgramD3D *program) : GetExecutableTask(program) {}
    gl::Error run() override
    {
        const auto &defaultInputLayout =
            GetDefaultInputLayoutFromShader(mProgram->mState.getAttachedVertexShader());

        ANGLE_TRY(
            mProgram->getVertexExecutableForInputLayout(defaultInputLayout, &mResult, &mInfoLog));

        return gl::NoError();
    }
};

class ProgramD3D::GetPixelExecutableTask : public ProgramD3D::GetExecutableTask
{
  public:
    GetPixelExecutableTask(ProgramD3D *program) : GetExecutableTask(program) {}
    gl::Error run() override
    {
        const auto &defaultPixelOutput =
            GetDefaultOutputLayoutFromShader(mProgram->getPixelShaderKey());

        ANGLE_TRY(
            mProgram->getPixelExecutableForOutputLayout(defaultPixelOutput, &mResult, &mInfoLog));

        return gl::NoError();
    }
};

class ProgramD3D::GetGeometryExecutableTask : public ProgramD3D::GetExecutableTask
{
  public:
    GetGeometryExecutableTask(ProgramD3D *program, const gl::ContextState &contextState)
        : GetExecutableTask(program), mContextState(contextState)
    {
    }

    gl::Error run() override
    {
        // Auto-generate the geometry shader here, if we expect to be using point rendering in
        // D3D11.
        if (mProgram->usesGeometryShader(GL_POINTS))
        {
            ANGLE_TRY(mProgram->getGeometryExecutableForPrimitiveType(mContextState, GL_POINTS,
                                                                      &mResult, &mInfoLog));
        }

        return gl::NoError();
    }

  private:
    const gl::ContextState &mContextState;
};

LinkResult ProgramD3D::compileProgramExecutables(const gl::ContextState &contextState,
                                                 gl::InfoLog &infoLog)
{
    // Ensure the compiler is initialized to avoid race conditions.
    ANGLE_TRY(mRenderer->ensureHLSLCompilerInitialized());

    WorkerThreadPool *workerPool = mRenderer->getWorkerThreadPool();

    GetVertexExecutableTask vertexTask(this);
    GetPixelExecutableTask pixelTask(this);
    GetGeometryExecutableTask geometryTask(this, contextState);

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

LinkResult ProgramD3D::compileComputeExecutable(gl::InfoLog &infoLog)
{
    // Ensure the compiler is initialized to avoid race conditions.
    ANGLE_TRY(mRenderer->ensureHLSLCompilerInitialized());

    std::string computeShader = mDynamicHLSL->generateComputeShaderLinkHLSL(mState);

    ShaderExecutableD3D *computeExecutable = nullptr;
    ANGLE_TRY(mRenderer->compileToExecutable(infoLog, computeShader, SHADER_COMPUTE,
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

LinkResult ProgramD3D::link(ContextImpl *contextImpl,
                            const gl::VaryingPacking &packing,
                            gl::InfoLog &infoLog)
{
    const auto &data = contextImpl->getContextState();

    reset();

    const gl::Shader *computeShader = mState.getAttachedComputeShader();
    if (computeShader)
    {
        mSamplersCS.resize(data.getCaps().maxComputeTextureImageUnits);

        defineUniformsAndAssignRegisters();

        LinkResult result = compileComputeExecutable(infoLog);
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

        initUniformBlockInfo(computeShader);
    }
    else
    {
        const gl::Shader *vertexShader   = mState.getAttachedVertexShader();
        const gl::Shader *fragmentShader = mState.getAttachedFragmentShader();

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
            packing.getMaxSemanticIndex() > data.getCaps().maxVaryingVectors)
        {
            infoLog << "Cannot pack these varyings on D3D9.";
            return false;
        }

        ProgramD3DMetadata metadata(mRenderer, vertexShaderD3D, fragmentShaderD3D);
        BuiltinVaryingsD3D builtins(metadata, packing);

        mDynamicHLSL->generateShaderLinkHLSL(data, mState, metadata, packing, builtins, &mPixelHLSL,
                                             &mVertexHLSL);

        mUsesPointSize = vertexShaderD3D->usesPointSize();
        mDynamicHLSL->getPixelShaderOutputKey(data, mState, metadata, &mPixelShaderKey);
        mUsesFragDepth = metadata.usesFragDepth();

        // Cache if we use flat shading
        mUsesFlatInterpolation = (FindFlatInterpolationVarying(fragmentShader->getVaryings()) ||
                                  FindFlatInterpolationVarying(vertexShader->getVaryings()));

        if (mRenderer->getMajorShaderModel() >= 4)
        {
            mGeometryShaderPreamble =
                mDynamicHLSL->generateGeometryShaderPreamble(packing, builtins);
        }

        initAttribLocationsToD3DSemantic();

        defineUniformsAndAssignRegisters();

        gatherTransformFeedbackVaryings(packing, builtins[SHADER_VERTEX]);

        LinkResult result = compileProgramExecutables(data, infoLog);
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

        initUniformBlockInfo(vertexShader);
        initUniformBlockInfo(fragmentShader);
    }

    return true;
}

GLboolean ProgramD3D::validate(const gl::Caps & /*caps*/, gl::InfoLog * /*infoLog*/)
{
    // TODO(jmadill): Do something useful here?
    return GL_TRUE;
}

void ProgramD3D::initUniformBlockInfo(const gl::Shader *shader)
{
    for (const sh::InterfaceBlock &interfaceBlock : shader->getInterfaceBlocks())
    {
        if (!interfaceBlock.staticUse && interfaceBlock.layout == sh::BLOCKLAYOUT_PACKED)
            continue;

        if (mBlockDataSizes.count(interfaceBlock.name) > 0)
            continue;

        size_t dataSize                      = getUniformBlockInfo(interfaceBlock);
        mBlockDataSizes[interfaceBlock.name] = dataSize;
    }
}

void ProgramD3D::ensureUniformBlocksInitialized()
{
    // Lazy init.
    if (mState.getUniformBlocks().empty() || !mD3DUniformBlocks.empty())
    {
        return;
    }

    // Assign registers and update sizes.
    const ShaderD3D *vertexShaderD3D = SafeGetImplAs<ShaderD3D>(mState.getAttachedVertexShader());
    const ShaderD3D *fragmentShaderD3D =
        SafeGetImplAs<ShaderD3D>(mState.getAttachedFragmentShader());
    const ShaderD3D *computeShaderD3D = SafeGetImplAs<ShaderD3D>(mState.getAttachedComputeShader());

    for (const gl::UniformBlock &uniformBlock : mState.getUniformBlocks())
    {
        unsigned int uniformBlockElement = uniformBlock.isArray ? uniformBlock.arrayElement : 0;

        D3DUniformBlock d3dUniformBlock;

        if (uniformBlock.vertexStaticUse)
        {
            ASSERT(vertexShaderD3D != nullptr);
            unsigned int baseRegister =
                vertexShaderD3D->getInterfaceBlockRegister(uniformBlock.name);
            d3dUniformBlock.vsRegisterIndex = baseRegister + uniformBlockElement;
        }

        if (uniformBlock.fragmentStaticUse)
        {
            ASSERT(fragmentShaderD3D != nullptr);
            unsigned int baseRegister =
                fragmentShaderD3D->getInterfaceBlockRegister(uniformBlock.name);
            d3dUniformBlock.psRegisterIndex = baseRegister + uniformBlockElement;
        }

        if (uniformBlock.computeStaticUse)
        {
            ASSERT(computeShaderD3D != nullptr);
            unsigned int baseRegister =
                computeShaderD3D->getInterfaceBlockRegister(uniformBlock.name);
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
}

gl::Error ProgramD3D::applyUniforms(GLenum drawMode)
{
    ASSERT(!mDirtySamplerMapping);

    ANGLE_TRY(mRenderer->applyUniforms(*this, drawMode, mD3DUniforms));

    for (D3DUniform *d3dUniform : mD3DUniforms)
    {
        d3dUniform->dirty = false;
    }

    return gl::NoError();
}

gl::Error ProgramD3D::applyUniformBuffers(const gl::ContextState &data)
{
    if (mState.getUniformBlocks().empty())
    {
        return gl::NoError();
    }

    ensureUniformBlocksInitialized();

    mVertexUBOCache.clear();
    mFragmentUBOCache.clear();

    const unsigned int reservedBuffersInVS = mRenderer->getReservedVertexUniformBuffers();
    const unsigned int reservedBuffersInFS = mRenderer->getReservedFragmentUniformBuffers();

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
            unsigned int registerIndex = uniformBlock.vsRegisterIndex - reservedBuffersInVS;
            ASSERT(registerIndex < data.getCaps().maxVertexUniformBlocks);

            if (mVertexUBOCache.size() <= registerIndex)
            {
                mVertexUBOCache.resize(registerIndex + 1, -1);
            }

            ASSERT(mVertexUBOCache[registerIndex] == -1);
            mVertexUBOCache[registerIndex] = blockBinding;
        }

        if (uniformBlock.fragmentStaticUse())
        {
            unsigned int registerIndex = uniformBlock.psRegisterIndex - reservedBuffersInFS;
            ASSERT(registerIndex < data.getCaps().maxFragmentUniformBlocks);

            if (mFragmentUBOCache.size() <= registerIndex)
            {
                mFragmentUBOCache.resize(registerIndex + 1, -1);
            }

            ASSERT(mFragmentUBOCache[registerIndex] == -1);
            mFragmentUBOCache[registerIndex] = blockBinding;
        }
    }

    return mRenderer->setUniformBuffers(data, mVertexUBOCache, mFragmentUBOCache);
}

void ProgramD3D::dirtyAllUniforms()
{
    for (D3DUniform *d3dUniform : mD3DUniforms)
    {
        d3dUniform->dirty = true;
    }
}

void ProgramD3D::setUniform1fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniform(location, count, v, GL_FLOAT);
}

void ProgramD3D::setUniform2fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniform(location, count, v, GL_FLOAT_VEC2);
}

void ProgramD3D::setUniform3fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniform(location, count, v, GL_FLOAT_VEC3);
}

void ProgramD3D::setUniform4fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniform(location, count, v, GL_FLOAT_VEC4);
}

void ProgramD3D::setUniformMatrix2fv(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value)
{
    setUniformMatrixfv<2, 2>(location, count, transpose, value, GL_FLOAT_MAT2);
}

void ProgramD3D::setUniformMatrix3fv(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value)
{
    setUniformMatrixfv<3, 3>(location, count, transpose, value, GL_FLOAT_MAT3);
}

void ProgramD3D::setUniformMatrix4fv(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value)
{
    setUniformMatrixfv<4, 4>(location, count, transpose, value, GL_FLOAT_MAT4);
}

void ProgramD3D::setUniformMatrix2x3fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfv<2, 3>(location, count, transpose, value, GL_FLOAT_MAT2x3);
}

void ProgramD3D::setUniformMatrix3x2fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfv<3, 2>(location, count, transpose, value, GL_FLOAT_MAT3x2);
}

void ProgramD3D::setUniformMatrix2x4fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfv<2, 4>(location, count, transpose, value, GL_FLOAT_MAT2x4);
}

void ProgramD3D::setUniformMatrix4x2fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfv<4, 2>(location, count, transpose, value, GL_FLOAT_MAT4x2);
}

void ProgramD3D::setUniformMatrix3x4fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfv<3, 4>(location, count, transpose, value, GL_FLOAT_MAT3x4);
}

void ProgramD3D::setUniformMatrix4x3fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfv<4, 3>(location, count, transpose, value, GL_FLOAT_MAT4x3);
}

void ProgramD3D::setUniform1iv(GLint location, GLsizei count, const GLint *v)
{
    setUniform(location, count, v, GL_INT);
}

void ProgramD3D::setUniform2iv(GLint location, GLsizei count, const GLint *v)
{
    setUniform(location, count, v, GL_INT_VEC2);
}

void ProgramD3D::setUniform3iv(GLint location, GLsizei count, const GLint *v)
{
    setUniform(location, count, v, GL_INT_VEC3);
}

void ProgramD3D::setUniform4iv(GLint location, GLsizei count, const GLint *v)
{
    setUniform(location, count, v, GL_INT_VEC4);
}

void ProgramD3D::setUniform1uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniform(location, count, v, GL_UNSIGNED_INT);
}

void ProgramD3D::setUniform2uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniform(location, count, v, GL_UNSIGNED_INT_VEC2);
}

void ProgramD3D::setUniform3uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniform(location, count, v, GL_UNSIGNED_INT_VEC3);
}

void ProgramD3D::setUniform4uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniform(location, count, v, GL_UNSIGNED_INT_VEC4);
}

void ProgramD3D::setUniformBlockBinding(GLuint /*uniformBlockIndex*/,
                                        GLuint /*uniformBlockBinding*/)
{
}

void ProgramD3D::defineUniformsAndAssignRegisters()
{
    D3DUniformMap uniformMap;
    const gl::Shader *computeShader = mState.getAttachedComputeShader();
    if (computeShader)
    {
        for (const sh::Uniform &computeUniform : computeShader->getUniforms())
        {
            if (computeUniform.staticUse)
            {
                defineUniformBase(computeShader, computeUniform, &uniformMap);
            }
        }
    }
    else
    {
        const gl::Shader *vertexShader = mState.getAttachedVertexShader();
        for (const sh::Uniform &vertexUniform : vertexShader->getUniforms())
        {
            if (vertexUniform.staticUse)
            {
                defineUniformBase(vertexShader, vertexUniform, &uniformMap);
            }
        }

        const gl::Shader *fragmentShader = mState.getAttachedFragmentShader();
        for (const sh::Uniform &fragmentUniform : fragmentShader->getUniforms())
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

        auto mapEntry = uniformMap.find(glUniform.name);
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

void ProgramD3D::defineUniform(GLenum shaderType,
                               const sh::ShaderVariable &uniform,
                               const std::string &fullName,
                               sh::HLSLBlockEncoder *encoder,
                               D3DUniformMap *uniformMap)
{
    if (uniform.isStruct())
    {
        for (unsigned int elementIndex = 0; elementIndex < uniform.elementCount(); elementIndex++)
        {
            const std::string &elementString = (uniform.isArray() ? ArrayString(elementIndex) : "");

            if (encoder)
                encoder->enterAggregateType();

            for (size_t fieldIndex = 0; fieldIndex < uniform.fields.size(); fieldIndex++)
            {
                const sh::ShaderVariable &field  = uniform.fields[fieldIndex];
                const std::string &fieldFullName = (fullName + elementString + "." + field.name);

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
        return;
    }

    // Not a struct. Arrays are treated as aggregate types.
    if (uniform.isArray() && encoder)
    {
        encoder->enterAggregateType();
    }

    // Advance the uniform offset, to track registers allocation for structs
    sh::BlockMemberInfo blockInfo =
        encoder ? encoder->encodeType(uniform.type, uniform.arraySize, false)
                : sh::BlockMemberInfo::getDefaultBlockInfo();

    auto uniformMapEntry   = uniformMap->find(fullName);
    D3DUniform *d3dUniform = nullptr;

    if (uniformMapEntry != uniformMap->end())
    {
        d3dUniform = uniformMapEntry->second;
    }
    else
    {
        d3dUniform = new D3DUniform(uniform.type, fullName, uniform.arraySize, true);
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

template <typename T>
void ProgramD3D::setUniform(GLint location, GLsizei countIn, const T *v, GLenum targetUniformType)
{
    const int components        = gl::VariableComponentCount(targetUniformType);
    const GLenum targetBoolType = gl::VariableBoolVectorType(targetUniformType);

    D3DUniform *targetUniform = getD3DUniformFromLocation(location);

    unsigned int elementCount = targetUniform->elementCount();
    unsigned int arrayElement = mState.getUniformLocations()[location].element;
    unsigned int count        = std::min(elementCount - arrayElement, static_cast<unsigned int>(countIn));

    if (targetUniform->type == targetUniformType)
    {
        T *target = reinterpret_cast<T *>(targetUniform->data) + arrayElement * 4;

        for (unsigned int i = 0; i < count; i++)
        {
            T *dest         = target + (i * 4);
            const T *source = v + (i * components);

            for (int c = 0; c < components; c++)
            {
                SetIfDirty(dest + c, source[c], &targetUniform->dirty);
            }
            for (int c = components; c < 4; c++)
            {
                SetIfDirty(dest + c, T(0), &targetUniform->dirty);
            }
        }
    }
    else if (targetUniform->type == targetBoolType)
    {
        GLint *boolParams = reinterpret_cast<GLint *>(targetUniform->data) + arrayElement * 4;

        for (unsigned int i = 0; i < count; i++)
        {
            GLint *dest     = boolParams + (i * 4);
            const T *source = v + (i * components);

            for (int c = 0; c < components; c++)
            {
                SetIfDirty(dest + c, (source[c] == static_cast<T>(0)) ? GL_FALSE : GL_TRUE,
                           &targetUniform->dirty);
            }
            for (int c = components; c < 4; c++)
            {
                SetIfDirty(dest + c, GL_FALSE, &targetUniform->dirty);
            }
        }
    }
    else if (targetUniform->isSampler())
    {
        ASSERT(targetUniformType == GL_INT);

        GLint *target = reinterpret_cast<GLint *>(targetUniform->data) + arrayElement * 4;

        bool wasDirty = targetUniform->dirty;

        for (unsigned int i = 0; i < count; i++)
        {
            GLint *dest         = target + (i * 4);
            const GLint *source = reinterpret_cast<const GLint *>(v) + (i * components);

            SetIfDirty(dest + 0, source[0], &targetUniform->dirty);
            SetIfDirty(dest + 1, 0, &targetUniform->dirty);
            SetIfDirty(dest + 2, 0, &targetUniform->dirty);
            SetIfDirty(dest + 3, 0, &targetUniform->dirty);
        }

        if (!wasDirty && targetUniform->dirty)
        {
            mDirtySamplerMapping = true;
        }
    }
    else
        UNREACHABLE();
}

template <int cols, int rows>
void ProgramD3D::setUniformMatrixfv(GLint location,
                                    GLsizei countIn,
                                    GLboolean transpose,
                                    const GLfloat *value,
                                    GLenum targetUniformType)
{
    D3DUniform *targetUniform = getD3DUniformFromLocation(location);

    unsigned int elementCount = targetUniform->elementCount();
    unsigned int arrayElement = mState.getUniformLocations()[location].element;
    unsigned int count        = std::min(elementCount - arrayElement, static_cast<unsigned int>(countIn));

    const unsigned int targetMatrixStride = (4 * rows);
    GLfloat *target =
        (GLfloat *)(targetUniform->data + arrayElement * sizeof(GLfloat) * targetMatrixStride);

    for (unsigned int i = 0; i < count; i++)
    {
        // Internally store matrices as transposed versions to accomodate HLSL matrix indexing
        if (transpose == GL_FALSE)
        {
            targetUniform->dirty =
                TransposeExpandMatrix<GLfloat, cols, rows>(target, value) || targetUniform->dirty;
        }
        else
        {
            targetUniform->dirty =
                ExpandMatrix<GLfloat, cols, rows>(target, value) || targetUniform->dirty;
        }
        target += targetMatrixStride;
        value += cols * rows;
    }
}

size_t ProgramD3D::getUniformBlockInfo(const sh::InterfaceBlock &interfaceBlock)
{
    ASSERT(interfaceBlock.staticUse || interfaceBlock.layout != sh::BLOCKLAYOUT_PACKED);

    // define member uniforms
    sh::Std140BlockEncoder std140Encoder;
    sh::HLSLBlockEncoder hlslEncoder(sh::HLSLBlockEncoder::ENCODE_PACKED, false);
    sh::BlockLayoutEncoder *encoder = nullptr;

    if (interfaceBlock.layout == sh::BLOCKLAYOUT_STANDARD)
    {
        encoder = &std140Encoder;
    }
    else
    {
        encoder = &hlslEncoder;
    }

    GetUniformBlockInfo(interfaceBlock.fields, interfaceBlock.fieldPrefix(), encoder,
                        interfaceBlock.isRowMajorLayout, &mBlockInfo);

    return encoder->getBlockSize();
}

void ProgramD3D::assignAllSamplerRegisters()
{
    for (D3DUniform *d3dUniform : mD3DUniforms)
    {
        if (d3dUniform->isSampler())
        {
            assignSamplerRegisters(d3dUniform);
        }
    }
}

void ProgramD3D::assignSamplerRegisters(D3DUniform *d3dUniform)
{
    ASSERT(d3dUniform->isSampler());
    const gl::Shader *computeShader = mState.getAttachedComputeShader();
    if (computeShader)
    {
        const ShaderD3D *computeShaderD3D = GetImplAs<ShaderD3D>(mState.getAttachedComputeShader());
        ASSERT(computeShaderD3D->hasUniform(d3dUniform));
        d3dUniform->csRegisterIndex = computeShaderD3D->getUniformRegister(d3dUniform->name);
        ASSERT(d3dUniform->csRegisterIndex != GL_INVALID_INDEX);
        AssignSamplers(d3dUniform->csRegisterIndex, d3dUniform->type, d3dUniform->arraySize,
                       mSamplersCS, &mUsedComputeSamplerRange);
    }
    else
    {
        const ShaderD3D *vertexShaderD3D = GetImplAs<ShaderD3D>(mState.getAttachedVertexShader());
        const ShaderD3D *fragmentShaderD3D =
            GetImplAs<ShaderD3D>(mState.getAttachedFragmentShader());
        ASSERT(vertexShaderD3D->hasUniform(d3dUniform) ||
               fragmentShaderD3D->hasUniform(d3dUniform));
        if (vertexShaderD3D->hasUniform(d3dUniform))
        {
            d3dUniform->vsRegisterIndex = vertexShaderD3D->getUniformRegister(d3dUniform->name);
            ASSERT(d3dUniform->vsRegisterIndex != GL_INVALID_INDEX);
            AssignSamplers(d3dUniform->vsRegisterIndex, d3dUniform->type, d3dUniform->arraySize,
                           mSamplersVS, &mUsedVertexSamplerRange);
        }
        if (fragmentShaderD3D->hasUniform(d3dUniform))
        {
            d3dUniform->psRegisterIndex = fragmentShaderD3D->getUniformRegister(d3dUniform->name);
            ASSERT(d3dUniform->psRegisterIndex != GL_INVALID_INDEX);
            AssignSamplers(d3dUniform->psRegisterIndex, d3dUniform->type, d3dUniform->arraySize,
                           mSamplersPS, &mUsedPixelSamplerRange);
        }
    }
}

// static
void ProgramD3D::AssignSamplers(unsigned int startSamplerIndex,
                                GLenum samplerType,
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
        sampler->textureType        = gl::SamplerTypeToTextureType(samplerType);
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
}

unsigned int ProgramD3D::getSerial() const
{
    return mSerial;
}

unsigned int ProgramD3D::issueSerial()
{
    return mCurrentSerial++;
}

void ProgramD3D::initAttribLocationsToD3DSemantic()
{
    const gl::Shader *vertexShader = mState.getAttachedVertexShader();
    ASSERT(vertexShader != nullptr);

    // Init semantic index
    for (const sh::Attribute &attribute : mState.getAttributes())
    {
        int d3dSemantic = vertexShader->getSemanticIndex(attribute.name);
        int regCount    = gl::VariableRegisterCount(attribute.type);

        for (int reg = 0; reg < regCount; ++reg)
        {
            mAttribLocationToD3DSemantic[attribute.location + reg] = d3dSemantic + reg;
        }
    }
}

void ProgramD3D::updateCachedInputLayout(const gl::State &state)
{
    mCachedInputLayout.clear();
    const auto &vertexAttributes = state.getVertexArray()->getVertexAttributes();

    for (unsigned int locationIndex : IterateBitSet(mState.getActiveAttribLocationsMask()))
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
            size_t subscript     = GL_INVALID_INDEX;
            std::string baseName = gl::ParseResourceName(tfVaryingName, &subscript);
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

bool ProgramD3D::getUniformBlockSize(const std::string &blockName, size_t *sizeOut) const
{
    std::string baseName = blockName;
    gl::ParseAndStripArrayIndex(&baseName);

    auto sizeIter = mBlockDataSizes.find(baseName);
    if (sizeIter == mBlockDataSizes.end())
    {
        *sizeOut = 0;
        return false;
    }

    *sizeOut = sizeIter->second;
    return true;
}

bool ProgramD3D::getUniformBlockMemberInfo(const std::string &memberUniformName,
                                           sh::BlockMemberInfo *memberInfoOut) const
{
    auto infoIter = mBlockInfo.find(memberUniformName);
    if (infoIter == mBlockInfo.end())
    {
        *memberInfoOut = sh::BlockMemberInfo::getDefaultBlockInfo();
        return false;
    }

    *memberInfoOut = infoIter->second;
    return true;
}

void ProgramD3D::setPathFragmentInputGen(const std::string &inputName,
                                         GLenum genMode,
                                         GLint components,
                                         const GLfloat *coeffs)
{
    UNREACHABLE();
}

}  // namespace rx
