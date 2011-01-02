//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Context.cpp: Implements the gl::Context class, managing all GL state and performing
// rendering operations. It is the GLES2 specific implementation of EGLContext.

#include "libGLESv2/Context.h"

#include <algorithm>

#include "libEGL/Display.h"

#include "libGLESv2/main.h"
#include "libGLESv2/mathutil.h"
#include "libGLESv2/utilities.h"
#include "libGLESv2/Blit.h"
#include "libGLESv2/ResourceManager.h"
#include "libGLESv2/Buffer.h"
#include "libGLESv2/FrameBuffer.h"
#include "libGLESv2/Program.h"
#include "libGLESv2/RenderBuffer.h"
#include "libGLESv2/Shader.h"
#include "libGLESv2/Texture.h"
#include "libGLESv2/geometry/backend.h"
#include "libGLESv2/geometry/VertexDataManager.h"
#include "libGLESv2/geometry/IndexDataManager.h"
#include "libGLESv2/geometry/dx9.h"

#undef near
#undef far

namespace gl
{
Context::Context(const egl::Config *config, const gl::Context *shareContext)
    : mConfig(config)
{
    setClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    mState.depthClearValue = 1.0f;
    mState.stencilClearValue = 0;

    mState.cullFace = false;
    mState.cullMode = GL_BACK;
    mState.frontFace = GL_CCW;
    mState.depthTest = false;
    mState.depthFunc = GL_LESS;
    mState.blend = false;
    mState.sourceBlendRGB = GL_ONE;
    mState.sourceBlendAlpha = GL_ONE;
    mState.destBlendRGB = GL_ZERO;
    mState.destBlendAlpha = GL_ZERO;
    mState.blendEquationRGB = GL_FUNC_ADD;
    mState.blendEquationAlpha = GL_FUNC_ADD;
    mState.blendColor.red = 0;
    mState.blendColor.green = 0;
    mState.blendColor.blue = 0;
    mState.blendColor.alpha = 0;
    mState.stencilTest = false;
    mState.stencilFunc = GL_ALWAYS;
    mState.stencilRef = 0;
    mState.stencilMask = -1;
    mState.stencilWritemask = -1;
    mState.stencilBackFunc = GL_ALWAYS;
    mState.stencilBackRef = 0;
    mState.stencilBackMask = - 1;
    mState.stencilBackWritemask = -1;
    mState.stencilFail = GL_KEEP;
    mState.stencilPassDepthFail = GL_KEEP;
    mState.stencilPassDepthPass = GL_KEEP;
    mState.stencilBackFail = GL_KEEP;
    mState.stencilBackPassDepthFail = GL_KEEP;
    mState.stencilBackPassDepthPass = GL_KEEP;
    mState.polygonOffsetFill = false;
    mState.polygonOffsetFactor = 0.0f;
    mState.polygonOffsetUnits = 0.0f;
    mState.sampleAlphaToCoverage = false;
    mState.sampleCoverage = false;
    mState.sampleCoverageValue = 1.0f;
    mState.sampleCoverageInvert = false;
    mState.scissorTest = false;
    mState.dither = true;
    mState.generateMipmapHint = GL_DONT_CARE;

    mState.lineWidth = 1.0f;

    mState.viewportX = 0;
    mState.viewportY = 0;
    mState.viewportWidth = config->mDisplayMode.Width;
    mState.viewportHeight = config->mDisplayMode.Height;
    mState.zNear = 0.0f;
    mState.zFar = 1.0f;

    mState.scissorX = 0;
    mState.scissorY = 0;
    mState.scissorWidth = config->mDisplayMode.Width;
    mState.scissorHeight = config->mDisplayMode.Height;

    mState.colorMaskRed = true;
    mState.colorMaskGreen = true;
    mState.colorMaskBlue = true;
    mState.colorMaskAlpha = true;
    mState.depthMask = true;

    if (shareContext != NULL)
    {
        mResourceManager = shareContext->mResourceManager;
        mResourceManager->addRef();
    }
    else
    {
        mResourceManager = new ResourceManager();
    }

    // [OpenGL ES 2.0.24] section 3.7 page 83:
    // In the initial state, TEXTURE_2D and TEXTURE_CUBE_MAP have twodimensional
    // and cube map texture state vectors respectively associated with them.
    // In order that access to these initial textures not be lost, they are treated as texture
    // objects all of whose names are 0.

    mTexture2DZero = new Texture2D(0);
    mTextureCubeMapZero = new TextureCubeMap(0);

    mColorbufferZero = NULL;
    mDepthStencilbufferZero = NULL;

    mState.activeSampler = 0;
    bindArrayBuffer(0);
    bindElementArrayBuffer(0);
    bindTextureCubeMap(0);
    bindTexture2D(0);
    bindReadFramebuffer(0);
    bindDrawFramebuffer(0);
    bindRenderbuffer(0);

    for (int type = 0; type < SAMPLER_TYPE_COUNT; type++)
    {
        mIncompleteTextures[type] = NULL;
    }

    mState.currentProgram = 0;

    mState.packAlignment = 4;
    mState.unpackAlignment = 4;

    mBufferBackEnd = NULL;
    mVertexDataManager = NULL;
    mIndexDataManager = NULL;
    mBlit = NULL;

    mInvalidEnum = false;
    mInvalidValue = false;
    mInvalidOperation = false;
    mOutOfMemory = false;
    mInvalidFramebufferOperation = false;

    mHasBeenCurrent = false;

    mMaxSupportedSamples = 0;
    mMaskedClearSavedState = NULL;
    markAllStateDirty();
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

    while (!mFramebufferMap.empty())
    {
        deleteFramebuffer(mFramebufferMap.begin()->first);
    }

    while (!mMultiSampleSupport.empty())
    {
        delete [] mMultiSampleSupport.begin()->second;
        mMultiSampleSupport.erase(mMultiSampleSupport.begin());
    }

    for (int type = 0; type < SAMPLER_TYPE_COUNT; type++)
    {
        for (int sampler = 0; sampler < MAX_TEXTURE_IMAGE_UNITS; sampler++)
        {
            mState.samplerTexture[type][sampler].set(NULL);
        }
    }

    for (int type = 0; type < SAMPLER_TYPE_COUNT; type++)
    {
        delete mIncompleteTextures[type];
    }

    for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
    {
        mState.vertexAttribute[i].mBoundBuffer.set(NULL);
    }

    mState.arrayBuffer.set(NULL);
    mState.elementArrayBuffer.set(NULL);
    mState.texture2D.set(NULL);
    mState.textureCubeMap.set(NULL);
    mState.renderbuffer.set(NULL);

    delete mTexture2DZero;
    delete mTextureCubeMapZero;

    delete mBufferBackEnd;
    delete mVertexDataManager;
    delete mIndexDataManager;
    delete mBlit;

    if (mMaskedClearSavedState)
    {
        mMaskedClearSavedState->Release();
    }

    mResourceManager->release();
}

void Context::makeCurrent(egl::Display *display, egl::Surface *surface)
{
    IDirect3DDevice9 *device = display->getDevice();

    if (!mHasBeenCurrent)
    {
        mDeviceCaps = display->getDeviceCaps();

        mBufferBackEnd = new Dx9BackEnd(this, device);
        mVertexDataManager = new VertexDataManager(this, mBufferBackEnd);
        mIndexDataManager = new IndexDataManager(this, mBufferBackEnd);
        mBlit = new Blit(this);

        const D3DFORMAT renderBufferFormats[] =
        {
            D3DFMT_A8R8G8B8,
            D3DFMT_X8R8G8B8,
            D3DFMT_R5G6B5,
            D3DFMT_D24S8
        };

        int max = 0;
        for (int i = 0; i < sizeof(renderBufferFormats) / sizeof(D3DFORMAT); ++i)
        {
            bool *multisampleArray = new bool[D3DMULTISAMPLE_16_SAMPLES + 1];
            display->getMultiSampleSupport(renderBufferFormats[i], multisampleArray);
            mMultiSampleSupport[renderBufferFormats[i]] = multisampleArray;

            for (int j = D3DMULTISAMPLE_16_SAMPLES; j >= 0; --j)
            {
                if (multisampleArray[j] && j != D3DMULTISAMPLE_NONMASKABLE && j > max)
                {
                    max = j;
                }
            }
        }

        mMaxSupportedSamples = max;

        mSupportsCompressedTextures = display->getCompressedTextureSupport();

        initExtensionString();

        mState.viewportX = 0;
        mState.viewportY = 0;
        mState.viewportWidth = surface->getWidth();
        mState.viewportHeight = surface->getHeight();

        mState.scissorX = 0;
        mState.scissorY = 0;
        mState.scissorWidth = surface->getWidth();
        mState.scissorHeight = surface->getHeight();

        mHasBeenCurrent = true;
    }

    // Wrap the existing Direct3D 9 resources into GL objects and assign them to the '0' names
    IDirect3DSurface9 *defaultRenderTarget = surface->getRenderTarget();
    IDirect3DSurface9 *depthStencil = surface->getDepthStencil();

    Colorbuffer *colorbufferZero = new Colorbuffer(defaultRenderTarget);
    DepthStencilbuffer *depthStencilbufferZero = new DepthStencilbuffer(depthStencil);
    Framebuffer *framebufferZero = new DefaultFramebuffer(colorbufferZero, depthStencilbufferZero);

    setFramebufferZero(framebufferZero);

    defaultRenderTarget->Release();

    if (depthStencil)
    {
        depthStencil->Release();
    }
    
    mSupportsShaderModel3 = mDeviceCaps.PixelShaderVersion == D3DPS_VERSION(3, 0);

    markAllStateDirty();
}

// This function will set all of the state-related dirty flags, so that all state is set during next pre-draw.
void Context::markAllStateDirty()
{
    mAppliedRenderTargetSerial = 0;
    mAppliedDepthbufferSerial = 0;
    mAppliedProgram = 0;

    mClearStateDirty = true;
    mCullStateDirty = true;
    mDepthStateDirty = true;
    mMaskStateDirty = true;
    mBlendStateDirty = true;
    mStencilStateDirty = true;
    mPolygonOffsetStateDirty = true;
    mScissorStateDirty = true;
    mSampleStateDirty = true;
    mDitherStateDirty = true;
    mFrontFaceDirty = true;

    if (mBufferBackEnd != NULL)
    {
        mBufferBackEnd->invalidate();
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

void Context::setCullFace(bool enabled)
{
    if (mState.cullFace != enabled)
    {
        mState.cullFace = enabled;
        mCullStateDirty = true;
    }
}

bool Context::isCullFaceEnabled() const
{
    return mState.cullFace;
}

void Context::setCullMode(GLenum mode)
{
    if (mState.cullMode != mode)
    {
        mState.cullMode = mode;
        mCullStateDirty = true;
    }
}

void Context::setFrontFace(GLenum front)
{
    if (mState.frontFace != front)
    {
        mState.frontFace = front;
        mFrontFaceDirty = true;
    }
}

void Context::setDepthTest(bool enabled)
{
    if (mState.depthTest != enabled)
    {
        mState.depthTest = enabled;
        mDepthStateDirty = true;
    }
}

bool Context::isDepthTestEnabled() const
{
    return mState.depthTest;
}

void Context::setDepthFunc(GLenum depthFunc)
{
    if (mState.depthFunc != depthFunc)
    {
        mState.depthFunc = depthFunc;
        mDepthStateDirty = true;
    }
}

void Context::setDepthRange(float zNear, float zFar)
{
    mState.zNear = zNear;
    mState.zFar = zFar;
}

void Context::setBlend(bool enabled)
{
    if (mState.blend != enabled)
    {
        mState.blend = enabled;
        mBlendStateDirty = true;
    }
}

bool Context::isBlendEnabled() const
{
    return mState.blend;
}

void Context::setBlendFactors(GLenum sourceRGB, GLenum destRGB, GLenum sourceAlpha, GLenum destAlpha)
{
    if (mState.sourceBlendRGB != sourceRGB ||
        mState.sourceBlendAlpha != sourceAlpha ||
        mState.destBlendRGB != destRGB ||
        mState.destBlendAlpha != destAlpha)
    {
        mState.sourceBlendRGB = sourceRGB;
        mState.destBlendRGB = destRGB;
        mState.sourceBlendAlpha = sourceAlpha;
        mState.destBlendAlpha = destAlpha;
        mBlendStateDirty = true;
    }
}

void Context::setBlendColor(float red, float green, float blue, float alpha)
{
    if (mState.blendColor.red != red ||
        mState.blendColor.green != green ||
        mState.blendColor.blue != blue ||
        mState.blendColor.alpha != alpha)
    {
        mState.blendColor.red = red;
        mState.blendColor.green = green;
        mState.blendColor.blue = blue;
        mState.blendColor.alpha = alpha;
        mBlendStateDirty = true;
    }
}

void Context::setBlendEquation(GLenum rgbEquation, GLenum alphaEquation)
{
    if (mState.blendEquationRGB != rgbEquation ||
        mState.blendEquationAlpha != alphaEquation)
    {
        mState.blendEquationRGB = rgbEquation;
        mState.blendEquationAlpha = alphaEquation;
        mBlendStateDirty = true;
    }
}

void Context::setStencilTest(bool enabled)
{
    if (mState.stencilTest != enabled)
    {
        mState.stencilTest = enabled;
        mStencilStateDirty = true;
    }
}

bool Context::isStencilTestEnabled() const
{
    return mState.stencilTest;
}

void Context::setStencilParams(GLenum stencilFunc, GLint stencilRef, GLuint stencilMask)
{
    if (mState.stencilFunc != stencilFunc ||
        mState.stencilRef != stencilRef ||
        mState.stencilMask != stencilMask)
    {
        mState.stencilFunc = stencilFunc;
        mState.stencilRef = (stencilRef > 0) ? stencilRef : 0;
        mState.stencilMask = stencilMask;
        mStencilStateDirty = true;
    }
}

void Context::setStencilBackParams(GLenum stencilBackFunc, GLint stencilBackRef, GLuint stencilBackMask)
{
    if (mState.stencilBackFunc != stencilBackFunc ||
        mState.stencilBackRef != stencilBackRef ||
        mState.stencilBackMask != stencilBackMask)
    {
        mState.stencilBackFunc = stencilBackFunc;
        mState.stencilBackRef = (stencilBackRef > 0) ? stencilBackRef : 0;
        mState.stencilBackMask = stencilBackMask;
        mStencilStateDirty = true;
    }
}

void Context::setStencilWritemask(GLuint stencilWritemask)
{
    if (mState.stencilWritemask != stencilWritemask)
    {
        mState.stencilWritemask = stencilWritemask;
        mStencilStateDirty = true;
    }
}

void Context::setStencilBackWritemask(GLuint stencilBackWritemask)
{
    if (mState.stencilBackWritemask != stencilBackWritemask)
    {
        mState.stencilBackWritemask = stencilBackWritemask;
        mStencilStateDirty = true;
    }
}

void Context::setStencilOperations(GLenum stencilFail, GLenum stencilPassDepthFail, GLenum stencilPassDepthPass)
{
    if (mState.stencilFail != stencilFail ||
        mState.stencilPassDepthFail != stencilPassDepthFail ||
        mState.stencilPassDepthPass != stencilPassDepthPass)
    {
        mState.stencilFail = stencilFail;
        mState.stencilPassDepthFail = stencilPassDepthFail;
        mState.stencilPassDepthPass = stencilPassDepthPass;
        mStencilStateDirty = true;
    }
}

void Context::setStencilBackOperations(GLenum stencilBackFail, GLenum stencilBackPassDepthFail, GLenum stencilBackPassDepthPass)
{
    if (mState.stencilBackFail != stencilBackFail ||
        mState.stencilBackPassDepthFail != stencilBackPassDepthFail ||
        mState.stencilBackPassDepthPass != stencilBackPassDepthPass)
    {
        mState.stencilBackFail = stencilBackFail;
        mState.stencilBackPassDepthFail = stencilBackPassDepthFail;
        mState.stencilBackPassDepthPass = stencilBackPassDepthPass;
        mStencilStateDirty = true;
    }
}

void Context::setPolygonOffsetFill(bool enabled)
{
    if (mState.polygonOffsetFill != enabled)
    {
        mState.polygonOffsetFill = enabled;
        mPolygonOffsetStateDirty = true;
    }
}

bool Context::isPolygonOffsetFillEnabled() const
{
    return mState.polygonOffsetFill;

}

void Context::setPolygonOffsetParams(GLfloat factor, GLfloat units)
{
    if (mState.polygonOffsetFactor != factor ||
        mState.polygonOffsetUnits != units)
    {
        mState.polygonOffsetFactor = factor;
        mState.polygonOffsetUnits = units;
        mPolygonOffsetStateDirty = true;
    }
}

void Context::setSampleAlphaToCoverage(bool enabled)
{
    if (mState.sampleAlphaToCoverage != enabled)
    {
        mState.sampleAlphaToCoverage = enabled;
        mSampleStateDirty = true;
    }
}

bool Context::isSampleAlphaToCoverageEnabled() const
{
    return mState.sampleAlphaToCoverage;
}

void Context::setSampleCoverage(bool enabled)
{
    if (mState.sampleCoverage != enabled)
    {
        mState.sampleCoverage = enabled;
        mSampleStateDirty = true;
    }
}

bool Context::isSampleCoverageEnabled() const
{
    return mState.sampleCoverage;
}

void Context::setSampleCoverageParams(GLclampf value, bool invert)
{
    if (mState.sampleCoverageValue != value ||
        mState.sampleCoverageInvert != invert)
    {
        mState.sampleCoverageValue = value;
        mState.sampleCoverageInvert = invert;
        mSampleStateDirty = true;
    }
}

void Context::setScissorTest(bool enabled)
{
    if (mState.scissorTest != enabled)
    {
        mState.scissorTest = enabled;
        mScissorStateDirty = true;
    }
}

bool Context::isScissorTestEnabled() const
{
    return mState.scissorTest;
}

void Context::setDither(bool enabled)
{
    if (mState.dither != enabled)
    {
        mState.dither = enabled;
        mDitherStateDirty = true;
    }
}

bool Context::isDitherEnabled() const
{
    return mState.dither;
}

void Context::setLineWidth(GLfloat width)
{
    mState.lineWidth = width;
}

void Context::setGenerateMipmapHint(GLenum hint)
{
    mState.generateMipmapHint = hint;
}

void Context::setViewportParams(GLint x, GLint y, GLsizei width, GLsizei height)
{
    mState.viewportX = x;
    mState.viewportY = y;
    mState.viewportWidth = width;
    mState.viewportHeight = height;
}

void Context::setScissorParams(GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (mState.scissorX != x || mState.scissorY != y || 
        mState.scissorWidth != width || mState.scissorHeight != height)
    {
        mState.scissorX = x;
        mState.scissorY = y;
        mState.scissorWidth = width;
        mState.scissorHeight = height;
        mScissorStateDirty = true;
    }
}

void Context::setColorMask(bool red, bool green, bool blue, bool alpha)
{
    if (mState.colorMaskRed != red || mState.colorMaskGreen != green ||
        mState.colorMaskBlue != blue || mState.colorMaskAlpha != alpha)
    {
        mState.colorMaskRed = red;
        mState.colorMaskGreen = green;
        mState.colorMaskBlue = blue;
        mState.colorMaskAlpha = alpha;
        mMaskStateDirty = true;
    }
}

void Context::setDepthMask(bool mask)
{
    if (mState.depthMask != mask)
    {
        mState.depthMask = mask;
        mMaskStateDirty = true;
    }
}

void Context::setActiveSampler(int active)
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

GLuint Context::getArrayBufferHandle() const
{
    return mState.arrayBuffer.id();
}

void Context::setVertexAttribEnabled(unsigned int attribNum, bool enabled)
{
    mState.vertexAttribute[attribNum].mEnabled = enabled;
}

const AttributeState &Context::getVertexAttribState(unsigned int attribNum)
{
    return mState.vertexAttribute[attribNum];
}

void Context::setVertexAttribState(unsigned int attribNum, Buffer *boundBuffer, GLint size, GLenum type, bool normalized,
                                   GLsizei stride, const void *pointer)
{
    mState.vertexAttribute[attribNum].mBoundBuffer.set(boundBuffer);
    mState.vertexAttribute[attribNum].mSize = size;
    mState.vertexAttribute[attribNum].mType = type;
    mState.vertexAttribute[attribNum].mNormalized = normalized;
    mState.vertexAttribute[attribNum].mStride = stride;
    mState.vertexAttribute[attribNum].mPointer = pointer;
}

const void *Context::getVertexAttribPointer(unsigned int attribNum) const
{
    return mState.vertexAttribute[attribNum].mPointer;
}

// returns entire set of attributes as a block
const AttributeState *Context::getVertexAttribBlock()
{
    return mState.vertexAttribute;
}

void Context::setPackAlignment(GLint alignment)
{
    mState.packAlignment = alignment;
}

GLint Context::getPackAlignment() const
{
    return mState.packAlignment;
}

void Context::setUnpackAlignment(GLint alignment)
{
    mState.unpackAlignment = alignment;
}

GLint Context::getUnpackAlignment() const
{
    return mState.unpackAlignment;
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

// Returns an unused framebuffer name
GLuint Context::createFramebuffer()
{
    unsigned int handle = 1;

    while (mFramebufferMap.find(handle) != mFramebufferMap.end())
    {
        handle++;
    }

    mFramebufferMap[handle] = NULL;

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

void Context::deleteFramebuffer(GLuint framebuffer)
{
    FramebufferMap::iterator framebufferObject = mFramebufferMap.find(framebuffer);

    if (framebufferObject != mFramebufferMap.end())
    {
        detachFramebuffer(framebuffer);

        delete framebufferObject->second;
        mFramebufferMap.erase(framebufferObject);
    }
}

Buffer *Context::getBuffer(GLuint handle)
{
    return mResourceManager->getBuffer(handle);
}

Shader *Context::getShader(GLuint handle)
{
    return mResourceManager->getShader(handle);
}

Program *Context::getProgram(GLuint handle)
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

Framebuffer *Context::getReadFramebuffer()
{
    return getFramebuffer(mState.readFramebuffer);
}

Framebuffer *Context::getDrawFramebuffer()
{
    return getFramebuffer(mState.drawFramebuffer);
}

void Context::bindArrayBuffer(unsigned int buffer)
{
    mResourceManager->checkBufferAllocation(buffer);

    mState.arrayBuffer.set(getBuffer(buffer));
}

void Context::bindElementArrayBuffer(unsigned int buffer)
{
    mResourceManager->checkBufferAllocation(buffer);

    mState.elementArrayBuffer.set(getBuffer(buffer));
}

void Context::bindTexture2D(GLuint texture)
{
    mResourceManager->checkTextureAllocation(texture, SAMPLER_2D);

    mState.texture2D.set(getTexture(texture));

    mState.samplerTexture[SAMPLER_2D][mState.activeSampler].set(mState.texture2D.get());
}

void Context::bindTextureCubeMap(GLuint texture)
{
    mResourceManager->checkTextureAllocation(texture, SAMPLER_CUBE);

    mState.textureCubeMap.set(getTexture(texture));

    mState.samplerTexture[SAMPLER_CUBE][mState.activeSampler].set(mState.textureCubeMap.get());
}

void Context::bindReadFramebuffer(GLuint framebuffer)
{
    if (!getFramebuffer(framebuffer))
    {
        mFramebufferMap[framebuffer] = new Framebuffer();
    }

    mState.readFramebuffer = framebuffer;
}

void Context::bindDrawFramebuffer(GLuint framebuffer)
{
    if (!getFramebuffer(framebuffer))
    {
        mFramebufferMap[framebuffer] = new Framebuffer();
    }

    mState.drawFramebuffer = framebuffer;
}

void Context::bindRenderbuffer(GLuint renderbuffer)
{
    mResourceManager->checkRenderbufferAllocation(renderbuffer);

    mState.renderbuffer.set(getRenderbuffer(renderbuffer));
}

void Context::useProgram(GLuint program)
{
    GLuint priorProgram = mState.currentProgram;
    mState.currentProgram = program;               // Must switch before trying to delete, otherwise it only gets flagged.

    if (priorProgram != program)
    {
        Program *newProgram = mResourceManager->getProgram(program);
        Program *oldProgram = mResourceManager->getProgram(priorProgram);

        if (newProgram)
        {
            newProgram->addRef();
        }
        
        if (oldProgram)
        {
            oldProgram->release();
        }
    }
}

void Context::setFramebufferZero(Framebuffer *buffer)
{
    delete mFramebufferMap[0];
    mFramebufferMap[0] = buffer;
}

void Context::setRenderbufferStorage(RenderbufferStorage *renderbuffer)
{
    Renderbuffer *renderbufferObject = mState.renderbuffer.get();
    renderbufferObject->setStorage(renderbuffer);
}

Framebuffer *Context::getFramebuffer(unsigned int handle)
{
    FramebufferMap::iterator framebuffer = mFramebufferMap.find(handle);

    if (framebuffer == mFramebufferMap.end())
    {
        return NULL;
    }
    else
    {
        return framebuffer->second;
    }
}

Buffer *Context::getArrayBuffer()
{
    return mState.arrayBuffer.get();
}

Buffer *Context::getElementArrayBuffer()
{
    return mState.elementArrayBuffer.get();
}

Program *Context::getCurrentProgram()
{
    return mResourceManager->getProgram(mState.currentProgram);
}

Texture2D *Context::getTexture2D()
{
    if (mState.texture2D.id() == 0)   // Special case: 0 refers to different initial textures based on the target
    {
        return mTexture2DZero;
    }

    return static_cast<Texture2D*>(mState.texture2D.get());
}

TextureCubeMap *Context::getTextureCubeMap()
{
    if (mState.textureCubeMap.id() == 0)   // Special case: 0 refers to different initial textures based on the target
    {
        return mTextureCubeMapZero;
    }

    return static_cast<TextureCubeMap*>(mState.textureCubeMap.get());
}

Texture *Context::getSamplerTexture(unsigned int sampler, SamplerType type)
{
    GLuint texid = mState.samplerTexture[type][sampler].id();

    if (texid == 0)
    {
        switch (type)
        {
          default: UNREACHABLE();
          case SAMPLER_2D: return mTexture2DZero;
          case SAMPLER_CUBE: return mTextureCubeMapZero;
        }
    }

    return mState.samplerTexture[type][sampler].get();
}

bool Context::getBooleanv(GLenum pname, GLboolean *params)
{
    switch (pname)
    {
      case GL_SHADER_COMPILER:          *params = GL_TRUE;                          break;
      case GL_SAMPLE_COVERAGE_INVERT:   *params = mState.sampleCoverageInvert;      break;
      case GL_DEPTH_WRITEMASK:          *params = mState.depthMask;                 break;
      case GL_COLOR_WRITEMASK:
        params[0] = mState.colorMaskRed;
        params[1] = mState.colorMaskGreen;
        params[2] = mState.colorMaskBlue;
        params[3] = mState.colorMaskAlpha;
        break;
      case GL_CULL_FACE:                *params = mState.cullFace;
      case GL_POLYGON_OFFSET_FILL:      *params = mState.polygonOffsetFill;
      case GL_SAMPLE_ALPHA_TO_COVERAGE: *params = mState.sampleAlphaToCoverage;
      case GL_SAMPLE_COVERAGE:          *params = mState.sampleCoverage;
      case GL_SCISSOR_TEST:             *params = mState.scissorTest;
      case GL_STENCIL_TEST:             *params = mState.stencilTest;
      case GL_DEPTH_TEST:               *params = mState.depthTest;
      case GL_BLEND:                    *params = mState.blend;
      case GL_DITHER:                   *params = mState.dither;
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
      case GL_LINE_WIDTH:               *params = mState.lineWidth;            break;
      case GL_SAMPLE_COVERAGE_VALUE:    *params = mState.sampleCoverageValue;  break;
      case GL_DEPTH_CLEAR_VALUE:        *params = mState.depthClearValue;      break;
      case GL_POLYGON_OFFSET_FACTOR:    *params = mState.polygonOffsetFactor;  break;
      case GL_POLYGON_OFFSET_UNITS:     *params = mState.polygonOffsetUnits;   break;
      case GL_ALIASED_LINE_WIDTH_RANGE:
        params[0] = gl::ALIASED_LINE_WIDTH_RANGE_MIN;
        params[1] = gl::ALIASED_LINE_WIDTH_RANGE_MAX;
        break;
      case GL_ALIASED_POINT_SIZE_RANGE:
        params[0] = gl::ALIASED_POINT_SIZE_RANGE_MIN;
        params[1] = supportsShaderModel3() ? gl::ALIASED_POINT_SIZE_RANGE_MAX_SM3 : gl::ALIASED_POINT_SIZE_RANGE_MAX_SM2;
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
      default:
        return false;
    }

    return true;
}

bool Context::getIntegerv(GLenum pname, GLint *params)
{
    // Please note: DEPTH_CLEAR_VALUE is not included in our internal getIntegerv implementation
    // because it is stored as a float, despite the fact that the GL ES 2.0 spec names
    // GetIntegerv as its native query function. As it would require conversion in any
    // case, this should make no difference to the calling application. You may find it in 
    // Context::getFloatv.
    switch (pname)
    {
      case GL_MAX_VERTEX_ATTRIBS:               *params = gl::MAX_VERTEX_ATTRIBS;               break;
      case GL_MAX_VERTEX_UNIFORM_VECTORS:       *params = gl::MAX_VERTEX_UNIFORM_VECTORS;       break;
      case GL_MAX_VARYING_VECTORS:              *params = gl::MAX_VARYING_VECTORS;              break;
      case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS: *params = gl::MAX_COMBINED_TEXTURE_IMAGE_UNITS; break;
      case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:   *params = gl::MAX_VERTEX_TEXTURE_IMAGE_UNITS;   break;
      case GL_MAX_TEXTURE_IMAGE_UNITS:          *params = gl::MAX_TEXTURE_IMAGE_UNITS;          break;
      case GL_MAX_FRAGMENT_UNIFORM_VECTORS:     *params = gl::MAX_FRAGMENT_UNIFORM_VECTORS;     break;
      case GL_MAX_RENDERBUFFER_SIZE:            *params = gl::MAX_RENDERBUFFER_SIZE;            break;
      case GL_NUM_SHADER_BINARY_FORMATS:        *params = 0;                                    break;
      case GL_SHADER_BINARY_FORMATS:      /* no shader binary formats are supported */          break;
      case GL_ARRAY_BUFFER_BINDING:             *params = mState.arrayBuffer.id();              break;
      case GL_ELEMENT_ARRAY_BUFFER_BINDING:     *params = mState.elementArrayBuffer.id();       break;
      //case GL_FRAMEBUFFER_BINDING:              // now equivalent to GL_DRAW_FRAMEBUFFER_BINDING_ANGLE
      case GL_DRAW_FRAMEBUFFER_BINDING_ANGLE:     *params = mState.drawFramebuffer;               break;
      case GL_READ_FRAMEBUFFER_BINDING_ANGLE:     *params = mState.readFramebuffer;               break;
      case GL_RENDERBUFFER_BINDING:             *params = mState.renderbuffer.id();             break;
      case GL_CURRENT_PROGRAM:                  *params = mState.currentProgram;                break;
      case GL_PACK_ALIGNMENT:                   *params = mState.packAlignment;                 break;
      case GL_UNPACK_ALIGNMENT:                 *params = mState.unpackAlignment;               break;
      case GL_GENERATE_MIPMAP_HINT:             *params = mState.generateMipmapHint;            break;
      case GL_ACTIVE_TEXTURE:                   *params = (mState.activeSampler + GL_TEXTURE0); break;
      case GL_STENCIL_FUNC:                     *params = mState.stencilFunc;                   break;
      case GL_STENCIL_REF:                      *params = mState.stencilRef;                    break;
      case GL_STENCIL_VALUE_MASK:               *params = mState.stencilMask;                   break;
      case GL_STENCIL_BACK_FUNC:                *params = mState.stencilBackFunc;               break;
      case GL_STENCIL_BACK_REF:                 *params = mState.stencilBackRef;                break;
      case GL_STENCIL_BACK_VALUE_MASK:          *params = mState.stencilBackMask;               break;
      case GL_STENCIL_FAIL:                     *params = mState.stencilFail;                   break;
      case GL_STENCIL_PASS_DEPTH_FAIL:          *params = mState.stencilPassDepthFail;          break;
      case GL_STENCIL_PASS_DEPTH_PASS:          *params = mState.stencilPassDepthPass;          break;
      case GL_STENCIL_BACK_FAIL:                *params = mState.stencilBackFail;               break;
      case GL_STENCIL_BACK_PASS_DEPTH_FAIL:     *params = mState.stencilBackPassDepthFail;      break;
      case GL_STENCIL_BACK_PASS_DEPTH_PASS:     *params = mState.stencilBackPassDepthPass;      break;
      case GL_DEPTH_FUNC:                       *params = mState.depthFunc;                     break;
      case GL_BLEND_SRC_RGB:                    *params = mState.sourceBlendRGB;                break;
      case GL_BLEND_SRC_ALPHA:                  *params = mState.sourceBlendAlpha;              break;
      case GL_BLEND_DST_RGB:                    *params = mState.destBlendRGB;                  break;
      case GL_BLEND_DST_ALPHA:                  *params = mState.destBlendAlpha;                break;
      case GL_BLEND_EQUATION_RGB:               *params = mState.blendEquationRGB;              break;
      case GL_BLEND_EQUATION_ALPHA:             *params = mState.blendEquationAlpha;            break;
      case GL_STENCIL_WRITEMASK:                *params = mState.stencilWritemask;              break;
      case GL_STENCIL_BACK_WRITEMASK:           *params = mState.stencilBackWritemask;          break;
      case GL_STENCIL_CLEAR_VALUE:              *params = mState.stencilClearValue;             break;
      case GL_SUBPIXEL_BITS:                    *params = 4;                                    break;
      case GL_MAX_TEXTURE_SIZE:                 *params = gl::MAX_TEXTURE_SIZE;                 break;
      case GL_MAX_CUBE_MAP_TEXTURE_SIZE:        *params = gl::MAX_CUBE_MAP_TEXTURE_SIZE;        break;
      case GL_NUM_COMPRESSED_TEXTURE_FORMATS:   
        {
            if (supportsCompressedTextures())
            {
                // at current, only GL_COMPRESSED_RGB_S3TC_DXT1_EXT and 
                // GL_COMPRESSED_RGBA_S3TC_DXT1_EXT are supported
                *params = 2;
            }
            else
            {
                *params = 0;
            }
        }
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
      case GL_IMPLEMENTATION_COLOR_READ_TYPE:   *params = gl::IMPLEMENTATION_COLOR_READ_TYPE;   break;
      case GL_IMPLEMENTATION_COLOR_READ_FORMAT: *params = gl::IMPLEMENTATION_COLOR_READ_FORMAT; break;
      case GL_MAX_VIEWPORT_DIMS:
        {
            int maxDimension = std::max((int)gl::MAX_RENDERBUFFER_SIZE, (int)gl::MAX_TEXTURE_SIZE);
            params[0] = maxDimension;
            params[1] = maxDimension;
        }
        break;
      case GL_COMPRESSED_TEXTURE_FORMATS:
        {
            if (supportsCompressedTextures())
            {
                params[0] = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
                params[1] = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
            }
        }
        break;
      case GL_VIEWPORT:
        params[0] = mState.viewportX;
        params[1] = mState.viewportY;
        params[2] = mState.viewportWidth;
        params[3] = mState.viewportHeight;
        break;
      case GL_SCISSOR_BOX:
        params[0] = mState.scissorX;
        params[1] = mState.scissorY;
        params[2] = mState.scissorWidth;
        params[3] = mState.scissorHeight;
        break;
      case GL_CULL_FACE_MODE:                   *params = mState.cullMode;                 break;
      case GL_FRONT_FACE:                       *params = mState.frontFace;                break;
      case GL_RED_BITS:
      case GL_GREEN_BITS:
      case GL_BLUE_BITS:
      case GL_ALPHA_BITS:
        {
            gl::Framebuffer *framebuffer = getDrawFramebuffer();
            gl::Colorbuffer *colorbuffer = framebuffer->getColorbuffer();

            if (colorbuffer)
            {
                switch (pname)
                {
                  case GL_RED_BITS:   *params = colorbuffer->getRedSize();   break;
                  case GL_GREEN_BITS: *params = colorbuffer->getGreenSize(); break;
                  case GL_BLUE_BITS:  *params = colorbuffer->getBlueSize();  break;
                  case GL_ALPHA_BITS: *params = colorbuffer->getAlphaSize(); break;
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
            gl::DepthStencilbuffer *depthbuffer = framebuffer->getDepthbuffer();

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
            gl::DepthStencilbuffer *stencilbuffer = framebuffer->getStencilbuffer();

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
            if (mState.activeSampler < 0 || mState.activeSampler > gl::MAX_TEXTURE_IMAGE_UNITS - 1)
            {
                error(GL_INVALID_OPERATION);
                return false;
            }

            *params = mState.samplerTexture[SAMPLER_2D][mState.activeSampler].id();
        }
        break;
      case GL_TEXTURE_BINDING_CUBE_MAP:
        {
            if (mState.activeSampler < 0 || mState.activeSampler > gl::MAX_TEXTURE_IMAGE_UNITS - 1)
            {
                error(GL_INVALID_OPERATION);
                return false;
            }

            *params = mState.samplerTexture[SAMPLER_CUBE][mState.activeSampler].id();
        }
        break;
      default:
        return false;
    }

    return true;
}

bool Context::getQueryParameterInfo(GLenum pname, GLenum *type, unsigned int *numParams)
{
    // Please note: the query type returned for DEPTH_CLEAR_VALUE in this implementation
    // is FLOAT rather than INT, as would be suggested by the GL ES 2.0 spec. This is due
    // to the fact that it is stored internally as a float, and so would require conversion
    // if returned from Context::getIntegerv. Since this conversion is already implemented 
    // in the case that one calls glGetIntegerv to retrieve a float-typed state variable, we
    // place DEPTH_CLEAR_VALUE with the floats. This should make no difference to the calling
    // application.
    switch (pname)
    {
      case GL_COMPRESSED_TEXTURE_FORMATS: /* no compressed texture formats are supported */ 
      case GL_SHADER_BINARY_FORMATS:
        {
            *type = GL_INT;
            *numParams = 0;
        }
        break;
      case GL_MAX_VERTEX_ATTRIBS:
      case GL_MAX_VERTEX_UNIFORM_VECTORS:
      case GL_MAX_VARYING_VECTORS:
      case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
      case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
      case GL_MAX_TEXTURE_IMAGE_UNITS:
      case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
      case GL_MAX_RENDERBUFFER_SIZE:
      case GL_NUM_SHADER_BINARY_FORMATS:
      case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
      case GL_ARRAY_BUFFER_BINDING:
      case GL_FRAMEBUFFER_BINDING:
      case GL_RENDERBUFFER_BINDING:
      case GL_CURRENT_PROGRAM:
      case GL_PACK_ALIGNMENT:
      case GL_UNPACK_ALIGNMENT:
      case GL_GENERATE_MIPMAP_HINT:
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
        {
            *type = GL_INT;
            *numParams = 1;
        }
        break;
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
        break;
      case GL_MAX_VIEWPORT_DIMS:
        {
            *type = GL_INT;
            *numParams = 2;
        }
        break;
      case GL_VIEWPORT:
      case GL_SCISSOR_BOX:
        {
            *type = GL_INT;
            *numParams = 4;
        }
        break;
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
        {
            *type = GL_BOOL;
            *numParams = 1;
        }
        break;
      case GL_COLOR_WRITEMASK:
        {
            *type = GL_BOOL;
            *numParams = 4;
        }
        break;
      case GL_POLYGON_OFFSET_FACTOR:
      case GL_POLYGON_OFFSET_UNITS:
      case GL_SAMPLE_COVERAGE_VALUE:
      case GL_DEPTH_CLEAR_VALUE:
      case GL_LINE_WIDTH:
        {
            *type = GL_FLOAT;
            *numParams = 1;
        }
        break;
      case GL_ALIASED_LINE_WIDTH_RANGE:
      case GL_ALIASED_POINT_SIZE_RANGE:
      case GL_DEPTH_RANGE:
        {
            *type = GL_FLOAT;
            *numParams = 2;
        }
        break;
      case GL_COLOR_CLEAR_VALUE:
      case GL_BLEND_COLOR:
        {
            *type = GL_FLOAT;
            *numParams = 4;
        }
        break;
      default:
        return false;
    }

    return true;
}

// Applies the render target surface, depth stencil surface, viewport rectangle and
// scissor rectangle to the Direct3D 9 device
bool Context::applyRenderTarget(bool ignoreViewport)
{
    IDirect3DDevice9 *device = getDevice();

    Framebuffer *framebufferObject = getDrawFramebuffer();

    if (!framebufferObject || framebufferObject->completeness() != GL_FRAMEBUFFER_COMPLETE)
    {
        error(GL_INVALID_FRAMEBUFFER_OPERATION);

        return false;
    }

    IDirect3DSurface9 *renderTarget = framebufferObject->getRenderTarget();
    IDirect3DSurface9 *depthStencil = NULL;

    unsigned int renderTargetSerial = framebufferObject->getRenderTargetSerial();
    if (renderTargetSerial != mAppliedRenderTargetSerial)
    {
        device->SetRenderTarget(0, renderTarget);
        mAppliedRenderTargetSerial = renderTargetSerial;
        mScissorStateDirty = true; // Scissor area must be clamped to render target's size-- this is different for different render targets.
    }

    unsigned int depthbufferSerial = 0;
    unsigned int stencilbufferSerial = 0;
    if (framebufferObject->getDepthbufferType() != GL_NONE)
    {
        depthStencil = framebufferObject->getDepthbuffer()->getDepthStencil();
        depthbufferSerial = framebufferObject->getDepthbuffer()->getSerial();
    }
    else if (framebufferObject->getStencilbufferType() != GL_NONE)
    {
        depthStencil = framebufferObject->getStencilbuffer()->getDepthStencil();
        stencilbufferSerial = framebufferObject->getStencilbuffer()->getSerial();
    }

    if (depthbufferSerial != mAppliedDepthbufferSerial ||
        stencilbufferSerial != mAppliedStencilbufferSerial)
    {
        device->SetDepthStencilSurface(depthStencil);
        mAppliedDepthbufferSerial = depthbufferSerial;
        mAppliedStencilbufferSerial = stencilbufferSerial;
    }

    D3DVIEWPORT9 viewport;
    D3DSURFACE_DESC desc;
    renderTarget->GetDesc(&desc);

    if (ignoreViewport)
    {
        viewport.X = 0;
        viewport.Y = 0;
        viewport.Width = desc.Width;
        viewport.Height = desc.Height;
        viewport.MinZ = 0.0f;
        viewport.MaxZ = 1.0f;
    }
    else
    {
        viewport.X = std::max(mState.viewportX, 0);
        viewport.Y = std::max(mState.viewportY, 0);
        viewport.Width = std::min(mState.viewportWidth, (int)desc.Width - (int)viewport.X);
        viewport.Height = std::min(mState.viewportHeight, (int)desc.Height - (int)viewport.Y);
        viewport.MinZ = clamp01(mState.zNear);
        viewport.MaxZ = clamp01(mState.zFar);
    }

    if (viewport.Width <= 0 || viewport.Height <= 0)
    {
        return false;   // Nothing to render
    }

    device->SetViewport(&viewport);

    if (mScissorStateDirty)
    {
        if (mState.scissorTest)
        {
            RECT rect = {mState.scissorX,
                         mState.scissorY,
                         mState.scissorX + mState.scissorWidth,
                         mState.scissorY + mState.scissorHeight};
            rect.right = std::min(static_cast<UINT>(rect.right), desc.Width);
            rect.bottom = std::min(static_cast<UINT>(rect.bottom), desc.Height);
            device->SetScissorRect(&rect);
            device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
        }
        else
        {
            device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        }

        mScissorStateDirty = false;
    }

    if (mState.currentProgram)
    {
        Program *programObject = getCurrentProgram();

        GLint halfPixelSize = programObject->getDxHalfPixelSizeLocation();
        GLfloat xy[2] = {1.0f / viewport.Width, 1.0f / viewport.Height};
        programObject->setUniform2fv(halfPixelSize, 1, (GLfloat*)&xy);

        GLint window = programObject->getDxViewportLocation();
        GLfloat whxy[4] = {mState.viewportWidth / 2.0f, mState.viewportHeight / 2.0f, 
                          (float)mState.viewportX + mState.viewportWidth / 2.0f, 
                          (float)mState.viewportY + mState.viewportHeight / 2.0f};
        programObject->setUniform4fv(window, 1, (GLfloat*)&whxy);

        GLint depth = programObject->getDxDepthLocation();
        GLfloat dz[2] = {(mState.zFar - mState.zNear) / 2.0f, (mState.zNear + mState.zFar) / 2.0f};
        programObject->setUniform2fv(depth, 1, (GLfloat*)&dz);

        GLint near = programObject->getDepthRangeNearLocation();
        programObject->setUniform1fv(near, 1, &mState.zNear);

        GLint far = programObject->getDepthRangeFarLocation();
        programObject->setUniform1fv(far, 1, &mState.zFar);

        GLint diff = programObject->getDepthRangeDiffLocation();
        GLfloat zDiff = mState.zFar - mState.zNear;
        programObject->setUniform1fv(diff, 1, &zDiff);
    }

    return true;
}

// Applies the fixed-function state (culling, depth test, alpha blending, stenciling, etc) to the Direct3D 9 device
void Context::applyState(GLenum drawMode)
{
    IDirect3DDevice9 *device = getDevice();
    Program *programObject = getCurrentProgram();

    GLint frontCCW = programObject->getDxFrontCCWLocation();
    GLint ccw = (mState.frontFace == GL_CCW);
    programObject->setUniform1iv(frontCCW, 1, &ccw);

    GLint pointsOrLines = programObject->getDxPointsOrLinesLocation();
    GLint alwaysFront = !isTriangleMode(drawMode);
    programObject->setUniform1iv(pointsOrLines, 1, &alwaysFront);

    Framebuffer *framebufferObject = getDrawFramebuffer();

    if (mCullStateDirty || mFrontFaceDirty)
    {
        if (mState.cullFace)
        {
            device->SetRenderState(D3DRS_CULLMODE, es2dx::ConvertCullMode(mState.cullMode, mState.frontFace));
        }
        else
        {
            device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        }

        mCullStateDirty = false;
    }

    if (mDepthStateDirty)
    {
        if (mState.depthTest && framebufferObject->getDepthbufferType() != GL_NONE)
        {
            device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
            device->SetRenderState(D3DRS_ZFUNC, es2dx::ConvertComparison(mState.depthFunc));
        }
        else
        {
            device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
        }

        mDepthStateDirty = false;
    }

    if (mBlendStateDirty)
    {
        if (mState.blend)
        {
            device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);

            if (mState.sourceBlendRGB != GL_CONSTANT_ALPHA && mState.sourceBlendRGB != GL_ONE_MINUS_CONSTANT_ALPHA &&
                mState.destBlendRGB != GL_CONSTANT_ALPHA && mState.destBlendRGB != GL_ONE_MINUS_CONSTANT_ALPHA)
            {
                device->SetRenderState(D3DRS_BLENDFACTOR, es2dx::ConvertColor(mState.blendColor));
            }
            else
            {
                device->SetRenderState(D3DRS_BLENDFACTOR, D3DCOLOR_RGBA(unorm<8>(mState.blendColor.alpha),
                                                                        unorm<8>(mState.blendColor.alpha),
                                                                        unorm<8>(mState.blendColor.alpha),
                                                                        unorm<8>(mState.blendColor.alpha)));
            }

            device->SetRenderState(D3DRS_SRCBLEND, es2dx::ConvertBlendFunc(mState.sourceBlendRGB));
            device->SetRenderState(D3DRS_DESTBLEND, es2dx::ConvertBlendFunc(mState.destBlendRGB));
            device->SetRenderState(D3DRS_BLENDOP, es2dx::ConvertBlendOp(mState.blendEquationRGB));

            if (mState.sourceBlendRGB != mState.sourceBlendAlpha || 
                mState.destBlendRGB != mState.destBlendAlpha || 
                mState.blendEquationRGB != mState.blendEquationAlpha)
            {
                device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);

                device->SetRenderState(D3DRS_SRCBLENDALPHA, es2dx::ConvertBlendFunc(mState.sourceBlendAlpha));
                device->SetRenderState(D3DRS_DESTBLENDALPHA, es2dx::ConvertBlendFunc(mState.destBlendAlpha));
                device->SetRenderState(D3DRS_BLENDOPALPHA, es2dx::ConvertBlendOp(mState.blendEquationAlpha));

            }
            else
            {
                device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
            }
        }
        else
        {
            device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        }

        mBlendStateDirty = false;
    }

    if (mStencilStateDirty || mFrontFaceDirty)
    {
        if (mState.stencilTest && framebufferObject->hasStencil())
        {
            device->SetRenderState(D3DRS_STENCILENABLE, TRUE);
            device->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, TRUE);

            // FIXME: Unsupported by D3D9
            const D3DRENDERSTATETYPE D3DRS_CCW_STENCILREF = D3DRS_STENCILREF;
            const D3DRENDERSTATETYPE D3DRS_CCW_STENCILMASK = D3DRS_STENCILMASK;
            const D3DRENDERSTATETYPE D3DRS_CCW_STENCILWRITEMASK = D3DRS_STENCILWRITEMASK;
            if (mState.stencilWritemask != mState.stencilBackWritemask || 
                mState.stencilRef != mState.stencilBackRef || 
                mState.stencilMask != mState.stencilBackMask)
            {
                ERR("Separate front/back stencil writemasks, reference values, or stencil mask values are invalid under WebGL.");
                return error(GL_INVALID_OPERATION);
            }

            // get the maximum size of the stencil ref
            gl::DepthStencilbuffer *stencilbuffer = framebufferObject->getStencilbuffer();
            GLuint maxStencil = (1 << stencilbuffer->getStencilSize()) - 1;

            device->SetRenderState(mState.frontFace == GL_CCW ? D3DRS_STENCILWRITEMASK : D3DRS_CCW_STENCILWRITEMASK, mState.stencilWritemask);
            device->SetRenderState(mState.frontFace == GL_CCW ? D3DRS_STENCILFUNC : D3DRS_CCW_STENCILFUNC, 
                                   es2dx::ConvertComparison(mState.stencilFunc));

            device->SetRenderState(mState.frontFace == GL_CCW ? D3DRS_STENCILREF : D3DRS_CCW_STENCILREF, (mState.stencilRef < (GLint)maxStencil) ? mState.stencilRef : maxStencil);
            device->SetRenderState(mState.frontFace == GL_CCW ? D3DRS_STENCILMASK : D3DRS_CCW_STENCILMASK, mState.stencilMask);

            device->SetRenderState(mState.frontFace == GL_CCW ? D3DRS_STENCILFAIL : D3DRS_CCW_STENCILFAIL, 
                                   es2dx::ConvertStencilOp(mState.stencilFail));
            device->SetRenderState(mState.frontFace == GL_CCW ? D3DRS_STENCILZFAIL : D3DRS_CCW_STENCILZFAIL, 
                                   es2dx::ConvertStencilOp(mState.stencilPassDepthFail));
            device->SetRenderState(mState.frontFace == GL_CCW ? D3DRS_STENCILPASS : D3DRS_CCW_STENCILPASS, 
                                   es2dx::ConvertStencilOp(mState.stencilPassDepthPass));

            device->SetRenderState(mState.frontFace == GL_CW ? D3DRS_STENCILWRITEMASK : D3DRS_CCW_STENCILWRITEMASK, mState.stencilBackWritemask);
            device->SetRenderState(mState.frontFace == GL_CW ? D3DRS_STENCILFUNC : D3DRS_CCW_STENCILFUNC, 
                                   es2dx::ConvertComparison(mState.stencilBackFunc));

            device->SetRenderState(mState.frontFace == GL_CW ? D3DRS_STENCILREF : D3DRS_CCW_STENCILREF, (mState.stencilBackRef < (GLint)maxStencil) ? mState.stencilBackRef : maxStencil);
            device->SetRenderState(mState.frontFace == GL_CW ? D3DRS_STENCILMASK : D3DRS_CCW_STENCILMASK, mState.stencilBackMask);

            device->SetRenderState(mState.frontFace == GL_CW ? D3DRS_STENCILFAIL : D3DRS_CCW_STENCILFAIL, 
                                   es2dx::ConvertStencilOp(mState.stencilBackFail));
            device->SetRenderState(mState.frontFace == GL_CW ? D3DRS_STENCILZFAIL : D3DRS_CCW_STENCILZFAIL, 
                                   es2dx::ConvertStencilOp(mState.stencilBackPassDepthFail));
            device->SetRenderState(mState.frontFace == GL_CW ? D3DRS_STENCILPASS : D3DRS_CCW_STENCILPASS, 
                                   es2dx::ConvertStencilOp(mState.stencilBackPassDepthPass));
        }
        else
        {
            device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        }

        mStencilStateDirty = false;
    }

    if (mMaskStateDirty)
    {
        device->SetRenderState(D3DRS_COLORWRITEENABLE, es2dx::ConvertColorMask(mState.colorMaskRed, mState.colorMaskGreen, 
                                                                               mState.colorMaskBlue, mState.colorMaskAlpha));
        device->SetRenderState(D3DRS_ZWRITEENABLE, mState.depthMask ? TRUE : FALSE);

        mMaskStateDirty = false;
    }

    if (mPolygonOffsetStateDirty)
    {
        if (mState.polygonOffsetFill)
        {
            gl::DepthStencilbuffer *depthbuffer = framebufferObject->getDepthbuffer();
            if (depthbuffer)
            {
                device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, *((DWORD*)&mState.polygonOffsetFactor));
                float depthBias = ldexp(mState.polygonOffsetUnits, -(int)(depthbuffer->getDepthSize()));
                device->SetRenderState(D3DRS_DEPTHBIAS, *((DWORD*)&depthBias));
            }
        }
        else
        {
            device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
            device->SetRenderState(D3DRS_DEPTHBIAS, 0);
        }

        mPolygonOffsetStateDirty = false;
    }

    if (framebufferObject->isMultisample() && mSampleStateDirty)
    {
        if (mState.sampleAlphaToCoverage)
        {
            FIXME("Sample alpha to coverage is unimplemented.");
        }

        if (mState.sampleCoverage)
        {
            device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, TRUE);
            unsigned int mask = 0;
            if (mState.sampleCoverageValue != 0)
            {
                float threshold = 0.5f;

                for (int i = 0; i < framebufferObject->getSamples(); ++i)
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

            device->SetRenderState(D3DRS_MULTISAMPLEMASK, mask);
        }
        else
        {
            device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, FALSE);
        }

        mSampleStateDirty = false;
    }

    if (mDitherStateDirty)
    {
        device->SetRenderState(D3DRS_DITHERENABLE, mState.dither ? TRUE : FALSE);

        mDitherStateDirty = false;
    }

    mFrontFaceDirty = false;
}

// Fill in the semanticIndex field of the array of TranslatedAttributes based on the active GLSL program.
void Context::lookupAttributeMapping(TranslatedAttribute *attributes)
{
    for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
    {
        if (attributes[i].enabled)
        {
            attributes[i].semanticIndex = getCurrentProgram()->getSemanticIndex(i);
        }
    }
}

GLenum Context::applyVertexBuffer(GLenum mode, GLint first, GLsizei count, bool *useIndexing, TranslatedIndexData *indexInfo)
{
    TranslatedAttribute translated[MAX_VERTEX_ATTRIBS];

    GLenum err = mVertexDataManager->preRenderValidate(first, count, translated);
    if (err != GL_NO_ERROR)
    {
        return err;
    }

    lookupAttributeMapping(translated);

    mBufferBackEnd->setupAttributesPreDraw(translated);

    for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
    {
        if (translated[i].enabled && translated[i].nonArray)
        {
            err = mIndexDataManager->preRenderValidateUnindexed(mode, count, indexInfo);
            if (err != GL_NO_ERROR)
            {
                return err;
            }

            mBufferBackEnd->setupIndicesPreDraw(*indexInfo);

            *useIndexing = true;
            return GL_NO_ERROR;
        }
    }

    *useIndexing = false;
    return GL_NO_ERROR;
}

GLenum Context::applyVertexBuffer(const TranslatedIndexData &indexInfo)
{
    TranslatedAttribute translated[MAX_VERTEX_ATTRIBS];

    GLenum err = mVertexDataManager->preRenderValidate(indexInfo.minIndex, indexInfo.maxIndex-indexInfo.minIndex+1, translated);

    if (err == GL_NO_ERROR)
    {
        lookupAttributeMapping(translated);

        mBufferBackEnd->setupAttributesPreDraw(translated);
    }

    return err;
}

// Applies the indices and element array bindings to the Direct3D 9 device
GLenum Context::applyIndexBuffer(const void *indices, GLsizei count, GLenum mode, GLenum type, TranslatedIndexData *indexInfo)
{
    GLenum err = mIndexDataManager->preRenderValidate(mode, type, count, mState.elementArrayBuffer.get(), indices, indexInfo);

    if (err == GL_NO_ERROR)
    {
        mBufferBackEnd->setupIndicesPreDraw(*indexInfo);
    }

    return err;
}

// Applies the shaders and shader constants to the Direct3D 9 device
void Context::applyShaders()
{
    IDirect3DDevice9 *device = getDevice();
    Program *programObject = getCurrentProgram();
    IDirect3DVertexShader9 *vertexShader = programObject->getVertexShader();
    IDirect3DPixelShader9 *pixelShader = programObject->getPixelShader();

    device->SetVertexShader(vertexShader);
    device->SetPixelShader(pixelShader);

    if (programObject->getSerial() != mAppliedProgram)
    {
        programObject->dirtyAllUniforms();
        programObject->dirtyAllSamplers();
        mAppliedProgram = programObject->getSerial();
    }

    programObject->applyUniforms();
}

// Applies the textures and sampler states to the Direct3D 9 device
void Context::applyTextures()
{
    IDirect3DDevice9 *device = getDevice();
    Program *programObject = getCurrentProgram();

    for (int sampler = 0; sampler < MAX_TEXTURE_IMAGE_UNITS; sampler++)
    {
        int textureUnit = programObject->getSamplerMapping(sampler);
        if (textureUnit != -1)
        {
            SamplerType textureType = programObject->getSamplerType(sampler);

            Texture *texture = getSamplerTexture(textureUnit, textureType);

            if (programObject->isSamplerDirty(sampler) || texture->isDirty())
            {
                if (texture->isComplete())
                {
                    GLenum wrapS = texture->getWrapS();
                    GLenum wrapT = texture->getWrapT();
                    GLenum minFilter = texture->getMinFilter();
                    GLenum magFilter = texture->getMagFilter();

                    device->SetSamplerState(sampler, D3DSAMP_ADDRESSU, es2dx::ConvertTextureWrap(wrapS));
                    device->SetSamplerState(sampler, D3DSAMP_ADDRESSV, es2dx::ConvertTextureWrap(wrapT));

                    device->SetSamplerState(sampler, D3DSAMP_MAGFILTER, es2dx::ConvertMagFilter(magFilter));
                    D3DTEXTUREFILTERTYPE d3dMinFilter, d3dMipFilter;
                    es2dx::ConvertMinFilter(minFilter, &d3dMinFilter, &d3dMipFilter);
                    device->SetSamplerState(sampler, D3DSAMP_MINFILTER, d3dMinFilter);
                    device->SetSamplerState(sampler, D3DSAMP_MIPFILTER, d3dMipFilter);

                    device->SetTexture(sampler, texture->getTexture());
                }
                else
                {
                    device->SetTexture(sampler, getIncompleteTexture(textureType)->getTexture());
                }
            }

            programObject->setSamplerDirty(sampler, false);
        }
        else
        {
            if (programObject->isSamplerDirty(sampler))
            {
                device->SetTexture(sampler, NULL);
                programObject->setSamplerDirty(sampler, false);
            }
        }   
    }
}

void Context::readPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels)
{
    Framebuffer *framebuffer = getReadFramebuffer();

    if (framebuffer->completeness() != GL_FRAMEBUFFER_COMPLETE)
    {
        return error(GL_INVALID_FRAMEBUFFER_OPERATION);
    }

    if (getReadFramebufferHandle() != 0 && framebuffer->getSamples() != 0)
    {
        return error(GL_INVALID_OPERATION);
    }

    IDirect3DSurface9 *renderTarget = framebuffer->getRenderTarget();
    IDirect3DDevice9 *device = getDevice();

    D3DSURFACE_DESC desc;
    renderTarget->GetDesc(&desc);

    IDirect3DSurface9 *systemSurface;
    HRESULT result = device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &systemSurface, NULL);

    if (result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY)
    {
        return error(GL_OUT_OF_MEMORY);
    }

    ASSERT(SUCCEEDED(result));

    if (desc.MultiSampleType != D3DMULTISAMPLE_NONE)
    {
        UNIMPLEMENTED();   // FIXME: Requires resolve using StretchRect into non-multisampled render target
    }

    result = device->GetRenderTargetData(renderTarget, systemSurface);

    if (FAILED(result))
    {
        systemSurface->Release();

        switch (result)
        {
            case D3DERR_DRIVERINTERNALERROR:
            case D3DERR_DEVICELOST:
                return error(GL_OUT_OF_MEMORY);
            default:
                UNREACHABLE();
                return;   // No sensible error to generate
        }
    }

    D3DLOCKED_RECT lock;
    RECT rect = {std::max(x, 0),
                 std::max(y, 0),
                 std::min(x + width, (int)desc.Width),
                 std::min(y + height, (int)desc.Height)};

    result = systemSurface->LockRect(&lock, &rect, D3DLOCK_READONLY);

    if (FAILED(result))
    {
        UNREACHABLE();
        systemSurface->Release();

        return;   // No sensible error to generate
    }

    unsigned char *source = (unsigned char*)lock.pBits;
    unsigned char *dest = (unsigned char*)pixels;
    unsigned short *dest16 = (unsigned short*)pixels;

    GLsizei outputPitch = ComputePitch(width, format, type, mState.packAlignment);

    for (int j = 0; j < rect.bottom - rect.top; j++)
    {
        if (desc.Format == D3DFMT_A8R8G8B8 &&
            format == GL_BGRA_EXT &&
            type == GL_UNSIGNED_BYTE)
        {
            // Fast path for EXT_read_format_bgra, given
            // an RGBA source buffer.  Note that buffers with no
            // alpha go through the slow path below.
            memcpy(dest + j * outputPitch,
                   source + j * lock.Pitch,
                   (rect.right - rect.left) * 4);
            continue;
        }

        for (int i = 0; i < rect.right - rect.left; i++)
        {
            float r;
            float g;
            float b;
            float a;

            switch (desc.Format)
            {
              case D3DFMT_R5G6B5:
                {
                    unsigned short rgb = *(unsigned short*)(source + 2 * i + j * lock.Pitch);

                    a = 1.0f;
                    b = (rgb & 0x001F) * (1.0f / 0x001F);
                    g = (rgb & 0x07E0) * (1.0f / 0x07E0);
                    r = (rgb & 0xF800) * (1.0f / 0xF800);
                }
                break;
              case D3DFMT_X1R5G5B5:
                {
                    unsigned short xrgb = *(unsigned short*)(source + 2 * i + j * lock.Pitch);

                    a = 1.0f;
                    b = (xrgb & 0x001F) * (1.0f / 0x001F);
                    g = (xrgb & 0x03E0) * (1.0f / 0x03E0);
                    r = (xrgb & 0x7C00) * (1.0f / 0x7C00);
                }
                break;
              case D3DFMT_A1R5G5B5:
                {
                    unsigned short argb = *(unsigned short*)(source + 2 * i + j * lock.Pitch);

                    a = (argb & 0x8000) ? 1.0f : 0.0f;
                    b = (argb & 0x001F) * (1.0f / 0x001F);
                    g = (argb & 0x03E0) * (1.0f / 0x03E0);
                    r = (argb & 0x7C00) * (1.0f / 0x7C00);
                }
                break;
              case D3DFMT_A8R8G8B8:
                {
                    unsigned int argb = *(unsigned int*)(source + 4 * i + j * lock.Pitch);

                    a = (argb & 0xFF000000) * (1.0f / 0xFF000000);
                    b = (argb & 0x000000FF) * (1.0f / 0x000000FF);
                    g = (argb & 0x0000FF00) * (1.0f / 0x0000FF00);
                    r = (argb & 0x00FF0000) * (1.0f / 0x00FF0000);
                }
                break;
              case D3DFMT_X8R8G8B8:
                {
                    unsigned int xrgb = *(unsigned int*)(source + 4 * i + j * lock.Pitch);

                    a = 1.0f;
                    b = (xrgb & 0x000000FF) * (1.0f / 0x000000FF);
                    g = (xrgb & 0x0000FF00) * (1.0f / 0x0000FF00);
                    r = (xrgb & 0x00FF0000) * (1.0f / 0x00FF0000);
                }
                break;
              case D3DFMT_A2R10G10B10:
                {
                    unsigned int argb = *(unsigned int*)(source + 4 * i + j * lock.Pitch);

                    a = (argb & 0xC0000000) * (1.0f / 0xC0000000);
                    b = (argb & 0x000003FF) * (1.0f / 0x000003FF);
                    g = (argb & 0x000FFC00) * (1.0f / 0x000FFC00);
                    r = (argb & 0x3FF00000) * (1.0f / 0x3FF00000);
                }
                break;
              default:
                UNIMPLEMENTED();   // FIXME
                UNREACHABLE();
            }

            switch (format)
            {
              case GL_RGBA:
                switch (type)
                {
                  case GL_UNSIGNED_BYTE:
                    dest[4 * i + j * outputPitch + 0] = (unsigned char)(255 * r + 0.5f);
                    dest[4 * i + j * outputPitch + 1] = (unsigned char)(255 * g + 0.5f);
                    dest[4 * i + j * outputPitch + 2] = (unsigned char)(255 * b + 0.5f);
                    dest[4 * i + j * outputPitch + 3] = (unsigned char)(255 * a + 0.5f);
                    break;
                  default: UNREACHABLE();
                }
                break;
              case GL_BGRA_EXT:
                switch (type)
                {
                  case GL_UNSIGNED_BYTE:
                    dest[4 * i + j * outputPitch + 0] = (unsigned char)(255 * b + 0.5f);
                    dest[4 * i + j * outputPitch + 1] = (unsigned char)(255 * g + 0.5f);
                    dest[4 * i + j * outputPitch + 2] = (unsigned char)(255 * r + 0.5f);
                    dest[4 * i + j * outputPitch + 3] = (unsigned char)(255 * a + 0.5f);
                    break;
                  case GL_UNSIGNED_SHORT_4_4_4_4_REV_EXT:
                    // According to the desktop GL spec in the "Transfer of Pixel Rectangles" section
                    // this type is packed as follows:
                    //   15   14   13   12   11   10    9    8    7    6    5    4    3    2    1    0
                    //  --------------------------------------------------------------------------------
                    // |       4th         |        3rd         |        2nd        |   1st component   |
                    //  --------------------------------------------------------------------------------
                    // in the case of BGRA_EXT, B is the first component, G the second, and so forth.
                    dest16[i + j * outputPitch / sizeof(unsigned short)] =
                        ((unsigned short)(15 * a + 0.5f) << 12)|
                        ((unsigned short)(15 * r + 0.5f) << 8) |
                        ((unsigned short)(15 * g + 0.5f) << 4) |
                        ((unsigned short)(15 * b + 0.5f) << 0);
                    break;
                  case GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT:
                    // According to the desktop GL spec in the "Transfer of Pixel Rectangles" section
                    // this type is packed as follows:
                    //   15   14   13   12   11   10    9    8    7    6    5    4    3    2    1    0
                    //  --------------------------------------------------------------------------------
                    // | 4th |          3rd           |           2nd          |      1st component     |
                    //  --------------------------------------------------------------------------------
                    // in the case of BGRA_EXT, B is the first component, G the second, and so forth.
                    dest16[i + j * outputPitch / sizeof(unsigned short)] =
                        ((unsigned short)(     a + 0.5f) << 15) |
                        ((unsigned short)(31 * r + 0.5f) << 10) |
                        ((unsigned short)(31 * g + 0.5f) << 5) |
                        ((unsigned short)(31 * b + 0.5f) << 0);
                    break;
                  default: UNREACHABLE();
                }
                break;
              case GL_RGB:   // IMPLEMENTATION_COLOR_READ_FORMAT
                switch (type)
                {
                  case GL_UNSIGNED_SHORT_5_6_5:   // IMPLEMENTATION_COLOR_READ_TYPE
                    dest16[i + j * outputPitch / sizeof(unsigned short)] = 
                        ((unsigned short)(31 * b + 0.5f) << 0) |
                        ((unsigned short)(63 * g + 0.5f) << 5) |
                        ((unsigned short)(31 * r + 0.5f) << 11);
                    break;
                  default: UNREACHABLE();
                }
                break;
              default: UNREACHABLE();
            }
        }
    }

    systemSurface->UnlockRect();

    systemSurface->Release();
}

void Context::clear(GLbitfield mask)
{
    Framebuffer *framebufferObject = getDrawFramebuffer();

    if (!framebufferObject || framebufferObject->completeness() != GL_FRAMEBUFFER_COMPLETE)
    {
        error(GL_INVALID_FRAMEBUFFER_OPERATION);

        return;
    }

    egl::Display *display = getDisplay();
    IDirect3DDevice9 *device = getDevice();
    DWORD flags = 0;

    if (mask & GL_COLOR_BUFFER_BIT)
    {
        mask &= ~GL_COLOR_BUFFER_BIT;

        if (framebufferObject->getColorbufferType() != GL_NONE)
        {
            flags |= D3DCLEAR_TARGET;
        }
    }

    if (mask & GL_DEPTH_BUFFER_BIT)
    {
        mask &= ~GL_DEPTH_BUFFER_BIT;
        if (mState.depthMask && framebufferObject->getDepthbufferType() != GL_NONE)
        {
            flags |= D3DCLEAR_ZBUFFER;
        }
    }

    GLuint stencilUnmasked = 0x0;

    if (mask & GL_STENCIL_BUFFER_BIT)
    {
        mask &= ~GL_STENCIL_BUFFER_BIT;
        if (framebufferObject->getStencilbufferType() != GL_NONE)
        {
            IDirect3DSurface9 *depthStencil = framebufferObject->getStencilbuffer()->getDepthStencil();
            D3DSURFACE_DESC desc;
            depthStencil->GetDesc(&desc);

            unsigned int stencilSize = es2dx::GetStencilSize(desc.Format);
            stencilUnmasked = (0x1 << stencilSize) - 1;

            if (stencilUnmasked != 0x0)
            {
                flags |= D3DCLEAR_STENCIL;
            }
        }
    }

    if (mask != 0)
    {
        return error(GL_INVALID_VALUE);
    }

    if (!applyRenderTarget(true))   // Clips the clear to the scissor rectangle but not the viewport
    {
        return;
    }

    D3DCOLOR color = D3DCOLOR_ARGB(unorm<8>(mState.colorClearValue.alpha), 
                                            unorm<8>(mState.colorClearValue.red), 
                                            unorm<8>(mState.colorClearValue.green), 
                                            unorm<8>(mState.colorClearValue.blue));
    float depth = clamp01(mState.depthClearValue);
    int stencil = mState.stencilClearValue & 0x000000FF;

    IDirect3DSurface9 *renderTarget = framebufferObject->getRenderTarget();

    D3DSURFACE_DESC desc;
    renderTarget->GetDesc(&desc);

    bool alphaUnmasked = (es2dx::GetAlphaSize(desc.Format) == 0) || mState.colorMaskAlpha;

    const bool needMaskedStencilClear = (flags & D3DCLEAR_STENCIL) &&
                                        (mState.stencilWritemask & stencilUnmasked) != stencilUnmasked;
    const bool needMaskedColorClear = (flags & D3DCLEAR_TARGET) &&
                                      !(mState.colorMaskRed && mState.colorMaskGreen &&
                                        mState.colorMaskBlue && alphaUnmasked);

    if (needMaskedColorClear || needMaskedStencilClear)
    {
        // State which is altered in all paths from this point to the clear call is saved.
        // State which is altered in only some paths will be flagged dirty in the case that
        //  that path is taken.
        HRESULT hr;
        if (mMaskedClearSavedState == NULL)
        {
            hr = device->BeginStateBlock();
            ASSERT(SUCCEEDED(hr) || hr == D3DERR_OUTOFVIDEOMEMORY || hr == E_OUTOFMEMORY);

            device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
            device->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
            device->SetRenderState(D3DRS_ZENABLE, FALSE);
            device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
            device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
            device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
            device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
            device->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
            device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
            device->SetPixelShader(NULL);
            device->SetVertexShader(NULL);
            device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
            device->SetStreamSourceFreq(0, 1);

            hr = device->EndStateBlock(&mMaskedClearSavedState);
            ASSERT(SUCCEEDED(hr) || hr == D3DERR_OUTOFVIDEOMEMORY || hr == E_OUTOFMEMORY);
        }

        ASSERT(mMaskedClearSavedState != NULL);

        if (mMaskedClearSavedState != NULL)
        {
            hr = mMaskedClearSavedState->Capture();
            ASSERT(SUCCEEDED(hr));
        }

        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
        device->SetRenderState(D3DRS_ZENABLE, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);

        if (flags & D3DCLEAR_TARGET)
        {
            device->SetRenderState(D3DRS_COLORWRITEENABLE, (mState.colorMaskRed   ? D3DCOLORWRITEENABLE_RED   : 0) |
                                                           (mState.colorMaskGreen ? D3DCOLORWRITEENABLE_GREEN : 0) |
                                                           (mState.colorMaskBlue  ? D3DCOLORWRITEENABLE_BLUE  : 0) |
                                                           (mState.colorMaskAlpha ? D3DCOLORWRITEENABLE_ALPHA : 0));
        }
        else
        {
            device->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
        }

        if (stencilUnmasked != 0x0 && (flags & D3DCLEAR_STENCIL))
        {
            device->SetRenderState(D3DRS_STENCILENABLE, TRUE);
            device->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
            device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
            device->SetRenderState(D3DRS_STENCILREF, stencil);
            device->SetRenderState(D3DRS_STENCILWRITEMASK, mState.stencilWritemask);
            device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_REPLACE);
            device->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_REPLACE);
            device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
            mStencilStateDirty = true;
        }
        else
        {
            device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        }

        device->SetPixelShader(NULL);
        device->SetVertexShader(NULL);
        device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
        device->SetStreamSourceFreq(0, 1);

        struct Vertex
        {
            float x, y, z, w;
            D3DCOLOR diffuse;
        };

        Vertex quad[4];
        quad[0].x = 0.0f;
        quad[0].y = (float)desc.Height;
        quad[0].z = 0.0f;
        quad[0].w = 1.0f;
        quad[0].diffuse = color;

        quad[1].x = (float)desc.Width;
        quad[1].y = (float)desc.Height;
        quad[1].z = 0.0f;
        quad[1].w = 1.0f;
        quad[1].diffuse = color;

        quad[2].x = 0.0f;
        quad[2].y = 0.0f;
        quad[2].z = 0.0f;
        quad[2].w = 1.0f;
        quad[2].diffuse = color;

        quad[3].x = (float)desc.Width;
        quad[3].y = 0.0f;
        quad[3].z = 0.0f;
        quad[3].w = 1.0f;
        quad[3].diffuse = color;

        display->startScene();
        device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(Vertex));

        if (flags & D3DCLEAR_ZBUFFER)
        {
            device->SetRenderState(D3DRS_ZENABLE, TRUE);
            device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
            device->Clear(0, NULL, D3DCLEAR_ZBUFFER, color, depth, stencil);
        }

        if (mMaskedClearSavedState != NULL)
        {
            mMaskedClearSavedState->Apply();
        }
    }
    else if (flags)
    {
        device->Clear(0, NULL, flags, color, depth, stencil);
    }
}

void Context::drawArrays(GLenum mode, GLint first, GLsizei count)
{
    if (!mState.currentProgram)
    {
        return error(GL_INVALID_OPERATION);
    }

    egl::Display *display = getDisplay();
    IDirect3DDevice9 *device = getDevice();
    D3DPRIMITIVETYPE primitiveType;
    int primitiveCount;

    if(!es2dx::ConvertPrimitiveType(mode, count, &primitiveType, &primitiveCount))
        return error(GL_INVALID_ENUM);

    if (primitiveCount <= 0)
    {
        return;
    }

    if (!applyRenderTarget(false))
    {
        return;
    }

    applyState(mode);

    TranslatedIndexData indexInfo;
    bool useIndexing;
    GLenum err = applyVertexBuffer(mode, first, count, &useIndexing, &indexInfo);
    if (err != GL_NO_ERROR)
    {
        return error(err);
    }

    applyShaders();
    applyTextures();

    if (!getCurrentProgram()->validateSamplers())
    {
        return error(GL_INVALID_OPERATION);
    }

    if (!cullSkipsDraw(mode))
    {
        display->startScene();
        if (useIndexing)
        {
            device->DrawIndexedPrimitive(primitiveType, -(INT)indexInfo.minIndex, indexInfo.minIndex, indexInfo.maxIndex-indexInfo.minIndex+1, indexInfo.offset/indexInfo.indexSize, primitiveCount);
        }
        else
        {
            device->DrawPrimitive(primitiveType, 0, primitiveCount);
        }
    }
}

void Context::drawElements(GLenum mode, GLsizei count, GLenum type, const void* indices)
{
    if (!mState.currentProgram)
    {
        return error(GL_INVALID_OPERATION);
    }

    if (!indices && !mState.elementArrayBuffer)
    {
        return error(GL_INVALID_OPERATION);
    }

    egl::Display *display = getDisplay();
    IDirect3DDevice9 *device = getDevice();
    D3DPRIMITIVETYPE primitiveType;
    int primitiveCount;

    if(!es2dx::ConvertPrimitiveType(mode, count, &primitiveType, &primitiveCount))
        return error(GL_INVALID_ENUM);

    if (primitiveCount <= 0)
    {
        return;
    }

    if (!applyRenderTarget(false))
    {
        return;
    }

    applyState(mode);

    TranslatedIndexData indexInfo;
    GLenum err = applyIndexBuffer(indices, count, mode, type, &indexInfo);
    if (err != GL_NO_ERROR)
    {
        return error(err);
    }

    err = applyVertexBuffer(indexInfo);
    if (err != GL_NO_ERROR)
    {
        return error(err);
    }

    applyShaders();
    applyTextures();

    if (!getCurrentProgram()->validateSamplers())
    {
        return error(GL_INVALID_OPERATION);
    }

    if (!cullSkipsDraw(mode))
    {
        display->startScene();
        device->DrawIndexedPrimitive(primitiveType, -(INT)indexInfo.minIndex, indexInfo.minIndex, indexInfo.maxIndex-indexInfo.minIndex+1, indexInfo.offset/indexInfo.indexSize, primitiveCount);
    }
}

void Context::finish()
{
    egl::Display *display = getDisplay();
    IDirect3DDevice9 *device = getDevice();
    IDirect3DQuery9 *occlusionQuery = NULL;

    HRESULT result = device->CreateQuery(D3DQUERYTYPE_OCCLUSION, &occlusionQuery);

    if (result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY)
    {
        return error(GL_OUT_OF_MEMORY);
    }

    ASSERT(SUCCEEDED(result));

    if (occlusionQuery)
    {
        IDirect3DStateBlock9 *savedState = NULL;
        device->CreateStateBlock(D3DSBT_ALL, &savedState);

        HRESULT result = occlusionQuery->Issue(D3DISSUE_BEGIN);
        ASSERT(SUCCEEDED(result));

        // Render something outside the render target
        device->SetStreamSourceFreq(0, 1);
        device->SetPixelShader(NULL);
        device->SetVertexShader(NULL);
        device->SetFVF(D3DFVF_XYZRHW);
        float data[4] = {-1.0f, -1.0f, -1.0f, 1.0f};
        display->startScene();
        device->DrawPrimitiveUP(D3DPT_POINTLIST, 1, data, sizeof(data));

        result = occlusionQuery->Issue(D3DISSUE_END);
        ASSERT(SUCCEEDED(result));

        while (occlusionQuery->GetData(NULL, 0, D3DGETDATA_FLUSH) == S_FALSE)
        {
            // Keep polling, but allow other threads to do something useful first
            Sleep(0);
        }

        occlusionQuery->Release();

        if (savedState)
        {
            savedState->Apply();
            savedState->Release();
        }
    }
}

void Context::flush()
{
    IDirect3DDevice9 *device = getDevice();
    IDirect3DQuery9 *eventQuery = NULL;

    HRESULT result = device->CreateQuery(D3DQUERYTYPE_EVENT, &eventQuery);

    if (result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY)
    {
        return error(GL_OUT_OF_MEMORY);
    }

    ASSERT(SUCCEEDED(result));

    if (eventQuery)
    {
        HRESULT result = eventQuery->Issue(D3DISSUE_END);
        ASSERT(SUCCEEDED(result));

        result = eventQuery->GetData(NULL, 0, D3DGETDATA_FLUSH);
        eventQuery->Release();

        if (result == D3DERR_DEVICELOST)
        {
            error(GL_OUT_OF_MEMORY);
        }
    }
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

bool Context::supportsShaderModel3() const
{
    return mSupportsShaderModel3;
}

int Context::getMaxSupportedSamples() const
{
    return mMaxSupportedSamples;
}

int Context::getNearestSupportedSamples(D3DFORMAT format, int requested) const
{
    if (requested == 0)
    {
        return requested;
    }

    std::map<D3DFORMAT, bool *>::const_iterator itr = mMultiSampleSupport.find(format);
    if (itr == mMultiSampleSupport.end())
    {
        return -1;
    }

    for (int i = requested; i <= D3DMULTISAMPLE_16_SAMPLES; ++i)
    {
        if (itr->second[i] && i != D3DMULTISAMPLE_NONMASKABLE)
        {
            return i;
        }
    }

    return -1;
}

bool Context::supportsCompressedTextures() const
{
    return mSupportsCompressedTextures;
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

    if (mState.elementArrayBuffer.id() == buffer)
    {
        mState.elementArrayBuffer.set(NULL);
    }

    for (int attribute = 0; attribute < MAX_VERTEX_ATTRIBS; attribute++)
    {
        if (mState.vertexAttribute[attribute].mBoundBuffer.id() == buffer)
        {
            mState.vertexAttribute[attribute].mBoundBuffer.set(NULL);
        }
    }
}

void Context::detachTexture(GLuint texture)
{
    // [OpenGL ES 2.0.24] section 3.8 page 84:
    // If a texture object is deleted, it is as if all texture units which are bound to that texture object are
    // rebound to texture object zero

    for (int type = 0; type < SAMPLER_TYPE_COUNT; type++)
    {
        for (int sampler = 0; sampler < MAX_TEXTURE_IMAGE_UNITS; sampler++)
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

Texture *Context::getIncompleteTexture(SamplerType type)
{
    Texture *t = mIncompleteTextures[type];

    if (t == NULL)
    {
        static const GLubyte color[] = { 0, 0, 0, 255 };

        switch (type)
        {
          default:
            UNREACHABLE();
            // default falls through to SAMPLER_2D

          case SAMPLER_2D:
            {
                Texture2D *incomplete2d = new Texture2D(Texture::INCOMPLETE_TEXTURE_ID);
                incomplete2d->setImage(0, GL_RGBA, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 1, color);
                t = incomplete2d;
            }
            break;

          case SAMPLER_CUBE:
            {
              TextureCubeMap *incompleteCube = new TextureCubeMap(Texture::INCOMPLETE_TEXTURE_ID);

              incompleteCube->setImagePosX(0, GL_RGBA, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 1, color);
              incompleteCube->setImageNegX(0, GL_RGBA, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 1, color);
              incompleteCube->setImagePosY(0, GL_RGBA, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 1, color);
              incompleteCube->setImageNegY(0, GL_RGBA, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 1, color);
              incompleteCube->setImagePosZ(0, GL_RGBA, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 1, color);
              incompleteCube->setImageNegZ(0, GL_RGBA, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 1, color);

              t = incompleteCube;
            }
            break;
        }

        mIncompleteTextures[type] = t;
    }

    return t;
}

bool Context::cullSkipsDraw(GLenum drawMode)
{
    return mState.cullFace && mState.cullMode == GL_FRONT_AND_BACK && isTriangleMode(drawMode);
}

bool Context::isTriangleMode(GLenum drawMode)
{
    switch (drawMode)
    {
      case GL_TRIANGLES:
      case GL_TRIANGLE_FAN:
      case GL_TRIANGLE_STRIP:
        return true;
      case GL_POINTS:
      case GL_LINES:
      case GL_LINE_LOOP:
      case GL_LINE_STRIP:
        return false;
      default: UNREACHABLE();
    }

    return false;
}

void Context::setVertexAttrib(GLuint index, const GLfloat *values)
{
    ASSERT(index < gl::MAX_VERTEX_ATTRIBS);

    mState.vertexAttribute[index].mCurrentValue[0] = values[0];
    mState.vertexAttribute[index].mCurrentValue[1] = values[1];
    mState.vertexAttribute[index].mCurrentValue[2] = values[2];
    mState.vertexAttribute[index].mCurrentValue[3] = values[3];

    mVertexDataManager->dirtyCurrentValues();
}

void Context::initExtensionString()
{
    mExtensionString += "GL_OES_packed_depth_stencil ";
    mExtensionString += "GL_EXT_texture_format_BGRA8888 ";
    mExtensionString += "GL_EXT_read_format_bgra ";
    mExtensionString += "GL_ANGLE_framebuffer_blit ";
    mExtensionString += "GL_OES_rgb8_rgba8 ";

    if (supportsCompressedTextures())
    {
        mExtensionString += "GL_EXT_texture_compression_dxt1 ";
    }

    if (getMaxSupportedSamples() != 0)
    {
        mExtensionString += "GL_ANGLE_framebuffer_multisample ";
    }

    if (mBufferBackEnd->supportIntIndices())
    {
        mExtensionString += "GL_OES_element_index_uint ";
    }

    std::string::size_type end = mExtensionString.find_last_not_of(' ');
    if (end != std::string::npos)
    {
        mExtensionString.resize(end+1);
    }
}

const char *Context::getExtensionString() const
{
    return mExtensionString.c_str();
}

void Context::blitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, 
                              GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                              GLbitfield mask)
{
    IDirect3DDevice9 *device = getDevice();

    Framebuffer *readFramebuffer = getReadFramebuffer();
    Framebuffer *drawFramebuffer = getDrawFramebuffer();

    if (!readFramebuffer || readFramebuffer->completeness() != GL_FRAMEBUFFER_COMPLETE ||
        !drawFramebuffer || drawFramebuffer->completeness() != GL_FRAMEBUFFER_COMPLETE)
    {
        return error(GL_INVALID_FRAMEBUFFER_OPERATION);
    }

    if (drawFramebuffer->getSamples() != 0)
    {
        return error(GL_INVALID_OPERATION);
    }

    RECT sourceRect;
    RECT destRect;

    if (srcX0 < srcX1)
    {
        sourceRect.left = srcX0;
        sourceRect.right = srcX1;
        destRect.left = dstX0;
        destRect.right = dstX1;
    }
    else
    {
        sourceRect.left = srcX1;
        destRect.left = dstX1;
        sourceRect.right = srcX0;
        destRect.right = dstX0;
    }

    // Arguments to StretchRect must be in D3D-style (0-top) coordinates, so we must 
    // flip our Y-values here
    if (srcY0 < srcY1)
    {
        sourceRect.bottom = srcY1;
        destRect.bottom = dstY1;
        sourceRect.top = srcY0;
        destRect.top = dstY0;
    }
    else
    {
        sourceRect.bottom = srcY0;
        destRect.bottom = dstY0;
        sourceRect.top = srcY1;
        destRect.top = dstY1;
    }

    RECT sourceScissoredRect = sourceRect;
    RECT destScissoredRect = destRect;

    if (mState.scissorTest)
    {
        // Only write to parts of the destination framebuffer which pass the scissor test
        // Please note: the destRect is now in D3D-style coordinates, so the *top* of the
        // rect will be checked against scissorY, rather than the bottom.
        if (destRect.left < mState.scissorX)
        {
            int xDiff = mState.scissorX - destRect.left;
            destScissoredRect.left = mState.scissorX;
            sourceScissoredRect.left += xDiff;
        }

        if (destRect.right > mState.scissorX + mState.scissorWidth)
        {
            int xDiff = destRect.right - (mState.scissorX + mState.scissorWidth);
            destScissoredRect.right = mState.scissorX + mState.scissorWidth;
            sourceScissoredRect.right -= xDiff;
        }

        if (destRect.top < mState.scissorY)
        {
            int yDiff = mState.scissorY - destRect.top;
            destScissoredRect.top = mState.scissorY;
            sourceScissoredRect.top += yDiff;
        }

        if (destRect.bottom > mState.scissorY + mState.scissorHeight)
        {
            int yDiff = destRect.bottom - (mState.scissorY + mState.scissorHeight);
            destScissoredRect.bottom = mState.scissorY + mState.scissorHeight;
            sourceScissoredRect.bottom -= yDiff;
        }
    }

    bool blitRenderTarget = false;
    bool blitDepthStencil = false;

    RECT sourceTrimmedRect = sourceScissoredRect;
    RECT destTrimmedRect = destScissoredRect;

    // The source & destination rectangles also may need to be trimmed if they fall out of the bounds of 
    // the actual draw and read surfaces.
    if (sourceTrimmedRect.left < 0)
    {
        int xDiff = 0 - sourceTrimmedRect.left;
        sourceTrimmedRect.left = 0;
        destTrimmedRect.left += xDiff;
    }

    int readBufferWidth = readFramebuffer->getColorbuffer()->getWidth();
    int readBufferHeight = readFramebuffer->getColorbuffer()->getHeight();
    int drawBufferWidth = drawFramebuffer->getColorbuffer()->getWidth();
    int drawBufferHeight = drawFramebuffer->getColorbuffer()->getHeight();

    if (sourceTrimmedRect.right > readBufferWidth)
    {
        int xDiff = sourceTrimmedRect.right - readBufferWidth;
        sourceTrimmedRect.right = readBufferWidth;
        destTrimmedRect.right -= xDiff;
    }

    if (sourceTrimmedRect.top < 0)
    {
        int yDiff = 0 - sourceTrimmedRect.top;
        sourceTrimmedRect.top = 0;
        destTrimmedRect.top += yDiff;
    }

    if (sourceTrimmedRect.bottom > readBufferHeight)
    {
        int yDiff = sourceTrimmedRect.bottom - readBufferHeight;
        sourceTrimmedRect.bottom = readBufferHeight;
        destTrimmedRect.bottom -= yDiff;
    }

    if (destTrimmedRect.left < 0)
    {
        int xDiff = 0 - destTrimmedRect.left;
        destTrimmedRect.left = 0;
        sourceTrimmedRect.left += xDiff;
    }

    if (destTrimmedRect.right > drawBufferWidth)
    {
        int xDiff = destTrimmedRect.right - drawBufferWidth;
        destTrimmedRect.right = drawBufferWidth;
        sourceTrimmedRect.right -= xDiff;
    }

    if (destTrimmedRect.top < 0)
    {
        int yDiff = 0 - destTrimmedRect.top;
        destTrimmedRect.top = 0;
        sourceTrimmedRect.top += yDiff;
    }

    if (destTrimmedRect.bottom > drawBufferHeight)
    {
        int yDiff = destTrimmedRect.bottom - drawBufferHeight;
        destTrimmedRect.bottom = drawBufferHeight;
        sourceTrimmedRect.bottom -= yDiff;
    }

    bool partialBufferCopy = false;
    if (sourceTrimmedRect.bottom - sourceTrimmedRect.top < readFramebuffer->getColorbuffer()->getHeight() ||
        sourceTrimmedRect.right - sourceTrimmedRect.left < readFramebuffer->getColorbuffer()->getWidth() || 
        destTrimmedRect.bottom - destTrimmedRect.top < drawFramebuffer->getColorbuffer()->getHeight() ||
        destTrimmedRect.right - destTrimmedRect.left < drawFramebuffer->getColorbuffer()->getWidth() ||
        sourceTrimmedRect.top != 0 || destTrimmedRect.top != 0 || sourceTrimmedRect.left != 0 || destTrimmedRect.left != 0)
    {
        partialBufferCopy = true;
    }

    if (mask & GL_COLOR_BUFFER_BIT)
    {
        if (readFramebuffer->getColorbufferType() != drawFramebuffer->getColorbufferType() ||
            readFramebuffer->getColorbuffer()->getD3DFormat() != drawFramebuffer->getColorbuffer()->getD3DFormat())
        {
            ERR("Color buffer format conversion in BlitFramebufferANGLE not supported by this implementation");
            return error(GL_INVALID_OPERATION);
        }
        
        if (partialBufferCopy && readFramebuffer->getSamples() != 0)
        {
            return error(GL_INVALID_OPERATION);
        }

        blitRenderTarget = true;

    }

    if (mask & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT))
    {
        DepthStencilbuffer *readDSBuffer = NULL;
        DepthStencilbuffer *drawDSBuffer = NULL;

        // We support OES_packed_depth_stencil, and do not support a separately attached depth and stencil buffer, so if we have
        // both a depth and stencil buffer, it will be the same buffer.

        if (mask & GL_DEPTH_BUFFER_BIT)
        {
            if (readFramebuffer->getDepthbuffer() && drawFramebuffer->getDepthbuffer())
            {
                if (readFramebuffer->getDepthbufferType() != drawFramebuffer->getDepthbufferType() ||
                    readFramebuffer->getDepthbuffer()->getD3DFormat() != drawFramebuffer->getDepthbuffer()->getD3DFormat())
                {
                    return error(GL_INVALID_OPERATION);
                }

                blitDepthStencil = true;
                readDSBuffer = readFramebuffer->getDepthbuffer();
                drawDSBuffer = drawFramebuffer->getDepthbuffer();
            }
        }

        if (mask & GL_STENCIL_BUFFER_BIT)
        {
            if (readFramebuffer->getStencilbuffer() && drawFramebuffer->getStencilbuffer())
            {
                if (readFramebuffer->getStencilbufferType() != drawFramebuffer->getStencilbufferType() ||
                    readFramebuffer->getStencilbuffer()->getD3DFormat() != drawFramebuffer->getStencilbuffer()->getD3DFormat())
                {
                    return error(GL_INVALID_OPERATION);
                }

                blitDepthStencil = true;
                readDSBuffer = readFramebuffer->getStencilbuffer();
                drawDSBuffer = drawFramebuffer->getStencilbuffer();
            }
        }

        if (partialBufferCopy)
        {
            ERR("Only whole-buffer depth and stencil blits are supported by this implementation.");
            return error(GL_INVALID_OPERATION); // only whole-buffer copies are permitted
        }

        if ((drawDSBuffer && drawDSBuffer->getSamples() != 0) || 
            (readDSBuffer && readDSBuffer->getSamples() != 0))
        {
            return error(GL_INVALID_OPERATION);
        }
    }

    if (blitRenderTarget || blitDepthStencil)
    {
        egl::Display *display = getDisplay();
        display->endScene();

        if (blitRenderTarget)
        {
            HRESULT result = device->StretchRect(readFramebuffer->getRenderTarget(), &sourceTrimmedRect, 
                                                 drawFramebuffer->getRenderTarget(), &destTrimmedRect, D3DTEXF_NONE);

            if (FAILED(result))
            {
                ERR("BlitFramebufferANGLE failed: StretchRect returned %x.", result);
                return;
            }
        }

        if (blitDepthStencil)
        {
            HRESULT result = device->StretchRect(readFramebuffer->getDepthStencil(), NULL, drawFramebuffer->getDepthStencil(), NULL, D3DTEXF_NONE);

            if (FAILED(result))
            {
                ERR("BlitFramebufferANGLE failed: StretchRect returned %x.", result);
                return;
            }
        }
    }
}

}

extern "C"
{
gl::Context *glCreateContext(const egl::Config *config, const gl::Context *shareContext)
{
    return new gl::Context(config, shareContext);
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
