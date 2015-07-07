#include "precompiled.h"
//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Context.cpp: Implements the gl::Context class, managing all GL state and performing
// rendering operations. It is the GLES2 specific implementation of EGLContext.

#include "libGLESv2/Context.h"

#include "libGLESv2/main.h"
#include "common/utilities.h"
#include "libGLESv2/formatutils.h"
#include "libGLESv2/Buffer.h"
#include "libGLESv2/Fence.h"
#include "libGLESv2/Framebuffer.h"
#include "libGLESv2/Renderbuffer.h"
#include "libGLESv2/Program.h"
#include "libGLESv2/ProgramBinary.h"
#include "libGLESv2/Query.h"
#include "libGLESv2/Texture.h"
#include "libGLESv2/ResourceManager.h"
#include "libGLESv2/renderer/IndexDataManager.h"
#include "libGLESv2/renderer/RenderTarget.h"
#include "libGLESv2/renderer/Renderer.h"
#include "libGLESv2/VertexArray.h"
#include "libGLESv2/Sampler.h"
#include "libGLESv2/validationES.h"
#include "libGLESv2/TransformFeedback.h"

#include "libEGL/Surface.h"

#undef near
#undef far

namespace gl
{
static const char* makeStaticString(const std::string& str)
{
    static std::set<std::string> strings;
    std::set<std::string>::iterator it = strings.find(str);
    if (it != strings.end())
      return it->c_str();

    return strings.insert(str).first->c_str();
}

Context::Context(int clientVersion, const gl::Context *shareContext, rx::Renderer *renderer, bool notifyResets, bool robustAccess) : mRenderer(renderer)
{
    ASSERT(robustAccess == false);   // Unimplemented

    mFenceNVHandleAllocator.setBaseHandle(0);

    setClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    mClientVersion = clientVersion;

    mState.depthClearValue = 1.0f;
    mState.stencilClearValue = 0;

    mState.rasterizer.rasterizerDiscard = false;
    mState.rasterizer.cullFace = false;
    mState.rasterizer.cullMode = GL_BACK;
    mState.rasterizer.frontFace = GL_CCW;
    mState.rasterizer.polygonOffsetFill = false;
    mState.rasterizer.polygonOffsetFactor = 0.0f;
    mState.rasterizer.polygonOffsetUnits = 0.0f;
    mState.rasterizer.pointDrawMode = false;
    mState.rasterizer.multiSample = false;
    mState.scissorTest = false;
    mState.scissor.x = 0;
    mState.scissor.y = 0;
    mState.scissor.width = 0;
    mState.scissor.height = 0;

    mState.blend.blend = false;
    mState.blend.sourceBlendRGB = GL_ONE;
    mState.blend.sourceBlendAlpha = GL_ONE;
    mState.blend.destBlendRGB = GL_ZERO;
    mState.blend.destBlendAlpha = GL_ZERO;
    mState.blend.blendEquationRGB = GL_FUNC_ADD;
    mState.blend.blendEquationAlpha = GL_FUNC_ADD;
    mState.blend.sampleAlphaToCoverage = false;
    mState.blend.dither = true;

    mState.blendColor.red = 0;
    mState.blendColor.green = 0;
    mState.blendColor.blue = 0;
    mState.blendColor.alpha = 0;

    mState.depthStencil.depthTest = false;
    mState.depthStencil.depthFunc = GL_LESS;
    mState.depthStencil.depthMask = true;
    mState.depthStencil.stencilTest = false;
    mState.depthStencil.stencilFunc = GL_ALWAYS;
    mState.depthStencil.stencilMask = -1;
    mState.depthStencil.stencilWritemask = -1;
    mState.depthStencil.stencilBackFunc = GL_ALWAYS;
    mState.depthStencil.stencilBackMask = - 1;
    mState.depthStencil.stencilBackWritemask = -1;
    mState.depthStencil.stencilFail = GL_KEEP;
    mState.depthStencil.stencilPassDepthFail = GL_KEEP;
    mState.depthStencil.stencilPassDepthPass = GL_KEEP;
    mState.depthStencil.stencilBackFail = GL_KEEP;
    mState.depthStencil.stencilBackPassDepthFail = GL_KEEP;
    mState.depthStencil.stencilBackPassDepthPass = GL_KEEP;

    mState.stencilRef = 0;
    mState.stencilBackRef = 0;

    mState.sampleCoverage = false;
    mState.sampleCoverageValue = 1.0f;
    mState.sampleCoverageInvert = false;
    mState.generateMipmapHint = GL_DONT_CARE;
    mState.fragmentShaderDerivativeHint = GL_DONT_CARE;

    mState.lineWidth = 1.0f;

    mState.viewport.x = 0;
    mState.viewport.y = 0;
    mState.viewport.width = 0;
    mState.viewport.height = 0;
    mState.zNear = 0.0f;
    mState.zFar = 1.0f;

    mState.blend.colorMaskRed = true;
    mState.blend.colorMaskGreen = true;
    mState.blend.colorMaskBlue = true;
    mState.blend.colorMaskAlpha = true;

    const GLfloat defaultFloatValues[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    for (int attribIndex = 0; attribIndex < MAX_VERTEX_ATTRIBS; attribIndex++)
    {
        mState.vertexAttribCurrentValues[attribIndex].setFloatValues(defaultFloatValues);
    }

    if (shareContext != NULL)
    {
        mResourceManager = shareContext->mResourceManager;
        mResourceManager->addRef();
    }
    else
    {
        mResourceManager = new ResourceManager(mRenderer);
    }

    // [OpenGL ES 2.0.24] section 3.7 page 83:
    // In the initial state, TEXTURE_2D and TEXTURE_CUBE_MAP have twodimensional
    // and cube map texture state vectors respectively associated with them.
    // In order that access to these initial textures not be lost, they are treated as texture
    // objects all of whose names are 0.

    mTexture2DZero.set(new Texture2D(mRenderer, 0));
    mTextureCubeMapZero.set(new TextureCubeMap(mRenderer, 0));
    mTexture3DZero.set(new Texture3D(mRenderer, 0));
    mTexture2DArrayZero.set(new Texture2DArray(mRenderer, 0));

    for (unsigned int textureUnit = 0; textureUnit < ArraySize(mState.samplers); textureUnit++)
    {
        mState.samplers[textureUnit] = 0;
    }

    mState.activeSampler = 0;
    bindVertexArray(0);
    bindArrayBuffer(0);
    bindElementArrayBuffer(0);
    bindTextureCubeMap(0);
    bindTexture2D(0);
    bindReadFramebuffer(0);
    bindDrawFramebuffer(0);
    bindRenderbuffer(0);

    mState.activeQueries[GL_ANY_SAMPLES_PASSED].set(NULL);
    mState.activeQueries[GL_ANY_SAMPLES_PASSED_CONSERVATIVE].set(NULL);
    mState.activeQueries[GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN].set(NULL);

    bindGenericUniformBuffer(0);
    for (int i = 0; i < IMPLEMENTATION_MAX_COMBINED_SHADER_UNIFORM_BUFFERS; i++)
    {
        bindIndexedUniformBuffer(0, i, 0, -1);
    }

    bindGenericTransformFeedbackBuffer(0);
    for (int i = 0; i < IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS; i++)
    {
        bindIndexedTransformFeedbackBuffer(0, i, 0, -1);
    }

    bindCopyReadBuffer(0);
    bindCopyWriteBuffer(0);
    bindPixelPackBuffer(0);
    bindPixelUnpackBuffer(0);

    // [OpenGL ES 3.0.2] section 2.14.1 pg 85:
    // In the initial state, a default transform feedback object is bound and treated as
    // a transform feedback object with a name of zero. That object is bound any time
    // BindTransformFeedback is called with id of zero
    mTransformFeedbackZero.set(new TransformFeedback(0));
    bindTransformFeedback(0);

    mState.currentProgram = 0;
    mCurrentProgramBinary.set(NULL);

    mCombinedExtensionsString = NULL;
    mRendererString = NULL;

    mInvalidEnum = false;
    mInvalidValue = false;
    mInvalidOperation = false;
    mOutOfMemory = false;
    mInvalidFramebufferOperation = false;

    mHasBeenCurrent = false;
    mContextLost = false;
    mResetStatus = GL_NO_ERROR;
    mResetStrategy = (notifyResets ? GL_LOSE_CONTEXT_ON_RESET_EXT : GL_NO_RESET_NOTIFICATION_EXT);
    mRobustAccess = robustAccess;

    mSupportsBGRATextures = false;
    mSupportsDXT1Textures = false;
    mSupportsDXT3Textures = false;
    mSupportsDXT5Textures = false;
    mSupportsEventQueries = false;
    mSupportsOcclusionQueries = false;
    mNumCompressedTextureFormats = 0;
}

Context::~Context()
{
    if (mState.currentProgram != 0)
    {
        Program *programObject = mResourceManager->getProgram(mState.currentProgram);
        if (programObject)
        {
            programObject->release();
        }
        mState.currentProgram = 0;
    }
    mCurrentProgramBinary.set(NULL);

    while (!mFramebufferMap.empty())
    {
        deleteFramebuffer(mFramebufferMap.begin()->first);
    }

    while (!mFenceNVMap.empty())
    {
        deleteFenceNV(mFenceNVMap.begin()->first);
    }

    while (!mQueryMap.empty())
    {
        deleteQuery(mQueryMap.begin()->first);
    }

    while (!mVertexArrayMap.empty())
    {
        deleteVertexArray(mVertexArrayMap.begin()->first);
    }

    mTransformFeedbackZero.set(NULL);
    while (!mTransformFeedbackMap.empty())
    {
        deleteTransformFeedback(mTransformFeedbackMap.begin()->first);
    }

    for (int type = 0; type < TEXTURE_TYPE_COUNT; type++)
    {
        for (int sampler = 0; sampler < IMPLEMENTATION_MAX_COMBINED_TEXTURE_IMAGE_UNITS; sampler++)
        {
            mState.samplerTexture[type][sampler].set(NULL);
        }
    }

    for (int type = 0; type < TEXTURE_TYPE_COUNT; type++)
    {
        mIncompleteTextures[type].set(NULL);
    }

    const GLfloat defaultFloatValues[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    for (int attribIndex = 0; attribIndex < MAX_VERTEX_ATTRIBS; attribIndex++)
    {
        mState.vertexAttribCurrentValues[attribIndex].setFloatValues(defaultFloatValues);
    }

    mState.arrayBuffer.set(NULL);
    mState.renderbuffer.set(NULL);

    mState.transformFeedback.set(NULL);

    mTexture2DZero.set(NULL);
    mTextureCubeMapZero.set(NULL);
    mTexture3DZero.set(NULL);
    mTexture2DArrayZero.set(NULL);

    for (State::ActiveQueryMap::iterator i = mState.activeQueries.begin(); i != mState.activeQueries.end(); i++)
    {
        i->second.set(NULL);
    }

    mState.genericUniformBuffer.set(NULL);
    for (int i = 0; i < IMPLEMENTATION_MAX_COMBINED_SHADER_UNIFORM_BUFFERS; i++)
    {
        mState.uniformBuffers[i].set(NULL);
    }

    mState.genericTransformFeedbackBuffer.set(NULL);
    for (int i = 0; i < IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS; i++)
    {
        mState.transformFeedbackBuffers[i].set(NULL);
    }

    mState.copyReadBuffer.set(NULL);
    mState.copyWriteBuffer.set(NULL);

    mState.pack.pixelBuffer.set(NULL);
    mState.unpack.pixelBuffer.set(NULL);

    mResourceManager->release();
}

void Context::makeCurrent(egl::Surface *surface)
{
    if (!mHasBeenCurrent)
    {
        mMajorShaderModel = mRenderer->getMajorShaderModel();
        mMaximumPointSize = mRenderer->getMaxPointSize();
        mSupportsVertexTexture = mRenderer->getVertexTextureSupport();
        mSupportsNonPower2Texture = mRenderer->getNonPower2TextureSupport();
        mSupportsInstancing = mRenderer->getInstancingSupport();

        mMaxViewportDimension = mRenderer->getMaxViewportDimension();
        mMax2DTextureDimension = std::min(std::min(mRenderer->getMaxTextureWidth(), mRenderer->getMaxTextureHeight()),
                                          (int)gl::IMPLEMENTATION_MAX_2D_TEXTURE_SIZE);
        mMaxCubeTextureDimension = std::min(mMax2DTextureDimension, (int)gl::IMPLEMENTATION_MAX_CUBE_MAP_TEXTURE_SIZE);
        mMax3DTextureDimension = std::min(std::min(mMax2DTextureDimension, mRenderer->getMaxTextureDepth()),
                                          (int)gl::IMPLEMENTATION_MAX_3D_TEXTURE_SIZE);
        mMax2DArrayTextureLayers = mRenderer->getMaxTextureArrayLayers();
        mMaxRenderbufferDimension = mMax2DTextureDimension;
        mMax2DTextureLevel = log2(mMax2DTextureDimension) + 1;
        mMaxCubeTextureLevel = log2(mMaxCubeTextureDimension) + 1;
        mMax3DTextureLevel = log2(mMax3DTextureDimension) + 1;
        mMax2DArrayTextureLevel = log2(mMax2DTextureDimension) + 1;
        mMaxTextureAnisotropy = mRenderer->getTextureMaxAnisotropy();
        TRACE("Max2DTextureDimension=%d, MaxCubeTextureDimension=%d, Max3DTextureDimension=%d, Max2DArrayTextureLayers = %d, "
              "Max2DTextureLevel=%d, MaxCubeTextureLevel=%d, Max3DTextureLevel=%d, Max2DArrayTextureLevel=%d, "
              "MaxRenderbufferDimension=%d, MaxTextureAnisotropy=%f",
              mMax2DTextureDimension, mMaxCubeTextureDimension, mMax3DTextureDimension, mMax2DArrayTextureLayers,
              mMax2DTextureLevel, mMaxCubeTextureLevel, mMax3DTextureLevel, mMax2DArrayTextureLevel,
              mMaxRenderbufferDimension, mMaxTextureAnisotropy);

        mSupportsEventQueries = mRenderer->getEventQuerySupport();
        mSupportsOcclusionQueries = mRenderer->getOcclusionQuerySupport();
        mSupportsBGRATextures = mRenderer->getBGRATextureSupport();
        mSupportsDXT1Textures = mRenderer->getDXT1TextureSupport();
        mSupportsDXT3Textures = mRenderer->getDXT3TextureSupport();
        mSupportsDXT5Textures = mRenderer->getDXT5TextureSupport();
        mSupportsFloat32Textures = mRenderer->getFloat32TextureSupport();
        mSupportsFloat32LinearFilter = mRenderer->getFloat32TextureFilteringSupport();
        mSupportsFloat32RenderableTextures = mRenderer->getFloat32TextureRenderingSupport();
        mSupportsFloat16Textures = mRenderer->getFloat16TextureSupport();
        mSupportsFloat16LinearFilter = mRenderer->getFloat16TextureFilteringSupport();
        mSupportsFloat16RenderableTextures = mRenderer->getFloat16TextureRenderingSupport();
        mSupportsLuminanceTextures = mRenderer->getLuminanceTextureSupport();
        mSupportsLuminanceAlphaTextures = mRenderer->getLuminanceAlphaTextureSupport();
        mSupportsRGTextures = mRenderer->getRGTextureSupport();
        mSupportsDepthTextures = mRenderer->getDepthTextureSupport();
        mSupportsTextureFilterAnisotropy = mRenderer->getTextureFilterAnisotropySupport();
        mSupports32bitIndices = mRenderer->get32BitIndexSupport();
        mSupportsPBOs = mRenderer->getPBOSupport();

        mNumCompressedTextureFormats = 0;
        if (supportsDXT1Textures())
        {
            mNumCompressedTextureFormats += 2;
        }
        if (supportsDXT3Textures())
        {
            mNumCompressedTextureFormats += 1;
        }
        if (supportsDXT5Textures())
        {
            mNumCompressedTextureFormats += 1;
        }

        initExtensionString();
        initRendererString();

        mState.viewport.x = 0;
        mState.viewport.y = 0;
        mState.viewport.width = surface->getWidth();
        mState.viewport.height = surface->getHeight();

        mState.scissor.x = 0;
        mState.scissor.y = 0;
        mState.scissor.width = surface->getWidth();
        mState.scissor.height = surface->getHeight();

        mHasBeenCurrent = true;
    }

    // Wrap the existing swapchain resources into GL objects and assign them to the '0' names
    rx::SwapChain *swapchain = surface->getSwapChain();

    Colorbuffer *colorbufferZero = new Colorbuffer(mRenderer, swapchain);
    DepthStencilbuffer *depthStencilbufferZero = new DepthStencilbuffer(mRenderer, swapchain);
    Framebuffer *framebufferZero = new DefaultFramebuffer(mRenderer, colorbufferZero, depthStencilbufferZero);

    setFramebufferZero(framebufferZero);

    // Store the current client version in the renderer
    mRenderer->setCurrentClientVersion(mClientVersion);
}

// NOTE: this function should not assume that this context is current!
void Context::markContextLost()
{
    if (mResetStrategy == GL_LOSE_CONTEXT_ON_RESET_EXT)
        mResetStatus = GL_UNKNOWN_CONTEXT_RESET_EXT;
    mContextLost = true;
}

bool Context::isContextLost()
{
    return mContextLost;
}

void Context::setCap(GLenum cap, bool enabled)
{
    switch (cap)
    {
      case GL_CULL_FACE:                     setCullFace(enabled);              break;
      case GL_POLYGON_OFFSET_FILL:           setPolygonOffsetFill(enabled);     break;
      case GL_SAMPLE_ALPHA_TO_COVERAGE:      setSampleAlphaToCoverage(enabled); break;
      case GL_SAMPLE_COVERAGE:               setSampleCoverage(enabled);        break;
      case GL_SCISSOR_TEST:                  setScissorTest(enabled);           break;
      case GL_STENCIL_TEST:                  setStencilTest(enabled);           break;
      case GL_DEPTH_TEST:                    setDepthTest(enabled);             break;
      case GL_BLEND:                         setBlend(enabled);                 break;
      case GL_DITHER:                        setDither(enabled);                break;
      case GL_PRIMITIVE_RESTART_FIXED_INDEX: UNIMPLEMENTED();                   break;
      case GL_RASTERIZER_DISCARD:            setRasterizerDiscard(enabled);     break;
      default:                               UNREACHABLE();
    }
}

bool Context::getCap(GLenum cap)
{
    switch (cap)
    {
      case GL_CULL_FACE:                     return isCullFaceEnabled();
      case GL_POLYGON_OFFSET_FILL:           return isPolygonOffsetFillEnabled();
      case GL_SAMPLE_ALPHA_TO_COVERAGE:      return isSampleAlphaToCoverageEnabled();
      case GL_SAMPLE_COVERAGE:               return isSampleCoverageEnabled();
      case GL_SCISSOR_TEST:                  return isScissorTestEnabled();
      case GL_STENCIL_TEST:                  return isStencilTestEnabled();
      case GL_DEPTH_TEST:                    return isDepthTestEnabled();
      case GL_BLEND:                         return isBlendEnabled();
      case GL_DITHER:                        return isDitherEnabled();
      case GL_PRIMITIVE_RESTART_FIXED_INDEX: UNIMPLEMENTED(); return false;
      case GL_RASTERIZER_DISCARD:            return isRasterizerDiscardEnabled();
      default:                               UNREACHABLE(); return false;
    }
}

void Context::setClearColor(float red, float green, float blue, float alpha)
{
    mState.colorClearValue.red = red;
    mState.colorClearValue.green = green;
    mState.colorClearValue.blue = blue;
    mState.colorClearValue.alpha = alpha;
}

void Context::setClearDepth(float depth)
{
    mState.depthClearValue = depth;
}

void Context::setClearStencil(int stencil)
{
    mState.stencilClearValue = stencil;
}

void Context::setRasterizerDiscard(bool enabled)
{
    mState.rasterizer.rasterizerDiscard = enabled;
}

bool Context::isRasterizerDiscardEnabled() const
{
    return mState.rasterizer.rasterizerDiscard;
}

void Context::setCullFace(bool enabled)
{
    mState.rasterizer.cullFace = enabled;
}

bool Context::isCullFaceEnabled() const
{
    return mState.rasterizer.cullFace;
}

void Context::setCullMode(GLenum mode)
{
    mState.rasterizer.cullMode = mode;
}

void Context::setFrontFace(GLenum front)
{
    mState.rasterizer.frontFace = front;
}

void Context::setDepthTest(bool enabled)
{
    mState.depthStencil.depthTest = enabled;
}

bool Context::isDepthTestEnabled() const
{
    return mState.depthStencil.depthTest;
}

void Context::setDepthFunc(GLenum depthFunc)
{
     mState.depthStencil.depthFunc = depthFunc;
}

void Context::setDepthRange(float zNear, float zFar)
{
    mState.zNear = zNear;
    mState.zFar = zFar;
}

void Context::setBlend(bool enabled)
{
    mState.blend.blend = enabled;
}

bool Context::isBlendEnabled() const
{
    return mState.blend.blend;
}

void Context::setBlendFactors(GLenum sourceRGB, GLenum destRGB, GLenum sourceAlpha, GLenum destAlpha)
{
    mState.blend.sourceBlendRGB = sourceRGB;
    mState.blend.destBlendRGB = destRGB;
    mState.blend.sourceBlendAlpha = sourceAlpha;
    mState.blend.destBlendAlpha = destAlpha;
}

void Context::setBlendColor(float red, float green, float blue, float alpha)
{
    mState.blendColor.red = red;
    mState.blendColor.green = green;
    mState.blendColor.blue = blue;
    mState.blendColor.alpha = alpha;
}

void Context::setBlendEquation(GLenum rgbEquation, GLenum alphaEquation)
{
    mState.blend.blendEquationRGB = rgbEquation;
    mState.blend.blendEquationAlpha = alphaEquation;
}

void Context::setStencilTest(bool enabled)
{
    mState.depthStencil.stencilTest = enabled;
}

bool Context::isStencilTestEnabled() const
{
    return mState.depthStencil.stencilTest;
}

void Context::setStencilParams(GLenum stencilFunc, GLint stencilRef, GLuint stencilMask)
{
    mState.depthStencil.stencilFunc = stencilFunc;
    mState.stencilRef = (stencilRef > 0) ? stencilRef : 0;
    mState.depthStencil.stencilMask = stencilMask;
}

void Context::setStencilBackParams(GLenum stencilBackFunc, GLint stencilBackRef, GLuint stencilBackMask)
{
    mState.depthStencil.stencilBackFunc = stencilBackFunc;
    mState.stencilBackRef = (stencilBackRef > 0) ? stencilBackRef : 0;
    mState.depthStencil.stencilBackMask = stencilBackMask;
}

void Context::setStencilWritemask(GLuint stencilWritemask)
{
    mState.depthStencil.stencilWritemask = stencilWritemask;
}

void Context::setStencilBackWritemask(GLuint stencilBackWritemask)
{
    mState.depthStencil.stencilBackWritemask = stencilBackWritemask;
}

void Context::setStencilOperations(GLenum stencilFail, GLenum stencilPassDepthFail, GLenum stencilPassDepthPass)
{
    mState.depthStencil.stencilFail = stencilFail;
    mState.depthStencil.stencilPassDepthFail = stencilPassDepthFail;
    mState.depthStencil.stencilPassDepthPass = stencilPassDepthPass;
}

void Context::setStencilBackOperations(GLenum stencilBackFail, GLenum stencilBackPassDepthFail, GLenum stencilBackPassDepthPass)
{
    mState.depthStencil.stencilBackFail = stencilBackFail;
    mState.depthStencil.stencilBackPassDepthFail = stencilBackPassDepthFail;
    mState.depthStencil.stencilBackPassDepthPass = stencilBackPassDepthPass;
}

void Context::setPolygonOffsetFill(bool enabled)
{
     mState.rasterizer.polygonOffsetFill = enabled;
}

bool Context::isPolygonOffsetFillEnabled() const
{
    return mState.rasterizer.polygonOffsetFill;
}

void Context::setPolygonOffsetParams(GLfloat factor, GLfloat units)
{
    // An application can pass NaN values here, so handle this gracefully
    mState.rasterizer.polygonOffsetFactor = factor != factor ? 0.0f : factor;
    mState.rasterizer.polygonOffsetUnits = units != units ? 0.0f : units;
}

void Context::setSampleAlphaToCoverage(bool enabled)
{
    mState.blend.sampleAlphaToCoverage = enabled;
}

bool Context::isSampleAlphaToCoverageEnabled() const
{
    return mState.blend.sampleAlphaToCoverage;
}

void Context::setSampleCoverage(bool enabled)
{
    mState.sampleCoverage = enabled;
}

bool Context::isSampleCoverageEnabled() const
{
    return mState.sampleCoverage;
}

void Context::setSampleCoverageParams(GLclampf value, bool invert)
{
    mState.sampleCoverageValue = value;
    mState.sampleCoverageInvert = invert;
}

void Context::setScissorTest(bool enabled)
{
    mState.scissorTest = enabled;
}

bool Context::isScissorTestEnabled() const
{
    return mState.scissorTest;
}

void Context::setDither(bool enabled)
{
    mState.blend.dither = enabled;
}

bool Context::isDitherEnabled() const
{
    return mState.blend.dither;
}

void Context::setLineWidth(GLfloat width)
{
    mState.lineWidth = width;
}

void Context::setGenerateMipmapHint(GLenum hint)
{
    mState.generateMipmapHint = hint;
}

void Context::setFragmentShaderDerivativeHint(GLenum hint)
{
    mState.fragmentShaderDerivativeHint = hint;
    // TODO: Propagate the hint to shader translator so we can write
    // ddx, ddx_coarse, or ddx_fine depending on the hint.
    // Ignore for now. It is valid for implementations to ignore hint.
}

void Context::setViewportParams(GLint x, GLint y, GLsizei width, GLsizei height)
{
    mState.viewport.x = x;
    mState.viewport.y = y;
    mState.viewport.width = width;
    mState.viewport.height = height;
}

void Context::setScissorParams(GLint x, GLint y, GLsizei width, GLsizei height)
{
    mState.scissor.x = x;
    mState.scissor.y = y;
    mState.scissor.width = width;
    mState.scissor.height = height;
}

void Context::getScissorParams(GLint *x, GLint *y, GLsizei *width, GLsizei *height)
{
    *x = mState.scissor.x;
    *y = mState.scissor.y;
    *width = mState.scissor.width;
    *height = mState.scissor.height;
}

void Context::setColorMask(bool red, bool green, bool blue, bool alpha)
{
    mState.blend.colorMaskRed = red;
    mState.blend.colorMaskGreen = green;
    mState.blend.colorMaskBlue = blue;
    mState.blend.colorMaskAlpha = alpha;
}

void Context::setDepthMask(bool mask)
{
    mState.depthStencil.depthMask = mask;
}

void Context::setActiveSampler(unsigned int active)
{
    mState.activeSampler = active;
}

GLuint Context::getReadFramebufferHandle() const
{
    return mState.readFramebuffer;
}

GLuint Context::getDrawFramebufferHandle() const
{
    return mState.drawFramebuffer;
}

GLuint Context::getRenderbufferHandle() const
{
    return mState.renderbuffer.id();
}

GLuint Context::getVertexArrayHandle() const
{
    return mState.vertexArray;
}

GLuint Context::getSamplerHandle(GLuint textureUnit) const
{
    ASSERT(textureUnit < ArraySize(mState.samplers));
    return mState.samplers[textureUnit];
}

GLuint Context::getArrayBufferHandle() const
{
    return mState.arrayBuffer.id();
}

GLuint Context::getActiveQuery(GLenum target) const
{
    // All query types should already exist in the activeQueries map
    ASSERT(mState.activeQueries.find(target) != mState.activeQueries.end());

    const Query *queryObject = mState.activeQueries.at(target).get();
    return queryObject ? queryObject->id() : 0;
}

void Context::setEnableVertexAttribArray(unsigned int attribNum, bool enabled)
{
    getCurrentVertexArray()->enableAttribute(attribNum, enabled);
}

const VertexAttribute &Context::getVertexAttribState(unsigned int attribNum) const
{
    return getCurrentVertexArray()->getVertexAttribute(attribNum);
}

const VertexAttribCurrentValueData &Context::getVertexAttribCurrentValue(unsigned int attribNum) const
{
    ASSERT(attribNum < MAX_VERTEX_ATTRIBS);
    return mState.vertexAttribCurrentValues[attribNum];
}

void Context::setVertexAttribState(unsigned int attribNum, Buffer *boundBuffer, GLint size, GLenum type, bool normalized,
                                   bool pureInteger, GLsizei stride, const void *pointer)
{
    getCurrentVertexArray()->setAttributeState(attribNum, boundBuffer, size, type, normalized, pureInteger, stride, pointer);
}

const void *Context::getVertexAttribPointer(unsigned int attribNum) const
{
    return getCurrentVertexArray()->getVertexAttribute(attribNum).mPointer;
}

void Context::setPackAlignment(GLint alignment)
{
    mState.pack.alignment = alignment;
}

GLint Context::getPackAlignment() const
{
    return mState.pack.alignment;
}

void Context::setUnpackAlignment(GLint alignment)
{
    mState.unpack.alignment = alignment;
}

GLint Context::getUnpackAlignment() const
{
    return mState.unpack.alignment;
}

void Context::setPackReverseRowOrder(bool reverseRowOrder)
{
    mState.pack.reverseRowOrder = reverseRowOrder;
}

bool Context::getPackReverseRowOrder() const
{
    return mState.pack.reverseRowOrder;
}

const PixelUnpackState &Context::getUnpackState() const
{
    return mState.unpack;
}

const PixelPackState &Context::getPackState() const
{
    return mState.pack;
}

GLuint Context::createBuffer()
{
    return mResourceManager->createBuffer();
}

GLuint Context::createProgram()
{
    return mResourceManager->createProgram();
}

GLuint Context::createShader(GLenum type)
{
    return mResourceManager->createShader(type);
}

GLuint Context::createTexture()
{
    return mResourceManager->createTexture();
}

GLuint Context::createRenderbuffer()
{
    return mResourceManager->createRenderbuffer();
}

GLsync Context::createFenceSync(GLenum condition)
{
    GLuint handle = mResourceManager->createFenceSync();

    gl::FenceSync *fenceSync = mResourceManager->getFenceSync(handle);
    ASSERT(fenceSync);

    fenceSync->set(condition);

    return reinterpret_cast<GLsync>(handle);
}

GLuint Context::createVertexArray()
{
    GLuint handle = mVertexArrayHandleAllocator.allocate();

    // Although the spec states VAO state is not initialized until the object is bound,
    // we create it immediately. The resulting behaviour is transparent to the application,
    // since it's not currently possible to access the state until the object is bound.
    mVertexArrayMap[handle] = new VertexArray(mRenderer, handle);

    return handle;
}

GLuint Context::createSampler()
{
    return mResourceManager->createSampler();
}

GLuint Context::createTransformFeedback()
{
    GLuint handle = mTransformFeedbackAllocator.allocate();
    TransformFeedback *transformFeedback = new TransformFeedback(handle);
    transformFeedback->addRef();
    mTransformFeedbackMap[handle] = transformFeedback;
    return handle;
}

// Returns an unused framebuffer name
GLuint Context::createFramebuffer()
{
    GLuint handle = mFramebufferHandleAllocator.allocate();

    mFramebufferMap[handle] = NULL;

    return handle;
}

GLuint Context::createFenceNV()
{
    GLuint handle = mFenceNVHandleAllocator.allocate();

    mFenceNVMap[handle] = new FenceNV(mRenderer);

    return handle;
}

// Returns an unused query name
GLuint Context::createQuery()
{
    GLuint handle = mQueryHandleAllocator.allocate();

    mQueryMap[handle] = NULL;

    return handle;
}

void Context::deleteBuffer(GLuint buffer)
{
    if (mResourceManager->getBuffer(buffer))
    {
        detachBuffer(buffer);
    }
    
    mResourceManager->deleteBuffer(buffer);
}

void Context::deleteShader(GLuint shader)
{
    mResourceManager->deleteShader(shader);
}

void Context::deleteProgram(GLuint program)
{
    mResourceManager->deleteProgram(program);
}

void Context::deleteTexture(GLuint texture)
{
    if (mResourceManager->getTexture(texture))
    {
        detachTexture(texture);
    }

    mResourceManager->deleteTexture(texture);
}

void Context::deleteRenderbuffer(GLuint renderbuffer)
{
    if (mResourceManager->getRenderbuffer(renderbuffer))
    {
        detachRenderbuffer(renderbuffer);
    }
    
    mResourceManager->deleteRenderbuffer(renderbuffer);
}

void Context::deleteFenceSync(GLsync fenceSync)
{
    // The spec specifies the underlying Fence object is not deleted until all current
    // wait commands finish. However, since the name becomes invalid, we cannot query the fence,
    // and since our API is currently designed for being called from a single thread, we can delete
    // the fence immediately.
    mResourceManager->deleteFenceSync(reinterpret_cast<GLuint>(fenceSync));
}

void Context::deleteVertexArray(GLuint vertexArray)
{
    auto vertexArrayObject = mVertexArrayMap.find(vertexArray);

    if (vertexArrayObject != mVertexArrayMap.end())
    {
        detachVertexArray(vertexArray);

        mVertexArrayHandleAllocator.release(vertexArrayObject->first);
        delete vertexArrayObject->second;
        mVertexArrayMap.erase(vertexArrayObject);
    }
}

void Context::deleteSampler(GLuint sampler)
{
    if (mResourceManager->getSampler(sampler))
    {
        detachSampler(sampler);
    }

    mResourceManager->deleteSampler(sampler);
}

void Context::deleteTransformFeedback(GLuint transformFeedback)
{
    TransformFeedbackMap::const_iterator iter = mTransformFeedbackMap.find(transformFeedback);
    if (iter != mTransformFeedbackMap.end())
    {
        detachTransformFeedback(transformFeedback);
        mTransformFeedbackAllocator.release(transformFeedback);
        iter->second->release();
        mTransformFeedbackMap.erase(iter);
    }
}

void Context::deleteFramebuffer(GLuint framebuffer)
{
    FramebufferMap::iterator framebufferObject = mFramebufferMap.find(framebuffer);

    if (framebufferObject != mFramebufferMap.end())
    {
        detachFramebuffer(framebuffer);

        mFramebufferHandleAllocator.release(framebufferObject->first);
        delete framebufferObject->second;
        mFramebufferMap.erase(framebufferObject);
    }
}

void Context::deleteFenceNV(GLuint fence)
{
    FenceNVMap::iterator fenceObject = mFenceNVMap.find(fence);

    if (fenceObject != mFenceNVMap.end())
    {
        mFenceNVHandleAllocator.release(fenceObject->first);
        delete fenceObject->second;
        mFenceNVMap.erase(fenceObject);
    }
}

void Context::deleteQuery(GLuint query)
{
    QueryMap::iterator queryObject = mQueryMap.find(query);
    if (queryObject != mQueryMap.end())
    {
        mQueryHandleAllocator.release(queryObject->first);
        if (queryObject->second)
        {
            queryObject->second->release();
        }
        mQueryMap.erase(queryObject);
    }
}

Buffer *Context::getBuffer(GLuint handle)
{
    return mResourceManager->getBuffer(handle);
}

Shader *Context::getShader(GLuint handle) const
{
    return mResourceManager->getShader(handle);
}

Program *Context::getProgram(GLuint handle) const
{
    return mResourceManager->getProgram(handle);
}

Texture *Context::getTexture(GLuint handle)
{
    return mResourceManager->getTexture(handle);
}

Renderbuffer *Context::getRenderbuffer(GLuint handle)
{
    return mResourceManager->getRenderbuffer(handle);
}

FenceSync *Context::getFenceSync(GLsync handle) const
{
    return mResourceManager->getFenceSync(reinterpret_cast<GLuint>(handle));
}

VertexArray *Context::getVertexArray(GLuint handle) const
{
    auto vertexArray = mVertexArrayMap.find(handle);

    if (vertexArray == mVertexArrayMap.end())
    {
        return NULL;
    }
    else
    {
        return vertexArray->second;
    }
}

Sampler *Context::getSampler(GLuint handle) const
{
    return mResourceManager->getSampler(handle);
}

TransformFeedback *Context::getTransformFeedback(GLuint handle) const
{
    if (handle == 0)
    {
        return mTransformFeedbackZero.get();
    }
    else
    {
        TransformFeedbackMap::const_iterator iter = mTransformFeedbackMap.find(handle);
        return (iter != mTransformFeedbackMap.end()) ? iter->second : NULL;
    }
}

Framebuffer *Context::getReadFramebuffer()
{
    return getFramebuffer(mState.readFramebuffer);
}

Framebuffer *Context::getDrawFramebuffer()
{
    return mBoundDrawFramebuffer;
}

VertexArray *Context::getCurrentVertexArray() const
{
    VertexArray *vao = getVertexArray(mState.vertexArray);
    ASSERT(vao != NULL);
    return vao;
}

TransformFeedback *Context::getCurrentTransformFeedback() const
{
    return mState.transformFeedback.get();
}

bool Context::isSampler(GLuint samplerName) const
{
    return mResourceManager->isSampler(samplerName);
}

void Context::bindArrayBuffer(unsigned int buffer)
{
    mResourceManager->checkBufferAllocation(buffer);

    mState.arrayBuffer.set(getBuffer(buffer));
}

void Context::bindElementArrayBuffer(unsigned int buffer)
{
    mResourceManager->checkBufferAllocation(buffer);

    getCurrentVertexArray()->setElementArrayBuffer(getBuffer(buffer));
}

void Context::bindTexture2D(GLuint texture)
{
    mResourceManager->checkTextureAllocation(texture, TEXTURE_2D);

    mState.samplerTexture[TEXTURE_2D][mState.activeSampler].set(getTexture(texture));
}

void Context::bindTextureCubeMap(GLuint texture)
{
    mResourceManager->checkTextureAllocation(texture, TEXTURE_CUBE);

    mState.samplerTexture[TEXTURE_CUBE][mState.activeSampler].set(getTexture(texture));
}

void Context::bindTexture3D(GLuint texture)
{
    mResourceManager->checkTextureAllocation(texture, TEXTURE_3D);

    mState.samplerTexture[TEXTURE_3D][mState.activeSampler].set(getTexture(texture));
}

void Context::bindTexture2DArray(GLuint texture)
{
    mResourceManager->checkTextureAllocation(texture, TEXTURE_2D_ARRAY);

    mState.samplerTexture[TEXTURE_2D_ARRAY][mState.activeSampler].set(getTexture(texture));
}

void Context::bindReadFramebuffer(GLuint framebuffer)
{
    if (!getFramebuffer(framebuffer))
    {
        mFramebufferMap[framebuffer] = new Framebuffer(mRenderer);
    }

    mState.readFramebuffer = framebuffer;
}

void Context::bindDrawFramebuffer(GLuint framebuffer)
{
    if (!getFramebuffer(framebuffer))
    {
        mFramebufferMap[framebuffer] = new Framebuffer(mRenderer);
    }

    mState.drawFramebuffer = framebuffer;

    mBoundDrawFramebuffer = getFramebuffer(framebuffer);
}

void Context::bindRenderbuffer(GLuint renderbuffer)
{
    mResourceManager->checkRenderbufferAllocation(renderbuffer);

    mState.renderbuffer.set(getRenderbuffer(renderbuffer));
}

void Context::bindVertexArray(GLuint vertexArray)
{
    if (!getVertexArray(vertexArray))
    {
        mVertexArrayMap[vertexArray] = new VertexArray(mRenderer, vertexArray);
    }

    mState.vertexArray = vertexArray;
}

void Context::bindSampler(GLuint textureUnit, GLuint sampler)
{
    ASSERT(textureUnit < ArraySize(mState.samplers));
    mResourceManager->checkSamplerAllocation(sampler);

    mState.samplers[textureUnit] = sampler;
}

void Context::bindGenericUniformBuffer(GLuint buffer)
{
    mResourceManager->checkBufferAllocation(buffer);

    mState.genericUniformBuffer.set(getBuffer(buffer));
}

void Context::bindIndexedUniformBuffer(GLuint buffer, GLuint index, GLintptr offset, GLsizeiptr size)
{
    mResourceManager->checkBufferAllocation(buffer);

    mState.uniformBuffers[index].set(getBuffer(buffer), offset, size);
}

void Context::bindGenericTransformFeedbackBuffer(GLuint buffer)
{
    mResourceManager->checkBufferAllocation(buffer);

    mState.genericTransformFeedbackBuffer.set(getBuffer(buffer));
}

void Context::bindIndexedTransformFeedbackBuffer(GLuint buffer, GLuint index, GLintptr offset, GLsizeiptr size)
{
    mResourceManager->checkBufferAllocation(buffer);

    mState.transformFeedbackBuffers[index].set(getBuffer(buffer), offset, size);
}

void Context::bindCopyReadBuffer(GLuint buffer)
{
    mResourceManager->checkBufferAllocation(buffer);

    mState.copyReadBuffer.set(getBuffer(buffer));
}

void Context::bindCopyWriteBuffer(GLuint buffer)
{
    mResourceManager->checkBufferAllocation(buffer);

    mState.copyWriteBuffer.set(getBuffer(buffer));
}

void Context::bindPixelPackBuffer(GLuint buffer)
{
    mResourceManager->checkBufferAllocation(buffer);

    mState.pack.pixelBuffer.set(getBuffer(buffer));
}

void Context::bindPixelUnpackBuffer(GLuint buffer)
{
    mResourceManager->checkBufferAllocation(buffer);

    mState.unpack.pixelBuffer.set(getBuffer(buffer));
}

void Context::useProgram(GLuint program)
{
    GLuint priorProgram = mState.currentProgram;
    mState.currentProgram = program;               // Must switch before trying to delete, otherwise it only gets flagged.

    if (priorProgram != program)
    {
        Program *newProgram = mResourceManager->getProgram(program);
        Program *oldProgram = mResourceManager->getProgram(priorProgram);
        mCurrentProgramBinary.set(NULL);

        if (newProgram)
        {
            newProgram->addRef();
            mCurrentProgramBinary.set(newProgram->getProgramBinary());
        }
        
        if (oldProgram)
        {
            oldProgram->release();
        }
    }
}

void Context::linkProgram(GLuint program)
{
    Program *programObject = mResourceManager->getProgram(program);

    bool linked = programObject->link();

    // if the current program was relinked successfully we
    // need to install the new executables
    if (linked && program == mState.currentProgram)
    {
        mCurrentProgramBinary.set(programObject->getProgramBinary());
    }
}

void Context::setProgramBinary(GLuint program, const void *binary, GLint length)
{
    Program *programObject = mResourceManager->getProgram(program);

    bool loaded = programObject->setProgramBinary(binary, length);

    // if the current program was reloaded successfully we
    // need to install the new executables
    if (loaded && program == mState.currentProgram)
    {
        mCurrentProgramBinary.set(programObject->getProgramBinary());
    }

}

void Context::bindTransformFeedback(GLuint transformFeedback)
{
    TransformFeedback *transformFeedbackObject = getTransformFeedback(transformFeedback);
    mState.transformFeedback.set(transformFeedbackObject);
}

void Context::beginQuery(GLenum target, GLuint query)
{
    // From EXT_occlusion_query_boolean: If BeginQueryEXT is called with an <id>  
    // of zero, if the active query object name for <target> is non-zero (for the  
    // targets ANY_SAMPLES_PASSED_EXT and ANY_SAMPLES_PASSED_CONSERVATIVE_EXT, if  
    // the active query for either target is non-zero), if <id> is the name of an 
    // existing query object whose type does not match <target>, or if <id> is the
    // active query object name for any query type, the error INVALID_OPERATION is
    // generated.

    // Ensure no other queries are active
    // NOTE: If other queries than occlusion are supported, we will need to check
    // separately that:
    //    a) The query ID passed is not the current active query for any target/type
    //    b) There are no active queries for the requested target (and in the case
    //       of GL_ANY_SAMPLES_PASSED_EXT and GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT,
    //       no query may be active for either if glBeginQuery targets either.
    for (State::ActiveQueryMap::iterator i = mState.activeQueries.begin(); i != mState.activeQueries.end(); i++)
    {
        if (i->second.get() != NULL)
        {
            return gl::error(GL_INVALID_OPERATION);
        }
    }

    Query *queryObject = getQuery(query, true, target);

    // check that name was obtained with glGenQueries
    if (!queryObject)
    {
        return gl::error(GL_INVALID_OPERATION);
    }

    // check for type mismatch
    if (queryObject->getType() != target)
    {
        return gl::error(GL_INVALID_OPERATION);
    }

    // set query as active for specified target
    mState.activeQueries[target].set(queryObject);

    // begin query
    queryObject->begin();
}

void Context::endQuery(GLenum target)
{
    Query *queryObject = mState.activeQueries[target].get();

    if (queryObject == NULL)
    {
        return gl::error(GL_INVALID_OPERATION);
    }

    queryObject->end();

    mState.activeQueries[target].set(NULL);
}

void Context::setFramebufferZero(Framebuffer *buffer)
{
    delete mFramebufferMap[0];
    mFramebufferMap[0] = buffer;
    if (mState.drawFramebuffer == 0)
    {
        mBoundDrawFramebuffer = buffer;
    }
}

void Context::setRenderbufferStorage(GLsizei width, GLsizei height, GLenum internalformat, GLsizei samples)
{
    const bool color = gl::IsColorRenderingSupported(internalformat, this);
    const bool depth = gl::IsDepthRenderingSupported(internalformat, this);
    const bool stencil = gl::IsStencilRenderingSupported(internalformat, this);

    RenderbufferStorage *renderbuffer = NULL;

    if (color)
    {
        renderbuffer = new gl::Colorbuffer(mRenderer,width, height, internalformat, samples);
    }
    else if (depth && stencil)
    {
        renderbuffer = new gl::DepthStencilbuffer(mRenderer, width, height, samples);
    }
    else if (depth)
    {
        renderbuffer = new gl::Depthbuffer(mRenderer, width, height, samples);
    }
    else if (stencil)
    {
        renderbuffer = new gl::Stencilbuffer(mRenderer, width, height, samples);
    }
    else
    {
        UNREACHABLE();
        return;
    }

    Renderbuffer *renderbufferObject = mState.renderbuffer.get();
    renderbufferObject->setStorage(renderbuffer);
}

Framebuffer *Context::getFramebuffer(unsigned int handle) const
{
    FramebufferMap::const_iterator framebuffer = mFramebufferMap.find(handle);

    if (framebuffer == mFramebufferMap.end())
    {
        return NULL;
    }
    else
    {
        return framebuffer->second;
    }
}

FenceNV *Context::getFenceNV(unsigned int handle)
{
    FenceNVMap::iterator fence = mFenceNVMap.find(handle);

    if (fence == mFenceNVMap.end())
    {
        return NULL;
    }
    else
    {
        return fence->second;
    }
}

Query *Context::getQuery(unsigned int handle, bool create, GLenum type)
{
    QueryMap::iterator query = mQueryMap.find(handle);

    if (query == mQueryMap.end())
    {
        return NULL;
    }
    else
    {
        if (!query->second && create)
        {
            query->second = new Query(mRenderer, type, handle);
            query->second->addRef();
        }
        return query->second;
    }
}

Buffer *Context::getTargetBuffer(GLenum target) const
{
    switch (target)
    {
      case GL_ARRAY_BUFFER:              return mState.arrayBuffer.get();
      case GL_COPY_READ_BUFFER:          return mState.copyReadBuffer.get();
      case GL_COPY_WRITE_BUFFER:         return mState.copyWriteBuffer.get();
      case GL_ELEMENT_ARRAY_BUFFER:      return getCurrentVertexArray()->getElementArrayBuffer();
      case GL_PIXEL_PACK_BUFFER:         return mState.pack.pixelBuffer.get();
      case GL_PIXEL_UNPACK_BUFFER:       return mState.unpack.pixelBuffer.get();
      case GL_TRANSFORM_FEEDBACK_BUFFER: return mState.genericTransformFeedbackBuffer.get();
      case GL_UNIFORM_BUFFER:            return mState.genericUniformBuffer.get();
      default: UNREACHABLE();            return NULL;
    }
}

Buffer *Context::getArrayBuffer()
{
    return mState.arrayBuffer.get();
}

Buffer *Context::getElementArrayBuffer() const
{
    return getCurrentVertexArray()->getElementArrayBuffer();
}

ProgramBinary *Context::getCurrentProgramBinary()
{
    return mCurrentProgramBinary.get();
}

Texture *Context::getTargetTexture(GLenum target) const
{
    if (!ValidTextureTarget(this, target))
    {
        return NULL;
    }

    switch (target)
    {
      case GL_TEXTURE_2D:       return getTexture2D();
      case GL_TEXTURE_CUBE_MAP: return getTextureCubeMap();
      case GL_TEXTURE_3D:       return getTexture3D();
      case GL_TEXTURE_2D_ARRAY: return getTexture2DArray();
      default:                  return NULL;
    }
}

GLuint Context::getTargetFramebufferHandle(GLenum target) const
{
    if (!ValidFramebufferTarget(target))
    {
        return GL_INVALID_INDEX;
    }

    if (target == GL_READ_FRAMEBUFFER_ANGLE)
    {
        return mState.readFramebuffer;
    }
    else
    {
        return mState.drawFramebuffer;
    }
}

Framebuffer *Context::getTargetFramebuffer(GLenum target) const
{
    GLuint framebufferHandle = getTargetFramebufferHandle(target);
    return (framebufferHandle == GL_INVALID_INDEX ? NULL : getFramebuffer(framebufferHandle));
}

Texture2D *Context::getTexture2D() const
{
    return static_cast<Texture2D*>(getSamplerTexture(mState.activeSampler, TEXTURE_2D));
}

TextureCubeMap *Context::getTextureCubeMap() const
{
    return static_cast<TextureCubeMap*>(getSamplerTexture(mState.activeSampler, TEXTURE_CUBE));
}

Texture3D *Context::getTexture3D() const
{
    return static_cast<Texture3D*>(getSamplerTexture(mState.activeSampler, TEXTURE_3D));
}

Texture2DArray *Context::getTexture2DArray() const
{
    return static_cast<Texture2DArray*>(getSamplerTexture(mState.activeSampler, TEXTURE_2D_ARRAY));
}

Buffer *Context::getGenericUniformBuffer()
{
    return mState.genericUniformBuffer.get();
}

Buffer *Context::getGenericTransformFeedbackBuffer()
{
    return mState.genericTransformFeedbackBuffer.get();
}

Buffer *Context::getCopyReadBuffer()
{
    return mState.copyReadBuffer.get();
}

Buffer *Context::getCopyWriteBuffer()
{
    return mState.copyWriteBuffer.get();
}

Buffer *Context::getPixelPackBuffer()
{
    return mState.pack.pixelBuffer.get();
}

Buffer *Context::getPixelUnpackBuffer()
{
    return mState.unpack.pixelBuffer.get();
}

Texture *Context::getSamplerTexture(unsigned int sampler, TextureType type) const
{
    GLuint texid = mState.samplerTexture[type][sampler].id();

    if (texid == 0)   // Special case: 0 refers to different initial textures based on the target
    {
        switch (type)
        {
          default: UNREACHABLE();
          case TEXTURE_2D:       return mTexture2DZero.get();
          case TEXTURE_CUBE:     return mTextureCubeMapZero.get();
          case TEXTURE_3D:       return mTexture3DZero.get();
          case TEXTURE_2D_ARRAY: return mTexture2DArrayZero.get();
        }
    }

    return mState.samplerTexture[type][sampler].get();
}

bool Context::getBooleanv(GLenum pname, GLboolean *params)
{
    switch (pname)
    {
      case GL_SHADER_COMPILER:           *params = GL_TRUE;                             break;
      case GL_SAMPLE_COVERAGE_INVERT:    *params = mState.sampleCoverageInvert;         break;
      case GL_DEPTH_WRITEMASK:           *params = mState.depthStencil.depthMask;       break;
      case GL_COLOR_WRITEMASK:
        params[0] = mState.blend.colorMaskRed;
        params[1] = mState.blend.colorMaskGreen;
        params[2] = mState.blend.colorMaskBlue;
        params[3] = mState.blend.colorMaskAlpha;
        break;
      case GL_CULL_FACE:                 *params = mState.rasterizer.cullFace;          break;
      case GL_POLYGON_OFFSET_FILL:       *params = mState.rasterizer.polygonOffsetFill; break;
      case GL_SAMPLE_ALPHA_TO_COVERAGE:  *params = mState.blend.sampleAlphaToCoverage;  break;
      case GL_SAMPLE_COVERAGE:           *params = mState.sampleCoverage;               break;
      case GL_SCISSOR_TEST:              *params = mState.scissorTest;                  break;
      case GL_STENCIL_TEST:              *params = mState.depthStencil.stencilTest;     break;
      case GL_DEPTH_TEST:                *params = mState.depthStencil.depthTest;       break;
      case GL_BLEND:                     *params = mState.blend.blend;                  break;
      case GL_DITHER:                    *params = mState.blend.dither;                 break;
      case GL_CONTEXT_ROBUST_ACCESS_EXT: *params = mRobustAccess ? GL_TRUE : GL_FALSE;  break;
      case GL_TRANSFORM_FEEDBACK_ACTIVE: *params = getCurrentTransformFeedback()->isStarted(); break;
      case GL_TRANSFORM_FEEDBACK_PAUSED: *params = getCurrentTransformFeedback()->isPaused();  break;
      default:
        return false;
    }

    return true;
}

bool Context::getFloatv(GLenum pname, GLfloat *params)
{
    // Please note: DEPTH_CLEAR_VALUE is included in our internal getFloatv implementation
    // because it is stored as a float, despite the fact that the GL ES 2.0 spec names
    // GetIntegerv as its native query function. As it would require conversion in any
    // case, this should make no difference to the calling application.
    switch (pname)
    {
      case GL_LINE_WIDTH:               *params = mState.lineWidth;                         break;
      case GL_SAMPLE_COVERAGE_VALUE:    *params = mState.sampleCoverageValue;               break;
      case GL_DEPTH_CLEAR_VALUE:        *params = mState.depthClearValue;                   break;
      case GL_POLYGON_OFFSET_FACTOR:    *params = mState.rasterizer.polygonOffsetFactor;    break;
      case GL_POLYGON_OFFSET_UNITS:     *params = mState.rasterizer.polygonOffsetUnits;     break;
      case GL_ALIASED_LINE_WIDTH_RANGE:
        params[0] = gl::ALIASED_LINE_WIDTH_RANGE_MIN;
        params[1] = gl::ALIASED_LINE_WIDTH_RANGE_MAX;
        break;
      case GL_ALIASED_POINT_SIZE_RANGE:
        params[0] = gl::ALIASED_POINT_SIZE_RANGE_MIN;
        params[1] = getMaximumPointSize();
        break;
      case GL_DEPTH_RANGE:
        params[0] = mState.zNear;
        params[1] = mState.zFar;
        break;
      case GL_COLOR_CLEAR_VALUE:
        params[0] = mState.colorClearValue.red;
        params[1] = mState.colorClearValue.green;
        params[2] = mState.colorClearValue.blue;
        params[3] = mState.colorClearValue.alpha;
        break;
      case GL_BLEND_COLOR:
        params[0] = mState.blendColor.red;
        params[1] = mState.blendColor.green;
        params[2] = mState.blendColor.blue;
        params[3] = mState.blendColor.alpha;
        break;
      case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT:
        if (!supportsTextureFilterAnisotropy())
        {
            return false;
        }
        *params = mMaxTextureAnisotropy;
        break;
      default:
        return false;
    }

    return true;
}

bool Context::getIntegerv(GLenum pname, GLint *params)
{
    if (pname >= GL_DRAW_BUFFER0_EXT && pname <= GL_DRAW_BUFFER15_EXT)
    {
        unsigned int colorAttachment = (pname - GL_DRAW_BUFFER0_EXT);

        if (colorAttachment >= mRenderer->getMaxRenderTargets())
        {
            // return true to stop further operation in the parent call
            return gl::error(GL_INVALID_OPERATION, true);
        }

        Framebuffer *framebuffer = getDrawFramebuffer();

        *params = framebuffer->getDrawBufferState(colorAttachment);
        return true;
    }

    // Please note: DEPTH_CLEAR_VALUE is not included in our internal getIntegerv implementation
    // because it is stored as a float, despite the fact that the GL ES 2.0 spec names
    // GetIntegerv as its native query function. As it would require conversion in any
    // case, this should make no difference to the calling application. You may find it in 
    // Context::getFloatv.
    switch (pname)
    {
      case GL_MAX_VERTEX_ATTRIBS:                       *params = gl::MAX_VERTEX_ATTRIBS;                               break;
      case GL_MAX_VERTEX_UNIFORM_VECTORS:               *params = mRenderer->getMaxVertexUniformVectors();              break;
      case GL_MAX_VERTEX_UNIFORM_COMPONENTS:            *params = mRenderer->getMaxVertexUniformVectors() * 4;          break;
      case GL_MAX_VARYING_VECTORS:                      *params = mRenderer->getMaxVaryingVectors();                    break;
      case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:         *params = mRenderer->getMaxCombinedTextureImageUnits();         break;
      case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:           *params = mRenderer->getMaxVertexTextureImageUnits();           break;
      case GL_MAX_TEXTURE_IMAGE_UNITS:                  *params = gl::MAX_TEXTURE_IMAGE_UNITS;                          break;
      case GL_MAX_FRAGMENT_UNIFORM_VECTORS:             *params = mRenderer->getMaxFragmentUniformVectors();            break;
      case GL_MAX_FRAGMENT_UNIFORM_COMPONENTS:          *params = mRenderer->getMaxFragmentUniformVectors() * 4;        break;
      case GL_MAX_RENDERBUFFER_SIZE:                    *params = getMaximumRenderbufferDimension();                    break;
      case GL_MAX_COLOR_ATTACHMENTS_EXT:                *params = mRenderer->getMaxRenderTargets();                     break;
      case GL_MAX_DRAW_BUFFERS_EXT:                     *params = mRenderer->getMaxRenderTargets();                     break;
      case GL_NUM_SHADER_BINARY_FORMATS:                *params = 0;                                                    break;
      case GL_SHADER_BINARY_FORMATS:                    /* no shader binary formats are supported */                    break;
      case GL_ARRAY_BUFFER_BINDING:                     *params = mState.arrayBuffer.id();                              break;
      case GL_ELEMENT_ARRAY_BUFFER_BINDING:             *params = getCurrentVertexArray()->getElementArrayBufferId();   break;
      //case GL_FRAMEBUFFER_BINDING:                    // now equivalent to GL_DRAW_FRAMEBUFFER_BINDING_ANGLE
      case GL_DRAW_FRAMEBUFFER_BINDING_ANGLE:           *params = mState.drawFramebuffer;                               break;
      case GL_READ_FRAMEBUFFER_BINDING_ANGLE:           *params = mState.readFramebuffer;                               break;
      case GL_RENDERBUFFER_BINDING:                     *params = mState.renderbuffer.id();                             break;
      case GL_VERTEX_ARRAY_BINDING:                     *params = mState.vertexArray;                                   break;
      case GL_CURRENT_PROGRAM:                          *params = mState.currentProgram;                                break;
      case GL_PACK_ALIGNMENT:                           *params = mState.pack.alignment;                                break;
      case GL_PACK_REVERSE_ROW_ORDER_ANGLE:             *params = mState.pack.reverseRowOrder;                          break;
      case GL_UNPACK_ALIGNMENT:                         *params = mState.unpack.alignment;                              break;
      case GL_GENERATE_MIPMAP_HINT:                     *params = mState.generateMipmapHint;                            break;
      case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:      *params = mState.fragmentShaderDerivativeHint;                  break;
      case GL_ACTIVE_TEXTURE:                           *params = (mState.activeSampler + GL_TEXTURE0);                 break;
      case GL_STENCIL_FUNC:                             *params = mState.depthStencil.stencilFunc;                      break;
      case GL_STENCIL_REF:                              *params = mState.stencilRef;                                    break;
      case GL_STENCIL_VALUE_MASK:                       *params = clampToInt(mState.depthStencil.stencilMask);          break;
      case GL_STENCIL_BACK_FUNC:                        *params = mState.depthStencil.stencilBackFunc;                  break;
      case GL_STENCIL_BACK_REF:                         *params = mState.stencilBackRef;                                break;
      case GL_STENCIL_BACK_VALUE_MASK:                  *params = clampToInt(mState.depthStencil.stencilBackMask);      break;
      case GL_STENCIL_FAIL:                             *params = mState.depthStencil.stencilFail;                      break;
      case GL_STENCIL_PASS_DEPTH_FAIL:                  *params = mState.depthStencil.stencilPassDepthFail;             break;
      case GL_STENCIL_PASS_DEPTH_PASS:                  *params = mState.depthStencil.stencilPassDepthPass;             break;
      case GL_STENCIL_BACK_FAIL:                        *params = mState.depthStencil.stencilBackFail;                  break;
      case GL_STENCIL_BACK_PASS_DEPTH_FAIL:             *params = mState.depthStencil.stencilBackPassDepthFail;         break;
      case GL_STENCIL_BACK_PASS_DEPTH_PASS:             *params = mState.depthStencil.stencilBackPassDepthPass;         break;
      case GL_DEPTH_FUNC:                               *params = mState.depthStencil.depthFunc;                        break;
      case GL_BLEND_SRC_RGB:                            *params = mState.blend.sourceBlendRGB;                          break;
      case GL_BLEND_SRC_ALPHA:                          *params = mState.blend.sourceBlendAlpha;                        break;
      case GL_BLEND_DST_RGB:                            *params = mState.blend.destBlendRGB;                            break;
      case GL_BLEND_DST_ALPHA:                          *params = mState.blend.destBlendAlpha;                          break;
      case GL_BLEND_EQUATION_RGB:                       *params = mState.blend.blendEquationRGB;                        break;
      case GL_BLEND_EQUATION_ALPHA:                     *params = mState.blend.blendEquationAlpha;                      break;
      case GL_STENCIL_WRITEMASK:                        *params = clampToInt(mState.depthStencil.stencilWritemask);     break;
      case GL_STENCIL_BACK_WRITEMASK:                   *params = clampToInt(mState.depthStencil.stencilBackWritemask); break;
      case GL_STENCIL_CLEAR_VALUE:                      *params = mState.stencilClearValue;                             break;
      case GL_SUBPIXEL_BITS:                            *params = 4;                                                    break;
      case GL_MAX_TEXTURE_SIZE:                         *params = getMaximum2DTextureDimension();                       break;
      case GL_MAX_CUBE_MAP_TEXTURE_SIZE:                *params = getMaximumCubeTextureDimension();                     break;
      case GL_MAX_3D_TEXTURE_SIZE:                      *params = getMaximum3DTextureDimension();                       break;
      case GL_MAX_ARRAY_TEXTURE_LAYERS:                 *params = getMaximum2DArrayTextureLayers();                     break;
      case GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT:          *params = getUniformBufferOffsetAlignment();                    break;
      case GL_MAX_UNIFORM_BUFFER_BINDINGS:              *params = getMaximumCombinedUniformBufferBindings();            break;
      case GL_MAX_VERTEX_UNIFORM_BLOCKS:                *params = mRenderer->getMaxVertexShaderUniformBuffers();        break;
      case GL_MAX_FRAGMENT_UNIFORM_BLOCKS:              *params = mRenderer->getMaxFragmentShaderUniformBuffers();      break;
      case GL_MAX_COMBINED_UNIFORM_BLOCKS:              *params = getMaximumCombinedUniformBufferBindings();            break;
      case GL_MAJOR_VERSION:                            *params = mClientVersion;                                       break;
      case GL_MINOR_VERSION:                            *params = 0;                                                    break;
      case GL_MAX_ELEMENTS_INDICES:                     *params = mRenderer->getMaxRecommendedElementsIndices();        break;
      case GL_MAX_ELEMENTS_VERTICES:                    *params = mRenderer->getMaxRecommendedElementsVertices();       break;
      case GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS: *params = mRenderer->getMaxTransformFeedbackInterleavedComponents(); break;
      case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS:       *params = mRenderer->getMaxTransformFeedbackBuffers();               break;
      case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS:    *params = mRenderer->getMaxTransformFeedbackSeparateComponents();    break;
      case GL_NUM_COMPRESSED_TEXTURE_FORMATS:   
        params[0] = mNumCompressedTextureFormats;
        break;
      case GL_MAX_SAMPLES_ANGLE:
        {
            GLsizei maxSamples = getMaxSupportedSamples();
            if (maxSamples != 0)
            {
                *params = maxSamples;
            }
            else
            {
                return false;
            }

            break;
        }
      case GL_SAMPLE_BUFFERS:                   
      case GL_SAMPLES:
        {
            gl::Framebuffer *framebuffer = getDrawFramebuffer();
            if (framebuffer->completeness() == GL_FRAMEBUFFER_COMPLETE)
            {
                switch (pname)
                {
                  case GL_SAMPLE_BUFFERS:
                    if (framebuffer->getSamples() != 0)
                    {
                        *params = 1;
                    }
                    else
                    {
                        *params = 0;
                    }
                    break;
                  case GL_SAMPLES:
                    *params = framebuffer->getSamples();
                    break;
                }
            }
            else 
            {
                *params = 0;
            }
        }
        break;
      case GL_IMPLEMENTATION_COLOR_READ_TYPE:
      case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
        {
            GLenum internalFormat, format, type;
            if (getCurrentReadFormatType(&internalFormat, &format, &type))
            {
                if (pname == GL_IMPLEMENTATION_COLOR_READ_FORMAT)
                    *params = format;
                else
                    *params = type;
            }
        }
        break;
      case GL_MAX_VIEWPORT_DIMS:
        {
            params[0] = mMaxViewportDimension;
            params[1] = mMaxViewportDimension;
        }
        break;
      case GL_COMPRESSED_TEXTURE_FORMATS:
        {
            if (supportsDXT1Textures())
            {
                *params++ = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
                *params++ = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
            }
            if (supportsDXT3Textures())
            {
                *params++ = GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE;
            }
            if (supportsDXT5Textures())
            {
                *params++ = GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE;
            }
        }
        break;
      case GL_VIEWPORT:
        params[0] = mState.viewport.x;
        params[1] = mState.viewport.y;
        params[2] = mState.viewport.width;
        params[3] = mState.viewport.height;
        break;
      case GL_SCISSOR_BOX:
        params[0] = mState.scissor.x;
        params[1] = mState.scissor.y;
        params[2] = mState.scissor.width;
        params[3] = mState.scissor.height;
        break;
      case GL_CULL_FACE_MODE:                   *params = mState.rasterizer.cullMode;   break;
      case GL_FRONT_FACE:                       *params = mState.rasterizer.frontFace;  break;
      case GL_RED_BITS:
      case GL_GREEN_BITS:
      case GL_BLUE_BITS:
      case GL_ALPHA_BITS:
        {
            gl::Framebuffer *framebuffer = getDrawFramebuffer();
            gl::Renderbuffer *colorbuffer = framebuffer->getFirstColorbuffer();

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
            gl::Framebuffer *framebuffer = getDrawFramebuffer();
            gl::Renderbuffer *depthbuffer = framebuffer->getDepthbuffer();

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
            gl::Framebuffer *framebuffer = getDrawFramebuffer();
            gl::Renderbuffer *stencilbuffer = framebuffer->getStencilbuffer();

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
        {
            if (mState.activeSampler > mRenderer->getMaxCombinedTextureImageUnits() - 1)
            {
                gl::error(GL_INVALID_OPERATION);
                return false;
            }

            *params = mState.samplerTexture[TEXTURE_2D][mState.activeSampler].id();
        }
        break;
      case GL_TEXTURE_BINDING_CUBE_MAP:
        {
            if (mState.activeSampler > mRenderer->getMaxCombinedTextureImageUnits() - 1)
            {
                gl::error(GL_INVALID_OPERATION);
                return false;
            }

            *params = mState.samplerTexture[TEXTURE_CUBE][mState.activeSampler].id();
        }
        break;
      case GL_TEXTURE_BINDING_3D:
        {
            if (mState.activeSampler > mRenderer->getMaxCombinedTextureImageUnits() - 1)
            {
                gl::error(GL_INVALID_OPERATION);
                return false;
            }

            *params = mState.samplerTexture[TEXTURE_3D][mState.activeSampler].id();
        }
        break;
      case GL_TEXTURE_BINDING_2D_ARRAY:
        {
            if (mState.activeSampler > mRenderer->getMaxCombinedTextureImageUnits() - 1)
            {
                gl::error(GL_INVALID_OPERATION);
                return false;
            }

            *params = mState.samplerTexture[TEXTURE_2D_ARRAY][mState.activeSampler].id();
        }
        break;
      case GL_RESET_NOTIFICATION_STRATEGY_EXT:
        *params = mResetStrategy;
        break;
      case GL_NUM_PROGRAM_BINARY_FORMATS_OES:
        *params = 1;
        break;
      case GL_PROGRAM_BINARY_FORMATS_OES:
        *params = GL_PROGRAM_BINARY_ANGLE;
        break;
      case GL_UNIFORM_BUFFER_BINDING:
        *params = mState.genericUniformBuffer.id();
        break;
      case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
        *params = mState.genericTransformFeedbackBuffer.id();
        break;
      case GL_COPY_READ_BUFFER_BINDING:
        *params = mState.copyReadBuffer.id();
        break;
      case GL_COPY_WRITE_BUFFER_BINDING:
        *params = mState.copyWriteBuffer.id();
        break;
      case GL_PIXEL_PACK_BUFFER_BINDING:
        *params = mState.pack.pixelBuffer.id();
        break;
      case GL_PIXEL_UNPACK_BUFFER_BINDING:
        *params = mState.unpack.pixelBuffer.id();
        break;
      case GL_NUM_EXTENSIONS:
        *params = static_cast<GLint>(getNumExtensions());
        break;
      default:
        return false;
    }

    return true;
}

bool Context::getInteger64v(GLenum pname, GLint64 *params)
{
    switch (pname)
    {
      case GL_MAX_ELEMENT_INDEX:
        *params = static_cast<GLint64>(std::numeric_limits<unsigned int>::max());
        break;
      case GL_MAX_UNIFORM_BLOCK_SIZE:
        *params = static_cast<GLint64>(mRenderer->getMaxUniformBufferSize());
        break;
      case GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS:
        {
            GLint64 uniformBufferComponents = static_cast<GLint64>(mRenderer->getMaxVertexShaderUniformBuffers()) * static_cast<GLint64>(mRenderer->getMaxUniformBufferSize() / 4);
            GLint64 defaultBufferComponents = static_cast<GLint64>(mRenderer->getMaxVertexUniformVectors() * 4);
            *params = uniformBufferComponents + defaultBufferComponents;
        }
        break;
      case GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS:
        {
            GLint64 uniformBufferComponents = static_cast<GLint64>(mRenderer->getMaxFragmentShaderUniformBuffers()) * static_cast<GLint64>(mRenderer->getMaxUniformBufferSize() / 4);
            GLint64 defaultBufferComponents = static_cast<GLint64>(mRenderer->getMaxVertexUniformVectors() * 4);
            *params = uniformBufferComponents + defaultBufferComponents;
        }
        break;
      case GL_MAX_SERVER_WAIT_TIMEOUT:
        // We do not wait for server fence objects internally, so report a max timeout of zero.
        *params = 0;
        break;
      default:
        return false;
    }

    return true;
}

bool Context::getIndexedIntegerv(GLenum target, GLuint index, GLint *data)
{
    switch (target)
    {
      case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
        if (index < IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS)
        {
            *data = mState.transformFeedbackBuffers[index].id();
        }
        break;
      case GL_UNIFORM_BUFFER_BINDING:
        if (index < IMPLEMENTATION_MAX_COMBINED_SHADER_UNIFORM_BUFFERS)
        {
            *data = mState.uniformBuffers[index].id();
        }
        break;
      default:
        return false;
    }

    return true;
}

bool Context::getIndexedInteger64v(GLenum target, GLuint index, GLint64 *data)
{
    switch (target)
    {
      case GL_TRANSFORM_FEEDBACK_BUFFER_START:
        if (index < IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS)
        {
            *data = mState.transformFeedbackBuffers[index].getOffset();
        }
        break;
      case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
        if (index < IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS)
        {
            *data = mState.transformFeedbackBuffers[index].getSize();
        }
        break;
      case GL_UNIFORM_BUFFER_START:
        if (index < IMPLEMENTATION_MAX_COMBINED_SHADER_UNIFORM_BUFFERS)
        {
            *data = mState.uniformBuffers[index].getOffset();
        }
        break;
      case GL_UNIFORM_BUFFER_SIZE:
        if (index < IMPLEMENTATION_MAX_COMBINED_SHADER_UNIFORM_BUFFERS)
        {
            *data = mState.uniformBuffers[index].getSize();
        }
        break;
      default:
        return false;
    }

    return true;
}

bool Context::getQueryParameterInfo(GLenum pname, GLenum *type, unsigned int *numParams)
{
    if (pname >= GL_DRAW_BUFFER0_EXT && pname <= GL_DRAW_BUFFER15_EXT)
    {
        *type = GL_INT;
        *numParams = 1;
        return true;
    }

    // Please note: the query type returned for DEPTH_CLEAR_VALUE in this implementation
    // is FLOAT rather than INT, as would be suggested by the GL ES 2.0 spec. This is due
    // to the fact that it is stored internally as a float, and so would require conversion
    // if returned from Context::getIntegerv. Since this conversion is already implemented 
    // in the case that one calls glGetIntegerv to retrieve a float-typed state variable, we
    // place DEPTH_CLEAR_VALUE with the floats. This should make no difference to the calling
    // application.
    switch (pname)
    {
      case GL_COMPRESSED_TEXTURE_FORMATS:
        {
            *type = GL_INT;
            *numParams = mNumCompressedTextureFormats;
        }
        return true;
      case GL_SHADER_BINARY_FORMATS:
        {
            *type = GL_INT;
            *numParams = 0;
        }
        return true;
      case GL_MAX_VERTEX_ATTRIBS:
      case GL_MAX_VERTEX_UNIFORM_VECTORS:
      case GL_MAX_VARYING_VECTORS:
      case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
      case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
      case GL_MAX_TEXTURE_IMAGE_UNITS:
      case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
      case GL_MAX_RENDERBUFFER_SIZE:
      case GL_MAX_COLOR_ATTACHMENTS_EXT:
      case GL_MAX_DRAW_BUFFERS_EXT:
      case GL_NUM_SHADER_BINARY_FORMATS:
      case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
      case GL_ARRAY_BUFFER_BINDING:
      case GL_FRAMEBUFFER_BINDING:
      case GL_RENDERBUFFER_BINDING:
      case GL_CURRENT_PROGRAM:
      case GL_PACK_ALIGNMENT:
      case GL_PACK_REVERSE_ROW_ORDER_ANGLE:
      case GL_UNPACK_ALIGNMENT:
      case GL_GENERATE_MIPMAP_HINT:
      case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:
      case GL_RED_BITS:
      case GL_GREEN_BITS:
      case GL_BLUE_BITS:
      case GL_ALPHA_BITS:
      case GL_DEPTH_BITS:
      case GL_STENCIL_BITS:
      case GL_ELEMENT_ARRAY_BUFFER_BINDING:
      case GL_CULL_FACE_MODE:
      case GL_FRONT_FACE:
      case GL_ACTIVE_TEXTURE:
      case GL_STENCIL_FUNC:
      case GL_STENCIL_VALUE_MASK:
      case GL_STENCIL_REF:
      case GL_STENCIL_FAIL:
      case GL_STENCIL_PASS_DEPTH_FAIL:
      case GL_STENCIL_PASS_DEPTH_PASS:
      case GL_STENCIL_BACK_FUNC:
      case GL_STENCIL_BACK_VALUE_MASK:
      case GL_STENCIL_BACK_REF:
      case GL_STENCIL_BACK_FAIL:
      case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
      case GL_STENCIL_BACK_PASS_DEPTH_PASS:
      case GL_DEPTH_FUNC:
      case GL_BLEND_SRC_RGB:
      case GL_BLEND_SRC_ALPHA:
      case GL_BLEND_DST_RGB:
      case GL_BLEND_DST_ALPHA:
      case GL_BLEND_EQUATION_RGB:
      case GL_BLEND_EQUATION_ALPHA:
      case GL_STENCIL_WRITEMASK:
      case GL_STENCIL_BACK_WRITEMASK:
      case GL_STENCIL_CLEAR_VALUE:
      case GL_SUBPIXEL_BITS:
      case GL_MAX_TEXTURE_SIZE:
      case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
      case GL_SAMPLE_BUFFERS:
      case GL_SAMPLES:
      case GL_IMPLEMENTATION_COLOR_READ_TYPE:
      case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
      case GL_TEXTURE_BINDING_2D:
      case GL_TEXTURE_BINDING_CUBE_MAP:
      case GL_RESET_NOTIFICATION_STRATEGY_EXT:
      case GL_NUM_PROGRAM_BINARY_FORMATS_OES:
      case GL_PROGRAM_BINARY_FORMATS_OES:
        {
            *type = GL_INT;
            *numParams = 1;
        }
        return true;
      case GL_MAX_SAMPLES_ANGLE:
        {
            if (getMaxSupportedSamples() != 0)
            {
                *type = GL_INT;
                *numParams = 1;
            }
            else
            {
                return false;
            }
        }
        return true;
      case GL_MAX_VIEWPORT_DIMS:
        {
            *type = GL_INT;
            *numParams = 2;
        }
        return true;
      case GL_VIEWPORT:
      case GL_SCISSOR_BOX:
        {
            *type = GL_INT;
            *numParams = 4;
        }
        return true;
      case GL_SHADER_COMPILER:
      case GL_SAMPLE_COVERAGE_INVERT:
      case GL_DEPTH_WRITEMASK:
      case GL_CULL_FACE:                // CULL_FACE through DITHER are natural to IsEnabled,
      case GL_POLYGON_OFFSET_FILL:      // but can be retrieved through the Get{Type}v queries.
      case GL_SAMPLE_ALPHA_TO_COVERAGE: // For this purpose, they are treated here as bool-natural
      case GL_SAMPLE_COVERAGE:
      case GL_SCISSOR_TEST:
      case GL_STENCIL_TEST:
      case GL_DEPTH_TEST:
      case GL_BLEND:
      case GL_DITHER:
      case GL_CONTEXT_ROBUST_ACCESS_EXT:
        {
            *type = GL_BOOL;
            *numParams = 1;
        }
        return true;
      case GL_COLOR_WRITEMASK:
        {
            *type = GL_BOOL;
            *numParams = 4;
        }
        return true;
      case GL_POLYGON_OFFSET_FACTOR:
      case GL_POLYGON_OFFSET_UNITS:
      case GL_SAMPLE_COVERAGE_VALUE:
      case GL_DEPTH_CLEAR_VALUE:
      case GL_LINE_WIDTH:
        {
            *type = GL_FLOAT;
            *numParams = 1;
        }
        return true;
      case GL_ALIASED_LINE_WIDTH_RANGE:
      case GL_ALIASED_POINT_SIZE_RANGE:
      case GL_DEPTH_RANGE:
        {
            *type = GL_FLOAT;
            *numParams = 2;
        }
        return true;
      case GL_COLOR_CLEAR_VALUE:
      case GL_BLEND_COLOR:
        {
            *type = GL_FLOAT;
            *numParams = 4;
        }
        return true;
      case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT:
        if (!supportsTextureFilterAnisotropy())
        {
            return false;
        }
        *type = GL_FLOAT;
        *numParams = 1;
        return true;
    }

    if (mClientVersion < 3)
    {
        return false;
    }

    // Check for ES3.0+ parameter names
    switch (pname)
    {
      case GL_MAX_UNIFORM_BUFFER_BINDINGS:
      case GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT:
      case GL_UNIFORM_BUFFER_BINDING:
      case GL_TRANSFORM_FEEDBACK_BINDING:
      case GL_COPY_READ_BUFFER_BINDING:
      case GL_COPY_WRITE_BUFFER_BINDING:
      case GL_PIXEL_PACK_BUFFER_BINDING:
      case GL_PIXEL_UNPACK_BUFFER_BINDING:
      case GL_TEXTURE_BINDING_3D:
      case GL_TEXTURE_BINDING_2D_ARRAY:
      case GL_MAX_3D_TEXTURE_SIZE:
      case GL_MAX_ARRAY_TEXTURE_LAYERS:
      case GL_MAX_VERTEX_UNIFORM_BLOCKS:
      case GL_MAX_FRAGMENT_UNIFORM_BLOCKS:
      case GL_MAX_COMBINED_UNIFORM_BLOCKS:
      case GL_VERTEX_ARRAY_BINDING:
      case GL_MAX_VERTEX_UNIFORM_COMPONENTS:
      case GL_MAX_FRAGMENT_UNIFORM_COMPONENTS:
      case GL_NUM_EXTENSIONS:
      case GL_MAJOR_VERSION:
      case GL_MINOR_VERSION:
      case GL_MAX_ELEMENTS_INDICES:
      case GL_MAX_ELEMENTS_VERTICES:
      case GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS:
      case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS:
      case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS:
        {
            *type = GL_INT;
            *numParams = 1;
        }
        return true;

      case GL_MAX_ELEMENT_INDEX:
      case GL_MAX_UNIFORM_BLOCK_SIZE:
      case GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS:
      case GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS:
      case GL_MAX_SERVER_WAIT_TIMEOUT:
        {
            *type = GL_INT_64_ANGLEX;
            *numParams = 1;
        }
        return true;

      case GL_TRANSFORM_FEEDBACK_ACTIVE:
      case GL_TRANSFORM_FEEDBACK_PAUSED:
        {
            *type = GL_BOOL;
            *numParams = 1;
        }
        return true;
    }

    return false;
}

bool Context::getIndexedQueryParameterInfo(GLenum target, GLenum *type, unsigned int *numParams)
{
    if (mClientVersion < 3)
    {
        return false;
    }

    switch (target)
    {
      case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
      case GL_UNIFORM_BUFFER_BINDING:
        {
            *type = GL_INT;
            *numParams = 1;
        }
        return true;
      case GL_TRANSFORM_FEEDBACK_BUFFER_START:
      case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
      case GL_UNIFORM_BUFFER_START:
      case GL_UNIFORM_BUFFER_SIZE:
        {
            *type = GL_INT_64_ANGLEX;
            *numParams = 1;
        }
    }

    return false;
}

// Applies the render target surface, depth stencil surface, viewport rectangle and
// scissor rectangle to the renderer
bool Context::applyRenderTarget(GLenum drawMode, bool ignoreViewport)
{
    Framebuffer *framebufferObject = getDrawFramebuffer();

    if (!framebufferObject || framebufferObject->completeness() != GL_FRAMEBUFFER_COMPLETE)
    {
        return gl::error(GL_INVALID_FRAMEBUFFER_OPERATION, false);
    }

    mRenderer->applyRenderTarget(framebufferObject);

    if (!mRenderer->setViewport(mState.viewport, mState.zNear, mState.zFar, drawMode, mState.rasterizer.frontFace,
                                ignoreViewport))
    {
        return false;
    }

    mRenderer->setScissorRectangle(mState.scissor, mState.scissorTest);

    return true;
}

// Applies the fixed-function state (culling, depth test, alpha blending, stenciling, etc) to the Direct3D 9 device
void Context::applyState(GLenum drawMode)
{
    Framebuffer *framebufferObject = getDrawFramebuffer();
    int samples = framebufferObject->getSamples();

    mState.rasterizer.pointDrawMode = (drawMode == GL_POINTS);
    mState.rasterizer.multiSample = (samples != 0);
    mRenderer->setRasterizerState(mState.rasterizer);

    unsigned int mask = 0;
    if (mState.sampleCoverage)
    {
        if (mState.sampleCoverageValue != 0)
        {
            
            float threshold = 0.5f;

            for (int i = 0; i < samples; ++i)
            {
                mask <<= 1;

                if ((i + 1) * mState.sampleCoverageValue >= threshold)
                {
                    threshold += 1.0f;
                    mask |= 1;
                }
            }
        }

        if (mState.sampleCoverageInvert)
        {
            mask = ~mask;
        }
    }
    else
    {
        mask = 0xFFFFFFFF;
    }
    mRenderer->setBlendState(framebufferObject, mState.blend, mState.blendColor, mask);

    mRenderer->setDepthStencilState(mState.depthStencil, mState.stencilRef, mState.stencilBackRef,
                                    mState.rasterizer.frontFace == GL_CCW);
}

// Applies the shaders and shader constants to the Direct3D 9 device
void Context::applyShaders(ProgramBinary *programBinary, bool transformFeedbackActive)
{
    const VertexAttribute *vertexAttributes = getCurrentVertexArray()->getVertexAttributes();

    VertexFormat inputLayout[gl::MAX_VERTEX_ATTRIBS];
    VertexFormat::GetInputLayout(inputLayout, programBinary, vertexAttributes, mState.vertexAttribCurrentValues);

    mRenderer->applyShaders(programBinary, mState.rasterizer.rasterizerDiscard, transformFeedbackActive, inputLayout);

    programBinary->applyUniforms();
}

bool Context::getCurrentTextureAndSamplerState(ProgramBinary *programBinary, SamplerType type, int index, Texture **outTexture,
                                               TextureType *outTextureType, SamplerState *outSampler)
{
    int textureUnit = programBinary->getSamplerMapping(type, index);   // OpenGL texture image unit index

    if (textureUnit != -1)
    {
        TextureType textureType = programBinary->getSamplerTextureType(type, index);
        Texture *texture = getSamplerTexture(textureUnit, textureType);

        SamplerState samplerState;
        texture->getSamplerState(&samplerState);

        if (mState.samplers[textureUnit] != 0)
        {
            Sampler *samplerObject = getSampler(mState.samplers[textureUnit]);
            samplerObject->getState(&samplerState);
        }

        *outTexture = texture;
        *outTextureType = textureType;
        *outSampler = samplerState;

        return true;
    }
    else
    {
        return false;
    }
}

void Context::generateSwizzles(ProgramBinary *programBinary)
{
    generateSwizzles(programBinary, SAMPLER_PIXEL);

    if (mSupportsVertexTexture)
    {
        generateSwizzles(programBinary, SAMPLER_VERTEX);
    }
}

void Context::generateSwizzles(ProgramBinary *programBinary, SamplerType type)
{
    // Range of Direct3D samplers of given sampler type
    int samplerRange = programBinary->getUsedSamplerRange(type);
    for (int samplerIndex = 0; samplerIndex < samplerRange; samplerIndex++)
    {
        Texture *texture = NULL;
        TextureType textureType;
        SamplerState samplerState;
        if (getCurrentTextureAndSamplerState(programBinary, type, samplerIndex, &texture, &textureType, &samplerState) && texture->isSwizzled())
        {
            mRenderer->generateSwizzle(texture);
        }
    }
}

// Applies the textures and sampler states to the Direct3D 9 device
void Context::applyTextures(ProgramBinary *programBinary)
{
    applyTextures(programBinary, SAMPLER_PIXEL);

    if (mSupportsVertexTexture)
    {
        applyTextures(programBinary, SAMPLER_VERTEX);
    }
}

// For each Direct3D sampler of either the pixel or vertex stage,
// looks up the corresponding OpenGL texture image unit and texture type,
// and sets the texture and its addressing/filtering state (or NULL when inactive).
void Context::applyTextures(ProgramBinary *programBinary, SamplerType type)
{
    FramebufferTextureSerialSet boundFramebufferTextures = getBoundFramebufferTextureSerials();

    // Range of Direct3D samplers of given sampler type
    int samplerCount = (type == SAMPLER_PIXEL) ? MAX_TEXTURE_IMAGE_UNITS : mRenderer->getMaxVertexTextureImageUnits();
    int samplerRange = programBinary->getUsedSamplerRange(type);

    for (int samplerIndex = 0; samplerIndex < samplerRange; samplerIndex++)
    {
        Texture *texture = NULL;
        TextureType textureType;
        SamplerState samplerState;
        if (getCurrentTextureAndSamplerState(programBinary, type, samplerIndex, &texture, &textureType, &samplerState))
        {
            if (texture->isSamplerComplete(samplerState) &&
                boundFramebufferTextures.find(texture->getTextureSerial()) == boundFramebufferTextures.end())
            {
                mRenderer->setSamplerState(type, samplerIndex, samplerState);
                mRenderer->setTexture(type, samplerIndex, texture);
                texture->resetDirty();
            }
            else
            {
                mRenderer->setTexture(type, samplerIndex, getIncompleteTexture(textureType));
            }
        }
        else
        {
            mRenderer->setTexture(type, samplerIndex, NULL);
        }
    }

    for (int samplerIndex = samplerRange; samplerIndex < samplerCount; samplerIndex++)
    {
        mRenderer->setTexture(type, samplerIndex, NULL);
    }
}

bool Context::applyUniformBuffers()
{
    Program *programObject = getProgram(mState.currentProgram);
    ProgramBinary *programBinary = programObject->getProgramBinary();

    std::vector<gl::Buffer*> boundBuffers;

    for (unsigned int uniformBlockIndex = 0; uniformBlockIndex < programBinary->getActiveUniformBlockCount(); uniformBlockIndex++)
    {
        GLuint blockBinding = programObject->getUniformBlockBinding(uniformBlockIndex);
        const OffsetBindingPointer<Buffer>& boundBuffer = mState.uniformBuffers[blockBinding];
        if (boundBuffer.id() == 0)
        {
            // undefined behaviour
            return false;
        }
        else
        {
            gl::Buffer *uniformBuffer = boundBuffer.get();
            ASSERT(uniformBuffer);
            boundBuffers.push_back(uniformBuffer);
        }
    }

    return programBinary->applyUniformBuffers(boundBuffers);
}

bool Context::applyTransformFeedbackBuffers()
{
    TransformFeedback *curTransformFeedback = getCurrentTransformFeedback();
    if (curTransformFeedback && curTransformFeedback->isStarted() && !curTransformFeedback->isPaused())
    {
        Buffer *transformFeedbackBuffers[IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS];
        GLintptr transformFeedbackOffsets[IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS];
        for (size_t i = 0; i < IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS; i++)
        {
            transformFeedbackBuffers[i] = mState.transformFeedbackBuffers[i].get();
            transformFeedbackOffsets[i] = mState.transformFeedbackBuffers[i].getOffset();
        }
        mRenderer->applyTransformFeedbackBuffers(transformFeedbackBuffers, transformFeedbackOffsets);
        return true;
    }
    else
    {
        return false;
    }
}

void Context::markTransformFeedbackUsage()
{
    for (size_t i = 0; i < IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS; i++)
    {
        Buffer *buffer = mState.transformFeedbackBuffers[i].get();
        if (buffer)
        {
            buffer->markTransformFeedbackUsage();
        }
    }
}

void Context::clear(GLbitfield mask)
{
    if (isRasterizerDiscardEnabled())
    {
        return;
    }

    ClearParameters clearParams = { 0 };
    for (unsigned int i = 0; i < ArraySize(clearParams.clearColor); i++)
    {
        clearParams.clearColor[i] = false;
    }
    clearParams.colorFClearValue = mState.colorClearValue;
    clearParams.colorClearType = GL_FLOAT;
    clearParams.colorMaskRed = mState.blend.colorMaskRed;
    clearParams.colorMaskGreen = mState.blend.colorMaskGreen;
    clearParams.colorMaskBlue = mState.blend.colorMaskBlue;
    clearParams.colorMaskAlpha = mState.blend.colorMaskAlpha;
    clearParams.clearDepth = false;
    clearParams.depthClearValue = mState.depthClearValue;
    clearParams.clearStencil = false;
    clearParams.stencilClearValue = mState.stencilClearValue;
    clearParams.stencilWriteMask = mState.depthStencil.stencilWritemask;
    clearParams.scissorEnabled = mState.scissorTest;
    clearParams.scissor = mState.scissor;

    Framebuffer *framebufferObject = getDrawFramebuffer();
    if (mask & GL_COLOR_BUFFER_BIT)
    {
        if (framebufferObject->hasEnabledColorAttachment())
        {
            for (unsigned int i = 0; i < ArraySize(clearParams.clearColor); i++)
            {
                clearParams.clearColor[i] = true;
            }
        }
    }

    if (mask & GL_DEPTH_BUFFER_BIT)
    {
        if (mState.depthStencil.depthMask && framebufferObject->getDepthbufferType() != GL_NONE)
        {
            clearParams.clearDepth = true;
        }
    }

    if (mask & GL_STENCIL_BUFFER_BIT)
    {
        if (framebufferObject->getStencilbufferType() != GL_NONE)
        {
            rx::RenderTarget *depthStencil = framebufferObject->getStencilbuffer()->getDepthStencil();
            if (!depthStencil)
            {
                ERR("Depth stencil pointer unexpectedly null.");
                return;
            }

            if (gl::GetStencilBits(depthStencil->getActualFormat(), mClientVersion) > 0)
            {
                clearParams.clearStencil = true;
            }
        }
    }


    if (!applyRenderTarget(GL_TRIANGLES, true))   // Clips the clear to the scissor rectangle but not the viewport
    {
        return;
    }

    mRenderer->clear(clearParams, framebufferObject);
}

void Context::clearBufferfv(GLenum buffer, int drawbuffer, const float *values)
{
    if (isRasterizerDiscardEnabled())
    {
        return;
    }

    // glClearBufferfv can be called to clear the color buffer or depth buffer
    ClearParameters clearParams = { 0 };

    if (buffer == GL_COLOR)
    {
        for (unsigned int i = 0; i < ArraySize(clearParams.clearColor); i++)
        {
            clearParams.clearColor[i] = (drawbuffer == static_cast<int>(i));
        }
        clearParams.colorFClearValue = ColorF(values[0], values[1], values[2], values[3]);
        clearParams.colorClearType = GL_FLOAT;
    }
    else
    {
        for (unsigned int i = 0; i < ArraySize(clearParams.clearColor); i++)
        {
            clearParams.clearColor[i] = false;
        }
        clearParams.colorFClearValue = mState.colorClearValue;
        clearParams.colorClearType = GL_FLOAT;
    }

    clearParams.colorMaskRed = mState.blend.colorMaskRed;
    clearParams.colorMaskGreen = mState.blend.colorMaskGreen;
    clearParams.colorMaskBlue = mState.blend.colorMaskBlue;
    clearParams.colorMaskAlpha = mState.blend.colorMaskAlpha;

    if (buffer == GL_DEPTH)
    {
        clearParams.clearDepth = true;
        clearParams.depthClearValue = values[0];
    }
    else
    {
        clearParams.clearDepth = false;
        clearParams.depthClearValue = mState.depthClearValue;
    }

    clearParams.clearStencil = false;
    clearParams.stencilClearValue = mState.stencilClearValue;
    clearParams.stencilWriteMask = mState.depthStencil.stencilWritemask;
    clearParams.scissorEnabled = mState.scissorTest;
    clearParams.scissor = mState.scissor;

    if (!applyRenderTarget(GL_TRIANGLES, true))   // Clips the clear to the scissor rectangle but not the viewport
    {
        return;
    }

    mRenderer->clear(clearParams, getDrawFramebuffer());
}

void Context::clearBufferuiv(GLenum buffer, int drawbuffer, const unsigned int *values)
{
    if (isRasterizerDiscardEnabled())
    {
        return;
    }

    // glClearBufferuv can only be called to clear a color buffer
    ClearParameters clearParams = { 0 };
    for (unsigned int i = 0; i < ArraySize(clearParams.clearColor); i++)
    {
        clearParams.clearColor[i] = (drawbuffer == static_cast<int>(i));
    }
    clearParams.colorUIClearValue = ColorUI(values[0], values[1], values[2], values[3]);
    clearParams.colorClearType = GL_UNSIGNED_INT;
    clearParams.colorMaskRed = mState.blend.colorMaskRed;
    clearParams.colorMaskGreen = mState.blend.colorMaskGreen;
    clearParams.colorMaskBlue = mState.blend.colorMaskBlue;
    clearParams.colorMaskAlpha = mState.blend.colorMaskAlpha;
    clearParams.clearDepth = false;
    clearParams.depthClearValue = mState.depthClearValue;
    clearParams.clearStencil = false;
    clearParams.stencilClearValue = mState.stencilClearValue;
    clearParams.stencilWriteMask = mState.depthStencil.stencilWritemask;
    clearParams.scissorEnabled = mState.scissorTest;
    clearParams.scissor = mState.scissor;

    if (!applyRenderTarget(GL_TRIANGLES, true))   // Clips the clear to the scissor rectangle but not the viewport
    {
        return;
    }

    mRenderer->clear(clearParams, getDrawFramebuffer());
}

void Context::clearBufferiv(GLenum buffer, int drawbuffer, const int *values)
{
    if (isRasterizerDiscardEnabled())
    {
        return;
    }

    // glClearBufferfv can be called to clear the color buffer or stencil buffer
    ClearParameters clearParams = { 0 };

    if (buffer == GL_COLOR)
    {
        for (unsigned int i = 0; i < ArraySize(clearParams.clearColor); i++)
        {
            clearParams.clearColor[i] = (drawbuffer == static_cast<int>(i));
        }
        clearParams.colorIClearValue = ColorI(values[0], values[1], values[2], values[3]);
        clearParams.colorClearType = GL_INT;
    }
    else
    {
        for (unsigned int i = 0; i < ArraySize(clearParams.clearColor); i++)
        {
            clearParams.clearColor[i] = false;
        }
        clearParams.colorFClearValue = mState.colorClearValue;
        clearParams.colorClearType = GL_FLOAT;
    }

    clearParams.colorMaskRed = mState.blend.colorMaskRed;
    clearParams.colorMaskGreen = mState.blend.colorMaskGreen;
    clearParams.colorMaskBlue = mState.blend.colorMaskBlue;
    clearParams.colorMaskAlpha = mState.blend.colorMaskAlpha;

    clearParams.clearDepth = false;
    clearParams.depthClearValue = mState.depthClearValue;

    if (buffer == GL_STENCIL)
    {
        clearParams.clearStencil = true;
        clearParams.stencilClearValue = values[1];
    }
    else
    {
        clearParams.clearStencil = false;
        clearParams.stencilClearValue = mState.stencilClearValue;
    }
    clearParams.stencilWriteMask = mState.depthStencil.stencilWritemask;

    clearParams.scissorEnabled = mState.scissorTest;
    clearParams.scissor = mState.scissor;

    if (!applyRenderTarget(GL_TRIANGLES, true))   // Clips the clear to the scissor rectangle but not the viewport
    {
        return;
    }

    mRenderer->clear(clearParams, getDrawFramebuffer());
}

void Context::clearBufferfi(GLenum buffer, int drawbuffer, float depth, int stencil)
{
    if (isRasterizerDiscardEnabled())
    {
        return;
    }

    // glClearBufferfi can only be called to clear a depth stencil buffer
    ClearParameters clearParams = { 0 };
    for (unsigned int i = 0; i < ArraySize(clearParams.clearColor); i++)
    {
        clearParams.clearColor[i] = false;
    }
    clearParams.colorFClearValue = mState.colorClearValue;
    clearParams.colorClearType = GL_FLOAT;
    clearParams.colorMaskRed = mState.blend.colorMaskRed;
    clearParams.colorMaskGreen = mState.blend.colorMaskGreen;
    clearParams.colorMaskBlue = mState.blend.colorMaskBlue;
    clearParams.colorMaskAlpha = mState.blend.colorMaskAlpha;
    clearParams.clearDepth = true;
    clearParams.depthClearValue = depth;
    clearParams.clearStencil = true;
    clearParams.stencilClearValue = stencil;
    clearParams.stencilWriteMask = mState.depthStencil.stencilWritemask;
    clearParams.scissorEnabled = mState.scissorTest;
    clearParams.scissor = mState.scissor;

    if (!applyRenderTarget(GL_TRIANGLES, true))   // Clips the clear to the scissor rectangle but not the viewport
    {
        return;
    }

    mRenderer->clear(clearParams, getDrawFramebuffer());
}

void Context::readPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                         GLenum format, GLenum type, GLsizei *bufSize, void* pixels)
{
    gl::Framebuffer *framebuffer = getReadFramebuffer();

    bool isSized = IsSizedInternalFormat(format, mClientVersion);
    GLenum sizedInternalFormat = (isSized ? format : GetSizedInternalFormat(format, type, mClientVersion));
    GLuint outputPitch = GetRowPitch(sizedInternalFormat, type, mClientVersion, width, mState.pack.alignment);

    mRenderer->readPixels(framebuffer, x, y, width, height, format, type, outputPitch, mState.pack, pixels);
}

void Context::drawArrays(GLenum mode, GLint first, GLsizei count, GLsizei instances)
{
    if (!mState.currentProgram)
    {
        return gl::error(GL_INVALID_OPERATION);
    }

    ProgramBinary *programBinary = getCurrentProgramBinary();
    programBinary->applyUniforms();

    generateSwizzles(programBinary);

    if (!mRenderer->applyPrimitiveType(mode, count))
    {
        return;
    }

    if (!applyRenderTarget(mode, false))
    {
        return;
    }

    applyState(mode);

    GLenum err = mRenderer->applyVertexBuffer(programBinary, getCurrentVertexArray()->getVertexAttributes(), mState.vertexAttribCurrentValues, first, count, instances);
    if (err != GL_NO_ERROR)
    {
        return gl::error(err);
    }

    bool transformFeedbackActive = applyTransformFeedbackBuffers();

    applyShaders(programBinary, transformFeedbackActive);
    applyTextures(programBinary);

    if (!applyUniformBuffers())
    {
        return;
    }

    if (!programBinary->validateSamplers(NULL))
    {
        return gl::error(GL_INVALID_OPERATION);
    }

    if (!skipDraw(mode))
    {
        mRenderer->drawArrays(mode, count, instances, transformFeedbackActive);

        if (transformFeedbackActive)
        {
            markTransformFeedbackUsage();
        }
    }
}

void Context::drawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei instances)
{
    if (!mState.currentProgram)
    {
        return gl::error(GL_INVALID_OPERATION);
    }

    VertexArray *vao = getCurrentVertexArray();
    if (!indices && !vao->getElementArrayBuffer())
    {
        return gl::error(GL_INVALID_OPERATION);
    }

    ProgramBinary *programBinary = getCurrentProgramBinary();
    programBinary->applyUniforms();

    generateSwizzles(programBinary);

    if (!mRenderer->applyPrimitiveType(mode, count))
    {
        return;
    }

    if (!applyRenderTarget(mode, false))
    {
        return;
    }

    applyState(mode);

    rx::TranslatedIndexData indexInfo;
    GLenum err = mRenderer->applyIndexBuffer(indices, vao->getElementArrayBuffer(), count, mode, type, &indexInfo);
    if (err != GL_NO_ERROR)
    {
        return gl::error(err);
    }

    GLsizei vertexCount = indexInfo.maxIndex - indexInfo.minIndex + 1;
    err = mRenderer->applyVertexBuffer(programBinary, vao->getVertexAttributes(), mState.vertexAttribCurrentValues, indexInfo.minIndex, vertexCount, instances);
    if (err != GL_NO_ERROR)
    {
        return gl::error(err);
    }

    bool transformFeedbackActive = applyTransformFeedbackBuffers();
    // Transform feedback is not allowed for DrawElements, this error should have been caught at the API validation
    // layer.
    ASSERT(!transformFeedbackActive);

    applyShaders(programBinary, transformFeedbackActive);
    applyTextures(programBinary);

    if (!applyUniformBuffers())
    {
        return;
    }

    if (!programBinary->validateSamplers(NULL))
    {
        return gl::error(GL_INVALID_OPERATION);
    }

    if (!skipDraw(mode))
    {
        mRenderer->drawElements(mode, count, type, indices, vao->getElementArrayBuffer(), indexInfo, instances);
    }
}

// Implements glFlush when block is false, glFinish when block is true
void Context::sync(bool block)
{
    mRenderer->sync(block);
}

void Context::recordInvalidEnum()
{
    mInvalidEnum = true;
}

void Context::recordInvalidValue()
{
    mInvalidValue = true;
}

void Context::recordInvalidOperation()
{
    mInvalidOperation = true;
}

void Context::recordOutOfMemory()
{
    mOutOfMemory = true;
}

void Context::recordInvalidFramebufferOperation()
{
    mInvalidFramebufferOperation = true;
}

// Get one of the recorded errors and clear its flag, if any.
// [OpenGL ES 2.0.24] section 2.5 page 13.
GLenum Context::getError()
{
    if (mInvalidEnum)
    {
        mInvalidEnum = false;

        return GL_INVALID_ENUM;
    }

    if (mInvalidValue)
    {
        mInvalidValue = false;

        return GL_INVALID_VALUE;
    }

    if (mInvalidOperation)
    {
        mInvalidOperation = false;

        return GL_INVALID_OPERATION;
    }

    if (mOutOfMemory)
    {
        mOutOfMemory = false;

        return GL_OUT_OF_MEMORY;
    }

    if (mInvalidFramebufferOperation)
    {
        mInvalidFramebufferOperation = false;

        return GL_INVALID_FRAMEBUFFER_OPERATION;
    }

    return GL_NO_ERROR;
}

GLenum Context::getResetStatus()
{
    if (mResetStatus == GL_NO_ERROR && !mContextLost)
    {
        // mResetStatus will be set by the markContextLost callback
        // in the case a notification is sent
        mRenderer->testDeviceLost(true);
    }

    GLenum status = mResetStatus;

    if (mResetStatus != GL_NO_ERROR)
    {
        ASSERT(mContextLost);

        if (mRenderer->testDeviceResettable())
        {
            mResetStatus = GL_NO_ERROR;
        }
    }
    
    return status;
}

bool Context::isResetNotificationEnabled()
{
    return (mResetStrategy == GL_LOSE_CONTEXT_ON_RESET_EXT);
}

int Context::getClientVersion() const
{
    return mClientVersion;
}

int Context::getMajorShaderModel() const
{
    return mMajorShaderModel;
}

float Context::getMaximumPointSize() const
{
    return mMaximumPointSize;
}

unsigned int Context::getMaximumCombinedTextureImageUnits() const
{
    return mRenderer->getMaxCombinedTextureImageUnits();
}

unsigned int Context::getMaximumCombinedUniformBufferBindings() const
{
    return mRenderer->getMaxVertexShaderUniformBuffers() +
           mRenderer->getMaxFragmentShaderUniformBuffers();
}

int Context::getMaxSupportedSamples() const
{
    return mRenderer->getMaxSupportedSamples();
}

GLsizei Context::getMaxSupportedFormatSamples(GLenum internalFormat) const
{
    return mRenderer->getMaxSupportedFormatSamples(internalFormat);
}

GLsizei Context::getNumSampleCounts(GLenum internalFormat) const
{
    return mRenderer->getNumSampleCounts(internalFormat);
}

void Context::getSampleCounts(GLenum internalFormat, GLsizei bufSize, GLint *params) const
{
    mRenderer->getSampleCounts(internalFormat, bufSize, params);
}

unsigned int Context::getMaxTransformFeedbackBufferBindings() const
{
    return mRenderer->getMaxTransformFeedbackBuffers();
}

GLintptr Context::getUniformBufferOffsetAlignment() const
{
    // setting a large alignment forces uniform buffers to bind with zero offset
    return static_cast<GLintptr>(std::numeric_limits<GLint>::max());
}

unsigned int Context::getMaximumRenderTargets() const
{
    return mRenderer->getMaxRenderTargets();
}

bool Context::supportsEventQueries() const
{
    return mSupportsEventQueries;
}

bool Context::supportsOcclusionQueries() const
{
    return mSupportsOcclusionQueries;
}

bool Context::supportsBGRATextures() const
{
    return mSupportsBGRATextures;
}

bool Context::supportsDXT1Textures() const
{
    return mSupportsDXT1Textures;
}

bool Context::supportsDXT3Textures() const
{
    return mSupportsDXT3Textures;
}

bool Context::supportsDXT5Textures() const
{
    return mSupportsDXT5Textures;
}

bool Context::supportsFloat32Textures() const
{
    return mSupportsFloat32Textures;
}

bool Context::supportsFloat32LinearFilter() const
{
    return mSupportsFloat32LinearFilter;
}

bool Context::supportsFloat32RenderableTextures() const
{
    return mSupportsFloat32RenderableTextures;
}

bool Context::supportsFloat16Textures() const
{
    return mSupportsFloat16Textures;
}

bool Context::supportsFloat16LinearFilter() const
{
    return mSupportsFloat16LinearFilter;
}

bool Context::supportsFloat16RenderableTextures() const
{
    return mSupportsFloat16RenderableTextures;
}

int Context::getMaximumRenderbufferDimension() const
{
    return mMaxRenderbufferDimension;
}

int Context::getMaximum2DTextureDimension() const
{
    return mMax2DTextureDimension;
}

int Context::getMaximumCubeTextureDimension() const
{
    return mMaxCubeTextureDimension;
}

int Context::getMaximum3DTextureDimension() const
{
    return mMax3DTextureDimension;
}

int Context::getMaximum2DArrayTextureLayers() const
{
    return mMax2DArrayTextureLayers;
}

int Context::getMaximum2DTextureLevel() const
{
    return mMax2DTextureLevel;
}

int Context::getMaximumCubeTextureLevel() const
{
    return mMaxCubeTextureLevel;
}

int Context::getMaximum3DTextureLevel() const
{
    return mMax3DTextureLevel;
}

int Context::getMaximum2DArrayTextureLevel() const
{
    return mMax2DArrayTextureLevel;
}

bool Context::supportsLuminanceTextures() const
{
    return mSupportsLuminanceTextures;
}

bool Context::supportsLuminanceAlphaTextures() const
{
    return mSupportsLuminanceAlphaTextures;
}

bool Context::supportsRGTextures() const
{
    return mSupportsRGTextures;
}

bool Context::supportsDepthTextures() const
{
    return mSupportsDepthTextures;
}

bool Context::supports32bitIndices() const
{
    return mSupports32bitIndices;
}

bool Context::supportsNonPower2Texture() const
{
    return mSupportsNonPower2Texture;
}

bool Context::supportsInstancing() const
{
    return mSupportsInstancing;
}

bool Context::supportsTextureFilterAnisotropy() const
{
    return mSupportsTextureFilterAnisotropy;
}

bool Context::supportsPBOs() const
{
    return mSupportsPBOs;
}

float Context::getTextureMaxAnisotropy() const
{
    return mMaxTextureAnisotropy;
}

bool Context::getCurrentReadFormatType(GLenum *internalFormat, GLenum *format, GLenum *type)
{
    Framebuffer *framebuffer = getReadFramebuffer();
    if (!framebuffer || framebuffer->completeness() != GL_FRAMEBUFFER_COMPLETE)
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    Renderbuffer *renderbuffer = framebuffer->getReadColorbuffer();
    if (!renderbuffer)
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    *internalFormat = renderbuffer->getActualFormat();
    *format = gl::GetFormat(renderbuffer->getActualFormat(), mClientVersion);
    *type = gl::GetType(renderbuffer->getActualFormat(), mClientVersion);

    return true;
}

void Context::detachBuffer(GLuint buffer)
{
    // [OpenGL ES 2.0.24] section 2.9 page 22:
    // If a buffer object is deleted while it is bound, all bindings to that object in the current context
    // (i.e. in the thread that called Delete-Buffers) are reset to zero.

    if (mState.arrayBuffer.id() == buffer)
    {
        mState.arrayBuffer.set(NULL);
    }

    // mark as freed among the vertex array objects
    for (auto vaoIt = mVertexArrayMap.begin(); vaoIt != mVertexArrayMap.end(); vaoIt++)
    {
        vaoIt->second->detachBuffer(buffer);
    }
}

void Context::detachTexture(GLuint texture)
{
    // [OpenGL ES 2.0.24] section 3.8 page 84:
    // If a texture object is deleted, it is as if all texture units which are bound to that texture object are
    // rebound to texture object zero

    for (int type = 0; type < TEXTURE_TYPE_COUNT; type++)
    {
        for (int sampler = 0; sampler < IMPLEMENTATION_MAX_COMBINED_TEXTURE_IMAGE_UNITS; sampler++)
        {
            if (mState.samplerTexture[type][sampler].id() == texture)
            {
                mState.samplerTexture[type][sampler].set(NULL);
            }
        }
    }

    // [OpenGL ES 2.0.24] section 4.4 page 112:
    // If a texture object is deleted while its image is attached to the currently bound framebuffer, then it is
    // as if FramebufferTexture2D had been called, with a texture of 0, for each attachment point to which this
    // image was attached in the currently bound framebuffer.

    Framebuffer *readFramebuffer = getReadFramebuffer();
    Framebuffer *drawFramebuffer = getDrawFramebuffer();

    if (readFramebuffer)
    {
        readFramebuffer->detachTexture(texture);
    }

    if (drawFramebuffer && drawFramebuffer != readFramebuffer)
    {
        drawFramebuffer->detachTexture(texture);
    }
}

void Context::detachFramebuffer(GLuint framebuffer)
{
    // [OpenGL ES 2.0.24] section 4.4 page 107:
    // If a framebuffer that is currently bound to the target FRAMEBUFFER is deleted, it is as though
    // BindFramebuffer had been executed with the target of FRAMEBUFFER and framebuffer of zero.

    if (mState.readFramebuffer == framebuffer)
    {
        bindReadFramebuffer(0);
    }

    if (mState.drawFramebuffer == framebuffer)
    {
        bindDrawFramebuffer(0);
    }
}

void Context::detachRenderbuffer(GLuint renderbuffer)
{
    // [OpenGL ES 2.0.24] section 4.4 page 109:
    // If a renderbuffer that is currently bound to RENDERBUFFER is deleted, it is as though BindRenderbuffer
    // had been executed with the target RENDERBUFFER and name of zero.

    if (mState.renderbuffer.id() == renderbuffer)
    {
        bindRenderbuffer(0);
    }

    // [OpenGL ES 2.0.24] section 4.4 page 111:
    // If a renderbuffer object is deleted while its image is attached to the currently bound framebuffer,
    // then it is as if FramebufferRenderbuffer had been called, with a renderbuffer of 0, for each attachment
    // point to which this image was attached in the currently bound framebuffer.

    Framebuffer *readFramebuffer = getReadFramebuffer();
    Framebuffer *drawFramebuffer = getDrawFramebuffer();

    if (readFramebuffer)
    {
        readFramebuffer->detachRenderbuffer(renderbuffer);
    }

    if (drawFramebuffer && drawFramebuffer != readFramebuffer)
    {
        drawFramebuffer->detachRenderbuffer(renderbuffer);
    }
}

void Context::detachVertexArray(GLuint vertexArray)
{
    // [OpenGL ES 3.0.2] section 2.10 page 43:
    // If a vertex array object that is currently bound is deleted, the binding
    // for that object reverts to zero and the default vertex array becomes current.
    if (mState.vertexArray == vertexArray)
    {
        bindVertexArray(0);
    }
}

void Context::detachTransformFeedback(GLuint transformFeedback)
{
    if (mState.transformFeedback.id() == transformFeedback)
    {
        bindTransformFeedback(0);
    }
}

void Context::detachSampler(GLuint sampler)
{
    // [OpenGL ES 3.0.2] section 3.8.2 pages 123-124:
    // If a sampler object that is currently bound to one or more texture units is
    // deleted, it is as though BindSampler is called once for each texture unit to
    // which the sampler is bound, with unit set to the texture unit and sampler set to zero.
    for (unsigned int textureUnit = 0; textureUnit < ArraySize(mState.samplers); textureUnit++)
    {
        if (mState.samplers[textureUnit] == sampler)
        {
            mState.samplers[textureUnit] = 0;
        }
    }
}

Texture *Context::getIncompleteTexture(TextureType type)
{
    Texture *t = mIncompleteTextures[type].get();

    if (t == NULL)
    {
        const GLubyte color[] = { 0, 0, 0, 255 };
        const PixelUnpackState incompleteUnpackState(1);

        switch (type)
        {
          default:
            UNREACHABLE();
            // default falls through to TEXTURE_2D

          case TEXTURE_2D:
            {
                Texture2D *incomplete2d = new Texture2D(mRenderer, Texture::INCOMPLETE_TEXTURE_ID);
                incomplete2d->setImage(0, 1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, incompleteUnpackState, color);
                t = incomplete2d;
            }
            break;

          case TEXTURE_CUBE:
            {
              TextureCubeMap *incompleteCube = new TextureCubeMap(mRenderer, Texture::INCOMPLETE_TEXTURE_ID);

              incompleteCube->setImagePosX(0, 1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, incompleteUnpackState, color);
              incompleteCube->setImageNegX(0, 1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, incompleteUnpackState, color);
              incompleteCube->setImagePosY(0, 1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, incompleteUnpackState, color);
              incompleteCube->setImageNegY(0, 1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, incompleteUnpackState, color);
              incompleteCube->setImagePosZ(0, 1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, incompleteUnpackState, color);
              incompleteCube->setImageNegZ(0, 1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, incompleteUnpackState, color);

              t = incompleteCube;
            }
            break;

          case TEXTURE_3D:
            {
                Texture3D *incomplete3d = new Texture3D(mRenderer, Texture::INCOMPLETE_TEXTURE_ID);
                incomplete3d->setImage(0, 1, 1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, incompleteUnpackState, color);

                t = incomplete3d;
            }
            break;

          case TEXTURE_2D_ARRAY:
            {
                Texture2DArray *incomplete2darray = new Texture2DArray(mRenderer, Texture::INCOMPLETE_TEXTURE_ID);
                incomplete2darray->setImage(0, 1, 1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, incompleteUnpackState, color);

                t = incomplete2darray;
            }
            break;
        }

        mIncompleteTextures[type].set(t);
    }

    return t;
}

bool Context::skipDraw(GLenum drawMode)
{
    if (drawMode == GL_POINTS)
    {
        // ProgramBinary assumes non-point rendering if gl_PointSize isn't written,
        // which affects varying interpolation. Since the value of gl_PointSize is
        // undefined when not written, just skip drawing to avoid unexpected results.
        if (!getCurrentProgramBinary()->usesPointSize())
        {
            // This is stictly speaking not an error, but developers should be 
            // notified of risking undefined behavior.
            ERR("Point rendering without writing to gl_PointSize.");

            return true;
        }
    }
    else if (IsTriangleMode(drawMode))
    {
        if (mState.rasterizer.cullFace && mState.rasterizer.cullMode == GL_FRONT_AND_BACK)
        {
            return true;
        }
    }

    return false;
}

void Context::setVertexAttribf(GLuint index, const GLfloat values[4])
{
    ASSERT(index < gl::MAX_VERTEX_ATTRIBS);
    mState.vertexAttribCurrentValues[index].setFloatValues(values);
}

void Context::setVertexAttribu(GLuint index, const GLuint values[4])
{
    ASSERT(index < gl::MAX_VERTEX_ATTRIBS);
    mState.vertexAttribCurrentValues[index].setUnsignedIntValues(values);
}

void Context::setVertexAttribi(GLuint index, const GLint values[4])
{
    ASSERT(index < gl::MAX_VERTEX_ATTRIBS);
    mState.vertexAttribCurrentValues[index].setIntValues(values);
}

void Context::setVertexAttribDivisor(GLuint index, GLuint divisor)
{
    getCurrentVertexArray()->setVertexAttribDivisor(index, divisor);
}

void Context::samplerParameteri(GLuint sampler, GLenum pname, GLint param)
{
    mResourceManager->checkSamplerAllocation(sampler);

    Sampler *samplerObject = getSampler(sampler);
    ASSERT(samplerObject);

    switch (pname)
    {
      case GL_TEXTURE_MIN_FILTER:    samplerObject->setMinFilter(static_cast<GLenum>(param));       break;
      case GL_TEXTURE_MAG_FILTER:    samplerObject->setMagFilter(static_cast<GLenum>(param));       break;
      case GL_TEXTURE_WRAP_S:        samplerObject->setWrapS(static_cast<GLenum>(param));           break;
      case GL_TEXTURE_WRAP_T:        samplerObject->setWrapT(static_cast<GLenum>(param));           break;
      case GL_TEXTURE_WRAP_R:        samplerObject->setWrapR(static_cast<GLenum>(param));           break;
      case GL_TEXTURE_MIN_LOD:       samplerObject->setMinLod(static_cast<GLfloat>(param));         break;
      case GL_TEXTURE_MAX_LOD:       samplerObject->setMaxLod(static_cast<GLfloat>(param));         break;
      case GL_TEXTURE_COMPARE_MODE:  samplerObject->setComparisonMode(static_cast<GLenum>(param));  break;
      case GL_TEXTURE_COMPARE_FUNC:  samplerObject->setComparisonFunc(static_cast<GLenum>(param));  break;
      default:                       UNREACHABLE(); break;
    }
}

void Context::samplerParameterf(GLuint sampler, GLenum pname, GLfloat param)
{
    mResourceManager->checkSamplerAllocation(sampler);

    Sampler *samplerObject = getSampler(sampler);
    ASSERT(samplerObject);

    switch (pname)
    {
      case GL_TEXTURE_MIN_FILTER:    samplerObject->setMinFilter(uiround<GLenum>(param));       break;
      case GL_TEXTURE_MAG_FILTER:    samplerObject->setMagFilter(uiround<GLenum>(param));       break;
      case GL_TEXTURE_WRAP_S:        samplerObject->setWrapS(uiround<GLenum>(param));           break;
      case GL_TEXTURE_WRAP_T:        samplerObject->setWrapT(uiround<GLenum>(param));           break;
      case GL_TEXTURE_WRAP_R:        samplerObject->setWrapR(uiround<GLenum>(param));           break;
      case GL_TEXTURE_MIN_LOD:       samplerObject->setMinLod(param);                                      break;
      case GL_TEXTURE_MAX_LOD:       samplerObject->setMaxLod(param);                                      break;
      case GL_TEXTURE_COMPARE_MODE:  samplerObject->setComparisonMode(uiround<GLenum>(param));  break;
      case GL_TEXTURE_COMPARE_FUNC:  samplerObject->setComparisonFunc(uiround<GLenum>(param));  break;
      default:                       UNREACHABLE(); break;
    }
}

GLint Context::getSamplerParameteri(GLuint sampler, GLenum pname)
{
    mResourceManager->checkSamplerAllocation(sampler);

    Sampler *samplerObject = getSampler(sampler);
    ASSERT(samplerObject);

    switch (pname)
    {
      case GL_TEXTURE_MIN_FILTER:    return static_cast<GLint>(samplerObject->getMinFilter());
      case GL_TEXTURE_MAG_FILTER:    return static_cast<GLint>(samplerObject->getMagFilter());
      case GL_TEXTURE_WRAP_S:        return static_cast<GLint>(samplerObject->getWrapS());
      case GL_TEXTURE_WRAP_T:        return static_cast<GLint>(samplerObject->getWrapT());
      case GL_TEXTURE_WRAP_R:        return static_cast<GLint>(samplerObject->getWrapR());
      case GL_TEXTURE_MIN_LOD:       return uiround<GLint>(samplerObject->getMinLod());
      case GL_TEXTURE_MAX_LOD:       return uiround<GLint>(samplerObject->getMaxLod());
      case GL_TEXTURE_COMPARE_MODE:  return static_cast<GLint>(samplerObject->getComparisonMode());
      case GL_TEXTURE_COMPARE_FUNC:  return static_cast<GLint>(samplerObject->getComparisonFunc());
      default:                       UNREACHABLE(); return 0;
    }
}

GLfloat Context::getSamplerParameterf(GLuint sampler, GLenum pname)
{
    mResourceManager->checkSamplerAllocation(sampler);

    Sampler *samplerObject = getSampler(sampler);
    ASSERT(samplerObject);

    switch (pname)
    {
      case GL_TEXTURE_MIN_FILTER:    return static_cast<GLfloat>(samplerObject->getMinFilter());
      case GL_TEXTURE_MAG_FILTER:    return static_cast<GLfloat>(samplerObject->getMagFilter());
      case GL_TEXTURE_WRAP_S:        return static_cast<GLfloat>(samplerObject->getWrapS());
      case GL_TEXTURE_WRAP_T:        return static_cast<GLfloat>(samplerObject->getWrapT());
      case GL_TEXTURE_WRAP_R:        return static_cast<GLfloat>(samplerObject->getWrapR());
      case GL_TEXTURE_MIN_LOD:       return samplerObject->getMinLod();
      case GL_TEXTURE_MAX_LOD:       return samplerObject->getMaxLod();
      case GL_TEXTURE_COMPARE_MODE:  return static_cast<GLfloat>(samplerObject->getComparisonMode());
      case GL_TEXTURE_COMPARE_FUNC:  return static_cast<GLfloat>(samplerObject->getComparisonFunc());
      default:                       UNREACHABLE(); return 0;
    }
}

// keep list sorted in following order
// OES extensions
// EXT extensions
// Vendor extensions
void Context::initExtensionString()
{
    // Do not report extension in GLES 3 contexts for now
    if (mClientVersion == 2)
    {
        // OES extensions
        if (supports32bitIndices())
        {
            mExtensionStringList.push_back("GL_OES_element_index_uint");
        }

        mExtensionStringList.push_back("GL_OES_packed_depth_stencil");
        mExtensionStringList.push_back("GL_OES_get_program_binary");
        mExtensionStringList.push_back("GL_OES_rgb8_rgba8");

        if (supportsPBOs())
        {
            mExtensionStringList.push_back("GL_OES_mapbuffer");
            mExtensionStringList.push_back("GL_EXT_map_buffer_range");
        }

        if (mRenderer->getDerivativeInstructionSupport())
        {
            mExtensionStringList.push_back("GL_OES_standard_derivatives");
        }

        if (supportsFloat16Textures())
        {
            mExtensionStringList.push_back("GL_OES_texture_half_float");
        }
        if (supportsFloat16LinearFilter())
        {
            mExtensionStringList.push_back("GL_OES_texture_half_float_linear");
        }
        if (supportsFloat32Textures())
        {
            mExtensionStringList.push_back("GL_OES_texture_float");
        }
        if (supportsFloat32LinearFilter())
        {
            mExtensionStringList.push_back("GL_OES_texture_float_linear");
        }

        if (supportsRGTextures())
        {
            mExtensionStringList.push_back("GL_EXT_texture_rg");
        }

        if (supportsNonPower2Texture())
        {
            mExtensionStringList.push_back("GL_OES_texture_npot");
        }

        // Multi-vendor (EXT) extensions
        if (supportsOcclusionQueries())
        {
            mExtensionStringList.push_back("GL_EXT_occlusion_query_boolean");
        }

        mExtensionStringList.push_back("GL_EXT_read_format_bgra");
        mExtensionStringList.push_back("GL_EXT_robustness");
        mExtensionStringList.push_back("GL_EXT_shader_texture_lod");

        if (supportsDXT1Textures())
        {
            mExtensionStringList.push_back("GL_EXT_texture_compression_dxt1");
        }

        if (supportsTextureFilterAnisotropy())
        {
            mExtensionStringList.push_back("GL_EXT_texture_filter_anisotropic");
        }

        if (supportsBGRATextures())
        {
            mExtensionStringList.push_back("GL_EXT_texture_format_BGRA8888");
        }

        if (mRenderer->getMaxRenderTargets() > 1)
        {
            mExtensionStringList.push_back("GL_EXT_draw_buffers");
        }

        mExtensionStringList.push_back("GL_EXT_texture_storage");
        mExtensionStringList.push_back("GL_EXT_frag_depth");
        mExtensionStringList.push_back("GL_EXT_blend_minmax");

        // ANGLE-specific extensions
        if (supportsDepthTextures())
        {
            mExtensionStringList.push_back("GL_ANGLE_depth_texture");
        }

        mExtensionStringList.push_back("GL_ANGLE_framebuffer_blit");
        if (getMaxSupportedSamples() != 0)
        {
            mExtensionStringList.push_back("GL_ANGLE_framebuffer_multisample");
        }

        if (supportsInstancing())
        {
            mExtensionStringList.push_back("GL_ANGLE_instanced_arrays");
        }

        mExtensionStringList.push_back("GL_ANGLE_pack_reverse_row_order");

        if (supportsDXT3Textures())
        {
            mExtensionStringList.push_back("GL_ANGLE_texture_compression_dxt3");
        }
        if (supportsDXT5Textures())
        {
            mExtensionStringList.push_back("GL_ANGLE_texture_compression_dxt5");
        }

        mExtensionStringList.push_back("GL_ANGLE_texture_usage");
        mExtensionStringList.push_back("GL_ANGLE_translated_shader_source");

        // Other vendor-specific extensions
        if (supportsEventQueries())
        {
            mExtensionStringList.push_back("GL_NV_fence");
        }
    }

    if (mClientVersion == 3)
    {
        mExtensionStringList.push_back("GL_EXT_color_buffer_float");

        mExtensionStringList.push_back("GL_EXT_read_format_bgra");

        if (supportsBGRATextures())
        {
            mExtensionStringList.push_back("GL_EXT_texture_format_BGRA8888");
        }
    }

    // Join the extension strings to one long string for use with GetString
    std::stringstream strstr;
    for (unsigned int extensionIndex = 0; extensionIndex < mExtensionStringList.size(); extensionIndex++)
    {
        strstr << mExtensionStringList[extensionIndex];
        strstr << " ";
    }

    mCombinedExtensionsString = makeStaticString(strstr.str());
}

const char *Context::getCombinedExtensionsString() const
{
    return mCombinedExtensionsString;
}

const char *Context::getExtensionString(const GLuint index) const
{
    ASSERT(index < mExtensionStringList.size());
    return mExtensionStringList[index].c_str();
}

unsigned int Context::getNumExtensions() const
{
    return mExtensionStringList.size();
}

void Context::initRendererString()
{
    std::ostringstream rendererString;
    rendererString << "ANGLE (";
    rendererString << mRenderer->getRendererDescription();
    rendererString << ")";

    mRendererString = makeStaticString(rendererString.str());
}

const char *Context::getRendererString() const
{
    return mRendererString;
}

Context::FramebufferTextureSerialSet Context::getBoundFramebufferTextureSerials()
{
    FramebufferTextureSerialSet set;

    Framebuffer *drawFramebuffer = getDrawFramebuffer();
    for (unsigned int i = 0; i < IMPLEMENTATION_MAX_DRAW_BUFFERS; i++)
    {
        Renderbuffer *renderBuffer = drawFramebuffer->getColorbuffer(i);
        if (renderBuffer && renderBuffer->getTextureSerial() != 0)
        {
            set.insert(renderBuffer->getTextureSerial());
        }
    }

    Renderbuffer *depthStencilBuffer = drawFramebuffer->getDepthOrStencilbuffer();
    if (depthStencilBuffer && depthStencilBuffer->getTextureSerial() != 0)
    {
        set.insert(depthStencilBuffer->getTextureSerial());
    }

    return set;
}

void Context::blitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                              GLbitfield mask, GLenum filter)
{
    Framebuffer *readFramebuffer = getReadFramebuffer();
    Framebuffer *drawFramebuffer = getDrawFramebuffer();

    bool blitRenderTarget = false;
    bool blitDepth = false;
    bool blitStencil = false;
    if ((mask & GL_COLOR_BUFFER_BIT) && readFramebuffer->getReadColorbuffer() && drawFramebuffer->getFirstColorbuffer())
    {
        blitRenderTarget = true;
    }
    if ((mask & GL_STENCIL_BUFFER_BIT) && readFramebuffer->getStencilbuffer() && drawFramebuffer->getStencilbuffer())
    {
        blitStencil = true;
    }
    if ((mask & GL_DEPTH_BUFFER_BIT) && readFramebuffer->getDepthbuffer() && drawFramebuffer->getDepthbuffer())
    {
        blitDepth = true;
    }

    gl::Rectangle srcRect(srcX0, srcY0, srcX1 - srcX0, srcY1 - srcY0);
    gl::Rectangle dstRect(dstX0, dstY0, dstX1 - dstX0, dstY1 - dstY0);
    if (blitRenderTarget || blitDepth || blitStencil)
    {
        const gl::Rectangle *scissor = mState.scissorTest ? &mState.scissor : NULL;
        mRenderer->blitRect(readFramebuffer, srcRect, drawFramebuffer, dstRect, scissor,
                            blitRenderTarget, blitDepth, blitStencil, filter);
    }
}

void Context::invalidateFrameBuffer(GLenum target, GLsizei numAttachments, const GLenum* attachments,
                                    GLint x, GLint y, GLsizei width, GLsizei height)
{
    Framebuffer *frameBuffer = NULL;
    switch (target)
    {
      case GL_FRAMEBUFFER:
      case GL_DRAW_FRAMEBUFFER:
        frameBuffer = getDrawFramebuffer();
        break;
      case GL_READ_FRAMEBUFFER:
        frameBuffer = getReadFramebuffer();
        break;
      default:
        UNREACHABLE();
    }

    if (frameBuffer && frameBuffer->completeness() == GL_FRAMEBUFFER_COMPLETE)
    {
        for (int i = 0; i < numAttachments; ++i)
        {
            rx::RenderTarget *renderTarget = NULL;

            if (attachments[i] >= GL_COLOR_ATTACHMENT0 && attachments[i] <= GL_COLOR_ATTACHMENT15)
            {
                gl::Renderbuffer *renderBuffer = frameBuffer->getColorbuffer(attachments[i] - GL_COLOR_ATTACHMENT0);
                if (renderBuffer)
                {
                    renderTarget = renderBuffer->getRenderTarget();
                }
            }
            else if (attachments[i] == GL_COLOR)
            {
                 gl::Renderbuffer *renderBuffer = frameBuffer->getColorbuffer(0);
                 if (renderBuffer)
                 {
                     renderTarget = renderBuffer->getRenderTarget();
                 }
            }
            else
            {
                gl::Renderbuffer *renderBuffer = NULL;
                switch (attachments[i])
                {
                  case GL_DEPTH_ATTACHMENT:
                  case GL_DEPTH:
                    renderBuffer = frameBuffer->getDepthbuffer();
                    break;
                  case GL_STENCIL_ATTACHMENT:
                  case GL_STENCIL:
                    renderBuffer = frameBuffer->getStencilbuffer();
                    break;
                  case GL_DEPTH_STENCIL_ATTACHMENT:
                    renderBuffer = frameBuffer->getDepthOrStencilbuffer();
                    break;
                  default:
                    UNREACHABLE();
                }

                if (renderBuffer)
                {
                    renderTarget = renderBuffer->getDepthStencil();
                }
            }

            if (renderTarget)
            {
                renderTarget->invalidate(x, y, width, height);
            }
        }
    }
}

bool Context::hasMappedBuffer(GLenum target) const
{
    if (target == GL_ARRAY_BUFFER)
    {
        for (unsigned int attribIndex = 0; attribIndex < gl::MAX_VERTEX_ATTRIBS; attribIndex++)
        {
            const gl::VertexAttribute &vertexAttrib = getVertexAttribState(attribIndex);
            gl::Buffer *boundBuffer = vertexAttrib.mBoundBuffer.get();
            if (vertexAttrib.mArrayEnabled && boundBuffer && boundBuffer->mapped())
            {
                return true;
            }
        }
    }
    else if (target == GL_ELEMENT_ARRAY_BUFFER)
    {
        Buffer *elementBuffer = getElementArrayBuffer();
        return (elementBuffer && elementBuffer->mapped());
    }
    else if (target == GL_TRANSFORM_FEEDBACK_BUFFER)
    {
        UNIMPLEMENTED();
    }
    else UNREACHABLE();
    return false;
}

}

extern "C"
{
gl::Context *glCreateContext(int clientVersion, const gl::Context *shareContext, rx::Renderer *renderer, bool notifyResets, bool robustAccess)
{
    return new gl::Context(clientVersion, shareContext, renderer, notifyResets, robustAccess);
}

void glDestroyContext(gl::Context *context)
{
    delete context;

    if (context == gl::getContext())
    {
        gl::makeCurrent(NULL, NULL, NULL);
    }
}

void glMakeCurrent(gl::Context *context, egl::Display *display, egl::Surface *surface)
{
    gl::makeCurrent(context, display, surface);
}

gl::Context *glGetCurrentContext()
{
    return gl::getContext();
}

}
