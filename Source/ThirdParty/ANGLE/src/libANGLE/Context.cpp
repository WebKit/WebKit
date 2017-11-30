//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Context.cpp: Implements the gl::Context class, managing all GL state and performing
// rendering operations. It is the GLES2 specific implementation of EGLContext.

#include "libANGLE/Context.h"

#include <string.h>
#include <iterator>
#include <sstream>
#include <vector>

#include "common/matrix_utils.h"
#include "common/platform.h"
#include "common/utilities.h"
#include "common/version.h"
#include "libANGLE/Buffer.h"
#include "libANGLE/Compiler.h"
#include "libANGLE/Display.h"
#include "libANGLE/Fence.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/Path.h"
#include "libANGLE/Program.h"
#include "libANGLE/ProgramPipeline.h"
#include "libANGLE/Query.h"
#include "libANGLE/Renderbuffer.h"
#include "libANGLE/ResourceManager.h"
#include "libANGLE/Sampler.h"
#include "libANGLE/Surface.h"
#include "libANGLE/Texture.h"
#include "libANGLE/TransformFeedback.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/Workarounds.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/queryconversions.h"
#include "libANGLE/queryutils.h"
#include "libANGLE/renderer/ContextImpl.h"
#include "libANGLE/renderer/EGLImplFactory.h"
#include "libANGLE/renderer/Format.h"
#include "libANGLE/validationES.h"

namespace
{

#define ANGLE_HANDLE_ERR(X) \
    handleError(X);         \
    return;
#define ANGLE_CONTEXT_TRY(EXPR) ANGLE_TRY_TEMPLATE(EXPR, ANGLE_HANDLE_ERR);

template <typename T>
std::vector<gl::Path *> GatherPaths(gl::PathManager &resourceManager,
                                    GLsizei numPaths,
                                    const void *paths,
                                    GLuint pathBase)
{
    std::vector<gl::Path *> ret;
    ret.reserve(numPaths);

    const auto *nameArray = static_cast<const T *>(paths);

    for (GLsizei i = 0; i < numPaths; ++i)
    {
        const GLuint pathName = nameArray[i] + pathBase;

        ret.push_back(resourceManager.getPath(pathName));
    }

    return ret;
}

std::vector<gl::Path *> GatherPaths(gl::PathManager &resourceManager,
                                    GLsizei numPaths,
                                    GLenum pathNameType,
                                    const void *paths,
                                    GLuint pathBase)
{
    switch (pathNameType)
    {
        case GL_UNSIGNED_BYTE:
            return GatherPaths<GLubyte>(resourceManager, numPaths, paths, pathBase);

        case GL_BYTE:
            return GatherPaths<GLbyte>(resourceManager, numPaths, paths, pathBase);

        case GL_UNSIGNED_SHORT:
            return GatherPaths<GLushort>(resourceManager, numPaths, paths, pathBase);

        case GL_SHORT:
            return GatherPaths<GLshort>(resourceManager, numPaths, paths, pathBase);

        case GL_UNSIGNED_INT:
            return GatherPaths<GLuint>(resourceManager, numPaths, paths, pathBase);

        case GL_INT:
            return GatherPaths<GLint>(resourceManager, numPaths, paths, pathBase);
    }

    UNREACHABLE();
    return std::vector<gl::Path *>();
}

template <typename T>
gl::Error GetQueryObjectParameter(gl::Query *query, GLenum pname, T *params)
{
    ASSERT(query != nullptr);

    switch (pname)
    {
        case GL_QUERY_RESULT_EXT:
            return query->getResult(params);
        case GL_QUERY_RESULT_AVAILABLE_EXT:
        {
            bool available;
            gl::Error error = query->isResultAvailable(&available);
            if (!error.isError())
            {
                *params = gl::CastFromStateValue<T>(pname, static_cast<GLuint>(available));
            }
            return error;
        }
        default:
            UNREACHABLE();
            return gl::InternalError() << "Unreachable Error";
    }
}

void MarkTransformFeedbackBufferUsage(gl::TransformFeedback *transformFeedback)
{
    if (transformFeedback && transformFeedback->isActive() && !transformFeedback->isPaused())
    {
        for (size_t tfBufferIndex = 0; tfBufferIndex < transformFeedback->getIndexedBufferCount();
             tfBufferIndex++)
        {
            const gl::OffsetBindingPointer<gl::Buffer> &buffer =
                transformFeedback->getIndexedBuffer(tfBufferIndex);
            if (buffer.get() != nullptr)
            {
                buffer->onTransformFeedback();
            }
        }
    }
}

// Attribute map queries.
EGLint GetClientMajorVersion(const egl::AttributeMap &attribs)
{
    return static_cast<EGLint>(attribs.get(EGL_CONTEXT_CLIENT_VERSION, 1));
}

EGLint GetClientMinorVersion(const egl::AttributeMap &attribs)
{
    return static_cast<EGLint>(attribs.get(EGL_CONTEXT_MINOR_VERSION, 0));
}

gl::Version GetClientVersion(const egl::AttributeMap &attribs)
{
    return gl::Version(GetClientMajorVersion(attribs), GetClientMinorVersion(attribs));
}

GLenum GetResetStrategy(const egl::AttributeMap &attribs)
{
    EGLAttrib attrib = attribs.get(EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT,
                                   EGL_NO_RESET_NOTIFICATION);
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
    return (attribs.get(EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT, EGL_FALSE) == EGL_TRUE) ||
           ((attribs.get(EGL_CONTEXT_FLAGS_KHR, 0) & EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR) !=
            0);
}

bool GetDebug(const egl::AttributeMap &attribs)
{
    return (attribs.get(EGL_CONTEXT_OPENGL_DEBUG, EGL_FALSE) == EGL_TRUE) ||
           ((attribs.get(EGL_CONTEXT_FLAGS_KHR, 0) & EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR) != 0);
}

bool GetNoError(const egl::AttributeMap &attribs)
{
    return (attribs.get(EGL_CONTEXT_OPENGL_NO_ERROR_KHR, EGL_FALSE) == EGL_TRUE);
}

bool GetWebGLContext(const egl::AttributeMap &attribs)
{
    return (attribs.get(EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE, EGL_FALSE) == EGL_TRUE);
}

bool GetBindGeneratesResource(const egl::AttributeMap &attribs)
{
    return (attribs.get(EGL_CONTEXT_BIND_GENERATES_RESOURCE_CHROMIUM, EGL_TRUE) == EGL_TRUE);
}

bool GetClientArraysEnabled(const egl::AttributeMap &attribs)
{
    return (attribs.get(EGL_CONTEXT_CLIENT_ARRAYS_ENABLED_ANGLE, EGL_TRUE) == EGL_TRUE);
}

bool GetRobustResourceInit(const egl::AttributeMap &attribs)
{
    return (attribs.get(EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE, EGL_FALSE) == EGL_TRUE);
}

std::string GetObjectLabelFromPointer(GLsizei length, const GLchar *label)
{
    std::string labelName;
    if (label != nullptr)
    {
        size_t labelLength = length < 0 ? strlen(label) : length;
        labelName          = std::string(label, labelLength);
    }
    return labelName;
}

void GetObjectLabelBase(const std::string &objectLabel,
                        GLsizei bufSize,
                        GLsizei *length,
                        GLchar *label)
{
    size_t writeLength = objectLabel.length();
    if (label != nullptr && bufSize > 0)
    {
        writeLength = std::min(static_cast<size_t>(bufSize) - 1, objectLabel.length());
        std::copy(objectLabel.begin(), objectLabel.begin() + writeLength, label);
        label[writeLength] = '\0';
    }

    if (length != nullptr)
    {
        *length = static_cast<GLsizei>(writeLength);
    }
}

template <typename CapT, typename MaxT>
void LimitCap(CapT *cap, MaxT maximum)
{
    *cap = std::min(*cap, static_cast<CapT>(maximum));
}

}  // anonymous namespace

namespace gl
{

Context::Context(rx::EGLImplFactory *implFactory,
                 const egl::Config *config,
                 const Context *shareContext,
                 TextureManager *shareTextures,
                 MemoryProgramCache *memoryProgramCache,
                 const egl::AttributeMap &attribs,
                 const egl::DisplayExtensions &displayExtensions)

    : ValidationContext(shareContext,
                        shareTextures,
                        GetClientVersion(attribs),
                        &mGLState,
                        mCaps,
                        mTextureCaps,
                        mExtensions,
                        mLimitations,
                        GetNoError(attribs)),
      mImplementation(implFactory->createContext(mState)),
      mCompiler(),
      mConfig(config),
      mClientType(EGL_OPENGL_ES_API),
      mHasBeenCurrent(false),
      mContextLost(false),
      mResetStatus(GL_NO_ERROR),
      mContextLostForced(false),
      mResetStrategy(GetResetStrategy(attribs)),
      mRobustAccess(GetRobustAccess(attribs)),
      mCurrentSurface(static_cast<egl::Surface *>(EGL_NO_SURFACE)),
      mCurrentDisplay(static_cast<egl::Display *>(EGL_NO_DISPLAY)),
      mSurfacelessFramebuffer(nullptr),
      mWebGLContext(GetWebGLContext(attribs)),
      mMemoryProgramCache(memoryProgramCache),
      mScratchBuffer(1000u),
      mZeroFilledBuffer(1000u)
{
    mImplementation->setMemoryProgramCache(memoryProgramCache);

    bool robustResourceInit = GetRobustResourceInit(attribs);
    initCaps(displayExtensions, robustResourceInit);
    initWorkarounds();

    mGLState.initialize(this, GetDebug(attribs), GetBindGeneratesResource(attribs),
                        GetClientArraysEnabled(attribs), robustResourceInit,
                        mMemoryProgramCache != nullptr);

    mFenceNVHandleAllocator.setBaseHandle(0);

    // [OpenGL ES 2.0.24] section 3.7 page 83:
    // In the initial state, TEXTURE_2D and TEXTURE_CUBE_MAP have two-dimensional
    // and cube map texture state vectors respectively associated with them.
    // In order that access to these initial textures not be lost, they are treated as texture
    // objects all of whose names are 0.

    Texture *zeroTexture2D = new Texture(mImplementation.get(), 0, GL_TEXTURE_2D);
    mZeroTextures[GL_TEXTURE_2D].set(this, zeroTexture2D);

    Texture *zeroTextureCube = new Texture(mImplementation.get(), 0, GL_TEXTURE_CUBE_MAP);
    mZeroTextures[GL_TEXTURE_CUBE_MAP].set(this, zeroTextureCube);

    if (getClientVersion() >= Version(3, 0))
    {
        // TODO: These could also be enabled via extension
        Texture *zeroTexture3D = new Texture(mImplementation.get(), 0, GL_TEXTURE_3D);
        mZeroTextures[GL_TEXTURE_3D].set(this, zeroTexture3D);

        Texture *zeroTexture2DArray = new Texture(mImplementation.get(), 0, GL_TEXTURE_2D_ARRAY);
        mZeroTextures[GL_TEXTURE_2D_ARRAY].set(this, zeroTexture2DArray);
    }
    if (getClientVersion() >= Version(3, 1))
    {
        Texture *zeroTexture2DMultisample =
            new Texture(mImplementation.get(), 0, GL_TEXTURE_2D_MULTISAMPLE);
        mZeroTextures[GL_TEXTURE_2D_MULTISAMPLE].set(this, zeroTexture2DMultisample);

        for (unsigned int i = 0; i < mCaps.maxAtomicCounterBufferBindings; i++)
        {
            bindBufferRange(BufferBinding::AtomicCounter, 0, i, 0, 0);
        }

        for (unsigned int i = 0; i < mCaps.maxShaderStorageBufferBindings; i++)
        {
            bindBufferRange(BufferBinding::ShaderStorage, i, 0, 0, 0);
        }
    }

    const Extensions &nativeExtensions = mImplementation->getNativeExtensions();
    if (nativeExtensions.textureRectangle)
    {
        Texture *zeroTextureRectangle =
            new Texture(mImplementation.get(), 0, GL_TEXTURE_RECTANGLE_ANGLE);
        mZeroTextures[GL_TEXTURE_RECTANGLE_ANGLE].set(this, zeroTextureRectangle);
    }

    if (nativeExtensions.eglImageExternal || nativeExtensions.eglStreamConsumerExternal)
    {
        Texture *zeroTextureExternal =
            new Texture(mImplementation.get(), 0, GL_TEXTURE_EXTERNAL_OES);
        mZeroTextures[GL_TEXTURE_EXTERNAL_OES].set(this, zeroTextureExternal);
    }

    mGLState.initializeZeroTextures(this, mZeroTextures);

    bindVertexArray(0);

    if (getClientVersion() >= Version(3, 0))
    {
        // [OpenGL ES 3.0.2] section 2.14.1 pg 85:
        // In the initial state, a default transform feedback object is bound and treated as
        // a transform feedback object with a name of zero. That object is bound any time
        // BindTransformFeedback is called with id of zero
        bindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    }

    for (auto type : angle::AllEnums<BufferBinding>())
    {
        bindBuffer(type, 0);
    }

    bindRenderbuffer(GL_RENDERBUFFER, 0);

    for (unsigned int i = 0; i < mCaps.maxUniformBufferBindings; i++)
    {
        bindBufferRange(BufferBinding::Uniform, i, 0, 0, -1);
    }

    // Initialize dirty bit masks
    mTexImageDirtyBits.set(State::DIRTY_BIT_UNPACK_STATE);
    mTexImageDirtyBits.set(State::DIRTY_BIT_UNPACK_BUFFER_BINDING);
    // No dirty objects.

    // Readpixels uses the pack state and read FBO
    mReadPixelsDirtyBits.set(State::DIRTY_BIT_PACK_STATE);
    mReadPixelsDirtyBits.set(State::DIRTY_BIT_PACK_BUFFER_BINDING);
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
    mBlitDirtyBits.set(State::DIRTY_BIT_FRAMEBUFFER_SRGB);
    mBlitDirtyObjects.set(State::DIRTY_OBJECT_READ_FRAMEBUFFER);
    mBlitDirtyObjects.set(State::DIRTY_OBJECT_DRAW_FRAMEBUFFER);

    handleError(mImplementation->initialize());
}

egl::Error Context::onDestroy(const egl::Display *display)
{
    for (auto fence : mFenceNVMap)
    {
        SafeDelete(fence.second);
    }
    mFenceNVMap.clear();

    for (auto query : mQueryMap)
    {
        if (query.second != nullptr)
        {
            query.second->release(this);
        }
    }
    mQueryMap.clear();

    for (auto vertexArray : mVertexArrayMap)
    {
        if (vertexArray.second)
        {
            vertexArray.second->onDestroy(this);
        }
    }
    mVertexArrayMap.clear();

    for (auto transformFeedback : mTransformFeedbackMap)
    {
        if (transformFeedback.second != nullptr)
        {
            transformFeedback.second->release(this);
        }
    }
    mTransformFeedbackMap.clear();

    for (auto &zeroTexture : mZeroTextures)
    {
        ANGLE_TRY(zeroTexture.second->onDestroy(this));
        zeroTexture.second.set(this, nullptr);
    }
    mZeroTextures.clear();

    SafeDelete(mSurfacelessFramebuffer);

    ANGLE_TRY(releaseSurface(display));
    releaseShaderCompiler();

    mGLState.reset(this);

    mState.mBuffers->release(this);
    mState.mShaderPrograms->release(this);
    mState.mTextures->release(this);
    mState.mRenderbuffers->release(this);
    mState.mSamplers->release(this);
    mState.mSyncs->release(this);
    mState.mPaths->release(this);
    mState.mFramebuffers->release(this);
    mState.mPipelines->release(this);

    mImplementation->onDestroy(this);

    return egl::NoError();
}

Context::~Context()
{
}

egl::Error Context::makeCurrent(egl::Display *display, egl::Surface *surface)
{
    mCurrentDisplay = display;

    if (!mHasBeenCurrent)
    {
        initRendererString();
        initVersionStrings();
        initExtensionStrings();

        int width  = 0;
        int height = 0;
        if (surface != nullptr)
        {
            width  = surface->getWidth();
            height = surface->getHeight();
        }

        mGLState.setViewportParams(0, 0, width, height);
        mGLState.setScissorParams(0, 0, width, height);

        mHasBeenCurrent = true;
    }

    // TODO(jmadill): Rework this when we support ContextImpl
    mGLState.setAllDirtyBits();
    mGLState.setAllDirtyObjects();

    ANGLE_TRY(releaseSurface(display));

    Framebuffer *newDefault = nullptr;
    if (surface != nullptr)
    {
        ANGLE_TRY(surface->setIsCurrent(this, true));
        mCurrentSurface = surface;
        newDefault      = surface->getDefaultFramebuffer();
    }
    else
    {
        if (mSurfacelessFramebuffer == nullptr)
        {
            mSurfacelessFramebuffer = new Framebuffer(mImplementation.get());
        }

        newDefault = mSurfacelessFramebuffer;
    }

    // Update default framebuffer, the binding of the previous default
    // framebuffer (or lack of) will have a nullptr.
    {
        if (mGLState.getReadFramebuffer() == nullptr)
        {
            mGLState.setReadFramebufferBinding(newDefault);
        }
        if (mGLState.getDrawFramebuffer() == nullptr)
        {
            mGLState.setDrawFramebufferBinding(newDefault);
        }
        mState.mFramebuffers->setDefaultFramebuffer(newDefault);
    }

    // Notify the renderer of a context switch
    mImplementation->onMakeCurrent(this);
    return egl::NoError();
}

egl::Error Context::releaseSurface(const egl::Display *display)
{
    // Remove the default framebuffer
    Framebuffer *currentDefault = nullptr;
    if (mCurrentSurface != nullptr)
    {
        currentDefault = mCurrentSurface->getDefaultFramebuffer();
    }
    else if (mSurfacelessFramebuffer != nullptr)
    {
        currentDefault = mSurfacelessFramebuffer;
    }

    if (mGLState.getReadFramebuffer() == currentDefault)
    {
        mGLState.setReadFramebufferBinding(nullptr);
    }
    if (mGLState.getDrawFramebuffer() == currentDefault)
    {
        mGLState.setDrawFramebufferBinding(nullptr);
    }
    mState.mFramebuffers->setDefaultFramebuffer(nullptr);

    if (mCurrentSurface)
    {
        ANGLE_TRY(mCurrentSurface->setIsCurrent(this, false));
        mCurrentSurface = nullptr;
    }

    return egl::NoError();
}

GLuint Context::createBuffer()
{
    return mState.mBuffers->createBuffer();
}

GLuint Context::createProgram()
{
    return mState.mShaderPrograms->createProgram(mImplementation.get());
}

GLuint Context::createShader(GLenum type)
{
    return mState.mShaderPrograms->createShader(mImplementation.get(), mLimitations, type);
}

GLuint Context::createTexture()
{
    return mState.mTextures->createTexture();
}

GLuint Context::createRenderbuffer()
{
    return mState.mRenderbuffers->createRenderbuffer();
}

GLuint Context::createPaths(GLsizei range)
{
    auto resultOrError = mState.mPaths->createPaths(mImplementation.get(), range);
    if (resultOrError.isError())
    {
        handleError(resultOrError.getError());
        return 0;
    }
    return resultOrError.getResult();
}

// Returns an unused framebuffer name
GLuint Context::createFramebuffer()
{
    return mState.mFramebuffers->createFramebuffer();
}

GLuint Context::createFenceNV()
{
    GLuint handle = mFenceNVHandleAllocator.allocate();
    mFenceNVMap.assign(handle, new FenceNV(mImplementation->createFenceNV()));
    return handle;
}

GLuint Context::createProgramPipeline()
{
    return mState.mPipelines->createProgramPipeline();
}

GLuint Context::createShaderProgramv(GLenum type, GLsizei count, const GLchar *const *strings)
{
    UNIMPLEMENTED();
    return 0u;
}

void Context::deleteBuffer(GLuint buffer)
{
    if (mState.mBuffers->getBuffer(buffer))
    {
        detachBuffer(buffer);
    }

    mState.mBuffers->deleteObject(this, buffer);
}

void Context::deleteShader(GLuint shader)
{
    mState.mShaderPrograms->deleteShader(this, shader);
}

void Context::deleteProgram(GLuint program)
{
    mState.mShaderPrograms->deleteProgram(this, program);
}

void Context::deleteTexture(GLuint texture)
{
    if (mState.mTextures->getTexture(texture))
    {
        detachTexture(texture);
    }

    mState.mTextures->deleteObject(this, texture);
}

void Context::deleteRenderbuffer(GLuint renderbuffer)
{
    if (mState.mRenderbuffers->getRenderbuffer(renderbuffer))
    {
        detachRenderbuffer(renderbuffer);
    }

    mState.mRenderbuffers->deleteObject(this, renderbuffer);
}

void Context::deleteSync(GLsync sync)
{
    // The spec specifies the underlying Fence object is not deleted until all current
    // wait commands finish. However, since the name becomes invalid, we cannot query the fence,
    // and since our API is currently designed for being called from a single thread, we can delete
    // the fence immediately.
    mState.mSyncs->deleteObject(this, static_cast<GLuint>(reinterpret_cast<uintptr_t>(sync)));
}

void Context::deleteProgramPipeline(GLuint pipeline)
{
    if (mState.mPipelines->getProgramPipeline(pipeline))
    {
        detachProgramPipeline(pipeline);
    }

    mState.mPipelines->deleteObject(this, pipeline);
}

void Context::deletePaths(GLuint first, GLsizei range)
{
    mState.mPaths->deletePaths(first, range);
}

bool Context::hasPathData(GLuint path) const
{
    const auto *pathObj = mState.mPaths->getPath(path);
    if (pathObj == nullptr)
        return false;

    return pathObj->hasPathData();
}

bool Context::hasPath(GLuint path) const
{
    return mState.mPaths->hasPath(path);
}

void Context::setPathCommands(GLuint path,
                              GLsizei numCommands,
                              const GLubyte *commands,
                              GLsizei numCoords,
                              GLenum coordType,
                              const void *coords)
{
    auto *pathObject = mState.mPaths->getPath(path);

    handleError(pathObject->setCommands(numCommands, commands, numCoords, coordType, coords));
}

void Context::setPathParameterf(GLuint path, GLenum pname, GLfloat value)
{
    auto *pathObj = mState.mPaths->getPath(path);

    switch (pname)
    {
        case GL_PATH_STROKE_WIDTH_CHROMIUM:
            pathObj->setStrokeWidth(value);
            break;
        case GL_PATH_END_CAPS_CHROMIUM:
            pathObj->setEndCaps(static_cast<GLenum>(value));
            break;
        case GL_PATH_JOIN_STYLE_CHROMIUM:
            pathObj->setJoinStyle(static_cast<GLenum>(value));
            break;
        case GL_PATH_MITER_LIMIT_CHROMIUM:
            pathObj->setMiterLimit(value);
            break;
        case GL_PATH_STROKE_BOUND_CHROMIUM:
            pathObj->setStrokeBound(value);
            break;
        default:
            UNREACHABLE();
            break;
    }
}

void Context::getPathParameterfv(GLuint path, GLenum pname, GLfloat *value) const
{
    const auto *pathObj = mState.mPaths->getPath(path);

    switch (pname)
    {
        case GL_PATH_STROKE_WIDTH_CHROMIUM:
            *value = pathObj->getStrokeWidth();
            break;
        case GL_PATH_END_CAPS_CHROMIUM:
            *value = static_cast<GLfloat>(pathObj->getEndCaps());
            break;
        case GL_PATH_JOIN_STYLE_CHROMIUM:
            *value = static_cast<GLfloat>(pathObj->getJoinStyle());
            break;
        case GL_PATH_MITER_LIMIT_CHROMIUM:
            *value = pathObj->getMiterLimit();
            break;
        case GL_PATH_STROKE_BOUND_CHROMIUM:
            *value = pathObj->getStrokeBound();
            break;
        default:
            UNREACHABLE();
            break;
    }
}

void Context::setPathStencilFunc(GLenum func, GLint ref, GLuint mask)
{
    mGLState.setPathStencilFunc(func, ref, mask);
}

void Context::deleteFramebuffer(GLuint framebuffer)
{
    if (mState.mFramebuffers->getFramebuffer(framebuffer))
    {
        detachFramebuffer(framebuffer);
    }

    mState.mFramebuffers->deleteObject(this, framebuffer);
}

void Context::deleteFenceNV(GLuint fence)
{
    FenceNV *fenceObject = nullptr;
    if (mFenceNVMap.erase(fence, &fenceObject))
    {
        mFenceNVHandleAllocator.release(fence);
        delete fenceObject;
    }
}

Buffer *Context::getBuffer(GLuint handle) const
{
    return mState.mBuffers->getBuffer(handle);
}

Texture *Context::getTexture(GLuint handle) const
{
    return mState.mTextures->getTexture(handle);
}

Renderbuffer *Context::getRenderbuffer(GLuint handle) const
{
    return mState.mRenderbuffers->getRenderbuffer(handle);
}

Sync *Context::getSync(GLsync handle) const
{
    return mState.mSyncs->getSync(static_cast<GLuint>(reinterpret_cast<uintptr_t>(handle)));
}

VertexArray *Context::getVertexArray(GLuint handle) const
{
    return mVertexArrayMap.query(handle);
}

Sampler *Context::getSampler(GLuint handle) const
{
    return mState.mSamplers->getSampler(handle);
}

TransformFeedback *Context::getTransformFeedback(GLuint handle) const
{
    return mTransformFeedbackMap.query(handle);
}

ProgramPipeline *Context::getProgramPipeline(GLuint handle) const
{
    return mState.mPipelines->getProgramPipeline(handle);
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
    return getSync(reinterpret_cast<GLsync>(const_cast<void *>(ptr)));
}

void Context::objectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar *label)
{
    LabeledObject *object = getLabeledObject(identifier, name);
    ASSERT(object != nullptr);

    std::string labelName = GetObjectLabelFromPointer(length, label);
    object->setLabel(labelName);

    // TODO(jmadill): Determine if the object is dirty based on 'name'. Conservatively assume the
    // specified object is active until we do this.
    mGLState.setObjectDirty(identifier);
}

void Context::objectPtrLabel(const void *ptr, GLsizei length, const GLchar *label)
{
    LabeledObject *object = getLabeledObjectFromPtr(ptr);
    ASSERT(object != nullptr);

    std::string labelName = GetObjectLabelFromPointer(length, label);
    object->setLabel(labelName);
}

void Context::getObjectLabel(GLenum identifier,
                             GLuint name,
                             GLsizei bufSize,
                             GLsizei *length,
                             GLchar *label) const
{
    LabeledObject *object = getLabeledObject(identifier, name);
    ASSERT(object != nullptr);

    const std::string &objectLabel = object->getLabel();
    GetObjectLabelBase(objectLabel, bufSize, length, label);
}

void Context::getObjectPtrLabel(const void *ptr,
                                GLsizei bufSize,
                                GLsizei *length,
                                GLchar *label) const
{
    LabeledObject *object = getLabeledObjectFromPtr(ptr);
    ASSERT(object != nullptr);

    const std::string &objectLabel = object->getLabel();
    GetObjectLabelBase(objectLabel, bufSize, length, label);
}

bool Context::isSampler(GLuint samplerName) const
{
    return mState.mSamplers->isSampler(samplerName);
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
        texture = mState.mTextures->checkTextureAllocation(mImplementation.get(), handle, target);
    }

    ASSERT(texture);
    mGLState.setSamplerTexture(this, target, texture);
}

void Context::bindReadFramebuffer(GLuint framebufferHandle)
{
    Framebuffer *framebuffer = mState.mFramebuffers->checkFramebufferAllocation(
        mImplementation.get(), mCaps, framebufferHandle);
    mGLState.setReadFramebufferBinding(framebuffer);
}

void Context::bindDrawFramebuffer(GLuint framebufferHandle)
{
    Framebuffer *framebuffer = mState.mFramebuffers->checkFramebufferAllocation(
        mImplementation.get(), mCaps, framebufferHandle);
    mGLState.setDrawFramebufferBinding(framebuffer);
}

void Context::bindVertexArray(GLuint vertexArrayHandle)
{
    VertexArray *vertexArray = checkVertexArrayAllocation(vertexArrayHandle);
    mGLState.setVertexArrayBinding(vertexArray);
}

void Context::bindVertexBuffer(GLuint bindingIndex,
                               GLuint bufferHandle,
                               GLintptr offset,
                               GLsizei stride)
{
    Buffer *buffer = mState.mBuffers->checkBufferAllocation(mImplementation.get(), bufferHandle);
    mGLState.bindVertexBuffer(this, bindingIndex, buffer, offset, stride);
}

void Context::bindSampler(GLuint textureUnit, GLuint samplerHandle)
{
    ASSERT(textureUnit < mCaps.maxCombinedTextureImageUnits);
    Sampler *sampler =
        mState.mSamplers->checkSamplerAllocation(mImplementation.get(), samplerHandle);
    mGLState.setSamplerBinding(this, textureUnit, sampler);
}

void Context::bindImageTexture(GLuint unit,
                               GLuint texture,
                               GLint level,
                               GLboolean layered,
                               GLint layer,
                               GLenum access,
                               GLenum format)
{
    Texture *tex = mState.mTextures->getTexture(texture);
    mGLState.setImageUnit(this, unit, tex, level, layered, layer, access, format);
}

void Context::useProgram(GLuint program)
{
    mGLState.setProgram(this, getProgram(program));
}

void Context::useProgramStages(GLuint pipeline, GLbitfield stages, GLuint program)
{
    UNIMPLEMENTED();
}

void Context::bindTransformFeedback(GLenum target, GLuint transformFeedbackHandle)
{
    ASSERT(target == GL_TRANSFORM_FEEDBACK);
    TransformFeedback *transformFeedback =
        checkTransformFeedbackAllocation(transformFeedbackHandle);
    mGLState.setTransformFeedbackBinding(this, transformFeedback);
}

void Context::bindProgramPipeline(GLuint pipelineHandle)
{
    ProgramPipeline *pipeline =
        mState.mPipelines->checkProgramPipelineAllocation(mImplementation.get(), pipelineHandle);
    mGLState.setProgramPipelineBinding(this, pipeline);
}

void Context::beginQuery(GLenum target, GLuint query)
{
    Query *queryObject = getQuery(query, true, target);
    ASSERT(queryObject);

    // begin query
    ANGLE_CONTEXT_TRY(queryObject->begin());

    // set query as active for specified target only if begin succeeded
    mGLState.setActiveQuery(this, target, queryObject);
}

void Context::endQuery(GLenum target)
{
    Query *queryObject = mGLState.getActiveQuery(target);
    ASSERT(queryObject);

    handleError(queryObject->end());

    // Always unbind the query, even if there was an error. This may delete the query object.
    mGLState.setActiveQuery(this, target, nullptr);
}

void Context::queryCounter(GLuint id, GLenum target)
{
    ASSERT(target == GL_TIMESTAMP_EXT);

    Query *queryObject = getQuery(id, true, target);
    ASSERT(queryObject);

    handleError(queryObject->queryCounter());
}

void Context::getQueryiv(GLenum target, GLenum pname, GLint *params)
{
    switch (pname)
    {
        case GL_CURRENT_QUERY_EXT:
            params[0] = mGLState.getActiveQueryId(target);
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

void Context::getQueryObjectiv(GLuint id, GLenum pname, GLint *params)
{
    handleError(GetQueryObjectParameter(getQuery(id), pname, params));
}

void Context::getQueryObjectuiv(GLuint id, GLenum pname, GLuint *params)
{
    handleError(GetQueryObjectParameter(getQuery(id), pname, params));
}

void Context::getQueryObjecti64v(GLuint id, GLenum pname, GLint64 *params)
{
    handleError(GetQueryObjectParameter(getQuery(id), pname, params));
}

void Context::getQueryObjectui64v(GLuint id, GLenum pname, GLuint64 *params)
{
    handleError(GetQueryObjectParameter(getQuery(id), pname, params));
}

Framebuffer *Context::getFramebuffer(GLuint handle) const
{
    return mState.mFramebuffers->getFramebuffer(handle);
}

FenceNV *Context::getFenceNV(GLuint handle)
{
    return mFenceNVMap.query(handle);
}

Query *Context::getQuery(GLuint handle, bool create, GLenum type)
{
    if (!mQueryMap.contains(handle))
    {
        return nullptr;
    }

    Query *query = mQueryMap.query(handle);
    if (!query && create)
    {
        query = new Query(mImplementation->createQuery(type), handle);
        query->addRef();
        mQueryMap.assign(handle, query);
    }
    return query;
}

Query *Context::getQuery(GLuint handle) const
{
    return mQueryMap.query(handle);
}

Texture *Context::getTargetTexture(GLenum target) const
{
    ASSERT(ValidTextureTarget(this, target) || ValidTextureExternalTarget(this, target));
    return mGLState.getTargetTexture(target);
}

Texture *Context::getSamplerTexture(unsigned int sampler, GLenum type) const
{
    return mGLState.getSamplerTexture(sampler, type);
}

Compiler *Context::getCompiler() const
{
    if (mCompiler.get() == nullptr)
    {
        mCompiler.set(this, new Compiler(mImplementation.get(), mState));
    }
    return mCompiler.get();
}

void Context::getBooleanvImpl(GLenum pname, GLboolean *params)
{
    switch (pname)
    {
        case GL_SHADER_COMPILER:
            *params = GL_TRUE;
            break;
        case GL_CONTEXT_ROBUST_ACCESS_EXT:
            *params = mRobustAccess ? GL_TRUE : GL_FALSE;
            break;
        default:
            mGLState.getBooleanv(pname, params);
            break;
    }
}

void Context::getFloatvImpl(GLenum pname, GLfloat *params)
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

        case GL_PATH_MODELVIEW_MATRIX_CHROMIUM:
        case GL_PATH_PROJECTION_MATRIX_CHROMIUM:
        {
            ASSERT(mExtensions.pathRendering);
            const GLfloat *m = mGLState.getPathRenderingMatrix(pname);
            memcpy(params, m, 16 * sizeof(GLfloat));
        }
        break;

        default:
            mGLState.getFloatv(pname, params);
            break;
    }
}

void Context::getIntegervImpl(GLenum pname, GLint *params)
{
    // Queries about context capabilities and maximums are answered by Context.
    // Queries about current GL state values are answered by State.

    switch (pname)
    {
        case GL_MAX_VERTEX_ATTRIBS:
            *params = mCaps.maxVertexAttributes;
            break;
        case GL_MAX_VERTEX_UNIFORM_VECTORS:
            *params = mCaps.maxVertexUniformVectors;
            break;
        case GL_MAX_VERTEX_UNIFORM_COMPONENTS:
            *params = mCaps.maxVertexUniformComponents;
            break;
        case GL_MAX_VARYING_VECTORS:
            *params = mCaps.maxVaryingVectors;
            break;
        case GL_MAX_VARYING_COMPONENTS:
            *params = mCaps.maxVertexOutputComponents;
            break;
        case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
            *params = mCaps.maxCombinedTextureImageUnits;
            break;
        case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
            *params = mCaps.maxVertexTextureImageUnits;
            break;
        case GL_MAX_TEXTURE_IMAGE_UNITS:
            *params = mCaps.maxTextureImageUnits;
            break;
        case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
            *params = mCaps.maxFragmentUniformVectors;
            break;
        case GL_MAX_FRAGMENT_UNIFORM_COMPONENTS:
            *params = mCaps.maxFragmentUniformComponents;
            break;
        case GL_MAX_RENDERBUFFER_SIZE:
            *params = mCaps.maxRenderbufferSize;
            break;
        case GL_MAX_COLOR_ATTACHMENTS_EXT:
            *params = mCaps.maxColorAttachments;
            break;
        case GL_MAX_DRAW_BUFFERS_EXT:
            *params = mCaps.maxDrawBuffers;
            break;
        // case GL_FRAMEBUFFER_BINDING:                    // now equivalent to
        // GL_DRAW_FRAMEBUFFER_BINDING_ANGLE
        case GL_SUBPIXEL_BITS:
            *params = 4;
            break;
        case GL_MAX_TEXTURE_SIZE:
            *params = mCaps.max2DTextureSize;
            break;
        case GL_MAX_RECTANGLE_TEXTURE_SIZE_ANGLE:
            *params = mCaps.maxRectangleTextureSize;
            break;
        case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
            *params = mCaps.maxCubeMapTextureSize;
            break;
        case GL_MAX_3D_TEXTURE_SIZE:
            *params = mCaps.max3DTextureSize;
            break;
        case GL_MAX_ARRAY_TEXTURE_LAYERS:
            *params = mCaps.maxArrayTextureLayers;
            break;
        case GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT:
            *params = mCaps.uniformBufferOffsetAlignment;
            break;
        case GL_MAX_UNIFORM_BUFFER_BINDINGS:
            *params = mCaps.maxUniformBufferBindings;
            break;
        case GL_MAX_VERTEX_UNIFORM_BLOCKS:
            *params = mCaps.maxVertexUniformBlocks;
            break;
        case GL_MAX_FRAGMENT_UNIFORM_BLOCKS:
            *params = mCaps.maxFragmentUniformBlocks;
            break;
        case GL_MAX_COMBINED_UNIFORM_BLOCKS:
            *params = mCaps.maxCombinedTextureImageUnits;
            break;
        case GL_MAX_VERTEX_OUTPUT_COMPONENTS:
            *params = mCaps.maxVertexOutputComponents;
            break;
        case GL_MAX_FRAGMENT_INPUT_COMPONENTS:
            *params = mCaps.maxFragmentInputComponents;
            break;
        case GL_MIN_PROGRAM_TEXEL_OFFSET:
            *params = mCaps.minProgramTexelOffset;
            break;
        case GL_MAX_PROGRAM_TEXEL_OFFSET:
            *params = mCaps.maxProgramTexelOffset;
            break;
        case GL_MAJOR_VERSION:
            *params = getClientVersion().major;
            break;
        case GL_MINOR_VERSION:
            *params = getClientVersion().minor;
            break;
        case GL_MAX_ELEMENTS_INDICES:
            *params = mCaps.maxElementsIndices;
            break;
        case GL_MAX_ELEMENTS_VERTICES:
            *params = mCaps.maxElementsVertices;
            break;
        case GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS:
            *params = mCaps.maxTransformFeedbackInterleavedComponents;
            break;
        case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS:
            *params = mCaps.maxTransformFeedbackSeparateAttributes;
            break;
        case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS:
            *params = mCaps.maxTransformFeedbackSeparateComponents;
            break;
        case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
            *params = static_cast<GLint>(mCaps.compressedTextureFormats.size());
            break;
        case GL_MAX_SAMPLES_ANGLE:
            *params = mCaps.maxSamples;
            break;
        case GL_MAX_VIEWPORT_DIMS:
        {
            params[0] = mCaps.maxViewportWidth;
            params[1] = mCaps.maxViewportHeight;
        }
        break;
        case GL_COMPRESSED_TEXTURE_FORMATS:
            std::copy(mCaps.compressedTextureFormats.begin(), mCaps.compressedTextureFormats.end(),
                      params);
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

        // GL_ANGLE_multiview
        case GL_MAX_VIEWS_ANGLE:
            *params = mExtensions.maxViews;
            break;

        // GL_EXT_disjoint_timer_query
        case GL_GPU_DISJOINT_EXT:
            *params = mImplementation->getGPUDisjoint();
            break;
        case GL_MAX_FRAMEBUFFER_WIDTH:
            *params = mCaps.maxFramebufferWidth;
            break;
        case GL_MAX_FRAMEBUFFER_HEIGHT:
            *params = mCaps.maxFramebufferHeight;
            break;
        case GL_MAX_FRAMEBUFFER_SAMPLES:
            *params = mCaps.maxFramebufferSamples;
            break;
        case GL_MAX_SAMPLE_MASK_WORDS:
            *params = mCaps.maxSampleMaskWords;
            break;
        case GL_MAX_COLOR_TEXTURE_SAMPLES:
            *params = mCaps.maxColorTextureSamples;
            break;
        case GL_MAX_DEPTH_TEXTURE_SAMPLES:
            *params = mCaps.maxDepthTextureSamples;
            break;
        case GL_MAX_INTEGER_SAMPLES:
            *params = mCaps.maxIntegerSamples;
            break;
        case GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET:
            *params = mCaps.maxVertexAttribRelativeOffset;
            break;
        case GL_MAX_VERTEX_ATTRIB_BINDINGS:
            *params = mCaps.maxVertexAttribBindings;
            break;
        case GL_MAX_VERTEX_ATTRIB_STRIDE:
            *params = mCaps.maxVertexAttribStride;
            break;
        case GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS:
            *params = mCaps.maxVertexAtomicCounterBuffers;
            break;
        case GL_MAX_VERTEX_ATOMIC_COUNTERS:
            *params = mCaps.maxVertexAtomicCounters;
            break;
        case GL_MAX_VERTEX_IMAGE_UNIFORMS:
            *params = mCaps.maxVertexImageUniforms;
            break;
        case GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS:
            *params = mCaps.maxVertexShaderStorageBlocks;
            break;
        case GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS:
            *params = mCaps.maxFragmentAtomicCounterBuffers;
            break;
        case GL_MAX_FRAGMENT_ATOMIC_COUNTERS:
            *params = mCaps.maxFragmentAtomicCounters;
            break;
        case GL_MAX_FRAGMENT_IMAGE_UNIFORMS:
            *params = mCaps.maxFragmentImageUniforms;
            break;
        case GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS:
            *params = mCaps.maxFragmentShaderStorageBlocks;
            break;
        case GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET:
            *params = mCaps.minProgramTextureGatherOffset;
            break;
        case GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET:
            *params = mCaps.maxProgramTextureGatherOffset;
            break;
        case GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS:
            *params = mCaps.maxComputeWorkGroupInvocations;
            break;
        case GL_MAX_COMPUTE_UNIFORM_BLOCKS:
            *params = mCaps.maxComputeUniformBlocks;
            break;
        case GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS:
            *params = mCaps.maxComputeTextureImageUnits;
            break;
        case GL_MAX_COMPUTE_SHARED_MEMORY_SIZE:
            *params = mCaps.maxComputeSharedMemorySize;
            break;
        case GL_MAX_COMPUTE_UNIFORM_COMPONENTS:
            *params = mCaps.maxComputeUniformComponents;
            break;
        case GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS:
            *params = mCaps.maxComputeAtomicCounterBuffers;
            break;
        case GL_MAX_COMPUTE_ATOMIC_COUNTERS:
            *params = mCaps.maxComputeAtomicCounters;
            break;
        case GL_MAX_COMPUTE_IMAGE_UNIFORMS:
            *params = mCaps.maxComputeImageUniforms;
            break;
        case GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS:
            *params = mCaps.maxCombinedComputeUniformComponents;
            break;
        case GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS:
            *params = mCaps.maxComputeShaderStorageBlocks;
            break;
        case GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES:
            *params = mCaps.maxCombinedShaderOutputResources;
            break;
        case GL_MAX_UNIFORM_LOCATIONS:
            *params = mCaps.maxUniformLocations;
            break;
        case GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS:
            *params = mCaps.maxAtomicCounterBufferBindings;
            break;
        case GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE:
            *params = mCaps.maxAtomicCounterBufferSize;
            break;
        case GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS:
            *params = mCaps.maxCombinedAtomicCounterBuffers;
            break;
        case GL_MAX_COMBINED_ATOMIC_COUNTERS:
            *params = mCaps.maxCombinedAtomicCounters;
            break;
        case GL_MAX_IMAGE_UNITS:
            *params = mCaps.maxImageUnits;
            break;
        case GL_MAX_COMBINED_IMAGE_UNIFORMS:
            *params = mCaps.maxCombinedImageUniforms;
            break;
        case GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS:
            *params = mCaps.maxShaderStorageBufferBindings;
            break;
        case GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS:
            *params = mCaps.maxCombinedShaderStorageBlocks;
            break;
        case GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT:
            *params = mCaps.shaderStorageBufferOffsetAlignment;
            break;
        default:
            mGLState.getIntegerv(this, pname, params);
            break;
    }
}

void Context::getInteger64vImpl(GLenum pname, GLint64 *params)
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
            *params = mImplementation->getTimestamp();
            break;

        case GL_MAX_SHADER_STORAGE_BLOCK_SIZE:
            *params = mCaps.maxShaderStorageBlockSize;
            break;
        default:
            UNREACHABLE();
            break;
    }
}

void Context::getPointerv(GLenum pname, void **params) const
{
    mGLState.getPointerv(pname, params);
}

void Context::getIntegeri_v(GLenum target, GLuint index, GLint *data)
{
    // Queries about context capabilities and maximums are answered by Context.
    // Queries about current GL state values are answered by State.

    GLenum nativeType;
    unsigned int numParams;
    bool queryStatus = getIndexedQueryParameterInfo(target, &nativeType, &numParams);
    ASSERT(queryStatus);

    if (nativeType == GL_INT)
    {
        switch (target)
        {
            case GL_MAX_COMPUTE_WORK_GROUP_COUNT:
                ASSERT(index < 3u);
                *data = mCaps.maxComputeWorkGroupCount[index];
                break;
            case GL_MAX_COMPUTE_WORK_GROUP_SIZE:
                ASSERT(index < 3u);
                *data = mCaps.maxComputeWorkGroupSize[index];
                break;
            default:
                mGLState.getIntegeri_v(target, index, data);
        }
    }
    else
    {
        CastIndexedStateValues(this, nativeType, target, index, numParams, data);
    }
}

void Context::getInteger64i_v(GLenum target, GLuint index, GLint64 *data)
{
    // Queries about context capabilities and maximums are answered by Context.
    // Queries about current GL state values are answered by State.

    GLenum nativeType;
    unsigned int numParams;
    bool queryStatus = getIndexedQueryParameterInfo(target, &nativeType, &numParams);
    ASSERT(queryStatus);

    if (nativeType == GL_INT_64_ANGLEX)
    {
        mGLState.getInteger64i_v(target, index, data);
    }
    else
    {
        CastIndexedStateValues(this, nativeType, target, index, numParams, data);
    }
}

void Context::getBooleani_v(GLenum target, GLuint index, GLboolean *data)
{
    // Queries about context capabilities and maximums are answered by Context.
    // Queries about current GL state values are answered by State.

    GLenum nativeType;
    unsigned int numParams;
    bool queryStatus = getIndexedQueryParameterInfo(target, &nativeType, &numParams);
    ASSERT(queryStatus);

    if (nativeType == GL_BOOL)
    {
        mGLState.getBooleani_v(target, index, data);
    }
    else
    {
        CastIndexedStateValues(this, nativeType, target, index, numParams, data);
    }
}

void Context::getBufferParameteriv(BufferBinding target, GLenum pname, GLint *params)
{
    Buffer *buffer = mGLState.getTargetBuffer(target);
    QueryBufferParameteriv(buffer, pname, params);
}

void Context::getFramebufferAttachmentParameteriv(GLenum target,
                                                  GLenum attachment,
                                                  GLenum pname,
                                                  GLint *params)
{
    const Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    QueryFramebufferAttachmentParameteriv(framebuffer, attachment, pname, params);
}

void Context::getRenderbufferParameteriv(GLenum target, GLenum pname, GLint *params)
{
    Renderbuffer *renderbuffer = mGLState.getCurrentRenderbuffer();
    QueryRenderbufferiv(this, renderbuffer, pname, params);
}

void Context::getTexParameterfv(GLenum target, GLenum pname, GLfloat *params)
{
    Texture *texture = getTargetTexture(target);
    QueryTexParameterfv(texture, pname, params);
}

void Context::getTexParameteriv(GLenum target, GLenum pname, GLint *params)
{
    Texture *texture = getTargetTexture(target);
    QueryTexParameteriv(texture, pname, params);
}

void Context::getTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params)
{
    Texture *texture =
        getTargetTexture(IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target);
    QueryTexLevelParameteriv(texture, target, level, pname, params);
}

void Context::getTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params)
{
    Texture *texture =
        getTargetTexture(IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target);
    QueryTexLevelParameterfv(texture, target, level, pname, params);
}

void Context::texParameterf(GLenum target, GLenum pname, GLfloat param)
{
    Texture *texture = getTargetTexture(target);
    SetTexParameterf(this, texture, pname, param);
    onTextureChange(texture);
}

void Context::texParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
    Texture *texture = getTargetTexture(target);
    SetTexParameterfv(this, texture, pname, params);
    onTextureChange(texture);
}

void Context::texParameteri(GLenum target, GLenum pname, GLint param)
{
    Texture *texture = getTargetTexture(target);
    SetTexParameteri(this, texture, pname, param);
    onTextureChange(texture);
}

void Context::texParameteriv(GLenum target, GLenum pname, const GLint *params)
{
    Texture *texture = getTargetTexture(target);
    SetTexParameteriv(this, texture, pname, params);
    onTextureChange(texture);
}

void Context::drawArrays(GLenum mode, GLint first, GLsizei count)
{
    ANGLE_CONTEXT_TRY(prepareForDraw());
    ANGLE_CONTEXT_TRY(mImplementation->drawArrays(this, mode, first, count));
    MarkTransformFeedbackBufferUsage(mGLState.getCurrentTransformFeedback());
}

void Context::drawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instanceCount)
{
    ANGLE_CONTEXT_TRY(prepareForDraw());
    ANGLE_CONTEXT_TRY(
        mImplementation->drawArraysInstanced(this, mode, first, count, instanceCount));
    MarkTransformFeedbackBufferUsage(mGLState.getCurrentTransformFeedback());
}

void Context::drawElements(GLenum mode, GLsizei count, GLenum type, const void *indices)
{
    ANGLE_CONTEXT_TRY(prepareForDraw());
    ANGLE_CONTEXT_TRY(mImplementation->drawElements(this, mode, count, type, indices));
}

void Context::drawElementsInstanced(GLenum mode,
                                    GLsizei count,
                                    GLenum type,
                                    const void *indices,
                                    GLsizei instances)
{
    ANGLE_CONTEXT_TRY(prepareForDraw());
    ANGLE_CONTEXT_TRY(
        mImplementation->drawElementsInstanced(this, mode, count, type, indices, instances));
}

void Context::drawRangeElements(GLenum mode,
                                GLuint start,
                                GLuint end,
                                GLsizei count,
                                GLenum type,
                                const void *indices)
{
    ANGLE_CONTEXT_TRY(prepareForDraw());
    ANGLE_CONTEXT_TRY(
        mImplementation->drawRangeElements(this, mode, start, end, count, type, indices));
}

void Context::drawArraysIndirect(GLenum mode, const void *indirect)
{
    ANGLE_CONTEXT_TRY(prepareForDraw());
    ANGLE_CONTEXT_TRY(mImplementation->drawArraysIndirect(this, mode, indirect));
}

void Context::drawElementsIndirect(GLenum mode, GLenum type, const void *indirect)
{
    ANGLE_CONTEXT_TRY(prepareForDraw());
    ANGLE_CONTEXT_TRY(mImplementation->drawElementsIndirect(this, mode, type, indirect));
}

void Context::flush()
{
    handleError(mImplementation->flush(this));
}

void Context::finish()
{
    handleError(mImplementation->finish(this));
}

void Context::insertEventMarker(GLsizei length, const char *marker)
{
    ASSERT(mImplementation);
    mImplementation->insertEventMarker(length, marker);
}

void Context::pushGroupMarker(GLsizei length, const char *marker)
{
    ASSERT(mImplementation);
    mImplementation->pushGroupMarker(length, marker);
}

void Context::popGroupMarker()
{
    ASSERT(mImplementation);
    mImplementation->popGroupMarker();
}

void Context::bindUniformLocation(GLuint program, GLint location, const GLchar *name)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);

    programObject->bindUniformLocation(location, name);
}

void Context::setCoverageModulation(GLenum components)
{
    mGLState.setCoverageModulation(components);
}

void Context::loadPathRenderingMatrix(GLenum matrixMode, const GLfloat *matrix)
{
    mGLState.loadPathRenderingMatrix(matrixMode, matrix);
}

void Context::loadPathRenderingIdentityMatrix(GLenum matrixMode)
{
    GLfloat I[16];
    angle::Matrix<GLfloat>::setToIdentity(I);

    mGLState.loadPathRenderingMatrix(matrixMode, I);
}

void Context::stencilFillPath(GLuint path, GLenum fillMode, GLuint mask)
{
    const auto *pathObj = mState.mPaths->getPath(path);
    if (!pathObj)
        return;

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    syncRendererState();

    mImplementation->stencilFillPath(pathObj, fillMode, mask);
}

void Context::stencilStrokePath(GLuint path, GLint reference, GLuint mask)
{
    const auto *pathObj = mState.mPaths->getPath(path);
    if (!pathObj)
        return;

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    syncRendererState();

    mImplementation->stencilStrokePath(pathObj, reference, mask);
}

void Context::coverFillPath(GLuint path, GLenum coverMode)
{
    const auto *pathObj = mState.mPaths->getPath(path);
    if (!pathObj)
        return;

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    syncRendererState();

    mImplementation->coverFillPath(pathObj, coverMode);
}

void Context::coverStrokePath(GLuint path, GLenum coverMode)
{
    const auto *pathObj = mState.mPaths->getPath(path);
    if (!pathObj)
        return;

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    syncRendererState();

    mImplementation->coverStrokePath(pathObj, coverMode);
}

void Context::stencilThenCoverFillPath(GLuint path, GLenum fillMode, GLuint mask, GLenum coverMode)
{
    const auto *pathObj = mState.mPaths->getPath(path);
    if (!pathObj)
        return;

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    syncRendererState();

    mImplementation->stencilThenCoverFillPath(pathObj, fillMode, mask, coverMode);
}

void Context::stencilThenCoverStrokePath(GLuint path,
                                         GLint reference,
                                         GLuint mask,
                                         GLenum coverMode)
{
    const auto *pathObj = mState.mPaths->getPath(path);
    if (!pathObj)
        return;

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    syncRendererState();

    mImplementation->stencilThenCoverStrokePath(pathObj, reference, mask, coverMode);
}

void Context::coverFillPathInstanced(GLsizei numPaths,
                                     GLenum pathNameType,
                                     const void *paths,
                                     GLuint pathBase,
                                     GLenum coverMode,
                                     GLenum transformType,
                                     const GLfloat *transformValues)
{
    const auto &pathObjects = GatherPaths(*mState.mPaths, numPaths, pathNameType, paths, pathBase);

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    syncRendererState();

    mImplementation->coverFillPathInstanced(pathObjects, coverMode, transformType, transformValues);
}

void Context::coverStrokePathInstanced(GLsizei numPaths,
                                       GLenum pathNameType,
                                       const void *paths,
                                       GLuint pathBase,
                                       GLenum coverMode,
                                       GLenum transformType,
                                       const GLfloat *transformValues)
{
    const auto &pathObjects = GatherPaths(*mState.mPaths, numPaths, pathNameType, paths, pathBase);

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    syncRendererState();

    mImplementation->coverStrokePathInstanced(pathObjects, coverMode, transformType,
                                              transformValues);
}

void Context::stencilFillPathInstanced(GLsizei numPaths,
                                       GLenum pathNameType,
                                       const void *paths,
                                       GLuint pathBase,
                                       GLenum fillMode,
                                       GLuint mask,
                                       GLenum transformType,
                                       const GLfloat *transformValues)
{
    const auto &pathObjects = GatherPaths(*mState.mPaths, numPaths, pathNameType, paths, pathBase);

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    syncRendererState();

    mImplementation->stencilFillPathInstanced(pathObjects, fillMode, mask, transformType,
                                              transformValues);
}

void Context::stencilStrokePathInstanced(GLsizei numPaths,
                                         GLenum pathNameType,
                                         const void *paths,
                                         GLuint pathBase,
                                         GLint reference,
                                         GLuint mask,
                                         GLenum transformType,
                                         const GLfloat *transformValues)
{
    const auto &pathObjects = GatherPaths(*mState.mPaths, numPaths, pathNameType, paths, pathBase);

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    syncRendererState();

    mImplementation->stencilStrokePathInstanced(pathObjects, reference, mask, transformType,
                                                transformValues);
}

void Context::stencilThenCoverFillPathInstanced(GLsizei numPaths,
                                                GLenum pathNameType,
                                                const void *paths,
                                                GLuint pathBase,
                                                GLenum fillMode,
                                                GLuint mask,
                                                GLenum coverMode,
                                                GLenum transformType,
                                                const GLfloat *transformValues)
{
    const auto &pathObjects = GatherPaths(*mState.mPaths, numPaths, pathNameType, paths, pathBase);

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    syncRendererState();

    mImplementation->stencilThenCoverFillPathInstanced(pathObjects, coverMode, fillMode, mask,
                                                       transformType, transformValues);
}

void Context::stencilThenCoverStrokePathInstanced(GLsizei numPaths,
                                                  GLenum pathNameType,
                                                  const void *paths,
                                                  GLuint pathBase,
                                                  GLint reference,
                                                  GLuint mask,
                                                  GLenum coverMode,
                                                  GLenum transformType,
                                                  const GLfloat *transformValues)
{
    const auto &pathObjects = GatherPaths(*mState.mPaths, numPaths, pathNameType, paths, pathBase);

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    syncRendererState();

    mImplementation->stencilThenCoverStrokePathInstanced(pathObjects, coverMode, reference, mask,
                                                         transformType, transformValues);
}

void Context::bindFragmentInputLocation(GLuint program, GLint location, const GLchar *name)
{
    auto *programObject = getProgram(program);

    programObject->bindFragmentInputLocation(location, name);
}

void Context::programPathFragmentInputGen(GLuint program,
                                          GLint location,
                                          GLenum genMode,
                                          GLint components,
                                          const GLfloat *coeffs)
{
    auto *programObject = getProgram(program);

    programObject->pathFragmentInputGen(this, location, genMode, components, coeffs);
}

GLuint Context::getProgramResourceIndex(GLuint program, GLenum programInterface, const GLchar *name)
{
    const auto *programObject = getProgram(program);
    return QueryProgramResourceIndex(programObject, programInterface, name);
}

void Context::getProgramResourceName(GLuint program,
                                     GLenum programInterface,
                                     GLuint index,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLchar *name)
{
    const auto *programObject = getProgram(program);
    QueryProgramResourceName(programObject, programInterface, index, bufSize, length, name);
}

GLint Context::getProgramResourceLocation(GLuint program,
                                          GLenum programInterface,
                                          const GLchar *name)
{
    const auto *programObject = getProgram(program);
    return QueryProgramResourceLocation(programObject, programInterface, name);
}

void Context::getProgramResourceiv(GLuint program,
                                   GLenum programInterface,
                                   GLuint index,
                                   GLsizei propCount,
                                   const GLenum *props,
                                   GLsizei bufSize,
                                   GLsizei *length,
                                   GLint *params)
{
    const auto *programObject = getProgram(program);
    QueryProgramResourceiv(programObject, programInterface, index, propCount, props, bufSize,
                           length, params);
}

void Context::getProgramInterfaceiv(GLuint program,
                                    GLenum programInterface,
                                    GLenum pname,
                                    GLint *params)
{
    const auto *programObject = getProgram(program);
    QueryProgramInterfaceiv(programObject, programInterface, pname, params);
}

void Context::handleError(const Error &error)
{
    if (error.isError())
    {
        GLenum code = error.getCode();
        mErrors.insert(code);
        if (code == GL_OUT_OF_MEMORY && getWorkarounds().loseContextOnOutOfMemory)
        {
            markContextLost();
        }

        ASSERT(!error.getMessage().empty());
        mGLState.getDebug().insertMessage(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, error.getID(),
                                          GL_DEBUG_SEVERITY_HIGH, error.getMessage());
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

// NOTE: this function should not assume that this context is current!
void Context::markContextLost()
{
    if (mResetStrategy == GL_LOSE_CONTEXT_ON_RESET_EXT)
    {
        mResetStatus       = GL_UNKNOWN_CONTEXT_RESET_EXT;
        mContextLostForced = true;
    }
    mContextLost = true;
}

bool Context::isContextLost()
{
    return mContextLost;
}

GLenum Context::getResetStatus()
{
    // Even if the application doesn't want to know about resets, we want to know
    // as it will allow us to skip all the calls.
    if (mResetStrategy == GL_NO_RESET_NOTIFICATION_EXT)
    {
        if (!mContextLost && mImplementation->getResetStatus() != GL_NO_ERROR)
        {
            mContextLost = true;
        }

        // EXT_robustness, section 2.6: If the reset notification behavior is
        // NO_RESET_NOTIFICATION_EXT, then the implementation will never deliver notification of
        // reset events, and GetGraphicsResetStatusEXT will always return NO_ERROR.
        return GL_NO_ERROR;
    }

    // The GL_EXT_robustness spec says that if a reset is encountered, a reset
    // status should be returned at least once, and GL_NO_ERROR should be returned
    // once the device has finished resetting.
    if (!mContextLost)
    {
        ASSERT(mResetStatus == GL_NO_ERROR);
        mResetStatus = mImplementation->getResetStatus();

        if (mResetStatus != GL_NO_ERROR)
        {
            mContextLost = true;
        }
    }
    else if (!mContextLostForced && mResetStatus != GL_NO_ERROR)
    {
        // If markContextLost was used to mark the context lost then
        // assume that is not recoverable, and continue to report the
        // lost reset status for the lifetime of this context.
        mResetStatus = mImplementation->getResetStatus();
    }

    return mResetStatus;
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
    const Framebuffer *framebuffer = mState.mFramebuffers->getFramebuffer(0);
    if (framebuffer == nullptr)
    {
        return EGL_NONE;
    }

    const FramebufferAttachment *backAttachment = framebuffer->getAttachment(GL_BACK);
    ASSERT(backAttachment != nullptr);
    return backAttachment->getSurface()->getRenderBuffer();
}

VertexArray *Context::checkVertexArrayAllocation(GLuint vertexArrayHandle)
{
    // Only called after a prior call to Gen.
    VertexArray *vertexArray = getVertexArray(vertexArrayHandle);
    if (!vertexArray)
    {
        vertexArray = new VertexArray(mImplementation.get(), vertexArrayHandle,
                                      mCaps.maxVertexAttributes, mCaps.maxVertexAttribBindings);

        mVertexArrayMap.assign(vertexArrayHandle, vertexArray);
    }

    return vertexArray;
}

TransformFeedback *Context::checkTransformFeedbackAllocation(GLuint transformFeedbackHandle)
{
    // Only called after a prior call to Gen.
    TransformFeedback *transformFeedback = getTransformFeedback(transformFeedbackHandle);
    if (!transformFeedback)
    {
        transformFeedback =
            new TransformFeedback(mImplementation.get(), transformFeedbackHandle, mCaps);
        transformFeedback->addRef();
        mTransformFeedbackMap.assign(transformFeedbackHandle, transformFeedback);
    }

    return transformFeedback;
}

bool Context::isVertexArrayGenerated(GLuint vertexArray)
{
    ASSERT(mVertexArrayMap.contains(0));
    return mVertexArrayMap.contains(vertexArray);
}

bool Context::isTransformFeedbackGenerated(GLuint transformFeedback)
{
    ASSERT(mTransformFeedbackMap.contains(0));
    return mTransformFeedbackMap.contains(transformFeedback);
}

void Context::detachTexture(GLuint texture)
{
    // Simple pass-through to State's detachTexture method, as textures do not require
    // allocation map management either here or in the resource manager at detach time.
    // Zero textures are held by the Context, and we don't attempt to request them from
    // the State.
    mGLState.detachTexture(this, mZeroTextures, texture);
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
    mGLState.detachBuffer(this, buffer);
}

void Context::detachFramebuffer(GLuint framebuffer)
{
    // Framebuffer detachment is handled by Context, because 0 is a valid
    // Framebuffer object, and a pointer to it must be passed from Context
    // to State at binding time.

    // [OpenGL ES 2.0.24] section 4.4 page 107:
    // If a framebuffer that is currently bound to the target FRAMEBUFFER is deleted, it is as
    // though BindFramebuffer had been executed with the target of FRAMEBUFFER and framebuffer of
    // zero.

    if (mGLState.removeReadFramebufferBinding(framebuffer) && framebuffer != 0)
    {
        bindReadFramebuffer(0);
    }

    if (mGLState.removeDrawFramebufferBinding(framebuffer) && framebuffer != 0)
    {
        bindDrawFramebuffer(0);
    }
}

void Context::detachRenderbuffer(GLuint renderbuffer)
{
    mGLState.detachRenderbuffer(this, renderbuffer);
}

void Context::detachVertexArray(GLuint vertexArray)
{
    // Vertex array detachment is handled by Context, because 0 is a valid
    // VAO, and a pointer to it must be passed from Context to State at
    // binding time.

    // [OpenGL ES 3.0.2] section 2.10 page 43:
    // If a vertex array object that is currently bound is deleted, the binding
    // for that object reverts to zero and the default vertex array becomes current.
    if (mGLState.removeVertexArrayBinding(vertexArray))
    {
        bindVertexArray(0);
    }
}

void Context::detachTransformFeedback(GLuint transformFeedback)
{
    // Transform feedback detachment is handled by Context, because 0 is a valid
    // transform feedback, and a pointer to it must be passed from Context to State at
    // binding time.

    // The OpenGL specification doesn't mention what should happen when the currently bound
    // transform feedback object is deleted. Since it is a container object, we treat it like
    // VAOs and FBOs and set the current bound transform feedback back to 0.
    if (mGLState.removeTransformFeedbackBinding(this, transformFeedback))
    {
        bindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    }
}

void Context::detachSampler(GLuint sampler)
{
    mGLState.detachSampler(this, sampler);
}

void Context::detachProgramPipeline(GLuint pipeline)
{
    mGLState.detachProgramPipeline(this, pipeline);
}

void Context::vertexAttribDivisor(GLuint index, GLuint divisor)
{
    mGLState.setVertexAttribDivisor(this, index, divisor);
}

void Context::samplerParameteri(GLuint sampler, GLenum pname, GLint param)
{
    Sampler *samplerObject =
        mState.mSamplers->checkSamplerAllocation(mImplementation.get(), sampler);
    SetSamplerParameteri(samplerObject, pname, param);
    mGLState.setObjectDirty(GL_SAMPLER);
}

void Context::samplerParameteriv(GLuint sampler, GLenum pname, const GLint *param)
{
    Sampler *samplerObject =
        mState.mSamplers->checkSamplerAllocation(mImplementation.get(), sampler);
    SetSamplerParameteriv(samplerObject, pname, param);
    mGLState.setObjectDirty(GL_SAMPLER);
}

void Context::samplerParameterf(GLuint sampler, GLenum pname, GLfloat param)
{
    Sampler *samplerObject =
        mState.mSamplers->checkSamplerAllocation(mImplementation.get(), sampler);
    SetSamplerParameterf(samplerObject, pname, param);
    mGLState.setObjectDirty(GL_SAMPLER);
}

void Context::samplerParameterfv(GLuint sampler, GLenum pname, const GLfloat *param)
{
    Sampler *samplerObject =
        mState.mSamplers->checkSamplerAllocation(mImplementation.get(), sampler);
    SetSamplerParameterfv(samplerObject, pname, param);
    mGLState.setObjectDirty(GL_SAMPLER);
}

void Context::getSamplerParameteriv(GLuint sampler, GLenum pname, GLint *params)
{
    const Sampler *samplerObject =
        mState.mSamplers->checkSamplerAllocation(mImplementation.get(), sampler);
    QuerySamplerParameteriv(samplerObject, pname, params);
    mGLState.setObjectDirty(GL_SAMPLER);
}

void Context::getSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat *params)
{
    const Sampler *samplerObject =
        mState.mSamplers->checkSamplerAllocation(mImplementation.get(), sampler);
    QuerySamplerParameterfv(samplerObject, pname, params);
    mGLState.setObjectDirty(GL_SAMPLER);
}

void Context::programParameteri(GLuint program, GLenum pname, GLint value)
{
    gl::Program *programObject = getProgram(program);
    SetProgramParameteri(programObject, pname, value);
}

void Context::initRendererString()
{
    std::ostringstream rendererString;
    rendererString << "ANGLE (";
    rendererString << mImplementation->getRendererDescription();
    rendererString << ")";

    mRendererString = MakeStaticString(rendererString.str());
}

void Context::initVersionStrings()
{
    const Version &clientVersion = getClientVersion();

    std::ostringstream versionString;
    versionString << "OpenGL ES " << clientVersion.major << "." << clientVersion.minor << " (ANGLE "
                  << ANGLE_VERSION_STRING << ")";
    mVersionString = MakeStaticString(versionString.str());

    std::ostringstream shadingLanguageVersionString;
    shadingLanguageVersionString << "OpenGL ES GLSL ES "
                                 << (clientVersion.major == 2 ? 1 : clientVersion.major) << "."
                                 << clientVersion.minor << "0 (ANGLE " << ANGLE_VERSION_STRING
                                 << ")";
    mShadingLanguageString = MakeStaticString(shadingLanguageVersionString.str());
}

void Context::initExtensionStrings()
{
    auto mergeExtensionStrings = [](const std::vector<const char *> &strings) {
        std::ostringstream combinedStringStream;
        std::copy(strings.begin(), strings.end(),
                  std::ostream_iterator<const char *>(combinedStringStream, " "));
        return MakeStaticString(combinedStringStream.str());
    };

    mExtensionStrings.clear();
    for (const auto &extensionString : mExtensions.getStrings())
    {
        mExtensionStrings.push_back(MakeStaticString(extensionString));
    }
    mExtensionString = mergeExtensionStrings(mExtensionStrings);

    const gl::Extensions &nativeExtensions = mImplementation->getNativeExtensions();

    mRequestableExtensionStrings.clear();
    for (const auto &extensionInfo : GetExtensionInfoMap())
    {
        if (extensionInfo.second.Requestable &&
            !(mExtensions.*(extensionInfo.second.ExtensionsMember)) &&
            nativeExtensions.*(extensionInfo.second.ExtensionsMember))
        {
            mRequestableExtensionStrings.push_back(MakeStaticString(extensionInfo.first));
        }
    }
    mRequestableExtensionString = mergeExtensionStrings(mRequestableExtensionStrings);
}

const GLubyte *Context::getString(GLenum name) const
{
    switch (name)
    {
        case GL_VENDOR:
            return reinterpret_cast<const GLubyte *>("Google Inc.");

        case GL_RENDERER:
            return reinterpret_cast<const GLubyte *>(mRendererString);

        case GL_VERSION:
            return reinterpret_cast<const GLubyte *>(mVersionString);

        case GL_SHADING_LANGUAGE_VERSION:
            return reinterpret_cast<const GLubyte *>(mShadingLanguageString);

        case GL_EXTENSIONS:
            return reinterpret_cast<const GLubyte *>(mExtensionString);

        case GL_REQUESTABLE_EXTENSIONS_ANGLE:
            return reinterpret_cast<const GLubyte *>(mRequestableExtensionString);

        default:
            UNREACHABLE();
            return nullptr;
    }
}

const GLubyte *Context::getStringi(GLenum name, GLuint index) const
{
    switch (name)
    {
        case GL_EXTENSIONS:
            return reinterpret_cast<const GLubyte *>(mExtensionStrings[index]);

        case GL_REQUESTABLE_EXTENSIONS_ANGLE:
            return reinterpret_cast<const GLubyte *>(mRequestableExtensionStrings[index]);

        default:
            UNREACHABLE();
            return nullptr;
    }
}

size_t Context::getExtensionStringCount() const
{
    return mExtensionStrings.size();
}

bool Context::isExtensionRequestable(const char *name)
{
    const ExtensionInfoMap &extensionInfos = GetExtensionInfoMap();
    auto extension                         = extensionInfos.find(name);

    const Extensions &nativeExtensions = mImplementation->getNativeExtensions();
    return extension != extensionInfos.end() && extension->second.Requestable &&
           nativeExtensions.*(extension->second.ExtensionsMember);
}

void Context::requestExtension(const char *name)
{
    const ExtensionInfoMap &extensionInfos = GetExtensionInfoMap();
    ASSERT(extensionInfos.find(name) != extensionInfos.end());
    const auto &extension = extensionInfos.at(name);
    ASSERT(extension.Requestable);
    ASSERT(mImplementation->getNativeExtensions().*(extension.ExtensionsMember));

    if (mExtensions.*(extension.ExtensionsMember))
    {
        // Extension already enabled
        return;
    }

    mExtensions.*(extension.ExtensionsMember) = true;
    updateCaps();
    initExtensionStrings();

    // Release the shader compiler so it will be re-created with the requested extensions enabled.
    releaseShaderCompiler();

    // Invalidate all textures and framebuffer. Some extensions make new formats renderable or
    // sampleable.
    mState.mTextures->signalAllTexturesDirty();
    for (auto &zeroTexture : mZeroTextures)
    {
        zeroTexture.second->signalDirty(InitState::Initialized);
    }

    mState.mFramebuffers->invalidateFramebufferComplenessCache();
}

size_t Context::getRequestableExtensionStringCount() const
{
    return mRequestableExtensionStrings.size();
}

void Context::beginTransformFeedback(GLenum primitiveMode)
{
    TransformFeedback *transformFeedback = mGLState.getCurrentTransformFeedback();
    ASSERT(transformFeedback != nullptr);
    ASSERT(!transformFeedback->isPaused());

    transformFeedback->begin(this, primitiveMode, mGLState.getProgram());
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

void Context::initCaps(const egl::DisplayExtensions &displayExtensions, bool robustResourceInit)
{
    mCaps = mImplementation->getNativeCaps();

    mExtensions = mImplementation->getNativeExtensions();

    mLimitations = mImplementation->getNativeLimitations();

    if (getClientVersion() < Version(3, 0))
    {
        // Disable ES3+ extensions
        mExtensions.colorBufferFloat      = false;
        mExtensions.eglImageExternalEssl3 = false;
        mExtensions.textureNorm16         = false;
        mExtensions.multiview             = false;
        mExtensions.maxViews              = 1u;
    }

    if (getClientVersion() < ES_3_1)
    {
        // Disable ES3.1+ extensions
        mExtensions.geometryShader = false;
    }

    if (getClientVersion() > Version(2, 0))
    {
        // FIXME(geofflang): Don't support EXT_sRGB in non-ES2 contexts
        // mExtensions.sRGB = false;
    }

    // Some extensions are always available because they are implemented in the GL layer.
    mExtensions.bindUniformLocation   = true;
    mExtensions.vertexArrayObject     = true;
    mExtensions.bindGeneratesResource = true;
    mExtensions.clientArrays          = true;
    mExtensions.requestExtension      = true;

    // Enable the no error extension if the context was created with the flag.
    mExtensions.noError = mSkipValidation;

    // Enable surfaceless to advertise we'll have the correct behavior when there is no default FBO
    mExtensions.surfacelessContext = displayExtensions.surfacelessContext;

    // Explicitly enable GL_KHR_debug
    mExtensions.debug                   = true;
    mExtensions.maxDebugMessageLength   = 1024;
    mExtensions.maxDebugLoggedMessages  = 1024;
    mExtensions.maxDebugGroupStackDepth = 1024;
    mExtensions.maxLabelLength          = 1024;

    // Explicitly enable GL_ANGLE_robust_client_memory
    mExtensions.robustClientMemory = true;

    // Determine robust resource init availability from EGL.
    mExtensions.robustResourceInitialization = robustResourceInit;

    // mExtensions.robustBufferAccessBehavior is true only if robust access is true and the backend
    // supports it.
    mExtensions.robustBufferAccessBehavior =
        mRobustAccess && mExtensions.robustBufferAccessBehavior;

    // Enable the cache control query unconditionally.
    mExtensions.programCacheControl = true;

    // Apply implementation limits
    LimitCap(&mCaps.maxVertexAttributes, MAX_VERTEX_ATTRIBS);

    if (getClientVersion() < ES_3_1)
    {
        mCaps.maxVertexAttribBindings = mCaps.maxVertexAttributes;
    }
    else
    {
        LimitCap(&mCaps.maxVertexAttribBindings, MAX_VERTEX_ATTRIB_BINDINGS);
    }

    LimitCap(&mCaps.maxVertexUniformBlocks, IMPLEMENTATION_MAX_VERTEX_SHADER_UNIFORM_BUFFERS);
    LimitCap(&mCaps.maxVertexOutputComponents, IMPLEMENTATION_MAX_VARYING_VECTORS * 4);
    LimitCap(&mCaps.maxFragmentInputComponents, IMPLEMENTATION_MAX_VARYING_VECTORS * 4);

    // Limit textures as well, so we can use fast bitsets with texture bindings.
    LimitCap(&mCaps.maxCombinedTextureImageUnits, IMPLEMENTATION_MAX_ACTIVE_TEXTURES);
    LimitCap(&mCaps.maxVertexTextureImageUnits, IMPLEMENTATION_MAX_ACTIVE_TEXTURES / 2);
    LimitCap(&mCaps.maxTextureImageUnits, IMPLEMENTATION_MAX_ACTIVE_TEXTURES / 2);

    mCaps.maxSampleMaskWords = std::min<GLuint>(mCaps.maxSampleMaskWords, MAX_SAMPLE_MASK_WORDS);

    // WebGL compatibility
    mExtensions.webglCompatibility = mWebGLContext;
    for (const auto &extensionInfo : GetExtensionInfoMap())
    {
        // If this context is for WebGL, disable all enableable extensions
        if (mWebGLContext && extensionInfo.second.Requestable)
        {
            mExtensions.*(extensionInfo.second.ExtensionsMember) = false;
        }
    }

    // Generate texture caps
    updateCaps();
}

void Context::updateCaps()
{
    mCaps.compressedTextureFormats.clear();
    mTextureCaps.clear();

    for (GLenum sizedInternalFormat : GetAllSizedInternalFormats())
    {
        TextureCaps formatCaps = mImplementation->getNativeTextureCaps().get(sizedInternalFormat);
        const InternalFormat &formatInfo = GetSizedInternalFormatInfo(sizedInternalFormat);

        // Update the format caps based on the client version and extensions.
        // Caps are AND'd with the renderer caps because some core formats are still unsupported in
        // ES3.
        formatCaps.texturable =
            formatCaps.texturable && formatInfo.textureSupport(getClientVersion(), mExtensions);
        formatCaps.renderable =
            formatCaps.renderable && formatInfo.renderSupport(getClientVersion(), mExtensions);
        formatCaps.filterable =
            formatCaps.filterable && formatInfo.filterSupport(getClientVersion(), mExtensions);

        // OpenGL ES does not support multisampling with non-rendererable formats
        // OpenGL ES 3.0 or prior does not support multisampling with integer formats
        if (!formatCaps.renderable ||
            (getClientVersion() < ES_3_1 &&
             (formatInfo.componentType == GL_INT || formatInfo.componentType == GL_UNSIGNED_INT)))
        {
            formatCaps.sampleCounts.clear();
        }
        else
        {
            // We may have limited the max samples for some required renderbuffer formats due to
            // non-conformant formats. In this case MAX_SAMPLES needs to be lowered accordingly.
            GLuint formatMaxSamples = formatCaps.getMaxSamples();

            // GLES 3.0.5 section 4.4.2.2: "Implementations must support creation of renderbuffers
            // in these required formats with up to the value of MAX_SAMPLES multisamples, with the
            // exception of signed and unsigned integer formats."
            if (formatInfo.componentType != GL_INT && formatInfo.componentType != GL_UNSIGNED_INT &&
                formatInfo.isRequiredRenderbufferFormat(getClientVersion()))
            {
                ASSERT(getClientVersion() < ES_3_0 || formatMaxSamples >= 4);
                mCaps.maxSamples = std::min(mCaps.maxSamples, formatMaxSamples);
            }

            // Handle GLES 3.1 MAX_*_SAMPLES values similarly to MAX_SAMPLES.
            if (getClientVersion() >= ES_3_1)
            {
                // GLES 3.1 section 9.2.5: "Implementations must support creation of renderbuffers
                // in these required formats with up to the value of MAX_SAMPLES multisamples, with
                // the exception that the signed and unsigned integer formats are required only to
                // support creation of renderbuffers with up to the value of MAX_INTEGER_SAMPLES
                // multisamples, which must be at least one."
                if (formatInfo.componentType == GL_INT ||
                    formatInfo.componentType == GL_UNSIGNED_INT)
                {
                    mCaps.maxIntegerSamples = std::min(mCaps.maxIntegerSamples, formatMaxSamples);
                }

                // GLES 3.1 section 19.3.1.
                if (formatCaps.texturable)
                {
                    if (formatInfo.depthBits > 0)
                    {
                        mCaps.maxDepthTextureSamples =
                            std::min(mCaps.maxDepthTextureSamples, formatMaxSamples);
                    }
                    else if (formatInfo.redBits > 0)
                    {
                        mCaps.maxColorTextureSamples =
                            std::min(mCaps.maxColorTextureSamples, formatMaxSamples);
                    }
                }
            }
        }

        if (formatCaps.texturable && formatInfo.compressed)
        {
            mCaps.compressedTextureFormats.push_back(sizedInternalFormat);
        }

        mTextureCaps.insert(sizedInternalFormat, formatCaps);
    }

    // If program binary is disabled, blank out the memory cache pointer.
    if (!mImplementation->getNativeExtensions().getProgramBinary)
    {
        mMemoryProgramCache = nullptr;
    }
}

void Context::initWorkarounds()
{
    // Apply back-end workarounds.
    mImplementation->applyNativeWorkarounds(&mWorkarounds);

    // Lose the context upon out of memory error if the application is
    // expecting to watch for those events.
    mWorkarounds.loseContextOnOutOfMemory = (mResetStrategy == GL_LOSE_CONTEXT_ON_RESET_EXT);
}

Error Context::prepareForDraw()
{
    syncRendererState();

    if (isRobustResourceInitEnabled())
    {
        ANGLE_TRY(mGLState.clearUnclearedActiveTextures(this));
        ANGLE_TRY(mGLState.getDrawFramebuffer()->ensureDrawAttachmentsInitialized(this));
    }

    return NoError();
}

void Context::syncRendererState()
{
    mGLState.syncDirtyObjects(this);
    const State::DirtyBits &dirtyBits = mGLState.getDirtyBits();
    mImplementation->syncState(this, dirtyBits);
    mGLState.clearDirtyBits();
}

void Context::syncRendererState(const State::DirtyBits &bitMask,
                                const State::DirtyObjects &objectMask)
{
    mGLState.syncDirtyObjects(this, objectMask);
    const State::DirtyBits &dirtyBits = (mGLState.getDirtyBits() & bitMask);
    mImplementation->syncState(this, dirtyBits);
    mGLState.clearDirtyBits(dirtyBits);
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
    Framebuffer *drawFramebuffer = mGLState.getDrawFramebuffer();
    ASSERT(drawFramebuffer);

    Rectangle srcArea(srcX0, srcY0, srcX1 - srcX0, srcY1 - srcY0);
    Rectangle dstArea(dstX0, dstY0, dstX1 - dstX0, dstY1 - dstY0);

    syncStateForBlit();

    handleError(drawFramebuffer->blit(this, srcArea, dstArea, mask, filter));
}

void Context::clear(GLbitfield mask)
{
    syncStateForClear();
    handleError(mGLState.getDrawFramebuffer()->clear(this, mask));
}

void Context::clearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *values)
{
    syncStateForClear();
    handleError(mGLState.getDrawFramebuffer()->clearBufferfv(this, buffer, drawbuffer, values));
}

void Context::clearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *values)
{
    syncStateForClear();
    handleError(mGLState.getDrawFramebuffer()->clearBufferuiv(this, buffer, drawbuffer, values));
}

void Context::clearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *values)
{
    syncStateForClear();
    handleError(mGLState.getDrawFramebuffer()->clearBufferiv(this, buffer, drawbuffer, values));
}

void Context::clearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
    Framebuffer *framebufferObject = mGLState.getDrawFramebuffer();
    ASSERT(framebufferObject);

    // If a buffer is not present, the clear has no effect
    if (framebufferObject->getDepthbuffer() == nullptr &&
        framebufferObject->getStencilbuffer() == nullptr)
    {
        return;
    }

    syncStateForClear();
    handleError(framebufferObject->clearBufferfi(this, buffer, drawbuffer, depth, stencil));
}

void Context::readPixels(GLint x,
                         GLint y,
                         GLsizei width,
                         GLsizei height,
                         GLenum format,
                         GLenum type,
                         void *pixels)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    syncStateForReadPixels();

    Framebuffer *readFBO = mGLState.getReadFramebuffer();
    ASSERT(readFBO);

    Rectangle area(x, y, width, height);
    handleError(readFBO->readPixels(this, area, format, type, pixels));
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
    mGLState.syncDirtyObject(this, GL_READ_FRAMEBUFFER);

    Rectangle sourceArea(x, y, width, height);

    Framebuffer *framebuffer = mGLState.getReadFramebuffer();
    Texture *texture =
        getTargetTexture(IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target);
    handleError(texture->copyImage(this, target, level, sourceArea, internalformat, framebuffer));
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
    if (width == 0 || height == 0)
    {
        return;
    }

    // Only sync the read FBO
    mGLState.syncDirtyObject(this, GL_READ_FRAMEBUFFER);

    Offset destOffset(xoffset, yoffset, 0);
    Rectangle sourceArea(x, y, width, height);

    Framebuffer *framebuffer = mGLState.getReadFramebuffer();
    Texture *texture =
        getTargetTexture(IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target);
    handleError(texture->copySubImage(this, target, level, destOffset, sourceArea, framebuffer));
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
    if (width == 0 || height == 0)
    {
        return;
    }

    // Only sync the read FBO
    mGLState.syncDirtyObject(this, GL_READ_FRAMEBUFFER);

    Offset destOffset(xoffset, yoffset, zoffset);
    Rectangle sourceArea(x, y, width, height);

    Framebuffer *framebuffer = mGLState.getReadFramebuffer();
    Texture *texture         = getTargetTexture(target);
    handleError(texture->copySubImage(this, target, level, destOffset, sourceArea, framebuffer));
}

void Context::framebufferTexture2D(GLenum target,
                                   GLenum attachment,
                                   GLenum textarget,
                                   GLuint texture,
                                   GLint level)
{
    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (texture != 0)
    {
        Texture *textureObj = getTexture(texture);

        ImageIndex index = ImageIndex::MakeInvalid();

        if (textarget == GL_TEXTURE_2D)
        {
            index = ImageIndex::Make2D(level);
        }
        else if (textarget == GL_TEXTURE_RECTANGLE_ANGLE)
        {
            index = ImageIndex::MakeRectangle(level);
        }
        else if (textarget == GL_TEXTURE_2D_MULTISAMPLE)
        {
            ASSERT(level == 0);
            index = ImageIndex::Make2DMultisample();
        }
        else
        {
            ASSERT(IsCubeMapTextureTarget(textarget));
            index = ImageIndex::MakeCube(textarget, level);
        }

        framebuffer->setAttachment(this, GL_TEXTURE, attachment, index, textureObj);
    }
    else
    {
        framebuffer->resetAttachment(this, attachment);
    }

    mGLState.setObjectDirty(target);
}

void Context::framebufferRenderbuffer(GLenum target,
                                      GLenum attachment,
                                      GLenum renderbuffertarget,
                                      GLuint renderbuffer)
{
    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (renderbuffer != 0)
    {
        Renderbuffer *renderbufferObject = getRenderbuffer(renderbuffer);

        framebuffer->setAttachment(this, GL_RENDERBUFFER, attachment, gl::ImageIndex::MakeInvalid(),
                                   renderbufferObject);
    }
    else
    {
        framebuffer->resetAttachment(this, attachment);
    }

    mGLState.setObjectDirty(target);
}

void Context::framebufferTextureLayer(GLenum target,
                                      GLenum attachment,
                                      GLuint texture,
                                      GLint level,
                                      GLint layer)
{
    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
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

        framebuffer->setAttachment(this, GL_TEXTURE, attachment, index, textureObject);
    }
    else
    {
        framebuffer->resetAttachment(this, attachment);
    }

    mGLState.setObjectDirty(target);
}

void Context::framebufferTextureMultiviewLayeredANGLE(GLenum target,
                                                      GLenum attachment,
                                                      GLuint texture,
                                                      GLint level,
                                                      GLint baseViewIndex,
                                                      GLsizei numViews)
{
    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (texture != 0)
    {
        Texture *textureObj = getTexture(texture);

        ImageIndex index = ImageIndex::Make2DArrayRange(level, baseViewIndex, numViews);
        framebuffer->setAttachmentMultiviewLayered(this, GL_TEXTURE, attachment, index, textureObj,
                                                   numViews, baseViewIndex);
    }
    else
    {
        framebuffer->resetAttachment(this, attachment);
    }

    mGLState.setObjectDirty(target);
}

void Context::framebufferTextureMultiviewSideBySideANGLE(GLenum target,
                                                         GLenum attachment,
                                                         GLuint texture,
                                                         GLint level,
                                                         GLsizei numViews,
                                                         const GLint *viewportOffsets)
{
    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (texture != 0)
    {
        Texture *textureObj = getTexture(texture);

        ImageIndex index = ImageIndex::Make2D(level);
        framebuffer->setAttachmentMultiviewSideBySide(this, GL_TEXTURE, attachment, index,
                                                      textureObj, numViews, viewportOffsets);
    }
    else
    {
        framebuffer->resetAttachment(this, attachment);
    }

    mGLState.setObjectDirty(target);
}

void Context::drawBuffers(GLsizei n, const GLenum *bufs)
{
    Framebuffer *framebuffer = mGLState.getDrawFramebuffer();
    ASSERT(framebuffer);
    framebuffer->setDrawBuffers(n, bufs);
    mGLState.setObjectDirty(GL_DRAW_FRAMEBUFFER);
}

void Context::readBuffer(GLenum mode)
{
    Framebuffer *readFBO = mGLState.getReadFramebuffer();
    readFBO->setReadBuffer(mode);
    mGLState.setObjectDirty(GL_READ_FRAMEBUFFER);
}

void Context::discardFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments)
{
    // Only sync the FBO
    mGLState.syncDirtyObject(this, target);

    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    // The specification isn't clear what should be done when the framebuffer isn't complete.
    // We leave it up to the framebuffer implementation to decide what to do.
    handleError(framebuffer->discard(this, numAttachments, attachments));
}

void Context::invalidateFramebuffer(GLenum target,
                                    GLsizei numAttachments,
                                    const GLenum *attachments)
{
    // Only sync the FBO
    mGLState.syncDirtyObject(this, target);

    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (framebuffer->checkStatus(this) != GL_FRAMEBUFFER_COMPLETE)
    {
        return;
    }

    handleError(framebuffer->invalidate(this, numAttachments, attachments));
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
    mGLState.syncDirtyObject(this, target);

    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (framebuffer->checkStatus(this) != GL_FRAMEBUFFER_COMPLETE)
    {
        return;
    }

    Rectangle area(x, y, width, height);
    handleError(framebuffer->invalidateSub(this, numAttachments, attachments, area));
}

void Context::texImage2D(GLenum target,
                         GLint level,
                         GLint internalformat,
                         GLsizei width,
                         GLsizei height,
                         GLint border,
                         GLenum format,
                         GLenum type,
                         const void *pixels)
{
    syncStateForTexImage();

    Extents size(width, height, 1);
    Texture *texture =
        getTargetTexture(IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target);
    handleError(texture->setImage(this, mGLState.getUnpackState(), target, level, internalformat,
                                  size, format, type, reinterpret_cast<const uint8_t *>(pixels)));
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
                         const void *pixels)
{
    syncStateForTexImage();

    Extents size(width, height, depth);
    Texture *texture = getTargetTexture(target);
    handleError(texture->setImage(this, mGLState.getUnpackState(), target, level, internalformat,
                                  size, format, type, reinterpret_cast<const uint8_t *>(pixels)));
}

void Context::texSubImage2D(GLenum target,
                            GLint level,
                            GLint xoffset,
                            GLint yoffset,
                            GLsizei width,
                            GLsizei height,
                            GLenum format,
                            GLenum type,
                            const void *pixels)
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
    handleError(texture->setSubImage(this, mGLState.getUnpackState(), target, level, area, format,
                                     type, reinterpret_cast<const uint8_t *>(pixels)));
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
                            const void *pixels)
{
    // Zero sized uploads are valid but no-ops
    if (width == 0 || height == 0 || depth == 0)
    {
        return;
    }

    syncStateForTexImage();

    Box area(xoffset, yoffset, zoffset, width, height, depth);
    Texture *texture = getTargetTexture(target);
    handleError(texture->setSubImage(this, mGLState.getUnpackState(), target, level, area, format,
                                     type, reinterpret_cast<const uint8_t *>(pixels)));
}

void Context::compressedTexImage2D(GLenum target,
                                   GLint level,
                                   GLenum internalformat,
                                   GLsizei width,
                                   GLsizei height,
                                   GLint border,
                                   GLsizei imageSize,
                                   const void *data)
{
    syncStateForTexImage();

    Extents size(width, height, 1);
    Texture *texture =
        getTargetTexture(IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target);
    handleError(texture->setCompressedImage(this, mGLState.getUnpackState(), target, level,
                                            internalformat, size, imageSize,
                                            reinterpret_cast<const uint8_t *>(data)));
}

void Context::compressedTexImage3D(GLenum target,
                                   GLint level,
                                   GLenum internalformat,
                                   GLsizei width,
                                   GLsizei height,
                                   GLsizei depth,
                                   GLint border,
                                   GLsizei imageSize,
                                   const void *data)
{
    syncStateForTexImage();

    Extents size(width, height, depth);
    Texture *texture = getTargetTexture(target);
    handleError(texture->setCompressedImage(this, mGLState.getUnpackState(), target, level,
                                            internalformat, size, imageSize,
                                            reinterpret_cast<const uint8_t *>(data)));
}

void Context::compressedTexSubImage2D(GLenum target,
                                      GLint level,
                                      GLint xoffset,
                                      GLint yoffset,
                                      GLsizei width,
                                      GLsizei height,
                                      GLenum format,
                                      GLsizei imageSize,
                                      const void *data)
{
    syncStateForTexImage();

    Box area(xoffset, yoffset, 0, width, height, 1);
    Texture *texture =
        getTargetTexture(IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target);
    handleError(texture->setCompressedSubImage(this, mGLState.getUnpackState(), target, level, area,
                                               format, imageSize,
                                               reinterpret_cast<const uint8_t *>(data)));
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
                                      const void *data)
{
    // Zero sized uploads are valid but no-ops
    if (width == 0 || height == 0)
    {
        return;
    }

    syncStateForTexImage();

    Box area(xoffset, yoffset, zoffset, width, height, depth);
    Texture *texture = getTargetTexture(target);
    handleError(texture->setCompressedSubImage(this, mGLState.getUnpackState(), target, level, area,
                                               format, imageSize,
                                               reinterpret_cast<const uint8_t *>(data)));
}

void Context::generateMipmap(GLenum target)
{
    Texture *texture = getTargetTexture(target);
    handleError(texture->generateMipmap(this));
}

void Context::copyTextureCHROMIUM(GLuint sourceId,
                                  GLint sourceLevel,
                                  GLenum destTarget,
                                  GLuint destId,
                                  GLint destLevel,
                                  GLint internalFormat,
                                  GLenum destType,
                                  GLboolean unpackFlipY,
                                  GLboolean unpackPremultiplyAlpha,
                                  GLboolean unpackUnmultiplyAlpha)
{
    syncStateForTexImage();

    gl::Texture *sourceTexture = getTexture(sourceId);
    gl::Texture *destTexture   = getTexture(destId);
    handleError(destTexture->copyTexture(this, destTarget, destLevel, internalFormat, destType,
                                         sourceLevel, ConvertToBool(unpackFlipY),
                                         ConvertToBool(unpackPremultiplyAlpha),
                                         ConvertToBool(unpackUnmultiplyAlpha), sourceTexture));
}

void Context::copySubTextureCHROMIUM(GLuint sourceId,
                                     GLint sourceLevel,
                                     GLenum destTarget,
                                     GLuint destId,
                                     GLint destLevel,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint x,
                                     GLint y,
                                     GLsizei width,
                                     GLsizei height,
                                     GLboolean unpackFlipY,
                                     GLboolean unpackPremultiplyAlpha,
                                     GLboolean unpackUnmultiplyAlpha)
{
    // Zero sized copies are valid but no-ops
    if (width == 0 || height == 0)
    {
        return;
    }

    syncStateForTexImage();

    gl::Texture *sourceTexture = getTexture(sourceId);
    gl::Texture *destTexture   = getTexture(destId);
    Offset offset(xoffset, yoffset, 0);
    Rectangle area(x, y, width, height);
    handleError(destTexture->copySubTexture(this, destTarget, destLevel, offset, sourceLevel, area,
                                            ConvertToBool(unpackFlipY),
                                            ConvertToBool(unpackPremultiplyAlpha),
                                            ConvertToBool(unpackUnmultiplyAlpha), sourceTexture));
}

void Context::compressedCopyTextureCHROMIUM(GLuint sourceId, GLuint destId)
{
    syncStateForTexImage();

    gl::Texture *sourceTexture = getTexture(sourceId);
    gl::Texture *destTexture   = getTexture(destId);
    handleError(destTexture->copyCompressedTexture(this, sourceTexture));
}

void Context::getBufferPointerv(BufferBinding target, GLenum pname, void **params)
{
    Buffer *buffer = mGLState.getTargetBuffer(target);
    ASSERT(buffer);

    QueryBufferPointerv(buffer, pname, params);
}

void *Context::mapBuffer(BufferBinding target, GLenum access)
{
    Buffer *buffer = mGLState.getTargetBuffer(target);
    ASSERT(buffer);

    Error error = buffer->map(this, access);
    if (error.isError())
    {
        handleError(error);
        return nullptr;
    }

    return buffer->getMapPointer();
}

GLboolean Context::unmapBuffer(BufferBinding target)
{
    Buffer *buffer = mGLState.getTargetBuffer(target);
    ASSERT(buffer);

    GLboolean result;
    Error error = buffer->unmap(this, &result);
    if (error.isError())
    {
        handleError(error);
        return GL_FALSE;
    }

    return result;
}

void *Context::mapBufferRange(BufferBinding target,
                              GLintptr offset,
                              GLsizeiptr length,
                              GLbitfield access)
{
    Buffer *buffer = mGLState.getTargetBuffer(target);
    ASSERT(buffer);

    Error error = buffer->mapRange(this, offset, length, access);
    if (error.isError())
    {
        handleError(error);
        return nullptr;
    }

    return buffer->getMapPointer();
}

void Context::flushMappedBufferRange(BufferBinding /*target*/,
                                     GLintptr /*offset*/,
                                     GLsizeiptr /*length*/)
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

void Context::activeShaderProgram(GLuint pipeline, GLuint program)
{
    UNIMPLEMENTED();
}

void Context::activeTexture(GLenum texture)
{
    mGLState.setActiveSampler(texture - GL_TEXTURE0);
}

void Context::blendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    mGLState.setBlendColor(clamp01(red), clamp01(green), clamp01(blue), clamp01(alpha));
}

void Context::blendEquation(GLenum mode)
{
    mGLState.setBlendEquation(mode, mode);
}

void Context::blendEquationSeparate(GLenum modeRGB, GLenum modeAlpha)
{
    mGLState.setBlendEquation(modeRGB, modeAlpha);
}

void Context::blendFunc(GLenum sfactor, GLenum dfactor)
{
    mGLState.setBlendFactors(sfactor, dfactor, sfactor, dfactor);
}

void Context::blendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
    mGLState.setBlendFactors(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void Context::clearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    mGLState.setColorClearValue(red, green, blue, alpha);
}

void Context::clearDepthf(GLfloat depth)
{
    mGLState.setDepthClearValue(depth);
}

void Context::clearStencil(GLint s)
{
    mGLState.setStencilClearValue(s);
}

void Context::colorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
    mGLState.setColorMask(ConvertToBool(red), ConvertToBool(green), ConvertToBool(blue),
                          ConvertToBool(alpha));
}

void Context::cullFace(CullFaceMode mode)
{
    mGLState.setCullMode(mode);
}

void Context::depthFunc(GLenum func)
{
    mGLState.setDepthFunc(func);
}

void Context::depthMask(GLboolean flag)
{
    mGLState.setDepthMask(ConvertToBool(flag));
}

void Context::depthRangef(GLfloat zNear, GLfloat zFar)
{
    mGLState.setDepthRange(zNear, zFar);
}

void Context::disable(GLenum cap)
{
    mGLState.setEnableFeature(cap, false);
}

void Context::disableVertexAttribArray(GLuint index)
{
    mGLState.setEnableVertexAttribArray(index, false);
}

void Context::enable(GLenum cap)
{
    mGLState.setEnableFeature(cap, true);
}

void Context::enableVertexAttribArray(GLuint index)
{
    mGLState.setEnableVertexAttribArray(index, true);
}

void Context::frontFace(GLenum mode)
{
    mGLState.setFrontFace(mode);
}

void Context::hint(GLenum target, GLenum mode)
{
    switch (target)
    {
        case GL_GENERATE_MIPMAP_HINT:
            mGLState.setGenerateMipmapHint(mode);
            break;

        case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:
            mGLState.setFragmentShaderDerivativeHint(mode);
            break;

        default:
            UNREACHABLE();
            return;
    }
}

void Context::lineWidth(GLfloat width)
{
    mGLState.setLineWidth(width);
}

void Context::pixelStorei(GLenum pname, GLint param)
{
    switch (pname)
    {
        case GL_UNPACK_ALIGNMENT:
            mGLState.setUnpackAlignment(param);
            break;

        case GL_PACK_ALIGNMENT:
            mGLState.setPackAlignment(param);
            break;

        case GL_PACK_REVERSE_ROW_ORDER_ANGLE:
            mGLState.setPackReverseRowOrder(param != 0);
            break;

        case GL_UNPACK_ROW_LENGTH:
            ASSERT((getClientMajorVersion() >= 3) || getExtensions().unpackSubimage);
            mGLState.setUnpackRowLength(param);
            break;

        case GL_UNPACK_IMAGE_HEIGHT:
            ASSERT(getClientMajorVersion() >= 3);
            mGLState.setUnpackImageHeight(param);
            break;

        case GL_UNPACK_SKIP_IMAGES:
            ASSERT(getClientMajorVersion() >= 3);
            mGLState.setUnpackSkipImages(param);
            break;

        case GL_UNPACK_SKIP_ROWS:
            ASSERT((getClientMajorVersion() >= 3) || getExtensions().unpackSubimage);
            mGLState.setUnpackSkipRows(param);
            break;

        case GL_UNPACK_SKIP_PIXELS:
            ASSERT((getClientMajorVersion() >= 3) || getExtensions().unpackSubimage);
            mGLState.setUnpackSkipPixels(param);
            break;

        case GL_PACK_ROW_LENGTH:
            ASSERT((getClientMajorVersion() >= 3) || getExtensions().packSubimage);
            mGLState.setPackRowLength(param);
            break;

        case GL_PACK_SKIP_ROWS:
            ASSERT((getClientMajorVersion() >= 3) || getExtensions().packSubimage);
            mGLState.setPackSkipRows(param);
            break;

        case GL_PACK_SKIP_PIXELS:
            ASSERT((getClientMajorVersion() >= 3) || getExtensions().packSubimage);
            mGLState.setPackSkipPixels(param);
            break;

        default:
            UNREACHABLE();
            return;
    }
}

void Context::polygonOffset(GLfloat factor, GLfloat units)
{
    mGLState.setPolygonOffsetParams(factor, units);
}

void Context::sampleCoverage(GLfloat value, GLboolean invert)
{
    mGLState.setSampleCoverageParams(clamp01(value), ConvertToBool(invert));
}

void Context::sampleMaski(GLuint maskNumber, GLbitfield mask)
{
    mGLState.setSampleMaskParams(maskNumber, mask);
}

void Context::scissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
    mGLState.setScissorParams(x, y, width, height);
}

void Context::stencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask)
{
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK)
    {
        mGLState.setStencilParams(func, ref, mask);
    }

    if (face == GL_BACK || face == GL_FRONT_AND_BACK)
    {
        mGLState.setStencilBackParams(func, ref, mask);
    }
}

void Context::stencilMaskSeparate(GLenum face, GLuint mask)
{
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK)
    {
        mGLState.setStencilWritemask(mask);
    }

    if (face == GL_BACK || face == GL_FRONT_AND_BACK)
    {
        mGLState.setStencilBackWritemask(mask);
    }
}

void Context::stencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass)
{
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK)
    {
        mGLState.setStencilOperations(fail, zfail, zpass);
    }

    if (face == GL_BACK || face == GL_FRONT_AND_BACK)
    {
        mGLState.setStencilBackOperations(fail, zfail, zpass);
    }
}

void Context::vertexAttrib1f(GLuint index, GLfloat x)
{
    GLfloat vals[4] = {x, 0, 0, 1};
    mGLState.setVertexAttribf(index, vals);
}

void Context::vertexAttrib1fv(GLuint index, const GLfloat *values)
{
    GLfloat vals[4] = {values[0], 0, 0, 1};
    mGLState.setVertexAttribf(index, vals);
}

void Context::vertexAttrib2f(GLuint index, GLfloat x, GLfloat y)
{
    GLfloat vals[4] = {x, y, 0, 1};
    mGLState.setVertexAttribf(index, vals);
}

void Context::vertexAttrib2fv(GLuint index, const GLfloat *values)
{
    GLfloat vals[4] = {values[0], values[1], 0, 1};
    mGLState.setVertexAttribf(index, vals);
}

void Context::vertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z)
{
    GLfloat vals[4] = {x, y, z, 1};
    mGLState.setVertexAttribf(index, vals);
}

void Context::vertexAttrib3fv(GLuint index, const GLfloat *values)
{
    GLfloat vals[4] = {values[0], values[1], values[2], 1};
    mGLState.setVertexAttribf(index, vals);
}

void Context::vertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    GLfloat vals[4] = {x, y, z, w};
    mGLState.setVertexAttribf(index, vals);
}

void Context::vertexAttrib4fv(GLuint index, const GLfloat *values)
{
    mGLState.setVertexAttribf(index, values);
}

void Context::vertexAttribPointer(GLuint index,
                                  GLint size,
                                  GLenum type,
                                  GLboolean normalized,
                                  GLsizei stride,
                                  const void *ptr)
{
    mGLState.setVertexAttribPointer(this, index, mGLState.getTargetBuffer(BufferBinding::Array),
                                    size, type, ConvertToBool(normalized), false, stride, ptr);
}

void Context::vertexAttribFormat(GLuint attribIndex,
                                 GLint size,
                                 GLenum type,
                                 GLboolean normalized,
                                 GLuint relativeOffset)
{
    mGLState.setVertexAttribFormat(attribIndex, size, type, ConvertToBool(normalized), false,
                                   relativeOffset);
}

void Context::vertexAttribIFormat(GLuint attribIndex,
                                  GLint size,
                                  GLenum type,
                                  GLuint relativeOffset)
{
    mGLState.setVertexAttribFormat(attribIndex, size, type, false, true, relativeOffset);
}

void Context::vertexAttribBinding(GLuint attribIndex, GLuint bindingIndex)
{
    mGLState.setVertexAttribBinding(this, attribIndex, bindingIndex);
}

void Context::vertexBindingDivisor(GLuint bindingIndex, GLuint divisor)
{
    mGLState.setVertexBindingDivisor(bindingIndex, divisor);
}

void Context::viewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    mGLState.setViewportParams(x, y, width, height);
}

void Context::vertexAttribIPointer(GLuint index,
                                   GLint size,
                                   GLenum type,
                                   GLsizei stride,
                                   const void *pointer)
{
    mGLState.setVertexAttribPointer(this, index, mGLState.getTargetBuffer(BufferBinding::Array),
                                    size, type, false, true, stride, pointer);
}

void Context::vertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w)
{
    GLint vals[4] = {x, y, z, w};
    mGLState.setVertexAttribi(index, vals);
}

void Context::vertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w)
{
    GLuint vals[4] = {x, y, z, w};
    mGLState.setVertexAttribu(index, vals);
}

void Context::vertexAttribI4iv(GLuint index, const GLint *v)
{
    mGLState.setVertexAttribi(index, v);
}

void Context::vertexAttribI4uiv(GLuint index, const GLuint *v)
{
    mGLState.setVertexAttribu(index, v);
}

void Context::getVertexAttribiv(GLuint index, GLenum pname, GLint *params)
{
    const VertexAttribCurrentValueData &currentValues =
        getGLState().getVertexAttribCurrentValue(index);
    const VertexArray *vao = getGLState().getVertexArray();
    QueryVertexAttribiv(vao->getVertexAttribute(index), vao->getBindingFromAttribIndex(index),
                        currentValues, pname, params);
}

void Context::getVertexAttribfv(GLuint index, GLenum pname, GLfloat *params)
{
    const VertexAttribCurrentValueData &currentValues =
        getGLState().getVertexAttribCurrentValue(index);
    const VertexArray *vao = getGLState().getVertexArray();
    QueryVertexAttribfv(vao->getVertexAttribute(index), vao->getBindingFromAttribIndex(index),
                        currentValues, pname, params);
}

void Context::getVertexAttribIiv(GLuint index, GLenum pname, GLint *params)
{
    const VertexAttribCurrentValueData &currentValues =
        getGLState().getVertexAttribCurrentValue(index);
    const VertexArray *vao = getGLState().getVertexArray();
    QueryVertexAttribIiv(vao->getVertexAttribute(index), vao->getBindingFromAttribIndex(index),
                         currentValues, pname, params);
}

void Context::getVertexAttribIuiv(GLuint index, GLenum pname, GLuint *params)
{
    const VertexAttribCurrentValueData &currentValues =
        getGLState().getVertexAttribCurrentValue(index);
    const VertexArray *vao = getGLState().getVertexArray();
    QueryVertexAttribIuiv(vao->getVertexAttribute(index), vao->getBindingFromAttribIndex(index),
                          currentValues, pname, params);
}

void Context::getVertexAttribPointerv(GLuint index, GLenum pname, void **pointer)
{
    const VertexAttribute &attrib = getGLState().getVertexArray()->getVertexAttribute(index);
    QueryVertexAttribPointerv(attrib, pname, pointer);
}

void Context::debugMessageControl(GLenum source,
                                  GLenum type,
                                  GLenum severity,
                                  GLsizei count,
                                  const GLuint *ids,
                                  GLboolean enabled)
{
    std::vector<GLuint> idVector(ids, ids + count);
    mGLState.getDebug().setMessageControl(source, type, severity, std::move(idVector),
                                          ConvertToBool(enabled));
}

void Context::debugMessageInsert(GLenum source,
                                 GLenum type,
                                 GLuint id,
                                 GLenum severity,
                                 GLsizei length,
                                 const GLchar *buf)
{
    std::string msg(buf, (length > 0) ? static_cast<size_t>(length) : strlen(buf));
    mGLState.getDebug().insertMessage(source, type, id, severity, std::move(msg));
}

void Context::debugMessageCallback(GLDEBUGPROCKHR callback, const void *userParam)
{
    mGLState.getDebug().setCallback(callback, userParam);
}

GLuint Context::getDebugMessageLog(GLuint count,
                                   GLsizei bufSize,
                                   GLenum *sources,
                                   GLenum *types,
                                   GLuint *ids,
                                   GLenum *severities,
                                   GLsizei *lengths,
                                   GLchar *messageLog)
{
    return static_cast<GLuint>(mGLState.getDebug().getMessages(count, bufSize, sources, types, ids,
                                                               severities, lengths, messageLog));
}

void Context::pushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar *message)
{
    std::string msg(message, (length > 0) ? static_cast<size_t>(length) : strlen(message));
    mGLState.getDebug().pushGroup(source, id, std::move(msg));
    mImplementation->pushDebugGroup(source, id, length, message);
}

void Context::popDebugGroup()
{
    mGLState.getDebug().popGroup();
    mImplementation->popDebugGroup();
}

void Context::bufferData(BufferBinding target, GLsizeiptr size, const void *data, BufferUsage usage)
{
    Buffer *buffer = mGLState.getTargetBuffer(target);
    ASSERT(buffer);
    handleError(buffer->bufferData(this, target, data, size, usage));
}

void Context::bufferSubData(BufferBinding target,
                            GLintptr offset,
                            GLsizeiptr size,
                            const void *data)
{
    if (data == nullptr)
    {
        return;
    }

    Buffer *buffer = mGLState.getTargetBuffer(target);
    ASSERT(buffer);
    handleError(buffer->bufferSubData(this, target, data, size, offset));
}

void Context::attachShader(GLuint program, GLuint shader)
{
    Program *programObject = mState.mShaderPrograms->getProgram(program);
    Shader *shaderObject   = mState.mShaderPrograms->getShader(shader);
    ASSERT(programObject && shaderObject);
    programObject->attachShader(shaderObject);
}

const Workarounds &Context::getWorkarounds() const
{
    return mWorkarounds;
}

void Context::copyBufferSubData(BufferBinding readTarget,
                                BufferBinding writeTarget,
                                GLintptr readOffset,
                                GLintptr writeOffset,
                                GLsizeiptr size)
{
    // if size is zero, the copy is a successful no-op
    if (size == 0)
    {
        return;
    }

    // TODO(jmadill): cache these.
    Buffer *readBuffer  = mGLState.getTargetBuffer(readTarget);
    Buffer *writeBuffer = mGLState.getTargetBuffer(writeTarget);

    handleError(writeBuffer->copyBufferSubData(this, readBuffer, readOffset, writeOffset, size));
}

void Context::bindAttribLocation(GLuint program, GLuint index, const GLchar *name)
{
    Program *programObject = getProgram(program);
    // TODO(jmadill): Re-use this from the validation if possible.
    ASSERT(programObject);
    programObject->bindAttributeLocation(index, name);
}

void Context::bindBuffer(BufferBinding target, GLuint buffer)
{
    Buffer *bufferObject = mState.mBuffers->checkBufferAllocation(mImplementation.get(), buffer);
    mGLState.setBufferBinding(this, target, bufferObject);
}

void Context::bindBufferBase(BufferBinding target, GLuint index, GLuint buffer)
{
    bindBufferRange(target, index, buffer, 0, 0);
}

void Context::bindBufferRange(BufferBinding target,
                              GLuint index,
                              GLuint buffer,
                              GLintptr offset,
                              GLsizeiptr size)
{
    Buffer *bufferObject = mState.mBuffers->checkBufferAllocation(mImplementation.get(), buffer);
    mGLState.setIndexedBufferBinding(this, target, index, bufferObject, offset, size);
}

void Context::bindFramebuffer(GLenum target, GLuint framebuffer)
{
    if (target == GL_READ_FRAMEBUFFER || target == GL_FRAMEBUFFER)
    {
        bindReadFramebuffer(framebuffer);
    }

    if (target == GL_DRAW_FRAMEBUFFER || target == GL_FRAMEBUFFER)
    {
        bindDrawFramebuffer(framebuffer);
    }
}

void Context::bindRenderbuffer(GLenum target, GLuint renderbuffer)
{
    ASSERT(target == GL_RENDERBUFFER);
    Renderbuffer *object =
        mState.mRenderbuffers->checkRenderbufferAllocation(mImplementation.get(), renderbuffer);
    mGLState.setRenderbufferBinding(this, object);
}

void Context::texStorage2DMultisample(GLenum target,
                                      GLsizei samples,
                                      GLenum internalformat,
                                      GLsizei width,
                                      GLsizei height,
                                      GLboolean fixedsamplelocations)
{
    Extents size(width, height, 1);
    Texture *texture = getTargetTexture(target);
    handleError(texture->setStorageMultisample(this, target, samples, internalformat, size,
                                               ConvertToBool(fixedsamplelocations)));
}

void Context::getMultisamplefv(GLenum pname, GLuint index, GLfloat *val)
{
    // According to spec 3.1 Table 20.49: Framebuffer Dependent Values,
    // the sample position should be queried by DRAW_FRAMEBUFFER.
    mGLState.syncDirtyObject(this, GL_DRAW_FRAMEBUFFER);
    const Framebuffer *framebuffer = mGLState.getDrawFramebuffer();

    switch (pname)
    {
        case GL_SAMPLE_POSITION:
            handleError(framebuffer->getSamplePosition(index, val));
            break;
        default:
            UNREACHABLE();
    }
}

void Context::renderbufferStorage(GLenum target,
                                  GLenum internalformat,
                                  GLsizei width,
                                  GLsizei height)
{
    // Hack for the special WebGL 1 "DEPTH_STENCIL" internal format.
    GLenum convertedInternalFormat = getConvertedRenderbufferFormat(internalformat);

    Renderbuffer *renderbuffer = mGLState.getCurrentRenderbuffer();
    handleError(renderbuffer->setStorage(this, convertedInternalFormat, width, height));
}

void Context::renderbufferStorageMultisample(GLenum target,
                                             GLsizei samples,
                                             GLenum internalformat,
                                             GLsizei width,
                                             GLsizei height)
{
    // Hack for the special WebGL 1 "DEPTH_STENCIL" internal format.
    GLenum convertedInternalFormat = getConvertedRenderbufferFormat(internalformat);

    Renderbuffer *renderbuffer = mGLState.getCurrentRenderbuffer();
    handleError(
        renderbuffer->setStorageMultisample(this, samples, convertedInternalFormat, width, height));
}

void Context::getSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *values)
{
    const Sync *syncObject = getSync(sync);
    handleError(QuerySynciv(syncObject, pname, bufSize, length, values));
}

void Context::getFramebufferParameteriv(GLenum target, GLenum pname, GLint *params)
{
    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    QueryFramebufferParameteriv(framebuffer, pname, params);
}

void Context::framebufferParameteri(GLenum target, GLenum pname, GLint param)
{
    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    SetFramebufferParameteri(framebuffer, pname, param);
}

Error Context::getScratchBuffer(size_t requstedSizeBytes,
                                angle::MemoryBuffer **scratchBufferOut) const
{
    if (!mScratchBuffer.get(requstedSizeBytes, scratchBufferOut))
    {
        return OutOfMemory() << "Failed to allocate internal buffer.";
    }
    return NoError();
}

Error Context::getZeroFilledBuffer(size_t requstedSizeBytes,
                                   angle::MemoryBuffer **zeroBufferOut) const
{
    if (!mZeroFilledBuffer.getInitialized(requstedSizeBytes, zeroBufferOut, 0))
    {
        return OutOfMemory() << "Failed to allocate internal buffer.";
    }
    return NoError();
}

void Context::dispatchCompute(GLuint numGroupsX, GLuint numGroupsY, GLuint numGroupsZ)
{
    if (numGroupsX == 0u || numGroupsY == 0u || numGroupsZ == 0u)
    {
        return;
    }

    // TODO(jmadill): Dirty bits for compute.
    if (isRobustResourceInitEnabled())
    {
        ANGLE_CONTEXT_TRY(mGLState.clearUnclearedActiveTextures(this));
    }

    handleError(mImplementation->dispatchCompute(this, numGroupsX, numGroupsY, numGroupsZ));
}

void Context::dispatchComputeIndirect(GLintptr indirect)
{
    UNIMPLEMENTED();
}

void Context::texStorage2D(GLenum target,
                           GLsizei levels,
                           GLenum internalFormat,
                           GLsizei width,
                           GLsizei height)
{
    Extents size(width, height, 1);
    Texture *texture = getTargetTexture(target);
    handleError(texture->setStorage(this, target, levels, internalFormat, size));
}

void Context::texStorage3D(GLenum target,
                           GLsizei levels,
                           GLenum internalFormat,
                           GLsizei width,
                           GLsizei height,
                           GLsizei depth)
{
    Extents size(width, height, depth);
    Texture *texture = getTargetTexture(target);
    handleError(texture->setStorage(this, target, levels, internalFormat, size));
}

void Context::memoryBarrier(GLbitfield barriers)
{
    UNIMPLEMENTED();
}

void Context::memoryBarrierByRegion(GLbitfield barriers)
{
    UNIMPLEMENTED();
}

GLenum Context::checkFramebufferStatus(GLenum target)
{
    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    return framebuffer->checkStatus(this);
}

void Context::compileShader(GLuint shader)
{
    Shader *shaderObject = GetValidShader(this, shader);
    if (!shaderObject)
    {
        return;
    }
    shaderObject->compile(this);
}

void Context::deleteBuffers(GLsizei n, const GLuint *buffers)
{
    for (int i = 0; i < n; i++)
    {
        deleteBuffer(buffers[i]);
    }
}

void Context::deleteFramebuffers(GLsizei n, const GLuint *framebuffers)
{
    for (int i = 0; i < n; i++)
    {
        if (framebuffers[i] != 0)
        {
            deleteFramebuffer(framebuffers[i]);
        }
    }
}

void Context::deleteRenderbuffers(GLsizei n, const GLuint *renderbuffers)
{
    for (int i = 0; i < n; i++)
    {
        deleteRenderbuffer(renderbuffers[i]);
    }
}

void Context::deleteTextures(GLsizei n, const GLuint *textures)
{
    for (int i = 0; i < n; i++)
    {
        if (textures[i] != 0)
        {
            deleteTexture(textures[i]);
        }
    }
}

void Context::detachShader(GLuint program, GLuint shader)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);

    Shader *shaderObject = getShader(shader);
    ASSERT(shaderObject);

    programObject->detachShader(this, shaderObject);
}

void Context::genBuffers(GLsizei n, GLuint *buffers)
{
    for (int i = 0; i < n; i++)
    {
        buffers[i] = createBuffer();
    }
}

void Context::genFramebuffers(GLsizei n, GLuint *framebuffers)
{
    for (int i = 0; i < n; i++)
    {
        framebuffers[i] = createFramebuffer();
    }
}

void Context::genRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
    for (int i = 0; i < n; i++)
    {
        renderbuffers[i] = createRenderbuffer();
    }
}

void Context::genTextures(GLsizei n, GLuint *textures)
{
    for (int i = 0; i < n; i++)
    {
        textures[i] = createTexture();
    }
}

void Context::getActiveAttrib(GLuint program,
                              GLuint index,
                              GLsizei bufsize,
                              GLsizei *length,
                              GLint *size,
                              GLenum *type,
                              GLchar *name)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->getActiveAttribute(index, bufsize, length, size, type, name);
}

void Context::getActiveUniform(GLuint program,
                               GLuint index,
                               GLsizei bufsize,
                               GLsizei *length,
                               GLint *size,
                               GLenum *type,
                               GLchar *name)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->getActiveUniform(index, bufsize, length, size, type, name);
}

void Context::getAttachedShaders(GLuint program, GLsizei maxcount, GLsizei *count, GLuint *shaders)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->getAttachedShaders(maxcount, count, shaders);
}

GLint Context::getAttribLocation(GLuint program, const GLchar *name)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    return programObject->getAttributeLocation(name);
}

void Context::getBooleanv(GLenum pname, GLboolean *params)
{
    GLenum nativeType;
    unsigned int numParams = 0;
    getQueryParameterInfo(pname, &nativeType, &numParams);

    if (nativeType == GL_BOOL)
    {
        getBooleanvImpl(pname, params);
    }
    else
    {
        CastStateValues(this, nativeType, pname, numParams, params);
    }
}

void Context::getFloatv(GLenum pname, GLfloat *params)
{
    GLenum nativeType;
    unsigned int numParams = 0;
    getQueryParameterInfo(pname, &nativeType, &numParams);

    if (nativeType == GL_FLOAT)
    {
        getFloatvImpl(pname, params);
    }
    else
    {
        CastStateValues(this, nativeType, pname, numParams, params);
    }
}

void Context::getIntegerv(GLenum pname, GLint *params)
{
    GLenum nativeType;
    unsigned int numParams = 0;
    getQueryParameterInfo(pname, &nativeType, &numParams);

    if (nativeType == GL_INT)
    {
        getIntegervImpl(pname, params);
    }
    else
    {
        CastStateValues(this, nativeType, pname, numParams, params);
    }
}

void Context::getProgramiv(GLuint program, GLenum pname, GLint *params)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    QueryProgramiv(this, programObject, pname, params);
}

void Context::getProgramPipelineiv(GLuint pipeline, GLenum pname, GLint *params)
{
    UNIMPLEMENTED();
}

void Context::getProgramInfoLog(GLuint program, GLsizei bufsize, GLsizei *length, GLchar *infolog)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->getInfoLog(bufsize, length, infolog);
}

void Context::getProgramPipelineInfoLog(GLuint pipeline,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLchar *infoLog)
{
    UNIMPLEMENTED();
}

void Context::getShaderiv(GLuint shader, GLenum pname, GLint *params)
{
    Shader *shaderObject = getShader(shader);
    ASSERT(shaderObject);
    QueryShaderiv(this, shaderObject, pname, params);
}

void Context::getShaderInfoLog(GLuint shader, GLsizei bufsize, GLsizei *length, GLchar *infolog)
{
    Shader *shaderObject = getShader(shader);
    ASSERT(shaderObject);
    shaderObject->getInfoLog(this, bufsize, length, infolog);
}

void Context::getShaderPrecisionFormat(GLenum shadertype,
                                       GLenum precisiontype,
                                       GLint *range,
                                       GLint *precision)
{
    // TODO(jmadill): Compute shaders.

    switch (shadertype)
    {
        case GL_VERTEX_SHADER:
            switch (precisiontype)
            {
                case GL_LOW_FLOAT:
                    mCaps.vertexLowpFloat.get(range, precision);
                    break;
                case GL_MEDIUM_FLOAT:
                    mCaps.vertexMediumpFloat.get(range, precision);
                    break;
                case GL_HIGH_FLOAT:
                    mCaps.vertexHighpFloat.get(range, precision);
                    break;

                case GL_LOW_INT:
                    mCaps.vertexLowpInt.get(range, precision);
                    break;
                case GL_MEDIUM_INT:
                    mCaps.vertexMediumpInt.get(range, precision);
                    break;
                case GL_HIGH_INT:
                    mCaps.vertexHighpInt.get(range, precision);
                    break;

                default:
                    UNREACHABLE();
                    return;
            }
            break;

        case GL_FRAGMENT_SHADER:
            switch (precisiontype)
            {
                case GL_LOW_FLOAT:
                    mCaps.fragmentLowpFloat.get(range, precision);
                    break;
                case GL_MEDIUM_FLOAT:
                    mCaps.fragmentMediumpFloat.get(range, precision);
                    break;
                case GL_HIGH_FLOAT:
                    mCaps.fragmentHighpFloat.get(range, precision);
                    break;

                case GL_LOW_INT:
                    mCaps.fragmentLowpInt.get(range, precision);
                    break;
                case GL_MEDIUM_INT:
                    mCaps.fragmentMediumpInt.get(range, precision);
                    break;
                case GL_HIGH_INT:
                    mCaps.fragmentHighpInt.get(range, precision);
                    break;

                default:
                    UNREACHABLE();
                    return;
            }
            break;

        default:
            UNREACHABLE();
            return;
    }
}

void Context::getShaderSource(GLuint shader, GLsizei bufsize, GLsizei *length, GLchar *source)
{
    Shader *shaderObject = getShader(shader);
    ASSERT(shaderObject);
    shaderObject->getSource(bufsize, length, source);
}

void Context::getUniformfv(GLuint program, GLint location, GLfloat *params)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->getUniformfv(this, location, params);
}

void Context::getUniformiv(GLuint program, GLint location, GLint *params)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->getUniformiv(this, location, params);
}

GLint Context::getUniformLocation(GLuint program, const GLchar *name)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    return programObject->getUniformLocation(name);
}

GLboolean Context::isBuffer(GLuint buffer)
{
    if (buffer == 0)
    {
        return GL_FALSE;
    }

    return (getBuffer(buffer) ? GL_TRUE : GL_FALSE);
}

GLboolean Context::isEnabled(GLenum cap)
{
    return mGLState.getEnableFeature(cap);
}

GLboolean Context::isFramebuffer(GLuint framebuffer)
{
    if (framebuffer == 0)
    {
        return GL_FALSE;
    }

    return (getFramebuffer(framebuffer) ? GL_TRUE : GL_FALSE);
}

GLboolean Context::isProgram(GLuint program)
{
    if (program == 0)
    {
        return GL_FALSE;
    }

    return (getProgram(program) ? GL_TRUE : GL_FALSE);
}

GLboolean Context::isRenderbuffer(GLuint renderbuffer)
{
    if (renderbuffer == 0)
    {
        return GL_FALSE;
    }

    return (getRenderbuffer(renderbuffer) ? GL_TRUE : GL_FALSE);
}

GLboolean Context::isShader(GLuint shader)
{
    if (shader == 0)
    {
        return GL_FALSE;
    }

    return (getShader(shader) ? GL_TRUE : GL_FALSE);
}

GLboolean Context::isTexture(GLuint texture)
{
    if (texture == 0)
    {
        return GL_FALSE;
    }

    return (getTexture(texture) ? GL_TRUE : GL_FALSE);
}

void Context::linkProgram(GLuint program)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    handleError(programObject->link(this));
    mGLState.onProgramExecutableChange(programObject);
}

void Context::releaseShaderCompiler()
{
    mCompiler.set(this, nullptr);
}

void Context::shaderBinary(GLsizei n,
                           const GLuint *shaders,
                           GLenum binaryformat,
                           const void *binary,
                           GLsizei length)
{
    // No binary shader formats are supported.
    UNIMPLEMENTED();
}

void Context::shaderSource(GLuint shader,
                           GLsizei count,
                           const GLchar *const *string,
                           const GLint *length)
{
    Shader *shaderObject = getShader(shader);
    ASSERT(shaderObject);
    shaderObject->setSource(count, string, length);
}

void Context::stencilFunc(GLenum func, GLint ref, GLuint mask)
{
    stencilFuncSeparate(GL_FRONT_AND_BACK, func, ref, mask);
}

void Context::stencilMask(GLuint mask)
{
    stencilMaskSeparate(GL_FRONT_AND_BACK, mask);
}

void Context::stencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
    stencilOpSeparate(GL_FRONT_AND_BACK, fail, zfail, zpass);
}

void Context::uniform1f(GLint location, GLfloat x)
{
    Program *program = mGLState.getProgram();
    program->setUniform1fv(location, 1, &x);
}

void Context::uniform1fv(GLint location, GLsizei count, const GLfloat *v)
{
    Program *program = mGLState.getProgram();
    program->setUniform1fv(location, count, v);
}

void Context::uniform1i(GLint location, GLint x)
{
    Program *program = mGLState.getProgram();
    if (program->setUniform1iv(location, 1, &x) == Program::SetUniformResult::SamplerChanged)
    {
        mGLState.setObjectDirty(GL_PROGRAM);
    }
}

void Context::uniform1iv(GLint location, GLsizei count, const GLint *v)
{
    Program *program = mGLState.getProgram();
    if (program->setUniform1iv(location, count, v) == Program::SetUniformResult::SamplerChanged)
    {
        mGLState.setObjectDirty(GL_PROGRAM);
    }
}

void Context::uniform2f(GLint location, GLfloat x, GLfloat y)
{
    GLfloat xy[2]    = {x, y};
    Program *program = mGLState.getProgram();
    program->setUniform2fv(location, 1, xy);
}

void Context::uniform2fv(GLint location, GLsizei count, const GLfloat *v)
{
    Program *program = mGLState.getProgram();
    program->setUniform2fv(location, count, v);
}

void Context::uniform2i(GLint location, GLint x, GLint y)
{
    GLint xy[2]      = {x, y};
    Program *program = mGLState.getProgram();
    program->setUniform2iv(location, 1, xy);
}

void Context::uniform2iv(GLint location, GLsizei count, const GLint *v)
{
    Program *program = mGLState.getProgram();
    program->setUniform2iv(location, count, v);
}

void Context::uniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z)
{
    GLfloat xyz[3]   = {x, y, z};
    Program *program = mGLState.getProgram();
    program->setUniform3fv(location, 1, xyz);
}

void Context::uniform3fv(GLint location, GLsizei count, const GLfloat *v)
{
    Program *program = mGLState.getProgram();
    program->setUniform3fv(location, count, v);
}

void Context::uniform3i(GLint location, GLint x, GLint y, GLint z)
{
    GLint xyz[3]     = {x, y, z};
    Program *program = mGLState.getProgram();
    program->setUniform3iv(location, 1, xyz);
}

void Context::uniform3iv(GLint location, GLsizei count, const GLint *v)
{
    Program *program = mGLState.getProgram();
    program->setUniform3iv(location, count, v);
}

void Context::uniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    GLfloat xyzw[4]  = {x, y, z, w};
    Program *program = mGLState.getProgram();
    program->setUniform4fv(location, 1, xyzw);
}

void Context::uniform4fv(GLint location, GLsizei count, const GLfloat *v)
{
    Program *program = mGLState.getProgram();
    program->setUniform4fv(location, count, v);
}

void Context::uniform4i(GLint location, GLint x, GLint y, GLint z, GLint w)
{
    GLint xyzw[4]    = {x, y, z, w};
    Program *program = mGLState.getProgram();
    program->setUniform4iv(location, 1, xyzw);
}

void Context::uniform4iv(GLint location, GLsizei count, const GLint *v)
{
    Program *program = mGLState.getProgram();
    program->setUniform4iv(location, count, v);
}

void Context::uniformMatrix2fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value)
{
    Program *program = mGLState.getProgram();
    program->setUniformMatrix2fv(location, count, transpose, value);
}

void Context::uniformMatrix3fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value)
{
    Program *program = mGLState.getProgram();
    program->setUniformMatrix3fv(location, count, transpose, value);
}

void Context::uniformMatrix4fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value)
{
    Program *program = mGLState.getProgram();
    program->setUniformMatrix4fv(location, count, transpose, value);
}

void Context::validateProgram(GLuint program)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->validate(mCaps);
}

void Context::validateProgramPipeline(GLuint pipeline)
{
    UNIMPLEMENTED();
}

void Context::getProgramBinary(GLuint program,
                               GLsizei bufSize,
                               GLsizei *length,
                               GLenum *binaryFormat,
                               void *binary)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject != nullptr);

    handleError(programObject->saveBinary(this, binaryFormat, binary, bufSize, length));
}

void Context::programBinary(GLuint program, GLenum binaryFormat, const void *binary, GLsizei length)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject != nullptr);

    handleError(programObject->loadBinary(this, binaryFormat, binary, length));
}

void Context::uniform1ui(GLint location, GLuint v0)
{
    Program *program = mGLState.getProgram();
    program->setUniform1uiv(location, 1, &v0);
}

void Context::uniform2ui(GLint location, GLuint v0, GLuint v1)
{
    Program *program  = mGLState.getProgram();
    const GLuint xy[] = {v0, v1};
    program->setUniform2uiv(location, 1, xy);
}

void Context::uniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2)
{
    Program *program   = mGLState.getProgram();
    const GLuint xyz[] = {v0, v1, v2};
    program->setUniform3uiv(location, 1, xyz);
}

void Context::uniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
    Program *program    = mGLState.getProgram();
    const GLuint xyzw[] = {v0, v1, v2, v3};
    program->setUniform4uiv(location, 1, xyzw);
}

void Context::uniform1uiv(GLint location, GLsizei count, const GLuint *value)
{
    Program *program = mGLState.getProgram();
    program->setUniform1uiv(location, count, value);
}
void Context::uniform2uiv(GLint location, GLsizei count, const GLuint *value)
{
    Program *program = mGLState.getProgram();
    program->setUniform2uiv(location, count, value);
}

void Context::uniform3uiv(GLint location, GLsizei count, const GLuint *value)
{
    Program *program = mGLState.getProgram();
    program->setUniform3uiv(location, count, value);
}

void Context::uniform4uiv(GLint location, GLsizei count, const GLuint *value)
{
    Program *program = mGLState.getProgram();
    program->setUniform4uiv(location, count, value);
}

void Context::genQueries(GLsizei n, GLuint *ids)
{
    for (GLsizei i = 0; i < n; i++)
    {
        GLuint handle = mQueryHandleAllocator.allocate();
        mQueryMap.assign(handle, nullptr);
        ids[i] = handle;
    }
}

void Context::deleteQueries(GLsizei n, const GLuint *ids)
{
    for (int i = 0; i < n; i++)
    {
        GLuint query = ids[i];

        Query *queryObject = nullptr;
        if (mQueryMap.erase(query, &queryObject))
        {
            mQueryHandleAllocator.release(query);
            if (queryObject)
            {
                queryObject->release(this);
            }
        }
    }
}

GLboolean Context::isQuery(GLuint id)
{
    return (getQuery(id, false, GL_NONE) != nullptr) ? GL_TRUE : GL_FALSE;
}

void Context::uniformMatrix2x3fv(GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value)
{
    Program *program = mGLState.getProgram();
    program->setUniformMatrix2x3fv(location, count, transpose, value);
}

void Context::uniformMatrix3x2fv(GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value)
{
    Program *program = mGLState.getProgram();
    program->setUniformMatrix3x2fv(location, count, transpose, value);
}

void Context::uniformMatrix2x4fv(GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value)
{
    Program *program = mGLState.getProgram();
    program->setUniformMatrix2x4fv(location, count, transpose, value);
}

void Context::uniformMatrix4x2fv(GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value)
{
    Program *program = mGLState.getProgram();
    program->setUniformMatrix4x2fv(location, count, transpose, value);
}

void Context::uniformMatrix3x4fv(GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value)
{
    Program *program = mGLState.getProgram();
    program->setUniformMatrix3x4fv(location, count, transpose, value);
}

void Context::uniformMatrix4x3fv(GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value)
{
    Program *program = mGLState.getProgram();
    program->setUniformMatrix4x3fv(location, count, transpose, value);
}

void Context::deleteVertexArrays(GLsizei n, const GLuint *arrays)
{
    for (int arrayIndex = 0; arrayIndex < n; arrayIndex++)
    {
        GLuint vertexArray = arrays[arrayIndex];

        if (arrays[arrayIndex] != 0)
        {
            VertexArray *vertexArrayObject = nullptr;
            if (mVertexArrayMap.erase(vertexArray, &vertexArrayObject))
            {
                if (vertexArrayObject != nullptr)
                {
                    detachVertexArray(vertexArray);
                    vertexArrayObject->onDestroy(this);
                }

                mVertexArrayHandleAllocator.release(vertexArray);
            }
        }
    }
}

void Context::genVertexArrays(GLsizei n, GLuint *arrays)
{
    for (int arrayIndex = 0; arrayIndex < n; arrayIndex++)
    {
        GLuint vertexArray = mVertexArrayHandleAllocator.allocate();
        mVertexArrayMap.assign(vertexArray, nullptr);
        arrays[arrayIndex] = vertexArray;
    }
}

bool Context::isVertexArray(GLuint array)
{
    if (array == 0)
    {
        return GL_FALSE;
    }

    VertexArray *vao = getVertexArray(array);
    return (vao != nullptr ? GL_TRUE : GL_FALSE);
}

void Context::endTransformFeedback()
{
    TransformFeedback *transformFeedback = mGLState.getCurrentTransformFeedback();
    transformFeedback->end(this);
}

void Context::transformFeedbackVaryings(GLuint program,
                                        GLsizei count,
                                        const GLchar *const *varyings,
                                        GLenum bufferMode)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setTransformFeedbackVaryings(count, varyings, bufferMode);
}

void Context::getTransformFeedbackVarying(GLuint program,
                                          GLuint index,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLsizei *size,
                                          GLenum *type,
                                          GLchar *name)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->getTransformFeedbackVarying(index, bufSize, length, size, type, name);
}

void Context::deleteTransformFeedbacks(GLsizei n, const GLuint *ids)
{
    for (int i = 0; i < n; i++)
    {
        GLuint transformFeedback = ids[i];
        if (transformFeedback == 0)
        {
            continue;
        }

        TransformFeedback *transformFeedbackObject = nullptr;
        if (mTransformFeedbackMap.erase(transformFeedback, &transformFeedbackObject))
        {
            if (transformFeedbackObject != nullptr)
            {
                detachTransformFeedback(transformFeedback);
                transformFeedbackObject->release(this);
            }

            mTransformFeedbackHandleAllocator.release(transformFeedback);
        }
    }
}

void Context::genTransformFeedbacks(GLsizei n, GLuint *ids)
{
    for (int i = 0; i < n; i++)
    {
        GLuint transformFeedback = mTransformFeedbackHandleAllocator.allocate();
        mTransformFeedbackMap.assign(transformFeedback, nullptr);
        ids[i] = transformFeedback;
    }
}

bool Context::isTransformFeedback(GLuint id)
{
    if (id == 0)
    {
        // The 3.0.4 spec [section 6.1.11] states that if ID is zero, IsTransformFeedback
        // returns FALSE
        return GL_FALSE;
    }

    const TransformFeedback *transformFeedback = getTransformFeedback(id);
    return ((transformFeedback != nullptr) ? GL_TRUE : GL_FALSE);
}

void Context::pauseTransformFeedback()
{
    TransformFeedback *transformFeedback = mGLState.getCurrentTransformFeedback();
    transformFeedback->pause();
}

void Context::resumeTransformFeedback()
{
    TransformFeedback *transformFeedback = mGLState.getCurrentTransformFeedback();
    transformFeedback->resume();
}

void Context::getUniformuiv(GLuint program, GLint location, GLuint *params)
{
    const Program *programObject = getProgram(program);
    programObject->getUniformuiv(this, location, params);
}

GLint Context::getFragDataLocation(GLuint program, const GLchar *name)
{
    const Program *programObject = getProgram(program);
    return programObject->getFragDataLocation(name);
}

void Context::getUniformIndices(GLuint program,
                                GLsizei uniformCount,
                                const GLchar *const *uniformNames,
                                GLuint *uniformIndices)
{
    const Program *programObject = getProgram(program);
    if (!programObject->isLinked())
    {
        for (int uniformId = 0; uniformId < uniformCount; uniformId++)
        {
            uniformIndices[uniformId] = GL_INVALID_INDEX;
        }
    }
    else
    {
        for (int uniformId = 0; uniformId < uniformCount; uniformId++)
        {
            uniformIndices[uniformId] = programObject->getUniformIndex(uniformNames[uniformId]);
        }
    }
}

void Context::getActiveUniformsiv(GLuint program,
                                  GLsizei uniformCount,
                                  const GLuint *uniformIndices,
                                  GLenum pname,
                                  GLint *params)
{
    const Program *programObject = getProgram(program);
    for (int uniformId = 0; uniformId < uniformCount; uniformId++)
    {
        const GLuint index = uniformIndices[uniformId];
        params[uniformId]  = GetUniformResourceProperty(programObject, index, pname);
    }
}

GLuint Context::getUniformBlockIndex(GLuint program, const GLchar *uniformBlockName)
{
    const Program *programObject = getProgram(program);
    return programObject->getUniformBlockIndex(uniformBlockName);
}

void Context::getActiveUniformBlockiv(GLuint program,
                                      GLuint uniformBlockIndex,
                                      GLenum pname,
                                      GLint *params)
{
    const Program *programObject = getProgram(program);
    QueryActiveUniformBlockiv(programObject, uniformBlockIndex, pname, params);
}

void Context::getActiveUniformBlockName(GLuint program,
                                        GLuint uniformBlockIndex,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLchar *uniformBlockName)
{
    const Program *programObject = getProgram(program);
    programObject->getActiveUniformBlockName(uniformBlockIndex, bufSize, length, uniformBlockName);
}

void Context::uniformBlockBinding(GLuint program,
                                  GLuint uniformBlockIndex,
                                  GLuint uniformBlockBinding)
{
    Program *programObject = getProgram(program);
    programObject->bindUniformBlock(uniformBlockIndex, uniformBlockBinding);
}

GLsync Context::fenceSync(GLenum condition, GLbitfield flags)
{
    GLuint handle     = mState.mSyncs->createSync(mImplementation.get());
    GLsync syncHandle = reinterpret_cast<GLsync>(static_cast<uintptr_t>(handle));

    Sync *syncObject = getSync(syncHandle);
    Error error      = syncObject->set(condition, flags);
    if (error.isError())
    {
        deleteSync(syncHandle);
        handleError(error);
        return nullptr;
    }

    return syncHandle;
}

GLboolean Context::isSync(GLsync sync)
{
    return (getSync(sync) != nullptr);
}

GLenum Context::clientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
    Sync *syncObject = getSync(sync);

    GLenum result = GL_WAIT_FAILED;
    handleError(syncObject->clientWait(flags, timeout, &result));
    return result;
}

void Context::waitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
    Sync *syncObject = getSync(sync);
    handleError(syncObject->serverWait(flags, timeout));
}

void Context::getInteger64v(GLenum pname, GLint64 *params)
{
    GLenum nativeType      = GL_NONE;
    unsigned int numParams = 0;
    getQueryParameterInfo(pname, &nativeType, &numParams);

    if (nativeType == GL_INT_64_ANGLEX)
    {
        getInteger64vImpl(pname, params);
    }
    else
    {
        CastStateValues(this, nativeType, pname, numParams, params);
    }
}

void Context::getBufferParameteri64v(BufferBinding target, GLenum pname, GLint64 *params)
{
    Buffer *buffer = mGLState.getTargetBuffer(target);
    QueryBufferParameteri64v(buffer, pname, params);
}

void Context::genSamplers(GLsizei count, GLuint *samplers)
{
    for (int i = 0; i < count; i++)
    {
        samplers[i] = mState.mSamplers->createSampler();
    }
}

void Context::deleteSamplers(GLsizei count, const GLuint *samplers)
{
    for (int i = 0; i < count; i++)
    {
        GLuint sampler = samplers[i];

        if (mState.mSamplers->getSampler(sampler))
        {
            detachSampler(sampler);
        }

        mState.mSamplers->deleteObject(this, sampler);
    }
}

void Context::getInternalformativ(GLenum target,
                                  GLenum internalformat,
                                  GLenum pname,
                                  GLsizei bufSize,
                                  GLint *params)
{
    const TextureCaps &formatCaps = mTextureCaps.get(internalformat);
    QueryInternalFormativ(formatCaps, pname, bufSize, params);
}

void Context::programUniform1i(GLuint program, GLint location, GLint v0)
{
    programUniform1iv(program, location, 1, &v0);
}

void Context::programUniform2i(GLuint program, GLint location, GLint v0, GLint v1)
{
    GLint xy[2] = {v0, v1};
    programUniform2iv(program, location, 1, xy);
}

void Context::programUniform3i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2)
{
    GLint xyz[3] = {v0, v1, v2};
    programUniform3iv(program, location, 1, xyz);
}

void Context::programUniform4i(GLuint program,
                               GLint location,
                               GLint v0,
                               GLint v1,
                               GLint v2,
                               GLint v3)
{
    GLint xyzw[4] = {v0, v1, v2, v3};
    programUniform4iv(program, location, 1, xyzw);
}

void Context::programUniform1ui(GLuint program, GLint location, GLuint v0)
{
    programUniform1uiv(program, location, 1, &v0);
}

void Context::programUniform2ui(GLuint program, GLint location, GLuint v0, GLuint v1)
{
    GLuint xy[2] = {v0, v1};
    programUniform2uiv(program, location, 1, xy);
}

void Context::programUniform3ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2)
{
    GLuint xyz[3] = {v0, v1, v2};
    programUniform3uiv(program, location, 1, xyz);
}

void Context::programUniform4ui(GLuint program,
                                GLint location,
                                GLuint v0,
                                GLuint v1,
                                GLuint v2,
                                GLuint v3)
{
    GLuint xyzw[4] = {v0, v1, v2, v3};
    programUniform4uiv(program, location, 1, xyzw);
}

void Context::programUniform1f(GLuint program, GLint location, GLfloat v0)
{
    programUniform1fv(program, location, 1, &v0);
}

void Context::programUniform2f(GLuint program, GLint location, GLfloat v0, GLfloat v1)
{
    GLfloat xy[2] = {v0, v1};
    programUniform2fv(program, location, 1, xy);
}

void Context::programUniform3f(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2)
{
    GLfloat xyz[3] = {v0, v1, v2};
    programUniform3fv(program, location, 1, xyz);
}

void Context::programUniform4f(GLuint program,
                               GLint location,
                               GLfloat v0,
                               GLfloat v1,
                               GLfloat v2,
                               GLfloat v3)
{
    GLfloat xyzw[4] = {v0, v1, v2, v3};
    programUniform4fv(program, location, 1, xyzw);
}

void Context::programUniform1iv(GLuint program, GLint location, GLsizei count, const GLint *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    if (programObject->setUniform1iv(location, count, value) ==
        Program::SetUniformResult::SamplerChanged)
    {
        mGLState.setObjectDirty(GL_PROGRAM);
    }
}

void Context::programUniform2iv(GLuint program, GLint location, GLsizei count, const GLint *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniform2iv(location, count, value);
}

void Context::programUniform3iv(GLuint program, GLint location, GLsizei count, const GLint *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniform3iv(location, count, value);
}

void Context::programUniform4iv(GLuint program, GLint location, GLsizei count, const GLint *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniform4iv(location, count, value);
}

void Context::programUniform1uiv(GLuint program, GLint location, GLsizei count, const GLuint *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniform1uiv(location, count, value);
}

void Context::programUniform2uiv(GLuint program, GLint location, GLsizei count, const GLuint *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniform2uiv(location, count, value);
}

void Context::programUniform3uiv(GLuint program, GLint location, GLsizei count, const GLuint *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniform3uiv(location, count, value);
}

void Context::programUniform4uiv(GLuint program, GLint location, GLsizei count, const GLuint *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniform4uiv(location, count, value);
}

void Context::programUniform1fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniform1fv(location, count, value);
}

void Context::programUniform2fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniform2fv(location, count, value);
}

void Context::programUniform3fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniform3fv(location, count, value);
}

void Context::programUniform4fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniform4fv(location, count, value);
}

void Context::programUniformMatrix2fv(GLuint program,
                                      GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniformMatrix2fv(location, count, transpose, value);
}

void Context::programUniformMatrix3fv(GLuint program,
                                      GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniformMatrix3fv(location, count, transpose, value);
}

void Context::programUniformMatrix4fv(GLuint program,
                                      GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniformMatrix4fv(location, count, transpose, value);
}

void Context::programUniformMatrix2x3fv(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniformMatrix2x3fv(location, count, transpose, value);
}

void Context::programUniformMatrix3x2fv(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniformMatrix3x2fv(location, count, transpose, value);
}

void Context::programUniformMatrix2x4fv(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniformMatrix2x4fv(location, count, transpose, value);
}

void Context::programUniformMatrix4x2fv(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniformMatrix4x2fv(location, count, transpose, value);
}

void Context::programUniformMatrix3x4fv(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniformMatrix3x4fv(location, count, transpose, value);
}

void Context::programUniformMatrix4x3fv(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniformMatrix4x3fv(location, count, transpose, value);
}

void Context::onTextureChange(const Texture *texture)
{
    // Conservatively assume all textures are dirty.
    // TODO(jmadill): More fine-grained update.
    mGLState.setObjectDirty(GL_TEXTURE);
}

void Context::genProgramPipelines(GLsizei count, GLuint *pipelines)
{
    for (int i = 0; i < count; i++)
    {
        pipelines[i] = createProgramPipeline();
    }
}

void Context::deleteProgramPipelines(GLsizei count, const GLuint *pipelines)
{
    for (int i = 0; i < count; i++)
    {
        if (pipelines[i] != 0)
        {
            deleteProgramPipeline(pipelines[i]);
        }
    }
}

GLboolean Context::isProgramPipeline(GLuint pipeline)
{
    if (pipeline == 0)
    {
        return GL_FALSE;
    }

    return (getProgramPipeline(pipeline) ? GL_TRUE : GL_FALSE);
}

}  // namespace gl
