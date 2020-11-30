//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramD3D.cpp: Defines the rx::ProgramD3D class which implements rx::ProgramImpl.

#include "libANGLE/renderer/d3d/ProgramD3D.h"

#include "common/MemoryBuffer.h"
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
#include "libANGLE/renderer/d3d/ContextD3D.h"
#include "libANGLE/renderer/d3d/DynamicHLSL.h"
#include "libANGLE/renderer/d3d/FramebufferD3D.h"
#include "libANGLE/renderer/d3d/ShaderD3D.h"
#include "libANGLE/renderer/d3d/ShaderExecutableD3D.h"
#include "libANGLE/renderer/d3d/VertexDataManager.h"
#include "libANGLE/renderer/renderer_utils.h"
#include "libANGLE/trace.h"

using namespace angle;

namespace rx
{

namespace
{

void GetDefaultInputLayoutFromShader(gl::Shader *vertexShader, gl::InputLayout *inputLayoutOut)
{
    inputLayoutOut->clear();

    if (!vertexShader)
    {
        return;
    }

    for (const sh::ShaderVariable &shaderAttr : vertexShader->getActiveAttributes())
    {
        if (shaderAttr.type != GL_NONE)
        {
            GLenum transposedType = gl::TransposeMatrixType(shaderAttr.type);

            for (size_t rowIndex = 0;
                 static_cast<int>(rowIndex) < gl::VariableRowCount(transposedType); ++rowIndex)
            {
                GLenum componentType = gl::VariableComponentType(transposedType);
                GLuint components    = static_cast<GLuint>(gl::VariableColumnCount(transposedType));
                bool pureInt         = (componentType != GL_FLOAT);

                gl::VertexAttribType attribType =
                    gl::FromGLenum<gl::VertexAttribType>(componentType);

                angle::FormatID defaultID =
                    gl::GetVertexFormatID(attribType, GL_FALSE, components, pureInt);

                inputLayoutOut->push_back(defaultID);
            }
        }
    }
}

size_t GetMaxOutputIndex(const std::vector<PixelShaderOutputVariable> &shaderOutputVars,
                         size_t location)
{
    size_t maxIndex = 0;
    for (auto &outputVar : shaderOutputVars)
    {
        if (outputVar.outputLocation == location)
        {
            maxIndex = std::max(maxIndex, outputVar.outputIndex);
        }
    }
    return maxIndex;
}

void GetDefaultOutputLayoutFromShader(
    const std::vector<PixelShaderOutputVariable> &shaderOutputVars,
    std::vector<GLenum> *outputLayoutOut)
{
    outputLayoutOut->clear();

    if (!shaderOutputVars.empty())
    {
        size_t location = shaderOutputVars[0].outputLocation;
        size_t maxIndex = GetMaxOutputIndex(shaderOutputVars, location);
        outputLayoutOut->assign(maxIndex + 1,
                                GL_COLOR_ATTACHMENT0 + static_cast<unsigned int>(location));
    }
}

void GetDefaultImage2DBindLayoutFromComputeShader(
    const std::vector<sh::ShaderVariable> &image2DUniforms,
    gl::ImageUnitTextureTypeMap *image2DBindLayout)
{
    image2DBindLayout->clear();

    for (const sh::ShaderVariable &image2D : image2DUniforms)
    {
        if (gl::IsImage2DType(image2D.type))
        {
            if (image2D.binding == -1)
            {
                image2DBindLayout->insert(std::make_pair(0, gl::TextureType::_2D));
            }
            else
            {
                for (unsigned int index = 0; index < image2D.getArraySizeProduct(); index++)
                {
                    image2DBindLayout->insert(
                        std::make_pair(image2D.binding + index, gl::TextureType::_2D));
                }
            }
        }
    }
}

gl::PrimitiveMode GetGeometryShaderTypeFromDrawMode(gl::PrimitiveMode drawMode)
{
    switch (drawMode)
    {
        // Uses the point sprite geometry shader.
        case gl::PrimitiveMode::Points:
            return gl::PrimitiveMode::Points;

        // All line drawing uses the same geometry shader.
        case gl::PrimitiveMode::Lines:
        case gl::PrimitiveMode::LineStrip:
        case gl::PrimitiveMode::LineLoop:
            return gl::PrimitiveMode::Lines;

        // The triangle fan primitive is emulated with strips in D3D11.
        case gl::PrimitiveMode::Triangles:
        case gl::PrimitiveMode::TriangleFan:
            return gl::PrimitiveMode::Triangles;

        // Special case for triangle strips.
        case gl::PrimitiveMode::TriangleStrip:
            return gl::PrimitiveMode::TriangleStrip;

        default:
            UNREACHABLE();
            return gl::PrimitiveMode::InvalidEnum;
    }
}

bool HasFlatInterpolationVarying(const std::vector<sh::ShaderVariable> &varyings)
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

bool FindFlatInterpolationVaryingPerShader(gl::Shader *shader)
{
    ASSERT(shader);
    switch (shader->getType())
    {
        case gl::ShaderType::Vertex:
            return HasFlatInterpolationVarying(shader->getOutputVaryings());
        case gl::ShaderType::Fragment:
            return HasFlatInterpolationVarying(shader->getInputVaryings());
        case gl::ShaderType::Geometry:
            return HasFlatInterpolationVarying(shader->getInputVaryings()) ||
                   HasFlatInterpolationVarying(shader->getOutputVaryings());
        default:
            UNREACHABLE();
            return false;
    }
}

bool FindFlatInterpolationVarying(const gl::ShaderMap<gl::Shader *> &shaders)
{
    for (gl::ShaderType shaderType : gl::kAllGraphicsShaderTypes)
    {
        gl::Shader *shader = shaders[shaderType];
        if (!shader)
        {
            continue;
        }

        if (FindFlatInterpolationVaryingPerShader(shader))
        {
            return true;
        }
    }

    return false;
}

// Helper class that gathers uniform info from the default uniform block.
class UniformEncodingVisitorD3D : public sh::BlockEncoderVisitor
{
  public:
    UniformEncodingVisitorD3D(gl::ShaderType shaderType,
                              HLSLRegisterType registerType,
                              sh::BlockLayoutEncoder *encoder,
                              D3DUniformMap *uniformMapOut)
        : sh::BlockEncoderVisitor("", "", encoder),
          mShaderType(shaderType),
          mRegisterType(registerType),
          mUniformMapOut(uniformMapOut)
    {}

    void visitNamedSampler(const sh::ShaderVariable &sampler,
                           const std::string &name,
                           const std::string &mappedName,
                           const std::vector<unsigned int> &arraySizes) override
    {
        auto uniformMapEntry = mUniformMapOut->find(name);
        if (uniformMapEntry == mUniformMapOut->end())
        {
            (*mUniformMapOut)[name] =
                new D3DUniform(sampler.type, mRegisterType, name, sampler.arraySizes, true);
        }
    }

    void encodeVariable(const sh::ShaderVariable &variable,
                        const sh::BlockMemberInfo &variableInfo,
                        const std::string &name,
                        const std::string &mappedName) override
    {
        auto uniformMapEntry   = mUniformMapOut->find(name);
        D3DUniform *d3dUniform = nullptr;

        if (uniformMapEntry != mUniformMapOut->end())
        {
            d3dUniform = uniformMapEntry->second;
        }
        else
        {
            d3dUniform =
                new D3DUniform(variable.type, mRegisterType, name, variable.arraySizes, true);
            (*mUniformMapOut)[name] = d3dUniform;
        }

        d3dUniform->registerElement = static_cast<unsigned int>(
            sh::BlockLayoutEncoder::GetBlockRegisterElement(variableInfo));
        unsigned int reg =
            static_cast<unsigned int>(sh::BlockLayoutEncoder::GetBlockRegister(variableInfo));

        ASSERT(mShaderType != gl::ShaderType::InvalidEnum);
        d3dUniform->mShaderRegisterIndexes[mShaderType] = reg;
    }

  private:
    gl::ShaderType mShaderType;
    HLSLRegisterType mRegisterType;
    D3DUniformMap *mUniformMapOut;
};

class HLSLBlockLayoutEncoderFactory : public gl::CustomBlockLayoutEncoderFactory
{
  public:
    sh::BlockLayoutEncoder *makeEncoder() override
    {
        return new sh::HLSLBlockEncoder(sh::HLSLBlockEncoder::ENCODE_PACKED, false);
    }
};
}  // anonymous namespace

// D3DUniform Implementation

D3DUniform::D3DUniform(GLenum type,
                       HLSLRegisterType reg,
                       const std::string &nameIn,
                       const std::vector<unsigned int> &arraySizesIn,
                       bool defaultBlock)
    : typeInfo(gl::GetUniformTypeInfo(type)),
      name(nameIn),
      arraySizes(arraySizesIn),
      mShaderData({}),
      regType(reg),
      registerCount(0),
      registerElement(0)
{
    mShaderRegisterIndexes.fill(GL_INVALID_INDEX);

    // We use data storage for default block uniforms to cache values that are sent to D3D during
    // rendering
    // Uniform blocks/buffers are treated separately by the Renderer (ES3 path only)
    if (defaultBlock)
    {
        // Use the row count as register count, will work for non-square matrices.
        registerCount = typeInfo.rowCount * getArraySizeProduct();
    }
}

D3DUniform::~D3DUniform() {}

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

bool D3DUniform::isImage() const
{
    return typeInfo.isImageType;
}

bool D3DUniform::isImage2D() const
{
    return gl::IsImage2DType(typeInfo.type);
}

bool D3DUniform::isReferencedByShader(gl::ShaderType shaderType) const
{
    return mShaderRegisterIndexes[shaderType] != GL_INVALID_INDEX;
}

const uint8_t *D3DUniform::firstNonNullData() const
{
    if (!mSamplerData.empty())
    {
        return reinterpret_cast<const uint8_t *>(mSamplerData.data());
    }

    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        if (mShaderData[shaderType])
        {
            return mShaderData[shaderType];
        }
    }

    UNREACHABLE();
    return nullptr;
}

// D3DInterfaceBlock Implementation
D3DInterfaceBlock::D3DInterfaceBlock()
{
    mShaderRegisterIndexes.fill(GL_INVALID_INDEX);
}

D3DInterfaceBlock::D3DInterfaceBlock(const D3DInterfaceBlock &other) = default;

D3DUniformBlock::D3DUniformBlock()
{
    mUseStructuredBuffers.fill(false);
    mByteWidths.fill(0u);
    mStructureByteStrides.fill(0u);
}

D3DUniformBlock::D3DUniformBlock(const D3DUniformBlock &other) = default;

// D3DVarying Implementation

D3DVarying::D3DVarying() : semanticIndex(0), componentCount(0), outputSlot(0) {}

D3DVarying::D3DVarying(const std::string &semanticNameIn,
                       unsigned int semanticIndexIn,
                       unsigned int componentCountIn,
                       unsigned int outputSlotIn)
    : semanticName(semanticNameIn),
      semanticIndex(semanticIndexIn),
      componentCount(componentCountIn),
      outputSlot(outputSlotIn)
{}

// ProgramD3DMetadata Implementation

ProgramD3DMetadata::ProgramD3DMetadata(RendererD3D *renderer,
                                       const gl::ShaderMap<const ShaderD3D *> &attachedShaders,
                                       EGLenum clientType)
    : mRendererMajorShaderModel(renderer->getMajorShaderModel()),
      mShaderModelSuffix(renderer->getShaderModelSuffix()),
      mUsesInstancedPointSpriteEmulation(
          renderer->getFeatures().useInstancedPointSpriteEmulation.enabled),
      mUsesViewScale(renderer->presentPathFastEnabled()),
      mCanSelectViewInVertexShader(renderer->canSelectViewInVertexShader()),
      mAttachedShaders(attachedShaders),
      mClientType(clientType)
{}

ProgramD3DMetadata::~ProgramD3DMetadata() = default;

int ProgramD3DMetadata::getRendererMajorShaderModel() const
{
    return mRendererMajorShaderModel;
}

bool ProgramD3DMetadata::usesBroadcast(const gl::State &data) const
{
    const rx::ShaderD3D *shader = mAttachedShaders[gl::ShaderType::Fragment];
    return (shader && shader->usesFragColor() && shader->usesMultipleRenderTargets() &&
            data.getClientMajorVersion() < 3);
}

bool ProgramD3DMetadata::usesSecondaryColor() const
{
    const rx::ShaderD3D *shader = mAttachedShaders[gl::ShaderType::Fragment];
    return (shader && shader->usesSecondaryColor());
}

bool ProgramD3DMetadata::usesFragDepth() const
{
    const rx::ShaderD3D *shader = mAttachedShaders[gl::ShaderType::Fragment];
    return (shader && shader->usesFragDepth());
}

bool ProgramD3DMetadata::usesPointCoord() const
{
    const rx::ShaderD3D *shader = mAttachedShaders[gl::ShaderType::Fragment];
    return (shader && shader->usesPointCoord());
}

bool ProgramD3DMetadata::usesFragCoord() const
{
    const rx::ShaderD3D *shader = mAttachedShaders[gl::ShaderType::Fragment];
    return (shader && shader->usesFragCoord());
}

bool ProgramD3DMetadata::usesPointSize() const
{
    const rx::ShaderD3D *shader = mAttachedShaders[gl::ShaderType::Vertex];
    return (shader && shader->usesPointSize());
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
    const rx::ShaderD3D *shader = mAttachedShaders[gl::ShaderType::Vertex];
    return (shader && shader->hasANGLEMultiviewEnabled());
}

bool ProgramD3DMetadata::usesVertexID() const
{
    const rx::ShaderD3D *shader = mAttachedShaders[gl::ShaderType::Vertex];
    return (shader && shader->usesVertexID());
}

bool ProgramD3DMetadata::usesViewID() const
{
    const rx::ShaderD3D *shader = mAttachedShaders[gl::ShaderType::Fragment];
    return (shader && shader->usesViewID());
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
    const rx::ShaderD3D *shader = mAttachedShaders[gl::ShaderType::Fragment];
    return (shader && shader->usesMultipleRenderTargets());
}

bool ProgramD3DMetadata::usesCustomOutVars() const
{

    const rx::ShaderD3D *shader = mAttachedShaders[gl::ShaderType::Vertex];
    int version                 = shader ? shader->getData().getShaderVersion() : -1;

    switch (mClientType)
    {
        case EGL_OPENGL_API:
            return version >= 130;
        default:
            return version >= 300;
    }
}

const ShaderD3D *ProgramD3DMetadata::getFragmentShader() const
{
    return mAttachedShaders[gl::ShaderType::Fragment];
}

// ProgramD3D::GetExecutableTask class
class ProgramD3D::GetExecutableTask : public Closure, public d3d::Context
{
  public:
    GetExecutableTask(ProgramD3D *program) : mProgram(program) {}

    virtual angle::Result run() = 0;

    void operator()() override { mResult = run(); }

    angle::Result getResult() const { return mResult; }
    const gl::InfoLog &getInfoLog() const { return mInfoLog; }
    ShaderExecutableD3D *getExecutable() { return mExecutable; }

    void handleResult(HRESULT hr,
                      const char *message,
                      const char *file,
                      const char *function,
                      unsigned int line) override
    {
        mStoredHR       = hr;
        mStoredMessage  = message;
        mStoredFile     = file;
        mStoredFunction = function;
        mStoredLine     = line;
    }

    void popError(d3d::Context *context)
    {
        ASSERT(mStoredFile);
        ASSERT(mStoredFunction);
        context->handleResult(mStoredHR, mStoredMessage.c_str(), mStoredFile, mStoredFunction,
                              mStoredLine);
    }

  protected:
    ProgramD3D *mProgram  = nullptr;
    angle::Result mResult = angle::Result::Continue;
    gl::InfoLog mInfoLog;
    ShaderExecutableD3D *mExecutable = nullptr;
    HRESULT mStoredHR                = S_OK;
    std::string mStoredMessage;
    const char *mStoredFile     = nullptr;
    const char *mStoredFunction = nullptr;
    unsigned int mStoredLine    = 0;
};

// ProgramD3D Implementation

ProgramD3D::VertexExecutable::VertexExecutable(const gl::InputLayout &inputLayout,
                                               const Signature &signature,
                                               ShaderExecutableD3D *shaderExecutable)
    : mInputs(inputLayout), mSignature(signature), mShaderExecutable(shaderExecutable)
{}

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
        angle::FormatID vertexFormatID = inputLayout[index];
        if (vertexFormatID == angle::FormatID::NONE)
            continue;

        VertexConversionType conversionType = renderer->getVertexConversionType(vertexFormatID);
        if ((conversionType & VERTEX_CONVERT_GPU) == 0)
            continue;

        GLenum componentType   = renderer->getVertexComponentType(vertexFormatID);
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
{}

ProgramD3D::PixelExecutable::~PixelExecutable()
{
    SafeDelete(mShaderExecutable);
}

ProgramD3D::ComputeExecutable::ComputeExecutable(
    const gl::ImageUnitTextureTypeMap &signature,
    std::unique_ptr<ShaderExecutableD3D> shaderExecutable)
    : mSignature(signature), mShaderExecutable(std::move(shaderExecutable))
{}

ProgramD3D::ComputeExecutable::~ComputeExecutable() {}

ProgramD3D::Sampler::Sampler()
    : active(false), logicalTextureUnit(0), textureType(gl::TextureType::_2D)
{}

ProgramD3D::Image::Image() : active(false), logicalImageUnit(0) {}

unsigned int ProgramD3D::mCurrentSerial = 1;

ProgramD3D::ProgramD3D(const gl::ProgramState &state, RendererD3D *renderer)
    : ProgramImpl(state),
      mRenderer(renderer),
      mDynamicHLSL(nullptr),
      mUsesPointSize(false),
      mUsesFlatInterpolation(false),
      mUsedShaderSamplerRanges({}),
      mDirtySamplerMapping(true),
      mUsedComputeImageRange(0, 0),
      mUsedComputeReadonlyImageRange(0, 0),
      mUsedComputeAtomicCounterRange(0, 0),
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

bool ProgramD3D::usesGeometryShaderForPointSpriteEmulation() const
{
    return usesPointSpriteEmulation() && !usesInstancedPointSpriteEmulation();
}

bool ProgramD3D::usesGetDimensionsIgnoresBaseLevel() const
{
    return mRenderer->getFeatures().getDimensionsIgnoresBaseLevel.enabled;
}

bool ProgramD3D::usesGeometryShader(const gl::State &state, const gl::PrimitiveMode drawMode) const
{
    if (mHasANGLEMultiviewEnabled && !mRenderer->canSelectViewInVertexShader())
    {
        return true;
    }
    if (drawMode != gl::PrimitiveMode::Points)
    {
        if (!mUsesFlatInterpolation)
        {
            return false;
        }
        return state.getProvokingVertex() == gl::ProvokingVertexConvention::LastVertexConvention;
    }
    return usesGeometryShaderForPointSpriteEmulation();
}

bool ProgramD3D::usesInstancedPointSpriteEmulation() const
{
    return mRenderer->getFeatures().useInstancedPointSpriteEmulation.enabled;
}

GLint ProgramD3D::getSamplerMapping(gl::ShaderType type,
                                    unsigned int samplerIndex,
                                    const gl::Caps &caps) const
{
    GLint logicalTextureUnit = -1;

    ASSERT(type != gl::ShaderType::InvalidEnum);

    ASSERT(samplerIndex < static_cast<unsigned int>(caps.maxShaderTextureImageUnits[type]));

    const auto &samplers = mShaderSamplers[type];
    if (samplerIndex < samplers.size() && samplers[samplerIndex].active)
    {
        logicalTextureUnit = samplers[samplerIndex].logicalTextureUnit;
    }

    if (logicalTextureUnit >= 0 && logicalTextureUnit < caps.maxCombinedTextureImageUnits)
    {
        return logicalTextureUnit;
    }

    return -1;
}

// Returns the texture type for a given Direct3D 9 sampler type and
// index (0-15 for the pixel shader and 0-3 for the vertex shader).
gl::TextureType ProgramD3D::getSamplerTextureType(gl::ShaderType type,
                                                  unsigned int samplerIndex) const
{
    ASSERT(type != gl::ShaderType::InvalidEnum);

    const auto &samplers = mShaderSamplers[type];
    ASSERT(samplerIndex < samplers.size());
    ASSERT(samplers[samplerIndex].active);

    return samplers[samplerIndex].textureType;
}

gl::RangeUI ProgramD3D::getUsedSamplerRange(gl::ShaderType type) const
{
    ASSERT(type != gl::ShaderType::InvalidEnum);
    return mUsedShaderSamplerRanges[type];
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

        for (gl::ShaderType shaderType : gl::AllShaderTypes())
        {
            if (!d3dUniform->isReferencedByShader(shaderType))
            {
                continue;
            }

            unsigned int firstIndex = d3dUniform->mShaderRegisterIndexes[shaderType];

            std::vector<Sampler> &samplers = mShaderSamplers[shaderType];
            for (int i = 0; i < count; i++)
            {
                unsigned int samplerIndex = firstIndex + i;

                if (samplerIndex < samplers.size())
                {
                    ASSERT(samplers[samplerIndex].active);
                    samplers[samplerIndex].logicalTextureUnit = d3dUniform->mSamplerData[i];
                }
            }
        }
    }

    return SamplerMapping::WasDirty;
}

GLint ProgramD3D::getImageMapping(gl::ShaderType type,
                                  unsigned int imageIndex,
                                  bool readonly,
                                  const gl::Caps &caps) const
{
    GLint logicalImageUnit = -1;
    ASSERT(imageIndex < static_cast<unsigned int>(caps.maxImageUnits));
    switch (type)
    {
        case gl::ShaderType::Compute:
            if (readonly && imageIndex < mReadonlyImagesCS.size() &&
                mReadonlyImagesCS[imageIndex].active)
            {
                logicalImageUnit = mReadonlyImagesCS[imageIndex].logicalImageUnit;
            }
            else if (imageIndex < mImagesCS.size() && mImagesCS[imageIndex].active)
            {
                logicalImageUnit = mImagesCS[imageIndex].logicalImageUnit;
            }
            break;
        // TODO(xinghua.cao@intel.com): add image mapping for vertex shader and pixel shader.
        default:
            UNREACHABLE();
    }

    if (logicalImageUnit >= 0 && logicalImageUnit < caps.maxImageUnits)
    {
        return logicalImageUnit;
    }

    return -1;
}

gl::RangeUI ProgramD3D::getUsedImageRange(gl::ShaderType type, bool readonly) const
{
    switch (type)
    {
        case gl::ShaderType::Compute:
            return readonly ? mUsedComputeReadonlyImageRange : mUsedComputeImageRange;
        // TODO(xinghua.cao@intel.com): add real image range of vertex shader and pixel shader.
        case gl::ShaderType::Vertex:
        case gl::ShaderType::Fragment:
            return {0, 0};
        default:
            UNREACHABLE();
            return {0, 0};
    }
}

class ProgramD3D::LoadBinaryTask : public ProgramD3D::GetExecutableTask
{
  public:
    LoadBinaryTask(ProgramD3D *program, gl::BinaryInputStream *stream, gl::InfoLog &infoLog)
        : ProgramD3D::GetExecutableTask(program)
    {
        ASSERT(mProgram);
        ASSERT(stream);

        // Copy the remaining data from the stream locally so that the client can't modify it when
        // loading off thread.
        size_t dataSize    = stream->remainingSize();
        mDataCopySucceeded = mStreamData.resize(dataSize);
        if (mDataCopySucceeded)
        {
            memcpy(mStreamData.data(), stream->data() + stream->offset(), dataSize);
        }
    }

    angle::Result run() override
    {
        ANGLE_TRACE_EVENT0("gpu.angle", "ProgramD3D::LoadBinaryTask::run");
        if (!mDataCopySucceeded)
        {
            mInfoLog << "Failed to copy program binary data to local buffer.";
            return angle::Result::Incomplete;
        }

        gl::BinaryInputStream stream(mStreamData.data(), mStreamData.size());
        return mProgram->loadBinaryShaderExecutables(this, &stream, mInfoLog);
    }

  private:
    bool mDataCopySucceeded;
    angle::MemoryBuffer mStreamData;
};

class ProgramD3D::LoadBinaryLinkEvent final : public LinkEvent
{
  public:
    LoadBinaryLinkEvent(std::shared_ptr<WorkerThreadPool> workerPool,
                        ProgramD3D *program,
                        gl::BinaryInputStream *stream,
                        gl::InfoLog &infoLog)
        : mTask(std::make_shared<ProgramD3D::LoadBinaryTask>(program, stream, infoLog)),
          mWaitableEvent(angle::WorkerThreadPool::PostWorkerTask(workerPool, mTask))
    {}

    angle::Result wait(const gl::Context *context) override
    {
        mWaitableEvent->wait();

        // Continue and Incomplete are not errors. For Stop, pass the error to the ContextD3D.
        if (mTask->getResult() != angle::Result::Stop)
        {
            return angle::Result::Continue;
        }

        ContextD3D *contextD3D = GetImplAs<ContextD3D>(context);
        mTask->popError(contextD3D);
        return angle::Result::Stop;
    }

    bool isLinking() override { return !mWaitableEvent->isReady(); }

  private:
    std::shared_ptr<ProgramD3D::LoadBinaryTask> mTask;
    std::shared_ptr<WaitableEvent> mWaitableEvent;
};

std::unique_ptr<rx::LinkEvent> ProgramD3D::load(const gl::Context *context,
                                                gl::BinaryInputStream *stream,
                                                gl::InfoLog &infoLog)
{

    // TODO(jmadill): Use Renderer from contextImpl.

    reset();

    DeviceIdentifier binaryDeviceIdentifier = {};
    stream->readBytes(reinterpret_cast<unsigned char *>(&binaryDeviceIdentifier),
                      sizeof(DeviceIdentifier));

    DeviceIdentifier identifier = mRenderer->getAdapterIdentifier();
    if (memcmp(&identifier, &binaryDeviceIdentifier, sizeof(DeviceIdentifier)) != 0)
    {
        infoLog << "Invalid program binary, device configuration has changed.";
        return nullptr;
    }

    int compileFlags = stream->readInt<int>();
    if (compileFlags != ANGLE_COMPILE_OPTIMIZATION_LEVEL)
    {
        infoLog << "Mismatched compilation flags.";
        return nullptr;
    }

    for (int &index : mAttribLocationToD3DSemantic)
    {
        stream->readInt(&index);
    }

    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        const unsigned int samplerCount = stream->readInt<unsigned int>();
        for (unsigned int i = 0; i < samplerCount; ++i)
        {
            Sampler sampler;
            stream->readBool(&sampler.active);
            stream->readInt(&sampler.logicalTextureUnit);
            stream->readEnum(&sampler.textureType);
            mShaderSamplers[shaderType].push_back(sampler);
        }

        unsigned int samplerRangeLow, samplerRangeHigh;
        stream->readInt(&samplerRangeLow);
        stream->readInt(&samplerRangeHigh);
        mUsedShaderSamplerRanges[shaderType] = gl::RangeUI(samplerRangeLow, samplerRangeHigh);
    }

    const unsigned int csImageCount = stream->readInt<unsigned int>();
    for (unsigned int i = 0; i < csImageCount; ++i)
    {
        Image image;
        stream->readBool(&image.active);
        stream->readInt(&image.logicalImageUnit);
        mImagesCS.push_back(image);
    }

    const unsigned int csReadonlyImageCount = stream->readInt<unsigned int>();
    for (unsigned int i = 0; i < csReadonlyImageCount; ++i)
    {
        Image image;
        stream->readBool(&image.active);
        stream->readInt(&image.logicalImageUnit);
        mReadonlyImagesCS.push_back(image);
    }

    unsigned int computeImageRangeLow, computeImageRangeHigh, computeReadonlyImageRangeLow,
        computeReadonlyImageRangeHigh;
    stream->readInt(&computeImageRangeLow);
    stream->readInt(&computeImageRangeHigh);
    stream->readInt(&computeReadonlyImageRangeLow);
    stream->readInt(&computeReadonlyImageRangeHigh);
    mUsedComputeImageRange = gl::RangeUI(computeImageRangeLow, computeImageRangeHigh);
    mUsedComputeReadonlyImageRange =
        gl::RangeUI(computeReadonlyImageRangeLow, computeReadonlyImageRangeHigh);

    unsigned int atomicCounterRangeLow, atomicCounterRangeHigh;
    stream->readInt(&atomicCounterRangeLow);
    stream->readInt(&atomicCounterRangeHigh);
    mUsedComputeAtomicCounterRange = gl::RangeUI(atomicCounterRangeLow, atomicCounterRangeHigh);

    const unsigned int shaderStorageBlockCount = stream->readInt<unsigned int>();
    if (stream->error())
    {
        infoLog << "Invalid program binary.";
        return nullptr;
    }

    ASSERT(mD3DShaderStorageBlocks.empty());
    for (unsigned int blockIndex = 0; blockIndex < shaderStorageBlockCount; ++blockIndex)
    {
        D3DInterfaceBlock shaderStorageBlock;
        for (gl::ShaderType shaderType : gl::AllShaderTypes())
        {
            stream->readInt(&shaderStorageBlock.mShaderRegisterIndexes[shaderType]);
        }
        mD3DShaderStorageBlocks.push_back(shaderStorageBlock);
    }

    const unsigned int image2DUniformCount = stream->readInt<unsigned int>();
    if (stream->error())
    {
        infoLog << "Invalid program binary.";
        return nullptr;
    }

    ASSERT(mImage2DUniforms.empty());
    for (unsigned int image2DUniformIndex = 0; image2DUniformIndex < image2DUniformCount;
         ++image2DUniformIndex)
    {
        sh::ShaderVariable image2Duniform;
        gl::LoadShaderVar(stream, &image2Duniform);
        mImage2DUniforms.push_back(image2Duniform);
    }

    for (unsigned int ii = 0; ii < gl::IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFERS; ++ii)
    {
        unsigned int index                             = stream->readInt<unsigned int>();
        mComputeAtomicCounterBufferRegisterIndices[ii] = index;
    }

    const unsigned int uniformCount = stream->readInt<unsigned int>();
    if (stream->error())
    {
        infoLog << "Invalid program binary.";
        return nullptr;
    }

    const auto &linkedUniforms = mState.getUniforms();
    ASSERT(mD3DUniforms.empty());
    for (unsigned int uniformIndex = 0; uniformIndex < uniformCount; uniformIndex++)
    {
        const gl::LinkedUniform &linkedUniform = linkedUniforms[uniformIndex];

        D3DUniform *d3dUniform =
            new D3DUniform(linkedUniform.type, HLSLRegisterType::None, linkedUniform.name,
                           linkedUniform.arraySizes, linkedUniform.isInDefaultBlock());
        stream->readInt<HLSLRegisterType>(&d3dUniform->regType);
        for (gl::ShaderType shaderType : gl::AllShaderTypes())
        {
            stream->readInt(&d3dUniform->mShaderRegisterIndexes[shaderType]);
        }
        stream->readInt(&d3dUniform->registerCount);
        stream->readInt(&d3dUniform->registerElement);

        mD3DUniforms.push_back(d3dUniform);
    }

    const unsigned int blockCount = stream->readInt<unsigned int>();
    if (stream->error())
    {
        infoLog << "Invalid program binary.";
        return nullptr;
    }

    ASSERT(mD3DUniformBlocks.empty());
    for (unsigned int blockIndex = 0; blockIndex < blockCount; ++blockIndex)
    {
        D3DUniformBlock uniformBlock;
        for (gl::ShaderType shaderType : gl::AllShaderTypes())
        {
            stream->readInt(&uniformBlock.mShaderRegisterIndexes[shaderType]);
            stream->readBool(&uniformBlock.mUseStructuredBuffers[shaderType]);
            stream->readInt(&uniformBlock.mByteWidths[shaderType]);
            stream->readInt(&uniformBlock.mStructureByteStrides[shaderType]);
        }
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

    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        stream->readString(&mShaderHLSL[shaderType]);
        stream->readBytes(reinterpret_cast<unsigned char *>(&mShaderWorkarounds[shaderType]),
                          sizeof(angle::CompilerWorkaroundsD3D));
    }

    stream->readBool(&mUsesFragDepth);
    stream->readBool(&mHasANGLEMultiviewEnabled);
    stream->readBool(&mUsesVertexID);
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
        stream->readInt(&mPixelShaderKey[pixelShaderKeyIndex].outputLocation);
        stream->readInt(&mPixelShaderKey[pixelShaderKeyIndex].outputIndex);
    }

    stream->readString(&mGeometryShaderPreamble);

    return std::make_unique<LoadBinaryLinkEvent>(context->getWorkerThreadPool(), this, stream,
                                                 infoLog);
}

angle::Result ProgramD3D::loadBinaryShaderExecutables(d3d::Context *contextD3D,
                                                      gl::BinaryInputStream *stream,
                                                      gl::InfoLog &infoLog)
{
    const unsigned char *binary = reinterpret_cast<const unsigned char *>(stream->data());

    bool separateAttribs = (mState.getTransformFeedbackBufferMode() == GL_SEPARATE_ATTRIBS);

    const unsigned int vertexShaderCount = stream->readInt<unsigned int>();
    for (unsigned int vertexShaderIndex = 0; vertexShaderIndex < vertexShaderCount;
         vertexShaderIndex++)
    {
        size_t inputLayoutSize = stream->readInt<size_t>();
        gl::InputLayout inputLayout(inputLayoutSize, angle::FormatID::NONE);

        for (size_t inputIndex = 0; inputIndex < inputLayoutSize; inputIndex++)
        {
            inputLayout[inputIndex] = stream->readInt<angle::FormatID>();
        }

        unsigned int vertexShaderSize             = stream->readInt<unsigned int>();
        const unsigned char *vertexShaderFunction = binary + stream->offset();

        ShaderExecutableD3D *shaderExecutable = nullptr;

        ANGLE_TRY(mRenderer->loadExecutable(contextD3D, vertexShaderFunction, vertexShaderSize,
                                            gl::ShaderType::Vertex, mStreamOutVaryings,
                                            separateAttribs, &shaderExecutable));

        if (!shaderExecutable)
        {
            infoLog << "Could not create vertex shader.";
            return angle::Result::Incomplete;
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

        ANGLE_TRY(mRenderer->loadExecutable(contextD3D, pixelShaderFunction, pixelShaderSize,
                                            gl::ShaderType::Fragment, mStreamOutVaryings,
                                            separateAttribs, &shaderExecutable));

        if (!shaderExecutable)
        {
            infoLog << "Could not create pixel shader.";
            return angle::Result::Incomplete;
        }

        // add new binary
        mPixelExecutables.push_back(
            std::unique_ptr<PixelExecutable>(new PixelExecutable(outputs, shaderExecutable)));

        stream->skip(pixelShaderSize);
    }

    for (auto &geometryExe : mGeometryExecutables)
    {
        unsigned int geometryShaderSize = stream->readInt<unsigned int>();
        if (geometryShaderSize == 0)
        {
            continue;
        }

        const unsigned char *geometryShaderFunction = binary + stream->offset();

        ShaderExecutableD3D *geometryExecutable = nullptr;
        ANGLE_TRY(mRenderer->loadExecutable(contextD3D, geometryShaderFunction, geometryShaderSize,
                                            gl::ShaderType::Geometry, mStreamOutVaryings,
                                            separateAttribs, &geometryExecutable));

        if (!geometryExecutable)
        {
            infoLog << "Could not create geometry shader.";
            return angle::Result::Incomplete;
        }

        geometryExe.reset(geometryExecutable);

        stream->skip(geometryShaderSize);
    }

    const size_t computeShaderCount = stream->readInt<unsigned int>();
    for (size_t computeShaderIndex = 0; computeShaderIndex < computeShaderCount;
         computeShaderIndex++)
    {
        const size_t signatureCount = stream->readInt<unsigned int>();
        gl::ImageUnitTextureTypeMap signatures;
        for (size_t signatureIndex = 0; signatureIndex < signatureCount; signatureIndex++)
        {
            unsigned int imageUint;
            gl::TextureType textureType;
            stream->readInt<unsigned int>(&imageUint);
            stream->readInt<gl::TextureType>(&textureType);
            signatures.insert(std::pair<unsigned int, gl::TextureType>(imageUint, textureType));
        }

        const size_t computeShaderSize             = stream->readInt<unsigned int>();
        const unsigned char *computeShaderFunction = binary + stream->offset();

        ShaderExecutableD3D *computeExecutable = nullptr;
        ANGLE_TRY(mRenderer->loadExecutable(contextD3D, computeShaderFunction, computeShaderSize,
                                            gl::ShaderType::Compute, std::vector<D3DVarying>(),
                                            false, &computeExecutable));

        if (!computeExecutable)
        {
            infoLog << "Could not create compute shader.";
            return angle::Result::Incomplete;
        }

        // add new binary
        mComputeExecutables.push_back(std::unique_ptr<ComputeExecutable>(new ComputeExecutable(
            signatures, std::unique_ptr<ShaderExecutableD3D>(computeExecutable))));

        stream->skip(computeShaderSize);
    }

    const size_t bindLayoutCount = stream->readInt<unsigned int>();
    for (size_t bindLayoutIndex = 0; bindLayoutIndex < bindLayoutCount; bindLayoutIndex++)
    {
        mComputeShaderImage2DBindLayoutCache.insert(std::pair<unsigned int, gl::TextureType>(
            stream->readInt<unsigned int>(), gl::TextureType::_2D));
    }

    initializeUniformStorage(mState.getExecutable().getLinkedShaderStages());

    dirtyAllUniforms();

    return angle::Result::Continue;
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

    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        stream->writeInt(mShaderSamplers[shaderType].size());
        for (unsigned int i = 0; i < mShaderSamplers[shaderType].size(); ++i)
        {
            stream->writeInt(mShaderSamplers[shaderType][i].active);
            stream->writeInt(mShaderSamplers[shaderType][i].logicalTextureUnit);
            stream->writeEnum(mShaderSamplers[shaderType][i].textureType);
        }

        stream->writeInt(mUsedShaderSamplerRanges[shaderType].low());
        stream->writeInt(mUsedShaderSamplerRanges[shaderType].high());
    }

    stream->writeInt(mImagesCS.size());
    for (unsigned int i = 0; i < mImagesCS.size(); ++i)
    {
        stream->writeInt(mImagesCS[i].active);
        stream->writeInt(mImagesCS[i].logicalImageUnit);
    }

    stream->writeInt(mReadonlyImagesCS.size());
    for (unsigned int i = 0; i < mReadonlyImagesCS.size(); ++i)
    {
        stream->writeInt(mReadonlyImagesCS[i].active);
        stream->writeInt(mReadonlyImagesCS[i].logicalImageUnit);
    }

    stream->writeInt(mUsedComputeImageRange.low());
    stream->writeInt(mUsedComputeImageRange.high());
    stream->writeInt(mUsedComputeReadonlyImageRange.low());
    stream->writeInt(mUsedComputeReadonlyImageRange.high());
    stream->writeInt(mUsedComputeAtomicCounterRange.low());
    stream->writeInt(mUsedComputeAtomicCounterRange.high());

    stream->writeInt(mD3DShaderStorageBlocks.size());
    for (const D3DInterfaceBlock &shaderStorageBlock : mD3DShaderStorageBlocks)
    {
        for (gl::ShaderType shaderType : gl::AllShaderTypes())
        {
            stream->writeIntOrNegOne(shaderStorageBlock.mShaderRegisterIndexes[shaderType]);
        }
    }

    stream->writeInt(mImage2DUniforms.size());
    for (const sh::ShaderVariable &image2DUniform : mImage2DUniforms)
    {
        gl::WriteShaderVar(stream, image2DUniform);
    }

    for (unsigned int ii = 0; ii < gl::IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFERS; ++ii)
    {
        stream->writeInt(mComputeAtomicCounterBufferRegisterIndices[ii]);
    }

    stream->writeInt(mD3DUniforms.size());
    for (const D3DUniform *uniform : mD3DUniforms)
    {
        // Type, name and arraySize are redundant, so aren't stored in the binary.
        stream->writeInt(static_cast<unsigned int>(uniform->regType));
        for (gl::ShaderType shaderType : gl::AllShaderTypes())
        {
            stream->writeIntOrNegOne(uniform->mShaderRegisterIndexes[shaderType]);
        }
        stream->writeInt(uniform->registerCount);
        stream->writeInt(uniform->registerElement);
    }

    stream->writeInt(mD3DUniformBlocks.size());
    for (const D3DUniformBlock &uniformBlock : mD3DUniformBlocks)
    {
        for (gl::ShaderType shaderType : gl::AllShaderTypes())
        {
            stream->writeIntOrNegOne(uniformBlock.mShaderRegisterIndexes[shaderType]);
            stream->writeInt(uniformBlock.mUseStructuredBuffers[shaderType]);
            stream->writeInt(uniformBlock.mByteWidths[shaderType]);
            stream->writeInt(uniformBlock.mStructureByteStrides[shaderType]);
        }
    }

    stream->writeInt(mStreamOutVaryings.size());
    for (const auto &varying : mStreamOutVaryings)
    {
        stream->writeString(varying.semanticName);
        stream->writeInt(varying.semanticIndex);
        stream->writeInt(varying.componentCount);
        stream->writeInt(varying.outputSlot);
    }

    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        stream->writeString(mShaderHLSL[shaderType]);
        stream->writeBytes(reinterpret_cast<unsigned char *>(&mShaderWorkarounds[shaderType]),
                           sizeof(angle::CompilerWorkaroundsD3D));
    }

    stream->writeInt(mUsesFragDepth);
    stream->writeInt(mHasANGLEMultiviewEnabled);
    stream->writeInt(mUsesVertexID);
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
        stream->writeInt(variable.outputLocation);
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

    stream->writeInt(mComputeExecutables.size());
    for (size_t computeExecutableIndex = 0; computeExecutableIndex < mComputeExecutables.size();
         computeExecutableIndex++)
    {
        ComputeExecutable *computeExecutable = mComputeExecutables[computeExecutableIndex].get();

        const gl::ImageUnitTextureTypeMap signatures = computeExecutable->signature();
        stream->writeInt(signatures.size());
        for (const auto &signature : signatures)
        {
            stream->writeInt(signature.first);
            stream->writeInt(static_cast<unsigned int>(signature.second));
        }

        size_t computeShaderSize = computeExecutable->shaderExecutable()->getLength();
        stream->writeInt(computeShaderSize);

        const uint8_t *computeBlob = computeExecutable->shaderExecutable()->getFunction();
        stream->writeBytes(computeBlob, computeShaderSize);
    }

    stream->writeInt(mComputeShaderImage2DBindLayoutCache.size());
    for (auto &image2DBindLayout : mComputeShaderImage2DBindLayoutCache)
    {
        stream->writeInt(image2DBindLayout.first);
    }
}

void ProgramD3D::setBinaryRetrievableHint(bool /* retrievable */) {}

void ProgramD3D::setSeparable(bool /* separable */) {}

angle::Result ProgramD3D::getPixelExecutableForCachedOutputLayout(
    d3d::Context *context,
    ShaderExecutableD3D **outExecutable,
    gl::InfoLog *infoLog)
{
    if (mCachedPixelExecutableIndex.valid())
    {
        *outExecutable = mPixelExecutables[mCachedPixelExecutableIndex.value()]->shaderExecutable();
        return angle::Result::Continue;
    }

    std::string finalPixelHLSL = mDynamicHLSL->generatePixelShaderForOutputSignature(
        mShaderHLSL[gl::ShaderType::Fragment], mPixelShaderKey, mUsesFragDepth,
        mPixelShaderOutputLayoutCache);

    // Generate new pixel executable
    ShaderExecutableD3D *pixelExecutable = nullptr;

    gl::InfoLog tempInfoLog;
    gl::InfoLog *currentInfoLog = infoLog ? infoLog : &tempInfoLog;

    ANGLE_TRY(mRenderer->compileToExecutable(
        context, *currentInfoLog, finalPixelHLSL, gl::ShaderType::Fragment, mStreamOutVaryings,
        (mState.getTransformFeedbackBufferMode() == GL_SEPARATE_ATTRIBS),
        mShaderWorkarounds[gl::ShaderType::Fragment], &pixelExecutable));

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
    return angle::Result::Continue;
}

angle::Result ProgramD3D::getVertexExecutableForCachedInputLayout(
    d3d::Context *context,
    ShaderExecutableD3D **outExectuable,
    gl::InfoLog *infoLog)
{
    if (mCachedVertexExecutableIndex.valid())
    {
        *outExectuable =
            mVertexExecutables[mCachedVertexExecutableIndex.value()]->shaderExecutable();
        return angle::Result::Continue;
    }

    // Generate new dynamic layout with attribute conversions
    std::string finalVertexHLSL = mDynamicHLSL->generateVertexShaderForInputLayout(
        mShaderHLSL[gl::ShaderType::Vertex], mCachedInputLayout, mState.getProgramInputs());

    // Generate new vertex executable
    ShaderExecutableD3D *vertexExecutable = nullptr;

    gl::InfoLog tempInfoLog;
    gl::InfoLog *currentInfoLog = infoLog ? infoLog : &tempInfoLog;

    ANGLE_TRY(mRenderer->compileToExecutable(
        context, *currentInfoLog, finalVertexHLSL, gl::ShaderType::Vertex, mStreamOutVaryings,
        (mState.getTransformFeedbackBufferMode() == GL_SEPARATE_ATTRIBS),
        mShaderWorkarounds[gl::ShaderType::Vertex], &vertexExecutable));

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
    return angle::Result::Continue;
}

angle::Result ProgramD3D::getGeometryExecutableForPrimitiveType(d3d::Context *context,
                                                                const gl::State &state,
                                                                gl::PrimitiveMode drawMode,
                                                                ShaderExecutableD3D **outExecutable,
                                                                gl::InfoLog *infoLog)
{
    if (outExecutable)
    {
        *outExecutable = nullptr;
    }

    // Return a null shader if the current rendering doesn't use a geometry shader
    if (!usesGeometryShader(state, drawMode))
    {
        return angle::Result::Continue;
    }

    gl::PrimitiveMode geometryShaderType = GetGeometryShaderTypeFromDrawMode(drawMode);

    if (mGeometryExecutables[geometryShaderType])
    {
        if (outExecutable)
        {
            *outExecutable = mGeometryExecutables[geometryShaderType].get();
        }
        return angle::Result::Continue;
    }
    const gl::Caps &caps     = state.getCaps();
    std::string geometryHLSL = mDynamicHLSL->generateGeometryShaderHLSL(
        caps, geometryShaderType, mState, mRenderer->presentPathFastEnabled(),
        mHasANGLEMultiviewEnabled, mRenderer->canSelectViewInVertexShader(),
        usesGeometryShaderForPointSpriteEmulation(), mGeometryShaderPreamble);

    gl::InfoLog tempInfoLog;
    gl::InfoLog *currentInfoLog = infoLog ? infoLog : &tempInfoLog;

    ShaderExecutableD3D *geometryExecutable = nullptr;
    angle::Result result                    = mRenderer->compileToExecutable(
        context, *currentInfoLog, geometryHLSL, gl::ShaderType::Geometry, mStreamOutVaryings,
        (mState.getTransformFeedbackBufferMode() == GL_SEPARATE_ATTRIBS),
        angle::CompilerWorkaroundsD3D(), &geometryExecutable);

    if (!infoLog && result == angle::Result::Stop)
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
    return result;
}

class ProgramD3D::GetVertexExecutableTask : public ProgramD3D::GetExecutableTask
{
  public:
    GetVertexExecutableTask(ProgramD3D *program) : GetExecutableTask(program) {}
    angle::Result run() override
    {
        ANGLE_TRACE_EVENT0("gpu.angle", "ProgramD3D::GetVertexExecutableTask::run");
        if (!mProgram->mState.getAttachedShader(gl::ShaderType::Vertex))
        {
            return angle::Result::Continue;
        }

        mProgram->updateCachedInputLayoutFromShader();

        ANGLE_TRY(mProgram->getVertexExecutableForCachedInputLayout(this, &mExecutable, &mInfoLog));

        return angle::Result::Continue;
    }
};

void ProgramD3D::updateCachedInputLayoutFromShader()
{
    GetDefaultInputLayoutFromShader(mState.getAttachedShader(gl::ShaderType::Vertex),
                                    &mCachedInputLayout);
    VertexExecutable::getSignature(mRenderer, mCachedInputLayout, &mCachedVertexSignature);
    updateCachedVertexExecutableIndex();
}

class ProgramD3D::GetPixelExecutableTask : public ProgramD3D::GetExecutableTask
{
  public:
    GetPixelExecutableTask(ProgramD3D *program) : GetExecutableTask(program) {}
    angle::Result run() override
    {
        ANGLE_TRACE_EVENT0("gpu.angle", "ProgramD3D::GetPixelExecutableTask::run");
        if (!mProgram->mState.getAttachedShader(gl::ShaderType::Fragment))
        {
            return angle::Result::Continue;
        }

        mProgram->updateCachedOutputLayoutFromShader();

        ANGLE_TRY(mProgram->getPixelExecutableForCachedOutputLayout(this, &mExecutable, &mInfoLog));

        return angle::Result::Continue;
    }
};

void ProgramD3D::updateCachedOutputLayoutFromShader()
{
    GetDefaultOutputLayoutFromShader(mPixelShaderKey, &mPixelShaderOutputLayoutCache);
    updateCachedPixelExecutableIndex();
}

void ProgramD3D::updateCachedImage2DBindLayoutFromComputeShader()
{
    GetDefaultImage2DBindLayoutFromComputeShader(mImage2DUniforms,
                                                 &mComputeShaderImage2DBindLayoutCache);
    updateCachedComputeExecutableIndex();
}

class ProgramD3D::GetGeometryExecutableTask : public ProgramD3D::GetExecutableTask
{
  public:
    GetGeometryExecutableTask(ProgramD3D *program, const gl::State &state)
        : GetExecutableTask(program), mState(state)
    {}

    angle::Result run() override
    {
        ANGLE_TRACE_EVENT0("gpu.angle", "ProgramD3D::GetGeometryExecutableTask::run");
        // Auto-generate the geometry shader here, if we expect to be using point rendering in
        // D3D11.
        if (mProgram->usesGeometryShader(mState, gl::PrimitiveMode::Points))
        {
            ANGLE_TRY(mProgram->getGeometryExecutableForPrimitiveType(
                this, mState, gl::PrimitiveMode::Points, &mExecutable, &mInfoLog));
        }

        return angle::Result::Continue;
    }

  private:
    const gl::State &mState;
};

class ProgramD3D::GetComputeExecutableTask : public ProgramD3D::GetExecutableTask
{
  public:
    GetComputeExecutableTask(ProgramD3D *program) : GetExecutableTask(program) {}
    angle::Result run() override
    {
        ANGLE_TRACE_EVENT0("gpu.angle", "ProgramD3D::GetComputeExecutableTask::run");
        mProgram->updateCachedImage2DBindLayoutFromComputeShader();
        ShaderExecutableD3D *computeExecutable = nullptr;
        ANGLE_TRY(mProgram->getComputeExecutableForImage2DBindLayout(this, &computeExecutable,
                                                                     &mInfoLog));

        return computeExecutable ? angle::Result::Continue : angle::Result::Incomplete;
    }
};

// The LinkEvent implementation for linking a rendering(VS, FS, GS) program.
class ProgramD3D::GraphicsProgramLinkEvent final : public LinkEvent
{
  public:
    GraphicsProgramLinkEvent(gl::InfoLog &infoLog,
                             std::shared_ptr<WorkerThreadPool> workerPool,
                             std::shared_ptr<ProgramD3D::GetVertexExecutableTask> vertexTask,
                             std::shared_ptr<ProgramD3D::GetPixelExecutableTask> pixelTask,
                             std::shared_ptr<ProgramD3D::GetGeometryExecutableTask> geometryTask,
                             bool useGS,
                             const ShaderD3D *vertexShader,
                             const ShaderD3D *fragmentShader)
        : mInfoLog(infoLog),
          mVertexTask(vertexTask),
          mPixelTask(pixelTask),
          mGeometryTask(geometryTask),
          mWaitEvents({{std::shared_ptr<WaitableEvent>(
                            angle::WorkerThreadPool::PostWorkerTask(workerPool, mVertexTask)),
                        std::shared_ptr<WaitableEvent>(
                            angle::WorkerThreadPool::PostWorkerTask(workerPool, mPixelTask)),
                        std::shared_ptr<WaitableEvent>(
                            angle::WorkerThreadPool::PostWorkerTask(workerPool, mGeometryTask))}}),
          mUseGS(useGS),
          mVertexShader(vertexShader),
          mFragmentShader(fragmentShader)
    {}

    angle::Result wait(const gl::Context *context) override
    {
        ANGLE_TRACE_EVENT0("gpu.angle", "ProgramD3D::GraphicsProgramLinkEvent::wait");
        WaitableEvent::WaitMany(&mWaitEvents);

        ANGLE_TRY(checkTask(context, mVertexTask.get()));
        ANGLE_TRY(checkTask(context, mPixelTask.get()));
        ANGLE_TRY(checkTask(context, mGeometryTask.get()));

        if (mVertexTask->getResult() == angle::Result::Incomplete ||
            mPixelTask->getResult() == angle::Result::Incomplete ||
            mGeometryTask->getResult() == angle::Result::Incomplete)
        {
            return angle::Result::Incomplete;
        }

        ShaderExecutableD3D *defaultVertexExecutable = mVertexTask->getExecutable();
        ShaderExecutableD3D *defaultPixelExecutable  = mPixelTask->getExecutable();
        ShaderExecutableD3D *pointGS                 = mGeometryTask->getExecutable();

        if (mUseGS && pointGS)
        {
            // Geometry shaders are currently only used internally, so there is no corresponding
            // shader object at the interface level. For now the geometry shader debug info is
            // prepended to the vertex shader.
            mVertexShader->appendDebugInfo("// GEOMETRY SHADER BEGIN\n\n");
            mVertexShader->appendDebugInfo(pointGS->getDebugInfo());
            mVertexShader->appendDebugInfo("\nGEOMETRY SHADER END\n\n\n");
        }

        if (defaultVertexExecutable)
        {
            mVertexShader->appendDebugInfo(defaultVertexExecutable->getDebugInfo());
        }

        if (defaultPixelExecutable)
        {
            mFragmentShader->appendDebugInfo(defaultPixelExecutable->getDebugInfo());
        }

        bool isLinked = (defaultVertexExecutable && defaultPixelExecutable && (!mUseGS || pointGS));
        if (!isLinked)
        {
            mInfoLog << "Failed to create D3D Shaders";
        }
        return isLinked ? angle::Result::Continue : angle::Result::Incomplete;
    }

    bool isLinking() override
    {
        for (auto &event : mWaitEvents)
        {
            if (!event->isReady())
            {
                return true;
            }
        }
        return false;
    }

  private:
    angle::Result checkTask(const gl::Context *context, ProgramD3D::GetExecutableTask *task)
    {
        if (!task->getInfoLog().empty())
        {
            mInfoLog << task->getInfoLog().str();
        }

        // Continue and Incomplete are not errors. For Stop, pass the error to the ContextD3D.
        if (task->getResult() != angle::Result::Stop)
        {
            return angle::Result::Continue;
        }

        ContextD3D *contextD3D = GetImplAs<ContextD3D>(context);
        task->popError(contextD3D);
        return angle::Result::Stop;
    }

    gl::InfoLog &mInfoLog;
    std::shared_ptr<ProgramD3D::GetVertexExecutableTask> mVertexTask;
    std::shared_ptr<ProgramD3D::GetPixelExecutableTask> mPixelTask;
    std::shared_ptr<ProgramD3D::GetGeometryExecutableTask> mGeometryTask;
    std::array<std::shared_ptr<WaitableEvent>, 3> mWaitEvents;
    bool mUseGS;
    const ShaderD3D *mVertexShader;
    const ShaderD3D *mFragmentShader;
};

// The LinkEvent implementation for linking a computing program.
class ProgramD3D::ComputeProgramLinkEvent final : public LinkEvent
{
  public:
    ComputeProgramLinkEvent(gl::InfoLog &infoLog,
                            std::shared_ptr<ProgramD3D::GetComputeExecutableTask> computeTask,
                            std::shared_ptr<WaitableEvent> event)
        : mInfoLog(infoLog), mComputeTask(computeTask), mWaitEvent(event)
    {}

    bool isLinking() override { return !mWaitEvent->isReady(); }

    angle::Result wait(const gl::Context *context) override
    {
        ANGLE_TRACE_EVENT0("gpu.angle", "ProgramD3D::ComputeProgramLinkEvent::wait");
        mWaitEvent->wait();

        angle::Result result = mComputeTask->getResult();
        if (result != angle::Result::Continue)
        {
            mInfoLog << "Failed to create D3D compute shader.";
        }
        return result;
    }

  private:
    gl::InfoLog &mInfoLog;
    std::shared_ptr<ProgramD3D::GetComputeExecutableTask> mComputeTask;
    std::shared_ptr<WaitableEvent> mWaitEvent;
};

std::unique_ptr<LinkEvent> ProgramD3D::compileProgramExecutables(const gl::Context *context,
                                                                 gl::InfoLog &infoLog)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ProgramD3D::compileProgramExecutables");
    // Ensure the compiler is initialized to avoid race conditions.
    angle::Result result = mRenderer->ensureHLSLCompilerInitialized(GetImplAs<ContextD3D>(context));
    if (result != angle::Result::Continue)
    {
        return std::make_unique<LinkEventDone>(result);
    }

    auto vertexTask   = std::make_shared<GetVertexExecutableTask>(this);
    auto pixelTask    = std::make_shared<GetPixelExecutableTask>(this);
    auto geometryTask = std::make_shared<GetGeometryExecutableTask>(this, context->getState());
    bool useGS        = usesGeometryShader(context->getState(), gl::PrimitiveMode::Points);
    gl::Shader *vertexShader         = mState.getAttachedShader(gl::ShaderType::Vertex);
    gl::Shader *fragmentShader       = mState.getAttachedShader(gl::ShaderType::Fragment);
    const ShaderD3D *vertexShaderD3D = vertexShader ? GetImplAs<ShaderD3D>(vertexShader) : nullptr;
    const ShaderD3D *fragmentShaderD3D =
        fragmentShader ? GetImplAs<ShaderD3D>(fragmentShader) : nullptr;

    return std::make_unique<GraphicsProgramLinkEvent>(infoLog, context->getWorkerThreadPool(),
                                                      vertexTask, pixelTask, geometryTask, useGS,
                                                      vertexShaderD3D, fragmentShaderD3D);
}

std::unique_ptr<LinkEvent> ProgramD3D::compileComputeExecutable(const gl::Context *context,
                                                                gl::InfoLog &infoLog)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ProgramD3D::compileComputeExecutable");
    // Ensure the compiler is initialized to avoid race conditions.
    angle::Result result = mRenderer->ensureHLSLCompilerInitialized(GetImplAs<ContextD3D>(context));
    if (result != angle::Result::Continue)
    {
        return std::make_unique<LinkEventDone>(result);
    }
    auto computeTask = std::make_shared<GetComputeExecutableTask>(this);

    std::shared_ptr<WaitableEvent> waitableEvent;

    // TODO(jie.a.chen@intel.com): Fix the flaky bug.
    // http://anglebug.com/3349
    bool compileInParallel = false;
    if (!compileInParallel)
    {
        (*computeTask)();
        waitableEvent = std::make_shared<WaitableEventDone>();
    }
    else
    {
        waitableEvent =
            WorkerThreadPool::PostWorkerTask(context->getWorkerThreadPool(), computeTask);
    }

    return std::make_unique<ComputeProgramLinkEvent>(infoLog, computeTask, waitableEvent);
}

angle::Result ProgramD3D::getComputeExecutableForImage2DBindLayout(
    d3d::Context *context,
    ShaderExecutableD3D **outExecutable,
    gl::InfoLog *infoLog)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ProgramD3D::getComputeExecutableForImage2DBindLayout");
    if (mCachedComputeExecutableIndex.valid())
    {
        *outExecutable =
            mComputeExecutables[mCachedComputeExecutableIndex.value()]->shaderExecutable();
        return angle::Result::Continue;
    }

    std::string finalComputeHLSL = mDynamicHLSL->generateComputeShaderForImage2DBindSignature(
        context, *this, mState, mImage2DUniforms, mComputeShaderImage2DBindLayoutCache);

    // Generate new compute executable
    ShaderExecutableD3D *computeExecutable = nullptr;

    gl::InfoLog tempInfoLog;
    gl::InfoLog *currentInfoLog = infoLog ? infoLog : &tempInfoLog;

    ANGLE_TRY(mRenderer->compileToExecutable(
        context, *currentInfoLog, finalComputeHLSL, gl::ShaderType::Compute,
        std::vector<D3DVarying>(), false, angle::CompilerWorkaroundsD3D(), &computeExecutable));

    if (computeExecutable)
    {
        mComputeExecutables.push_back(std::unique_ptr<ComputeExecutable>(
            new ComputeExecutable(mComputeShaderImage2DBindLayoutCache,
                                  std::unique_ptr<ShaderExecutableD3D>(computeExecutable))));
        mCachedComputeExecutableIndex = mComputeExecutables.size() - 1;
    }
    else if (!infoLog)
    {
        ERR() << "Error compiling dynamic compute executable:" << std::endl
              << tempInfoLog.str() << std::endl;
    }
    *outExecutable = computeExecutable;

    return angle::Result::Continue;
}

std::unique_ptr<LinkEvent> ProgramD3D::link(const gl::Context *context,
                                            const gl::ProgramLinkedResources &resources,
                                            gl::InfoLog &infoLog)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ProgramD3D::link");
    const auto &data = context->getState();

    reset();

    gl::Shader *computeShader = mState.getAttachedShader(gl::ShaderType::Compute);
    if (computeShader)
    {
        mShaderSamplers[gl::ShaderType::Compute].resize(
            data.getCaps().maxShaderTextureImageUnits[gl::ShaderType::Compute]);
        mImagesCS.resize(data.getCaps().maxImageUnits);
        mReadonlyImagesCS.resize(data.getCaps().maxImageUnits);

        mShaderUniformsDirty.set(gl::ShaderType::Compute);

        linkResources(resources);

        for (const sh::ShaderVariable &uniform : computeShader->getUniforms())
        {
            if (gl::IsImageType(uniform.type) && gl::IsImage2DType(uniform.type))
            {
                mImage2DUniforms.push_back(uniform);
            }
        }

        defineUniformsAndAssignRegisters();

        return compileComputeExecutable(context, infoLog);
    }
    else
    {
        gl::ShaderMap<const ShaderD3D *> shadersD3D = {};
        for (gl::ShaderType shaderType : gl::kAllGraphicsShaderTypes)
        {
            if (mState.getAttachedShader(shaderType))
            {
                shadersD3D[shaderType] = GetImplAs<ShaderD3D>(mState.getAttachedShader(shaderType));

                mShaderSamplers[shaderType].resize(
                    data.getCaps().maxShaderTextureImageUnits[shaderType]);

                shadersD3D[shaderType]->generateWorkarounds(&mShaderWorkarounds[shaderType]);

                mShaderUniformsDirty.set(shaderType);
            }
        }

        if (mRenderer->getNativeLimitations().noFrontFacingSupport)
        {
            const ShaderD3D *fragmentShader = shadersD3D[gl::ShaderType::Fragment];
            if (fragmentShader && fragmentShader->usesFrontFacing())
            {
                infoLog << "The current renderer doesn't support gl_FrontFacing";
                return std::make_unique<LinkEventDone>(angle::Result::Incomplete);
            }
        }

        ProgramD3DMetadata metadata(mRenderer, shadersD3D, context->getClientType());
        BuiltinVaryingsD3D builtins(metadata, resources.varyingPacking);

        mDynamicHLSL->generateShaderLinkHLSL(context->getCaps(), mState, metadata,
                                             resources.varyingPacking, builtins, &mShaderHLSL);

        const ShaderD3D *vertexShader = shadersD3D[gl::ShaderType::Vertex];
        mUsesPointSize                = vertexShader && vertexShader->usesPointSize();
        mDynamicHLSL->getPixelShaderOutputKey(data, mState, metadata, &mPixelShaderKey);
        mUsesFragDepth            = metadata.usesFragDepth();
        mUsesVertexID             = metadata.usesVertexID();
        mUsesViewID               = metadata.usesViewID();
        mHasANGLEMultiviewEnabled = metadata.hasANGLEMultiviewEnabled();

        // Cache if we use flat shading
        mUsesFlatInterpolation = FindFlatInterpolationVarying(mState.getAttachedShaders());

        if (mRenderer->getMajorShaderModel() >= 4)
        {
            mGeometryShaderPreamble = mDynamicHLSL->generateGeometryShaderPreamble(
                resources.varyingPacking, builtins, mHasANGLEMultiviewEnabled,
                metadata.canSelectViewInVertexShader());
        }

        initAttribLocationsToD3DSemantic();

        defineUniformsAndAssignRegisters();

        gatherTransformFeedbackVaryings(resources.varyingPacking, builtins[gl::ShaderType::Vertex]);

        linkResources(resources);

        return compileProgramExecutables(context, infoLog);
    }
}

GLboolean ProgramD3D::validate(const gl::Caps & /*caps*/, gl::InfoLog * /*infoLog*/)
{
    // TODO(jmadill): Do something useful here?
    return GL_TRUE;
}

void ProgramD3D::initializeShaderStorageBlocks()
{
    if (mState.getShaderStorageBlocks().empty())
    {
        return;
    }

    ASSERT(mD3DShaderStorageBlocks.empty());

    // Assign registers and update sizes.
    gl::ShaderMap<const ShaderD3D *> shadersD3D = {};
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        shadersD3D[shaderType] = SafeGetImplAs<ShaderD3D>(mState.getAttachedShader(shaderType));
    }

    for (const gl::InterfaceBlock &shaderStorageBlock : mState.getShaderStorageBlocks())
    {
        unsigned int shaderStorageBlockElement =
            shaderStorageBlock.isArray ? shaderStorageBlock.arrayElement : 0;

        D3DInterfaceBlock d3dShaderStorageBlock;

        for (gl::ShaderType shaderType : gl::AllShaderTypes())
        {
            if (shaderStorageBlock.isActive(shaderType))
            {
                ASSERT(shadersD3D[shaderType]);
                unsigned int baseRegister =
                    shadersD3D[shaderType]->getShaderStorageBlockRegister(shaderStorageBlock.name);
                d3dShaderStorageBlock.mShaderRegisterIndexes[shaderType] =
                    baseRegister + shaderStorageBlockElement;
            }
        }

        mD3DShaderStorageBlocks.push_back(d3dShaderStorageBlock);
    }
}

void ProgramD3D::initializeUniformBlocks()
{
    if (mState.getUniformBlocks().empty())
    {
        return;
    }

    ASSERT(mD3DUniformBlocks.empty());

    // Assign registers and update sizes.
    gl::ShaderMap<const ShaderD3D *> shadersD3D = {};
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        shadersD3D[shaderType] = SafeGetImplAs<ShaderD3D>(mState.getAttachedShader(shaderType));
    }

    for (const gl::InterfaceBlock &uniformBlock : mState.getUniformBlocks())
    {
        unsigned int uniformBlockElement = uniformBlock.isArray ? uniformBlock.arrayElement : 0;

        D3DUniformBlock d3dUniformBlock;

        for (gl::ShaderType shaderType : gl::AllShaderTypes())
        {
            if (uniformBlock.isActive(shaderType))
            {
                ASSERT(shadersD3D[shaderType]);
                unsigned int baseRegister =
                    shadersD3D[shaderType]->getUniformBlockRegister(uniformBlock.name);
                d3dUniformBlock.mShaderRegisterIndexes[shaderType] =
                    baseRegister + uniformBlockElement;
                bool useStructuredBuffer =
                    shadersD3D[shaderType]->shouldUniformBlockUseStructuredBuffer(
                        uniformBlock.name);
                if (useStructuredBuffer)
                {
                    d3dUniformBlock.mUseStructuredBuffers[shaderType] = true;
                    d3dUniformBlock.mByteWidths[shaderType]           = uniformBlock.dataSize;
                    d3dUniformBlock.mStructureByteStrides[shaderType] =
                        uniformBlock.firstFieldArraySize == 0u
                            ? uniformBlock.dataSize
                            : uniformBlock.dataSize / uniformBlock.firstFieldArraySize;
                }
            }
        }

        mD3DUniformBlocks.push_back(d3dUniformBlock);
    }
}

void ProgramD3D::initializeUniformStorage(const gl::ShaderBitSet &availableShaderStages)
{
    // Compute total default block size
    gl::ShaderMap<unsigned int> shaderRegisters = {};
    for (const D3DUniform *d3dUniform : mD3DUniforms)
    {
        if (d3dUniform->isSampler())
        {
            continue;
        }

        for (gl::ShaderType shaderType : availableShaderStages)
        {
            if (d3dUniform->isReferencedByShader(shaderType))
            {
                shaderRegisters[shaderType] = std::max(
                    shaderRegisters[shaderType],
                    d3dUniform->mShaderRegisterIndexes[shaderType] + d3dUniform->registerCount);
            }
        }
    }

    // We only reset uniform storages for the shader stages available in the program (attached
    // shaders in ProgramD3D::link() and linkedShaderStages in ProgramD3D::load()).
    for (gl::ShaderType shaderType : availableShaderStages)
    {
        mShaderUniformStorages[shaderType].reset(
            mRenderer->createUniformStorage(shaderRegisters[shaderType] * 16u));
    }

    // Iterate the uniforms again to assign data pointers to default block uniforms.
    for (D3DUniform *d3dUniform : mD3DUniforms)
    {
        if (d3dUniform->isSampler())
        {
            d3dUniform->mSamplerData.resize(d3dUniform->getArraySizeProduct(), 0);
            continue;
        }

        for (gl::ShaderType shaderType : availableShaderStages)
        {
            if (d3dUniform->isReferencedByShader(shaderType))
            {
                d3dUniform->mShaderData[shaderType] =
                    mShaderUniformStorages[shaderType]->getDataPointer(
                        d3dUniform->mShaderRegisterIndexes[shaderType],
                        d3dUniform->registerElement);
            }
        }
    }
}

void ProgramD3D::updateUniformBufferCache(const gl::Caps &caps)
{
    if (mState.getUniformBlocks().empty())
    {
        return;
    }

    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mShaderUBOCaches[shaderType].clear();
        mShaderUBOCachesUseSB[shaderType].clear();
    }

    for (unsigned int uniformBlockIndex = 0; uniformBlockIndex < mD3DUniformBlocks.size();
         uniformBlockIndex++)
    {
        const D3DUniformBlock &uniformBlock = mD3DUniformBlocks[uniformBlockIndex];
        GLuint blockBinding                 = mState.getUniformBlockBinding(uniformBlockIndex);

        // Unnecessary to apply an unreferenced standard or shared UBO
        for (gl::ShaderType shaderType : gl::AllShaderTypes())
        {
            if (!uniformBlock.activeInShader(shaderType))
            {
                continue;
            }

            bool useStructuredBuffer   = uniformBlock.mUseStructuredBuffers[shaderType];
            unsigned int registerIndex = uniformBlock.mShaderRegisterIndexes[shaderType];
            if (useStructuredBuffer)
            {
                D3DUBOCacheUseSB cacheUseSB;
                cacheUseSB.registerIndex       = registerIndex;
                cacheUseSB.binding             = blockBinding;
                cacheUseSB.byteWidth           = uniformBlock.mByteWidths[shaderType];
                cacheUseSB.structureByteStride = uniformBlock.mStructureByteStrides[shaderType];
                mShaderUBOCachesUseSB[shaderType].push_back(cacheUseSB);
            }
            else
            {
                ASSERT(registerIndex <
                       static_cast<unsigned int>(caps.maxShaderUniformBlocks[shaderType]));
                D3DUBOCache cache;
                cache.registerIndex = registerIndex;
                cache.binding       = blockBinding;
                mShaderUBOCaches[shaderType].push_back(cache);
            }
        }
    }

    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        GLuint uniformBlockCount = static_cast<GLuint>(mShaderUBOCaches[shaderType].size() +
                                                       mShaderUBOCachesUseSB[shaderType].size());
        ASSERT(uniformBlockCount <=
               static_cast<unsigned int>(caps.maxShaderUniformBlocks[shaderType]));
    }
}

unsigned int ProgramD3D::getAtomicCounterBufferRegisterIndex(GLuint binding,
                                                             gl::ShaderType shaderType) const
{
    if (shaderType != gl::ShaderType::Compute)
    {
        // Implement atomic counters for non-compute shaders
        // http://anglebug.com/1729
        UNIMPLEMENTED();
    }
    return mComputeAtomicCounterBufferRegisterIndices[binding];
}

unsigned int ProgramD3D::getShaderStorageBufferRegisterIndex(GLuint blockIndex,
                                                             gl::ShaderType shaderType) const
{
    return mD3DShaderStorageBlocks[blockIndex].mShaderRegisterIndexes[shaderType];
}

const std::vector<D3DUBOCache> &ProgramD3D::getShaderUniformBufferCache(
    gl::ShaderType shaderType) const
{
    return mShaderUBOCaches[shaderType];
}

const std::vector<D3DUBOCacheUseSB> &ProgramD3D::getShaderUniformBufferCacheUseSB(
    gl::ShaderType shaderType) const
{
    return mShaderUBOCachesUseSB[shaderType];
}

void ProgramD3D::dirtyAllUniforms()
{
    mShaderUniformsDirty = mState.getExecutable().getLinkedShaderStages();
}

void ProgramD3D::markUniformsClean()
{
    mShaderUniformsDirty.reset();
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
    setUniformMatrixfvInternal<2, 2>(location, count, transpose, value);
}

void ProgramD3D::setUniformMatrix3fv(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value)
{
    setUniformMatrixfvInternal<3, 3>(location, count, transpose, value);
}

void ProgramD3D::setUniformMatrix4fv(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value)
{
    setUniformMatrixfvInternal<4, 4>(location, count, transpose, value);
}

void ProgramD3D::setUniformMatrix2x3fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfvInternal<2, 3>(location, count, transpose, value);
}

void ProgramD3D::setUniformMatrix3x2fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfvInternal<3, 2>(location, count, transpose, value);
}

void ProgramD3D::setUniformMatrix2x4fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfvInternal<2, 4>(location, count, transpose, value);
}

void ProgramD3D::setUniformMatrix4x2fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfvInternal<4, 2>(location, count, transpose, value);
}

void ProgramD3D::setUniformMatrix3x4fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfvInternal<3, 4>(location, count, transpose, value);
}

void ProgramD3D::setUniformMatrix4x3fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfvInternal<4, 3>(location, count, transpose, value);
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

void ProgramD3D::defineUniformsAndAssignRegisters()
{
    D3DUniformMap uniformMap;

    gl::ShaderBitSet attachedShaders;
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        gl::Shader *shader = mState.getAttachedShader(shaderType);
        if (shader)
        {
            for (const sh::ShaderVariable &uniform : shader->getUniforms())
            {
                if (uniform.active)
                {
                    defineUniformBase(shader, uniform, &uniformMap);
                }
            }

            attachedShaders.set(shader->getType());
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
    assignAllAtomicCounterRegisters();
    // Samplers and readonly images share shader input resource slot, adjust low value of
    // readonly image range.
    mUsedComputeReadonlyImageRange =
        gl::RangeUI(mUsedShaderSamplerRanges[gl::ShaderType::Compute].high(),
                    mUsedShaderSamplerRanges[gl::ShaderType::Compute].high());
    // Atomic counter buffers and non-readonly images share input resource slots
    mUsedComputeImageRange =
        gl::RangeUI(mUsedComputeAtomicCounterRange.high(), mUsedComputeAtomicCounterRange.high());
    assignAllImageRegisters();
    initializeUniformStorage(attachedShaders);
}

void ProgramD3D::defineUniformBase(const gl::Shader *shader,
                                   const sh::ShaderVariable &uniform,
                                   D3DUniformMap *uniformMap)
{
    sh::DummyBlockEncoder dummyEncoder;

    // Samplers get their registers assigned in assignAllSamplerRegisters, and images get their
    // registers assigned in assignAllImageRegisters.
    if (gl::IsSamplerType(uniform.type))
    {
        UniformEncodingVisitorD3D visitor(shader->getType(), HLSLRegisterType::Texture,
                                          &dummyEncoder, uniformMap);
        sh::TraverseShaderVariable(uniform, false, &visitor);
        return;
    }

    if (gl::IsImageType(uniform.type))
    {
        if (uniform.readonly)
        {
            UniformEncodingVisitorD3D visitor(shader->getType(), HLSLRegisterType::Texture,
                                              &dummyEncoder, uniformMap);
            sh::TraverseShaderVariable(uniform, false, &visitor);
        }
        else
        {
            UniformEncodingVisitorD3D visitor(shader->getType(),
                                              HLSLRegisterType::UnorderedAccessView, &dummyEncoder,
                                              uniformMap);
            sh::TraverseShaderVariable(uniform, false, &visitor);
        }
        mImageBindingMap[uniform.name] = uniform.binding;
        return;
    }

    if (uniform.isBuiltIn() && !uniform.isEmulatedBuiltIn())
    {
        UniformEncodingVisitorD3D visitor(shader->getType(), HLSLRegisterType::None, &dummyEncoder,
                                          uniformMap);
        sh::TraverseShaderVariable(uniform, false, &visitor);
        return;
    }
    else if (gl::IsAtomicCounterType(uniform.type))
    {
        UniformEncodingVisitorD3D visitor(shader->getType(), HLSLRegisterType::UnorderedAccessView,
                                          &dummyEncoder, uniformMap);
        sh::TraverseShaderVariable(uniform, false, &visitor);
        mAtomicBindingMap[uniform.name] = uniform.binding;
        return;
    }

    const ShaderD3D *shaderD3D = GetImplAs<ShaderD3D>(shader);
    unsigned int startRegister = shaderD3D->getUniformRegister(uniform.name);
    ShShaderOutput outputType  = shaderD3D->getCompilerOutputType();
    sh::HLSLBlockEncoder encoder(sh::HLSLBlockEncoder::GetStrategyFor(outputType), true);
    encoder.skipRegisters(startRegister);

    UniformEncodingVisitorD3D visitor(shader->getType(), HLSLRegisterType::None, &encoder,
                                      uniformMap);
    sh::TraverseShaderVariable(uniform, false, &visitor);
}

bool ProgramD3D::hasNamedUniform(const std::string &name)
{
    for (D3DUniform *d3dUniform : mD3DUniforms)
    {
        if (d3dUniform->name == name)
        {
            return true;
        }
    }

    return false;
}

// Assume count is already clamped.
template <typename T>
void ProgramD3D::setUniformImpl(const gl::VariableLocation &locationInfo,
                                GLsizei count,
                                const T *v,
                                uint8_t *targetData,
                                GLenum uniformType)
{
    D3DUniform *targetUniform             = mD3DUniforms[locationInfo.index];
    const int components                  = targetUniform->typeInfo.componentCount;
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

    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        if (targetUniform->mShaderData[shaderType])
        {
            setUniformImpl(locationInfo, count, v, targetUniform->mShaderData[shaderType],
                           uniformType);
            mShaderUniformsDirty.set(shaderType);
        }
    }
}

template <int cols, int rows>
void ProgramD3D::setUniformMatrixfvInternal(GLint location,
                                            GLsizei countIn,
                                            GLboolean transpose,
                                            const GLfloat *value)
{
    D3DUniform *targetUniform                   = getD3DUniformFromLocation(location);
    const gl::VariableLocation &uniformLocation = mState.getUniformLocations()[location];
    unsigned int arrayElementOffset             = uniformLocation.arrayIndex;
    unsigned int elementCount                   = targetUniform->getArraySizeProduct();

    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        if (targetUniform->mShaderData[shaderType])
        {
            SetFloatUniformMatrixHLSL<cols, rows>::Run(arrayElementOffset, elementCount, countIn,
                                                       transpose, value,
                                                       targetUniform->mShaderData[shaderType]);
            mShaderUniformsDirty.set(shaderType);
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
    const std::string baseName = gl::ParseResourceName(d3dUniform->name, &subscripts);
    unsigned int registerOffset =
        mState.getUniforms()[uniformIndex].parentArrayIndex() * d3dUniform->getArraySizeProduct();

    bool hasUniform = false;
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        if (!mState.getAttachedShader(shaderType))
        {
            continue;
        }

        const ShaderD3D *shaderD3D = GetImplAs<ShaderD3D>(mState.getAttachedShader(shaderType));
        if (shaderD3D->hasUniform(baseName))
        {
            d3dUniform->mShaderRegisterIndexes[shaderType] =
                shaderD3D->getUniformRegister(baseName) + registerOffset;
            ASSERT(d3dUniform->mShaderRegisterIndexes[shaderType] != GL_INVALID_VALUE);

            AssignSamplers(d3dUniform->mShaderRegisterIndexes[shaderType], d3dUniform->typeInfo,
                           d3dUniform->getArraySizeProduct(), mShaderSamplers[shaderType],
                           &mUsedShaderSamplerRanges[shaderType]);
            hasUniform = true;
        }
    }

    ASSERT(hasUniform);
}

// static
void ProgramD3D::AssignSamplers(unsigned int startSamplerIndex,
                                const gl::UniformTypeInfo &typeInfo,
                                unsigned int samplerCount,
                                std::vector<Sampler> &outSamplers,
                                gl::RangeUI *outUsedRange)
{
    unsigned int samplerIndex = startSamplerIndex;
    unsigned int low          = outUsedRange->low();
    unsigned int high         = outUsedRange->high();

    do
    {
        ASSERT(samplerIndex < outSamplers.size());
        Sampler *sampler            = &outSamplers[samplerIndex];
        sampler->active             = true;
        sampler->textureType        = gl::FromGLenum<gl::TextureType>(typeInfo.textureType);
        sampler->logicalTextureUnit = 0;
        low                         = std::min(samplerIndex, low);
        high                        = std::max(samplerIndex + 1, high);
        samplerIndex++;
    } while (samplerIndex < startSamplerIndex + samplerCount);

    ASSERT(low < high);
    *outUsedRange = gl::RangeUI(low, high);
}

void ProgramD3D::assignAllImageRegisters()
{
    for (size_t uniformIndex = 0; uniformIndex < mD3DUniforms.size(); ++uniformIndex)
    {
        if (mD3DUniforms[uniformIndex]->isImage() && !mD3DUniforms[uniformIndex]->isImage2D())
        {
            assignImageRegisters(uniformIndex);
        }
    }
}

void ProgramD3D::assignAllAtomicCounterRegisters()
{
    if (mAtomicBindingMap.empty())
    {
        return;
    }
    gl::ShaderType shaderType       = gl::ShaderType::Compute;
    const gl::Shader *computeShader = mState.getAttachedShader(shaderType);
    if (computeShader)
    {
        const ShaderD3D *computeShaderD3D = GetImplAs<ShaderD3D>(computeShader);
        auto &registerIndices             = mComputeAtomicCounterBufferRegisterIndices;
        unsigned int firstRegister        = GL_INVALID_VALUE;
        unsigned int lastRegister         = 0;
        for (auto &atomicBinding : mAtomicBindingMap)
        {
            ASSERT(computeShaderD3D->hasUniform(atomicBinding.first));
            unsigned int currentRegister =
                computeShaderD3D->getUniformRegister(atomicBinding.first);
            ASSERT(currentRegister != GL_INVALID_INDEX);
            const int kBinding = atomicBinding.second;

            registerIndices[kBinding] = currentRegister;

            firstRegister = std::min(firstRegister, currentRegister);
            lastRegister  = std::max(lastRegister, currentRegister);
        }
        ASSERT(firstRegister != GL_INVALID_VALUE);
        ASSERT(lastRegister != GL_INVALID_VALUE);
        mUsedComputeAtomicCounterRange = gl::RangeUI(firstRegister, lastRegister + 1);
    }
    else
    {
        // Implement atomic counters for non-compute shaders
        // http://anglebug.com/1729
        UNIMPLEMENTED();
    }
}

void ProgramD3D::assignImageRegisters(size_t uniformIndex)
{
    D3DUniform *d3dUniform = mD3DUniforms[uniformIndex];
    ASSERT(d3dUniform->isImage());
    // If the uniform is an array of arrays, then we have separate entries for each inner array in
    // mD3DUniforms. However, the image register info is stored in the shader only for the
    // outermost array.
    std::vector<unsigned int> subscripts;
    const std::string baseName = gl::ParseResourceName(d3dUniform->name, &subscripts);
    unsigned int registerOffset =
        mState.getUniforms()[uniformIndex].parentArrayIndex() * d3dUniform->getArraySizeProduct();

    const gl::Shader *computeShader = mState.getAttachedShader(gl::ShaderType::Compute);
    if (computeShader)
    {
        const ShaderD3D *computeShaderD3D =
            GetImplAs<ShaderD3D>(mState.getAttachedShader(gl::ShaderType::Compute));
        ASSERT(computeShaderD3D->hasUniform(baseName));
        d3dUniform->mShaderRegisterIndexes[gl::ShaderType::Compute] =
            computeShaderD3D->getUniformRegister(baseName) + registerOffset;
        ASSERT(d3dUniform->mShaderRegisterIndexes[gl::ShaderType::Compute] != GL_INVALID_INDEX);
        auto bindingIter = mImageBindingMap.find(baseName);
        ASSERT(bindingIter != mImageBindingMap.end());
        if (d3dUniform->regType == HLSLRegisterType::Texture)
        {
            AssignImages(d3dUniform->mShaderRegisterIndexes[gl::ShaderType::Compute],
                         bindingIter->second, d3dUniform->getArraySizeProduct(), mReadonlyImagesCS,
                         &mUsedComputeReadonlyImageRange);
        }
        else if (d3dUniform->regType == HLSLRegisterType::UnorderedAccessView)
        {
            AssignImages(d3dUniform->mShaderRegisterIndexes[gl::ShaderType::Compute],
                         bindingIter->second, d3dUniform->getArraySizeProduct(), mImagesCS,
                         &mUsedComputeImageRange);
        }
        else
        {
            UNREACHABLE();
        }
    }
    else
    {
        // TODO(xinghua.cao@intel.com): Implement image variables in vertex shader and pixel shader.
        UNIMPLEMENTED();
    }
}

// static
void ProgramD3D::AssignImages(unsigned int startImageIndex,
                              int startLogicalImageUnit,
                              unsigned int imageCount,
                              std::vector<Image> &outImages,
                              gl::RangeUI *outUsedRange)
{
    unsigned int imageIndex = startImageIndex;
    unsigned int low        = outUsedRange->low();
    unsigned int high       = outUsedRange->high();

    // If declare without a binding qualifier, any uniform image variable (include all elements of
    // unbound image array) shoud be bound to unit zero.
    if (startLogicalImageUnit == -1)
    {
        ASSERT(imageIndex < outImages.size());
        Image *image            = &outImages[imageIndex];
        image->active           = true;
        image->logicalImageUnit = 0;
        low                     = std::min(imageIndex, low);
        high                    = std::max(imageIndex + 1, high);
        ASSERT(low < high);
        *outUsedRange = gl::RangeUI(low, high);
        return;
    }

    unsigned int logcalImageUnit = startLogicalImageUnit;
    do
    {
        ASSERT(imageIndex < outImages.size());
        Image *image            = &outImages[imageIndex];
        image->active           = true;
        image->logicalImageUnit = logcalImageUnit;
        low                     = std::min(imageIndex, low);
        high                    = std::max(imageIndex + 1, high);
        imageIndex++;
        logcalImageUnit++;
    } while (imageIndex < startImageIndex + imageCount);

    ASSERT(low < high);
    *outUsedRange = gl::RangeUI(low, high);
}

void ProgramD3D::assignImage2DRegisters(unsigned int startImageIndex,
                                        int startLogicalImageUnit,
                                        bool readonly)
{
    if (readonly)
    {
        AssignImages(startImageIndex, startLogicalImageUnit, 1, mReadonlyImagesCS,
                     &mUsedComputeReadonlyImageRange);
    }
    else
    {
        AssignImages(startImageIndex, startLogicalImageUnit, 1, mImagesCS, &mUsedComputeImageRange);
    }
}

void ProgramD3D::reset()
{
    mVertexExecutables.clear();
    mPixelExecutables.clear();
    mComputeExecutables.clear();

    for (auto &geometryExecutable : mGeometryExecutables)
    {
        geometryExecutable.reset(nullptr);
    }

    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mShaderHLSL[shaderType].clear();
    }

    mUsesFragDepth            = false;
    mHasANGLEMultiviewEnabled = false;
    mUsesVertexID             = false;
    mUsesViewID               = false;
    mPixelShaderKey.clear();
    mUsesPointSize         = false;
    mUsesFlatInterpolation = false;

    SafeDeleteContainer(mD3DUniforms);
    mD3DUniformBlocks.clear();
    mD3DShaderStorageBlocks.clear();
    mComputeAtomicCounterBufferRegisterIndices.fill({});

    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mShaderUniformStorages[shaderType].reset();
        mShaderSamplers[shaderType].clear();
    }

    mImagesCS.clear();
    mReadonlyImagesCS.clear();

    mUsedShaderSamplerRanges.fill({0, 0});
    mUsedComputeAtomicCounterRange = {0, 0};
    mDirtySamplerMapping           = true;
    mUsedComputeImageRange         = {0, 0};
    mUsedComputeReadonlyImageRange = {0, 0};

    mAttribLocationToD3DSemantic.fill(-1);

    mStreamOutVaryings.clear();

    mGeometryShaderPreamble.clear();

    markUniformsClean();

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

void ProgramD3D::initAttribLocationsToD3DSemantic()
{
    gl::Shader *vertexShader = mState.getAttachedShader(gl::ShaderType::Vertex);
    if (!vertexShader)
    {
        return;
    }

    // Init semantic index
    int semanticIndex = 0;
    for (const sh::ShaderVariable &attribute : vertexShader->getActiveAttributes())
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
    const gl::AttributesMask &attributesMask =
        mState.getExecutable().getActiveAttribLocationsMask();

    for (size_t locationIndex : attributesMask)
    {
        int d3dSemantic = mAttribLocationToD3DSemantic[locationIndex];

        if (d3dSemantic != -1)
        {
            if (mCachedInputLayout.size() < static_cast<size_t>(d3dSemantic + 1))
            {
                mCachedInputLayout.resize(d3dSemantic + 1, angle::FormatID::NONE);
            }
            mCachedInputLayout[d3dSemantic] =
                GetVertexFormatID(vertexAttributes[locationIndex],
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
            size_t maxIndex = binding != GL_NONE ? GetMaxOutputIndex(mPixelShaderKey,
                                                                     binding - GL_COLOR_ATTACHMENT0)
                                                 : 0;
            mPixelShaderOutputLayoutCache.insert(mPixelShaderOutputLayoutCache.end(), maxIndex + 1,
                                                 binding);
        }
        else
        {
            mPixelShaderOutputLayoutCache.push_back(GL_NONE);
        }
    }

    updateCachedPixelExecutableIndex();
}

void ProgramD3D::updateCachedComputeImage2DBindLayout(const gl::Context *context)
{
    const auto &glState = context->getState();
    for (auto &image2DBindLayout : mComputeShaderImage2DBindLayoutCache)
    {
        const gl::ImageUnit &imageUnit = glState.getImageUnit(image2DBindLayout.first);
        if (imageUnit.texture.get())
        {
            image2DBindLayout.second = imageUnit.texture->getType();
        }
        else
        {
            image2DBindLayout.second = gl::TextureType::_2D;
        }
    }

    updateCachedComputeExecutableIndex();
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
                mStreamOutVaryings.emplace_back(builtins.glPosition.semantic,
                                                builtins.glPosition.index, 4, outputSlot);
            }
        }
        else if (tfVaryingName == "gl_FragCoord")
        {
            if (builtins.glFragCoord.enabled)
            {
                mStreamOutVaryings.emplace_back(builtins.glFragCoord.semantic,
                                                builtins.glFragCoord.index, 4, outputSlot);
            }
        }
        else if (tfVaryingName == "gl_PointSize")
        {
            if (builtins.glPointSize.enabled)
            {
                mStreamOutVaryings.emplace_back("PSIZE", 0, 1, outputSlot);
            }
        }
        else
        {
            const auto &registerInfos = varyingPacking.getRegisterList();
            for (GLuint registerIndex = 0u; registerIndex < registerInfos.size(); ++registerIndex)
            {
                const auto &registerInfo = registerInfos[registerIndex];
                const auto &varying      = registerInfo.packedVarying->varying();
                GLenum transposedType    = gl::TransposeMatrixType(varying.type);
                int componentCount       = gl::VariableColumnCount(transposedType);
                ASSERT(!varying.isBuiltIn() && !varying.isStruct());

                // There can be more than one register assigned to a particular varying, and each
                // register needs its own stream out entry.
                if (registerInfo.tfVaryingName() == tfVaryingName)
                {
                    mStreamOutVaryings.emplace_back(varyingSemantic, registerIndex, componentCount,
                                                    outputSlot);
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

bool ProgramD3D::hasVertexExecutableForCachedInputLayout()
{
    return mCachedVertexExecutableIndex.valid();
}

bool ProgramD3D::hasGeometryExecutableForPrimitiveType(const gl::State &state,
                                                       gl::PrimitiveMode drawMode)
{
    if (!usesGeometryShader(state, drawMode))
    {
        // No shader necessary mean we have the required (null) executable.
        return true;
    }

    gl::PrimitiveMode geometryShaderType = GetGeometryShaderTypeFromDrawMode(drawMode);
    return mGeometryExecutables[geometryShaderType].get() != nullptr;
}

bool ProgramD3D::hasPixelExecutableForCachedOutputLayout()
{
    return mCachedPixelExecutableIndex.valid();
}

bool ProgramD3D::hasComputeExecutableForCachedImage2DBindLayout()
{
    return mCachedComputeExecutableIndex.valid();
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
        GetMatrixUniform(uniform.type, dataOut, reinterpret_cast<const DestT *>(srcPointer), true);
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

void ProgramD3D::updateCachedComputeExecutableIndex()
{
    mCachedComputeExecutableIndex.reset();
    for (size_t executableIndex = 0; executableIndex < mComputeExecutables.size();
         executableIndex++)
    {
        if (mComputeExecutables[executableIndex]->matchesSignature(
                mComputeShaderImage2DBindLayoutCache))
        {
            mCachedComputeExecutableIndex = executableIndex;
            break;
        }
    }
}

void ProgramD3D::linkResources(const gl::ProgramLinkedResources &resources)
{
    HLSLBlockLayoutEncoderFactory hlslEncoderFactory;
    gl::ProgramLinkedResourcesLinker linker(&hlslEncoderFactory);

    linker.linkResources(mState, resources);

    initializeUniformBlocks();
    initializeShaderStorageBlocks();
}

}  // namespace rx
