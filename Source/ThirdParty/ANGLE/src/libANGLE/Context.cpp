//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Context.cpp: Implements the gl::Context class, managing all GL state and performing
// rendering operations. It is the GLES2 specific implementation of EGLContext.

#include "libANGLE/Context.h"

#include <iterator>
#include <sstream>

#include "common/platform.h"
#include "common/utilities.h"
#include "libANGLE/Buffer.h"
#include "libANGLE/Compiler.h"
#include "libANGLE/Display.h"
#include "libANGLE/Fence.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/Program.h"
#include "libANGLE/Query.h"
#include "libANGLE/Renderbuffer.h"
#include "libANGLE/ResourceManager.h"
#include "libANGLE/Sampler.h"
#include "libANGLE/Surface.h"
#include "libANGLE/Texture.h"
#include "libANGLE/TransformFeedback.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/validationES.h"
#include "libANGLE/renderer/Renderer.h"

namespace
{

template <typename T>
gl::Error GetQueryObjectParameter(gl::Context *context, GLuint id, GLenum pname, T *params)
{
    gl::Query *queryObject = context->getQuery(id, false, GL_NONE);
    ASSERT(queryObject != nullptr);

    switch (pname)
    {
        case GL_QUERY_RESULT_EXT:
            return queryObject->getResult(params);
        case GL_QUERY_RESULT_AVAILABLE_EXT:
        {
            bool available;
            gl::Error error = queryObject->isResultAvailable(&available);
            if (!error.isError())
            {
                *params = static_cast<T>(available ? GL_TRUE : GL_FALSE);
            }
            return error;
        }
        default:
            UNREACHABLE();
            return gl::Error(GL_INVALID_OPERATION, "Unreachable Error");
    }
}

void MarkTransformFeedbackBufferUsage(gl::TransformFeedback *transformFeedback)
{
    if (transformFeedback && transformFeedback->isActive() && !transformFeedback->isPaused())
    {
        for (size_t tfBufferIndex = 0; tfBufferIndex < transformFeedback->getIndexedBufferCount();
             tfBufferIndex++)
        {
            const OffsetBindingPointer<gl::Buffer> &buffer =
                transformFeedback->getIndexedBuffer(tfBufferIndex);
            if (buffer.get() != nullptr)
            {
                buffer->onTransformFeedback();
            }
        }
    }
}

// Attribute map queries.
EGLint GetClientVersion(const egl::AttributeMap &attribs)
{
    return static_cast<EGLint>(attribs.get(EGL_CONTEXT_CLIENT_VERSION, 1));
}

GLenum GetResetStrategy(const egl::AttributeMap &attribs)
{
    EGLAttrib attrib = attribs.get(EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT,
                                   EGL_NO_RESET_NOTIFICATION_EXT);
    switch (attrib)
    {
        case EGL_NO_RESET_NOTIFICATION:
            return GL_NO_RESET_NOTIFICATION_EXT;
        case EGL_LOSE_CONTEXT_ON_RESET:
            return GL_LOSE_CONTEXT_ON_RESET_EXT;
        default:
            UNREACHABLE();
            return GL_NONE;
    }
}

bool GetRobustAccess(const egl::AttributeMap &attribs)
{
    return (attribs.get(EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT, EGL_FALSE) == EGL_TRUE);
}

bool GetDebug(const egl::AttributeMap &attribs)
{
    return (attribs.get(EGL_CONTEXT_OPENGL_DEBUG, EGL_FALSE) == EGL_TRUE);
}

bool GetNoError(const egl::AttributeMap &attribs)
{
    return (attribs.get(EGL_CONTEXT_OPENGL_NO_ERROR_KHR, EGL_FALSE) == EGL_TRUE);
}

}  // anonymous namespace

namespace gl
{

Context::Context(const egl::Config *config,
                 const Context *shareContext,
                 rx::Renderer *renderer,
                 const egl::AttributeMap &attribs)
    : ValidationContext(GetClientVersion(attribs),
                        mState,
                        mCaps,
                        mTextureCaps,
                        mExtensions,
                        nullptr,
                        mLimitations,
                        GetNoError(attribs)),
      mCompiler(nullptr),
      mRenderer(renderer),
      mClientVersion(GetClientVersion(attribs)),
      mConfig(config),
      mClientType(EGL_OPENGL_ES_API),
      mHasBeenCurrent(false),
      mContextLost(false),
      mResetStatus(GL_NO_ERROR),
      mResetStrategy(GetResetStrategy(attribs)),
      mRobustAccess(GetRobustAccess(attribs)),
      mCurrentSurface(nullptr),
      mResourceManager(nullptr)
{
    ASSERT(!mRobustAccess);  // Unimplemented

    initCaps(mClientVersion);

    mState.initialize(mCaps, mExtensions, mClientVersion, GetDebug(attribs));

    mFenceNVHandleAllocator.setBaseHandle(0);

    if (shareContext != NULL)
    {
        mResourceManager = shareContext->mResourceManager;
        mResourceManager->addRef();
    }
    else
    {
        mResourceManager = new ResourceManager(mRenderer);
    }

    mData.resourceManager = mResourceManager;

    // [OpenGL ES 2.0.24] section 3.7 page 83:
    // In the initial state, TEXTURE_2D and TEXTURE_CUBE_MAP have twodimensional
    // and cube map texture state vectors respectively associated with them.
    // In order that access to these initial textures not be lost, they are treated as texture
    // objects all of whose names are 0.

    Texture *zeroTexture2D = new Texture(mRenderer->createTexture(GL_TEXTURE_2D), 0, GL_TEXTURE_2D);
    mZeroTextures[GL_TEXTURE_2D].set(zeroTexture2D);

    Texture *zeroTextureCube = new Texture(mRenderer->createTexture(GL_TEXTURE_CUBE_MAP), 0, GL_TEXTURE_CUBE_MAP);
    mZeroTextures[GL_TEXTURE_CUBE_MAP].set(zeroTextureCube);

    if (mClientVersion >= 3)
    {
        // TODO: These could also be enabled via extension
        Texture *zeroTexture3D = new Texture(mRenderer->createTexture(GL_TEXTURE_3D), 0, GL_TEXTURE_3D);
        mZeroTextures[GL_TEXTURE_3D].set(zeroTexture3D);

        Texture *zeroTexture2DArray = new Texture(mRenderer->createTexture(GL_TEXTURE_2D_ARRAY), 0, GL_TEXTURE_2D_ARRAY);
        mZeroTextures[GL_TEXTURE_2D_ARRAY].set(zeroTexture2DArray);
    }

    mState.initializeZeroTextures(mZeroTextures);

    bindVertexArray(0);
    bindArrayBuffer(0);
    bindElementArrayBuffer(0);

    bindRenderbuffer(0);

    bindGenericUniformBuffer(0);
    for (unsigned int i = 0; i < mCaps.maxCombinedUniformBlocks; i++)
    {
        bindIndexedUniformBuffer(0, i, 0, -1);
    }

    bindCopyReadBuffer(0);
    bindCopyWriteBuffer(0);
    bindPixelPackBuffer(0);
    bindPixelUnpackBuffer(0);

    if (mClientVersion >= 3)
    {
        // [OpenGL ES 3.0.2] section 2.14.1 pg 85:
        // In the initial state, a default transform feedback object is bound and treated as
        // a transform feedback object with a name of zero. That object is bound any time
        // BindTransformFeedback is called with id of zero
        bindTransformFeedback(0);
    }

    mCompiler = new Compiler(mRenderer, getData());

    // Initialize dirty bit masks
    // TODO(jmadill): additional ES3 state
    mTexImageDirtyBits.set(State::DIRTY_BIT_UNPACK_ALIGNMENT);
    mTexImageDirtyBits.set(State::DIRTY_BIT_UNPACK_ROW_LENGTH);
    mTexImageDirtyBits.set(State::DIRTY_BIT_UNPACK_IMAGE_HEIGHT);
    mTexImageDirtyBits.set(State::DIRTY_BIT_UNPACK_SKIP_IMAGES);
    mTexImageDirtyBits.set(State::DIRTY_BIT_UNPACK_SKIP_ROWS);
    mTexImageDirtyBits.set(State::DIRTY_BIT_UNPACK_SKIP_PIXELS);
    // No dirty objects.

    // Readpixels uses the pack state and read FBO
    mReadPixelsDirtyBits.set(State::DIRTY_BIT_PACK_ALIGNMENT);
    mReadPixelsDirtyBits.set(State::DIRTY_BIT_PACK_REVERSE_ROW_ORDER);
    mReadPixelsDirtyBits.set(State::DIRTY_BIT_PACK_ROW_LENGTH);
    mReadPixelsDirtyBits.set(State::DIRTY_BIT_PACK_SKIP_ROWS);
    mReadPixelsDirtyBits.set(State::DIRTY_BIT_PACK_SKIP_PIXELS);
    mReadPixelsDirtyObjects.set(State::DIRTY_OBJECT_READ_FRAMEBUFFER);

    mClearDirtyBits.set(State::DIRTY_BIT_RASTERIZER_DISCARD_ENABLED);
    mClearDirtyBits.set(State::DIRTY_BIT_SCISSOR_TEST_ENABLED);
    mClearDirtyBits.set(State::DIRTY_BIT_SCISSOR);
    mClearDirtyBits.set(State::DIRTY_BIT_VIEWPORT);
    mClearDirtyBits.set(State::DIRTY_BIT_CLEAR_COLOR);
    mClearDirtyBits.set(State::DIRTY_BIT_CLEAR_DEPTH);
    mClearDirtyBits.set(State::DIRTY_BIT_CLEAR_STENCIL);
    mClearDirtyBits.set(State::DIRTY_BIT_COLOR_MASK);
    mClearDirtyBits.set(State::DIRTY_BIT_DEPTH_MASK);
    mClearDirtyBits.set(State::DIRTY_BIT_STENCIL_WRITEMASK_FRONT);
    mClearDirtyBits.set(State::DIRTY_BIT_STENCIL_WRITEMASK_BACK);
    mClearDirtyObjects.set(State::DIRTY_OBJECT_DRAW_FRAMEBUFFER);

    mBlitDirtyBits.set(State::DIRTY_BIT_SCISSOR_TEST_ENABLED);
    mBlitDirtyBits.set(State::DIRTY_BIT_SCISSOR);
    mBlitDirtyObjects.set(State::DIRTY_OBJECT_READ_FRAMEBUFFER);
    mBlitDirtyObjects.set(State::DIRTY_OBJECT_DRAW_FRAMEBUFFER);
}

Context::~Context()
{
    mState.reset();

    for (auto framebuffer : mFramebufferMap)
    {
        // Default framebuffer are owned by their respective Surface
        if (framebuffer.second != nullptr && framebuffer.second->id() != 0)
        {
            SafeDelete(framebuffer.second);
        }
    }

    for (auto fence : mFenceNVMap)
    {
        SafeDelete(fence.second);
    }

    for (auto query : mQueryMap)
    {
        if (query.second != nullptr)
        {
            query.second->release();
        }
    }

    for (auto vertexArray : mVertexArrayMap)
    {
        SafeDelete(vertexArray.second);
    }

    for (auto transformFeedback : mTransformFeedbackMap)
    {
        if (transformFeedback.second != nullptr)
        {
            transformFeedback.second->release();
        }
    }

    for (auto &zeroTexture : mZeroTextures)
    {
        zeroTexture.second.set(NULL);
    }
    mZeroTextures.clear();

    if (mCurrentSurface != nullptr)
    {
        releaseSurface();
    }

    if (mResourceManager)
    {
        mResourceManager->release();
    }

    SafeDelete(mCompiler);
}

void Context::makeCurrent(egl::Surface *surface)
{
    ASSERT(surface != nullptr);

    if (!mHasBeenCurrent)
    {
        initRendererString();
        initExtensionStrings();

        mState.setViewportParams(0, 0, surface->getWidth(), surface->getHeight());
        mState.setScissorParams(0, 0, surface->getWidth(), surface->getHeight());

        mHasBeenCurrent = true;
    }

    // TODO(jmadill): Rework this when we support ContextImpl
    mState.setAllDirtyBits();

    if (mCurrentSurface)
    {
        releaseSurface();
    }
    surface->setIsCurrent(true);
    mCurrentSurface = surface;

    // Update default framebuffer, the binding of the previous default
    // framebuffer (or lack of) will have a nullptr.
    {
        Framebuffer *newDefault = surface->getDefaultFramebuffer();
        if (mState.getReadFramebuffer() == nullptr)
        {
            mState.setReadFramebufferBinding(newDefault);
        }
        if (mState.getDrawFramebuffer() == nullptr)
        {
            mState.setDrawFramebufferBinding(newDefault);
        }
        mFramebufferMap[0] = newDefault;
    }

    // Notify the renderer of a context switch
    mRenderer->onMakeCurrent(getData());
}

void Context::releaseSurface()
{
    ASSERT(mCurrentSurface != nullptr);

    // Remove the default framebuffer
    {
        Framebuffer *currentDefault = mCurrentSurface->getDefaultFramebuffer();
        if (mState.getReadFramebuffer() == currentDefault)
        {
            mState.setReadFramebufferBinding(nullptr);
        }
        if (mState.getDrawFramebuffer() == currentDefault)
        {
            mState.setDrawFramebufferBinding(nullptr);
        }
        mFramebufferMap.erase(0);
    }

    mCurrentSurface->setIsCurrent(false);
    mCurrentSurface = nullptr;
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
    return mResourceManager->createShader(mRenderer->getRendererLimitations(), type);
}

GLuint Context::createTexture()
{
    return mResourceManager->createTexture();
}

GLuint Context::createRenderbuffer()
{
    return mResourceManager->createRenderbuffer();
}

GLsync Context::createFenceSync()
{
    GLuint handle = mResourceManager->createFenceSync();

    return reinterpret_cast<GLsync>(static_cast<uintptr_t>(handle));
}

GLuint Context::createVertexArray()
{
    GLuint vertexArray           = mVertexArrayHandleAllocator.allocate();
    mVertexArrayMap[vertexArray] = nullptr;
    return vertexArray;
}

GLuint Context::createSampler()
{
    return mResourceManager->createSampler();
}

GLuint Context::createTransformFeedback()
{
    GLuint transformFeedback                 = mTransformFeedbackAllocator.allocate();
    mTransformFeedbackMap[transformFeedback] = nullptr;
    return transformFeedback;
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

    mFenceNVMap[handle] = new FenceNV(mRenderer->createFenceNV());

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
    mResourceManager->deleteFenceSync(static_cast<GLuint>(reinterpret_cast<uintptr_t>(fenceSync)));
}

void Context::deleteVertexArray(GLuint vertexArray)
{
    auto iter = mVertexArrayMap.find(vertexArray);
    if (iter != mVertexArrayMap.end())
    {
        VertexArray *vertexArrayObject = iter->second;
        if (vertexArrayObject != nullptr)
        {
            detachVertexArray(vertexArray);
            delete vertexArrayObject;
        }

        mVertexArrayMap.erase(iter);
        mVertexArrayHandleAllocator.release(vertexArray);
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
    auto iter = mTransformFeedbackMap.find(transformFeedback);
    if (iter != mTransformFeedbackMap.end())
    {
        TransformFeedback *transformFeedbackObject = iter->second;
        if (transformFeedbackObject != nullptr)
        {
            detachTransformFeedback(transformFeedback);
            transformFeedbackObject->release();
        }

        mTransformFeedbackMap.erase(iter);
        mTransformFeedbackAllocator.release(transformFeedback);
    }
}

void Context::deleteFramebuffer(GLuint framebuffer)
{
    auto framebufferObject = mFramebufferMap.find(framebuffer);

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
    auto fenceObject = mFenceNVMap.find(fence);

    if (fenceObject != mFenceNVMap.end())
    {
        mFenceNVHandleAllocator.release(fenceObject->first);
        delete fenceObject->second;
        mFenceNVMap.erase(fenceObject);
    }
}

void Context::deleteQuery(GLuint query)
{
    auto queryObject = mQueryMap.find(query);
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

Buffer *Context::getBuffer(GLuint handle) const
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

Texture *Context::getTexture(GLuint handle) const
{
    return mResourceManager->getTexture(handle);
}

Renderbuffer *Context::getRenderbuffer(GLuint handle) const
{
    return mResourceManager->getRenderbuffer(handle);
}

FenceSync *Context::getFenceSync(GLsync handle) const
{
    return mResourceManager->getFenceSync(static_cast<GLuint>(reinterpret_cast<uintptr_t>(handle)));
}

VertexArray *Context::getVertexArray(GLuint handle) const
{
    auto vertexArray = mVertexArrayMap.find(handle);
    return (vertexArray != mVertexArrayMap.end()) ? vertexArray->second : nullptr;
}

Sampler *Context::getSampler(GLuint handle) const
{
    return mResourceManager->getSampler(handle);
}

TransformFeedback *Context::getTransformFeedback(GLuint handle) const
{
    auto iter = mTransformFeedbackMap.find(handle);
    return (iter != mTransformFeedbackMap.end()) ? iter->second : nullptr;
}

LabeledObject *Context::getLabeledObject(GLenum identifier, GLuint name) const
{
    switch (identifier)
    {
        case GL_BUFFER:
            return getBuffer(name);
        case GL_SHADER:
            return getShader(name);
        case GL_PROGRAM:
            return getProgram(name);
        case GL_VERTEX_ARRAY:
            return getVertexArray(name);
        case GL_QUERY:
            return getQuery(name);
        case GL_TRANSFORM_FEEDBACK:
            return getTransformFeedback(name);
        case GL_SAMPLER:
            return getSampler(name);
        case GL_TEXTURE:
            return getTexture(name);
        case GL_RENDERBUFFER:
            return getRenderbuffer(name);
        case GL_FRAMEBUFFER:
            return getFramebuffer(name);
        default:
            UNREACHABLE();
            return nullptr;
    }
}

LabeledObject *Context::getLabeledObjectFromPtr(const void *ptr) const
{
    return getFenceSync(reinterpret_cast<GLsync>(const_cast<void *>(ptr)));
}

bool Context::isSampler(GLuint samplerName) const
{
    return mResourceManager->isSampler(samplerName);
}

void Context::bindArrayBuffer(GLuint bufferHandle)
{
    Buffer *buffer = mResourceManager->checkBufferAllocation(bufferHandle);
    mState.setArrayBufferBinding(buffer);
}

void Context::bindElementArrayBuffer(GLuint bufferHandle)
{
    Buffer *buffer = mResourceManager->checkBufferAllocation(bufferHandle);
    mState.getVertexArray()->setElementArrayBuffer(buffer);
}

void Context::bindTexture(GLenum target, GLuint handle)
{
    Texture *texture = nullptr;

    if (handle == 0)
    {
        texture = mZeroTextures[target].get();
    }
    else
    {
        texture = mResourceManager->checkTextureAllocation(handle, target);
    }

    ASSERT(texture);
    mState.setSamplerTexture(target, texture);
}

void Context::bindReadFramebuffer(GLuint framebufferHandle)
{
    Framebuffer *framebuffer = checkFramebufferAllocation(framebufferHandle);
    mState.setReadFramebufferBinding(framebuffer);
}

void Context::bindDrawFramebuffer(GLuint framebufferHandle)
{
    Framebuffer *framebuffer = checkFramebufferAllocation(framebufferHandle);
    mState.setDrawFramebufferBinding(framebuffer);
}

void Context::bindRenderbuffer(GLuint renderbufferHandle)
{
    Renderbuffer *renderbuffer = mResourceManager->checkRenderbufferAllocation(renderbufferHandle);
    mState.setRenderbufferBinding(renderbuffer);
}

void Context::bindVertexArray(GLuint vertexArrayHandle)
{
    VertexArray *vertexArray = checkVertexArrayAllocation(vertexArrayHandle);
    mState.setVertexArrayBinding(vertexArray);
}

void Context::bindSampler(GLuint textureUnit, GLuint samplerHandle)
{
    ASSERT(textureUnit < mCaps.maxCombinedTextureImageUnits);
    Sampler *sampler = mResourceManager->checkSamplerAllocation(samplerHandle);
    mState.setSamplerBinding(textureUnit, sampler);
}

void Context::bindGenericUniformBuffer(GLuint bufferHandle)
{
    Buffer *buffer = mResourceManager->checkBufferAllocation(bufferHandle);
    mState.setGenericUniformBufferBinding(buffer);
}

void Context::bindIndexedUniformBuffer(GLuint bufferHandle,
                                       GLuint index,
                                       GLintptr offset,
                                       GLsizeiptr size)
{
    Buffer *buffer = mResourceManager->checkBufferAllocation(bufferHandle);
    mState.setIndexedUniformBufferBinding(index, buffer, offset, size);
}

void Context::bindGenericTransformFeedbackBuffer(GLuint bufferHandle)
{
    Buffer *buffer = mResourceManager->checkBufferAllocation(bufferHandle);
    mState.getCurrentTransformFeedback()->bindGenericBuffer(buffer);
}

void Context::bindIndexedTransformFeedbackBuffer(GLuint bufferHandle,
                                                 GLuint index,
                                                 GLintptr offset,
                                                 GLsizeiptr size)
{
    Buffer *buffer = mResourceManager->checkBufferAllocation(bufferHandle);
    mState.getCurrentTransformFeedback()->bindIndexedBuffer(index, buffer, offset, size);
}

void Context::bindCopyReadBuffer(GLuint bufferHandle)
{
    Buffer *buffer = mResourceManager->checkBufferAllocation(bufferHandle);
    mState.setCopyReadBufferBinding(buffer);
}

void Context::bindCopyWriteBuffer(GLuint bufferHandle)
{
    Buffer *buffer = mResourceManager->checkBufferAllocation(bufferHandle);
    mState.setCopyWriteBufferBinding(buffer);
}

void Context::bindPixelPackBuffer(GLuint bufferHandle)
{
    Buffer *buffer = mResourceManager->checkBufferAllocation(bufferHandle);
    mState.setPixelPackBufferBinding(buffer);
}

void Context::bindPixelUnpackBuffer(GLuint bufferHandle)
{
    Buffer *buffer = mResourceManager->checkBufferAllocation(bufferHandle);
    mState.setPixelUnpackBufferBinding(buffer);
}

void Context::useProgram(GLuint program)
{
    mState.setProgram(getProgram(program));
}

void Context::bindTransformFeedback(GLuint transformFeedbackHandle)
{
    TransformFeedback *transformFeedback =
        checkTransformFeedbackAllocation(transformFeedbackHandle);
    mState.setTransformFeedbackBinding(transformFeedback);
}

Error Context::beginQuery(GLenum target, GLuint query)
{
    Query *queryObject = getQuery(query, true, target);
    ASSERT(queryObject);

    // begin query
    Error error = queryObject->begin();
    if (error.isError())
    {
        return error;
    }

    // set query as active for specified target only if begin succeeded
    mState.setActiveQuery(target, queryObject);

    return Error(GL_NO_ERROR);
}

Error Context::endQuery(GLenum target)
{
    Query *queryObject = mState.getActiveQuery(target);
    ASSERT(queryObject);

    gl::Error error = queryObject->end();

    // Always unbind the query, even if there was an error. This may delete the query object.
    mState.setActiveQuery(target, NULL);

    return error;
}

Error Context::queryCounter(GLuint id, GLenum target)
{
    ASSERT(target == GL_TIMESTAMP_EXT);

    Query *queryObject = getQuery(id, true, target);
    ASSERT(queryObject);

    return queryObject->queryCounter();
}

void Context::getQueryiv(GLenum target, GLenum pname, GLint *params)
{
    switch (pname)
    {
        case GL_CURRENT_QUERY_EXT:
            params[0] = getState().getActiveQueryId(target);
            break;
        case GL_QUERY_COUNTER_BITS_EXT:
            switch (target)
            {
                case GL_TIME_ELAPSED_EXT:
                    params[0] = getExtensions().queryCounterBitsTimeElapsed;
                    break;
                case GL_TIMESTAMP_EXT:
                    params[0] = getExtensions().queryCounterBitsTimestamp;
                    break;
                default:
                    UNREACHABLE();
                    params[0] = 0;
                    break;
            }
            break;
        default:
            UNREACHABLE();
            return;
    }
}

Error Context::getQueryObjectiv(GLuint id, GLenum pname, GLint *params)
{
    return GetQueryObjectParameter(this, id, pname, params);
}

Error Context::getQueryObjectuiv(GLuint id, GLenum pname, GLuint *params)
{
    return GetQueryObjectParameter(this, id, pname, params);
}

Error Context::getQueryObjecti64v(GLuint id, GLenum pname, GLint64 *params)
{
    return GetQueryObjectParameter(this, id, pname, params);
}

Error Context::getQueryObjectui64v(GLuint id, GLenum pname, GLuint64 *params)
{
    return GetQueryObjectParameter(this, id, pname, params);
}

Framebuffer *Context::getFramebuffer(unsigned int handle) const
{
    auto framebufferIt = mFramebufferMap.find(handle);
    return ((framebufferIt == mFramebufferMap.end()) ? nullptr : framebufferIt->second);
}

FenceNV *Context::getFenceNV(unsigned int handle)
{
    auto fence = mFenceNVMap.find(handle);

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
    auto query = mQueryMap.find(handle);

    if (query == mQueryMap.end())
    {
        return NULL;
    }
    else
    {
        if (!query->second && create)
        {
            query->second = new Query(mRenderer->createQuery(type), handle);
            query->second->addRef();
        }
        return query->second;
    }
}

Query *Context::getQuery(GLuint handle) const
{
    auto iter = mQueryMap.find(handle);
    return (iter != mQueryMap.end()) ? iter->second : nullptr;
}

Texture *Context::getTargetTexture(GLenum target) const
{
    ASSERT(ValidTextureTarget(this, target));
    return mState.getTargetTexture(target);
}

Texture *Context::getSamplerTexture(unsigned int sampler, GLenum type) const
{
    return mState.getSamplerTexture(sampler, type);
}

Compiler *Context::getCompiler() const
{
    return mCompiler;
}

void Context::getBooleanv(GLenum pname, GLboolean *params)
{
    switch (pname)
    {
      case GL_SHADER_COMPILER:           *params = GL_TRUE;                             break;
      case GL_CONTEXT_ROBUST_ACCESS_EXT: *params = mRobustAccess ? GL_TRUE : GL_FALSE;  break;
      default:
        mState.getBooleanv(pname, params);
        break;
    }
}

void Context::getFloatv(GLenum pname, GLfloat *params)
{
    // Queries about context capabilities and maximums are answered by Context.
    // Queries about current GL state values are answered by State.
    switch (pname)
    {
      case GL_ALIASED_LINE_WIDTH_RANGE:
        params[0] = mCaps.minAliasedLineWidth;
        params[1] = mCaps.maxAliasedLineWidth;
        break;
      case GL_ALIASED_POINT_SIZE_RANGE:
        params[0] = mCaps.minAliasedPointSize;
        params[1] = mCaps.maxAliasedPointSize;
        break;
      case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT:
        ASSERT(mExtensions.textureFilterAnisotropic);
        *params = mExtensions.maxTextureAnisotropy;
        break;
      case GL_MAX_TEXTURE_LOD_BIAS:
        *params = mCaps.maxLODBias;
        break;
      default:
        mState.getFloatv(pname, params);
        break;
    }
}

void Context::getIntegerv(GLenum pname, GLint *params)
{
    // Queries about context capabilities and maximums are answered by Context.
    // Queries about current GL state values are answered by State.

    switch (pname)
    {
      case GL_MAX_VERTEX_ATTRIBS:                       *params = mCaps.maxVertexAttributes;                            break;
      case GL_MAX_VERTEX_UNIFORM_VECTORS:               *params = mCaps.maxVertexUniformVectors;                        break;
      case GL_MAX_VERTEX_UNIFORM_COMPONENTS:            *params = mCaps.maxVertexUniformComponents;                     break;
      case GL_MAX_VARYING_VECTORS:                      *params = mCaps.maxVaryingVectors;                              break;
      case GL_MAX_VARYING_COMPONENTS:                   *params = mCaps.maxVertexOutputComponents;                      break;
      case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:         *params = mCaps.maxCombinedTextureImageUnits;                   break;
      case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:           *params = mCaps.maxVertexTextureImageUnits;                     break;
      case GL_MAX_TEXTURE_IMAGE_UNITS:                  *params = mCaps.maxTextureImageUnits;                           break;
      case GL_MAX_FRAGMENT_UNIFORM_VECTORS:             *params = mCaps.maxFragmentUniformVectors;                      break;
      case GL_MAX_FRAGMENT_UNIFORM_COMPONENTS:          *params = mCaps.maxFragmentUniformComponents;                   break;
      case GL_MAX_RENDERBUFFER_SIZE:                    *params = mCaps.maxRenderbufferSize;                            break;
      case GL_MAX_COLOR_ATTACHMENTS_EXT:                *params = mCaps.maxColorAttachments;                            break;
      case GL_MAX_DRAW_BUFFERS_EXT:                     *params = mCaps.maxDrawBuffers;                                 break;
      //case GL_FRAMEBUFFER_BINDING:                    // now equivalent to GL_DRAW_FRAMEBUFFER_BINDING_ANGLE
      case GL_SUBPIXEL_BITS:                            *params = 4;                                                    break;
      case GL_MAX_TEXTURE_SIZE:                         *params = mCaps.max2DTextureSize;                               break;
      case GL_MAX_CUBE_MAP_TEXTURE_SIZE:                *params = mCaps.maxCubeMapTextureSize;                          break;
      case GL_MAX_3D_TEXTURE_SIZE:                      *params = mCaps.max3DTextureSize;                               break;
      case GL_MAX_ARRAY_TEXTURE_LAYERS:                 *params = mCaps.maxArrayTextureLayers;                          break;
      case GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT:          *params = mCaps.uniformBufferOffsetAlignment;                   break;
      case GL_MAX_UNIFORM_BUFFER_BINDINGS:              *params = mCaps.maxUniformBufferBindings;                       break;
      case GL_MAX_VERTEX_UNIFORM_BLOCKS:                *params = mCaps.maxVertexUniformBlocks;                         break;
      case GL_MAX_FRAGMENT_UNIFORM_BLOCKS:              *params = mCaps.maxFragmentUniformBlocks;                       break;
      case GL_MAX_COMBINED_UNIFORM_BLOCKS:              *params = mCaps.maxCombinedTextureImageUnits;                   break;
      case GL_MAX_VERTEX_OUTPUT_COMPONENTS:             *params = mCaps.maxVertexOutputComponents;                      break;
      case GL_MAX_FRAGMENT_INPUT_COMPONENTS:            *params = mCaps.maxFragmentInputComponents;                     break;
      case GL_MIN_PROGRAM_TEXEL_OFFSET:                 *params = mCaps.minProgramTexelOffset;                          break;
      case GL_MAX_PROGRAM_TEXEL_OFFSET:                 *params = mCaps.maxProgramTexelOffset;                          break;
      case GL_MAJOR_VERSION:                            *params = mClientVersion;                                       break;
      case GL_MINOR_VERSION:                            *params = 0;                                                    break;
      case GL_MAX_ELEMENTS_INDICES:                     *params = mCaps.maxElementsIndices;                             break;
      case GL_MAX_ELEMENTS_VERTICES:                    *params = mCaps.maxElementsVertices;                            break;
      case GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS: *params = mCaps.maxTransformFeedbackInterleavedComponents; break;
      case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS:       *params = mCaps.maxTransformFeedbackSeparateAttributes;    break;
      case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS:    *params = mCaps.maxTransformFeedbackSeparateComponents;    break;
      case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
          *params = static_cast<GLint>(mCaps.compressedTextureFormats.size());
          break;
      case GL_MAX_SAMPLES_ANGLE:                        *params = mCaps.maxSamples;                                     break;
      case GL_MAX_VIEWPORT_DIMS:
        {
            params[0] = mCaps.maxViewportWidth;
            params[1] = mCaps.maxViewportHeight;
        }
        break;
      case GL_COMPRESSED_TEXTURE_FORMATS:
        std::copy(mCaps.compressedTextureFormats.begin(), mCaps.compressedTextureFormats.end(), params);
        break;
      case GL_RESET_NOTIFICATION_STRATEGY_EXT:
        *params = mResetStrategy;
        break;
      case GL_NUM_SHADER_BINARY_FORMATS:
          *params = static_cast<GLint>(mCaps.shaderBinaryFormats.size());
        break;
      case GL_SHADER_BINARY_FORMATS:
        std::copy(mCaps.shaderBinaryFormats.begin(), mCaps.shaderBinaryFormats.end(), params);
        break;
      case GL_NUM_PROGRAM_BINARY_FORMATS:
          *params = static_cast<GLint>(mCaps.programBinaryFormats.size());
        break;
      case GL_PROGRAM_BINARY_FORMATS:
        std::copy(mCaps.programBinaryFormats.begin(), mCaps.programBinaryFormats.end(), params);
        break;
      case GL_NUM_EXTENSIONS:
        *params = static_cast<GLint>(mExtensionStrings.size());
        break;

      // GL_KHR_debug
      case GL_MAX_DEBUG_MESSAGE_LENGTH:
          *params = mExtensions.maxDebugMessageLength;
          break;
      case GL_MAX_DEBUG_LOGGED_MESSAGES:
          *params = mExtensions.maxDebugLoggedMessages;
          break;
      case GL_MAX_DEBUG_GROUP_STACK_DEPTH:
          *params = mExtensions.maxDebugGroupStackDepth;
          break;
      case GL_MAX_LABEL_LENGTH:
          *params = mExtensions.maxLabelLength;
          break;

      // GL_EXT_disjoint_timer_query
      case GL_GPU_DISJOINT_EXT:
          *params = mRenderer->getGPUDisjoint();
          break;

      default:
        mState.getIntegerv(getData(), pname, params);
        break;
    }
}

void Context::getInteger64v(GLenum pname, GLint64 *params)
{
    // Queries about context capabilities and maximums are answered by Context.
    // Queries about current GL state values are answered by State.
    switch (pname)
    {
      case GL_MAX_ELEMENT_INDEX:
        *params = mCaps.maxElementIndex;
        break;
      case GL_MAX_UNIFORM_BLOCK_SIZE:
        *params = mCaps.maxUniformBlockSize;
        break;
      case GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS:
        *params = mCaps.maxCombinedVertexUniformComponents;
        break;
      case GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS:
        *params = mCaps.maxCombinedFragmentUniformComponents;
        break;
      case GL_MAX_SERVER_WAIT_TIMEOUT:
        *params = mCaps.maxServerWaitTimeout;
        break;

      // GL_EXT_disjoint_timer_query
      case GL_TIMESTAMP_EXT:
          *params = mRenderer->getTimestamp();
          break;
      default:
        UNREACHABLE();
        break;
    }
}

void Context::getPointerv(GLenum pname, void **params) const
{
    mState.getPointerv(pname, params);
}

bool Context::getIndexedIntegerv(GLenum target, GLuint index, GLint *data)
{
    // Queries about context capabilities and maximums are answered by Context.
    // Queries about current GL state values are answered by State.
    // Indexed integer queries all refer to current state, so this function is a
    // mere passthrough.
    return mState.getIndexedIntegerv(target, index, data);
}

bool Context::getIndexedInteger64v(GLenum target, GLuint index, GLint64 *data)
{
    // Queries about context capabilities and maximums are answered by Context.
    // Queries about current GL state values are answered by State.
    // Indexed integer queries all refer to current state, so this function is a
    // mere passthrough.
    return mState.getIndexedInteger64v(target, index, data);
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
            *numParams = static_cast<unsigned int>(mCaps.compressedTextureFormats.size());
        }
        return true;
      case GL_PROGRAM_BINARY_FORMATS_OES:
        {
            *type = GL_INT;
            *numParams = static_cast<unsigned int>(mCaps.programBinaryFormats.size());
        }
        return true;
      case GL_SHADER_BINARY_FORMATS:
        {
            *type = GL_INT;
            *numParams = static_cast<unsigned int>(mCaps.shaderBinaryFormats.size());
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
      //case GL_FRAMEBUFFER_BINDING: // equivalent to DRAW_FRAMEBUFFER_BINDING_ANGLE
      case GL_DRAW_FRAMEBUFFER_BINDING_ANGLE:
      case GL_READ_FRAMEBUFFER_BINDING_ANGLE:
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
        {
            *type = GL_INT;
            *numParams = 1;
        }
        return true;
      case GL_MAX_SAMPLES_ANGLE:
        {
            if (mExtensions.framebufferMultisample)
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
        if (!mExtensions.maxTextureAnisotropy)
        {
            return false;
        }
        *type = GL_FLOAT;
        *numParams = 1;
        return true;
      case GL_TIMESTAMP_EXT:
          if (!mExtensions.disjointTimerQuery)
          {
              return false;
          }
          *type      = GL_INT_64_ANGLEX;
          *numParams = 1;
          return true;
      case GL_GPU_DISJOINT_EXT:
          if (!mExtensions.disjointTimerQuery)
          {
              return false;
          }
          *type      = GL_INT;
          *numParams = 1;
          return true;
    }

    if (mExtensions.debug)
    {
        switch (pname)
        {
            case GL_DEBUG_LOGGED_MESSAGES:
            case GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH:
            case GL_DEBUG_GROUP_STACK_DEPTH:
            case GL_MAX_DEBUG_MESSAGE_LENGTH:
            case GL_MAX_DEBUG_LOGGED_MESSAGES:
            case GL_MAX_DEBUG_GROUP_STACK_DEPTH:
            case GL_MAX_LABEL_LENGTH:
                *type      = GL_INT;
                *numParams = 1;
                return true;

            case GL_DEBUG_OUTPUT_SYNCHRONOUS:
            case GL_DEBUG_OUTPUT:
                *type      = GL_BOOL;
                *numParams = 1;
                return true;
        }
    }

    // Check for ES3.0+ parameter names which are also exposed as ES2 extensions
    switch (pname)
    {
        case GL_PACK_ROW_LENGTH:
        case GL_PACK_SKIP_ROWS:
        case GL_PACK_SKIP_PIXELS:
            if ((mClientVersion < 3) && !mExtensions.packSubimage)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;
        case GL_UNPACK_ROW_LENGTH:
        case GL_UNPACK_SKIP_ROWS:
        case GL_UNPACK_SKIP_PIXELS:
            if ((mClientVersion < 3) && !mExtensions.unpackSubimage)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;
        case GL_VERTEX_ARRAY_BINDING:
            if ((mClientVersion < 3) && !mExtensions.vertexArrayObject)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;
        case GL_PIXEL_PACK_BUFFER_BINDING:
        case GL_PIXEL_UNPACK_BUFFER_BINDING:
            if ((mClientVersion < 3) && !mExtensions.pixelBufferObject)
            {
                return false;
            }
            *type      = GL_INT;
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
      case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
      case GL_COPY_READ_BUFFER_BINDING:
      case GL_COPY_WRITE_BUFFER_BINDING:
      case GL_SAMPLER_BINDING:
      case GL_READ_BUFFER:
      case GL_TEXTURE_BINDING_3D:
      case GL_TEXTURE_BINDING_2D_ARRAY:
      case GL_MAX_3D_TEXTURE_SIZE:
      case GL_MAX_ARRAY_TEXTURE_LAYERS:
      case GL_MAX_VERTEX_UNIFORM_BLOCKS:
      case GL_MAX_FRAGMENT_UNIFORM_BLOCKS:
      case GL_MAX_COMBINED_UNIFORM_BLOCKS:
      case GL_MAX_VERTEX_OUTPUT_COMPONENTS:
      case GL_MAX_FRAGMENT_INPUT_COMPONENTS:
      case GL_MAX_VARYING_COMPONENTS:
      case GL_MAX_VERTEX_UNIFORM_COMPONENTS:
      case GL_MAX_FRAGMENT_UNIFORM_COMPONENTS:
      case GL_MIN_PROGRAM_TEXEL_OFFSET:
      case GL_MAX_PROGRAM_TEXEL_OFFSET:
      case GL_NUM_EXTENSIONS:
      case GL_MAJOR_VERSION:
      case GL_MINOR_VERSION:
      case GL_MAX_ELEMENTS_INDICES:
      case GL_MAX_ELEMENTS_VERTICES:
      case GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS:
      case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS:
      case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS:
      case GL_UNPACK_IMAGE_HEIGHT:
      case GL_UNPACK_SKIP_IMAGES:
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
      case GL_PRIMITIVE_RESTART_FIXED_INDEX:
      case GL_RASTERIZER_DISCARD:
        {
            *type = GL_BOOL;
            *numParams = 1;
        }
        return true;

      case GL_MAX_TEXTURE_LOD_BIAS:
        {
            *type = GL_FLOAT;
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

Error Context::drawArrays(GLenum mode, GLint first, GLsizei count)
{
    syncRendererState();
    Error error = mRenderer->drawArrays(getData(), mode, first, count);
    if (error.isError())
    {
        return error;
    }

    MarkTransformFeedbackBufferUsage(mState.getCurrentTransformFeedback());

    return Error(GL_NO_ERROR);
}

Error Context::drawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instanceCount)
{
    syncRendererState();
    Error error = mRenderer->drawArraysInstanced(getData(), mode, first, count, instanceCount);
    if (error.isError())
    {
        return error;
    }

    MarkTransformFeedbackBufferUsage(mState.getCurrentTransformFeedback());

    return Error(GL_NO_ERROR);
}

Error Context::drawElements(GLenum mode,
                            GLsizei count,
                            GLenum type,
                            const GLvoid *indices,
                            const IndexRange &indexRange)
{
    syncRendererState();
    return mRenderer->drawElements(getData(), mode, count, type, indices, indexRange);
}

Error Context::drawElementsInstanced(GLenum mode,
                                     GLsizei count,
                                     GLenum type,
                                     const GLvoid *indices,
                                     GLsizei instances,
                                     const IndexRange &indexRange)
{
    syncRendererState();
    return mRenderer->drawElementsInstanced(getData(), mode, count, type, indices, instances,
                                            indexRange);
}

Error Context::drawRangeElements(GLenum mode,
                                 GLuint start,
                                 GLuint end,
                                 GLsizei count,
                                 GLenum type,
                                 const GLvoid *indices,
                                 const IndexRange &indexRange)
{
    syncRendererState();
    return mRenderer->drawRangeElements(getData(), mode, start, end, count, type, indices,
                                        indexRange);
}

Error Context::flush()
{
    return mRenderer->flush();
}

Error Context::finish()
{
    return mRenderer->finish();
}

void Context::insertEventMarker(GLsizei length, const char *marker)
{
    ASSERT(mRenderer);
    mRenderer->insertEventMarker(length, marker);
}

void Context::pushGroupMarker(GLsizei length, const char *marker)
{
    ASSERT(mRenderer);
    mRenderer->pushGroupMarker(length, marker);
}

void Context::popGroupMarker()
{
    ASSERT(mRenderer);
    mRenderer->popGroupMarker();
}

void Context::recordError(const Error &error)
{
    if (error.isError())
    {
        mErrors.insert(error.getCode());

        if (!error.getMessage().empty())
        {
            auto &debug = mState.getDebug();
            debug.insertMessage(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, error.getID(),
                                GL_DEBUG_SEVERITY_HIGH, error.getMessage());
        }
    }
}

// Get one of the recorded errors and clear its flag, if any.
// [OpenGL ES 2.0.24] section 2.5 page 13.
GLenum Context::getError()
{
    if (mErrors.empty())
    {
        return GL_NO_ERROR;
    }
    else
    {
        GLenum error = *mErrors.begin();
        mErrors.erase(mErrors.begin());
        return error;
    }
}

GLenum Context::getResetStatus()
{
    //TODO(jmadill): needs MANGLE reworking
    if (mResetStatus == GL_NO_ERROR && !mContextLost)
    {
        // mResetStatus will be set by the markContextLost callback
        // in the case a notification is sent
        if (mRenderer->testDeviceLost())
        {
            mRenderer->notifyDeviceLost();
        }
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

const egl::Config *Context::getConfig() const
{
    return mConfig;
}

EGLenum Context::getClientType() const
{
    return mClientType;
}

EGLenum Context::getRenderBuffer() const
{
    auto framebufferIt = mFramebufferMap.find(0);
    if (framebufferIt != mFramebufferMap.end())
    {
        const Framebuffer *framebuffer              = framebufferIt->second;
        const FramebufferAttachment *backAttachment = framebuffer->getAttachment(GL_BACK);

        ASSERT(backAttachment != nullptr);
        return backAttachment->getSurface()->getRenderBuffer();
    }
    else
    {
        return EGL_NONE;
    }
}

VertexArray *Context::checkVertexArrayAllocation(GLuint vertexArrayHandle)
{
    // Only called after a prior call to Gen.
    VertexArray *vertexArray = getVertexArray(vertexArrayHandle);
    if (!vertexArray)
    {
        vertexArray                        = new VertexArray(mRenderer, vertexArrayHandle, MAX_VERTEX_ATTRIBS);
        mVertexArrayMap[vertexArrayHandle] = vertexArray;
    }

    return vertexArray;
}

TransformFeedback *Context::checkTransformFeedbackAllocation(GLuint transformFeedbackHandle)
{
    // Only called after a prior call to Gen.
    TransformFeedback *transformFeedback = getTransformFeedback(transformFeedbackHandle);
    if (!transformFeedback)
    {
        transformFeedback = new TransformFeedback(mRenderer, transformFeedbackHandle, mCaps);
        transformFeedback->addRef();
        mTransformFeedbackMap[transformFeedbackHandle] = transformFeedback;
    }

    return transformFeedback;
}

Framebuffer *Context::checkFramebufferAllocation(GLuint framebuffer)
{
    // Can be called from Bind without a prior call to Gen.
    auto framebufferIt = mFramebufferMap.find(framebuffer);
    bool neverCreated = framebufferIt == mFramebufferMap.end();
    if (neverCreated || framebufferIt->second == nullptr)
    {
        Framebuffer *newFBO = new Framebuffer(mCaps, mRenderer, framebuffer);
        if (neverCreated)
        {
            mFramebufferHandleAllocator.reserve(framebuffer);
            mFramebufferMap[framebuffer] = newFBO;
            return newFBO;
        }

        framebufferIt->second = newFBO;
    }

    return framebufferIt->second;
}

bool Context::isVertexArrayGenerated(GLuint vertexArray)
{
    return mVertexArrayMap.find(vertexArray) != mVertexArrayMap.end();
}

bool Context::isTransformFeedbackGenerated(GLuint transformFeedback)
{
    return mTransformFeedbackMap.find(transformFeedback) != mTransformFeedbackMap.end();
}

void Context::detachTexture(GLuint texture)
{
    // Simple pass-through to State's detachTexture method, as textures do not require
    // allocation map management either here or in the resource manager at detach time.
    // Zero textures are held by the Context, and we don't attempt to request them from
    // the State.
    mState.detachTexture(mZeroTextures, texture);
}

void Context::detachBuffer(GLuint buffer)
{
    // Simple pass-through to State's detachBuffer method, since
    // only buffer attachments to container objects that are bound to the current context
    // should be detached. And all those are available in State.

    // [OpenGL ES 3.2] section 5.1.2 page 45:
    // Attachments to unbound container objects, such as
    // deletion of a buffer attached to a vertex array object which is not bound to the context,
    // are not affected and continue to act as references on the deleted object
    mState.detachBuffer(buffer);
}

void Context::detachFramebuffer(GLuint framebuffer)
{
    // Framebuffer detachment is handled by Context, because 0 is a valid
    // Framebuffer object, and a pointer to it must be passed from Context
    // to State at binding time.

    // [OpenGL ES 2.0.24] section 4.4 page 107:
    // If a framebuffer that is currently bound to the target FRAMEBUFFER is deleted, it is as though
    // BindFramebuffer had been executed with the target of FRAMEBUFFER and framebuffer of zero.

    if (mState.removeReadFramebufferBinding(framebuffer) && framebuffer != 0)
    {
        bindReadFramebuffer(0);
    }

    if (mState.removeDrawFramebufferBinding(framebuffer) && framebuffer != 0)
    {
        bindDrawFramebuffer(0);
    }
}

void Context::detachRenderbuffer(GLuint renderbuffer)
{
    mState.detachRenderbuffer(renderbuffer);
}

void Context::detachVertexArray(GLuint vertexArray)
{
    // Vertex array detachment is handled by Context, because 0 is a valid
    // VAO, and a pointer to it must be passed from Context to State at
    // binding time.

    // [OpenGL ES 3.0.2] section 2.10 page 43:
    // If a vertex array object that is currently bound is deleted, the binding
    // for that object reverts to zero and the default vertex array becomes current.
    if (mState.removeVertexArrayBinding(vertexArray))
    {
        bindVertexArray(0);
    }
}

void Context::detachTransformFeedback(GLuint transformFeedback)
{
    mState.detachTransformFeedback(transformFeedback);
}

void Context::detachSampler(GLuint sampler)
{
    mState.detachSampler(sampler);
}

void Context::setVertexAttribDivisor(GLuint index, GLuint divisor)
{
    mState.setVertexAttribDivisor(index, divisor);
}

void Context::samplerParameteri(GLuint sampler, GLenum pname, GLint param)
{
    mResourceManager->checkSamplerAllocation(sampler);

    Sampler *samplerObject = getSampler(sampler);
    ASSERT(samplerObject);

    // clang-format off
    switch (pname)
    {
      case GL_TEXTURE_MIN_FILTER:         samplerObject->setMinFilter(static_cast<GLenum>(param));    break;
      case GL_TEXTURE_MAG_FILTER:         samplerObject->setMagFilter(static_cast<GLenum>(param));    break;
      case GL_TEXTURE_WRAP_S:             samplerObject->setWrapS(static_cast<GLenum>(param));        break;
      case GL_TEXTURE_WRAP_T:             samplerObject->setWrapT(static_cast<GLenum>(param));        break;
      case GL_TEXTURE_WRAP_R:             samplerObject->setWrapR(static_cast<GLenum>(param));        break;
      case GL_TEXTURE_MAX_ANISOTROPY_EXT: samplerObject->setMaxAnisotropy(std::min(static_cast<GLfloat>(param), getExtensions().maxTextureAnisotropy)); break;
      case GL_TEXTURE_MIN_LOD:            samplerObject->setMinLod(static_cast<GLfloat>(param));      break;
      case GL_TEXTURE_MAX_LOD:            samplerObject->setMaxLod(static_cast<GLfloat>(param));      break;
      case GL_TEXTURE_COMPARE_MODE:       samplerObject->setCompareMode(static_cast<GLenum>(param));  break;
      case GL_TEXTURE_COMPARE_FUNC:       samplerObject->setCompareFunc(static_cast<GLenum>(param));  break;
      default:                            UNREACHABLE(); break;
    }
    // clang-format on
}

void Context::samplerParameterf(GLuint sampler, GLenum pname, GLfloat param)
{
    mResourceManager->checkSamplerAllocation(sampler);

    Sampler *samplerObject = getSampler(sampler);
    ASSERT(samplerObject);

    // clang-format off
    switch (pname)
    {
      case GL_TEXTURE_MIN_FILTER:         samplerObject->setMinFilter(uiround<GLenum>(param));   break;
      case GL_TEXTURE_MAG_FILTER:         samplerObject->setMagFilter(uiround<GLenum>(param));   break;
      case GL_TEXTURE_WRAP_S:             samplerObject->setWrapS(uiround<GLenum>(param));       break;
      case GL_TEXTURE_WRAP_T:             samplerObject->setWrapT(uiround<GLenum>(param));       break;
      case GL_TEXTURE_WRAP_R:             samplerObject->setWrapR(uiround<GLenum>(param));       break;
      case GL_TEXTURE_MAX_ANISOTROPY_EXT: samplerObject->setMaxAnisotropy(std::min(param, getExtensions().maxTextureAnisotropy)); break;
      case GL_TEXTURE_MIN_LOD:            samplerObject->setMinLod(param);                       break;
      case GL_TEXTURE_MAX_LOD:            samplerObject->setMaxLod(param);                       break;
      case GL_TEXTURE_COMPARE_MODE:       samplerObject->setCompareMode(uiround<GLenum>(param)); break;
      case GL_TEXTURE_COMPARE_FUNC:       samplerObject->setCompareFunc(uiround<GLenum>(param)); break;
      default:                            UNREACHABLE(); break;
    }
    // clang-format on
}

GLint Context::getSamplerParameteri(GLuint sampler, GLenum pname)
{
    mResourceManager->checkSamplerAllocation(sampler);

    Sampler *samplerObject = getSampler(sampler);
    ASSERT(samplerObject);

    // clang-format off
    switch (pname)
    {
      case GL_TEXTURE_MIN_FILTER:         return static_cast<GLint>(samplerObject->getMinFilter());
      case GL_TEXTURE_MAG_FILTER:         return static_cast<GLint>(samplerObject->getMagFilter());
      case GL_TEXTURE_WRAP_S:             return static_cast<GLint>(samplerObject->getWrapS());
      case GL_TEXTURE_WRAP_T:             return static_cast<GLint>(samplerObject->getWrapT());
      case GL_TEXTURE_WRAP_R:             return static_cast<GLint>(samplerObject->getWrapR());
      case GL_TEXTURE_MAX_ANISOTROPY_EXT: return static_cast<GLint>(samplerObject->getMaxAnisotropy());
      case GL_TEXTURE_MIN_LOD:            return iround<GLint>(samplerObject->getMinLod());
      case GL_TEXTURE_MAX_LOD:            return iround<GLint>(samplerObject->getMaxLod());
      case GL_TEXTURE_COMPARE_MODE:       return static_cast<GLint>(samplerObject->getCompareMode());
      case GL_TEXTURE_COMPARE_FUNC:       return static_cast<GLint>(samplerObject->getCompareFunc());
      default:                            UNREACHABLE(); return 0;
    }
    // clang-format on
}

GLfloat Context::getSamplerParameterf(GLuint sampler, GLenum pname)
{
    mResourceManager->checkSamplerAllocation(sampler);

    Sampler *samplerObject = getSampler(sampler);
    ASSERT(samplerObject);

    // clang-format off
    switch (pname)
    {
      case GL_TEXTURE_MIN_FILTER:         return static_cast<GLfloat>(samplerObject->getMinFilter());
      case GL_TEXTURE_MAG_FILTER:         return static_cast<GLfloat>(samplerObject->getMagFilter());
      case GL_TEXTURE_WRAP_S:             return static_cast<GLfloat>(samplerObject->getWrapS());
      case GL_TEXTURE_WRAP_T:             return static_cast<GLfloat>(samplerObject->getWrapT());
      case GL_TEXTURE_WRAP_R:             return static_cast<GLfloat>(samplerObject->getWrapR());
      case GL_TEXTURE_MAX_ANISOTROPY_EXT: return samplerObject->getMaxAnisotropy();
      case GL_TEXTURE_MIN_LOD:            return samplerObject->getMinLod();
      case GL_TEXTURE_MAX_LOD:            return samplerObject->getMaxLod();
      case GL_TEXTURE_COMPARE_MODE:       return static_cast<GLfloat>(samplerObject->getCompareMode());
      case GL_TEXTURE_COMPARE_FUNC:       return static_cast<GLfloat>(samplerObject->getCompareFunc());
      default:                            UNREACHABLE(); return 0;
    }
    // clang-format on
}

void Context::programParameteri(GLuint program, GLenum pname, GLint value)
{
    gl::Program *programObject = getProgram(program);
    ASSERT(programObject != nullptr);

    ASSERT(pname == GL_PROGRAM_BINARY_RETRIEVABLE_HINT);
    programObject->setBinaryRetrievableHint(value != GL_FALSE);
}

void Context::initRendererString()
{
    std::ostringstream rendererString;
    rendererString << "ANGLE (";
    rendererString << mRenderer->getRendererDescription();
    rendererString << ")";

    mRendererString = MakeStaticString(rendererString.str());
}

const std::string &Context::getRendererString() const
{
    return mRendererString;
}

void Context::initExtensionStrings()
{
    mExtensionStrings = mExtensions.getStrings();

    std::ostringstream combinedStringStream;
    std::copy(mExtensionStrings.begin(), mExtensionStrings.end(), std::ostream_iterator<std::string>(combinedStringStream, " "));
    mExtensionString = combinedStringStream.str();
}

const std::string &Context::getExtensionString() const
{
    return mExtensionString;
}

const std::string &Context::getExtensionString(size_t idx) const
{
    return mExtensionStrings[idx];
}

size_t Context::getExtensionStringCount() const
{
    return mExtensionStrings.size();
}

void Context::beginTransformFeedback(GLenum primitiveMode)
{
    TransformFeedback *transformFeedback = getState().getCurrentTransformFeedback();
    ASSERT(transformFeedback != nullptr);
    ASSERT(!transformFeedback->isPaused());

    transformFeedback->begin(primitiveMode, getState().getProgram());
}

bool Context::hasActiveTransformFeedback(GLuint program) const
{
    for (auto pair : mTransformFeedbackMap)
    {
        if (pair.second != nullptr && pair.second->hasBoundProgram(program))
        {
            return true;
        }
    }
    return false;
}

void Context::initCaps(GLuint clientVersion)
{
    mCaps = mRenderer->getRendererCaps();

    mExtensions = mRenderer->getRendererExtensions();

    mLimitations = mRenderer->getRendererLimitations();

    if (clientVersion < 3)
    {
        // Disable ES3+ extensions
        mExtensions.colorBufferFloat = false;
    }

    if (clientVersion > 2)
    {
        // FIXME(geofflang): Don't support EXT_sRGB in non-ES2 contexts
        //mExtensions.sRGB = false;
    }

    // Explicitly enable GL_KHR_debug
    mExtensions.debug                   = true;
    mExtensions.maxDebugMessageLength   = 1024;
    mExtensions.maxDebugLoggedMessages  = 1024;
    mExtensions.maxDebugGroupStackDepth = 1024;
    mExtensions.maxLabelLength          = 1024;

    // Apply implementation limits
    mCaps.maxVertexAttributes = std::min<GLuint>(mCaps.maxVertexAttributes, MAX_VERTEX_ATTRIBS);
    mCaps.maxVertexUniformBlocks = std::min<GLuint>(mCaps.maxVertexUniformBlocks, IMPLEMENTATION_MAX_VERTEX_SHADER_UNIFORM_BUFFERS);
    mCaps.maxVertexOutputComponents = std::min<GLuint>(mCaps.maxVertexOutputComponents, IMPLEMENTATION_MAX_VARYING_VECTORS * 4);

    mCaps.maxFragmentInputComponents = std::min<GLuint>(mCaps.maxFragmentInputComponents, IMPLEMENTATION_MAX_VARYING_VECTORS * 4);

    mCaps.compressedTextureFormats.clear();

    const TextureCapsMap &rendererFormats = mRenderer->getRendererTextureCaps();
    for (TextureCapsMap::const_iterator i = rendererFormats.begin(); i != rendererFormats.end(); i++)
    {
        GLenum format = i->first;
        TextureCaps formatCaps = i->second;

        const InternalFormat &formatInfo = GetInternalFormatInfo(format);

        // Update the format caps based on the client version and extensions.
        // Caps are AND'd with the renderer caps because some core formats are still unsupported in
        // ES3.
        formatCaps.texturable =
            formatCaps.texturable && formatInfo.textureSupport(clientVersion, mExtensions);
        formatCaps.renderable =
            formatCaps.renderable && formatInfo.renderSupport(clientVersion, mExtensions);
        formatCaps.filterable =
            formatCaps.filterable && formatInfo.filterSupport(clientVersion, mExtensions);

        // OpenGL ES does not support multisampling with integer formats
        if (!formatInfo.renderSupport || formatInfo.componentType == GL_INT || formatInfo.componentType == GL_UNSIGNED_INT)
        {
            formatCaps.sampleCounts.clear();
        }

        if (formatCaps.texturable && formatInfo.compressed)
        {
            mCaps.compressedTextureFormats.push_back(format);
        }

        mTextureCaps.insert(format, formatCaps);
    }
}

void Context::syncRendererState()
{
    const State::DirtyBits &dirtyBits = mState.getDirtyBits();
    mRenderer->syncState(mState, dirtyBits);
    mState.clearDirtyBits();
    mState.syncDirtyObjects();
}

void Context::syncRendererState(const State::DirtyBits &bitMask,
                                const State::DirtyObjects &objectMask)
{
    const State::DirtyBits &dirtyBits = (mState.getDirtyBits() & bitMask);
    mRenderer->syncState(mState, dirtyBits);
    mState.clearDirtyBits(dirtyBits);

    mState.syncDirtyObjects(objectMask);
}

void Context::blitFramebuffer(GLint srcX0,
                              GLint srcY0,
                              GLint srcX1,
                              GLint srcY1,
                              GLint dstX0,
                              GLint dstY0,
                              GLint dstX1,
                              GLint dstY1,
                              GLbitfield mask,
                              GLenum filter)
{
    Framebuffer *readFramebuffer = mState.getReadFramebuffer();
    ASSERT(readFramebuffer);

    Framebuffer *drawFramebuffer = mState.getDrawFramebuffer();
    ASSERT(drawFramebuffer);

    Rectangle srcArea(srcX0, srcY0, srcX1 - srcX0, srcY1 - srcY0);
    Rectangle dstArea(dstX0, dstY0, dstX1 - dstX0, dstY1 - dstY0);

    syncStateForBlit();

    Error error = drawFramebuffer->blit(mState, srcArea, dstArea, mask, filter, readFramebuffer);
    if (error.isError())
    {
        recordError(error);
        return;
    }
}

void Context::clear(GLbitfield mask)
{
    syncStateForClear();

    Error error = mState.getDrawFramebuffer()->clear(mData, mask);
    if (error.isError())
    {
        recordError(error);
    }
}

void Context::clearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *values)
{
    syncStateForClear();

    Error error = mState.getDrawFramebuffer()->clearBufferfv(mData, buffer, drawbuffer, values);
    if (error.isError())
    {
        recordError(error);
    }
}

void Context::clearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *values)
{
    syncStateForClear();

    Error error = mState.getDrawFramebuffer()->clearBufferuiv(mData, buffer, drawbuffer, values);
    if (error.isError())
    {
        recordError(error);
    }
}

void Context::clearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *values)
{
    syncStateForClear();

    Error error = mState.getDrawFramebuffer()->clearBufferiv(mData, buffer, drawbuffer, values);
    if (error.isError())
    {
        recordError(error);
    }
}

void Context::clearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
    Framebuffer *framebufferObject = mState.getDrawFramebuffer();
    ASSERT(framebufferObject);

    // If a buffer is not present, the clear has no effect
    if (framebufferObject->getDepthbuffer() == nullptr &&
        framebufferObject->getStencilbuffer() == nullptr)
    {
        return;
    }

    syncStateForClear();

    Error error = framebufferObject->clearBufferfi(mData, buffer, drawbuffer, depth, stencil);
    if (error.isError())
    {
        recordError(error);
    }
}

void Context::readPixels(GLint x,
                         GLint y,
                         GLsizei width,
                         GLsizei height,
                         GLenum format,
                         GLenum type,
                         GLvoid *pixels)
{
    syncStateForReadPixels();

    Framebuffer *framebufferObject = mState.getReadFramebuffer();
    ASSERT(framebufferObject);

    Rectangle area(x, y, width, height);
    Error error = framebufferObject->readPixels(mState, area, format, type, pixels);
    if (error.isError())
    {
        recordError(error);
    }
}

void Context::copyTexImage2D(GLenum target,
                             GLint level,
                             GLenum internalformat,
                             GLint x,
                             GLint y,
                             GLsizei width,
                             GLsizei height,
                             GLint border)
{
    // Only sync the read FBO
    mState.syncDirtyObject(GL_READ_FRAMEBUFFER);

    Rectangle sourceArea(x, y, width, height);

    const Framebuffer *framebuffer = mState.getReadFramebuffer();
    Texture *texture =
        getTargetTexture(IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target);
    Error error = texture->copyImage(target, level, sourceArea, internalformat, framebuffer);
    if (error.isError())
    {
        recordError(error);
    }
}

void Context::copyTexSubImage2D(GLenum target,
                                GLint level,
                                GLint xoffset,
                                GLint yoffset,
                                GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height)
{
    // Only sync the read FBO
    mState.syncDirtyObject(GL_READ_FRAMEBUFFER);

    Offset destOffset(xoffset, yoffset, 0);
    Rectangle sourceArea(x, y, width, height);

    const Framebuffer *framebuffer = mState.getReadFramebuffer();
    Texture *texture =
        getTargetTexture(IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target);
    Error error = texture->copySubImage(target, level, destOffset, sourceArea, framebuffer);
    if (error.isError())
    {
        recordError(error);
    }
}

void Context::copyTexSubImage3D(GLenum target,
                                GLint level,
                                GLint xoffset,
                                GLint yoffset,
                                GLint zoffset,
                                GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height)
{
    // Only sync the read FBO
    mState.syncDirtyObject(GL_READ_FRAMEBUFFER);

    Offset destOffset(xoffset, yoffset, zoffset);
    Rectangle sourceArea(x, y, width, height);

    const Framebuffer *framebuffer = mState.getReadFramebuffer();
    Texture *texture               = getTargetTexture(target);
    Error error = texture->copySubImage(target, level, destOffset, sourceArea, framebuffer);
    if (error.isError())
    {
        recordError(error);
    }
}

void Context::framebufferTexture2D(GLenum target,
                                   GLenum attachment,
                                   GLenum textarget,
                                   GLuint texture,
                                   GLint level)
{
    Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (texture != 0)
    {
        Texture *textureObj = getTexture(texture);

        ImageIndex index = ImageIndex::MakeInvalid();

        if (textarget == GL_TEXTURE_2D)
        {
            index = ImageIndex::Make2D(level);
        }
        else
        {
            ASSERT(IsCubeMapTextureTarget(textarget));
            index = ImageIndex::MakeCube(textarget, level);
        }

        framebuffer->setAttachment(GL_TEXTURE, attachment, index, textureObj);
    }
    else
    {
        framebuffer->resetAttachment(attachment);
    }

    mState.setObjectDirty(target);
}

void Context::framebufferRenderbuffer(GLenum target,
                                      GLenum attachment,
                                      GLenum renderbuffertarget,
                                      GLuint renderbuffer)
{
    Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (renderbuffer != 0)
    {
        Renderbuffer *renderbufferObject = getRenderbuffer(renderbuffer);
        framebuffer->setAttachment(GL_RENDERBUFFER, attachment, gl::ImageIndex::MakeInvalid(),
                                   renderbufferObject);
    }
    else
    {
        framebuffer->resetAttachment(attachment);
    }

    mState.setObjectDirty(target);
}

void Context::framebufferTextureLayer(GLenum target,
                                      GLenum attachment,
                                      GLuint texture,
                                      GLint level,
                                      GLint layer)
{
    Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (texture != 0)
    {
        Texture *textureObject = getTexture(texture);

        ImageIndex index = ImageIndex::MakeInvalid();

        if (textureObject->getTarget() == GL_TEXTURE_3D)
        {
            index = ImageIndex::Make3D(level, layer);
        }
        else
        {
            ASSERT(textureObject->getTarget() == GL_TEXTURE_2D_ARRAY);
            index = ImageIndex::Make2DArray(level, layer);
        }

        framebuffer->setAttachment(GL_TEXTURE, attachment, index, textureObject);
    }
    else
    {
        framebuffer->resetAttachment(attachment);
    }

    mState.setObjectDirty(target);
}

void Context::drawBuffers(GLsizei n, const GLenum *bufs)
{
    Framebuffer *framebuffer = mState.getDrawFramebuffer();
    ASSERT(framebuffer);
    framebuffer->setDrawBuffers(n, bufs);
    mState.setObjectDirty(GL_DRAW_FRAMEBUFFER);
}

void Context::readBuffer(GLenum mode)
{
    Framebuffer *readFBO = mState.getReadFramebuffer();
    readFBO->setReadBuffer(mode);
    mState.setObjectDirty(GL_READ_FRAMEBUFFER);
}

void Context::discardFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments)
{
    // Only sync the FBO
    mState.syncDirtyObject(target);

    Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    // The specification isn't clear what should be done when the framebuffer isn't complete.
    // We leave it up to the framebuffer implementation to decide what to do.
    Error error = framebuffer->discard(numAttachments, attachments);
    if (error.isError())
    {
        recordError(error);
    }
}

void Context::invalidateFramebuffer(GLenum target,
                                    GLsizei numAttachments,
                                    const GLenum *attachments)
{
    // Only sync the FBO
    mState.syncDirtyObject(target);

    Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (framebuffer->checkStatus(mData) == GL_FRAMEBUFFER_COMPLETE)
    {
        Error error = framebuffer->invalidate(numAttachments, attachments);
        if (error.isError())
        {
            recordError(error);
            return;
        }
    }
}

void Context::invalidateSubFramebuffer(GLenum target,
                                       GLsizei numAttachments,
                                       const GLenum *attachments,
                                       GLint x,
                                       GLint y,
                                       GLsizei width,
                                       GLsizei height)
{
    // Only sync the FBO
    mState.syncDirtyObject(target);

    Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (framebuffer->checkStatus(mData) == GL_FRAMEBUFFER_COMPLETE)
    {
        Rectangle area(x, y, width, height);
        Error error = framebuffer->invalidateSub(numAttachments, attachments, area);
        if (error.isError())
        {
            recordError(error);
            return;
        }
    }
}

void Context::texImage2D(GLenum target,
                         GLint level,
                         GLint internalformat,
                         GLsizei width,
                         GLsizei height,
                         GLint border,
                         GLenum format,
                         GLenum type,
                         const GLvoid *pixels)
{
    syncStateForTexImage();

    Extents size(width, height, 1);
    Texture *texture =
        getTargetTexture(IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target);
    Error error = texture->setImage(mState.getUnpackState(), target, level, internalformat, size,
                                    format, type, reinterpret_cast<const uint8_t *>(pixels));
    if (error.isError())
    {
        recordError(error);
    }
}

void Context::texImage3D(GLenum target,
                         GLint level,
                         GLint internalformat,
                         GLsizei width,
                         GLsizei height,
                         GLsizei depth,
                         GLint border,
                         GLenum format,
                         GLenum type,
                         const GLvoid *pixels)
{
    syncStateForTexImage();

    Extents size(width, height, depth);
    Texture *texture = getTargetTexture(target);
    Error error = texture->setImage(mState.getUnpackState(), target, level, internalformat, size,
                                    format, type, reinterpret_cast<const uint8_t *>(pixels));
    if (error.isError())
    {
        recordError(error);
    }
}

void Context::texSubImage2D(GLenum target,
                            GLint level,
                            GLint xoffset,
                            GLint yoffset,
                            GLsizei width,
                            GLsizei height,
                            GLenum format,
                            GLenum type,
                            const GLvoid *pixels)
{
    // Zero sized uploads are valid but no-ops
    if (width == 0 || height == 0)
    {
        return;
    }

    syncStateForTexImage();

    Box area(xoffset, yoffset, 0, width, height, 1);
    Texture *texture =
        getTargetTexture(IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target);
    Error error = texture->setSubImage(mState.getUnpackState(), target, level, area, format, type,
                                       reinterpret_cast<const uint8_t *>(pixels));
    if (error.isError())
    {
        recordError(error);
    }
}

void Context::texSubImage3D(GLenum target,
                            GLint level,
                            GLint xoffset,
                            GLint yoffset,
                            GLint zoffset,
                            GLsizei width,
                            GLsizei height,
                            GLsizei depth,
                            GLenum format,
                            GLenum type,
                            const GLvoid *pixels)
{
    // Zero sized uploads are valid but no-ops
    if (width == 0 || height == 0 || depth == 0)
    {
        return;
    }

    syncStateForTexImage();

    Box area(xoffset, yoffset, zoffset, width, height, depth);
    Texture *texture = getTargetTexture(target);
    Error error = texture->setSubImage(mState.getUnpackState(), target, level, area, format, type,
                                       reinterpret_cast<const uint8_t *>(pixels));
    if (error.isError())
    {
        recordError(error);
    }
}

void Context::compressedTexImage2D(GLenum target,
                                   GLint level,
                                   GLenum internalformat,
                                   GLsizei width,
                                   GLsizei height,
                                   GLint border,
                                   GLsizei imageSize,
                                   const GLvoid *data)
{
    syncStateForTexImage();

    Extents size(width, height, 1);
    Texture *texture =
        getTargetTexture(IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target);
    Error error =
        texture->setCompressedImage(mState.getUnpackState(), target, level, internalformat, size,
                                    imageSize, reinterpret_cast<const uint8_t *>(data));
    if (error.isError())
    {
        recordError(error);
    }
}

void Context::compressedTexImage3D(GLenum target,
                                   GLint level,
                                   GLenum internalformat,
                                   GLsizei width,
                                   GLsizei height,
                                   GLsizei depth,
                                   GLint border,
                                   GLsizei imageSize,
                                   const GLvoid *data)
{
    syncStateForTexImage();

    Extents size(width, height, depth);
    Texture *texture = getTargetTexture(target);
    Error error =
        texture->setCompressedImage(mState.getUnpackState(), target, level, internalformat, size,
                                    imageSize, reinterpret_cast<const uint8_t *>(data));
    if (error.isError())
    {
        recordError(error);
    }
}

void Context::compressedTexSubImage2D(GLenum target,
                                      GLint level,
                                      GLint xoffset,
                                      GLint yoffset,
                                      GLsizei width,
                                      GLsizei height,
                                      GLenum format,
                                      GLsizei imageSize,
                                      const GLvoid *data)
{
    syncStateForTexImage();

    Box area(xoffset, yoffset, 0, width, height, 1);
    Texture *texture =
        getTargetTexture(IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target);
    Error error =
        texture->setCompressedSubImage(mState.getUnpackState(), target, level, area, format,
                                       imageSize, reinterpret_cast<const uint8_t *>(data));
    if (error.isError())
    {
        recordError(error);
    }
}

void Context::compressedTexSubImage3D(GLenum target,
                                      GLint level,
                                      GLint xoffset,
                                      GLint yoffset,
                                      GLint zoffset,
                                      GLsizei width,
                                      GLsizei height,
                                      GLsizei depth,
                                      GLenum format,
                                      GLsizei imageSize,
                                      const GLvoid *data)
{
    // Zero sized uploads are valid but no-ops
    if (width == 0 || height == 0)
    {
        return;
    }

    syncStateForTexImage();

    Box area(xoffset, yoffset, zoffset, width, height, depth);
    Texture *texture = getTargetTexture(target);
    Error error =
        texture->setCompressedSubImage(mState.getUnpackState(), target, level, area, format,
                                       imageSize, reinterpret_cast<const uint8_t *>(data));
    if (error.isError())
    {
        recordError(error);
    }
}

void Context::getBufferPointerv(GLenum target, GLenum /*pname*/, void **params)
{
    Buffer *buffer = getState().getTargetBuffer(target);
    ASSERT(buffer);

    if (!buffer->isMapped())
    {
        *params = nullptr;
    }
    else
    {
        *params = buffer->getMapPointer();
    }
}

GLvoid *Context::mapBuffer(GLenum target, GLenum access)
{
    Buffer *buffer = getState().getTargetBuffer(target);
    ASSERT(buffer);

    Error error = buffer->map(access);
    if (error.isError())
    {
        recordError(error);
        return nullptr;
    }

    return buffer->getMapPointer();
}

GLboolean Context::unmapBuffer(GLenum target)
{
    Buffer *buffer = getState().getTargetBuffer(target);
    ASSERT(buffer);

    GLboolean result;
    Error error = buffer->unmap(&result);
    if (error.isError())
    {
        recordError(error);
        return GL_FALSE;
    }

    return result;
}

GLvoid *Context::mapBufferRange(GLenum target,
                                GLintptr offset,
                                GLsizeiptr length,
                                GLbitfield access)
{
    Buffer *buffer = getState().getTargetBuffer(target);
    ASSERT(buffer);

    Error error = buffer->mapRange(offset, length, access);
    if (error.isError())
    {
        recordError(error);
        return nullptr;
    }

    return buffer->getMapPointer();
}

void Context::flushMappedBufferRange(GLenum /*target*/, GLintptr /*offset*/, GLsizeiptr /*length*/)
{
    // We do not currently support a non-trivial implementation of FlushMappedBufferRange
}

void Context::syncStateForReadPixels()
{
    syncRendererState(mReadPixelsDirtyBits, mReadPixelsDirtyObjects);
}

void Context::syncStateForTexImage()
{
    syncRendererState(mTexImageDirtyBits, mTexImageDirtyObjects);
}

void Context::syncStateForClear()
{
    syncRendererState(mClearDirtyBits, mClearDirtyObjects);
}

void Context::syncStateForBlit()
{
    syncRendererState(mBlitDirtyBits, mBlitDirtyObjects);
}

}  // namespace gl
