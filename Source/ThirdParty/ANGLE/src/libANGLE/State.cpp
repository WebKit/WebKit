//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// State.cpp: Implements the State class, encapsulating raw GL state.

#include "libANGLE/State.h"

#include <limits>
#include <string.h>

#include "common/bitset_utils.h"
#include "common/mathutil.h"
#include "common/matrix_utils.h"
#include "libANGLE/Caps.h"
#include "libANGLE/Context.h"
#include "libANGLE/Debug.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/Query.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/queryconversions.h"
#include "libANGLE/renderer/ContextImpl.h"

namespace
{

GLenum ActiveQueryType(const GLenum type)
{
    return (type == GL_ANY_SAMPLES_PASSED_CONSERVATIVE) ? GL_ANY_SAMPLES_PASSED : type;
}

}  // anonymous namepace

namespace gl
{

State::State()
    : mMaxDrawBuffers(0),
      mMaxCombinedTextureImageUnits(0),
      mDepthClearValue(0),
      mStencilClearValue(0),
      mScissorTest(false),
      mSampleCoverage(false),
      mSampleCoverageValue(0),
      mSampleCoverageInvert(false),
      mSampleMask(false),
      mMaxSampleMaskWords(0),
      mStencilRef(0),
      mStencilBackRef(0),
      mLineWidth(0),
      mGenerateMipmapHint(GL_NONE),
      mFragmentShaderDerivativeHint(GL_NONE),
      mBindGeneratesResource(true),
      mClientArraysEnabled(true),
      mNearZ(0),
      mFarZ(0),
      mReadFramebuffer(nullptr),
      mDrawFramebuffer(nullptr),
      mProgram(nullptr),
      mVertexArray(nullptr),
      mActiveSampler(0),
      mPrimitiveRestart(false),
      mMultiSampling(false),
      mSampleAlphaToOne(false),
      mFramebufferSRGB(true),
      mRobustResourceInit(false),
      mProgramBinaryCacheEnabled(false)
{
}

State::~State()
{
}

void State::initialize(const Context *context,
                       bool debug,
                       bool bindGeneratesResource,
                       bool clientArraysEnabled,
                       bool robustResourceInit,
                       bool programBinaryCacheEnabled)
{
    const Caps &caps             = context->getCaps();
    const Extensions &extensions = context->getExtensions();
    const Extensions &nativeExtensions = context->getImplementation()->getNativeExtensions();
    const Version &clientVersion = context->getClientVersion();

    mMaxDrawBuffers = caps.maxDrawBuffers;
    mMaxCombinedTextureImageUnits = caps.maxCombinedTextureImageUnits;

    setColorClearValue(0.0f, 0.0f, 0.0f, 0.0f);

    mDepthClearValue = 1.0f;
    mStencilClearValue = 0;

    mScissorTest = false;
    mScissor.x = 0;
    mScissor.y = 0;
    mScissor.width = 0;
    mScissor.height = 0;

    mBlendColor.red = 0;
    mBlendColor.green = 0;
    mBlendColor.blue = 0;
    mBlendColor.alpha = 0;

    mStencilRef = 0;
    mStencilBackRef = 0;

    mSampleCoverage = false;
    mSampleCoverageValue = 1.0f;
    mSampleCoverageInvert = false;

    mMaxSampleMaskWords = caps.maxSampleMaskWords;
    mSampleMask         = false;
    mSampleMaskValues.fill(~GLbitfield(0));

    mGenerateMipmapHint = GL_DONT_CARE;
    mFragmentShaderDerivativeHint = GL_DONT_CARE;

    mBindGeneratesResource = bindGeneratesResource;
    mClientArraysEnabled   = clientArraysEnabled;

    mLineWidth = 1.0f;

    mViewport.x = 0;
    mViewport.y = 0;
    mViewport.width = 0;
    mViewport.height = 0;
    mNearZ = 0.0f;
    mFarZ = 1.0f;

    mBlend.colorMaskRed = true;
    mBlend.colorMaskGreen = true;
    mBlend.colorMaskBlue = true;
    mBlend.colorMaskAlpha = true;

    mActiveSampler = 0;

    mVertexAttribCurrentValues.resize(caps.maxVertexAttributes);

    mUniformBuffers.resize(caps.maxUniformBufferBindings);

    mSamplerTextures[GL_TEXTURE_2D].resize(caps.maxCombinedTextureImageUnits);
    mSamplerTextures[GL_TEXTURE_CUBE_MAP].resize(caps.maxCombinedTextureImageUnits);
    if (clientVersion >= Version(3, 0))
    {
        // TODO: These could also be enabled via extension
        mSamplerTextures[GL_TEXTURE_2D_ARRAY].resize(caps.maxCombinedTextureImageUnits);
        mSamplerTextures[GL_TEXTURE_3D].resize(caps.maxCombinedTextureImageUnits);
    }
    if (clientVersion >= Version(3, 1))
    {
        mSamplerTextures[GL_TEXTURE_2D_MULTISAMPLE].resize(caps.maxCombinedTextureImageUnits);

        mAtomicCounterBuffers.resize(caps.maxAtomicCounterBufferBindings);
        mShaderStorageBuffers.resize(caps.maxShaderStorageBufferBindings);
        mImageUnits.resize(caps.maxImageUnits);
    }
    if (nativeExtensions.textureRectangle)
    {
        mSamplerTextures[GL_TEXTURE_RECTANGLE_ANGLE].resize(caps.maxCombinedTextureImageUnits);
    }
    if (nativeExtensions.eglImageExternal || nativeExtensions.eglStreamConsumerExternal)
    {
        mSamplerTextures[GL_TEXTURE_EXTERNAL_OES].resize(caps.maxCombinedTextureImageUnits);
    }
    mCompleteTextureCache.resize(caps.maxCombinedTextureImageUnits, nullptr);
    mCompleteTextureBindings.reserve(caps.maxCombinedTextureImageUnits);
    mCachedTexturesInitState = InitState::MayNeedInit;
    for (uint32_t textureIndex = 0; textureIndex < caps.maxCombinedTextureImageUnits;
         ++textureIndex)
    {
        mCompleteTextureBindings.emplace_back(OnAttachmentDirtyBinding(this, textureIndex));
    }

    mSamplers.resize(caps.maxCombinedTextureImageUnits);

    mActiveQueries[GL_ANY_SAMPLES_PASSED].set(context, nullptr);
    mActiveQueries[GL_ANY_SAMPLES_PASSED_CONSERVATIVE].set(context, nullptr);
    mActiveQueries[GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN].set(context, nullptr);
    mActiveQueries[GL_TIME_ELAPSED_EXT].set(context, nullptr);
    mActiveQueries[GL_COMMANDS_COMPLETED_CHROMIUM].set(context, nullptr);

    mProgram = nullptr;

    mReadFramebuffer = nullptr;
    mDrawFramebuffer = nullptr;

    mPrimitiveRestart = false;

    mDebug.setOutputEnabled(debug);
    mDebug.setMaxLoggedMessages(extensions.maxDebugLoggedMessages);

    mMultiSampling    = true;
    mSampleAlphaToOne = false;

    mCoverageModulation = GL_NONE;

    angle::Matrix<GLfloat>::setToIdentity(mPathMatrixProj);
    angle::Matrix<GLfloat>::setToIdentity(mPathMatrixMV);
    mPathStencilFunc = GL_ALWAYS;
    mPathStencilRef  = 0;
    mPathStencilMask = std::numeric_limits<GLuint>::max();

    mRobustResourceInit = robustResourceInit;
    mProgramBinaryCacheEnabled = programBinaryCacheEnabled;
}

void State::reset(const Context *context)
{
    for (auto &bindingVec : mSamplerTextures)
    {
        TextureBindingVector &textureVector = bindingVec.second;
        for (size_t textureIdx = 0; textureIdx < textureVector.size(); textureIdx++)
        {
            textureVector[textureIdx].set(context, nullptr);
        }
    }
    for (size_t samplerIdx = 0; samplerIdx < mSamplers.size(); samplerIdx++)
    {
        mSamplers[samplerIdx].set(context, nullptr);
    }

    for (auto &imageUnit : mImageUnits)
    {
        imageUnit.texture.set(context, nullptr);
        imageUnit.level   = 0;
        imageUnit.layered = false;
        imageUnit.layer   = 0;
        imageUnit.access  = GL_READ_ONLY;
        imageUnit.format  = GL_R32UI;
    }

    mRenderbuffer.set(context, nullptr);

    for (auto type : angle::AllEnums<BufferBinding>())
    {
        mBoundBuffers[type].set(context, nullptr);
    }

    if (mProgram)
    {
        mProgram->release(context);
    }
    mProgram = nullptr;

    mProgramPipeline.set(context, nullptr);

    mTransformFeedback.set(context, nullptr);

    for (State::ActiveQueryMap::iterator i = mActiveQueries.begin(); i != mActiveQueries.end(); i++)
    {
        i->second.set(context, nullptr);
    }

    for (auto &buf : mUniformBuffers)
    {
        buf.set(context, nullptr);
    }

    for (auto &buf : mAtomicCounterBuffers)
    {
        buf.set(context, nullptr);
    }

    for (auto &buf : mShaderStorageBuffers)
    {
        buf.set(context, nullptr);
    }

    angle::Matrix<GLfloat>::setToIdentity(mPathMatrixProj);
    angle::Matrix<GLfloat>::setToIdentity(mPathMatrixMV);
    mPathStencilFunc = GL_ALWAYS;
    mPathStencilRef  = 0;
    mPathStencilMask = std::numeric_limits<GLuint>::max();

    // TODO(jmadill): Is this necessary?
    setAllDirtyBits();
}

const RasterizerState &State::getRasterizerState() const
{
    return mRasterizer;
}

const BlendState &State::getBlendState() const
{
    return mBlend;
}

const DepthStencilState &State::getDepthStencilState() const
{
    return mDepthStencil;
}

void State::setColorClearValue(float red, float green, float blue, float alpha)
{
    mColorClearValue.red = red;
    mColorClearValue.green = green;
    mColorClearValue.blue = blue;
    mColorClearValue.alpha = alpha;
    mDirtyBits.set(DIRTY_BIT_CLEAR_COLOR);
}

void State::setDepthClearValue(float depth)
{
    mDepthClearValue = depth;
    mDirtyBits.set(DIRTY_BIT_CLEAR_DEPTH);
}

void State::setStencilClearValue(int stencil)
{
    mStencilClearValue = stencil;
    mDirtyBits.set(DIRTY_BIT_CLEAR_STENCIL);
}

void State::setColorMask(bool red, bool green, bool blue, bool alpha)
{
    mBlend.colorMaskRed = red;
    mBlend.colorMaskGreen = green;
    mBlend.colorMaskBlue = blue;
    mBlend.colorMaskAlpha = alpha;
    mDirtyBits.set(DIRTY_BIT_COLOR_MASK);
}

void State::setDepthMask(bool mask)
{
    mDepthStencil.depthMask = mask;
    mDirtyBits.set(DIRTY_BIT_DEPTH_MASK);
}

bool State::isRasterizerDiscardEnabled() const
{
    return mRasterizer.rasterizerDiscard;
}

void State::setRasterizerDiscard(bool enabled)
{
    mRasterizer.rasterizerDiscard = enabled;
    mDirtyBits.set(DIRTY_BIT_RASTERIZER_DISCARD_ENABLED);
}

bool State::isCullFaceEnabled() const
{
    return mRasterizer.cullFace;
}

void State::setCullFace(bool enabled)
{
    mRasterizer.cullFace = enabled;
    mDirtyBits.set(DIRTY_BIT_CULL_FACE_ENABLED);
}

void State::setCullMode(CullFaceMode mode)
{
    mRasterizer.cullMode = mode;
    mDirtyBits.set(DIRTY_BIT_CULL_FACE);
}

void State::setFrontFace(GLenum front)
{
    mRasterizer.frontFace = front;
    mDirtyBits.set(DIRTY_BIT_FRONT_FACE);
}

bool State::isDepthTestEnabled() const
{
    return mDepthStencil.depthTest;
}

void State::setDepthTest(bool enabled)
{
    mDepthStencil.depthTest = enabled;
    mDirtyBits.set(DIRTY_BIT_DEPTH_TEST_ENABLED);
}

void State::setDepthFunc(GLenum depthFunc)
{
     mDepthStencil.depthFunc = depthFunc;
     mDirtyBits.set(DIRTY_BIT_DEPTH_FUNC);
}

void State::setDepthRange(float zNear, float zFar)
{
    mNearZ = zNear;
    mFarZ = zFar;
    mDirtyBits.set(DIRTY_BIT_DEPTH_RANGE);
}

float State::getNearPlane() const
{
    return mNearZ;
}

float State::getFarPlane() const
{
    return mFarZ;
}

bool State::isBlendEnabled() const
{
    return mBlend.blend;
}

void State::setBlend(bool enabled)
{
    mBlend.blend = enabled;
    mDirtyBits.set(DIRTY_BIT_BLEND_ENABLED);
}

void State::setBlendFactors(GLenum sourceRGB, GLenum destRGB, GLenum sourceAlpha, GLenum destAlpha)
{
    mBlend.sourceBlendRGB = sourceRGB;
    mBlend.destBlendRGB = destRGB;
    mBlend.sourceBlendAlpha = sourceAlpha;
    mBlend.destBlendAlpha = destAlpha;
    mDirtyBits.set(DIRTY_BIT_BLEND_FUNCS);
}

void State::setBlendColor(float red, float green, float blue, float alpha)
{
    mBlendColor.red = red;
    mBlendColor.green = green;
    mBlendColor.blue = blue;
    mBlendColor.alpha = alpha;
    mDirtyBits.set(DIRTY_BIT_BLEND_COLOR);
}

void State::setBlendEquation(GLenum rgbEquation, GLenum alphaEquation)
{
    mBlend.blendEquationRGB = rgbEquation;
    mBlend.blendEquationAlpha = alphaEquation;
    mDirtyBits.set(DIRTY_BIT_BLEND_EQUATIONS);
}

const ColorF &State::getBlendColor() const
{
    return mBlendColor;
}

bool State::isStencilTestEnabled() const
{
    return mDepthStencil.stencilTest;
}

void State::setStencilTest(bool enabled)
{
    mDepthStencil.stencilTest = enabled;
    mDirtyBits.set(DIRTY_BIT_STENCIL_TEST_ENABLED);
}

void State::setStencilParams(GLenum stencilFunc, GLint stencilRef, GLuint stencilMask)
{
    mDepthStencil.stencilFunc = stencilFunc;
    mStencilRef = (stencilRef > 0) ? stencilRef : 0;
    mDepthStencil.stencilMask = stencilMask;
    mDirtyBits.set(DIRTY_BIT_STENCIL_FUNCS_FRONT);
}

void State::setStencilBackParams(GLenum stencilBackFunc, GLint stencilBackRef, GLuint stencilBackMask)
{
    mDepthStencil.stencilBackFunc = stencilBackFunc;
    mStencilBackRef = (stencilBackRef > 0) ? stencilBackRef : 0;
    mDepthStencil.stencilBackMask = stencilBackMask;
    mDirtyBits.set(DIRTY_BIT_STENCIL_FUNCS_BACK);
}

void State::setStencilWritemask(GLuint stencilWritemask)
{
    mDepthStencil.stencilWritemask = stencilWritemask;
    mDirtyBits.set(DIRTY_BIT_STENCIL_WRITEMASK_FRONT);
}

void State::setStencilBackWritemask(GLuint stencilBackWritemask)
{
    mDepthStencil.stencilBackWritemask = stencilBackWritemask;
    mDirtyBits.set(DIRTY_BIT_STENCIL_WRITEMASK_BACK);
}

void State::setStencilOperations(GLenum stencilFail, GLenum stencilPassDepthFail, GLenum stencilPassDepthPass)
{
    mDepthStencil.stencilFail = stencilFail;
    mDepthStencil.stencilPassDepthFail = stencilPassDepthFail;
    mDepthStencil.stencilPassDepthPass = stencilPassDepthPass;
    mDirtyBits.set(DIRTY_BIT_STENCIL_OPS_FRONT);
}

void State::setStencilBackOperations(GLenum stencilBackFail, GLenum stencilBackPassDepthFail, GLenum stencilBackPassDepthPass)
{
    mDepthStencil.stencilBackFail = stencilBackFail;
    mDepthStencil.stencilBackPassDepthFail = stencilBackPassDepthFail;
    mDepthStencil.stencilBackPassDepthPass = stencilBackPassDepthPass;
    mDirtyBits.set(DIRTY_BIT_STENCIL_OPS_BACK);
}

GLint State::getStencilRef() const
{
    return mStencilRef;
}

GLint State::getStencilBackRef() const
{
    return mStencilBackRef;
}

bool State::isPolygonOffsetFillEnabled() const
{
    return mRasterizer.polygonOffsetFill;
}

void State::setPolygonOffsetFill(bool enabled)
{
    mRasterizer.polygonOffsetFill = enabled;
    mDirtyBits.set(DIRTY_BIT_POLYGON_OFFSET_FILL_ENABLED);
}

void State::setPolygonOffsetParams(GLfloat factor, GLfloat units)
{
    // An application can pass NaN values here, so handle this gracefully
    mRasterizer.polygonOffsetFactor = factor != factor ? 0.0f : factor;
    mRasterizer.polygonOffsetUnits = units != units ? 0.0f : units;
    mDirtyBits.set(DIRTY_BIT_POLYGON_OFFSET);
}

bool State::isSampleAlphaToCoverageEnabled() const
{
    return mBlend.sampleAlphaToCoverage;
}

void State::setSampleAlphaToCoverage(bool enabled)
{
    mBlend.sampleAlphaToCoverage = enabled;
    mDirtyBits.set(DIRTY_BIT_SAMPLE_ALPHA_TO_COVERAGE_ENABLED);
}

bool State::isSampleCoverageEnabled() const
{
    return mSampleCoverage;
}

void State::setSampleCoverage(bool enabled)
{
    mSampleCoverage = enabled;
    mDirtyBits.set(DIRTY_BIT_SAMPLE_COVERAGE_ENABLED);
}

void State::setSampleCoverageParams(GLclampf value, bool invert)
{
    mSampleCoverageValue = value;
    mSampleCoverageInvert = invert;
    mDirtyBits.set(DIRTY_BIT_SAMPLE_COVERAGE);
}

GLclampf State::getSampleCoverageValue() const
{
    return mSampleCoverageValue;
}

bool State::getSampleCoverageInvert() const
{
    return mSampleCoverageInvert;
}

bool State::isSampleMaskEnabled() const
{
    return mSampleMask;
}

void State::setSampleMaskEnabled(bool enabled)
{
    mSampleMask = enabled;
    mDirtyBits.set(DIRTY_BIT_SAMPLE_MASK_ENABLED);
}

void State::setSampleMaskParams(GLuint maskNumber, GLbitfield mask)
{
    ASSERT(maskNumber < mMaxSampleMaskWords);
    mSampleMaskValues[maskNumber] = mask;
    // TODO(jmadill): Use a child dirty bit if we ever use more than two words.
    mDirtyBits.set(DIRTY_BIT_SAMPLE_MASK);
}

GLbitfield State::getSampleMaskWord(GLuint maskNumber) const
{
    ASSERT(maskNumber < mMaxSampleMaskWords);
    return mSampleMaskValues[maskNumber];
}

GLuint State::getMaxSampleMaskWords() const
{
    return mMaxSampleMaskWords;
}

void State::setSampleAlphaToOne(bool enabled)
{
    mSampleAlphaToOne = enabled;
    mDirtyBits.set(DIRTY_BIT_SAMPLE_ALPHA_TO_ONE);
}

bool State::isSampleAlphaToOneEnabled() const
{
    return mSampleAlphaToOne;
}

void State::setMultisampling(bool enabled)
{
    mMultiSampling = enabled;
    mDirtyBits.set(DIRTY_BIT_MULTISAMPLING);
}

bool State::isMultisamplingEnabled() const
{
    return mMultiSampling;
}

bool State::isScissorTestEnabled() const
{
    return mScissorTest;
}

void State::setScissorTest(bool enabled)
{
    mScissorTest = enabled;
    mDirtyBits.set(DIRTY_BIT_SCISSOR_TEST_ENABLED);
}

void State::setScissorParams(GLint x, GLint y, GLsizei width, GLsizei height)
{
    mScissor.x = x;
    mScissor.y = y;
    mScissor.width = width;
    mScissor.height = height;
    mDirtyBits.set(DIRTY_BIT_SCISSOR);
}

const Rectangle &State::getScissor() const
{
    return mScissor;
}

bool State::isDitherEnabled() const
{
    return mBlend.dither;
}

void State::setDither(bool enabled)
{
    mBlend.dither = enabled;
    mDirtyBits.set(DIRTY_BIT_DITHER_ENABLED);
}

bool State::isPrimitiveRestartEnabled() const
{
    return mPrimitiveRestart;
}

void State::setPrimitiveRestart(bool enabled)
{
    mPrimitiveRestart = enabled;
    mDirtyBits.set(DIRTY_BIT_PRIMITIVE_RESTART_ENABLED);
}

void State::setEnableFeature(GLenum feature, bool enabled)
{
    switch (feature)
    {
      case GL_MULTISAMPLE_EXT:               setMultisampling(enabled);         break;
      case GL_SAMPLE_ALPHA_TO_ONE_EXT:       setSampleAlphaToOne(enabled);      break;
      case GL_CULL_FACE:                     setCullFace(enabled);              break;
      case GL_POLYGON_OFFSET_FILL:           setPolygonOffsetFill(enabled);     break;
      case GL_SAMPLE_ALPHA_TO_COVERAGE:      setSampleAlphaToCoverage(enabled); break;
      case GL_SAMPLE_COVERAGE:               setSampleCoverage(enabled);        break;
      case GL_SCISSOR_TEST:                  setScissorTest(enabled);           break;
      case GL_STENCIL_TEST:                  setStencilTest(enabled);           break;
      case GL_DEPTH_TEST:                    setDepthTest(enabled);             break;
      case GL_BLEND:                         setBlend(enabled);                 break;
      case GL_DITHER:                        setDither(enabled);                break;
      case GL_PRIMITIVE_RESTART_FIXED_INDEX: setPrimitiveRestart(enabled);      break;
      case GL_RASTERIZER_DISCARD:            setRasterizerDiscard(enabled);     break;
      case GL_SAMPLE_MASK:
          setSampleMaskEnabled(enabled);
          break;
      case GL_DEBUG_OUTPUT_SYNCHRONOUS:
          mDebug.setOutputSynchronous(enabled);
          break;
      case GL_DEBUG_OUTPUT:
          mDebug.setOutputEnabled(enabled);
          break;
      case GL_FRAMEBUFFER_SRGB_EXT:
          setFramebufferSRGB(enabled);
          break;
      default:                               UNREACHABLE();
    }
}

bool State::getEnableFeature(GLenum feature) const
{
    switch (feature)
    {
      case GL_MULTISAMPLE_EXT:               return isMultisamplingEnabled();
      case GL_SAMPLE_ALPHA_TO_ONE_EXT:       return isSampleAlphaToOneEnabled();
      case GL_CULL_FACE:                     return isCullFaceEnabled();
      case GL_POLYGON_OFFSET_FILL:           return isPolygonOffsetFillEnabled();
      case GL_SAMPLE_ALPHA_TO_COVERAGE:      return isSampleAlphaToCoverageEnabled();
      case GL_SAMPLE_COVERAGE:               return isSampleCoverageEnabled();
      case GL_SCISSOR_TEST:                  return isScissorTestEnabled();
      case GL_STENCIL_TEST:                  return isStencilTestEnabled();
      case GL_DEPTH_TEST:                    return isDepthTestEnabled();
      case GL_BLEND:                         return isBlendEnabled();
      case GL_DITHER:                        return isDitherEnabled();
      case GL_PRIMITIVE_RESTART_FIXED_INDEX: return isPrimitiveRestartEnabled();
      case GL_RASTERIZER_DISCARD:            return isRasterizerDiscardEnabled();
      case GL_SAMPLE_MASK:
          return isSampleMaskEnabled();
      case GL_DEBUG_OUTPUT_SYNCHRONOUS:
          return mDebug.isOutputSynchronous();
      case GL_DEBUG_OUTPUT:
          return mDebug.isOutputEnabled();
      case GL_BIND_GENERATES_RESOURCE_CHROMIUM:
          return isBindGeneratesResourceEnabled();
      case GL_CLIENT_ARRAYS_ANGLE:
          return areClientArraysEnabled();
      case GL_FRAMEBUFFER_SRGB_EXT:
          return getFramebufferSRGB();
      case GL_ROBUST_RESOURCE_INITIALIZATION_ANGLE:
          return mRobustResourceInit;
      case GL_PROGRAM_CACHE_ENABLED_ANGLE:
          return mProgramBinaryCacheEnabled;

      default:
          UNREACHABLE();
          return false;
    }
}

void State::setLineWidth(GLfloat width)
{
    mLineWidth = width;
    mDirtyBits.set(DIRTY_BIT_LINE_WIDTH);
}

float State::getLineWidth() const
{
    return mLineWidth;
}

void State::setGenerateMipmapHint(GLenum hint)
{
    mGenerateMipmapHint = hint;
    mDirtyBits.set(DIRTY_BIT_GENERATE_MIPMAP_HINT);
}

void State::setFragmentShaderDerivativeHint(GLenum hint)
{
    mFragmentShaderDerivativeHint = hint;
    mDirtyBits.set(DIRTY_BIT_SHADER_DERIVATIVE_HINT);
    // TODO: Propagate the hint to shader translator so we can write
    // ddx, ddx_coarse, or ddx_fine depending on the hint.
    // Ignore for now. It is valid for implementations to ignore hint.
}

bool State::isBindGeneratesResourceEnabled() const
{
    return mBindGeneratesResource;
}

bool State::areClientArraysEnabled() const
{
    return mClientArraysEnabled;
}

void State::setViewportParams(GLint x, GLint y, GLsizei width, GLsizei height)
{
    mViewport.x = x;
    mViewport.y = y;
    mViewport.width = width;
    mViewport.height = height;
    mDirtyBits.set(DIRTY_BIT_VIEWPORT);
}

const Rectangle &State::getViewport() const
{
    return mViewport;
}

void State::setActiveSampler(unsigned int active)
{
    mActiveSampler = active;
}

unsigned int State::getActiveSampler() const
{
    return static_cast<unsigned int>(mActiveSampler);
}

void State::setSamplerTexture(const Context *context, GLenum type, Texture *texture)
{
    mSamplerTextures[type][mActiveSampler].set(context, texture);
    mDirtyBits.set(DIRTY_BIT_TEXTURE_BINDINGS);
    mDirtyObjects.set(DIRTY_OBJECT_PROGRAM_TEXTURES);
}

Texture *State::getTargetTexture(GLenum target) const
{
    return getSamplerTexture(static_cast<unsigned int>(mActiveSampler), target);
}

Texture *State::getSamplerTexture(unsigned int sampler, GLenum type) const
{
    const auto it = mSamplerTextures.find(type);
    ASSERT(it != mSamplerTextures.end());
    ASSERT(sampler < it->second.size());
    return it->second[sampler].get();
}

GLuint State::getSamplerTextureId(unsigned int sampler, GLenum type) const
{
    const auto it = mSamplerTextures.find(type);
    ASSERT(it != mSamplerTextures.end());
    ASSERT(sampler < it->second.size());
    return it->second[sampler].id();
}

void State::detachTexture(const Context *context, const TextureMap &zeroTextures, GLuint texture)
{
    // Textures have a detach method on State rather than a simple
    // removeBinding, because the zero/null texture objects are managed
    // separately, and don't have to go through the Context's maps or
    // the ResourceManager.

    // [OpenGL ES 2.0.24] section 3.8 page 84:
    // If a texture object is deleted, it is as if all texture units which are bound to that texture object are
    // rebound to texture object zero

    for (auto &bindingVec : mSamplerTextures)
    {
        GLenum textureType = bindingVec.first;
        TextureBindingVector &textureVector = bindingVec.second;
        for (BindingPointer<Texture> &binding : textureVector)
        {
            if (binding.id() == texture)
            {
                auto it = zeroTextures.find(textureType);
                ASSERT(it != zeroTextures.end());
                // Zero textures are the "default" textures instead of NULL
                binding.set(context, it->second.get());
                mDirtyBits.set(DIRTY_BIT_TEXTURE_BINDINGS);
            }
        }
    }

    for (auto &bindingImageUnit : mImageUnits)
    {
        if (bindingImageUnit.texture.id() == texture)
        {
            bindingImageUnit.texture.set(context, nullptr);
            bindingImageUnit.level   = 0;
            bindingImageUnit.layered = false;
            bindingImageUnit.layer   = 0;
            bindingImageUnit.access  = GL_READ_ONLY;
            bindingImageUnit.format  = GL_R32UI;
            break;
        }
    }

    // [OpenGL ES 2.0.24] section 4.4 page 112:
    // If a texture object is deleted while its image is attached to the currently bound framebuffer, then it is
    // as if Texture2DAttachment had been called, with a texture of 0, for each attachment point to which this
    // image was attached in the currently bound framebuffer.

    if (mReadFramebuffer && mReadFramebuffer->detachTexture(context, texture))
    {
        mDirtyObjects.set(DIRTY_OBJECT_READ_FRAMEBUFFER);
    }

    if (mDrawFramebuffer && mDrawFramebuffer->detachTexture(context, texture))
    {
        mDirtyObjects.set(DIRTY_OBJECT_DRAW_FRAMEBUFFER);
    }
}

void State::initializeZeroTextures(const Context *context, const TextureMap &zeroTextures)
{
    for (const auto &zeroTexture : zeroTextures)
    {
        auto &samplerTextureArray = mSamplerTextures[zeroTexture.first];

        for (size_t textureUnit = 0; textureUnit < samplerTextureArray.size(); ++textureUnit)
        {
            samplerTextureArray[textureUnit].set(context, zeroTexture.second.get());
        }
    }
}

void State::setSamplerBinding(const Context *context, GLuint textureUnit, Sampler *sampler)
{
    mSamplers[textureUnit].set(context, sampler);
    mDirtyBits.set(DIRTY_BIT_SAMPLER_BINDINGS);
    mDirtyObjects.set(DIRTY_OBJECT_PROGRAM_TEXTURES);
}

GLuint State::getSamplerId(GLuint textureUnit) const
{
    ASSERT(textureUnit < mSamplers.size());
    return mSamplers[textureUnit].id();
}

Sampler *State::getSampler(GLuint textureUnit) const
{
    return mSamplers[textureUnit].get();
}

void State::detachSampler(const Context *context, GLuint sampler)
{
    // [OpenGL ES 3.0.2] section 3.8.2 pages 123-124:
    // If a sampler object that is currently bound to one or more texture units is
    // deleted, it is as though BindSampler is called once for each texture unit to
    // which the sampler is bound, with unit set to the texture unit and sampler set to zero.
    for (BindingPointer<Sampler> &samplerBinding : mSamplers)
    {
        if (samplerBinding.id() == sampler)
        {
            samplerBinding.set(context, nullptr);
            mDirtyBits.set(DIRTY_BIT_SAMPLER_BINDINGS);
        }
    }
}

void State::setRenderbufferBinding(const Context *context, Renderbuffer *renderbuffer)
{
    mRenderbuffer.set(context, renderbuffer);
    mDirtyBits.set(DIRTY_BIT_RENDERBUFFER_BINDING);
}

GLuint State::getRenderbufferId() const
{
    return mRenderbuffer.id();
}

Renderbuffer *State::getCurrentRenderbuffer() const
{
    return mRenderbuffer.get();
}

void State::detachRenderbuffer(const Context *context, GLuint renderbuffer)
{
    // [OpenGL ES 2.0.24] section 4.4 page 109:
    // If a renderbuffer that is currently bound to RENDERBUFFER is deleted, it is as though BindRenderbuffer
    // had been executed with the target RENDERBUFFER and name of zero.

    if (mRenderbuffer.id() == renderbuffer)
    {
        setRenderbufferBinding(context, nullptr);
    }

    // [OpenGL ES 2.0.24] section 4.4 page 111:
    // If a renderbuffer object is deleted while its image is attached to the currently bound framebuffer,
    // then it is as if FramebufferRenderbuffer had been called, with a renderbuffer of 0, for each attachment
    // point to which this image was attached in the currently bound framebuffer.

    Framebuffer *readFramebuffer = mReadFramebuffer;
    Framebuffer *drawFramebuffer = mDrawFramebuffer;

    if (readFramebuffer && readFramebuffer->detachRenderbuffer(context, renderbuffer))
    {
        mDirtyObjects.set(DIRTY_OBJECT_READ_FRAMEBUFFER);
    }

    if (drawFramebuffer && drawFramebuffer != readFramebuffer)
    {
        if (drawFramebuffer->detachRenderbuffer(context, renderbuffer))
        {
            mDirtyObjects.set(DIRTY_OBJECT_DRAW_FRAMEBUFFER);
        }
    }

}

void State::setReadFramebufferBinding(Framebuffer *framebuffer)
{
    if (mReadFramebuffer == framebuffer)
        return;

    mReadFramebuffer = framebuffer;
    mDirtyBits.set(DIRTY_BIT_READ_FRAMEBUFFER_BINDING);

    if (mReadFramebuffer && mReadFramebuffer->hasAnyDirtyBit())
    {
        mDirtyObjects.set(DIRTY_OBJECT_READ_FRAMEBUFFER);
    }
}

void State::setDrawFramebufferBinding(Framebuffer *framebuffer)
{
    if (mDrawFramebuffer == framebuffer)
        return;

    mDrawFramebuffer = framebuffer;
    mDirtyBits.set(DIRTY_BIT_DRAW_FRAMEBUFFER_BINDING);

    if (mDrawFramebuffer && mDrawFramebuffer->hasAnyDirtyBit())
    {
        mDirtyObjects.set(DIRTY_OBJECT_DRAW_FRAMEBUFFER);
    }
}

Framebuffer *State::getTargetFramebuffer(GLenum target) const
{
    switch (target)
    {
        case GL_READ_FRAMEBUFFER_ANGLE:
            return mReadFramebuffer;
        case GL_DRAW_FRAMEBUFFER_ANGLE:
        case GL_FRAMEBUFFER:
            return mDrawFramebuffer;
        default:
            UNREACHABLE();
            return nullptr;
    }
}

Framebuffer *State::getReadFramebuffer() const
{
    return mReadFramebuffer;
}

Framebuffer *State::getDrawFramebuffer() const
{
    return mDrawFramebuffer;
}

bool State::removeReadFramebufferBinding(GLuint framebuffer)
{
    if (mReadFramebuffer != nullptr &&
        mReadFramebuffer->id() == framebuffer)
    {
        setReadFramebufferBinding(nullptr);
        return true;
    }

    return false;
}

bool State::removeDrawFramebufferBinding(GLuint framebuffer)
{
    if (mReadFramebuffer != nullptr &&
        mDrawFramebuffer->id() == framebuffer)
    {
        setDrawFramebufferBinding(nullptr);
        return true;
    }

    return false;
}

void State::setVertexArrayBinding(VertexArray *vertexArray)
{
    mVertexArray = vertexArray;
    mDirtyBits.set(DIRTY_BIT_VERTEX_ARRAY_BINDING);

    if (mVertexArray && mVertexArray->hasAnyDirtyBit())
    {
        mDirtyObjects.set(DIRTY_OBJECT_VERTEX_ARRAY);
    }
}

GLuint State::getVertexArrayId() const
{
    ASSERT(mVertexArray != nullptr);
    return mVertexArray->id();
}

VertexArray *State::getVertexArray() const
{
    ASSERT(mVertexArray != nullptr);
    return mVertexArray;
}

bool State::removeVertexArrayBinding(GLuint vertexArray)
{
    if (mVertexArray->id() == vertexArray)
    {
        mVertexArray = nullptr;
        mDirtyBits.set(DIRTY_BIT_VERTEX_ARRAY_BINDING);
        mDirtyObjects.set(DIRTY_OBJECT_VERTEX_ARRAY);
        return true;
    }

    return false;
}

void State::bindVertexBuffer(const Context *context,
                             GLuint bindingIndex,
                             Buffer *boundBuffer,
                             GLintptr offset,
                             GLsizei stride)
{
    getVertexArray()->bindVertexBuffer(context, bindingIndex, boundBuffer, offset, stride);
    mDirtyObjects.set(DIRTY_OBJECT_VERTEX_ARRAY);
}

void State::setVertexAttribBinding(const Context *context, GLuint attribIndex, GLuint bindingIndex)
{
    getVertexArray()->setVertexAttribBinding(context, attribIndex, bindingIndex);
    mDirtyObjects.set(DIRTY_OBJECT_VERTEX_ARRAY);
}

void State::setVertexAttribFormat(GLuint attribIndex,
                                  GLint size,
                                  GLenum type,
                                  bool normalized,
                                  bool pureInteger,
                                  GLuint relativeOffset)
{
    getVertexArray()->setVertexAttribFormat(attribIndex, size, type, normalized, pureInteger,
                                            relativeOffset);
    mDirtyObjects.set(DIRTY_OBJECT_VERTEX_ARRAY);
}

void State::setVertexBindingDivisor(GLuint bindingIndex, GLuint divisor)
{
    getVertexArray()->setVertexBindingDivisor(bindingIndex, divisor);
    mDirtyObjects.set(DIRTY_OBJECT_VERTEX_ARRAY);
}

void State::setProgram(const Context *context, Program *newProgram)
{
    if (mProgram != newProgram)
    {
        if (mProgram)
        {
            mProgram->release(context);
        }

        mProgram = newProgram;

        if (mProgram)
        {
            newProgram->addRef();
            mDirtyObjects.set(DIRTY_OBJECT_PROGRAM_TEXTURES);
        }
        mDirtyBits.set(DIRTY_BIT_PROGRAM_EXECUTABLE);
        mDirtyBits.set(DIRTY_BIT_PROGRAM_BINDING);
    }
}

Program *State::getProgram() const
{
    return mProgram;
}

void State::setTransformFeedbackBinding(const Context *context,
                                        TransformFeedback *transformFeedback)
{
    mTransformFeedback.set(context, transformFeedback);
}

TransformFeedback *State::getCurrentTransformFeedback() const
{
    return mTransformFeedback.get();
}

bool State::isTransformFeedbackActiveUnpaused() const
{
    TransformFeedback *curTransformFeedback = getCurrentTransformFeedback();
    return curTransformFeedback && curTransformFeedback->isActive() && !curTransformFeedback->isPaused();
}

bool State::removeTransformFeedbackBinding(const Context *context, GLuint transformFeedback)
{
    if (mTransformFeedback.id() == transformFeedback)
    {
        mTransformFeedback.set(context, nullptr);
        return true;
    }

    return false;
}

void State::setProgramPipelineBinding(const Context *context, ProgramPipeline *pipeline)
{
    mProgramPipeline.set(context, pipeline);
}

void State::detachProgramPipeline(const Context *context, GLuint pipeline)
{
    mProgramPipeline.set(context, nullptr);
}

bool State::isQueryActive(const GLenum type) const
{
    for (auto &iter : mActiveQueries)
    {
        const Query *query = iter.second.get();
        if (query != nullptr && ActiveQueryType(query->getType()) == ActiveQueryType(type))
        {
            return true;
        }
    }

    return false;
}

bool State::isQueryActive(Query *query) const
{
    for (auto &iter : mActiveQueries)
    {
        if (iter.second.get() == query)
        {
            return true;
        }
    }

    return false;
}

void State::setActiveQuery(const Context *context, GLenum target, Query *query)
{
    mActiveQueries[target].set(context, query);
}

GLuint State::getActiveQueryId(GLenum target) const
{
    const Query *query = getActiveQuery(target);
    return (query ? query->id() : 0u);
}

Query *State::getActiveQuery(GLenum target) const
{
    const auto it = mActiveQueries.find(target);

    // All query types should already exist in the activeQueries map
    ASSERT(it != mActiveQueries.end());

    return it->second.get();
}

void State::setBufferBinding(const Context *context, BufferBinding target, Buffer *buffer)
{
    switch (target)
    {
        case BufferBinding::PixelPack:
            mBoundBuffers[target].set(context, buffer);
            mDirtyBits.set(DIRTY_BIT_PACK_BUFFER_BINDING);
            break;
        case BufferBinding::PixelUnpack:
            mBoundBuffers[target].set(context, buffer);
            mDirtyBits.set(DIRTY_BIT_UNPACK_BUFFER_BINDING);
            break;
        case BufferBinding::DrawIndirect:
            mBoundBuffers[target].set(context, buffer);
            mDirtyBits.set(DIRTY_BIT_DRAW_INDIRECT_BUFFER_BINDING);
            break;
        case BufferBinding::TransformFeedback:
            if (mTransformFeedback.get() != nullptr)
            {
                mTransformFeedback->bindGenericBuffer(context, buffer);
            }
            break;
        case BufferBinding::ElementArray:
            getVertexArray()->setElementArrayBuffer(context, buffer);
            mDirtyObjects.set(DIRTY_OBJECT_VERTEX_ARRAY);
            break;
        default:
            mBoundBuffers[target].set(context, buffer);
            break;
    }
}
void State::setIndexedBufferBinding(const Context *context,
                                    BufferBinding target,
                                    GLuint index,
                                    Buffer *buffer,
                                    GLintptr offset,
                                    GLsizeiptr size)
{
    setBufferBinding(context, target, buffer);

    switch (target)
    {
        case BufferBinding::TransformFeedback:
            mTransformFeedback->bindIndexedBuffer(context, index, buffer, offset, size);
            break;
        case BufferBinding::Uniform:
            mUniformBuffers[index].set(context, buffer, offset, size);
            break;
        case BufferBinding::AtomicCounter:
            mAtomicCounterBuffers[index].set(context, buffer, offset, size);
            break;
        case BufferBinding::ShaderStorage:
            mShaderStorageBuffers[index].set(context, buffer, offset, size);
            break;
        default:
            UNREACHABLE();
            break;
    }
}

const OffsetBindingPointer<Buffer> &State::getIndexedUniformBuffer(size_t index) const
{
    ASSERT(static_cast<size_t>(index) < mUniformBuffers.size());
    return mUniformBuffers[index];
}

const OffsetBindingPointer<Buffer> &State::getIndexedAtomicCounterBuffer(size_t index) const
{
    ASSERT(static_cast<size_t>(index) < mAtomicCounterBuffers.size());
    return mAtomicCounterBuffers[index];
}

const OffsetBindingPointer<Buffer> &State::getIndexedShaderStorageBuffer(size_t index) const
{
    ASSERT(static_cast<size_t>(index) < mShaderStorageBuffers.size());
    return mShaderStorageBuffers[index];
}

Buffer *State::getTargetBuffer(BufferBinding target) const
{
    switch (target)
    {
        case BufferBinding::ElementArray:
            return getVertexArray()->getElementArrayBuffer().get();
        case BufferBinding::TransformFeedback:
            return mTransformFeedback->getGenericBuffer().get();
        default:
            return mBoundBuffers[target].get();
    }
}

void State::detachBuffer(const Context *context, GLuint bufferName)
{
    for (auto &buffer : mBoundBuffers)
    {
        if (buffer.id() == bufferName)
        {
            buffer.set(context, nullptr);
        }
    }

    TransformFeedback *curTransformFeedback = getCurrentTransformFeedback();
    if (curTransformFeedback)
    {
        curTransformFeedback->detachBuffer(context, bufferName);
    }

    getVertexArray()->detachBuffer(context, bufferName);
}

void State::setEnableVertexAttribArray(unsigned int attribNum, bool enabled)
{
    getVertexArray()->enableAttribute(attribNum, enabled);
    mDirtyObjects.set(DIRTY_OBJECT_VERTEX_ARRAY);
}

void State::setVertexAttribf(GLuint index, const GLfloat values[4])
{
    ASSERT(static_cast<size_t>(index) < mVertexAttribCurrentValues.size());
    mVertexAttribCurrentValues[index].setFloatValues(values);
    mDirtyBits.set(DIRTY_BIT_CURRENT_VALUES);
    mDirtyCurrentValues.set(index);
}

void State::setVertexAttribu(GLuint index, const GLuint values[4])
{
    ASSERT(static_cast<size_t>(index) < mVertexAttribCurrentValues.size());
    mVertexAttribCurrentValues[index].setUnsignedIntValues(values);
    mDirtyBits.set(DIRTY_BIT_CURRENT_VALUES);
    mDirtyCurrentValues.set(index);
}

void State::setVertexAttribi(GLuint index, const GLint values[4])
{
    ASSERT(static_cast<size_t>(index) < mVertexAttribCurrentValues.size());
    mVertexAttribCurrentValues[index].setIntValues(values);
    mDirtyBits.set(DIRTY_BIT_CURRENT_VALUES);
    mDirtyCurrentValues.set(index);
}

void State::setVertexAttribPointer(const Context *context,
                                   unsigned int attribNum,
                                   Buffer *boundBuffer,
                                   GLint size,
                                   GLenum type,
                                   bool normalized,
                                   bool pureInteger,
                                   GLsizei stride,
                                   const void *pointer)
{
    getVertexArray()->setVertexAttribPointer(context, attribNum, boundBuffer, size, type,
                                             normalized, pureInteger, stride, pointer);
    mDirtyObjects.set(DIRTY_OBJECT_VERTEX_ARRAY);
}

void State::setVertexAttribDivisor(const Context *context, GLuint index, GLuint divisor)
{
    getVertexArray()->setVertexAttribDivisor(context, index, divisor);
    mDirtyObjects.set(DIRTY_OBJECT_VERTEX_ARRAY);
}

const VertexAttribCurrentValueData &State::getVertexAttribCurrentValue(size_t attribNum) const
{
    ASSERT(attribNum < mVertexAttribCurrentValues.size());
    return mVertexAttribCurrentValues[attribNum];
}

const std::vector<VertexAttribCurrentValueData> &State::getVertexAttribCurrentValues() const
{
    return mVertexAttribCurrentValues;
}

const void *State::getVertexAttribPointer(unsigned int attribNum) const
{
    return getVertexArray()->getVertexAttribute(attribNum).pointer;
}

void State::setPackAlignment(GLint alignment)
{
    mPack.alignment = alignment;
    mDirtyBits.set(DIRTY_BIT_PACK_STATE);
}

GLint State::getPackAlignment() const
{
    return mPack.alignment;
}

void State::setPackReverseRowOrder(bool reverseRowOrder)
{
    mPack.reverseRowOrder = reverseRowOrder;
    mDirtyBits.set(DIRTY_BIT_PACK_STATE);
}

bool State::getPackReverseRowOrder() const
{
    return mPack.reverseRowOrder;
}

void State::setPackRowLength(GLint rowLength)
{
    mPack.rowLength = rowLength;
    mDirtyBits.set(DIRTY_BIT_PACK_STATE);
}

GLint State::getPackRowLength() const
{
    return mPack.rowLength;
}

void State::setPackSkipRows(GLint skipRows)
{
    mPack.skipRows = skipRows;
    mDirtyBits.set(DIRTY_BIT_PACK_STATE);
}

GLint State::getPackSkipRows() const
{
    return mPack.skipRows;
}

void State::setPackSkipPixels(GLint skipPixels)
{
    mPack.skipPixels = skipPixels;
    mDirtyBits.set(DIRTY_BIT_PACK_STATE);
}

GLint State::getPackSkipPixels() const
{
    return mPack.skipPixels;
}

const PixelPackState &State::getPackState() const
{
    return mPack;
}

PixelPackState &State::getPackState()
{
    return mPack;
}

void State::setUnpackAlignment(GLint alignment)
{
    mUnpack.alignment = alignment;
    mDirtyBits.set(DIRTY_BIT_UNPACK_STATE);
}

GLint State::getUnpackAlignment() const
{
    return mUnpack.alignment;
}

void State::setUnpackRowLength(GLint rowLength)
{
    mUnpack.rowLength = rowLength;
    mDirtyBits.set(DIRTY_BIT_UNPACK_STATE);
}

GLint State::getUnpackRowLength() const
{
    return mUnpack.rowLength;
}

void State::setUnpackImageHeight(GLint imageHeight)
{
    mUnpack.imageHeight = imageHeight;
    mDirtyBits.set(DIRTY_BIT_UNPACK_STATE);
}

GLint State::getUnpackImageHeight() const
{
    return mUnpack.imageHeight;
}

void State::setUnpackSkipImages(GLint skipImages)
{
    mUnpack.skipImages = skipImages;
    mDirtyBits.set(DIRTY_BIT_UNPACK_STATE);
}

GLint State::getUnpackSkipImages() const
{
    return mUnpack.skipImages;
}

void State::setUnpackSkipRows(GLint skipRows)
{
    mUnpack.skipRows = skipRows;
    mDirtyBits.set(DIRTY_BIT_UNPACK_STATE);
}

GLint State::getUnpackSkipRows() const
{
    return mUnpack.skipRows;
}

void State::setUnpackSkipPixels(GLint skipPixels)
{
    mUnpack.skipPixels = skipPixels;
    mDirtyBits.set(DIRTY_BIT_UNPACK_STATE);
}

GLint State::getUnpackSkipPixels() const
{
    return mUnpack.skipPixels;
}

const PixelUnpackState &State::getUnpackState() const
{
    return mUnpack;
}

PixelUnpackState &State::getUnpackState()
{
    return mUnpack;
}

const Debug &State::getDebug() const
{
    return mDebug;
}

Debug &State::getDebug()
{
    return mDebug;
}

void State::setCoverageModulation(GLenum components)
{
    mCoverageModulation = components;
    mDirtyBits.set(DIRTY_BIT_COVERAGE_MODULATION);
}

GLenum State::getCoverageModulation() const
{
    return mCoverageModulation;
}

void State::loadPathRenderingMatrix(GLenum matrixMode, const GLfloat *matrix)
{
    if (matrixMode == GL_PATH_MODELVIEW_CHROMIUM)
    {
        memcpy(mPathMatrixMV, matrix, 16 * sizeof(GLfloat));
        mDirtyBits.set(DIRTY_BIT_PATH_RENDERING_MATRIX_MV);
    }
    else if (matrixMode == GL_PATH_PROJECTION_CHROMIUM)
    {
        memcpy(mPathMatrixProj, matrix, 16 * sizeof(GLfloat));
        mDirtyBits.set(DIRTY_BIT_PATH_RENDERING_MATRIX_PROJ);
    }
    else
    {
        UNREACHABLE();
    }
}

const GLfloat *State::getPathRenderingMatrix(GLenum which) const
{
    if (which == GL_PATH_MODELVIEW_MATRIX_CHROMIUM)
    {
        return mPathMatrixMV;
    }
    else if (which == GL_PATH_PROJECTION_MATRIX_CHROMIUM)
    {
        return mPathMatrixProj;
    }

    UNREACHABLE();
    return nullptr;
}

void State::setPathStencilFunc(GLenum func, GLint ref, GLuint mask)
{
    mPathStencilFunc = func;
    mPathStencilRef  = ref;
    mPathStencilMask = mask;
    mDirtyBits.set(DIRTY_BIT_PATH_RENDERING_STENCIL_STATE);
}

GLenum State::getPathStencilFunc() const
{
    return mPathStencilFunc;
}

GLint State::getPathStencilRef() const
{
    return mPathStencilRef;
}

GLuint State::getPathStencilMask() const
{
    return mPathStencilMask;
}

void State::setFramebufferSRGB(bool sRGB)
{
    mFramebufferSRGB = sRGB;
    mDirtyBits.set(DIRTY_BIT_FRAMEBUFFER_SRGB);
}

bool State::getFramebufferSRGB() const
{
    return mFramebufferSRGB;
}

void State::getBooleanv(GLenum pname, GLboolean *params)
{
    switch (pname)
    {
      case GL_SAMPLE_COVERAGE_INVERT:    *params = mSampleCoverageInvert;         break;
      case GL_DEPTH_WRITEMASK:           *params = mDepthStencil.depthMask;       break;
      case GL_COLOR_WRITEMASK:
        params[0] = mBlend.colorMaskRed;
        params[1] = mBlend.colorMaskGreen;
        params[2] = mBlend.colorMaskBlue;
        params[3] = mBlend.colorMaskAlpha;
        break;
      case GL_CULL_FACE:
          *params = mRasterizer.cullFace;
          break;
      case GL_POLYGON_OFFSET_FILL:       *params = mRasterizer.polygonOffsetFill; break;
      case GL_SAMPLE_ALPHA_TO_COVERAGE:  *params = mBlend.sampleAlphaToCoverage;  break;
      case GL_SAMPLE_COVERAGE:           *params = mSampleCoverage;               break;
      case GL_SAMPLE_MASK:
          *params = mSampleMask;
          break;
      case GL_SCISSOR_TEST:              *params = mScissorTest;                  break;
      case GL_STENCIL_TEST:              *params = mDepthStencil.stencilTest;     break;
      case GL_DEPTH_TEST:                *params = mDepthStencil.depthTest;       break;
      case GL_BLEND:                     *params = mBlend.blend;                  break;
      case GL_DITHER:                    *params = mBlend.dither;                 break;
      case GL_TRANSFORM_FEEDBACK_ACTIVE: *params = getCurrentTransformFeedback()->isActive() ? GL_TRUE : GL_FALSE; break;
      case GL_TRANSFORM_FEEDBACK_PAUSED: *params = getCurrentTransformFeedback()->isPaused() ? GL_TRUE : GL_FALSE; break;
      case GL_PRIMITIVE_RESTART_FIXED_INDEX:
          *params = mPrimitiveRestart;
          break;
      case GL_RASTERIZER_DISCARD:
          *params = isRasterizerDiscardEnabled() ? GL_TRUE : GL_FALSE;
          break;
      case GL_DEBUG_OUTPUT_SYNCHRONOUS:
          *params = mDebug.isOutputSynchronous() ? GL_TRUE : GL_FALSE;
          break;
      case GL_DEBUG_OUTPUT:
          *params = mDebug.isOutputEnabled() ? GL_TRUE : GL_FALSE;
          break;
      case GL_MULTISAMPLE_EXT:
          *params = mMultiSampling;
          break;
      case GL_SAMPLE_ALPHA_TO_ONE_EXT:
          *params = mSampleAlphaToOne;
          break;
      case GL_BIND_GENERATES_RESOURCE_CHROMIUM:
          *params = isBindGeneratesResourceEnabled() ? GL_TRUE : GL_FALSE;
          break;
      case GL_CLIENT_ARRAYS_ANGLE:
          *params = areClientArraysEnabled() ? GL_TRUE : GL_FALSE;
          break;
      case GL_FRAMEBUFFER_SRGB_EXT:
          *params = getFramebufferSRGB() ? GL_TRUE : GL_FALSE;
          break;
      case GL_ROBUST_RESOURCE_INITIALIZATION_ANGLE:
          *params = mRobustResourceInit ? GL_TRUE : GL_FALSE;
          break;
      case GL_PROGRAM_CACHE_ENABLED_ANGLE:
          *params = mProgramBinaryCacheEnabled ? GL_TRUE : GL_FALSE;
          break;

      default:
        UNREACHABLE();
        break;
    }
}

void State::getFloatv(GLenum pname, GLfloat *params)
{
    // Please note: DEPTH_CLEAR_VALUE is included in our internal getFloatv implementation
    // because it is stored as a float, despite the fact that the GL ES 2.0 spec names
    // GetIntegerv as its native query function. As it would require conversion in any
    // case, this should make no difference to the calling application.
    switch (pname)
    {
      case GL_LINE_WIDTH:               *params = mLineWidth;                         break;
      case GL_SAMPLE_COVERAGE_VALUE:    *params = mSampleCoverageValue;               break;
      case GL_DEPTH_CLEAR_VALUE:        *params = mDepthClearValue;                   break;
      case GL_POLYGON_OFFSET_FACTOR:    *params = mRasterizer.polygonOffsetFactor;    break;
      case GL_POLYGON_OFFSET_UNITS:     *params = mRasterizer.polygonOffsetUnits;     break;
      case GL_DEPTH_RANGE:
        params[0] = mNearZ;
        params[1] = mFarZ;
        break;
      case GL_COLOR_CLEAR_VALUE:
        params[0] = mColorClearValue.red;
        params[1] = mColorClearValue.green;
        params[2] = mColorClearValue.blue;
        params[3] = mColorClearValue.alpha;
        break;
      case GL_BLEND_COLOR:
        params[0] = mBlendColor.red;
        params[1] = mBlendColor.green;
        params[2] = mBlendColor.blue;
        params[3] = mBlendColor.alpha;
        break;
      case GL_MULTISAMPLE_EXT:
        *params = static_cast<GLfloat>(mMultiSampling);
        break;
      case GL_SAMPLE_ALPHA_TO_ONE_EXT:
        *params = static_cast<GLfloat>(mSampleAlphaToOne);
      case GL_COVERAGE_MODULATION_CHROMIUM:
          params[0] = static_cast<GLfloat>(mCoverageModulation);
          break;
      default:
        UNREACHABLE();
        break;
    }
}

void State::getIntegerv(const Context *context, GLenum pname, GLint *params)
{
    if (pname >= GL_DRAW_BUFFER0_EXT && pname <= GL_DRAW_BUFFER15_EXT)
    {
        unsigned int colorAttachment = (pname - GL_DRAW_BUFFER0_EXT);
        ASSERT(colorAttachment < mMaxDrawBuffers);
        Framebuffer *framebuffer = mDrawFramebuffer;
        *params = framebuffer->getDrawBufferState(colorAttachment);
        return;
    }

    // Please note: DEPTH_CLEAR_VALUE is not included in our internal getIntegerv implementation
    // because it is stored as a float, despite the fact that the GL ES 2.0 spec names
    // GetIntegerv as its native query function. As it would require conversion in any
    // case, this should make no difference to the calling application. You may find it in
    // State::getFloatv.
    switch (pname)
    {
        case GL_ARRAY_BUFFER_BINDING:
            *params = mBoundBuffers[BufferBinding::Array].id();
            break;
        case GL_DRAW_INDIRECT_BUFFER_BINDING:
            *params = mBoundBuffers[BufferBinding::DrawIndirect].id();
            break;
        case GL_ELEMENT_ARRAY_BUFFER_BINDING:
            *params = getVertexArray()->getElementArrayBuffer().id();
            break;
        //case GL_FRAMEBUFFER_BINDING:                    // now equivalent to GL_DRAW_FRAMEBUFFER_BINDING_ANGLE
      case GL_DRAW_FRAMEBUFFER_BINDING_ANGLE:           *params = mDrawFramebuffer->id();                         break;
      case GL_READ_FRAMEBUFFER_BINDING_ANGLE:           *params = mReadFramebuffer->id();                         break;
      case GL_RENDERBUFFER_BINDING:                     *params = mRenderbuffer.id();                             break;
      case GL_VERTEX_ARRAY_BINDING:                     *params = mVertexArray->id();                             break;
      case GL_CURRENT_PROGRAM:                          *params = mProgram ? mProgram->id() : 0;                  break;
      case GL_PACK_ALIGNMENT:                           *params = mPack.alignment;                                break;
      case GL_PACK_REVERSE_ROW_ORDER_ANGLE:             *params = mPack.reverseRowOrder;                          break;
      case GL_PACK_ROW_LENGTH:
          *params = mPack.rowLength;
          break;
      case GL_PACK_SKIP_ROWS:
          *params = mPack.skipRows;
          break;
      case GL_PACK_SKIP_PIXELS:
          *params = mPack.skipPixels;
          break;
      case GL_UNPACK_ALIGNMENT:                         *params = mUnpack.alignment;                              break;
      case GL_UNPACK_ROW_LENGTH:                        *params = mUnpack.rowLength;                              break;
      case GL_UNPACK_IMAGE_HEIGHT:
          *params = mUnpack.imageHeight;
          break;
      case GL_UNPACK_SKIP_IMAGES:
          *params = mUnpack.skipImages;
          break;
      case GL_UNPACK_SKIP_ROWS:
          *params = mUnpack.skipRows;
          break;
      case GL_UNPACK_SKIP_PIXELS:
          *params = mUnpack.skipPixels;
          break;
      case GL_GENERATE_MIPMAP_HINT:                     *params = mGenerateMipmapHint;                            break;
      case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:      *params = mFragmentShaderDerivativeHint;                  break;
      case GL_ACTIVE_TEXTURE:
          *params = (static_cast<GLint>(mActiveSampler) + GL_TEXTURE0);
          break;
      case GL_STENCIL_FUNC:                             *params = mDepthStencil.stencilFunc;                      break;
      case GL_STENCIL_REF:                              *params = mStencilRef;                                    break;
      case GL_STENCIL_VALUE_MASK:
          *params = CastMaskValue(context, mDepthStencil.stencilMask);
          break;
      case GL_STENCIL_BACK_FUNC:                        *params = mDepthStencil.stencilBackFunc;                  break;
      case GL_STENCIL_BACK_REF:                         *params = mStencilBackRef;                                break;
      case GL_STENCIL_BACK_VALUE_MASK:
          *params = CastMaskValue(context, mDepthStencil.stencilBackMask);
          break;
      case GL_STENCIL_FAIL:                             *params = mDepthStencil.stencilFail;                      break;
      case GL_STENCIL_PASS_DEPTH_FAIL:                  *params = mDepthStencil.stencilPassDepthFail;             break;
      case GL_STENCIL_PASS_DEPTH_PASS:                  *params = mDepthStencil.stencilPassDepthPass;             break;
      case GL_STENCIL_BACK_FAIL:                        *params = mDepthStencil.stencilBackFail;                  break;
      case GL_STENCIL_BACK_PASS_DEPTH_FAIL:             *params = mDepthStencil.stencilBackPassDepthFail;         break;
      case GL_STENCIL_BACK_PASS_DEPTH_PASS:             *params = mDepthStencil.stencilBackPassDepthPass;         break;
      case GL_DEPTH_FUNC:                               *params = mDepthStencil.depthFunc;                        break;
      case GL_BLEND_SRC_RGB:                            *params = mBlend.sourceBlendRGB;                          break;
      case GL_BLEND_SRC_ALPHA:                          *params = mBlend.sourceBlendAlpha;                        break;
      case GL_BLEND_DST_RGB:                            *params = mBlend.destBlendRGB;                            break;
      case GL_BLEND_DST_ALPHA:                          *params = mBlend.destBlendAlpha;                          break;
      case GL_BLEND_EQUATION_RGB:                       *params = mBlend.blendEquationRGB;                        break;
      case GL_BLEND_EQUATION_ALPHA:                     *params = mBlend.blendEquationAlpha;                      break;
      case GL_STENCIL_WRITEMASK:
          *params = CastMaskValue(context, mDepthStencil.stencilWritemask);
          break;
      case GL_STENCIL_BACK_WRITEMASK:
          *params = CastMaskValue(context, mDepthStencil.stencilBackWritemask);
          break;
      case GL_STENCIL_CLEAR_VALUE:                      *params = mStencilClearValue;                             break;
      case GL_IMPLEMENTATION_COLOR_READ_TYPE:
          *params = mReadFramebuffer->getImplementationColorReadType(context);
          break;
      case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
          *params = mReadFramebuffer->getImplementationColorReadFormat(context);
          break;
      case GL_SAMPLE_BUFFERS:
      case GL_SAMPLES:
        {
            Framebuffer *framebuffer = mDrawFramebuffer;
            if (framebuffer->checkStatus(context) == GL_FRAMEBUFFER_COMPLETE)
            {
                switch (pname)
                {
                    case GL_SAMPLE_BUFFERS:
                        if (framebuffer->getSamples(context) != 0)
                        {
                            *params = 1;
                        }
                        else
                        {
                            *params = 0;
                        }
                        break;
                    case GL_SAMPLES:
                        *params = framebuffer->getSamples(context);
                        break;
                }
            }
            else
            {
                *params = 0;
            }
        }
        break;
      case GL_VIEWPORT:
        params[0] = mViewport.x;
        params[1] = mViewport.y;
        params[2] = mViewport.width;
        params[3] = mViewport.height;
        break;
      case GL_SCISSOR_BOX:
        params[0] = mScissor.x;
        params[1] = mScissor.y;
        params[2] = mScissor.width;
        params[3] = mScissor.height;
        break;
      case GL_CULL_FACE_MODE:
          *params = ToGLenum(mRasterizer.cullMode);
          break;
      case GL_FRONT_FACE:
          *params = mRasterizer.frontFace;
          break;
      case GL_RED_BITS:
      case GL_GREEN_BITS:
      case GL_BLUE_BITS:
      case GL_ALPHA_BITS:
        {
            Framebuffer *framebuffer                 = getDrawFramebuffer();
            const FramebufferAttachment *colorbuffer = framebuffer->getFirstColorbuffer();

            if (colorbuffer)
            {
                switch (pname)
                {
                case GL_RED_BITS:   *params = colorbuffer->getRedSize();      break;
                case GL_GREEN_BITS: *params = colorbuffer->getGreenSize();    break;
                case GL_BLUE_BITS:  *params = colorbuffer->getBlueSize();     break;
                case GL_ALPHA_BITS: *params = colorbuffer->getAlphaSize();    break;
                }
            }
            else
            {
                *params = 0;
            }
        }
        break;
      case GL_DEPTH_BITS:
        {
            const Framebuffer *framebuffer           = getDrawFramebuffer();
            const FramebufferAttachment *depthbuffer = framebuffer->getDepthbuffer();

            if (depthbuffer)
            {
                *params = depthbuffer->getDepthSize();
            }
            else
            {
                *params = 0;
            }
        }
        break;
      case GL_STENCIL_BITS:
        {
            const Framebuffer *framebuffer             = getDrawFramebuffer();
            const FramebufferAttachment *stencilbuffer = framebuffer->getStencilbuffer();

            if (stencilbuffer)
            {
                *params = stencilbuffer->getStencilSize();
            }
            else
            {
                *params = 0;
            }
        }
        break;
      case GL_TEXTURE_BINDING_2D:
        ASSERT(mActiveSampler < mMaxCombinedTextureImageUnits);
        *params = getSamplerTextureId(static_cast<unsigned int>(mActiveSampler), GL_TEXTURE_2D);
        break;
      case GL_TEXTURE_BINDING_RECTANGLE_ANGLE:
          ASSERT(mActiveSampler < mMaxCombinedTextureImageUnits);
          *params = getSamplerTextureId(static_cast<unsigned int>(mActiveSampler),
                                        GL_TEXTURE_RECTANGLE_ANGLE);
          break;
      case GL_TEXTURE_BINDING_CUBE_MAP:
        ASSERT(mActiveSampler < mMaxCombinedTextureImageUnits);
        *params =
            getSamplerTextureId(static_cast<unsigned int>(mActiveSampler), GL_TEXTURE_CUBE_MAP);
        break;
      case GL_TEXTURE_BINDING_3D:
        ASSERT(mActiveSampler < mMaxCombinedTextureImageUnits);
        *params = getSamplerTextureId(static_cast<unsigned int>(mActiveSampler), GL_TEXTURE_3D);
        break;
      case GL_TEXTURE_BINDING_2D_ARRAY:
        ASSERT(mActiveSampler < mMaxCombinedTextureImageUnits);
        *params =
            getSamplerTextureId(static_cast<unsigned int>(mActiveSampler), GL_TEXTURE_2D_ARRAY);
        break;
      case GL_TEXTURE_BINDING_2D_MULTISAMPLE:
          ASSERT(mActiveSampler < mMaxCombinedTextureImageUnits);
          *params = getSamplerTextureId(static_cast<unsigned int>(mActiveSampler),
                                        GL_TEXTURE_2D_MULTISAMPLE);
          break;
      case GL_TEXTURE_BINDING_EXTERNAL_OES:
          ASSERT(mActiveSampler < mMaxCombinedTextureImageUnits);
          *params = getSamplerTextureId(static_cast<unsigned int>(mActiveSampler),
                                        GL_TEXTURE_EXTERNAL_OES);
          break;
      case GL_UNIFORM_BUFFER_BINDING:
          *params = mBoundBuffers[BufferBinding::Uniform].id();
          break;
      case GL_TRANSFORM_FEEDBACK_BINDING:
        *params = mTransformFeedback.id();
        break;
      case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
          ASSERT(mTransformFeedback.get() != nullptr);
          *params = mTransformFeedback->getGenericBuffer().id();
          break;
      case GL_COPY_READ_BUFFER_BINDING:
          *params = mBoundBuffers[BufferBinding::CopyRead].id();
          break;
      case GL_COPY_WRITE_BUFFER_BINDING:
          *params = mBoundBuffers[BufferBinding::CopyWrite].id();
          break;
      case GL_PIXEL_PACK_BUFFER_BINDING:
          *params = mBoundBuffers[BufferBinding::PixelPack].id();
          break;
      case GL_PIXEL_UNPACK_BUFFER_BINDING:
          *params = mBoundBuffers[BufferBinding::PixelUnpack].id();
          break;
      case GL_READ_BUFFER:
          *params = mReadFramebuffer->getReadBufferState();
          break;
      case GL_SAMPLER_BINDING:
          ASSERT(mActiveSampler < mMaxCombinedTextureImageUnits);
          *params = getSamplerId(static_cast<GLuint>(mActiveSampler));
          break;
      case GL_DEBUG_LOGGED_MESSAGES:
          *params = static_cast<GLint>(mDebug.getMessageCount());
          break;
      case GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH:
          *params = static_cast<GLint>(mDebug.getNextMessageLength());
          break;
      case GL_DEBUG_GROUP_STACK_DEPTH:
          *params = static_cast<GLint>(mDebug.getGroupStackDepth());
          break;
      case GL_MULTISAMPLE_EXT:
          *params = static_cast<GLint>(mMultiSampling);
          break;
      case GL_SAMPLE_ALPHA_TO_ONE_EXT:
          *params = static_cast<GLint>(mSampleAlphaToOne);
      case GL_COVERAGE_MODULATION_CHROMIUM:
          *params = static_cast<GLint>(mCoverageModulation);
          break;
      case GL_ATOMIC_COUNTER_BUFFER_BINDING:
          *params = mBoundBuffers[BufferBinding::AtomicCounter].id();
          break;
      case GL_SHADER_STORAGE_BUFFER_BINDING:
          *params = mBoundBuffers[BufferBinding::ShaderStorage].id();
          break;
      default:
        UNREACHABLE();
        break;
    }
}

void State::getPointerv(GLenum pname, void **params) const
{
    switch (pname)
    {
        case GL_DEBUG_CALLBACK_FUNCTION:
            *params = reinterpret_cast<void *>(mDebug.getCallback());
            break;
        case GL_DEBUG_CALLBACK_USER_PARAM:
            *params = const_cast<void *>(mDebug.getUserParam());
            break;
        default:
            UNREACHABLE();
            break;
    }
}

void State::getIntegeri_v(GLenum target, GLuint index, GLint *data)
{
    switch (target)
    {
      case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
          ASSERT(static_cast<size_t>(index) < mTransformFeedback->getIndexedBufferCount());
          *data = mTransformFeedback->getIndexedBuffer(index).id();
          break;
      case GL_UNIFORM_BUFFER_BINDING:
          ASSERT(static_cast<size_t>(index) < mUniformBuffers.size());
          *data = mUniformBuffers[index].id();
          break;
      case GL_ATOMIC_COUNTER_BUFFER_BINDING:
          ASSERT(static_cast<size_t>(index) < mAtomicCounterBuffers.size());
          *data = mAtomicCounterBuffers[index].id();
          break;
      case GL_SHADER_STORAGE_BUFFER_BINDING:
          ASSERT(static_cast<size_t>(index) < mShaderStorageBuffers.size());
          *data = mShaderStorageBuffers[index].id();
          break;
      case GL_VERTEX_BINDING_BUFFER:
          ASSERT(static_cast<size_t>(index) < mVertexArray->getMaxBindings());
          *data = mVertexArray->getVertexBinding(index).getBuffer().id();
          break;
      case GL_VERTEX_BINDING_DIVISOR:
          ASSERT(static_cast<size_t>(index) < mVertexArray->getMaxBindings());
          *data = mVertexArray->getVertexBinding(index).getDivisor();
          break;
      case GL_VERTEX_BINDING_OFFSET:
          ASSERT(static_cast<size_t>(index) < mVertexArray->getMaxBindings());
          *data = static_cast<GLuint>(mVertexArray->getVertexBinding(index).getOffset());
          break;
      case GL_VERTEX_BINDING_STRIDE:
          ASSERT(static_cast<size_t>(index) < mVertexArray->getMaxBindings());
          *data = mVertexArray->getVertexBinding(index).getStride();
          break;
      case GL_SAMPLE_MASK_VALUE:
          ASSERT(static_cast<size_t>(index) < mSampleMaskValues.size());
          *data = mSampleMaskValues[index];
          break;
      default:
          UNREACHABLE();
          break;
    }
}

void State::getInteger64i_v(GLenum target, GLuint index, GLint64 *data)
{
    switch (target)
    {
      case GL_TRANSFORM_FEEDBACK_BUFFER_START:
          ASSERT(static_cast<size_t>(index) < mTransformFeedback->getIndexedBufferCount());
          *data = mTransformFeedback->getIndexedBuffer(index).getOffset();
          break;
      case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
          ASSERT(static_cast<size_t>(index) < mTransformFeedback->getIndexedBufferCount());
          *data = mTransformFeedback->getIndexedBuffer(index).getSize();
          break;
      case GL_UNIFORM_BUFFER_START:
          ASSERT(static_cast<size_t>(index) < mUniformBuffers.size());
          *data = mUniformBuffers[index].getOffset();
          break;
      case GL_UNIFORM_BUFFER_SIZE:
          ASSERT(static_cast<size_t>(index) < mUniformBuffers.size());
          *data = mUniformBuffers[index].getSize();
          break;
      case GL_ATOMIC_COUNTER_BUFFER_START:
          ASSERT(static_cast<size_t>(index) < mAtomicCounterBuffers.size());
          *data = mAtomicCounterBuffers[index].getOffset();
          break;
      case GL_ATOMIC_COUNTER_BUFFER_SIZE:
          ASSERT(static_cast<size_t>(index) < mAtomicCounterBuffers.size());
          *data = mAtomicCounterBuffers[index].getSize();
          break;
      case GL_SHADER_STORAGE_BUFFER_START:
          ASSERT(static_cast<size_t>(index) < mShaderStorageBuffers.size());
          *data = mShaderStorageBuffers[index].getOffset();
          break;
      case GL_SHADER_STORAGE_BUFFER_SIZE:
          ASSERT(static_cast<size_t>(index) < mShaderStorageBuffers.size());
          *data = mShaderStorageBuffers[index].getSize();
          break;
      default:
          UNREACHABLE();
          break;
    }
}

void State::getBooleani_v(GLenum target, GLuint index, GLboolean *data)
{
    UNREACHABLE();
}

bool State::hasMappedBuffer(BufferBinding target) const
{
    if (target == BufferBinding::Array)
    {
        const VertexArray *vao     = getVertexArray();
        const auto &vertexAttribs = vao->getVertexAttributes();
        const auto &vertexBindings = vao->getVertexBindings();
        size_t maxEnabledAttrib = vao->getMaxEnabledAttribute();
        for (size_t attribIndex = 0; attribIndex < maxEnabledAttrib; attribIndex++)
        {
            const VertexAttribute &vertexAttrib = vertexAttribs[attribIndex];
            auto *boundBuffer = vertexBindings[vertexAttrib.bindingIndex].getBuffer().get();
            if (vertexAttrib.enabled && boundBuffer && boundBuffer->isMapped())
            {
                return true;
            }
        }

        return false;
    }
    else
    {
        Buffer *buffer = getTargetBuffer(target);
        return (buffer && buffer->isMapped());
    }
}

void State::syncDirtyObjects(const Context *context)
{
    if (!mDirtyObjects.any())
        return;

    syncDirtyObjects(context, mDirtyObjects);
}

void State::syncDirtyObjects(const Context *context, const DirtyObjects &bitset)
{
    for (auto dirtyObject : bitset)
    {
        switch (dirtyObject)
        {
            case DIRTY_OBJECT_READ_FRAMEBUFFER:
                ASSERT(mReadFramebuffer);
                mReadFramebuffer->syncState(context);
                break;
            case DIRTY_OBJECT_DRAW_FRAMEBUFFER:
                ASSERT(mDrawFramebuffer);
                mDrawFramebuffer->syncState(context);
                break;
            case DIRTY_OBJECT_VERTEX_ARRAY:
                ASSERT(mVertexArray);
                mVertexArray->syncState(context);
                break;
            case DIRTY_OBJECT_PROGRAM_TEXTURES:
                syncProgramTextures(context);
                break;

            default:
                UNREACHABLE();
                break;
        }
    }

    mDirtyObjects &= ~bitset;
}

void State::syncProgramTextures(const Context *context)
{
    // TODO(jmadill): Fine-grained updates.
    if (!mProgram)
    {
        return;
    }

    ASSERT(mDirtyObjects[DIRTY_OBJECT_PROGRAM_TEXTURES]);
    mDirtyBits.set(DIRTY_BIT_TEXTURE_BINDINGS);

    ActiveTextureMask newActiveTextures;

    // Initialize to the 'Initialized' state and set to 'MayNeedInit' if any texture is not
    // initialized.
    mCachedTexturesInitState = InitState::Initialized;

    for (const SamplerBinding &samplerBinding : mProgram->getSamplerBindings())
    {
        if (samplerBinding.unreferenced)
            continue;

        GLenum textureType = samplerBinding.textureType;
        for (GLuint textureUnitIndex : samplerBinding.boundTextureUnits)
        {
            Texture *texture = getSamplerTexture(textureUnitIndex, textureType);
            Sampler *sampler = getSampler(textureUnitIndex);
            ASSERT(static_cast<size_t>(textureUnitIndex) < mCompleteTextureCache.size());
            ASSERT(static_cast<size_t>(textureUnitIndex) < newActiveTextures.size());

            ASSERT(texture);

            // Mark the texture binding bit as dirty if the texture completeness changes.
            // TODO(jmadill): Use specific dirty bit for completeness change.
            if (texture->isSamplerComplete(context, sampler) &&
                !mDrawFramebuffer->hasTextureAttachment(texture))
            {
                texture->syncState();
                mCompleteTextureCache[textureUnitIndex] = texture;
            }
            else
            {
                mCompleteTextureCache[textureUnitIndex] = nullptr;
            }

            // Bind the texture unconditionally, to recieve completeness change notifications.
            mCompleteTextureBindings[textureUnitIndex].bind(texture->getDirtyChannel());
            mActiveTexturesMask.set(textureUnitIndex);
            newActiveTextures.set(textureUnitIndex);

            if (sampler != nullptr)
            {
                sampler->syncState(context);
            }

            if (texture->initState() == InitState::MayNeedInit)
            {
                mCachedTexturesInitState = InitState::MayNeedInit;
            }
        }
    }

    // Unset now missing textures.
    ActiveTextureMask negativeMask = mActiveTexturesMask & ~newActiveTextures;
    if (negativeMask.any())
    {
        for (auto textureIndex : negativeMask)
        {
            mCompleteTextureBindings[textureIndex].reset();
            mCompleteTextureCache[textureIndex] = nullptr;
            mActiveTexturesMask.reset(textureIndex);
        }
    }
}

void State::syncDirtyObject(const Context *context, GLenum target)
{
    DirtyObjects localSet;

    switch (target)
    {
        case GL_READ_FRAMEBUFFER:
            localSet.set(DIRTY_OBJECT_READ_FRAMEBUFFER);
            break;
        case GL_DRAW_FRAMEBUFFER:
            localSet.set(DIRTY_OBJECT_DRAW_FRAMEBUFFER);
            break;
        case GL_FRAMEBUFFER:
            localSet.set(DIRTY_OBJECT_READ_FRAMEBUFFER);
            localSet.set(DIRTY_OBJECT_DRAW_FRAMEBUFFER);
            break;
        case GL_VERTEX_ARRAY:
            localSet.set(DIRTY_OBJECT_VERTEX_ARRAY);
            break;
        case GL_TEXTURE:
        case GL_SAMPLER:
        case GL_PROGRAM:
            localSet.set(DIRTY_OBJECT_PROGRAM_TEXTURES);
            break;
    }

    syncDirtyObjects(context, localSet);
}

void State::setObjectDirty(GLenum target)
{
    switch (target)
    {
        case GL_READ_FRAMEBUFFER:
            mDirtyObjects.set(DIRTY_OBJECT_READ_FRAMEBUFFER);
            break;
        case GL_DRAW_FRAMEBUFFER:
            mDirtyObjects.set(DIRTY_OBJECT_DRAW_FRAMEBUFFER);
            break;
        case GL_FRAMEBUFFER:
            mDirtyObjects.set(DIRTY_OBJECT_READ_FRAMEBUFFER);
            mDirtyObjects.set(DIRTY_OBJECT_DRAW_FRAMEBUFFER);
            break;
        case GL_VERTEX_ARRAY:
            mDirtyObjects.set(DIRTY_OBJECT_VERTEX_ARRAY);
            break;
        case GL_TEXTURE:
        case GL_SAMPLER:
        case GL_PROGRAM:
            mDirtyObjects.set(DIRTY_OBJECT_PROGRAM_TEXTURES);
            mDirtyBits.set(DIRTY_BIT_TEXTURE_BINDINGS);
            break;
    }
}

void State::onProgramExecutableChange(Program *program)
{
    // OpenGL Spec:
    // "If LinkProgram or ProgramBinary successfully re-links a program object
    //  that was already in use as a result of a previous call to UseProgram, then the
    //  generated executable code will be installed as part of the current rendering state."
    if (program->isLinked() && mProgram == program)
    {
        mDirtyBits.set(DIRTY_BIT_PROGRAM_EXECUTABLE);
        mDirtyObjects.set(DIRTY_OBJECT_PROGRAM_TEXTURES);
    }
}

void State::setImageUnit(const Context *context,
                         GLuint unit,
                         Texture *texture,
                         GLint level,
                         GLboolean layered,
                         GLint layer,
                         GLenum access,
                         GLenum format)
{
    mImageUnits[unit].texture.set(context, texture);
    mImageUnits[unit].level   = level;
    mImageUnits[unit].layered = layered;
    mImageUnits[unit].layer   = layer;
    mImageUnits[unit].access  = access;
    mImageUnits[unit].format  = format;
}

const ImageUnit &State::getImageUnit(GLuint unit) const
{
    return mImageUnits[unit];
}

// Handle a dirty texture event.
void State::signal(size_t textureIndex, InitState initState)
{
    // Conservatively assume all textures are dirty.
    // TODO(jmadill): More fine-grained update.
    mDirtyObjects.set(DIRTY_OBJECT_PROGRAM_TEXTURES);

    if (initState == InitState::MayNeedInit)
    {
        mCachedTexturesInitState = InitState::MayNeedInit;
    }
}

Error State::clearUnclearedActiveTextures(const Context *context)
{
    ASSERT(mRobustResourceInit);

    if (mCachedTexturesInitState == InitState::Initialized)
    {
        return NoError();
    }

    for (auto textureIndex : mActiveTexturesMask)
    {
        Texture *texture = mCompleteTextureCache[textureIndex];
        if (texture)
        {
            ANGLE_TRY(texture->ensureInitialized(context));
        }
    }

    mCachedTexturesInitState = InitState::Initialized;

    return NoError();
}

AttributesMask State::getAndResetDirtyCurrentValues() const
{
    AttributesMask retVal = mDirtyCurrentValues;
    mDirtyCurrentValues.reset();
    return retVal;
}

}  // namespace gl
