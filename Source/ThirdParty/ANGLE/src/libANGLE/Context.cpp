//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Context.cpp: Implements the gl::Context class, managing all GL state and performing
// rendering operations. It is the GLES2 specific implementation of EGLContext.

#include "libANGLE/Context.h"
#include "libANGLE/Context.inl.h"

#include <string.h>
#include <iterator>
#include <sstream>
#include <vector>

#include "common/PackedEnums.h"
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
#include "libANGLE/MemoryObject.h"
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
#include "libANGLE/renderer/BufferImpl.h"
#include "libANGLE/renderer/EGLImplFactory.h"
#include "libANGLE/renderer/Format.h"
#include "libANGLE/validationES.h"

namespace gl
{
namespace
{
template <typename T>
std::vector<Path *> GatherPaths(PathManager &resourceManager,
                                GLsizei numPaths,
                                const void *paths,
                                GLuint pathBase)
{
    std::vector<Path *> ret;
    ret.reserve(numPaths);

    const auto *nameArray = static_cast<const T *>(paths);

    for (GLsizei i = 0; i < numPaths; ++i)
    {
        const GLuint pathName = nameArray[i] + pathBase;

        ret.push_back(resourceManager.getPath(pathName));
    }

    return ret;
}

std::vector<Path *> GatherPaths(PathManager &resourceManager,
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
    return std::vector<Path *>();
}

template <typename T>
angle::Result GetQueryObjectParameter(const Context *context, Query *query, GLenum pname, T *params)
{
    ASSERT(query != nullptr || pname == GL_QUERY_RESULT_AVAILABLE_EXT);

    switch (pname)
    {
        case GL_QUERY_RESULT_EXT:
            return query->getResult(context, params);
        case GL_QUERY_RESULT_AVAILABLE_EXT:
        {
            bool available = false;
            if (context->isContextLost())
            {
                available = true;
            }
            else
            {
                ANGLE_TRY(query->isResultAvailable(context, &available));
            }
            *params = CastFromStateValue<T>(pname, static_cast<GLuint>(available));
            return angle::Result::Continue;
        }
        default:
            UNREACHABLE();
            return angle::Result::Stop;
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

Version GetClientVersion(const egl::AttributeMap &attribs)
{
    return Version(GetClientMajorVersion(attribs), GetClientMinorVersion(attribs));
}

GLenum GetResetStrategy(const egl::AttributeMap &attribs)
{
    EGLAttrib attrib =
        attribs.get(EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT, EGL_NO_RESET_NOTIFICATION);
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

bool GetExtensionsEnabled(const egl::AttributeMap &attribs, bool webGLContext)
{
    // If the context is WebGL, extensions are disabled by default
    EGLAttrib defaultValue = webGLContext ? EGL_FALSE : EGL_TRUE;
    return (attribs.get(EGL_EXTENSIONS_ENABLED_ANGLE, defaultValue) == EGL_TRUE);
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

// The rest default to false.
constexpr angle::PackedEnumMap<PrimitiveMode, bool, angle::EnumSize<PrimitiveMode>() + 1>
    kValidBasicDrawModes = {{
        {PrimitiveMode::Points, true},
        {PrimitiveMode::Lines, true},
        {PrimitiveMode::LineLoop, true},
        {PrimitiveMode::LineStrip, true},
        {PrimitiveMode::Triangles, true},
        {PrimitiveMode::TriangleStrip, true},
        {PrimitiveMode::TriangleFan, true},
    }};

enum SubjectIndexes : angle::SubjectIndex
{
    kTexture0SubjectIndex       = 0,
    kTextureMaxSubjectIndex     = kTexture0SubjectIndex + IMPLEMENTATION_MAX_ACTIVE_TEXTURES,
    kUniformBuffer0SubjectIndex = kTextureMaxSubjectIndex,
    kUniformBufferMaxSubjectIndex =
        kUniformBuffer0SubjectIndex + IMPLEMENTATION_MAX_UNIFORM_BUFFER_BINDINGS,
    kSampler0SubjectIndex    = kUniformBufferMaxSubjectIndex,
    kSamplerMaxSubjectIndex  = kSampler0SubjectIndex + IMPLEMENTATION_MAX_ACTIVE_TEXTURES,
    kVertexArraySubjectIndex = kSamplerMaxSubjectIndex,
    kReadFramebufferSubjectIndex,
    kDrawFramebufferSubjectIndex
};
}  // anonymous namespace

Context::Context(rx::EGLImplFactory *implFactory,
                 const egl::Config *config,
                 const Context *shareContext,
                 TextureManager *shareTextures,
                 MemoryProgramCache *memoryProgramCache,
                 const egl::AttributeMap &attribs,
                 const egl::DisplayExtensions &displayExtensions,
                 const egl::ClientExtensions &clientExtensions)
    : mState(reinterpret_cast<ContextID>(this),
             shareContext ? &shareContext->mState : nullptr,
             shareTextures,
             GetClientVersion(attribs),
             GetDebug(attribs),
             GetBindGeneratesResource(attribs),
             GetClientArraysEnabled(attribs),
             GetRobustResourceInit(attribs),
             memoryProgramCache != nullptr),
      mSkipValidation(GetNoError(attribs)),
      mDisplayTextureShareGroup(shareTextures != nullptr),
      mErrors(this),
      mImplementation(implFactory->createContext(mState, &mErrors, config, shareContext, attribs)),
      mLabel(nullptr),
      mCompiler(),
      mConfig(config),
      mClientType(EGL_OPENGL_ES_API),
      mHasBeenCurrent(false),
      mContextLost(false),
      mResetStatus(GraphicsResetStatus::NoError),
      mContextLostForced(false),
      mResetStrategy(GetResetStrategy(attribs)),
      mRobustAccess(GetRobustAccess(attribs)),
      mSurfacelessSupported(displayExtensions.surfacelessContext),
      mExplicitContextAvailable(clientExtensions.explicitContext),
      mCurrentSurface(static_cast<egl::Surface *>(EGL_NO_SURFACE)),
      mDisplay(static_cast<egl::Display *>(EGL_NO_DISPLAY)),
      mWebGLContext(GetWebGLContext(attribs)),
      mBufferAccessValidationEnabled(false),
      mExtensionsEnabled(GetExtensionsEnabled(attribs, mWebGLContext)),
      mMemoryProgramCache(memoryProgramCache),
      mVertexArrayObserverBinding(this, kVertexArraySubjectIndex),
      mDrawFramebufferObserverBinding(this, kDrawFramebufferSubjectIndex),
      mReadFramebufferObserverBinding(this, kReadFramebufferSubjectIndex),
      mScratchBuffer(1000u),
      mZeroFilledBuffer(1000u),
      mThreadPool(nullptr)
{
    for (angle::SubjectIndex uboIndex = kUniformBuffer0SubjectIndex;
         uboIndex < kUniformBufferMaxSubjectIndex; ++uboIndex)
    {
        mUniformBufferObserverBindings.emplace_back(this, uboIndex);
    }

    for (angle::SubjectIndex samplerIndex = kSampler0SubjectIndex;
         samplerIndex < kSamplerMaxSubjectIndex; ++samplerIndex)
    {
        mSamplerObserverBindings.emplace_back(this, samplerIndex);
    }
}

void Context::initialize()
{
    mImplementation->setMemoryProgramCache(mMemoryProgramCache);

    initCaps();
    initWorkarounds();

    mState.initialize(this);

    mFenceNVHandleAllocator.setBaseHandle(0);

    // [OpenGL ES 2.0.24] section 3.7 page 83:
    // In the initial state, TEXTURE_2D and TEXTURE_CUBE_MAP have two-dimensional
    // and cube map texture state vectors respectively associated with them.
    // In order that access to these initial textures not be lost, they are treated as texture
    // objects all of whose names are 0.

    Texture *zeroTexture2D = new Texture(mImplementation.get(), 0, TextureType::_2D);
    mZeroTextures[TextureType::_2D].set(this, zeroTexture2D);

    Texture *zeroTextureCube = new Texture(mImplementation.get(), 0, TextureType::CubeMap);
    mZeroTextures[TextureType::CubeMap].set(this, zeroTextureCube);

    if (getClientVersion() >= Version(3, 0))
    {
        // TODO: These could also be enabled via extension
        Texture *zeroTexture3D = new Texture(mImplementation.get(), 0, TextureType::_3D);
        mZeroTextures[TextureType::_3D].set(this, zeroTexture3D);

        Texture *zeroTexture2DArray = new Texture(mImplementation.get(), 0, TextureType::_2DArray);
        mZeroTextures[TextureType::_2DArray].set(this, zeroTexture2DArray);
    }
    if (getClientVersion() >= Version(3, 1) || mSupportedExtensions.textureMultisample)
    {
        Texture *zeroTexture2DMultisample =
            new Texture(mImplementation.get(), 0, TextureType::_2DMultisample);
        mZeroTextures[TextureType::_2DMultisample].set(this, zeroTexture2DMultisample);
    }
    if (getClientVersion() >= Version(3, 1))
    {
        Texture *zeroTexture2DMultisampleArray =
            new Texture(mImplementation.get(), 0, TextureType::_2DMultisampleArray);
        mZeroTextures[TextureType::_2DMultisampleArray].set(this, zeroTexture2DMultisampleArray);

        for (unsigned int i = 0; i < mState.mCaps.maxAtomicCounterBufferBindings; i++)
        {
            bindBufferRange(BufferBinding::AtomicCounter, i, 0, 0, 0);
        }

        for (unsigned int i = 0; i < mState.mCaps.maxShaderStorageBufferBindings; i++)
        {
            bindBufferRange(BufferBinding::ShaderStorage, i, 0, 0, 0);
        }
    }

    if (mSupportedExtensions.textureRectangle)
    {
        Texture *zeroTextureRectangle =
            new Texture(mImplementation.get(), 0, TextureType::Rectangle);
        mZeroTextures[TextureType::Rectangle].set(this, zeroTextureRectangle);
    }

    if (mSupportedExtensions.eglImageExternal || mSupportedExtensions.eglStreamConsumerExternal)
    {
        Texture *zeroTextureExternal = new Texture(mImplementation.get(), 0, TextureType::External);
        mZeroTextures[TextureType::External].set(this, zeroTextureExternal);
    }

    mState.initializeZeroTextures(this, mZeroTextures);

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

    for (unsigned int i = 0; i < mState.mCaps.maxUniformBufferBindings; i++)
    {
        bindBufferRange(BufferBinding::Uniform, i, 0, 0, -1);
    }

    // Initialize GLES1 renderer if appropriate.
    if (getClientVersion() < Version(2, 0))
    {
        mGLES1Renderer.reset(new GLES1Renderer());
    }

    // Initialize dirty bit masks
    mAllDirtyBits.set();

    mDrawDirtyObjects.set(State::DIRTY_OBJECT_DRAW_FRAMEBUFFER);
    mDrawDirtyObjects.set(State::DIRTY_OBJECT_VERTEX_ARRAY);
    mDrawDirtyObjects.set(State::DIRTY_OBJECT_TEXTURES);
    mDrawDirtyObjects.set(State::DIRTY_OBJECT_PROGRAM);
    mDrawDirtyObjects.set(State::DIRTY_OBJECT_SAMPLERS);

    mPathOperationDirtyObjects.set(State::DIRTY_OBJECT_DRAW_FRAMEBUFFER);
    mPathOperationDirtyObjects.set(State::DIRTY_OBJECT_VERTEX_ARRAY);
    mPathOperationDirtyObjects.set(State::DIRTY_OBJECT_TEXTURES);
    mPathOperationDirtyObjects.set(State::DIRTY_OBJECT_SAMPLERS);

    mTexImageDirtyBits.set(State::DIRTY_BIT_UNPACK_STATE);
    mTexImageDirtyBits.set(State::DIRTY_BIT_UNPACK_BUFFER_BINDING);
    // No dirty objects.

    // Readpixels uses the pack state and read FBO
    mReadPixelsDirtyBits.set(State::DIRTY_BIT_PACK_STATE);
    mReadPixelsDirtyBits.set(State::DIRTY_BIT_PACK_BUFFER_BINDING);
    mReadPixelsDirtyBits.set(State::DIRTY_BIT_READ_FRAMEBUFFER_BINDING);
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
    mClearDirtyBits.set(State::DIRTY_BIT_DRAW_FRAMEBUFFER_BINDING);
    mClearDirtyObjects.set(State::DIRTY_OBJECT_DRAW_FRAMEBUFFER);

    mBlitDirtyBits.set(State::DIRTY_BIT_SCISSOR_TEST_ENABLED);
    mBlitDirtyBits.set(State::DIRTY_BIT_SCISSOR);
    mBlitDirtyBits.set(State::DIRTY_BIT_FRAMEBUFFER_SRGB);
    mBlitDirtyBits.set(State::DIRTY_BIT_READ_FRAMEBUFFER_BINDING);
    mBlitDirtyBits.set(State::DIRTY_BIT_DRAW_FRAMEBUFFER_BINDING);
    mBlitDirtyObjects.set(State::DIRTY_OBJECT_READ_FRAMEBUFFER);
    mBlitDirtyObjects.set(State::DIRTY_OBJECT_DRAW_FRAMEBUFFER);

    mComputeDirtyBits.set(State::DIRTY_BIT_SHADER_STORAGE_BUFFER_BINDING);
    mComputeDirtyBits.set(State::DIRTY_BIT_UNIFORM_BUFFER_BINDINGS);
    mComputeDirtyBits.set(State::DIRTY_BIT_ATOMIC_COUNTER_BUFFER_BINDING);
    mComputeDirtyBits.set(State::DIRTY_BIT_PROGRAM_BINDING);
    mComputeDirtyBits.set(State::DIRTY_BIT_PROGRAM_EXECUTABLE);
    mComputeDirtyBits.set(State::DIRTY_BIT_TEXTURE_BINDINGS);
    mComputeDirtyBits.set(State::DIRTY_BIT_SAMPLER_BINDINGS);
    mComputeDirtyBits.set(State::DIRTY_BIT_IMAGE_BINDINGS);
    mComputeDirtyBits.set(State::DIRTY_BIT_DISPATCH_INDIRECT_BUFFER_BINDING);
    mComputeDirtyObjects.set(State::DIRTY_OBJECT_TEXTURES);
    mComputeDirtyObjects.set(State::DIRTY_OBJECT_PROGRAM);
    mComputeDirtyObjects.set(State::DIRTY_OBJECT_SAMPLERS);

    ANGLE_CONTEXT_TRY(mImplementation->initialize());
}

egl::Error Context::onDestroy(const egl::Display *display)
{
    if (mGLES1Renderer)
    {
        mGLES1Renderer->onDestroy(this, &mState);
    }

    ANGLE_TRY(unMakeCurrent(display));

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

    for (BindingPointer<Texture> &zeroTexture : mZeroTextures)
    {
        if (zeroTexture.get() != nullptr)
        {
            zeroTexture.set(this, nullptr);
        }
    }

    releaseShaderCompiler();

    mState.reset(this);

    mState.mBufferManager->release(this);
    mState.mShaderProgramManager->release(this);
    mState.mTextureManager->release(this);
    mState.mRenderbufferManager->release(this);
    mState.mSamplerManager->release(this);
    mState.mSyncManager->release(this);
    mState.mPathManager->release(this);
    mState.mFramebufferManager->release(this);
    mState.mProgramPipelineManager->release(this);
    mState.mMemoryObjectManager->release(this);

    mThreadPool.reset();

    mImplementation->onDestroy(this);

    return egl::NoError();
}

Context::~Context() {}

void Context::setLabel(EGLLabelKHR label)
{
    mLabel = label;
}

EGLLabelKHR Context::getLabel() const
{
    return mLabel;
}

egl::Error Context::makeCurrent(egl::Display *display, egl::Surface *surface)
{
    mDisplay = display;

    if (!mHasBeenCurrent)
    {
        initialize();
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

        mState.setViewportParams(0, 0, width, height);
        mState.setScissorParams(0, 0, width, height);

        mHasBeenCurrent = true;
    }

    // TODO(jmadill): Rework this when we support ContextImpl
    mState.setAllDirtyBits();
    mState.setAllDirtyObjects();

    ANGLE_TRY(setDefaultFramebuffer(surface));

    // Notify the renderer of a context switch.
    angle::Result implResult = mImplementation->onMakeCurrent(this);

    // If the implementation fails onMakeCurrent, unset the default framebuffer.
    if (implResult != angle::Result::Continue)
    {
        ANGLE_TRY(unsetDefaultFramebuffer());
        return angle::ResultToEGL(implResult);
    }

    return egl::NoError();
}

egl::Error Context::unMakeCurrent(const egl::Display *display)
{
    ANGLE_TRY(unsetDefaultFramebuffer());

    return angle::ResultToEGL(mImplementation->onUnMakeCurrent(this));
}

GLuint Context::createBuffer()
{
    return mState.mBufferManager->createBuffer();
}

GLuint Context::createProgram()
{
    return mState.mShaderProgramManager->createProgram(mImplementation.get());
}

GLuint Context::createShader(ShaderType type)
{
    return mState.mShaderProgramManager->createShader(mImplementation.get(), mState.mLimitations,
                                                      type);
}

GLuint Context::createTexture()
{
    return mState.mTextureManager->createTexture();
}

GLuint Context::createRenderbuffer()
{
    return mState.mRenderbufferManager->createRenderbuffer();
}

void Context::tryGenPaths(GLsizei range, GLuint *createdOut)
{
    ANGLE_CONTEXT_TRY(mState.mPathManager->createPaths(this, range, createdOut));
}

GLuint Context::genPaths(GLsizei range)
{
    GLuint created = 0;
    tryGenPaths(range, &created);
    return created;
}

// Returns an unused framebuffer name
GLuint Context::createFramebuffer()
{
    return mState.mFramebufferManager->createFramebuffer();
}

void Context::genFencesNV(GLsizei n, GLuint *fences)
{
    for (int i = 0; i < n; i++)
    {
        GLuint handle = mFenceNVHandleAllocator.allocate();
        mFenceNVMap.assign(handle, new FenceNV(mImplementation->createFenceNV()));
        fences[i] = handle;
    }
}

GLuint Context::createProgramPipeline()
{
    return mState.mProgramPipelineManager->createProgramPipeline();
}

GLuint Context::createShaderProgramv(ShaderType type, GLsizei count, const GLchar *const *strings)
{
    UNIMPLEMENTED();
    return 0u;
}

GLuint Context::createMemoryObject()
{
    return mState.mMemoryObjectManager->createMemoryObject(mImplementation.get());
}

void Context::deleteBuffer(GLuint bufferName)
{
    Buffer *buffer = mState.mBufferManager->getBuffer(bufferName);
    if (buffer)
    {
        detachBuffer(buffer);
    }

    mState.mBufferManager->deleteObject(this, bufferName);
}

void Context::deleteShader(GLuint shader)
{
    mState.mShaderProgramManager->deleteShader(this, shader);
}

void Context::deleteProgram(GLuint program)
{
    mState.mShaderProgramManager->deleteProgram(this, program);
}

void Context::deleteTexture(GLuint texture)
{
    if (mState.mTextureManager->getTexture(texture))
    {
        detachTexture(texture);
    }

    mState.mTextureManager->deleteObject(this, texture);
}

void Context::deleteRenderbuffer(GLuint renderbuffer)
{
    if (mState.mRenderbufferManager->getRenderbuffer(renderbuffer))
    {
        detachRenderbuffer(renderbuffer);
    }

    mState.mRenderbufferManager->deleteObject(this, renderbuffer);
}

void Context::deleteSync(GLsync sync)
{
    // The spec specifies the underlying Fence object is not deleted until all current
    // wait commands finish. However, since the name becomes invalid, we cannot query the fence,
    // and since our API is currently designed for being called from a single thread, we can delete
    // the fence immediately.
    mState.mSyncManager->deleteObject(this, static_cast<GLuint>(reinterpret_cast<uintptr_t>(sync)));
}

void Context::deleteProgramPipeline(GLuint pipeline)
{
    if (mState.mProgramPipelineManager->getProgramPipeline(pipeline))
    {
        detachProgramPipeline(pipeline);
    }

    mState.mProgramPipelineManager->deleteObject(this, pipeline);
}

void Context::deleteMemoryObject(GLuint memoryObject)
{
    mState.mMemoryObjectManager->deleteMemoryObject(this, memoryObject);
}

void Context::deletePaths(GLuint first, GLsizei range)
{
    mState.mPathManager->deletePaths(first, range);
}

bool Context::isPath(GLuint path) const
{
    const auto *pathObj = mState.mPathManager->getPath(path);
    if (pathObj == nullptr)
        return false;

    return pathObj->hasPathData();
}

bool Context::isPathGenerated(GLuint path) const
{
    return mState.mPathManager->hasPath(path);
}

void Context::pathCommands(GLuint path,
                           GLsizei numCommands,
                           const GLubyte *commands,
                           GLsizei numCoords,
                           GLenum coordType,
                           const void *coords)
{
    auto *pathObject = mState.mPathManager->getPath(path);

    ANGLE_CONTEXT_TRY(pathObject->setCommands(numCommands, commands, numCoords, coordType, coords));
}

void Context::pathParameterf(GLuint path, GLenum pname, GLfloat value)
{
    Path *pathObj = mState.mPathManager->getPath(path);

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

void Context::pathParameteri(GLuint path, GLenum pname, GLint value)
{
    // TODO(jmadill): Should use proper clamping/casting.
    pathParameterf(path, pname, static_cast<GLfloat>(value));
}

void Context::getPathParameterfv(GLuint path, GLenum pname, GLfloat *value)
{
    const Path *pathObj = mState.mPathManager->getPath(path);

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

void Context::getPathParameteriv(GLuint path, GLenum pname, GLint *value)
{
    GLfloat val = 0.0f;
    getPathParameterfv(path, pname, value != nullptr ? &val : nullptr);
    if (value)
        *value = static_cast<GLint>(val);
}

void Context::pathStencilFunc(GLenum func, GLint ref, GLuint mask)
{
    mState.setPathStencilFunc(func, ref, mask);
}

// GL_CHROMIUM_lose_context
void Context::loseContext(GraphicsResetStatus current, GraphicsResetStatus other)
{
    // TODO(geofflang): mark the rest of the share group lost. Requires access to the entire share
    // group from a context. http://anglebug.com/3379
    markContextLost(current);
}

void Context::deleteFramebuffer(GLuint framebuffer)
{
    if (mState.mFramebufferManager->getFramebuffer(framebuffer))
    {
        detachFramebuffer(framebuffer);
    }

    mState.mFramebufferManager->deleteObject(this, framebuffer);
}

void Context::deleteFencesNV(GLsizei n, const GLuint *fences)
{
    for (int i = 0; i < n; i++)
    {
        GLuint fence = fences[i];

        FenceNV *fenceObject = nullptr;
        if (mFenceNVMap.erase(fence, &fenceObject))
        {
            mFenceNVHandleAllocator.release(fence);
            delete fenceObject;
        }
    }
}

Buffer *Context::getBuffer(GLuint handle) const
{
    return mState.mBufferManager->getBuffer(handle);
}

Renderbuffer *Context::getRenderbuffer(GLuint handle) const
{
    return mState.mRenderbufferManager->getRenderbuffer(handle);
}

Sync *Context::getSync(GLsync handle) const
{
    return mState.mSyncManager->getSync(static_cast<GLuint>(reinterpret_cast<uintptr_t>(handle)));
}

VertexArray *Context::getVertexArray(GLuint handle) const
{
    return mVertexArrayMap.query(handle);
}

Sampler *Context::getSampler(GLuint handle) const
{
    return mState.mSamplerManager->getSampler(handle);
}

TransformFeedback *Context::getTransformFeedback(GLuint handle) const
{
    return mTransformFeedbackMap.query(handle);
}

ProgramPipeline *Context::getProgramPipeline(GLuint handle) const
{
    return mState.mProgramPipelineManager->getProgramPipeline(handle);
}

gl::LabeledObject *Context::getLabeledObject(GLenum identifier, GLuint name) const
{
    switch (identifier)
    {
        case GL_BUFFER:
            return getBuffer(name);
        case GL_SHADER:
            return getShader(name);
        case GL_PROGRAM:
            return getProgramNoResolveLink(name);
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

gl::LabeledObject *Context::getLabeledObjectFromPtr(const void *ptr) const
{
    return getSync(reinterpret_cast<GLsync>(const_cast<void *>(ptr)));
}

void Context::objectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar *label)
{
    gl::LabeledObject *object = getLabeledObject(identifier, name);
    ASSERT(object != nullptr);

    std::string labelName = GetObjectLabelFromPointer(length, label);
    object->setLabel(this, labelName);

    // TODO(jmadill): Determine if the object is dirty based on 'name'. Conservatively assume the
    // specified object is active until we do this.
    mState.setObjectDirty(identifier);
}

void Context::objectPtrLabel(const void *ptr, GLsizei length, const GLchar *label)
{
    gl::LabeledObject *object = getLabeledObjectFromPtr(ptr);
    ASSERT(object != nullptr);

    std::string labelName = GetObjectLabelFromPointer(length, label);
    object->setLabel(this, labelName);
}

void Context::getObjectLabel(GLenum identifier,
                             GLuint name,
                             GLsizei bufSize,
                             GLsizei *length,
                             GLchar *label) const
{
    gl::LabeledObject *object = getLabeledObject(identifier, name);
    ASSERT(object != nullptr);

    const std::string &objectLabel = object->getLabel();
    GetObjectLabelBase(objectLabel, bufSize, length, label);
}

void Context::getObjectPtrLabel(const void *ptr,
                                GLsizei bufSize,
                                GLsizei *length,
                                GLchar *label) const
{
    gl::LabeledObject *object = getLabeledObjectFromPtr(ptr);
    ASSERT(object != nullptr);

    const std::string &objectLabel = object->getLabel();
    GetObjectLabelBase(objectLabel, bufSize, length, label);
}

bool Context::isSampler(GLuint samplerName) const
{
    return mState.mSamplerManager->isSampler(samplerName);
}

void Context::bindTexture(TextureType target, GLuint handle)
{
    Texture *texture = nullptr;

    if (handle == 0)
    {
        texture = mZeroTextures[target].get();
    }
    else
    {
        texture =
            mState.mTextureManager->checkTextureAllocation(mImplementation.get(), handle, target);
    }

    ASSERT(texture);
    mState.setSamplerTexture(this, target, texture);
    mStateCache.onActiveTextureChange(this);
}

void Context::bindReadFramebuffer(GLuint framebufferHandle)
{
    Framebuffer *framebuffer = mState.mFramebufferManager->checkFramebufferAllocation(
        mImplementation.get(), mState.mCaps, framebufferHandle);
    mState.setReadFramebufferBinding(framebuffer);
    mReadFramebufferObserverBinding.bind(framebuffer);
}

void Context::bindDrawFramebuffer(GLuint framebufferHandle)
{
    Framebuffer *framebuffer = mState.mFramebufferManager->checkFramebufferAllocation(
        mImplementation.get(), mState.mCaps, framebufferHandle);
    mState.setDrawFramebufferBinding(framebuffer);
    mDrawFramebufferObserverBinding.bind(framebuffer);
    mStateCache.onDrawFramebufferChange(this);
}

void Context::bindVertexArray(GLuint vertexArrayHandle)
{
    VertexArray *vertexArray = checkVertexArrayAllocation(vertexArrayHandle);
    mState.setVertexArrayBinding(this, vertexArray);
    mVertexArrayObserverBinding.bind(vertexArray);
    mStateCache.onVertexArrayBindingChange(this);
}

void Context::bindVertexBuffer(GLuint bindingIndex,
                               GLuint bufferHandle,
                               GLintptr offset,
                               GLsizei stride)
{
    Buffer *buffer =
        mState.mBufferManager->checkBufferAllocation(mImplementation.get(), bufferHandle);
    mState.bindVertexBuffer(this, bindingIndex, buffer, offset, stride);
    mStateCache.onVertexArrayStateChange(this);
}

void Context::bindSampler(GLuint textureUnit, GLuint samplerHandle)
{
    ASSERT(textureUnit < mState.mCaps.maxCombinedTextureImageUnits);
    Sampler *sampler =
        mState.mSamplerManager->checkSamplerAllocation(mImplementation.get(), samplerHandle);
    mState.setSamplerBinding(this, textureUnit, sampler);
    mSamplerObserverBindings[textureUnit].bind(sampler);
    mStateCache.onActiveTextureChange(this);
}

void Context::bindImageTexture(GLuint unit,
                               GLuint texture,
                               GLint level,
                               GLboolean layered,
                               GLint layer,
                               GLenum access,
                               GLenum format)
{
    Texture *tex = mState.mTextureManager->getTexture(texture);
    mState.setImageUnit(this, unit, tex, level, layered, layer, access, format);
}

void Context::useProgram(GLuint program)
{
    ANGLE_CONTEXT_TRY(mState.setProgram(this, getProgramResolveLink(program)));
    mStateCache.onProgramExecutableChange(this);
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
    mState.setTransformFeedbackBinding(this, transformFeedback);
}

void Context::bindProgramPipeline(GLuint pipelineHandle)
{
    ProgramPipeline *pipeline = mState.mProgramPipelineManager->checkProgramPipelineAllocation(
        mImplementation.get(), pipelineHandle);
    mState.setProgramPipelineBinding(this, pipeline);
}

void Context::beginQuery(QueryType target, GLuint query)
{
    Query *queryObject = getQuery(query, true, target);
    ASSERT(queryObject);

    // begin query
    ANGLE_CONTEXT_TRY(queryObject->begin(this));

    // set query as active for specified target only if begin succeeded
    mState.setActiveQuery(this, target, queryObject);
    mStateCache.onQueryChange(this);
}

void Context::endQuery(QueryType target)
{
    Query *queryObject = mState.getActiveQuery(target);
    ASSERT(queryObject);

    // Intentionally don't call try here. We don't want an early return.
    (void)(queryObject->end(this));

    // Always unbind the query, even if there was an error. This may delete the query object.
    mState.setActiveQuery(this, target, nullptr);
    mStateCache.onQueryChange(this);
}

void Context::queryCounter(GLuint id, QueryType target)
{
    ASSERT(target == QueryType::Timestamp);

    Query *queryObject = getQuery(id, true, target);
    ASSERT(queryObject);

    ANGLE_CONTEXT_TRY(queryObject->queryCounter(this));
}

void Context::getQueryiv(QueryType target, GLenum pname, GLint *params)
{
    switch (pname)
    {
        case GL_CURRENT_QUERY_EXT:
            params[0] = mState.getActiveQueryId(target);
            break;
        case GL_QUERY_COUNTER_BITS_EXT:
            switch (target)
            {
                case QueryType::TimeElapsed:
                    params[0] = getExtensions().queryCounterBitsTimeElapsed;
                    break;
                case QueryType::Timestamp:
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

void Context::getQueryivRobust(QueryType target,
                               GLenum pname,
                               GLsizei bufSize,
                               GLsizei *length,
                               GLint *params)
{
    getQueryiv(target, pname, params);
}

void Context::getUnsignedBytev(GLenum pname, GLubyte *data)
{
    UNIMPLEMENTED();
}

void Context::getUnsignedBytei_v(GLenum target, GLuint index, GLubyte *data)
{
    UNIMPLEMENTED();
}

void Context::getQueryObjectiv(GLuint id, GLenum pname, GLint *params)
{
    ANGLE_CONTEXT_TRY(GetQueryObjectParameter(this, getQuery(id), pname, params));
}

void Context::getQueryObjectivRobust(GLuint id,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLint *params)
{
    getQueryObjectiv(id, pname, params);
}

void Context::getQueryObjectuiv(GLuint id, GLenum pname, GLuint *params)
{
    ANGLE_CONTEXT_TRY(GetQueryObjectParameter(this, getQuery(id), pname, params));
}

void Context::getQueryObjectuivRobust(GLuint id,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLuint *params)
{
    getQueryObjectuiv(id, pname, params);
}

void Context::getQueryObjecti64v(GLuint id, GLenum pname, GLint64 *params)
{
    ANGLE_CONTEXT_TRY(GetQueryObjectParameter(this, getQuery(id), pname, params));
}

void Context::getQueryObjecti64vRobust(GLuint id,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       GLint64 *params)
{
    getQueryObjecti64v(id, pname, params);
}

void Context::getQueryObjectui64v(GLuint id, GLenum pname, GLuint64 *params)
{
    ANGLE_CONTEXT_TRY(GetQueryObjectParameter(this, getQuery(id), pname, params));
}

void Context::getQueryObjectui64vRobust(GLuint id,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLuint64 *params)
{
    getQueryObjectui64v(id, pname, params);
}

Framebuffer *Context::getFramebuffer(GLuint handle) const
{
    return mState.mFramebufferManager->getFramebuffer(handle);
}

FenceNV *Context::getFenceNV(GLuint handle)
{
    return mFenceNVMap.query(handle);
}

Query *Context::getQuery(GLuint handle, bool create, QueryType type)
{
    if (!mQueryMap.contains(handle))
    {
        return nullptr;
    }

    Query *query = mQueryMap.query(handle);
    if (!query && create)
    {
        ASSERT(type != QueryType::InvalidEnum);
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

Texture *Context::getTextureByType(TextureType type) const
{
    ASSERT(ValidTextureTarget(this, type) || ValidTextureExternalTarget(this, type));
    return mState.getTargetTexture(type);
}

Texture *Context::getTextureByTarget(TextureTarget target) const
{
    return getTextureByType(TextureTargetToType(target));
}

Texture *Context::getSamplerTexture(unsigned int sampler, TextureType type) const
{
    return mState.getSamplerTexture(sampler, type);
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
            mState.getBooleanv(pname, params);
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
            params[0] = mState.mCaps.minAliasedLineWidth;
            params[1] = mState.mCaps.maxAliasedLineWidth;
            break;
        case GL_ALIASED_POINT_SIZE_RANGE:
            params[0] = mState.mCaps.minAliasedPointSize;
            params[1] = mState.mCaps.maxAliasedPointSize;
            break;
        case GL_SMOOTH_POINT_SIZE_RANGE:
            params[0] = mState.mCaps.minSmoothPointSize;
            params[1] = mState.mCaps.maxSmoothPointSize;
            break;
        case GL_SMOOTH_LINE_WIDTH_RANGE:
            params[0] = mState.mCaps.minSmoothLineWidth;
            params[1] = mState.mCaps.maxSmoothLineWidth;
            break;
        case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT:
            ASSERT(mState.mExtensions.textureFilterAnisotropic);
            *params = mState.mExtensions.maxTextureAnisotropy;
            break;
        case GL_MAX_TEXTURE_LOD_BIAS:
            *params = mState.mCaps.maxLODBias;
            break;

        case GL_PATH_MODELVIEW_MATRIX_CHROMIUM:
        case GL_PATH_PROJECTION_MATRIX_CHROMIUM:
        {
            // GLES1 emulation: // GL_PATH_(MODELVIEW|PROJECTION)_MATRIX_CHROMIUM collides with the
            // GLES1 constants for modelview/projection matrix.
            if (getClientVersion() < Version(2, 0))
            {
                mState.getFloatv(pname, params);
            }
            else
            {
                ASSERT(mState.mExtensions.pathRendering);
                const GLfloat *m = mState.getPathRenderingMatrix(pname);
                memcpy(params, m, 16 * sizeof(GLfloat));
            }
        }
        break;

        default:
            mState.getFloatv(pname, params);
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
            *params = mState.mCaps.maxVertexAttributes;
            break;
        case GL_MAX_VERTEX_UNIFORM_VECTORS:
            *params = mState.mCaps.maxVertexUniformVectors;
            break;
        case GL_MAX_VERTEX_UNIFORM_COMPONENTS:
            *params = mState.mCaps.maxShaderUniformComponents[ShaderType::Vertex];
            break;
        case GL_MAX_VARYING_VECTORS:
            *params = mState.mCaps.maxVaryingVectors;
            break;
        case GL_MAX_VARYING_COMPONENTS:
            *params = mState.mCaps.maxVertexOutputComponents;
            break;
        case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
            *params = mState.mCaps.maxCombinedTextureImageUnits;
            break;
        case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
            *params = mState.mCaps.maxShaderTextureImageUnits[ShaderType::Vertex];
            break;
        case GL_MAX_TEXTURE_IMAGE_UNITS:
            *params = mState.mCaps.maxShaderTextureImageUnits[ShaderType::Fragment];
            break;
        case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
            *params = mState.mCaps.maxFragmentUniformVectors;
            break;
        case GL_MAX_FRAGMENT_UNIFORM_COMPONENTS:
            *params = mState.mCaps.maxShaderUniformComponents[ShaderType::Fragment];
            break;
        case GL_MAX_RENDERBUFFER_SIZE:
            *params = mState.mCaps.maxRenderbufferSize;
            break;
        case GL_MAX_COLOR_ATTACHMENTS_EXT:
            *params = mState.mCaps.maxColorAttachments;
            break;
        case GL_MAX_DRAW_BUFFERS_EXT:
            *params = mState.mCaps.maxDrawBuffers;
            break;
        case GL_SUBPIXEL_BITS:
            *params = mState.mCaps.subPixelBits;
            break;
        case GL_MAX_TEXTURE_SIZE:
            *params = mState.mCaps.max2DTextureSize;
            break;
        case GL_MAX_RECTANGLE_TEXTURE_SIZE_ANGLE:
            *params = mState.mCaps.maxRectangleTextureSize;
            break;
        case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
            *params = mState.mCaps.maxCubeMapTextureSize;
            break;
        case GL_MAX_3D_TEXTURE_SIZE:
            *params = mState.mCaps.max3DTextureSize;
            break;
        case GL_MAX_ARRAY_TEXTURE_LAYERS:
            *params = mState.mCaps.maxArrayTextureLayers;
            break;
        case GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT:
            *params = mState.mCaps.uniformBufferOffsetAlignment;
            break;
        case GL_MAX_UNIFORM_BUFFER_BINDINGS:
            *params = mState.mCaps.maxUniformBufferBindings;
            break;
        case GL_MAX_VERTEX_UNIFORM_BLOCKS:
            *params = mState.mCaps.maxShaderUniformBlocks[ShaderType::Vertex];
            break;
        case GL_MAX_FRAGMENT_UNIFORM_BLOCKS:
            *params = mState.mCaps.maxShaderUniformBlocks[ShaderType::Fragment];
            break;
        case GL_MAX_COMBINED_UNIFORM_BLOCKS:
            *params = mState.mCaps.maxCombinedUniformBlocks;
            break;
        case GL_MAX_VERTEX_OUTPUT_COMPONENTS:
            *params = mState.mCaps.maxVertexOutputComponents;
            break;
        case GL_MAX_FRAGMENT_INPUT_COMPONENTS:
            *params = mState.mCaps.maxFragmentInputComponents;
            break;
        case GL_MIN_PROGRAM_TEXEL_OFFSET:
            *params = mState.mCaps.minProgramTexelOffset;
            break;
        case GL_MAX_PROGRAM_TEXEL_OFFSET:
            *params = mState.mCaps.maxProgramTexelOffset;
            break;
        case GL_MAJOR_VERSION:
            *params = getClientVersion().major;
            break;
        case GL_MINOR_VERSION:
            *params = getClientVersion().minor;
            break;
        case GL_MAX_ELEMENTS_INDICES:
            *params = mState.mCaps.maxElementsIndices;
            break;
        case GL_MAX_ELEMENTS_VERTICES:
            *params = mState.mCaps.maxElementsVertices;
            break;
        case GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS:
            *params = mState.mCaps.maxTransformFeedbackInterleavedComponents;
            break;
        case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS:
            *params = mState.mCaps.maxTransformFeedbackSeparateAttributes;
            break;
        case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS:
            *params = mState.mCaps.maxTransformFeedbackSeparateComponents;
            break;
        case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
            *params = static_cast<GLint>(mState.mCaps.compressedTextureFormats.size());
            break;
        case GL_MAX_SAMPLES_ANGLE:
            *params = mState.mCaps.maxSamples;
            break;
        case GL_MAX_VIEWPORT_DIMS:
        {
            params[0] = mState.mCaps.maxViewportWidth;
            params[1] = mState.mCaps.maxViewportHeight;
        }
        break;
        case GL_COMPRESSED_TEXTURE_FORMATS:
            std::copy(mState.mCaps.compressedTextureFormats.begin(),
                      mState.mCaps.compressedTextureFormats.end(), params);
            break;
        case GL_RESET_NOTIFICATION_STRATEGY_EXT:
            *params = mResetStrategy;
            break;
        case GL_NUM_SHADER_BINARY_FORMATS:
            *params = static_cast<GLint>(mState.mCaps.shaderBinaryFormats.size());
            break;
        case GL_SHADER_BINARY_FORMATS:
            std::copy(mState.mCaps.shaderBinaryFormats.begin(),
                      mState.mCaps.shaderBinaryFormats.end(), params);
            break;
        case GL_NUM_PROGRAM_BINARY_FORMATS:
            *params = static_cast<GLint>(mState.mCaps.programBinaryFormats.size());
            break;
        case GL_PROGRAM_BINARY_FORMATS:
            std::copy(mState.mCaps.programBinaryFormats.begin(),
                      mState.mCaps.programBinaryFormats.end(), params);
            break;
        case GL_NUM_EXTENSIONS:
            *params = static_cast<GLint>(mExtensionStrings.size());
            break;

        // GL_ANGLE_request_extension
        case GL_NUM_REQUESTABLE_EXTENSIONS_ANGLE:
            *params = static_cast<GLint>(mRequestableExtensionStrings.size());
            break;

        // GL_KHR_debug
        case GL_MAX_DEBUG_MESSAGE_LENGTH:
            *params = mState.mExtensions.maxDebugMessageLength;
            break;
        case GL_MAX_DEBUG_LOGGED_MESSAGES:
            *params = mState.mExtensions.maxDebugLoggedMessages;
            break;
        case GL_MAX_DEBUG_GROUP_STACK_DEPTH:
            *params = mState.mExtensions.maxDebugGroupStackDepth;
            break;
        case GL_MAX_LABEL_LENGTH:
            *params = mState.mExtensions.maxLabelLength;
            break;

        // GL_OVR_multiview2
        case GL_MAX_VIEWS_OVR:
            *params = mState.mExtensions.maxViews;
            break;

        // GL_EXT_disjoint_timer_query
        case GL_GPU_DISJOINT_EXT:
            *params = mImplementation->getGPUDisjoint();
            break;
        case GL_MAX_FRAMEBUFFER_WIDTH:
            *params = mState.mCaps.maxFramebufferWidth;
            break;
        case GL_MAX_FRAMEBUFFER_HEIGHT:
            *params = mState.mCaps.maxFramebufferHeight;
            break;
        case GL_MAX_FRAMEBUFFER_SAMPLES:
            *params = mState.mCaps.maxFramebufferSamples;
            break;
        case GL_MAX_SAMPLE_MASK_WORDS:
            *params = mState.mCaps.maxSampleMaskWords;
            break;
        case GL_MAX_COLOR_TEXTURE_SAMPLES:
            *params = mState.mCaps.maxColorTextureSamples;
            break;
        case GL_MAX_DEPTH_TEXTURE_SAMPLES:
            *params = mState.mCaps.maxDepthTextureSamples;
            break;
        case GL_MAX_INTEGER_SAMPLES:
            *params = mState.mCaps.maxIntegerSamples;
            break;
        case GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET:
            *params = mState.mCaps.maxVertexAttribRelativeOffset;
            break;
        case GL_MAX_VERTEX_ATTRIB_BINDINGS:
            *params = mState.mCaps.maxVertexAttribBindings;
            break;
        case GL_MAX_VERTEX_ATTRIB_STRIDE:
            *params = mState.mCaps.maxVertexAttribStride;
            break;
        case GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS:
            *params = mState.mCaps.maxShaderAtomicCounterBuffers[ShaderType::Vertex];
            break;
        case GL_MAX_VERTEX_ATOMIC_COUNTERS:
            *params = mState.mCaps.maxShaderAtomicCounters[ShaderType::Vertex];
            break;
        case GL_MAX_VERTEX_IMAGE_UNIFORMS:
            *params = mState.mCaps.maxShaderImageUniforms[ShaderType::Vertex];
            break;
        case GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS:
            *params = mState.mCaps.maxShaderStorageBlocks[ShaderType::Vertex];
            break;
        case GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS:
            *params = mState.mCaps.maxShaderAtomicCounterBuffers[ShaderType::Fragment];
            break;
        case GL_MAX_FRAGMENT_ATOMIC_COUNTERS:
            *params = mState.mCaps.maxShaderAtomicCounters[ShaderType::Fragment];
            break;
        case GL_MAX_FRAGMENT_IMAGE_UNIFORMS:
            *params = mState.mCaps.maxShaderImageUniforms[ShaderType::Fragment];
            break;
        case GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS:
            *params = mState.mCaps.maxShaderStorageBlocks[ShaderType::Fragment];
            break;
        case GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET:
            *params = mState.mCaps.minProgramTextureGatherOffset;
            break;
        case GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET:
            *params = mState.mCaps.maxProgramTextureGatherOffset;
            break;
        case GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS:
            *params = mState.mCaps.maxComputeWorkGroupInvocations;
            break;
        case GL_MAX_COMPUTE_UNIFORM_BLOCKS:
            *params = mState.mCaps.maxShaderUniformBlocks[ShaderType::Compute];
            break;
        case GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS:
            *params = mState.mCaps.maxShaderTextureImageUnits[ShaderType::Compute];
            break;
        case GL_MAX_COMPUTE_SHARED_MEMORY_SIZE:
            *params = mState.mCaps.maxComputeSharedMemorySize;
            break;
        case GL_MAX_COMPUTE_UNIFORM_COMPONENTS:
            *params = mState.mCaps.maxShaderUniformComponents[ShaderType::Compute];
            break;
        case GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS:
            *params = mState.mCaps.maxShaderAtomicCounterBuffers[ShaderType::Compute];
            break;
        case GL_MAX_COMPUTE_ATOMIC_COUNTERS:
            *params = mState.mCaps.maxShaderAtomicCounters[ShaderType::Compute];
            break;
        case GL_MAX_COMPUTE_IMAGE_UNIFORMS:
            *params = mState.mCaps.maxShaderImageUniforms[ShaderType::Compute];
            break;
        case GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS:
            *params = static_cast<GLint>(
                mState.mCaps.maxCombinedShaderUniformComponents[ShaderType::Compute]);
            break;
        case GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS:
            *params = mState.mCaps.maxShaderStorageBlocks[ShaderType::Compute];
            break;
        case GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES:
            *params = mState.mCaps.maxCombinedShaderOutputResources;
            break;
        case GL_MAX_UNIFORM_LOCATIONS:
            *params = mState.mCaps.maxUniformLocations;
            break;
        case GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS:
            *params = mState.mCaps.maxAtomicCounterBufferBindings;
            break;
        case GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE:
            *params = mState.mCaps.maxAtomicCounterBufferSize;
            break;
        case GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS:
            *params = mState.mCaps.maxCombinedAtomicCounterBuffers;
            break;
        case GL_MAX_COMBINED_ATOMIC_COUNTERS:
            *params = mState.mCaps.maxCombinedAtomicCounters;
            break;
        case GL_MAX_IMAGE_UNITS:
            *params = mState.mCaps.maxImageUnits;
            break;
        case GL_MAX_COMBINED_IMAGE_UNIFORMS:
            *params = mState.mCaps.maxCombinedImageUniforms;
            break;
        case GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS:
            *params = mState.mCaps.maxShaderStorageBufferBindings;
            break;
        case GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS:
            *params = mState.mCaps.maxCombinedShaderStorageBlocks;
            break;
        case GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT:
            *params = mState.mCaps.shaderStorageBufferOffsetAlignment;
            break;

        // GL_EXT_geometry_shader
        case GL_MAX_FRAMEBUFFER_LAYERS_EXT:
            *params = mState.mCaps.maxFramebufferLayers;
            break;
        case GL_LAYER_PROVOKING_VERTEX_EXT:
            *params = mState.mCaps.layerProvokingVertex;
            break;
        case GL_MAX_GEOMETRY_UNIFORM_COMPONENTS_EXT:
            *params = mState.mCaps.maxShaderUniformComponents[ShaderType::Geometry];
            break;
        case GL_MAX_GEOMETRY_UNIFORM_BLOCKS_EXT:
            *params = mState.mCaps.maxShaderUniformBlocks[ShaderType::Geometry];
            break;
        case GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS_EXT:
            *params = static_cast<GLint>(
                mState.mCaps.maxCombinedShaderUniformComponents[ShaderType::Geometry]);
            break;
        case GL_MAX_GEOMETRY_INPUT_COMPONENTS_EXT:
            *params = mState.mCaps.maxGeometryInputComponents;
            break;
        case GL_MAX_GEOMETRY_OUTPUT_COMPONENTS_EXT:
            *params = mState.mCaps.maxGeometryOutputComponents;
            break;
        case GL_MAX_GEOMETRY_OUTPUT_VERTICES_EXT:
            *params = mState.mCaps.maxGeometryOutputVertices;
            break;
        case GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS_EXT:
            *params = mState.mCaps.maxGeometryTotalOutputComponents;
            break;
        case GL_MAX_GEOMETRY_SHADER_INVOCATIONS_EXT:
            *params = mState.mCaps.maxGeometryShaderInvocations;
            break;
        case GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS_EXT:
            *params = mState.mCaps.maxShaderTextureImageUnits[ShaderType::Geometry];
            break;
        case GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS_EXT:
            *params = mState.mCaps.maxShaderAtomicCounterBuffers[ShaderType::Geometry];
            break;
        case GL_MAX_GEOMETRY_ATOMIC_COUNTERS_EXT:
            *params = mState.mCaps.maxShaderAtomicCounters[ShaderType::Geometry];
            break;
        case GL_MAX_GEOMETRY_IMAGE_UNIFORMS_EXT:
            *params = mState.mCaps.maxShaderImageUniforms[ShaderType::Geometry];
            break;
        case GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS_EXT:
            *params = mState.mCaps.maxShaderStorageBlocks[ShaderType::Geometry];
            break;
        // GLES1 emulation: Caps queries
        case GL_MAX_TEXTURE_UNITS:
            *params = mState.mCaps.maxMultitextureUnits;
            break;
        case GL_MAX_MODELVIEW_STACK_DEPTH:
            *params = mState.mCaps.maxModelviewMatrixStackDepth;
            break;
        case GL_MAX_PROJECTION_STACK_DEPTH:
            *params = mState.mCaps.maxProjectionMatrixStackDepth;
            break;
        case GL_MAX_TEXTURE_STACK_DEPTH:
            *params = mState.mCaps.maxTextureMatrixStackDepth;
            break;
        case GL_MAX_LIGHTS:
            *params = mState.mCaps.maxLights;
            break;
        case GL_MAX_CLIP_PLANES:
            *params = mState.mCaps.maxClipPlanes;
            break;
        // GLES1 emulation: Vertex attribute queries
        case GL_VERTEX_ARRAY_BUFFER_BINDING:
        case GL_NORMAL_ARRAY_BUFFER_BINDING:
        case GL_COLOR_ARRAY_BUFFER_BINDING:
        case GL_POINT_SIZE_ARRAY_BUFFER_BINDING_OES:
        case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING:
            getVertexAttribiv(static_cast<GLuint>(vertexArrayIndex(ParamToVertexArrayType(pname))),
                              GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, params);
            break;
        case GL_VERTEX_ARRAY_STRIDE:
        case GL_NORMAL_ARRAY_STRIDE:
        case GL_COLOR_ARRAY_STRIDE:
        case GL_POINT_SIZE_ARRAY_STRIDE_OES:
        case GL_TEXTURE_COORD_ARRAY_STRIDE:
            getVertexAttribiv(static_cast<GLuint>(vertexArrayIndex(ParamToVertexArrayType(pname))),
                              GL_VERTEX_ATTRIB_ARRAY_STRIDE, params);
            break;
        case GL_VERTEX_ARRAY_SIZE:
        case GL_COLOR_ARRAY_SIZE:
        case GL_TEXTURE_COORD_ARRAY_SIZE:
            getVertexAttribiv(static_cast<GLuint>(vertexArrayIndex(ParamToVertexArrayType(pname))),
                              GL_VERTEX_ATTRIB_ARRAY_SIZE, params);
            break;
        case GL_VERTEX_ARRAY_TYPE:
        case GL_COLOR_ARRAY_TYPE:
        case GL_NORMAL_ARRAY_TYPE:
        case GL_POINT_SIZE_ARRAY_TYPE_OES:
        case GL_TEXTURE_COORD_ARRAY_TYPE:
            getVertexAttribiv(static_cast<GLuint>(vertexArrayIndex(ParamToVertexArrayType(pname))),
                              GL_VERTEX_ATTRIB_ARRAY_TYPE, params);
            break;

        // GL_KHR_parallel_shader_compile
        case GL_MAX_SHADER_COMPILER_THREADS_KHR:
            *params = mState.getMaxShaderCompilerThreads();
            break;

        // GL_EXT_blend_func_extended
        case GL_MAX_DUAL_SOURCE_DRAW_BUFFERS_EXT:
            *params = mState.mExtensions.maxDualSourceDrawBuffers;
            break;

        default:
            ANGLE_CONTEXT_TRY(mState.getIntegerv(this, pname, params));
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
            *params = mState.mCaps.maxElementIndex;
            break;
        case GL_MAX_UNIFORM_BLOCK_SIZE:
            *params = mState.mCaps.maxUniformBlockSize;
            break;
        case GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS:
            *params = mState.mCaps.maxCombinedShaderUniformComponents[ShaderType::Vertex];
            break;
        case GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS:
            *params = mState.mCaps.maxCombinedShaderUniformComponents[ShaderType::Fragment];
            break;
        case GL_MAX_SERVER_WAIT_TIMEOUT:
            *params = mState.mCaps.maxServerWaitTimeout;
            break;

        // GL_EXT_disjoint_timer_query
        case GL_TIMESTAMP_EXT:
            *params = mImplementation->getTimestamp();
            break;

        case GL_MAX_SHADER_STORAGE_BLOCK_SIZE:
            *params = mState.mCaps.maxShaderStorageBlockSize;
            break;
        default:
            UNREACHABLE();
            break;
    }
}

void Context::getPointerv(GLenum pname, void **params) const
{
    mState.getPointerv(this, pname, params);
}

void Context::getPointervRobustANGLERobust(GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           void **params)
{
    UNIMPLEMENTED();
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
                *data = mState.mCaps.maxComputeWorkGroupCount[index];
                break;
            case GL_MAX_COMPUTE_WORK_GROUP_SIZE:
                ASSERT(index < 3u);
                *data = mState.mCaps.maxComputeWorkGroupSize[index];
                break;
            default:
                mState.getIntegeri_v(target, index, data);
        }
    }
    else
    {
        CastIndexedStateValues(this, nativeType, target, index, numParams, data);
    }
}

void Context::getIntegeri_vRobust(GLenum target,
                                  GLuint index,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLint *data)
{
    getIntegeri_v(target, index, data);
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
        mState.getInteger64i_v(target, index, data);
    }
    else
    {
        CastIndexedStateValues(this, nativeType, target, index, numParams, data);
    }
}

void Context::getInteger64i_vRobust(GLenum target,
                                    GLuint index,
                                    GLsizei bufSize,
                                    GLsizei *length,
                                    GLint64 *data)
{
    getInteger64i_v(target, index, data);
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
        mState.getBooleani_v(target, index, data);
    }
    else
    {
        CastIndexedStateValues(this, nativeType, target, index, numParams, data);
    }
}

void Context::getBooleani_vRobust(GLenum target,
                                  GLuint index,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLboolean *data)
{
    getBooleani_v(target, index, data);
}

void Context::getBufferParameteriv(BufferBinding target, GLenum pname, GLint *params)
{
    Buffer *buffer = mState.getTargetBuffer(target);
    QueryBufferParameteriv(buffer, pname, params);
}

void Context::getBufferParameterivRobust(BufferBinding target,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLsizei *length,
                                         GLint *params)
{
    getBufferParameteriv(target, pname, params);
}

void Context::getFramebufferAttachmentParameteriv(GLenum target,
                                                  GLenum attachment,
                                                  GLenum pname,
                                                  GLint *params)
{
    const Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
    QueryFramebufferAttachmentParameteriv(this, framebuffer, attachment, pname, params);
}

void Context::getFramebufferAttachmentParameterivRobust(GLenum target,
                                                        GLenum attachment,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei *length,
                                                        GLint *params)
{
    getFramebufferAttachmentParameteriv(target, attachment, pname, params);
}

void Context::getRenderbufferParameteriv(GLenum target, GLenum pname, GLint *params)
{
    Renderbuffer *renderbuffer = mState.getCurrentRenderbuffer();
    QueryRenderbufferiv(this, renderbuffer, pname, params);
}

void Context::getRenderbufferParameterivRobust(GLenum target,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               GLsizei *length,
                                               GLint *params)
{
    getRenderbufferParameteriv(target, pname, params);
}

void Context::getTexParameterfv(TextureType target, GLenum pname, GLfloat *params)
{
    const Texture *const texture = getTextureByType(target);
    QueryTexParameterfv(texture, pname, params);
}

void Context::getTexParameterfvRobust(TextureType target,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLfloat *params)
{
    getTexParameterfv(target, pname, params);
}

void Context::getTexParameteriv(TextureType target, GLenum pname, GLint *params)
{
    const Texture *const texture = getTextureByType(target);
    QueryTexParameteriv(texture, pname, params);
}

void Context::getTexParameterIiv(TextureType target, GLenum pname, GLint *params)
{
    const Texture *const texture = getTextureByType(target);
    QueryTexParameterIiv(texture, pname, params);
}

void Context::getTexParameterIuiv(TextureType target, GLenum pname, GLuint *params)
{
    const Texture *const texture = getTextureByType(target);
    QueryTexParameterIuiv(texture, pname, params);
}

void Context::getTexParameterivRobust(TextureType target,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLint *params)
{
    getTexParameteriv(target, pname, params);
}

void Context::getTexParameterIivRobust(TextureType target,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       GLint *params)
{
    UNIMPLEMENTED();
}

void Context::getTexParameterIuivRobust(TextureType target,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLuint *params)
{
    UNIMPLEMENTED();
}

void Context::getTexLevelParameteriv(TextureTarget target, GLint level, GLenum pname, GLint *params)
{
    Texture *texture = getTextureByTarget(target);
    QueryTexLevelParameteriv(texture, target, level, pname, params);
}

void Context::getTexLevelParameterivRobust(TextureTarget target,
                                           GLint level,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLint *params)
{
    UNIMPLEMENTED();
}

void Context::getTexLevelParameterfv(TextureTarget target,
                                     GLint level,
                                     GLenum pname,
                                     GLfloat *params)
{
    Texture *texture = getTextureByTarget(target);
    QueryTexLevelParameterfv(texture, target, level, pname, params);
}

void Context::getTexLevelParameterfvRobust(TextureTarget target,
                                           GLint level,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLfloat *params)
{
    UNIMPLEMENTED();
}

void Context::texParameterf(TextureType target, GLenum pname, GLfloat param)
{
    Texture *const texture = getTextureByType(target);
    SetTexParameterf(this, texture, pname, param);
}

void Context::texParameterfv(TextureType target, GLenum pname, const GLfloat *params)
{
    Texture *const texture = getTextureByType(target);
    SetTexParameterfv(this, texture, pname, params);
}

void Context::texParameterfvRobust(TextureType target,
                                   GLenum pname,
                                   GLsizei bufSize,
                                   const GLfloat *params)
{
    texParameterfv(target, pname, params);
}

void Context::texParameteri(TextureType target, GLenum pname, GLint param)
{
    Texture *const texture = getTextureByType(target);
    SetTexParameteri(this, texture, pname, param);
}

void Context::texParameteriv(TextureType target, GLenum pname, const GLint *params)
{
    Texture *const texture = getTextureByType(target);
    SetTexParameteriv(this, texture, pname, params);
}

void Context::texParameterIiv(TextureType target, GLenum pname, const GLint *params)
{
    Texture *const texture = getTextureByType(target);
    SetTexParameterIiv(this, texture, pname, params);
}

void Context::texParameterIuiv(TextureType target, GLenum pname, const GLuint *params)
{
    Texture *const texture = getTextureByType(target);
    SetTexParameterIuiv(this, texture, pname, params);
}

void Context::texParameterivRobust(TextureType target,
                                   GLenum pname,
                                   GLsizei bufSize,
                                   const GLint *params)
{
    texParameteriv(target, pname, params);
}

void Context::texParameterIivRobust(TextureType target,
                                    GLenum pname,
                                    GLsizei bufSize,
                                    const GLint *params)
{
    UNIMPLEMENTED();
}

void Context::texParameterIuivRobust(TextureType target,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     const GLuint *params)
{
    UNIMPLEMENTED();
}

void Context::drawArraysInstanced(PrimitiveMode mode,
                                  GLint first,
                                  GLsizei count,
                                  GLsizei instanceCount)
{
    // No-op if count draws no primitives for given mode
    if (noopDrawInstanced(mode, count, instanceCount))
    {
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    ANGLE_CONTEXT_TRY(
        mImplementation->drawArraysInstanced(this, mode, first, count, instanceCount));
    MarkTransformFeedbackBufferUsage(this, count, instanceCount);
}

void Context::drawElementsInstanced(PrimitiveMode mode,
                                    GLsizei count,
                                    DrawElementsType type,
                                    const void *indices,
                                    GLsizei instances)
{
    // No-op if count draws no primitives for given mode
    if (noopDrawInstanced(mode, count, instances))
    {
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    ANGLE_CONTEXT_TRY(
        mImplementation->drawElementsInstanced(this, mode, count, type, indices, instances));
}

void Context::drawRangeElements(PrimitiveMode mode,
                                GLuint start,
                                GLuint end,
                                GLsizei count,
                                DrawElementsType type,
                                const void *indices)
{
    // No-op if count draws no primitives for given mode
    if (noopDraw(mode, count))
    {
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    ANGLE_CONTEXT_TRY(
        mImplementation->drawRangeElements(this, mode, start, end, count, type, indices));
}

void Context::drawArraysIndirect(PrimitiveMode mode, const void *indirect)
{
    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    ANGLE_CONTEXT_TRY(mImplementation->drawArraysIndirect(this, mode, indirect));
}

void Context::drawElementsIndirect(PrimitiveMode mode, DrawElementsType type, const void *indirect)
{
    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    ANGLE_CONTEXT_TRY(mImplementation->drawElementsIndirect(this, mode, type, indirect));
}

void Context::flush()
{
    ANGLE_CONTEXT_TRY(mImplementation->flush(this));
}

void Context::finish()
{
    ANGLE_CONTEXT_TRY(mImplementation->finish(this));
}

void Context::insertEventMarker(GLsizei length, const char *marker)
{
    ASSERT(mImplementation);
    mImplementation->insertEventMarker(length, marker);
}

void Context::pushGroupMarker(GLsizei length, const char *marker)
{
    ASSERT(mImplementation);

    if (marker == nullptr)
    {
        // From the EXT_debug_marker spec,
        // "If <marker> is null then an empty string is pushed on the stack."
        mImplementation->pushGroupMarker(length, "");
    }
    else
    {
        mImplementation->pushGroupMarker(length, marker);
    }
}

void Context::popGroupMarker()
{
    ASSERT(mImplementation);
    mImplementation->popGroupMarker();
}

void Context::bindUniformLocation(GLuint program, GLint location, const GLchar *name)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);

    programObject->bindUniformLocation(location, name);
}

void Context::coverageModulation(GLenum components)
{
    mState.setCoverageModulation(components);
}

void Context::matrixLoadf(GLenum matrixMode, const GLfloat *matrix)
{
    mState.loadPathRenderingMatrix(matrixMode, matrix);
}

void Context::matrixLoadIdentity(GLenum matrixMode)
{
    GLfloat I[16];
    angle::Matrix<GLfloat>::setToIdentity(I);

    mState.loadPathRenderingMatrix(matrixMode, I);
}

void Context::stencilFillPath(GLuint path, GLenum fillMode, GLuint mask)
{
    const auto *pathObj = mState.mPathManager->getPath(path);
    if (!pathObj)
        return;

    ANGLE_CONTEXT_TRY(syncStateForPathOperation());

    mImplementation->stencilFillPath(pathObj, fillMode, mask);
}

void Context::stencilStrokePath(GLuint path, GLint reference, GLuint mask)
{
    const auto *pathObj = mState.mPathManager->getPath(path);
    if (!pathObj)
        return;

    ANGLE_CONTEXT_TRY(syncStateForPathOperation());

    mImplementation->stencilStrokePath(pathObj, reference, mask);
}

void Context::coverFillPath(GLuint path, GLenum coverMode)
{
    const auto *pathObj = mState.mPathManager->getPath(path);
    if (!pathObj)
        return;

    ANGLE_CONTEXT_TRY(syncStateForPathOperation());

    mImplementation->coverFillPath(pathObj, coverMode);
}

void Context::coverStrokePath(GLuint path, GLenum coverMode)
{
    const auto *pathObj = mState.mPathManager->getPath(path);
    if (!pathObj)
        return;

    ANGLE_CONTEXT_TRY(syncStateForPathOperation());

    mImplementation->coverStrokePath(pathObj, coverMode);
}

void Context::stencilThenCoverFillPath(GLuint path, GLenum fillMode, GLuint mask, GLenum coverMode)
{
    const auto *pathObj = mState.mPathManager->getPath(path);
    if (!pathObj)
        return;

    ANGLE_CONTEXT_TRY(syncStateForPathOperation());

    mImplementation->stencilThenCoverFillPath(pathObj, fillMode, mask, coverMode);
}

void Context::stencilThenCoverStrokePath(GLuint path,
                                         GLint reference,
                                         GLuint mask,
                                         GLenum coverMode)
{
    const auto *pathObj = mState.mPathManager->getPath(path);
    if (!pathObj)
        return;

    ANGLE_CONTEXT_TRY(syncStateForPathOperation());

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
    const auto &pathObjects =
        GatherPaths(*mState.mPathManager, numPaths, pathNameType, paths, pathBase);

    ANGLE_CONTEXT_TRY(syncStateForPathOperation());

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
    const auto &pathObjects =
        GatherPaths(*mState.mPathManager, numPaths, pathNameType, paths, pathBase);

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    ANGLE_CONTEXT_TRY(syncStateForPathOperation());

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
    const auto &pathObjects =
        GatherPaths(*mState.mPathManager, numPaths, pathNameType, paths, pathBase);

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    ANGLE_CONTEXT_TRY(syncStateForPathOperation());

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
    const auto &pathObjects =
        GatherPaths(*mState.mPathManager, numPaths, pathNameType, paths, pathBase);

    ANGLE_CONTEXT_TRY(syncStateForPathOperation());

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
    const auto &pathObjects =
        GatherPaths(*mState.mPathManager, numPaths, pathNameType, paths, pathBase);

    ANGLE_CONTEXT_TRY(syncStateForPathOperation());

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
    const auto &pathObjects =
        GatherPaths(*mState.mPathManager, numPaths, pathNameType, paths, pathBase);

    ANGLE_CONTEXT_TRY(syncStateForPathOperation());

    mImplementation->stencilThenCoverStrokePathInstanced(pathObjects, coverMode, reference, mask,
                                                         transformType, transformValues);
}

void Context::bindFragmentInputLocation(GLuint program, GLint location, const GLchar *name)
{
    auto *programObject = getProgramResolveLink(program);

    programObject->bindFragmentInputLocation(location, name);
}

void Context::programPathFragmentInputGen(GLuint program,
                                          GLint location,
                                          GLenum genMode,
                                          GLint components,
                                          const GLfloat *coeffs)
{
    auto *programObject = getProgramResolveLink(program);

    programObject->pathFragmentInputGen(location, genMode, components, coeffs);
}

GLuint Context::getProgramResourceIndex(GLuint program, GLenum programInterface, const GLchar *name)
{
    const Program *programObject = getProgramResolveLink(program);
    return QueryProgramResourceIndex(programObject, programInterface, name);
}

void Context::getProgramResourceName(GLuint program,
                                     GLenum programInterface,
                                     GLuint index,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLchar *name)
{
    const Program *programObject = getProgramResolveLink(program);
    QueryProgramResourceName(programObject, programInterface, index, bufSize, length, name);
}

GLint Context::getProgramResourceLocation(GLuint program,
                                          GLenum programInterface,
                                          const GLchar *name)
{
    const Program *programObject = getProgramResolveLink(program);
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
    const Program *programObject = getProgramResolveLink(program);
    QueryProgramResourceiv(programObject, programInterface, index, propCount, props, bufSize,
                           length, params);
}

void Context::getProgramInterfaceiv(GLuint program,
                                    GLenum programInterface,
                                    GLenum pname,
                                    GLint *params)
{
    const Program *programObject = getProgramResolveLink(program);
    QueryProgramInterfaceiv(programObject, programInterface, pname, params);
}

void Context::getProgramInterfaceivRobust(GLuint program,
                                          GLenum programInterface,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLint *params)
{
    UNIMPLEMENTED();
}

void Context::handleError(GLenum errorCode,
                          const char *message,
                          const char *file,
                          const char *function,
                          unsigned int line)
{
    mErrors.handleError(errorCode, message, file, function, line);
}

void Context::validationError(GLenum errorCode, const char *message)
{
    mErrors.validationError(errorCode, message);
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
        return mErrors.popError();
    }
}

// NOTE: this function should not assume that this context is current!
void Context::markContextLost(GraphicsResetStatus status)
{
    ASSERT(status != GraphicsResetStatus::NoError);
    if (mResetStrategy == GL_LOSE_CONTEXT_ON_RESET_EXT)
    {
        mResetStatus       = status;
        mContextLostForced = true;
    }
    mContextLost = true;
}

GLenum Context::getGraphicsResetStatus()
{
    // Even if the application doesn't want to know about resets, we want to know
    // as it will allow us to skip all the calls.
    if (mResetStrategy == GL_NO_RESET_NOTIFICATION_EXT)
    {
        if (!mContextLost && mImplementation->getResetStatus() != GraphicsResetStatus::NoError)
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
        ASSERT(mResetStatus == GraphicsResetStatus::NoError);
        mResetStatus = mImplementation->getResetStatus();

        if (mResetStatus != GraphicsResetStatus::NoError)
        {
            mContextLost = true;
        }
    }
    else if (!mContextLostForced && mResetStatus != GraphicsResetStatus::NoError)
    {
        // If markContextLost was used to mark the context lost then
        // assume that is not recoverable, and continue to report the
        // lost reset status for the lifetime of this context.
        mResetStatus = mImplementation->getResetStatus();
    }

    return ToGLenum(mResetStatus);
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
    const Framebuffer *framebuffer = mState.mFramebufferManager->getFramebuffer(0);
    if (framebuffer == nullptr)
    {
        return EGL_NONE;
    }

    const FramebufferAttachment *backAttachment = framebuffer->getAttachment(this, GL_BACK);
    ASSERT(backAttachment != nullptr);
    return backAttachment->getSurface()->getRenderBuffer();
}

VertexArray *Context::checkVertexArrayAllocation(GLuint vertexArrayHandle)
{
    // Only called after a prior call to Gen.
    VertexArray *vertexArray = getVertexArray(vertexArrayHandle);
    if (!vertexArray)
    {
        vertexArray =
            new VertexArray(mImplementation.get(), vertexArrayHandle,
                            mState.mCaps.maxVertexAttributes, mState.mCaps.maxVertexAttribBindings);

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
            new TransformFeedback(mImplementation.get(), transformFeedbackHandle, mState.mCaps);
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
    mState.detachTexture(this, mZeroTextures, texture);
}

void Context::detachBuffer(Buffer *buffer)
{
    // Simple pass-through to State's detachBuffer method, since
    // only buffer attachments to container objects that are bound to the current context
    // should be detached. And all those are available in State.

    // [OpenGL ES 3.2] section 5.1.2 page 45:
    // Attachments to unbound container objects, such as
    // deletion of a buffer attached to a vertex array object which is not bound to the context,
    // are not affected and continue to act as references on the deleted object
    ANGLE_CONTEXT_TRY(mState.detachBuffer(this, buffer));
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
    mState.detachRenderbuffer(this, renderbuffer);
}

void Context::detachVertexArray(GLuint vertexArray)
{
    // Vertex array detachment is handled by Context, because 0 is a valid
    // VAO, and a pointer to it must be passed from Context to State at
    // binding time.

    // [OpenGL ES 3.0.2] section 2.10 page 43:
    // If a vertex array object that is currently bound is deleted, the binding
    // for that object reverts to zero and the default vertex array becomes current.
    if (mState.removeVertexArrayBinding(this, vertexArray))
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
    if (mState.removeTransformFeedbackBinding(this, transformFeedback))
    {
        bindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    }
}

void Context::detachSampler(GLuint sampler)
{
    mState.detachSampler(this, sampler);
}

void Context::detachProgramPipeline(GLuint pipeline)
{
    mState.detachProgramPipeline(this, pipeline);
}

void Context::vertexAttribDivisor(GLuint index, GLuint divisor)
{
    mState.setVertexAttribDivisor(this, index, divisor);
    mStateCache.onVertexArrayStateChange(this);
}

void Context::samplerParameteri(GLuint sampler, GLenum pname, GLint param)
{
    Sampler *const samplerObject =
        mState.mSamplerManager->checkSamplerAllocation(mImplementation.get(), sampler);
    SetSamplerParameteri(this, samplerObject, pname, param);
}

void Context::samplerParameteriv(GLuint sampler, GLenum pname, const GLint *param)
{
    Sampler *const samplerObject =
        mState.mSamplerManager->checkSamplerAllocation(mImplementation.get(), sampler);
    SetSamplerParameteriv(this, samplerObject, pname, param);
}

void Context::samplerParameterIiv(GLuint sampler, GLenum pname, const GLint *param)
{
    Sampler *const samplerObject =
        mState.mSamplerManager->checkSamplerAllocation(mImplementation.get(), sampler);
    SetSamplerParameterIiv(this, samplerObject, pname, param);
}

void Context::samplerParameterIuiv(GLuint sampler, GLenum pname, const GLuint *param)
{
    Sampler *const samplerObject =
        mState.mSamplerManager->checkSamplerAllocation(mImplementation.get(), sampler);
    SetSamplerParameterIuiv(this, samplerObject, pname, param);
}

void Context::samplerParameterivRobust(GLuint sampler,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       const GLint *param)
{
    samplerParameteriv(sampler, pname, param);
}

void Context::samplerParameterIivRobust(GLuint sampler,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        const GLint *param)
{
    UNIMPLEMENTED();
}

void Context::samplerParameterIuivRobust(GLuint sampler,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         const GLuint *param)
{
    UNIMPLEMENTED();
}

void Context::samplerParameterf(GLuint sampler, GLenum pname, GLfloat param)
{
    Sampler *const samplerObject =
        mState.mSamplerManager->checkSamplerAllocation(mImplementation.get(), sampler);
    SetSamplerParameterf(this, samplerObject, pname, param);
}

void Context::samplerParameterfv(GLuint sampler, GLenum pname, const GLfloat *param)
{
    Sampler *const samplerObject =
        mState.mSamplerManager->checkSamplerAllocation(mImplementation.get(), sampler);
    SetSamplerParameterfv(this, samplerObject, pname, param);
}

void Context::samplerParameterfvRobust(GLuint sampler,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       const GLfloat *param)
{
    samplerParameterfv(sampler, pname, param);
}

void Context::getSamplerParameteriv(GLuint sampler, GLenum pname, GLint *params)
{
    const Sampler *const samplerObject =
        mState.mSamplerManager->checkSamplerAllocation(mImplementation.get(), sampler);
    QuerySamplerParameteriv(samplerObject, pname, params);
}

void Context::getSamplerParameterIiv(GLuint sampler, GLenum pname, GLint *params)
{
    const Sampler *const samplerObject =
        mState.mSamplerManager->checkSamplerAllocation(mImplementation.get(), sampler);
    QuerySamplerParameterIiv(samplerObject, pname, params);
}

void Context::getSamplerParameterIuiv(GLuint sampler, GLenum pname, GLuint *params)
{
    const Sampler *const samplerObject =
        mState.mSamplerManager->checkSamplerAllocation(mImplementation.get(), sampler);
    QuerySamplerParameterIuiv(samplerObject, pname, params);
}

void Context::getSamplerParameterivRobust(GLuint sampler,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLint *params)
{
    getSamplerParameteriv(sampler, pname, params);
}

void Context::getSamplerParameterIivRobust(GLuint sampler,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLint *params)
{
    UNIMPLEMENTED();
}

void Context::getSamplerParameterIuivRobust(GLuint sampler,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLuint *params)
{
    UNIMPLEMENTED();
}

void Context::getSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat *params)
{
    const Sampler *const samplerObject =
        mState.mSamplerManager->checkSamplerAllocation(mImplementation.get(), sampler);
    QuerySamplerParameterfv(samplerObject, pname, params);
}

void Context::getSamplerParameterfvRobust(GLuint sampler,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLfloat *params)
{
    getSamplerParameterfv(sampler, pname, params);
}

void Context::programParameteri(GLuint program, GLenum pname, GLint value)
{
    gl::Program *programObject = getProgramResolveLink(program);
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
    for (const auto &extensionString : mState.mExtensions.getStrings())
    {
        mExtensionStrings.push_back(MakeStaticString(extensionString));
    }
    mExtensionString = mergeExtensionStrings(mExtensionStrings);

    mRequestableExtensionStrings.clear();
    for (const auto &extensionInfo : GetExtensionInfoMap())
    {
        if (extensionInfo.second.Requestable &&
            !(mState.mExtensions.*(extensionInfo.second.ExtensionsMember)) &&
            mSupportedExtensions.*(extensionInfo.second.ExtensionsMember))
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

    return extension != extensionInfos.end() && extension->second.Requestable &&
           mSupportedExtensions.*(extension->second.ExtensionsMember);
}

void Context::requestExtension(const char *name)
{
    const ExtensionInfoMap &extensionInfos = GetExtensionInfoMap();
    ASSERT(extensionInfos.find(name) != extensionInfos.end());
    const auto &extension = extensionInfos.at(name);
    ASSERT(extension.Requestable);
    ASSERT(isExtensionRequestable(name));

    if (mState.mExtensions.*(extension.ExtensionsMember))
    {
        // Extension already enabled
        return;
    }

    mState.mExtensions.*(extension.ExtensionsMember) = true;
    updateCaps();
    initExtensionStrings();

    // Release the shader compiler so it will be re-created with the requested extensions enabled.
    releaseShaderCompiler();

    // Invalidate all textures and framebuffer. Some extensions make new formats renderable or
    // sampleable.
    mState.mTextureManager->signalAllTexturesDirty(this);
    for (auto &zeroTexture : mZeroTextures)
    {
        if (zeroTexture.get() != nullptr)
        {
            zeroTexture->signalDirtyStorage(this, InitState::Initialized);
        }
    }

    mState.mFramebufferManager->invalidateFramebufferComplenessCache(this);
}

size_t Context::getRequestableExtensionStringCount() const
{
    return mRequestableExtensionStrings.size();
}

void Context::beginTransformFeedback(PrimitiveMode primitiveMode)
{
    TransformFeedback *transformFeedback = mState.getCurrentTransformFeedback();
    ASSERT(transformFeedback != nullptr);
    ASSERT(!transformFeedback->isPaused());

    ANGLE_CONTEXT_TRY(transformFeedback->begin(this, primitiveMode, mState.getProgram()));
    mStateCache.onActiveTransformFeedbackChange(this);
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

Extensions Context::generateSupportedExtensions() const
{
    Extensions supportedExtensions = mImplementation->getNativeExtensions();

    // Explicitly enable GL_KHR_parallel_shader_compile
    supportedExtensions.parallelShaderCompile = true;

    if (getClientVersion() < ES_2_0)
    {
        // Default extensions for GLES1
        supportedExtensions.pointSizeArray        = true;
        supportedExtensions.textureCubeMap        = true;
        supportedExtensions.pointSprite           = true;
        supportedExtensions.drawTexture           = true;
        supportedExtensions.parallelShaderCompile = false;
    }

    if (getClientVersion() < ES_3_0)
    {
        // Disable ES3+ extensions
        supportedExtensions.colorBufferFloat      = false;
        supportedExtensions.eglImageExternalEssl3 = false;
        supportedExtensions.textureNorm16         = false;
        supportedExtensions.multiview2            = false;
        supportedExtensions.maxViews              = 1u;
        supportedExtensions.copyTexture3d         = false;
        supportedExtensions.textureMultisample    = false;

        // Don't expose GL_EXT_texture_sRGB_decode without sRGB texture support
        if (!supportedExtensions.sRGB)
        {
            supportedExtensions.textureSRGBDecode = false;
        }
    }

    if (getClientVersion() < ES_3_1)
    {
        // Disable ES3.1+ extensions
        supportedExtensions.geometryShader = false;

        // TODO(http://anglebug.com/2775): Multisample arrays could be supported on ES 3.0 as well
        // once 2D multisample texture extension is exposed there.
        supportedExtensions.textureStorageMultisample2DArray = false;
    }

    if (getClientVersion() > ES_2_0)
    {
        // FIXME(geofflang): Don't support EXT_sRGB in non-ES2 contexts
        // supportedExtensions.sRGB = false;
    }

    // Some extensions are always available because they are implemented in the GL layer.
    supportedExtensions.bindUniformLocation   = true;
    supportedExtensions.vertexArrayObject     = true;
    supportedExtensions.bindGeneratesResource = true;
    supportedExtensions.clientArrays          = true;
    supportedExtensions.requestExtension      = true;
    supportedExtensions.multiDraw             = true;

    // Enable the no error extension if the context was created with the flag.
    supportedExtensions.noError = mSkipValidation;

    // Enable surfaceless to advertise we'll have the correct behavior when there is no default FBO
    supportedExtensions.surfacelessContext = mSurfacelessSupported;

    // Explicitly enable GL_KHR_debug
    supportedExtensions.debug                   = true;
    supportedExtensions.maxDebugMessageLength   = 1024;
    supportedExtensions.maxDebugLoggedMessages  = 1024;
    supportedExtensions.maxDebugGroupStackDepth = 1024;
    supportedExtensions.maxLabelLength          = 1024;

    // Explicitly enable GL_ANGLE_robust_client_memory
    supportedExtensions.robustClientMemory = true;

    // Determine robust resource init availability from EGL.
    supportedExtensions.robustResourceInitialization = mState.isRobustResourceInitEnabled();

    // mState.mExtensions.robustBufferAccessBehavior is true only if robust access is true and the
    // backend supports it.
    supportedExtensions.robustBufferAccessBehavior =
        mRobustAccess && supportedExtensions.robustBufferAccessBehavior;

    // Enable the cache control query unconditionally.
    supportedExtensions.programCacheControl = true;

    // Enable EGL_ANGLE_explicit_context subextensions
    if (mExplicitContextAvailable)
    {
        // GL_ANGLE_explicit_context_gles1
        supportedExtensions.explicitContextGles1 = true;
        // GL_ANGLE_explicit_context
        supportedExtensions.explicitContext = true;
    }

    // If EGL_KHR_fence_sync is not enabled, don't expose GL_OES_EGL_sync.
    ASSERT(mDisplay);
    if (!mDisplay->getExtensions().fenceSync)
    {
        supportedExtensions.eglSync = false;
    }

    supportedExtensions.memorySize = true;

    // GL_CHROMIUM_lose_context is implemented in the frontend
    supportedExtensions.loseContextCHROMIUM = true;

    return supportedExtensions;
}

void Context::initCaps()
{
    mState.mCaps = mImplementation->getNativeCaps();

    mSupportedExtensions = generateSupportedExtensions();
    mState.mExtensions   = mSupportedExtensions;

    mState.mLimitations = mImplementation->getNativeLimitations();

    // GLES1 emulation: Initialize caps (Table 6.20 / 6.22 in the ES 1.1 spec)
    if (getClientVersion() < Version(2, 0))
    {
        mState.mCaps.maxMultitextureUnits          = 4;
        mState.mCaps.maxClipPlanes                 = 6;
        mState.mCaps.maxLights                     = 8;
        mState.mCaps.maxModelviewMatrixStackDepth  = Caps::GlobalMatrixStackDepth;
        mState.mCaps.maxProjectionMatrixStackDepth = Caps::GlobalMatrixStackDepth;
        mState.mCaps.maxTextureMatrixStackDepth    = Caps::GlobalMatrixStackDepth;
        mState.mCaps.minSmoothPointSize            = 1.0f;
        mState.mCaps.maxSmoothPointSize            = 1.0f;
        mState.mCaps.minSmoothLineWidth            = 1.0f;
        mState.mCaps.maxSmoothLineWidth            = 1.0f;
    }

    // Apply/Verify implementation limits
    LimitCap(&mState.mCaps.maxVertexAttributes, MAX_VERTEX_ATTRIBS);

    ASSERT(mState.mCaps.minAliasedPointSize >= 1.0f);

    if (getClientVersion() < ES_3_1)
    {
        mState.mCaps.maxVertexAttribBindings = mState.mCaps.maxVertexAttributes;
    }
    else
    {
        LimitCap(&mState.mCaps.maxVertexAttribBindings, MAX_VERTEX_ATTRIB_BINDINGS);
    }

    LimitCap(&mState.mCaps.maxShaderUniformBlocks[ShaderType::Vertex],
             IMPLEMENTATION_MAX_VERTEX_SHADER_UNIFORM_BUFFERS);
    LimitCap(&mState.mCaps.maxUniformBufferBindings, IMPLEMENTATION_MAX_UNIFORM_BUFFER_BINDINGS);

    LimitCap(&mState.mCaps.maxVertexOutputComponents, IMPLEMENTATION_MAX_VARYING_VECTORS * 4);
    LimitCap(&mState.mCaps.maxFragmentInputComponents, IMPLEMENTATION_MAX_VARYING_VECTORS * 4);

    // Limit textures as well, so we can use fast bitsets with texture bindings.
    LimitCap(&mState.mCaps.maxCombinedTextureImageUnits, IMPLEMENTATION_MAX_ACTIVE_TEXTURES);
    LimitCap(&mState.mCaps.maxShaderTextureImageUnits[ShaderType::Vertex],
             IMPLEMENTATION_MAX_ACTIVE_TEXTURES / 2);
    LimitCap(&mState.mCaps.maxShaderTextureImageUnits[ShaderType::Fragment],
             IMPLEMENTATION_MAX_ACTIVE_TEXTURES / 2);

    LimitCap(&mState.mCaps.maxImageUnits, IMPLEMENTATION_MAX_IMAGE_UNITS);

    LimitCap(&mState.mCaps.maxCombinedAtomicCounterBuffers,
             IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFERS);

    mState.mCaps.maxSampleMaskWords =
        std::min<GLuint>(mState.mCaps.maxSampleMaskWords, MAX_SAMPLE_MASK_WORDS);

    // WebGL compatibility
    mState.mExtensions.webglCompatibility = mWebGLContext;
    for (const auto &extensionInfo : GetExtensionInfoMap())
    {
        // If the user has requested that extensions start disabled and they are requestable,
        // disable them.
        if (!mExtensionsEnabled && extensionInfo.second.Requestable)
        {
            mState.mExtensions.*(extensionInfo.second.ExtensionsMember) = false;
        }
    }

    // Generate texture caps
    updateCaps();
}

void Context::updateCaps()
{
    mState.mCaps.compressedTextureFormats.clear();
    mState.mTextureCaps.clear();

    for (GLenum sizedInternalFormat : GetAllSizedInternalFormats())
    {
        TextureCaps formatCaps = mImplementation->getNativeTextureCaps().get(sizedInternalFormat);
        const InternalFormat &formatInfo = GetSizedInternalFormatInfo(sizedInternalFormat);

        // Update the format caps based on the client version and extensions.
        // Caps are AND'd with the renderer caps because some core formats are still unsupported in
        // ES3.
        formatCaps.texturable = formatCaps.texturable &&
                                formatInfo.textureSupport(getClientVersion(), mState.mExtensions);
        formatCaps.filterable = formatCaps.filterable &&
                                formatInfo.filterSupport(getClientVersion(), mState.mExtensions);
        formatCaps.textureAttachment =
            formatCaps.textureAttachment &&
            formatInfo.textureAttachmentSupport(getClientVersion(), mState.mExtensions);
        formatCaps.renderbuffer =
            formatCaps.renderbuffer &&
            formatInfo.renderbufferSupport(getClientVersion(), mState.mExtensions);

        // OpenGL ES does not support multisampling with non-rendererable formats
        // OpenGL ES 3.0 or prior does not support multisampling with integer formats
        if (!formatCaps.renderbuffer ||
            (getClientVersion() < ES_3_1 && !mSupportedExtensions.textureMultisample &&
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
                mState.mCaps.maxSamples = std::min(mState.mCaps.maxSamples, formatMaxSamples);
            }

            // Handle GLES 3.1 MAX_*_SAMPLES values similarly to MAX_SAMPLES.
            if (getClientVersion() >= ES_3_1 || mSupportedExtensions.textureMultisample)
            {
                // GLES 3.1 section 9.2.5: "Implementations must support creation of renderbuffers
                // in these required formats with up to the value of MAX_SAMPLES multisamples, with
                // the exception that the signed and unsigned integer formats are required only to
                // support creation of renderbuffers with up to the value of MAX_INTEGER_SAMPLES
                // multisamples, which must be at least one."
                if (formatInfo.componentType == GL_INT ||
                    formatInfo.componentType == GL_UNSIGNED_INT)
                {
                    mState.mCaps.maxIntegerSamples =
                        std::min(mState.mCaps.maxIntegerSamples, formatMaxSamples);
                }

                // GLES 3.1 section 19.3.1.
                if (formatCaps.texturable)
                {
                    if (formatInfo.depthBits > 0)
                    {
                        mState.mCaps.maxDepthTextureSamples =
                            std::min(mState.mCaps.maxDepthTextureSamples, formatMaxSamples);
                    }
                    else if (formatInfo.redBits > 0)
                    {
                        mState.mCaps.maxColorTextureSamples =
                            std::min(mState.mCaps.maxColorTextureSamples, formatMaxSamples);
                    }
                }
            }
        }

        if (formatCaps.texturable && formatInfo.compressed)
        {
            mState.mCaps.compressedTextureFormats.push_back(sizedInternalFormat);
        }

        mState.mTextureCaps.insert(sizedInternalFormat, formatCaps);
    }

    // If program binary is disabled, blank out the memory cache pointer.
    if (!mSupportedExtensions.getProgramBinary)
    {
        mMemoryProgramCache = nullptr;
    }

    // Compute which buffer types are allowed
    mValidBufferBindings.reset();
    mValidBufferBindings.set(BufferBinding::ElementArray);
    mValidBufferBindings.set(BufferBinding::Array);

    if (mState.mExtensions.pixelBufferObject || getClientVersion() >= ES_3_0)
    {
        mValidBufferBindings.set(BufferBinding::PixelPack);
        mValidBufferBindings.set(BufferBinding::PixelUnpack);
    }

    if (getClientVersion() >= ES_3_0)
    {
        mValidBufferBindings.set(BufferBinding::CopyRead);
        mValidBufferBindings.set(BufferBinding::CopyWrite);
        mValidBufferBindings.set(BufferBinding::TransformFeedback);
        mValidBufferBindings.set(BufferBinding::Uniform);
    }

    if (getClientVersion() >= ES_3_1)
    {
        mValidBufferBindings.set(BufferBinding::AtomicCounter);
        mValidBufferBindings.set(BufferBinding::ShaderStorage);
        mValidBufferBindings.set(BufferBinding::DrawIndirect);
        mValidBufferBindings.set(BufferBinding::DispatchIndirect);
    }

    mThreadPool = angle::WorkerThreadPool::Create(mState.mExtensions.parallelShaderCompile);

    // Reinitialize some dirty bits that depend on extensions.
    bool robustInit = mState.isRobustResourceInitEnabled();
    mDrawDirtyObjects.set(State::DIRTY_OBJECT_DRAW_ATTACHMENTS, robustInit);
    mDrawDirtyObjects.set(State::DIRTY_OBJECT_TEXTURES_INIT, robustInit);
    mDrawDirtyObjects.set(State::DIRTY_OBJECT_IMAGES_INIT, robustInit);
    mBlitDirtyObjects.set(State::DIRTY_OBJECT_DRAW_ATTACHMENTS, robustInit);
    mComputeDirtyObjects.set(State::DIRTY_OBJECT_TEXTURES_INIT, robustInit);
    mComputeDirtyObjects.set(State::DIRTY_OBJECT_IMAGES_INIT, robustInit);

    // We need to validate buffer bounds if we are in a WebGL or robust access context and the
    // back-end does not support robust buffer access behaviour.
    if (!mSupportedExtensions.robustBufferAccessBehavior && (mState.isWebGL() || mRobustAccess))
    {
        mBufferAccessValidationEnabled = true;
    }

    // Reinitialize state cache after extension changes.
    mStateCache.initialize(this);
}

void Context::initWorkarounds()
{
    // Apply back-end workarounds.
    mImplementation->applyNativeWorkarounds(&mWorkarounds);

    // Lose the context upon out of memory error if the application is
    // expecting to watch for those events.
    mWorkarounds.loseContextOnOutOfMemory = (mResetStrategy == GL_LOSE_CONTEXT_ON_RESET_EXT);

    if (mWorkarounds.syncFramebufferBindingsOnTexImage)
    {
        // Update the Framebuffer bindings on TexImage to work around an Intel bug.
        mTexImageDirtyBits.set(State::DIRTY_BIT_READ_FRAMEBUFFER_BINDING);
        mTexImageDirtyBits.set(State::DIRTY_BIT_DRAW_FRAMEBUFFER_BINDING);
    }
}

bool Context::noopDrawInstanced(PrimitiveMode mode, GLsizei count, GLsizei instanceCount)
{
    return (instanceCount == 0) || noopDraw(mode, count);
}

angle::Result Context::prepareForClear(GLbitfield mask)
{
    ANGLE_TRY(syncDirtyObjects(mClearDirtyObjects));
    ANGLE_TRY(mState.getDrawFramebuffer()->ensureClearAttachmentsInitialized(this, mask));
    ANGLE_TRY(syncDirtyBits(mClearDirtyBits));
    return angle::Result::Continue;
}

angle::Result Context::prepareForClearBuffer(GLenum buffer, GLint drawbuffer)
{
    ANGLE_TRY(syncDirtyObjects(mClearDirtyObjects));
    ANGLE_TRY(mState.getDrawFramebuffer()->ensureClearBufferAttachmentsInitialized(this, buffer,
                                                                                   drawbuffer));
    ANGLE_TRY(syncDirtyBits(mClearDirtyBits));
    return angle::Result::Continue;
}

ANGLE_INLINE angle::Result Context::prepareForDispatch()
{
    ANGLE_TRY(syncDirtyObjects(mComputeDirtyObjects));
    return syncDirtyBits(mComputeDirtyBits);
}

angle::Result Context::syncState(const State::DirtyBits &bitMask,
                                 const State::DirtyObjects &objectMask)
{
    ANGLE_TRY(syncDirtyObjects(objectMask));
    ANGLE_TRY(syncDirtyBits(bitMask));
    return angle::Result::Continue;
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
    if (mask == 0)
    {
        // ES3.0 spec, section 4.3.2 specifies that a mask of zero is valid and no
        // buffers are copied.
        return;
    }

    Framebuffer *drawFramebuffer = mState.getDrawFramebuffer();
    ASSERT(drawFramebuffer);

    Rectangle srcArea(srcX0, srcY0, srcX1 - srcX0, srcY1 - srcY0);
    Rectangle dstArea(dstX0, dstY0, dstX1 - dstX0, dstY1 - dstY0);

    ANGLE_CONTEXT_TRY(syncStateForBlit());

    ANGLE_CONTEXT_TRY(drawFramebuffer->blit(this, srcArea, dstArea, mask, filter));
}

void Context::clear(GLbitfield mask)
{
    ANGLE_CONTEXT_TRY(prepareForClear(mask));
    ANGLE_CONTEXT_TRY(mState.getDrawFramebuffer()->clear(this, mask));
}

void Context::clearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *values)
{
    Framebuffer *framebufferObject          = mState.getDrawFramebuffer();
    const FramebufferAttachment *attachment = nullptr;
    if (buffer == GL_DEPTH)
    {
        attachment = framebufferObject->getDepthbuffer();
    }
    if (buffer == GL_COLOR &&
        static_cast<size_t>(drawbuffer) < framebufferObject->getNumColorBuffers())
    {
        attachment = framebufferObject->getColorbuffer(drawbuffer);
    }
    // It's not an error to try to clear a non-existent buffer, but it's a no-op. We early out so
    // that the backend doesn't need to take this case into account.
    if (!attachment)
    {
        return;
    }
    ANGLE_CONTEXT_TRY(prepareForClearBuffer(buffer, drawbuffer));
    ANGLE_CONTEXT_TRY(framebufferObject->clearBufferfv(this, buffer, drawbuffer, values));
}

void Context::clearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *values)
{
    Framebuffer *framebufferObject          = mState.getDrawFramebuffer();
    const FramebufferAttachment *attachment = nullptr;
    if (buffer == GL_COLOR &&
        static_cast<size_t>(drawbuffer) < framebufferObject->getNumColorBuffers())
    {
        attachment = framebufferObject->getColorbuffer(drawbuffer);
    }
    // It's not an error to try to clear a non-existent buffer, but it's a no-op. We early out so
    // that the backend doesn't need to take this case into account.
    if (!attachment)
    {
        return;
    }
    ANGLE_CONTEXT_TRY(prepareForClearBuffer(buffer, drawbuffer));
    ANGLE_CONTEXT_TRY(framebufferObject->clearBufferuiv(this, buffer, drawbuffer, values));
}

void Context::clearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *values)
{
    Framebuffer *framebufferObject          = mState.getDrawFramebuffer();
    const FramebufferAttachment *attachment = nullptr;
    if (buffer == GL_STENCIL)
    {
        attachment = framebufferObject->getStencilbuffer();
    }
    if (buffer == GL_COLOR &&
        static_cast<size_t>(drawbuffer) < framebufferObject->getNumColorBuffers())
    {
        attachment = framebufferObject->getColorbuffer(drawbuffer);
    }
    // It's not an error to try to clear a non-existent buffer, but it's a no-op. We early out so
    // that the backend doesn't need to take this case into account.
    if (!attachment)
    {
        return;
    }
    ANGLE_CONTEXT_TRY(prepareForClearBuffer(buffer, drawbuffer));
    ANGLE_CONTEXT_TRY(framebufferObject->clearBufferiv(this, buffer, drawbuffer, values));
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

    ANGLE_CONTEXT_TRY(prepareForClearBuffer(buffer, drawbuffer));
    ANGLE_CONTEXT_TRY(framebufferObject->clearBufferfi(this, buffer, drawbuffer, depth, stencil));
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

    ANGLE_CONTEXT_TRY(syncStateForReadPixels());

    Framebuffer *readFBO = mState.getReadFramebuffer();
    ASSERT(readFBO);

    Rectangle area(x, y, width, height);
    ANGLE_CONTEXT_TRY(readFBO->readPixels(this, area, format, type, pixels));
}

void Context::readPixelsRobust(GLint x,
                               GLint y,
                               GLsizei width,
                               GLsizei height,
                               GLenum format,
                               GLenum type,
                               GLsizei bufSize,
                               GLsizei *length,
                               GLsizei *columns,
                               GLsizei *rows,
                               void *pixels)
{
    readPixels(x, y, width, height, format, type, pixels);
}

void Context::readnPixelsRobust(GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height,
                                GLenum format,
                                GLenum type,
                                GLsizei bufSize,
                                GLsizei *length,
                                GLsizei *columns,
                                GLsizei *rows,
                                void *data)
{
    readPixels(x, y, width, height, format, type, data);
}

void Context::copyTexImage2D(TextureTarget target,
                             GLint level,
                             GLenum internalformat,
                             GLint x,
                             GLint y,
                             GLsizei width,
                             GLsizei height,
                             GLint border)
{
    // Only sync the read FBO
    ANGLE_CONTEXT_TRY(mState.syncDirtyObject(this, GL_READ_FRAMEBUFFER));

    Rectangle sourceArea(x, y, width, height);

    Framebuffer *framebuffer = mState.getReadFramebuffer();
    Texture *texture         = getTextureByTarget(target);
    ANGLE_CONTEXT_TRY(
        texture->copyImage(this, target, level, sourceArea, internalformat, framebuffer));
}

void Context::copyTexSubImage2D(TextureTarget target,
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
    ANGLE_CONTEXT_TRY(mState.syncDirtyObject(this, GL_READ_FRAMEBUFFER));

    Offset destOffset(xoffset, yoffset, 0);
    Rectangle sourceArea(x, y, width, height);

    ImageIndex index = ImageIndex::MakeFromTarget(target, level);

    Framebuffer *framebuffer = mState.getReadFramebuffer();
    Texture *texture         = getTextureByTarget(target);
    ANGLE_CONTEXT_TRY(texture->copySubImage(this, index, destOffset, sourceArea, framebuffer));
}

void Context::copyTexSubImage3D(TextureTarget target,
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
    ANGLE_CONTEXT_TRY(mState.syncDirtyObject(this, GL_READ_FRAMEBUFFER));

    Offset destOffset(xoffset, yoffset, zoffset);
    Rectangle sourceArea(x, y, width, height);

    ImageIndex index = ImageIndex::MakeFromType(TextureTargetToType(target), level, zoffset);

    Framebuffer *framebuffer = mState.getReadFramebuffer();
    Texture *texture         = getTextureByTarget(target);
    ANGLE_CONTEXT_TRY(texture->copySubImage(this, index, destOffset, sourceArea, framebuffer));
}

void Context::framebufferTexture2D(GLenum target,
                                   GLenum attachment,
                                   TextureTarget textarget,
                                   GLuint texture,
                                   GLint level)
{
    Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (texture != 0)
    {
        Texture *textureObj = getTexture(texture);
        ImageIndex index    = ImageIndex::MakeFromTarget(textarget, level);
        framebuffer->setAttachment(this, GL_TEXTURE, attachment, index, textureObj);
    }
    else
    {
        framebuffer->resetAttachment(this, attachment);
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

        framebuffer->setAttachment(this, GL_RENDERBUFFER, attachment, gl::ImageIndex(),
                                   renderbufferObject);
    }
    else
    {
        framebuffer->resetAttachment(this, attachment);
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
        ImageIndex index       = ImageIndex::MakeFromType(textureObject->getType(), level, layer);
        framebuffer->setAttachment(this, GL_TEXTURE, attachment, index, textureObject);
    }
    else
    {
        framebuffer->resetAttachment(this, attachment);
    }

    mState.setObjectDirty(target);
}

void Context::framebufferTextureMultiview(GLenum target,
                                          GLenum attachment,
                                          GLuint texture,
                                          GLint level,
                                          GLint baseViewIndex,
                                          GLsizei numViews)
{
    Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (texture != 0)
    {
        Texture *textureObj = getTexture(texture);

        ImageIndex index;
        if (textureObj->getType() == TextureType::_2DArray)
        {
            index = ImageIndex::Make2DArrayRange(level, baseViewIndex, numViews);
        }
        else
        {
            ASSERT(textureObj->getType() == TextureType::_2DMultisampleArray);
            ASSERT(level == 0);
            index = ImageIndex::Make2DMultisampleArrayRange(baseViewIndex, numViews);
        }
        framebuffer->setAttachmentMultiview(this, GL_TEXTURE, attachment, index, textureObj,
                                            numViews, baseViewIndex);
    }
    else
    {
        framebuffer->resetAttachment(this, attachment);
    }

    mState.setObjectDirty(target);
}

void Context::framebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level)
{
    Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (texture != 0)
    {
        Texture *textureObj = getTexture(texture);

        ImageIndex index = ImageIndex::MakeFromType(
            textureObj->getType(), level, ImageIndex::kEntireLevel, ImageIndex::kEntireLevel);
        framebuffer->setAttachment(this, GL_TEXTURE, attachment, index, textureObj);
    }
    else
    {
        framebuffer->resetAttachment(this, attachment);
    }

    mState.setObjectDirty(target);
}

void Context::drawBuffers(GLsizei n, const GLenum *bufs)
{
    Framebuffer *framebuffer = mState.getDrawFramebuffer();
    ASSERT(framebuffer);
    framebuffer->setDrawBuffers(n, bufs);
    mState.setDrawFramebufferDirty();
    mStateCache.onDrawFramebufferChange(this);
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
    ANGLE_CONTEXT_TRY(mState.syncDirtyObject(this, target));

    Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    // The specification isn't clear what should be done when the framebuffer isn't complete.
    // We leave it up to the framebuffer implementation to decide what to do.
    ANGLE_CONTEXT_TRY(framebuffer->discard(this, numAttachments, attachments));
}

void Context::invalidateFramebuffer(GLenum target,
                                    GLsizei numAttachments,
                                    const GLenum *attachments)
{
    // Only sync the FBO
    ANGLE_CONTEXT_TRY(mState.syncDirtyObject(this, target));

    Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (!framebuffer->isComplete(this))
    {
        return;
    }

    ANGLE_CONTEXT_TRY(framebuffer->invalidate(this, numAttachments, attachments));
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
    ANGLE_CONTEXT_TRY(mState.syncDirtyObject(this, target));

    Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (!framebuffer->isComplete(this))
    {
        return;
    }

    Rectangle area(x, y, width, height);
    ANGLE_CONTEXT_TRY(framebuffer->invalidateSub(this, numAttachments, attachments, area));
}

void Context::texImage2D(TextureTarget target,
                         GLint level,
                         GLint internalformat,
                         GLsizei width,
                         GLsizei height,
                         GLint border,
                         GLenum format,
                         GLenum type,
                         const void *pixels)
{
    ANGLE_CONTEXT_TRY(syncStateForTexImage());

    Extents size(width, height, 1);
    Texture *texture = getTextureByTarget(target);
    ANGLE_CONTEXT_TRY(texture->setImage(this, mState.getUnpackState(), target, level,
                                        internalformat, size, format, type,
                                        static_cast<const uint8_t *>(pixels)));
}

void Context::texImage2DRobust(TextureTarget target,
                               GLint level,
                               GLint internalformat,
                               GLsizei width,
                               GLsizei height,
                               GLint border,
                               GLenum format,
                               GLenum type,
                               GLsizei bufSize,
                               const void *pixels)
{
    texImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

void Context::texImage3D(TextureTarget target,
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
    ANGLE_CONTEXT_TRY(syncStateForTexImage());

    Extents size(width, height, depth);
    Texture *texture = getTextureByTarget(target);
    ANGLE_CONTEXT_TRY(texture->setImage(this, mState.getUnpackState(), target, level,
                                        internalformat, size, format, type,
                                        static_cast<const uint8_t *>(pixels)));
}

void Context::texImage3DRobust(TextureTarget target,
                               GLint level,
                               GLint internalformat,
                               GLsizei width,
                               GLsizei height,
                               GLsizei depth,
                               GLint border,
                               GLenum format,
                               GLenum type,
                               GLsizei bufSize,
                               const void *pixels)
{
    texImage3D(target, level, internalformat, width, height, depth, border, format, type, pixels);
}

void Context::texSubImage2D(TextureTarget target,
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

    ANGLE_CONTEXT_TRY(syncStateForTexImage());

    Box area(xoffset, yoffset, 0, width, height, 1);
    Texture *texture = getTextureByTarget(target);

    gl::Buffer *unpackBuffer = mState.getTargetBuffer(gl::BufferBinding::PixelUnpack);

    ANGLE_CONTEXT_TRY(texture->setSubImage(this, mState.getUnpackState(), unpackBuffer, target,
                                           level, area, format, type,
                                           static_cast<const uint8_t *>(pixels)));
}

void Context::texSubImage2DRobust(TextureTarget target,
                                  GLint level,
                                  GLint xoffset,
                                  GLint yoffset,
                                  GLsizei width,
                                  GLsizei height,
                                  GLenum format,
                                  GLenum type,
                                  GLsizei bufSize,
                                  const void *pixels)
{
    texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void Context::texSubImage3D(TextureTarget target,
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

    ANGLE_CONTEXT_TRY(syncStateForTexImage());

    Box area(xoffset, yoffset, zoffset, width, height, depth);
    Texture *texture = getTextureByTarget(target);

    gl::Buffer *unpackBuffer = mState.getTargetBuffer(gl::BufferBinding::PixelUnpack);

    ANGLE_CONTEXT_TRY(texture->setSubImage(this, mState.getUnpackState(), unpackBuffer, target,
                                           level, area, format, type,
                                           static_cast<const uint8_t *>(pixels)));
}

void Context::texSubImage3DRobust(TextureTarget target,
                                  GLint level,
                                  GLint xoffset,
                                  GLint yoffset,
                                  GLint zoffset,
                                  GLsizei width,
                                  GLsizei height,
                                  GLsizei depth,
                                  GLenum format,
                                  GLenum type,
                                  GLsizei bufSize,
                                  const void *pixels)
{
    texSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type,
                  pixels);
}

void Context::compressedTexImage2D(TextureTarget target,
                                   GLint level,
                                   GLenum internalformat,
                                   GLsizei width,
                                   GLsizei height,
                                   GLint border,
                                   GLsizei imageSize,
                                   const void *data)
{
    ANGLE_CONTEXT_TRY(syncStateForTexImage());

    Extents size(width, height, 1);
    Texture *texture = getTextureByTarget(target);
    ANGLE_CONTEXT_TRY(texture->setCompressedImage(this, mState.getUnpackState(), target, level,
                                                  internalformat, size, imageSize,
                                                  static_cast<const uint8_t *>(data)));
}

void Context::compressedTexImage2DRobust(TextureTarget target,
                                         GLint level,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLint border,
                                         GLsizei imageSize,
                                         GLsizei dataSize,
                                         const GLvoid *data)
{
    compressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);
}

void Context::compressedTexImage3D(TextureTarget target,
                                   GLint level,
                                   GLenum internalformat,
                                   GLsizei width,
                                   GLsizei height,
                                   GLsizei depth,
                                   GLint border,
                                   GLsizei imageSize,
                                   const void *data)
{
    ANGLE_CONTEXT_TRY(syncStateForTexImage());

    Extents size(width, height, depth);
    Texture *texture = getTextureByTarget(target);
    ANGLE_CONTEXT_TRY(texture->setCompressedImage(this, mState.getUnpackState(), target, level,
                                                  internalformat, size, imageSize,
                                                  static_cast<const uint8_t *>(data)));
}

void Context::compressedTexImage3DRobust(TextureTarget target,
                                         GLint level,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth,
                                         GLint border,
                                         GLsizei imageSize,
                                         GLsizei dataSize,
                                         const GLvoid *data)
{
    compressedTexImage3D(target, level, internalformat, width, height, depth, border, imageSize,
                         data);
}

void Context::compressedTexSubImage2D(TextureTarget target,
                                      GLint level,
                                      GLint xoffset,
                                      GLint yoffset,
                                      GLsizei width,
                                      GLsizei height,
                                      GLenum format,
                                      GLsizei imageSize,
                                      const void *data)
{
    ANGLE_CONTEXT_TRY(syncStateForTexImage());

    Box area(xoffset, yoffset, 0, width, height, 1);
    Texture *texture = getTextureByTarget(target);
    ANGLE_CONTEXT_TRY(texture->setCompressedSubImage(this, mState.getUnpackState(), target, level,
                                                     area, format, imageSize,
                                                     static_cast<const uint8_t *>(data)));
}

void Context::compressedTexSubImage2DRobust(TextureTarget target,
                                            GLint level,
                                            GLint xoffset,
                                            GLint yoffset,
                                            GLsizei width,
                                            GLsizei height,
                                            GLenum format,
                                            GLsizei imageSize,
                                            GLsizei dataSize,
                                            const GLvoid *data)
{
    compressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize,
                            data);
}

void Context::compressedTexSubImage3D(TextureTarget target,
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

    ANGLE_CONTEXT_TRY(syncStateForTexImage());

    Box area(xoffset, yoffset, zoffset, width, height, depth);
    Texture *texture = getTextureByTarget(target);
    ANGLE_CONTEXT_TRY(texture->setCompressedSubImage(this, mState.getUnpackState(), target, level,
                                                     area, format, imageSize,
                                                     static_cast<const uint8_t *>(data)));
}

void Context::compressedTexSubImage3DRobust(TextureTarget target,
                                            GLint level,
                                            GLint xoffset,
                                            GLint yoffset,
                                            GLint zoffset,
                                            GLsizei width,
                                            GLsizei height,
                                            GLsizei depth,
                                            GLenum format,
                                            GLsizei imageSize,
                                            GLsizei dataSize,
                                            const GLvoid *data)
{
    compressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format,
                            imageSize, data);
}

void Context::generateMipmap(TextureType target)
{
    Texture *texture = getTextureByType(target);
    ANGLE_CONTEXT_TRY(texture->generateMipmap(this));
}

void Context::copyTexture(GLuint sourceId,
                          GLint sourceLevel,
                          TextureTarget destTarget,
                          GLuint destId,
                          GLint destLevel,
                          GLint internalFormat,
                          GLenum destType,
                          GLboolean unpackFlipY,
                          GLboolean unpackPremultiplyAlpha,
                          GLboolean unpackUnmultiplyAlpha)
{
    ANGLE_CONTEXT_TRY(syncStateForTexImage());

    gl::Texture *sourceTexture = getTexture(sourceId);
    gl::Texture *destTexture   = getTexture(destId);
    ANGLE_CONTEXT_TRY(
        destTexture->copyTexture(this, destTarget, destLevel, internalFormat, destType, sourceLevel,
                                 ConvertToBool(unpackFlipY), ConvertToBool(unpackPremultiplyAlpha),
                                 ConvertToBool(unpackUnmultiplyAlpha), sourceTexture));
}

void Context::copySubTexture(GLuint sourceId,
                             GLint sourceLevel,
                             TextureTarget destTarget,
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

    ANGLE_CONTEXT_TRY(syncStateForTexImage());

    gl::Texture *sourceTexture = getTexture(sourceId);
    gl::Texture *destTexture   = getTexture(destId);
    Offset offset(xoffset, yoffset, 0);
    Box box(x, y, 0, width, height, 1);
    ANGLE_CONTEXT_TRY(destTexture->copySubTexture(
        this, destTarget, destLevel, offset, sourceLevel, box, ConvertToBool(unpackFlipY),
        ConvertToBool(unpackPremultiplyAlpha), ConvertToBool(unpackUnmultiplyAlpha),
        sourceTexture));
}

void Context::copyTexture3D(GLuint sourceId,
                            GLint sourceLevel,
                            TextureTarget destTarget,
                            GLuint destId,
                            GLint destLevel,
                            GLint internalFormat,
                            GLenum destType,
                            GLboolean unpackFlipY,
                            GLboolean unpackPremultiplyAlpha,
                            GLboolean unpackUnmultiplyAlpha)
{
    ANGLE_CONTEXT_TRY(syncStateForTexImage());

    Texture *sourceTexture = getTexture(sourceId);
    Texture *destTexture   = getTexture(destId);
    ANGLE_CONTEXT_TRY(
        destTexture->copyTexture(this, destTarget, destLevel, internalFormat, destType, sourceLevel,
                                 ConvertToBool(unpackFlipY), ConvertToBool(unpackPremultiplyAlpha),
                                 ConvertToBool(unpackUnmultiplyAlpha), sourceTexture));
}

void Context::copySubTexture3D(GLuint sourceId,
                               GLint sourceLevel,
                               TextureTarget destTarget,
                               GLuint destId,
                               GLint destLevel,
                               GLint xoffset,
                               GLint yoffset,
                               GLint zoffset,
                               GLint x,
                               GLint y,
                               GLint z,
                               GLsizei width,
                               GLsizei height,
                               GLsizei depth,
                               GLboolean unpackFlipY,
                               GLboolean unpackPremultiplyAlpha,
                               GLboolean unpackUnmultiplyAlpha)
{
    // Zero sized copies are valid but no-ops
    if (width == 0 || height == 0 || depth == 0)
    {
        return;
    }

    ANGLE_CONTEXT_TRY(syncStateForTexImage());

    Texture *sourceTexture = getTexture(sourceId);
    Texture *destTexture   = getTexture(destId);
    Offset offset(xoffset, yoffset, zoffset);
    Box box(x, y, z, width, height, depth);
    ANGLE_CONTEXT_TRY(destTexture->copySubTexture(
        this, destTarget, destLevel, offset, sourceLevel, box, ConvertToBool(unpackFlipY),
        ConvertToBool(unpackPremultiplyAlpha), ConvertToBool(unpackUnmultiplyAlpha),
        sourceTexture));
}

void Context::compressedCopyTexture(GLuint sourceId, GLuint destId)
{
    ANGLE_CONTEXT_TRY(syncStateForTexImage());

    gl::Texture *sourceTexture = getTexture(sourceId);
    gl::Texture *destTexture   = getTexture(destId);
    ANGLE_CONTEXT_TRY(destTexture->copyCompressedTexture(this, sourceTexture));
}

void Context::getBufferPointerv(BufferBinding target, GLenum pname, void **params)
{
    Buffer *buffer = mState.getTargetBuffer(target);
    ASSERT(buffer);

    QueryBufferPointerv(buffer, pname, params);
}

void Context::getBufferPointervRobust(BufferBinding target,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      void **params)
{
    getBufferPointerv(target, pname, params);
}

void *Context::mapBuffer(BufferBinding target, GLenum access)
{
    Buffer *buffer = mState.getTargetBuffer(target);
    ASSERT(buffer);

    if (buffer->map(this, access) == angle::Result::Stop)
    {
        return nullptr;
    }

    return buffer->getMapPointer();
}

GLboolean Context::unmapBuffer(BufferBinding target)
{
    Buffer *buffer = mState.getTargetBuffer(target);
    ASSERT(buffer);

    GLboolean result;
    if (buffer->unmap(this, &result) == angle::Result::Stop)
    {
        return GL_FALSE;
    }

    return result;
}

void *Context::mapBufferRange(BufferBinding target,
                              GLintptr offset,
                              GLsizeiptr length,
                              GLbitfield access)
{
    Buffer *buffer = mState.getTargetBuffer(target);
    ASSERT(buffer);

    if (buffer->mapRange(this, offset, length, access) == angle::Result::Stop)
    {
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

angle::Result Context::syncStateForReadPixels()
{
    return syncState(mReadPixelsDirtyBits, mReadPixelsDirtyObjects);
}

angle::Result Context::syncStateForTexImage()
{
    return syncState(mTexImageDirtyBits, mTexImageDirtyObjects);
}

angle::Result Context::syncStateForBlit()
{
    return syncState(mBlitDirtyBits, mBlitDirtyObjects);
}

angle::Result Context::syncStateForPathOperation()
{
    ANGLE_TRY(syncDirtyObjects(mPathOperationDirtyObjects));

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    ANGLE_TRY(syncDirtyBits());

    return angle::Result::Continue;
}

void Context::activeShaderProgram(GLuint pipeline, GLuint program)
{
    UNIMPLEMENTED();
}

void Context::activeTexture(GLenum texture)
{
    mState.setActiveSampler(texture - GL_TEXTURE0);
}

void Context::blendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    mState.setBlendColor(clamp01(red), clamp01(green), clamp01(blue), clamp01(alpha));
}

void Context::blendEquation(GLenum mode)
{
    mState.setBlendEquation(mode, mode);
}

void Context::blendEquationSeparate(GLenum modeRGB, GLenum modeAlpha)
{
    mState.setBlendEquation(modeRGB, modeAlpha);
}

void Context::blendFunc(GLenum sfactor, GLenum dfactor)
{
    mState.setBlendFactors(sfactor, dfactor, sfactor, dfactor);
}

void Context::blendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
    mState.setBlendFactors(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void Context::clearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    mState.setColorClearValue(red, green, blue, alpha);
}

void Context::clearDepthf(GLfloat depth)
{
    mState.setDepthClearValue(clamp01(depth));
}

void Context::clearStencil(GLint s)
{
    mState.setStencilClearValue(s);
}

void Context::colorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
    mState.setColorMask(ConvertToBool(red), ConvertToBool(green), ConvertToBool(blue),
                        ConvertToBool(alpha));
}

void Context::cullFace(CullFaceMode mode)
{
    mState.setCullMode(mode);
}

void Context::depthFunc(GLenum func)
{
    mState.setDepthFunc(func);
}

void Context::depthMask(GLboolean flag)
{
    mState.setDepthMask(ConvertToBool(flag));
}

void Context::depthRangef(GLfloat zNear, GLfloat zFar)
{
    mState.setDepthRange(clamp01(zNear), clamp01(zFar));
}

void Context::disable(GLenum cap)
{
    mState.setEnableFeature(cap, false);
    mStateCache.onContextCapChange(this);
}

void Context::disableVertexAttribArray(GLuint index)
{
    mState.setEnableVertexAttribArray(index, false);
    mStateCache.onVertexArrayStateChange(this);
}

void Context::enable(GLenum cap)
{
    mState.setEnableFeature(cap, true);
    mStateCache.onContextCapChange(this);
}

void Context::enableVertexAttribArray(GLuint index)
{
    mState.setEnableVertexAttribArray(index, true);
    mStateCache.onVertexArrayStateChange(this);
}

void Context::frontFace(GLenum mode)
{
    mState.setFrontFace(mode);
}

void Context::hint(GLenum target, GLenum mode)
{
    switch (target)
    {
        case GL_GENERATE_MIPMAP_HINT:
            mState.setGenerateMipmapHint(mode);
            break;

        case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:
            mState.setFragmentShaderDerivativeHint(mode);
            break;

        case GL_PERSPECTIVE_CORRECTION_HINT:
        case GL_POINT_SMOOTH_HINT:
        case GL_LINE_SMOOTH_HINT:
        case GL_FOG_HINT:
            mState.gles1().setHint(target, mode);
            break;
        default:
            UNREACHABLE();
            return;
    }
}

void Context::lineWidth(GLfloat width)
{
    mState.setLineWidth(width);
}

void Context::pixelStorei(GLenum pname, GLint param)
{
    switch (pname)
    {
        case GL_UNPACK_ALIGNMENT:
            mState.setUnpackAlignment(param);
            break;

        case GL_PACK_ALIGNMENT:
            mState.setPackAlignment(param);
            break;

        case GL_PACK_REVERSE_ROW_ORDER_ANGLE:
            mState.setPackReverseRowOrder(param != 0);
            break;

        case GL_UNPACK_ROW_LENGTH:
            ASSERT((getClientMajorVersion() >= 3) || getExtensions().unpackSubimage);
            mState.setUnpackRowLength(param);
            break;

        case GL_UNPACK_IMAGE_HEIGHT:
            ASSERT(getClientMajorVersion() >= 3);
            mState.setUnpackImageHeight(param);
            break;

        case GL_UNPACK_SKIP_IMAGES:
            ASSERT(getClientMajorVersion() >= 3);
            mState.setUnpackSkipImages(param);
            break;

        case GL_UNPACK_SKIP_ROWS:
            ASSERT((getClientMajorVersion() >= 3) || getExtensions().unpackSubimage);
            mState.setUnpackSkipRows(param);
            break;

        case GL_UNPACK_SKIP_PIXELS:
            ASSERT((getClientMajorVersion() >= 3) || getExtensions().unpackSubimage);
            mState.setUnpackSkipPixels(param);
            break;

        case GL_PACK_ROW_LENGTH:
            ASSERT((getClientMajorVersion() >= 3) || getExtensions().packSubimage);
            mState.setPackRowLength(param);
            break;

        case GL_PACK_SKIP_ROWS:
            ASSERT((getClientMajorVersion() >= 3) || getExtensions().packSubimage);
            mState.setPackSkipRows(param);
            break;

        case GL_PACK_SKIP_PIXELS:
            ASSERT((getClientMajorVersion() >= 3) || getExtensions().packSubimage);
            mState.setPackSkipPixels(param);
            break;

        default:
            UNREACHABLE();
            return;
    }
}

void Context::polygonOffset(GLfloat factor, GLfloat units)
{
    mState.setPolygonOffsetParams(factor, units);
}

void Context::sampleCoverage(GLfloat value, GLboolean invert)
{
    mState.setSampleCoverageParams(clamp01(value), ConvertToBool(invert));
}

void Context::sampleMaski(GLuint maskNumber, GLbitfield mask)
{
    mState.setSampleMaskParams(maskNumber, mask);
}

void Context::scissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
    mState.setScissorParams(x, y, width, height);
}

void Context::stencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask)
{
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK)
    {
        mState.setStencilParams(func, ref, mask);
    }

    if (face == GL_BACK || face == GL_FRONT_AND_BACK)
    {
        mState.setStencilBackParams(func, ref, mask);
    }

    mStateCache.onStencilStateChange(this);
}

void Context::stencilMaskSeparate(GLenum face, GLuint mask)
{
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK)
    {
        mState.setStencilWritemask(mask);
    }

    if (face == GL_BACK || face == GL_FRONT_AND_BACK)
    {
        mState.setStencilBackWritemask(mask);
    }

    mStateCache.onStencilStateChange(this);
}

void Context::stencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass)
{
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK)
    {
        mState.setStencilOperations(fail, zfail, zpass);
    }

    if (face == GL_BACK || face == GL_FRONT_AND_BACK)
    {
        mState.setStencilBackOperations(fail, zfail, zpass);
    }
}

void Context::vertexAttrib1f(GLuint index, GLfloat x)
{
    GLfloat vals[4] = {x, 0, 0, 1};
    mState.setVertexAttribf(index, vals);
    mStateCache.onDefaultVertexAttributeChange(this);
}

void Context::vertexAttrib1fv(GLuint index, const GLfloat *values)
{
    GLfloat vals[4] = {values[0], 0, 0, 1};
    mState.setVertexAttribf(index, vals);
    mStateCache.onDefaultVertexAttributeChange(this);
}

void Context::vertexAttrib2f(GLuint index, GLfloat x, GLfloat y)
{
    GLfloat vals[4] = {x, y, 0, 1};
    mState.setVertexAttribf(index, vals);
    mStateCache.onDefaultVertexAttributeChange(this);
}

void Context::vertexAttrib2fv(GLuint index, const GLfloat *values)
{
    GLfloat vals[4] = {values[0], values[1], 0, 1};
    mState.setVertexAttribf(index, vals);
    mStateCache.onDefaultVertexAttributeChange(this);
}

void Context::vertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z)
{
    GLfloat vals[4] = {x, y, z, 1};
    mState.setVertexAttribf(index, vals);
    mStateCache.onDefaultVertexAttributeChange(this);
}

void Context::vertexAttrib3fv(GLuint index, const GLfloat *values)
{
    GLfloat vals[4] = {values[0], values[1], values[2], 1};
    mState.setVertexAttribf(index, vals);
    mStateCache.onDefaultVertexAttributeChange(this);
}

void Context::vertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    GLfloat vals[4] = {x, y, z, w};
    mState.setVertexAttribf(index, vals);
    mStateCache.onDefaultVertexAttributeChange(this);
}

void Context::vertexAttrib4fv(GLuint index, const GLfloat *values)
{
    mState.setVertexAttribf(index, values);
    mStateCache.onDefaultVertexAttributeChange(this);
}

void Context::vertexAttribPointer(GLuint index,
                                  GLint size,
                                  VertexAttribType type,
                                  GLboolean normalized,
                                  GLsizei stride,
                                  const void *ptr)
{
    mState.setVertexAttribPointer(this, index, mState.getTargetBuffer(BufferBinding::Array), size,
                                  type, ConvertToBool(normalized), stride, ptr);
    mStateCache.onVertexArrayStateChange(this);
}

void Context::vertexAttribFormat(GLuint attribIndex,
                                 GLint size,
                                 VertexAttribType type,
                                 GLboolean normalized,
                                 GLuint relativeOffset)
{
    mState.setVertexAttribFormat(attribIndex, size, type, ConvertToBool(normalized), false,
                                 relativeOffset);
    mStateCache.onVertexArrayFormatChange(this);
}

void Context::vertexAttribIFormat(GLuint attribIndex,
                                  GLint size,
                                  VertexAttribType type,
                                  GLuint relativeOffset)
{
    mState.setVertexAttribFormat(attribIndex, size, type, false, true, relativeOffset);
    mStateCache.onVertexArrayFormatChange(this);
}

void Context::vertexAttribBinding(GLuint attribIndex, GLuint bindingIndex)
{
    mState.setVertexAttribBinding(this, attribIndex, bindingIndex);
    mStateCache.onVertexArrayStateChange(this);
}

void Context::vertexBindingDivisor(GLuint bindingIndex, GLuint divisor)
{
    mState.setVertexBindingDivisor(bindingIndex, divisor);
    mStateCache.onVertexArrayFormatChange(this);
}

void Context::viewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    mState.setViewportParams(x, y, width, height);
}

void Context::vertexAttribIPointer(GLuint index,
                                   GLint size,
                                   VertexAttribType type,
                                   GLsizei stride,
                                   const void *pointer)
{
    mState.setVertexAttribIPointer(this, index, mState.getTargetBuffer(BufferBinding::Array), size,
                                   type, stride, pointer);
    mStateCache.onVertexArrayStateChange(this);
}

void Context::vertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w)
{
    GLint vals[4] = {x, y, z, w};
    mState.setVertexAttribi(index, vals);
    mStateCache.onDefaultVertexAttributeChange(this);
}

void Context::vertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w)
{
    GLuint vals[4] = {x, y, z, w};
    mState.setVertexAttribu(index, vals);
    mStateCache.onDefaultVertexAttributeChange(this);
}

void Context::vertexAttribI4iv(GLuint index, const GLint *v)
{
    mState.setVertexAttribi(index, v);
    mStateCache.onDefaultVertexAttributeChange(this);
}

void Context::vertexAttribI4uiv(GLuint index, const GLuint *v)
{
    mState.setVertexAttribu(index, v);
    mStateCache.onDefaultVertexAttributeChange(this);
}

void Context::getVertexAttribiv(GLuint index, GLenum pname, GLint *params)
{
    const VertexAttribCurrentValueData &currentValues =
        getState().getVertexAttribCurrentValue(index);
    const VertexArray *vao = getState().getVertexArray();
    QueryVertexAttribiv(vao->getVertexAttribute(index), vao->getBindingFromAttribIndex(index),
                        currentValues, pname, params);
}

void Context::getVertexAttribivRobust(GLuint index,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLint *params)
{
    getVertexAttribiv(index, pname, params);
}

void Context::getVertexAttribfv(GLuint index, GLenum pname, GLfloat *params)
{
    const VertexAttribCurrentValueData &currentValues =
        getState().getVertexAttribCurrentValue(index);
    const VertexArray *vao = getState().getVertexArray();
    QueryVertexAttribfv(vao->getVertexAttribute(index), vao->getBindingFromAttribIndex(index),
                        currentValues, pname, params);
}

void Context::getVertexAttribfvRobust(GLuint index,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLfloat *params)
{
    getVertexAttribfv(index, pname, params);
}

void Context::getVertexAttribIiv(GLuint index, GLenum pname, GLint *params)
{
    const VertexAttribCurrentValueData &currentValues =
        getState().getVertexAttribCurrentValue(index);
    const VertexArray *vao = getState().getVertexArray();
    QueryVertexAttribIiv(vao->getVertexAttribute(index), vao->getBindingFromAttribIndex(index),
                         currentValues, pname, params);
}

void Context::getVertexAttribIivRobust(GLuint index,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       GLint *params)
{
    getVertexAttribIiv(index, pname, params);
}

void Context::getVertexAttribIuiv(GLuint index, GLenum pname, GLuint *params)
{
    const VertexAttribCurrentValueData &currentValues =
        getState().getVertexAttribCurrentValue(index);
    const VertexArray *vao = getState().getVertexArray();
    QueryVertexAttribIuiv(vao->getVertexAttribute(index), vao->getBindingFromAttribIndex(index),
                          currentValues, pname, params);
}

void Context::getVertexAttribIuivRobust(GLuint index,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLuint *params)
{
    getVertexAttribIuiv(index, pname, params);
}

void Context::getVertexAttribPointerv(GLuint index, GLenum pname, void **pointer)
{
    const VertexAttribute &attrib = getState().getVertexArray()->getVertexAttribute(index);
    QueryVertexAttribPointerv(attrib, pname, pointer);
}

void Context::getVertexAttribPointervRobust(GLuint index,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            void **pointer)
{
    getVertexAttribPointerv(index, pname, pointer);
}

void Context::debugMessageControl(GLenum source,
                                  GLenum type,
                                  GLenum severity,
                                  GLsizei count,
                                  const GLuint *ids,
                                  GLboolean enabled)
{
    std::vector<GLuint> idVector(ids, ids + count);
    mState.getDebug().setMessageControl(source, type, severity, std::move(idVector),
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
    mState.getDebug().insertMessage(source, type, id, severity, std::move(msg));
}

void Context::debugMessageCallback(GLDEBUGPROCKHR callback, const void *userParam)
{
    mState.getDebug().setCallback(callback, userParam);
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
    return static_cast<GLuint>(mState.getDebug().getMessages(count, bufSize, sources, types, ids,
                                                             severities, lengths, messageLog));
}

void Context::pushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar *message)
{
    std::string msg(message, (length > 0) ? static_cast<size_t>(length) : strlen(message));
    mImplementation->pushDebugGroup(source, id, msg);
    mState.getDebug().pushGroup(source, id, std::move(msg));
}

void Context::popDebugGroup()
{
    mState.getDebug().popGroup();
    mImplementation->popDebugGroup();
}

void Context::bufferData(BufferBinding target, GLsizeiptr size, const void *data, BufferUsage usage)
{
    Buffer *buffer = mState.getTargetBuffer(target);
    ASSERT(buffer);
    ANGLE_CONTEXT_TRY(buffer->bufferData(this, target, data, size, usage));
}

void Context::bufferSubData(BufferBinding target,
                            GLintptr offset,
                            GLsizeiptr size,
                            const void *data)
{
    if (data == nullptr || size == 0)
    {
        return;
    }

    Buffer *buffer = mState.getTargetBuffer(target);
    ASSERT(buffer);
    ANGLE_CONTEXT_TRY(buffer->bufferSubData(this, target, data, size, offset));
}

void Context::attachShader(GLuint program, GLuint shader)
{
    Program *programObject = mState.mShaderProgramManager->getProgram(program);
    Shader *shaderObject   = mState.mShaderProgramManager->getShader(shader);
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
    Buffer *readBuffer  = mState.getTargetBuffer(readTarget);
    Buffer *writeBuffer = mState.getTargetBuffer(writeTarget);

    ANGLE_CONTEXT_TRY(
        writeBuffer->copyBufferSubData(this, readBuffer, readOffset, writeOffset, size));
}

void Context::bindAttribLocation(GLuint program, GLuint index, const GLchar *name)
{
    // Ideally we could share the program query with the validation layer if possible.
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->bindAttributeLocation(index, name);
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
    Buffer *object = mState.mBufferManager->checkBufferAllocation(mImplementation.get(), buffer);
    ANGLE_CONTEXT_TRY(mState.setIndexedBufferBinding(this, target, index, object, offset, size));
    if (target == BufferBinding::Uniform)
    {
        mUniformBufferObserverBindings[index].bind(object);
        mStateCache.onUniformBufferStateChange(this);
    }
    else
    {
        mStateCache.onBufferBindingChange(this);
    }
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
    Renderbuffer *object = mState.mRenderbufferManager->checkRenderbufferAllocation(
        mImplementation.get(), renderbuffer);
    mState.setRenderbufferBinding(this, object);
}

void Context::texStorage2DMultisample(TextureType target,
                                      GLsizei samples,
                                      GLenum internalformat,
                                      GLsizei width,
                                      GLsizei height,
                                      GLboolean fixedsamplelocations)
{
    Extents size(width, height, 1);
    Texture *texture = getTextureByType(target);
    ANGLE_CONTEXT_TRY(texture->setStorageMultisample(this, target, samples, internalformat, size,
                                                     ConvertToBool(fixedsamplelocations)));
}

void Context::texStorage3DMultisample(TextureType target,
                                      GLsizei samples,
                                      GLenum internalformat,
                                      GLsizei width,
                                      GLsizei height,
                                      GLsizei depth,
                                      GLboolean fixedsamplelocations)
{
    Extents size(width, height, depth);
    Texture *texture = getTextureByType(target);
    ANGLE_CONTEXT_TRY(texture->setStorageMultisample(this, target, samples, internalformat, size,
                                                     ConvertToBool(fixedsamplelocations)));
}

void Context::getMultisamplefv(GLenum pname, GLuint index, GLfloat *val)
{
    // According to spec 3.1 Table 20.49: Framebuffer Dependent Values,
    // the sample position should be queried by DRAW_FRAMEBUFFER.
    ANGLE_CONTEXT_TRY(mState.syncDirtyObject(this, GL_DRAW_FRAMEBUFFER));
    const Framebuffer *framebuffer = mState.getDrawFramebuffer();

    switch (pname)
    {
        case GL_SAMPLE_POSITION:
            ANGLE_CONTEXT_TRY(framebuffer->getSamplePosition(this, index, val));
            break;
        default:
            UNREACHABLE();
    }
}

void Context::getMultisamplefvRobust(GLenum pname,
                                     GLuint index,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLfloat *val)
{
    UNIMPLEMENTED();
}

void Context::renderbufferStorage(GLenum target,
                                  GLenum internalformat,
                                  GLsizei width,
                                  GLsizei height)
{
    // Hack for the special WebGL 1 "DEPTH_STENCIL" internal format.
    GLenum convertedInternalFormat = getConvertedRenderbufferFormat(internalformat);

    Renderbuffer *renderbuffer = mState.getCurrentRenderbuffer();
    ANGLE_CONTEXT_TRY(renderbuffer->setStorage(this, convertedInternalFormat, width, height));
}

void Context::renderbufferStorageMultisample(GLenum target,
                                             GLsizei samples,
                                             GLenum internalformat,
                                             GLsizei width,
                                             GLsizei height)
{
    // Hack for the special WebGL 1 "DEPTH_STENCIL" internal format.
    GLenum convertedInternalFormat = getConvertedRenderbufferFormat(internalformat);

    Renderbuffer *renderbuffer = mState.getCurrentRenderbuffer();
    ANGLE_CONTEXT_TRY(
        renderbuffer->setStorageMultisample(this, samples, convertedInternalFormat, width, height));
}

void Context::getSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *values)
{
    const Sync *syncObject = nullptr;
    if (!mContextLost)
    {
        syncObject = getSync(sync);
    }
    ANGLE_CONTEXT_TRY(QuerySynciv(this, syncObject, pname, bufSize, length, values));
}

void Context::getFramebufferParameteriv(GLenum target, GLenum pname, GLint *params)
{
    Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
    QueryFramebufferParameteriv(framebuffer, pname, params);
}

void Context::getFramebufferParameterivRobust(GLenum target,
                                              GLenum pname,
                                              GLsizei bufSize,
                                              GLsizei *length,
                                              GLint *params)
{
    UNIMPLEMENTED();
}

void Context::framebufferParameteri(GLenum target, GLenum pname, GLint param)
{
    Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
    SetFramebufferParameteri(this, framebuffer, pname, param);
}

bool Context::getScratchBuffer(size_t requstedSizeBytes,
                               angle::MemoryBuffer **scratchBufferOut) const
{
    return mScratchBuffer.get(requstedSizeBytes, scratchBufferOut);
}

bool Context::getZeroFilledBuffer(size_t requstedSizeBytes,
                                  angle::MemoryBuffer **zeroBufferOut) const
{
    return mZeroFilledBuffer.getInitialized(requstedSizeBytes, zeroBufferOut, 0);
}

void Context::dispatchCompute(GLuint numGroupsX, GLuint numGroupsY, GLuint numGroupsZ)
{
    if (numGroupsX == 0u || numGroupsY == 0u || numGroupsZ == 0u)
    {
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForDispatch());
    ANGLE_CONTEXT_TRY(mImplementation->dispatchCompute(this, numGroupsX, numGroupsY, numGroupsZ));
}

void Context::dispatchComputeIndirect(GLintptr indirect)
{
    ANGLE_CONTEXT_TRY(prepareForDispatch());
    ANGLE_CONTEXT_TRY(mImplementation->dispatchComputeIndirect(this, indirect));
}

void Context::texStorage2D(TextureType target,
                           GLsizei levels,
                           GLenum internalFormat,
                           GLsizei width,
                           GLsizei height)
{
    Extents size(width, height, 1);
    Texture *texture = getTextureByType(target);
    ANGLE_CONTEXT_TRY(texture->setStorage(this, target, levels, internalFormat, size));
}

void Context::texStorage3D(TextureType target,
                           GLsizei levels,
                           GLenum internalFormat,
                           GLsizei width,
                           GLsizei height,
                           GLsizei depth)
{
    Extents size(width, height, depth);
    Texture *texture = getTextureByType(target);
    ANGLE_CONTEXT_TRY(texture->setStorage(this, target, levels, internalFormat, size));
}

void Context::memoryBarrier(GLbitfield barriers)
{
    ANGLE_CONTEXT_TRY(mImplementation->memoryBarrier(this, barriers));
}

void Context::memoryBarrierByRegion(GLbitfield barriers)
{
    ANGLE_CONTEXT_TRY(mImplementation->memoryBarrierByRegion(this, barriers));
}

void Context::multiDrawArrays(PrimitiveMode mode,
                              const GLint *firsts,
                              const GLsizei *counts,
                              GLsizei drawcount)
{
    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    Program *programObject = mState.getLinkedProgram(this);
    const bool hasDrawID   = programObject && programObject->hasDrawIDUniform();
    if (hasDrawID)
    {
        for (GLsizei drawID = 0; drawID < drawcount; ++drawID)
        {
            if (noopDraw(mode, counts[drawID]))
            {
                continue;
            }
            programObject->setDrawIDUniform(drawID);
            ANGLE_CONTEXT_TRY(
                mImplementation->drawArrays(this, mode, firsts[drawID], counts[drawID]));
            MarkTransformFeedbackBufferUsage(this, counts[drawID], 1);
        }
    }
    else
    {
        for (GLsizei drawID = 0; drawID < drawcount; ++drawID)
        {
            if (noopDraw(mode, counts[drawID]))
            {
                continue;
            }
            ANGLE_CONTEXT_TRY(
                mImplementation->drawArrays(this, mode, firsts[drawID], counts[drawID]));
            MarkTransformFeedbackBufferUsage(this, counts[drawID], 1);
        }
    }
}

void Context::multiDrawArraysInstanced(PrimitiveMode mode,
                                       const GLint *firsts,
                                       const GLsizei *counts,
                                       const GLsizei *instanceCounts,
                                       GLsizei drawcount)
{
    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    Program *programObject = mState.getLinkedProgram(this);
    const bool hasDrawID   = programObject && programObject->hasDrawIDUniform();
    if (hasDrawID)
    {
        for (GLsizei drawID = 0; drawID < drawcount; ++drawID)
        {
            if (noopDrawInstanced(mode, counts[drawID], instanceCounts[drawID]))
            {
                continue;
            }
            programObject->setDrawIDUniform(drawID);
            ANGLE_CONTEXT_TRY(mImplementation->drawArraysInstanced(
                this, mode, firsts[drawID], counts[drawID], instanceCounts[drawID]));
            MarkTransformFeedbackBufferUsage(this, counts[drawID], instanceCounts[drawID]);
        }
    }
    else
    {
        for (GLsizei drawID = 0; drawID < drawcount; ++drawID)
        {
            if (noopDrawInstanced(mode, counts[drawID], instanceCounts[drawID]))
            {
                continue;
            }
            ANGLE_CONTEXT_TRY(mImplementation->drawArraysInstanced(
                this, mode, firsts[drawID], counts[drawID], instanceCounts[drawID]));
            MarkTransformFeedbackBufferUsage(this, counts[drawID], instanceCounts[drawID]);
        }
    }
}

void Context::multiDrawElements(PrimitiveMode mode,
                                const GLsizei *counts,
                                DrawElementsType type,
                                const GLvoid *const *indices,
                                GLsizei drawcount)
{
    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    Program *programObject = mState.getLinkedProgram(this);
    const bool hasDrawID   = programObject && programObject->hasDrawIDUniform();
    if (hasDrawID)
    {
        for (GLsizei drawID = 0; drawID < drawcount; ++drawID)
        {
            if (noopDraw(mode, counts[drawID]))
            {
                continue;
            }
            programObject->setDrawIDUniform(drawID);
            ANGLE_CONTEXT_TRY(
                mImplementation->drawElements(this, mode, counts[drawID], type, indices[drawID]));
        }
    }
    else
    {
        for (GLsizei drawID = 0; drawID < drawcount; ++drawID)
        {
            if (noopDraw(mode, counts[drawID]))
            {
                continue;
            }
            ANGLE_CONTEXT_TRY(
                mImplementation->drawElements(this, mode, counts[drawID], type, indices[drawID]));
        }
    }
}

void Context::multiDrawElementsInstanced(PrimitiveMode mode,
                                         const GLsizei *counts,
                                         DrawElementsType type,
                                         const GLvoid *const *indices,
                                         const GLsizei *instanceCounts,
                                         GLsizei drawcount)
{
    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    Program *programObject = mState.getLinkedProgram(this);
    const bool hasDrawID   = programObject && programObject->hasDrawIDUniform();
    if (hasDrawID)
    {
        for (GLsizei drawID = 0; drawID < drawcount; ++drawID)
        {
            if (noopDrawInstanced(mode, counts[drawID], instanceCounts[drawID]))
            {
                continue;
            }
            programObject->setDrawIDUniform(drawID);
            ANGLE_CONTEXT_TRY(mImplementation->drawElementsInstanced(
                this, mode, counts[drawID], type, indices[drawID], instanceCounts[drawID]));
        }
    }
    else
    {
        for (GLsizei drawID = 0; drawID < drawcount; ++drawID)
        {
            if (noopDrawInstanced(mode, counts[drawID], instanceCounts[drawID]))
            {
                continue;
            }
            ANGLE_CONTEXT_TRY(mImplementation->drawElementsInstanced(
                this, mode, counts[drawID], type, indices[drawID], instanceCounts[drawID]));
        }
    }
}

void Context::provokingVertex(ProvokingVertex provokeMode)
{
    mState.setProvokingVertex(provokeMode);
}

GLenum Context::checkFramebufferStatus(GLenum target)
{
    Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
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
    Program *programObject = getProgramNoResolveLink(program);
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
    Program *programObject = getProgramResolveLink(program);
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
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->getActiveUniform(index, bufsize, length, size, type, name);
}

void Context::getAttachedShaders(GLuint program, GLsizei maxcount, GLsizei *count, GLuint *shaders)
{
    Program *programObject = getProgramNoResolveLink(program);
    ASSERT(programObject);
    programObject->getAttachedShaders(maxcount, count, shaders);
}

GLint Context::getAttribLocation(GLuint program, const GLchar *name)
{
    Program *programObject = getProgramResolveLink(program);
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

void Context::getBooleanvRobust(GLenum pname, GLsizei bufSize, GLsizei *length, GLboolean *params)
{
    getBooleanv(pname, params);
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

void Context::getFloatvRobust(GLenum pname, GLsizei bufSize, GLsizei *length, GLfloat *params)
{
    getFloatv(pname, params);
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

void Context::getIntegervRobust(GLenum pname, GLsizei bufSize, GLsizei *length, GLint *data)
{
    getIntegerv(pname, data);
}

void Context::getProgramiv(GLuint program, GLenum pname, GLint *params)
{
    Program *programObject = nullptr;
    if (!mContextLost)
    {
        // Don't resolve link if checking the link completion status.
        programObject = (pname == GL_COMPLETION_STATUS_KHR ? getProgramNoResolveLink(program)
                                                           : getProgramResolveLink(program));
        ASSERT(programObject);
    }
    QueryProgramiv(this, programObject, pname, params);
}

void Context::getProgramivRobust(GLuint program,
                                 GLenum pname,
                                 GLsizei bufSize,
                                 GLsizei *length,
                                 GLint *params)
{
    getProgramiv(program, pname, params);
}

void Context::getProgramPipelineiv(GLuint pipeline, GLenum pname, GLint *params)
{
    UNIMPLEMENTED();
}

MemoryObject *Context::getMemoryObject(GLuint handle) const
{
    return mState.mMemoryObjectManager->getMemoryObject(handle);
}

void Context::getProgramInfoLog(GLuint program, GLsizei bufsize, GLsizei *length, GLchar *infolog)
{
    Program *programObject = getProgramResolveLink(program);
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
    Shader *shaderObject = nullptr;
    if (!mContextLost)
    {
        shaderObject = getShader(shader);
        ASSERT(shaderObject);
    }
    QueryShaderiv(this, shaderObject, pname, params);
}

void Context::getShaderivRobust(GLuint shader,
                                GLenum pname,
                                GLsizei bufSize,
                                GLsizei *length,
                                GLint *params)
{
    getShaderiv(shader, pname, params);
}

void Context::getShaderInfoLog(GLuint shader, GLsizei bufsize, GLsizei *length, GLchar *infolog)
{
    Shader *shaderObject = getShader(shader);
    ASSERT(shaderObject);
    shaderObject->getInfoLog(bufsize, length, infolog);
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
                    mState.mCaps.vertexLowpFloat.get(range, precision);
                    break;
                case GL_MEDIUM_FLOAT:
                    mState.mCaps.vertexMediumpFloat.get(range, precision);
                    break;
                case GL_HIGH_FLOAT:
                    mState.mCaps.vertexHighpFloat.get(range, precision);
                    break;

                case GL_LOW_INT:
                    mState.mCaps.vertexLowpInt.get(range, precision);
                    break;
                case GL_MEDIUM_INT:
                    mState.mCaps.vertexMediumpInt.get(range, precision);
                    break;
                case GL_HIGH_INT:
                    mState.mCaps.vertexHighpInt.get(range, precision);
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
                    mState.mCaps.fragmentLowpFloat.get(range, precision);
                    break;
                case GL_MEDIUM_FLOAT:
                    mState.mCaps.fragmentMediumpFloat.get(range, precision);
                    break;
                case GL_HIGH_FLOAT:
                    mState.mCaps.fragmentHighpFloat.get(range, precision);
                    break;

                case GL_LOW_INT:
                    mState.mCaps.fragmentLowpInt.get(range, precision);
                    break;
                case GL_MEDIUM_INT:
                    mState.mCaps.fragmentMediumpInt.get(range, precision);
                    break;
                case GL_HIGH_INT:
                    mState.mCaps.fragmentHighpInt.get(range, precision);
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
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->getUniformfv(this, location, params);
}

void Context::getUniformfvRobust(GLuint program,
                                 GLint location,
                                 GLsizei bufSize,
                                 GLsizei *length,
                                 GLfloat *params)
{
    getUniformfv(program, location, params);
}

void Context::getUniformiv(GLuint program, GLint location, GLint *params)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->getUniformiv(this, location, params);
}

void Context::getUniformivRobust(GLuint program,
                                 GLint location,
                                 GLsizei bufSize,
                                 GLsizei *length,
                                 GLint *params)
{
    getUniformiv(program, location, params);
}

GLint Context::getUniformLocation(GLuint program, const GLchar *name)
{
    Program *programObject = getProgramResolveLink(program);
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
    return mState.getEnableFeature(cap);
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

    return (getProgramNoResolveLink(program) ? GL_TRUE : GL_FALSE);
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
    Program *programObject = getProgramNoResolveLink(program);
    ASSERT(programObject);
    ANGLE_CONTEXT_TRY(programObject->link(this));
    ANGLE_CONTEXT_TRY(onProgramLink(programObject));
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

void Context::bindFragDataLocationIndexed(GLuint program,
                                          GLuint colorNumber,
                                          GLuint index,
                                          const char *name)
{
    Program *programObject = getProgramNoResolveLink(program);
    programObject->bindFragmentOutputLocation(colorNumber, name);
    programObject->bindFragmentOutputIndex(index, name);
}

void Context::bindFragDataLocation(GLuint program, GLuint colorNumber, const char *name)
{
    bindFragDataLocationIndexed(program, colorNumber, 0u, name);
}

int Context::getFragDataIndex(GLuint program, const char *name)
{
    Program *programObject = getProgramResolveLink(program);
    return programObject->getFragDataIndex(name);
}

int Context::getProgramResourceLocationIndex(GLuint program,
                                             GLenum programInterface,
                                             const char *name)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programInterface == GL_PROGRAM_OUTPUT);
    return programObject->getFragDataIndex(name);
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
    Program *program = mState.getProgram();
    program->setUniform1fv(location, 1, &x);
}

void Context::uniform1fv(GLint location, GLsizei count, const GLfloat *v)
{
    Program *program = mState.getProgram();
    program->setUniform1fv(location, count, v);
}

void Context::setUniform1iImpl(Program *program, GLint location, GLsizei count, const GLint *v)
{
    program->setUniform1iv(this, location, count, v);
}

void Context::onSamplerUniformChange(size_t textureUnitIndex)
{
    mState.onActiveTextureChange(this, textureUnitIndex);
    mStateCache.onActiveTextureChange(this);
}

void Context::uniform1i(GLint location, GLint x)
{
    setUniform1iImpl(mState.getProgram(), location, 1, &x);
}

void Context::uniform1iv(GLint location, GLsizei count, const GLint *v)
{
    setUniform1iImpl(mState.getProgram(), location, count, v);
}

void Context::uniform2f(GLint location, GLfloat x, GLfloat y)
{
    GLfloat xy[2]    = {x, y};
    Program *program = mState.getProgram();
    program->setUniform2fv(location, 1, xy);
}

void Context::uniform2fv(GLint location, GLsizei count, const GLfloat *v)
{
    Program *program = mState.getProgram();
    program->setUniform2fv(location, count, v);
}

void Context::uniform2i(GLint location, GLint x, GLint y)
{
    GLint xy[2]      = {x, y};
    Program *program = mState.getProgram();
    program->setUniform2iv(location, 1, xy);
}

void Context::uniform2iv(GLint location, GLsizei count, const GLint *v)
{
    Program *program = mState.getProgram();
    program->setUniform2iv(location, count, v);
}

void Context::uniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z)
{
    GLfloat xyz[3]   = {x, y, z};
    Program *program = mState.getProgram();
    program->setUniform3fv(location, 1, xyz);
}

void Context::uniform3fv(GLint location, GLsizei count, const GLfloat *v)
{
    Program *program = mState.getProgram();
    program->setUniform3fv(location, count, v);
}

void Context::uniform3i(GLint location, GLint x, GLint y, GLint z)
{
    GLint xyz[3]     = {x, y, z};
    Program *program = mState.getProgram();
    program->setUniform3iv(location, 1, xyz);
}

void Context::uniform3iv(GLint location, GLsizei count, const GLint *v)
{
    Program *program = mState.getProgram();
    program->setUniform3iv(location, count, v);
}

void Context::uniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    GLfloat xyzw[4]  = {x, y, z, w};
    Program *program = mState.getProgram();
    program->setUniform4fv(location, 1, xyzw);
}

void Context::uniform4fv(GLint location, GLsizei count, const GLfloat *v)
{
    Program *program = mState.getProgram();
    program->setUniform4fv(location, count, v);
}

void Context::uniform4i(GLint location, GLint x, GLint y, GLint z, GLint w)
{
    GLint xyzw[4]    = {x, y, z, w};
    Program *program = mState.getProgram();
    program->setUniform4iv(location, 1, xyzw);
}

void Context::uniform4iv(GLint location, GLsizei count, const GLint *v)
{
    Program *program = mState.getProgram();
    program->setUniform4iv(location, count, v);
}

void Context::uniformMatrix2fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value)
{
    Program *program = mState.getProgram();
    program->setUniformMatrix2fv(location, count, transpose, value);
}

void Context::uniformMatrix3fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value)
{
    Program *program = mState.getProgram();
    program->setUniformMatrix3fv(location, count, transpose, value);
}

void Context::uniformMatrix4fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value)
{
    Program *program = mState.getProgram();
    program->setUniformMatrix4fv(location, count, transpose, value);
}

void Context::validateProgram(GLuint program)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->validate(mState.mCaps);
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
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject != nullptr);

    ANGLE_CONTEXT_TRY(programObject->saveBinary(this, binaryFormat, binary, bufSize, length));
}

void Context::programBinary(GLuint program, GLenum binaryFormat, const void *binary, GLsizei length)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject != nullptr);

    ANGLE_CONTEXT_TRY(programObject->loadBinary(this, binaryFormat, binary, length));
    ANGLE_CONTEXT_TRY(onProgramLink(programObject));
}

void Context::uniform1ui(GLint location, GLuint v0)
{
    Program *program = mState.getProgram();
    program->setUniform1uiv(location, 1, &v0);
}

void Context::uniform2ui(GLint location, GLuint v0, GLuint v1)
{
    Program *program  = mState.getProgram();
    const GLuint xy[] = {v0, v1};
    program->setUniform2uiv(location, 1, xy);
}

void Context::uniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2)
{
    Program *program   = mState.getProgram();
    const GLuint xyz[] = {v0, v1, v2};
    program->setUniform3uiv(location, 1, xyz);
}

void Context::uniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
    Program *program    = mState.getProgram();
    const GLuint xyzw[] = {v0, v1, v2, v3};
    program->setUniform4uiv(location, 1, xyzw);
}

void Context::uniform1uiv(GLint location, GLsizei count, const GLuint *value)
{
    Program *program = mState.getProgram();
    program->setUniform1uiv(location, count, value);
}
void Context::uniform2uiv(GLint location, GLsizei count, const GLuint *value)
{
    Program *program = mState.getProgram();
    program->setUniform2uiv(location, count, value);
}

void Context::uniform3uiv(GLint location, GLsizei count, const GLuint *value)
{
    Program *program = mState.getProgram();
    program->setUniform3uiv(location, count, value);
}

void Context::uniform4uiv(GLint location, GLsizei count, const GLuint *value)
{
    Program *program = mState.getProgram();
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
    return (getQuery(id, false, QueryType::InvalidEnum) != nullptr) ? GL_TRUE : GL_FALSE;
}

void Context::uniformMatrix2x3fv(GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value)
{
    Program *program = mState.getProgram();
    program->setUniformMatrix2x3fv(location, count, transpose, value);
}

void Context::uniformMatrix3x2fv(GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value)
{
    Program *program = mState.getProgram();
    program->setUniformMatrix3x2fv(location, count, transpose, value);
}

void Context::uniformMatrix2x4fv(GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value)
{
    Program *program = mState.getProgram();
    program->setUniformMatrix2x4fv(location, count, transpose, value);
}

void Context::uniformMatrix4x2fv(GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value)
{
    Program *program = mState.getProgram();
    program->setUniformMatrix4x2fv(location, count, transpose, value);
}

void Context::uniformMatrix3x4fv(GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value)
{
    Program *program = mState.getProgram();
    program->setUniformMatrix3x4fv(location, count, transpose, value);
}

void Context::uniformMatrix4x3fv(GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value)
{
    Program *program = mState.getProgram();
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
    TransformFeedback *transformFeedback = mState.getCurrentTransformFeedback();
    ANGLE_CONTEXT_TRY(transformFeedback->end(this));
    mStateCache.onActiveTransformFeedbackChange(this);
}

void Context::transformFeedbackVaryings(GLuint program,
                                        GLsizei count,
                                        const GLchar *const *varyings,
                                        GLenum bufferMode)
{
    Program *programObject = getProgramResolveLink(program);
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
    Program *programObject = getProgramResolveLink(program);
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
    TransformFeedback *transformFeedback = mState.getCurrentTransformFeedback();
    ANGLE_CONTEXT_TRY(transformFeedback->pause(this));
    mStateCache.onActiveTransformFeedbackChange(this);
}

void Context::resumeTransformFeedback()
{
    TransformFeedback *transformFeedback = mState.getCurrentTransformFeedback();
    ANGLE_CONTEXT_TRY(transformFeedback->resume(this));
    mStateCache.onActiveTransformFeedbackChange(this);
}

void Context::getUniformuiv(GLuint program, GLint location, GLuint *params)
{
    const Program *programObject = getProgramResolveLink(program);
    programObject->getUniformuiv(this, location, params);
}

void Context::getUniformuivRobust(GLuint program,
                                  GLint location,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLuint *params)
{
    getUniformuiv(program, location, params);
}

GLint Context::getFragDataLocation(GLuint program, const GLchar *name)
{
    const Program *programObject = getProgramResolveLink(program);
    return programObject->getFragDataLocation(name);
}

void Context::getUniformIndices(GLuint program,
                                GLsizei uniformCount,
                                const GLchar *const *uniformNames,
                                GLuint *uniformIndices)
{
    const Program *programObject = getProgramResolveLink(program);
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
    const Program *programObject = getProgramResolveLink(program);
    for (int uniformId = 0; uniformId < uniformCount; uniformId++)
    {
        const GLuint index = uniformIndices[uniformId];
        params[uniformId]  = GetUniformResourceProperty(programObject, index, pname);
    }
}

GLuint Context::getUniformBlockIndex(GLuint program, const GLchar *uniformBlockName)
{
    const Program *programObject = getProgramResolveLink(program);
    return programObject->getUniformBlockIndex(uniformBlockName);
}

void Context::getActiveUniformBlockiv(GLuint program,
                                      GLuint uniformBlockIndex,
                                      GLenum pname,
                                      GLint *params)
{
    const Program *programObject = getProgramResolveLink(program);
    QueryActiveUniformBlockiv(programObject, uniformBlockIndex, pname, params);
}

void Context::getActiveUniformBlockivRobust(GLuint program,
                                            GLuint uniformBlockIndex,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLint *params)
{
    getActiveUniformBlockiv(program, uniformBlockIndex, pname, params);
}

void Context::getActiveUniformBlockName(GLuint program,
                                        GLuint uniformBlockIndex,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLchar *uniformBlockName)
{
    const Program *programObject = getProgramResolveLink(program);
    programObject->getActiveUniformBlockName(uniformBlockIndex, bufSize, length, uniformBlockName);
}

void Context::uniformBlockBinding(GLuint program,
                                  GLuint uniformBlockIndex,
                                  GLuint uniformBlockBinding)
{
    Program *programObject = getProgramResolveLink(program);
    programObject->bindUniformBlock(uniformBlockIndex, uniformBlockBinding);

    // Note: If the Program is shared between Contexts we would be better using Observer/Subject.
    if (programObject->isInUse())
    {
        mState.setObjectDirty(GL_PROGRAM);
        mStateCache.onUniformBufferStateChange(this);
    }
}

GLsync Context::fenceSync(GLenum condition, GLbitfield flags)
{
    GLuint handle     = mState.mSyncManager->createSync(mImplementation.get());
    GLsync syncHandle = reinterpret_cast<GLsync>(static_cast<uintptr_t>(handle));

    Sync *syncObject = getSync(syncHandle);
    if (syncObject->set(this, condition, flags) == angle::Result::Stop)
    {
        deleteSync(syncHandle);
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
    if (syncObject->clientWait(this, flags, timeout, &result) == angle::Result::Stop)
    {
        return GL_WAIT_FAILED;
    }
    return result;
}

void Context::waitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
    Sync *syncObject = getSync(sync);
    ANGLE_CONTEXT_TRY(syncObject->serverWait(this, flags, timeout));
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

void Context::getInteger64vRobust(GLenum pname, GLsizei bufSize, GLsizei *length, GLint64 *data)
{
    getInteger64v(pname, data);
}

void Context::getBufferParameteri64v(BufferBinding target, GLenum pname, GLint64 *params)
{
    Buffer *buffer = mState.getTargetBuffer(target);
    QueryBufferParameteri64v(buffer, pname, params);
}

void Context::getBufferParameteri64vRobust(BufferBinding target,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLint64 *params)
{
    getBufferParameteri64v(target, pname, params);
}

void Context::genSamplers(GLsizei count, GLuint *samplers)
{
    for (int i = 0; i < count; i++)
    {
        samplers[i] = mState.mSamplerManager->createSampler();
    }
}

void Context::deleteSamplers(GLsizei count, const GLuint *samplers)
{
    for (int i = 0; i < count; i++)
    {
        GLuint sampler = samplers[i];

        if (mState.mSamplerManager->getSampler(sampler))
        {
            detachSampler(sampler);
        }

        mState.mSamplerManager->deleteObject(this, sampler);
    }
}

void Context::getInternalformativ(GLenum target,
                                  GLenum internalformat,
                                  GLenum pname,
                                  GLsizei bufSize,
                                  GLint *params)
{
    const TextureCaps &formatCaps = mState.mTextureCaps.get(internalformat);
    QueryInternalFormativ(formatCaps, pname, bufSize, params);
}

void Context::getInternalformativRobust(GLenum target,
                                        GLenum internalformat,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLint *params)
{
    getInternalformativ(target, internalformat, pname, bufSize, params);
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
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    setUniform1iImpl(programObject, location, count, value);
}

void Context::programUniform2iv(GLuint program, GLint location, GLsizei count, const GLint *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniform2iv(location, count, value);
}

void Context::programUniform3iv(GLuint program, GLint location, GLsizei count, const GLint *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniform3iv(location, count, value);
}

void Context::programUniform4iv(GLuint program, GLint location, GLsizei count, const GLint *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniform4iv(location, count, value);
}

void Context::programUniform1uiv(GLuint program, GLint location, GLsizei count, const GLuint *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniform1uiv(location, count, value);
}

void Context::programUniform2uiv(GLuint program, GLint location, GLsizei count, const GLuint *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniform2uiv(location, count, value);
}

void Context::programUniform3uiv(GLuint program, GLint location, GLsizei count, const GLuint *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniform3uiv(location, count, value);
}

void Context::programUniform4uiv(GLuint program, GLint location, GLsizei count, const GLuint *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniform4uiv(location, count, value);
}

void Context::programUniform1fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniform1fv(location, count, value);
}

void Context::programUniform2fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniform2fv(location, count, value);
}

void Context::programUniform3fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniform3fv(location, count, value);
}

void Context::programUniform4fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniform4fv(location, count, value);
}

void Context::programUniformMatrix2fv(GLuint program,
                                      GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniformMatrix2fv(location, count, transpose, value);
}

void Context::programUniformMatrix3fv(GLuint program,
                                      GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniformMatrix3fv(location, count, transpose, value);
}

void Context::programUniformMatrix4fv(GLuint program,
                                      GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniformMatrix4fv(location, count, transpose, value);
}

void Context::programUniformMatrix2x3fv(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniformMatrix2x3fv(location, count, transpose, value);
}

void Context::programUniformMatrix3x2fv(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniformMatrix3x2fv(location, count, transpose, value);
}

void Context::programUniformMatrix2x4fv(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniformMatrix2x4fv(location, count, transpose, value);
}

void Context::programUniformMatrix4x2fv(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniformMatrix4x2fv(location, count, transpose, value);
}

void Context::programUniformMatrix3x4fv(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniformMatrix3x4fv(location, count, transpose, value);
}

void Context::programUniformMatrix4x3fv(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniformMatrix4x3fv(location, count, transpose, value);
}

bool Context::isCurrentTransformFeedback(const TransformFeedback *tf) const
{
    return mState.isCurrentTransformFeedback(tf);
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

void Context::finishFenceNV(GLuint fence)
{
    FenceNV *fenceObject = getFenceNV(fence);

    ASSERT(fenceObject && fenceObject->isSet());
    ANGLE_CONTEXT_TRY(fenceObject->finish(this));
}

void Context::getFenceivNV(GLuint fence, GLenum pname, GLint *params)
{
    FenceNV *fenceObject = getFenceNV(fence);

    ASSERT(fenceObject && fenceObject->isSet());

    switch (pname)
    {
        case GL_FENCE_STATUS_NV:
        {
            // GL_NV_fence spec:
            // Once the status of a fence has been finished (via FinishFenceNV) or tested and
            // the returned status is TRUE (via either TestFenceNV or GetFenceivNV querying the
            // FENCE_STATUS_NV), the status remains TRUE until the next SetFenceNV of the fence.
            GLboolean status = GL_TRUE;
            if (fenceObject->getStatus() != GL_TRUE)
            {
                ANGLE_CONTEXT_TRY(fenceObject->test(this, &status));
            }
            *params = status;
            break;
        }

        case GL_FENCE_CONDITION_NV:
        {
            *params = static_cast<GLint>(fenceObject->getCondition());
            break;
        }

        default:
            UNREACHABLE();
    }
}

void Context::getTranslatedShaderSource(GLuint shader,
                                        GLsizei bufsize,
                                        GLsizei *length,
                                        GLchar *source)
{
    Shader *shaderObject = getShader(shader);
    ASSERT(shaderObject);
    shaderObject->getTranslatedSourceWithDebugInfo(bufsize, length, source);
}

void Context::getnUniformfv(GLuint program, GLint location, GLsizei bufSize, GLfloat *params)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);

    programObject->getUniformfv(this, location, params);
}

void Context::getnUniformfvRobust(GLuint program,
                                  GLint location,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLfloat *params)
{
    UNIMPLEMENTED();
}

void Context::getnUniformiv(GLuint program, GLint location, GLsizei bufSize, GLint *params)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);

    programObject->getUniformiv(this, location, params);
}

void Context::getnUniformivRobust(GLuint program,
                                  GLint location,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLint *params)
{
    UNIMPLEMENTED();
}

void Context::getnUniformuivRobust(GLuint program,
                                   GLint location,
                                   GLsizei bufSize,
                                   GLsizei *length,
                                   GLuint *params)
{
    UNIMPLEMENTED();
}

GLboolean Context::isFenceNV(GLuint fence)
{
    FenceNV *fenceObject = getFenceNV(fence);

    if (fenceObject == nullptr)
    {
        return GL_FALSE;
    }

    // GL_NV_fence spec:
    // A name returned by GenFencesNV, but not yet set via SetFenceNV, is not the name of an
    // existing fence.
    return fenceObject->isSet();
}

void Context::readnPixels(GLint x,
                          GLint y,
                          GLsizei width,
                          GLsizei height,
                          GLenum format,
                          GLenum type,
                          GLsizei bufSize,
                          void *data)
{
    return readPixels(x, y, width, height, format, type, data);
}

void Context::setFenceNV(GLuint fence, GLenum condition)
{
    ASSERT(condition == GL_ALL_COMPLETED_NV);

    FenceNV *fenceObject = getFenceNV(fence);
    ASSERT(fenceObject != nullptr);
    ANGLE_CONTEXT_TRY(fenceObject->set(this, condition));
}

GLboolean Context::testFenceNV(GLuint fence)
{
    FenceNV *fenceObject = getFenceNV(fence);

    ASSERT(fenceObject != nullptr);
    ASSERT(fenceObject->isSet() == GL_TRUE);

    GLboolean result = GL_TRUE;
    if (fenceObject->test(this, &result) == angle::Result::Stop)
    {
        return GL_TRUE;
    }

    return result;
}

void Context::deleteMemoryObjects(GLsizei n, const GLuint *memoryObjects)
{
    for (int i = 0; i < n; i++)
    {
        deleteMemoryObject(memoryObjects[i]);
    }
}

GLboolean Context::isMemoryObject(GLuint memoryObject)
{
    if (memoryObject == 0)
    {
        return GL_FALSE;
    }

    return (getMemoryObject(memoryObject) ? GL_TRUE : GL_FALSE);
}

void Context::createMemoryObjects(GLsizei n, GLuint *memoryObjects)
{
    for (int i = 0; i < n; i++)
    {
        memoryObjects[i] = createMemoryObject();
    }
}

void Context::memoryObjectParameteriv(GLuint memoryObject, GLenum pname, const GLint *params)
{
    UNIMPLEMENTED();
}

void Context::getMemoryObjectParameteriv(GLuint memoryObject, GLenum pname, GLint *params)
{
    UNIMPLEMENTED();
}

void Context::texStorageMem2D(TextureType target,
                              GLsizei levels,
                              GLenum internalFormat,
                              GLsizei width,
                              GLsizei height,
                              GLuint memory,
                              GLuint64 offset)
{
    MemoryObject *memoryObject = getMemoryObject(memory);
    ASSERT(memoryObject);
    Extents size(width, height, 1);
    Texture *texture = getTextureByType(target);
    ANGLE_CONTEXT_TRY(texture->setStorageExternalMemory(this, target, levels, internalFormat, size,
                                                        memoryObject, offset));
}

void Context::texStorageMem2DMultisample(TextureType target,
                                         GLsizei samples,
                                         GLenum internalFormat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLboolean fixedSampleLocations,
                                         GLuint memory,
                                         GLuint64 offset)
{
    UNIMPLEMENTED();
}

void Context::texStorageMem3D(TextureType target,
                              GLsizei levels,
                              GLenum internalFormat,
                              GLsizei width,
                              GLsizei height,
                              GLsizei depth,
                              GLuint memory,
                              GLuint64 offset)
{
    UNIMPLEMENTED();
}

void Context::texStorageMem3DMultisample(TextureType target,
                                         GLsizei samples,
                                         GLenum internalFormat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth,
                                         GLboolean fixedSampleLocations,
                                         GLuint memory,
                                         GLuint64 offset)
{
    UNIMPLEMENTED();
}

void Context::bufferStorageMem(TextureType target, GLsizeiptr size, GLuint memory, GLuint64 offset)
{
    UNIMPLEMENTED();
}

void Context::importMemoryFd(GLuint memory, GLuint64 size, HandleType handleType, GLint fd)
{
    MemoryObject *memoryObject = getMemoryObject(memory);
    ASSERT(memoryObject != nullptr);
    ANGLE_CONTEXT_TRY(memoryObject->importFd(this, size, handleType, fd));
}

void Context::genSemaphores(GLsizei n, GLuint *semaphores)
{
    UNIMPLEMENTED();
}

void Context::deleteSemaphores(GLsizei n, const GLuint *semaphores)
{
    UNIMPLEMENTED();
}

GLboolean Context::isSemaphore(GLuint semaphore)
{
    UNIMPLEMENTED();
    return GL_FALSE;
}

void Context::semaphoreParameterui64v(GLuint semaphore, GLenum pname, const GLuint64 *params)
{
    UNIMPLEMENTED();
}

void Context::getSemaphoreParameterui64v(GLuint semaphore, GLenum pname, GLuint64 *params)
{
    UNIMPLEMENTED();
}

void Context::waitSemaphore(GLuint semaphore,
                            GLuint numBufferBarriers,
                            const GLuint *buffers,
                            GLuint numTextureBarriers,
                            const GLuint *textures,
                            const GLenum *srcLayouts)
{
    UNIMPLEMENTED();
}

void Context::signalSemaphore(GLuint semaphore,
                              GLuint numBufferBarriers,
                              const GLuint *buffers,
                              GLuint numTextureBarriers,
                              const GLuint *textures,
                              const GLenum *dstLayouts)
{
    UNIMPLEMENTED();
}

void Context::importSemaphoreFd(GLuint semaphore, HandleType handleType, GLint fd)
{
    UNIMPLEMENTED();
}

void Context::eGLImageTargetTexture2D(TextureType target, GLeglImageOES image)
{
    Texture *texture        = getTextureByType(target);
    egl::Image *imageObject = static_cast<egl::Image *>(image);
    ANGLE_CONTEXT_TRY(texture->setEGLImageTarget(this, target, imageObject));
}

void Context::eGLImageTargetRenderbufferStorage(GLenum target, GLeglImageOES image)
{
    Renderbuffer *renderbuffer = mState.getCurrentRenderbuffer();
    egl::Image *imageObject    = static_cast<egl::Image *>(image);
    ANGLE_CONTEXT_TRY(renderbuffer->setStorageEGLImageTarget(this, imageObject));
}

void Context::texStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width)
{
    UNIMPLEMENTED();
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
        case GL_COMPRESSED_TEXTURE_FORMATS:
        {
            *type      = GL_INT;
            *numParams = static_cast<unsigned int>(getCaps().compressedTextureFormats.size());
            return true;
        }
        case GL_SHADER_BINARY_FORMATS:
        {
            *type      = GL_INT;
            *numParams = static_cast<unsigned int>(getCaps().shaderBinaryFormats.size());
            return true;
        }

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
        case GL_FRAMEBUFFER_BINDING:  // GL_FRAMEBUFFER_BINDING now equivalent to
                                      // GL_DRAW_FRAMEBUFFER_BINDING_ANGLE
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
        case GL_RESET_NOTIFICATION_STRATEGY_EXT:
        {
            *type      = GL_INT;
            *numParams = 1;
            return true;
        }
        case GL_PACK_REVERSE_ROW_ORDER_ANGLE:
        {
            if (!getExtensions().packReverseRowOrder)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;
        }
        case GL_MAX_RECTANGLE_TEXTURE_SIZE_ANGLE:
        case GL_TEXTURE_BINDING_RECTANGLE_ANGLE:
        {
            if (!getExtensions().textureRectangle)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;
        }
        case GL_MAX_DRAW_BUFFERS_EXT:
        case GL_MAX_COLOR_ATTACHMENTS_EXT:
        {
            if ((getClientMajorVersion() < 3) && !getExtensions().drawBuffers)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;
        }
        case GL_MAX_VIEWPORT_DIMS:
        {
            *type      = GL_INT;
            *numParams = 2;
            return true;
        }
        case GL_VIEWPORT:
        case GL_SCISSOR_BOX:
        {
            *type      = GL_INT;
            *numParams = 4;
            return true;
        }
        case GL_SHADER_COMPILER:
        case GL_SAMPLE_COVERAGE_INVERT:
        case GL_DEPTH_WRITEMASK:
        case GL_CULL_FACE:                 // CULL_FACE through DITHER are natural to IsEnabled,
        case GL_POLYGON_OFFSET_FILL:       // but can be retrieved through the Get{Type}v queries.
        case GL_SAMPLE_ALPHA_TO_COVERAGE:  // For this purpose, they are treated here as
                                           // bool-natural
        case GL_SAMPLE_COVERAGE:
        case GL_SCISSOR_TEST:
        case GL_STENCIL_TEST:
        case GL_DEPTH_TEST:
        case GL_BLEND:
        case GL_DITHER:
        case GL_CONTEXT_ROBUST_ACCESS_EXT:
        {
            *type      = GL_BOOL;
            *numParams = 1;
            return true;
        }
        case GL_COLOR_WRITEMASK:
        {
            *type      = GL_BOOL;
            *numParams = 4;
            return true;
        }
        case GL_POLYGON_OFFSET_FACTOR:
        case GL_POLYGON_OFFSET_UNITS:
        case GL_SAMPLE_COVERAGE_VALUE:
        case GL_DEPTH_CLEAR_VALUE:
        case GL_LINE_WIDTH:
        {
            *type      = GL_FLOAT;
            *numParams = 1;
            return true;
        }
        case GL_ALIASED_LINE_WIDTH_RANGE:
        case GL_ALIASED_POINT_SIZE_RANGE:
        case GL_DEPTH_RANGE:
        {
            *type      = GL_FLOAT;
            *numParams = 2;
            return true;
        }
        case GL_COLOR_CLEAR_VALUE:
        case GL_BLEND_COLOR:
        {
            *type      = GL_FLOAT;
            *numParams = 4;
            return true;
        }
        case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT:
            if (!getExtensions().textureFilterAnisotropic)
            {
                return false;
            }
            *type      = GL_FLOAT;
            *numParams = 1;
            return true;
        case GL_TIMESTAMP_EXT:
            if (!getExtensions().disjointTimerQuery)
            {
                return false;
            }
            *type      = GL_INT_64_ANGLEX;
            *numParams = 1;
            return true;
        case GL_GPU_DISJOINT_EXT:
            if (!getExtensions().disjointTimerQuery)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;
        case GL_COVERAGE_MODULATION_CHROMIUM:
            if (!getExtensions().framebufferMixedSamples)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;
        case GL_TEXTURE_BINDING_EXTERNAL_OES:
            if (!getExtensions().eglStreamConsumerExternal && !getExtensions().eglImageExternal)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;
    }

    if (getExtensions().debug)
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

    if (getExtensions().multisampleCompatibility)
    {
        switch (pname)
        {
            case GL_MULTISAMPLE_EXT:
            case GL_SAMPLE_ALPHA_TO_ONE_EXT:
                *type      = GL_BOOL;
                *numParams = 1;
                return true;
        }
    }

    if (getExtensions().pathRendering)
    {
        switch (pname)
        {
            case GL_PATH_MODELVIEW_MATRIX_CHROMIUM:
            case GL_PATH_PROJECTION_MATRIX_CHROMIUM:
                *type      = GL_FLOAT;
                *numParams = 16;
                return true;
        }
    }

    if (getExtensions().bindGeneratesResource)
    {
        switch (pname)
        {
            case GL_BIND_GENERATES_RESOURCE_CHROMIUM:
                *type      = GL_BOOL;
                *numParams = 1;
                return true;
        }
    }

    if (getExtensions().clientArrays)
    {
        switch (pname)
        {
            case GL_CLIENT_ARRAYS_ANGLE:
                *type      = GL_BOOL;
                *numParams = 1;
                return true;
        }
    }

    if (getExtensions().sRGBWriteControl)
    {
        switch (pname)
        {
            case GL_FRAMEBUFFER_SRGB_EXT:
                *type      = GL_BOOL;
                *numParams = 1;
                return true;
        }
    }

    if (getExtensions().robustResourceInitialization &&
        pname == GL_ROBUST_RESOURCE_INITIALIZATION_ANGLE)
    {
        *type      = GL_BOOL;
        *numParams = 1;
        return true;
    }

    if (getExtensions().programCacheControl && pname == GL_PROGRAM_CACHE_ENABLED_ANGLE)
    {
        *type      = GL_BOOL;
        *numParams = 1;
        return true;
    }

    if (getExtensions().parallelShaderCompile && pname == GL_MAX_SHADER_COMPILER_THREADS_KHR)
    {
        *type      = GL_INT;
        *numParams = 1;
        return true;
    }

    if (getExtensions().blendFuncExtended && pname == GL_MAX_DUAL_SOURCE_DRAW_BUFFERS_EXT)
    {
        *type      = GL_INT;
        *numParams = 1;
        return true;
    }

    // Check for ES3.0+ parameter names which are also exposed as ES2 extensions
    switch (pname)
    {
        // GL_DRAW_FRAMEBUFFER_BINDING_ANGLE equivalent to FRAMEBUFFER_BINDING
        case GL_READ_FRAMEBUFFER_BINDING_ANGLE:
            if ((getClientMajorVersion() < 3) && !getExtensions().framebufferBlit)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;

        case GL_NUM_PROGRAM_BINARY_FORMATS_OES:
            if ((getClientMajorVersion() < 3) && !getExtensions().getProgramBinary)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;

        case GL_PROGRAM_BINARY_FORMATS_OES:
            if ((getClientMajorVersion() < 3) && !getExtensions().getProgramBinary)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = static_cast<unsigned int>(getCaps().programBinaryFormats.size());
            return true;

        case GL_PACK_ROW_LENGTH:
        case GL_PACK_SKIP_ROWS:
        case GL_PACK_SKIP_PIXELS:
            if ((getClientMajorVersion() < 3) && !getExtensions().packSubimage)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;
        case GL_UNPACK_ROW_LENGTH:
        case GL_UNPACK_SKIP_ROWS:
        case GL_UNPACK_SKIP_PIXELS:
            if ((getClientMajorVersion() < 3) && !getExtensions().unpackSubimage)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;
        case GL_VERTEX_ARRAY_BINDING:
            if ((getClientMajorVersion() < 3) && !getExtensions().vertexArrayObject)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;
        case GL_PIXEL_PACK_BUFFER_BINDING:
        case GL_PIXEL_UNPACK_BUFFER_BINDING:
            if ((getClientMajorVersion() < 3) && !getExtensions().pixelBufferObject)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;
        case GL_MAX_SAMPLES:
        {
            static_assert(GL_MAX_SAMPLES_ANGLE == GL_MAX_SAMPLES,
                          "GL_MAX_SAMPLES_ANGLE not equal to GL_MAX_SAMPLES");
            if ((getClientMajorVersion() < 3) && !getExtensions().framebufferMultisample)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;

            case GL_FRAGMENT_SHADER_DERIVATIVE_HINT:
                if ((getClientMajorVersion() < 3) && !getExtensions().standardDerivatives)
                {
                    return false;
                }
                *type      = GL_INT;
                *numParams = 1;
                return true;
        }
    }

    if (pname >= GL_DRAW_BUFFER0_EXT && pname <= GL_DRAW_BUFFER15_EXT)
    {
        if ((getClientVersion() < Version(3, 0)) && !getExtensions().drawBuffers)
        {
            return false;
        }
        *type      = GL_INT;
        *numParams = 1;
        return true;
    }

    if (getExtensions().multiview2 && pname == GL_MAX_VIEWS_OVR)
    {
        *type      = GL_INT;
        *numParams = 1;
        return true;
    }

    if (getClientVersion() < Version(2, 0))
    {
        switch (pname)
        {
            case GL_ALPHA_TEST_FUNC:
            case GL_CLIENT_ACTIVE_TEXTURE:
            case GL_MATRIX_MODE:
            case GL_MAX_TEXTURE_UNITS:
            case GL_MAX_MODELVIEW_STACK_DEPTH:
            case GL_MAX_PROJECTION_STACK_DEPTH:
            case GL_MAX_TEXTURE_STACK_DEPTH:
            case GL_MAX_LIGHTS:
            case GL_MAX_CLIP_PLANES:
            case GL_VERTEX_ARRAY_STRIDE:
            case GL_NORMAL_ARRAY_STRIDE:
            case GL_COLOR_ARRAY_STRIDE:
            case GL_TEXTURE_COORD_ARRAY_STRIDE:
            case GL_VERTEX_ARRAY_SIZE:
            case GL_COLOR_ARRAY_SIZE:
            case GL_TEXTURE_COORD_ARRAY_SIZE:
            case GL_VERTEX_ARRAY_TYPE:
            case GL_NORMAL_ARRAY_TYPE:
            case GL_COLOR_ARRAY_TYPE:
            case GL_TEXTURE_COORD_ARRAY_TYPE:
            case GL_VERTEX_ARRAY_BUFFER_BINDING:
            case GL_NORMAL_ARRAY_BUFFER_BINDING:
            case GL_COLOR_ARRAY_BUFFER_BINDING:
            case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING:
            case GL_POINT_SIZE_ARRAY_STRIDE_OES:
            case GL_POINT_SIZE_ARRAY_TYPE_OES:
            case GL_POINT_SIZE_ARRAY_BUFFER_BINDING_OES:
            case GL_SHADE_MODEL:
            case GL_MODELVIEW_STACK_DEPTH:
            case GL_PROJECTION_STACK_DEPTH:
            case GL_TEXTURE_STACK_DEPTH:
            case GL_LOGIC_OP_MODE:
            case GL_BLEND_SRC:
            case GL_BLEND_DST:
            case GL_PERSPECTIVE_CORRECTION_HINT:
            case GL_POINT_SMOOTH_HINT:
            case GL_LINE_SMOOTH_HINT:
            case GL_FOG_HINT:
                *type      = GL_INT;
                *numParams = 1;
                return true;
            case GL_ALPHA_TEST_REF:
            case GL_FOG_DENSITY:
            case GL_FOG_START:
            case GL_FOG_END:
            case GL_FOG_MODE:
            case GL_POINT_SIZE:
            case GL_POINT_SIZE_MIN:
            case GL_POINT_SIZE_MAX:
            case GL_POINT_FADE_THRESHOLD_SIZE:
                *type      = GL_FLOAT;
                *numParams = 1;
                return true;
            case GL_SMOOTH_POINT_SIZE_RANGE:
            case GL_SMOOTH_LINE_WIDTH_RANGE:
                *type      = GL_FLOAT;
                *numParams = 2;
                return true;
            case GL_CURRENT_COLOR:
            case GL_CURRENT_TEXTURE_COORDS:
            case GL_LIGHT_MODEL_AMBIENT:
            case GL_FOG_COLOR:
                *type      = GL_FLOAT;
                *numParams = 4;
                return true;
            case GL_CURRENT_NORMAL:
            case GL_POINT_DISTANCE_ATTENUATION:
                *type      = GL_FLOAT;
                *numParams = 3;
                return true;
            case GL_MODELVIEW_MATRIX:
            case GL_PROJECTION_MATRIX:
            case GL_TEXTURE_MATRIX:
                *type      = GL_FLOAT;
                *numParams = 16;
                return true;
            case GL_LIGHT_MODEL_TWO_SIDE:
                *type      = GL_BOOL;
                *numParams = 1;
                return true;
        }
    }

    if (getClientVersion() < Version(3, 0))
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
            *type      = GL_INT;
            *numParams = 1;
            return true;
        }

        case GL_MAX_ELEMENT_INDEX:
        case GL_MAX_UNIFORM_BLOCK_SIZE:
        case GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS:
        case GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS:
        case GL_MAX_SERVER_WAIT_TIMEOUT:
        {
            *type      = GL_INT_64_ANGLEX;
            *numParams = 1;
            return true;
        }

        case GL_TRANSFORM_FEEDBACK_ACTIVE:
        case GL_TRANSFORM_FEEDBACK_PAUSED:
        case GL_PRIMITIVE_RESTART_FIXED_INDEX:
        case GL_RASTERIZER_DISCARD:
        {
            *type      = GL_BOOL;
            *numParams = 1;
            return true;
        }

        case GL_MAX_TEXTURE_LOD_BIAS:
        {
            *type      = GL_FLOAT;
            *numParams = 1;
            return true;
        }
    }

    if (getExtensions().requestExtension)
    {
        switch (pname)
        {
            case GL_NUM_REQUESTABLE_EXTENSIONS_ANGLE:
                *type      = GL_INT;
                *numParams = 1;
                return true;
        }
    }

    if (getExtensions().textureMultisample)
    {
        switch (pname)
        {
            case GL_MAX_COLOR_TEXTURE_SAMPLES_ANGLE:
            case GL_MAX_INTEGER_SAMPLES_ANGLE:
            case GL_MAX_DEPTH_TEXTURE_SAMPLES_ANGLE:
            case GL_TEXTURE_BINDING_2D_MULTISAMPLE_ANGLE:
            case GL_MAX_SAMPLE_MASK_WORDS:
                *type      = GL_INT;
                *numParams = 1;
                return true;
        }
    }

    if (getClientVersion() < Version(3, 1))
    {
        return false;
    }

    switch (pname)
    {
        case GL_ATOMIC_COUNTER_BUFFER_BINDING:
        case GL_DRAW_INDIRECT_BUFFER_BINDING:
        case GL_DISPATCH_INDIRECT_BUFFER_BINDING:
        case GL_MAX_FRAMEBUFFER_WIDTH:
        case GL_MAX_FRAMEBUFFER_HEIGHT:
        case GL_MAX_FRAMEBUFFER_SAMPLES:
        case GL_MAX_SAMPLE_MASK_WORDS:
        case GL_MAX_COLOR_TEXTURE_SAMPLES:
        case GL_MAX_DEPTH_TEXTURE_SAMPLES:
        case GL_MAX_INTEGER_SAMPLES:
        case GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET:
        case GL_MAX_VERTEX_ATTRIB_BINDINGS:
        case GL_MAX_VERTEX_ATTRIB_STRIDE:
        case GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS:
        case GL_MAX_VERTEX_ATOMIC_COUNTERS:
        case GL_MAX_VERTEX_IMAGE_UNIFORMS:
        case GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS:
        case GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS:
        case GL_MAX_FRAGMENT_ATOMIC_COUNTERS:
        case GL_MAX_FRAGMENT_IMAGE_UNIFORMS:
        case GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS:
        case GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET:
        case GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET:
        case GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS:
        case GL_MAX_COMPUTE_UNIFORM_BLOCKS:
        case GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS:
        case GL_MAX_COMPUTE_SHARED_MEMORY_SIZE:
        case GL_MAX_COMPUTE_UNIFORM_COMPONENTS:
        case GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS:
        case GL_MAX_COMPUTE_ATOMIC_COUNTERS:
        case GL_MAX_COMPUTE_IMAGE_UNIFORMS:
        case GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS:
        case GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS:
        case GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES:
        case GL_MAX_UNIFORM_LOCATIONS:
        case GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS:
        case GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE:
        case GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS:
        case GL_MAX_COMBINED_ATOMIC_COUNTERS:
        case GL_MAX_IMAGE_UNITS:
        case GL_MAX_COMBINED_IMAGE_UNIFORMS:
        case GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS:
        case GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS:
        case GL_SHADER_STORAGE_BUFFER_BINDING:
        case GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT:
        case GL_TEXTURE_BINDING_2D_MULTISAMPLE:
        case GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY:
            *type      = GL_INT;
            *numParams = 1;
            return true;
        case GL_MAX_SHADER_STORAGE_BLOCK_SIZE:
            *type      = GL_INT_64_ANGLEX;
            *numParams = 1;
            return true;
        case GL_SAMPLE_MASK:
            *type      = GL_BOOL;
            *numParams = 1;
            return true;
    }

    if (getExtensions().geometryShader)
    {
        switch (pname)
        {
            case GL_MAX_FRAMEBUFFER_LAYERS_EXT:
            case GL_LAYER_PROVOKING_VERTEX_EXT:
            case GL_MAX_GEOMETRY_UNIFORM_COMPONENTS_EXT:
            case GL_MAX_GEOMETRY_UNIFORM_BLOCKS_EXT:
            case GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS_EXT:
            case GL_MAX_GEOMETRY_INPUT_COMPONENTS_EXT:
            case GL_MAX_GEOMETRY_OUTPUT_COMPONENTS_EXT:
            case GL_MAX_GEOMETRY_OUTPUT_VERTICES_EXT:
            case GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS_EXT:
            case GL_MAX_GEOMETRY_SHADER_INVOCATIONS_EXT:
            case GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS_EXT:
            case GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS_EXT:
            case GL_MAX_GEOMETRY_ATOMIC_COUNTERS_EXT:
            case GL_MAX_GEOMETRY_IMAGE_UNIFORMS_EXT:
            case GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS_EXT:
                *type      = GL_INT;
                *numParams = 1;
                return true;
        }
    }

    return false;
}

bool Context::getIndexedQueryParameterInfo(GLenum target, GLenum *type, unsigned int *numParams)
{
    if (getClientVersion() < Version(3, 0))
    {
        return false;
    }

    switch (target)
    {
        case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
        case GL_UNIFORM_BUFFER_BINDING:
        {
            *type      = GL_INT;
            *numParams = 1;
            return true;
        }
        case GL_TRANSFORM_FEEDBACK_BUFFER_START:
        case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
        case GL_UNIFORM_BUFFER_START:
        case GL_UNIFORM_BUFFER_SIZE:
        {
            *type      = GL_INT_64_ANGLEX;
            *numParams = 1;
            return true;
        }
    }

    if (getClientVersion() < Version(3, 1))
    {
        return false;
    }

    switch (target)
    {
        case GL_IMAGE_BINDING_LAYERED:
        {
            *type      = GL_BOOL;
            *numParams = 1;
            return true;
        }
        case GL_MAX_COMPUTE_WORK_GROUP_COUNT:
        case GL_MAX_COMPUTE_WORK_GROUP_SIZE:
        case GL_ATOMIC_COUNTER_BUFFER_BINDING:
        case GL_SHADER_STORAGE_BUFFER_BINDING:
        case GL_VERTEX_BINDING_BUFFER:
        case GL_VERTEX_BINDING_DIVISOR:
        case GL_VERTEX_BINDING_OFFSET:
        case GL_VERTEX_BINDING_STRIDE:
        case GL_SAMPLE_MASK_VALUE:
        case GL_IMAGE_BINDING_NAME:
        case GL_IMAGE_BINDING_LEVEL:
        case GL_IMAGE_BINDING_LAYER:
        case GL_IMAGE_BINDING_ACCESS:
        case GL_IMAGE_BINDING_FORMAT:
        {
            *type      = GL_INT;
            *numParams = 1;
            return true;
        }
        case GL_ATOMIC_COUNTER_BUFFER_START:
        case GL_ATOMIC_COUNTER_BUFFER_SIZE:
        case GL_SHADER_STORAGE_BUFFER_START:
        case GL_SHADER_STORAGE_BUFFER_SIZE:
        {
            *type      = GL_INT_64_ANGLEX;
            *numParams = 1;
            return true;
        }
    }

    return false;
}

Program *Context::getProgramNoResolveLink(GLuint handle) const
{
    return mState.mShaderProgramManager->getProgram(handle);
}

Shader *Context::getShader(GLuint handle) const
{
    return mState.mShaderProgramManager->getShader(handle);
}

bool Context::isRenderbufferGenerated(GLuint renderbuffer) const
{
    return mState.mRenderbufferManager->isHandleGenerated(renderbuffer);
}

bool Context::isFramebufferGenerated(GLuint framebuffer) const
{
    return mState.mFramebufferManager->isHandleGenerated(framebuffer);
}

bool Context::isProgramPipelineGenerated(GLuint pipeline) const
{
    return mState.mProgramPipelineManager->isHandleGenerated(pipeline);
}

bool Context::usingDisplayTextureShareGroup() const
{
    return mDisplayTextureShareGroup;
}

GLenum Context::getConvertedRenderbufferFormat(GLenum internalformat) const
{
    return mState.mExtensions.webglCompatibility && mState.mClientVersion.major == 2 &&
                   internalformat == GL_DEPTH_STENCIL
               ? GL_DEPTH24_STENCIL8
               : internalformat;
}

void Context::maxShaderCompilerThreads(GLuint count)
{
    GLuint oldCount = mState.getMaxShaderCompilerThreads();
    mState.setMaxShaderCompilerThreads(count);
    // A count of zero specifies a request for no parallel compiling or linking.
    if ((oldCount == 0 || count == 0) && (oldCount != 0 || count != 0))
    {
        mThreadPool = angle::WorkerThreadPool::Create(count > 0);
    }
    mThreadPool->setMaxThreads(count);
    mImplementation->setMaxShaderCompilerThreads(count);
}

bool Context::isGLES1() const
{
    return mState.getClientVersion() < Version(2, 0);
}

void Context::onSubjectStateChange(const Context *context,
                                   angle::SubjectIndex index,
                                   angle::SubjectMessage message)
{
    switch (index)
    {
        case kVertexArraySubjectIndex:
            switch (message)
            {
                case angle::SubjectMessage::CONTENTS_CHANGED:
                    mState.setObjectDirty(GL_VERTEX_ARRAY);
                    mStateCache.onVertexArrayBufferContentsChange(this);
                    break;
                case angle::SubjectMessage::RESOURCE_MAPPED:
                case angle::SubjectMessage::RESOURCE_UNMAPPED:
                case angle::SubjectMessage::BINDING_CHANGED:
                    mStateCache.onVertexArrayBufferStateChange(this);
                    break;
                default:
                    break;
            }
            break;

        case kReadFramebufferSubjectIndex:
            if (message == angle::SubjectMessage::STORAGE_CHANGED)
            {
                mState.setObjectDirty(GL_READ_FRAMEBUFFER);
            }
            break;

        case kDrawFramebufferSubjectIndex:
            if (message == angle::SubjectMessage::STORAGE_CHANGED)
            {
                mState.setDrawFramebufferDirty();
            }
            mStateCache.onDrawFramebufferChange(this);
            break;

        default:
            if (index < kTextureMaxSubjectIndex)
            {
                mState.onActiveTextureStateChange(this, index);
                mStateCache.onActiveTextureChange(this);
            }
            else if (index < kUniformBufferMaxSubjectIndex)
            {
                mState.onUniformBufferStateChange(index - kUniformBuffer0SubjectIndex);
                mStateCache.onUniformBufferStateChange(this);
            }
            else
            {
                ASSERT(index < kSamplerMaxSubjectIndex);
                mState.setSamplerDirty(index - kSampler0SubjectIndex);
                mState.onActiveTextureStateChange(this, index - kSampler0SubjectIndex);
            }
            break;
    }
}

angle::Result Context::onProgramLink(Program *programObject)
{
    // Don't parallel link a program which is active in any GL contexts. With this assumption, we
    // don't need to worry that:
    //   1. Draw calls after link use the new executable code or the old one depending on the link
    //      result.
    //   2. When a backend program, e.g., ProgramD3D is linking, other backend classes like
    //      StateManager11, Renderer11, etc., may have a chance to make unexpected calls to
    //      ProgramD3D.
    if (programObject->isInUse())
    {
        programObject->resolveLink(this);
        if (programObject->isLinked())
        {
            ANGLE_TRY(mState.onProgramExecutableChange(this, programObject));
        }
        mStateCache.onProgramExecutableChange(this);
    }

    return angle::Result::Continue;
}

egl::Error Context::setDefaultFramebuffer(egl::Surface *surface)
{
    ASSERT(mCurrentSurface == nullptr);

    Framebuffer *newDefault = nullptr;
    if (surface != nullptr)
    {
        ANGLE_TRY(surface->setIsCurrent(this, true));
        mCurrentSurface = surface;
        newDefault      = surface->createDefaultFramebuffer(this);
    }
    else
    {
        newDefault = new Framebuffer(mImplementation.get());
    }

    // Update default framebuffer, the binding of the previous default
    // framebuffer (or lack of) will have a nullptr.
    {
        mState.mFramebufferManager->setDefaultFramebuffer(newDefault);
        if (mState.getReadFramebuffer() == nullptr)
        {
            bindReadFramebuffer(0);
        }
        if (mState.getDrawFramebuffer() == nullptr)
        {
            bindDrawFramebuffer(0);
        }
    }

    return egl::NoError();
}

egl::Error Context::unsetDefaultFramebuffer()
{
    gl::Framebuffer *defaultFramebuffer = mState.mFramebufferManager->getFramebuffer(0);

    // Remove the default framebuffer
    if (mState.getReadFramebuffer() == defaultFramebuffer)
    {
        mState.setReadFramebufferBinding(nullptr);
        mReadFramebufferObserverBinding.bind(nullptr);
    }

    if (mState.getDrawFramebuffer() == defaultFramebuffer)
    {
        mState.setDrawFramebufferBinding(nullptr);
        mDrawFramebufferObserverBinding.bind(nullptr);
    }

    if (defaultFramebuffer)
    {
        defaultFramebuffer->onDestroy(this);
        delete defaultFramebuffer;
    }

    mState.mFramebufferManager->setDefaultFramebuffer(nullptr);

    // Always unset the current surface, even if setIsCurrent fails.
    egl::Surface *surface = mCurrentSurface;
    mCurrentSurface       = nullptr;
    if (surface)
    {
        ANGLE_TRY(surface->setIsCurrent(this, false));
    }

    return egl::NoError();
}

// ErrorSet implementation.
ErrorSet::ErrorSet(Context *context) : mContext(context) {}

ErrorSet::~ErrorSet() = default;

void ErrorSet::handleError(GLenum errorCode,
                           const char *message,
                           const char *file,
                           const char *function,
                           unsigned int line)
{
    if (errorCode == GL_OUT_OF_MEMORY && mContext->getWorkarounds().loseContextOnOutOfMemory)
    {
        mContext->markContextLost(GraphicsResetStatus::UnknownContextReset);
    }

    std::stringstream errorStream;
    errorStream << "Error: " << gl::FmtHex(errorCode) << ", in " << file << ", " << function << ":"
                << line << ". " << message;

    std::string formattedMessage = errorStream.str();

    // Always log a warning, this function is only called on unexpected internal errors.
    WARN() << formattedMessage;

    // validationError does the necessary work to process the error.
    validationError(errorCode, formattedMessage.c_str());
}

void ErrorSet::validationError(GLenum errorCode, const char *message)
{
    ASSERT(errorCode != GL_NO_ERROR);
    mErrors.insert(errorCode);

    mContext->getState().getDebug().insertMessage(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR,
                                                  errorCode, GL_DEBUG_SEVERITY_HIGH, message);
}

bool ErrorSet::empty() const
{
    return mErrors.empty();
}

GLenum ErrorSet::popError()
{
    ASSERT(!empty());
    GLenum error = *mErrors.begin();
    mErrors.erase(mErrors.begin());
    return error;
}

// StateCache implementation.
StateCache::StateCache()
    : mCachedHasAnyEnabledClientAttrib(false),
      mCachedNonInstancedVertexElementLimit(0),
      mCachedInstancedVertexElementLimit(0),
      mCachedBasicDrawStatesError(kInvalidPointer),
      mCachedBasicDrawElementsError(kInvalidPointer),
      mCachedTransformFeedbackActiveUnpaused(false)
{}

StateCache::~StateCache() = default;

ANGLE_INLINE void StateCache::updateVertexElementLimits(Context *context)
{
    if (context->isBufferAccessValidationEnabled())
    {
        updateVertexElementLimitsImpl(context);
    }
}

void StateCache::initialize(Context *context)
{
    updateValidDrawModes(context);
    updateValidBindTextureTypes(context);
    updateValidDrawElementsTypes(context);
    updateBasicDrawStatesError();
    updateBasicDrawElementsError();
    updateVertexAttribTypesValidation(context);
}

void StateCache::updateActiveAttribsMask(Context *context)
{
    bool isGLES1         = context->isGLES1();
    const State &glState = context->getState();

    if (!isGLES1 && !glState.getProgram())
    {
        mCachedActiveBufferedAttribsMask = AttributesMask();
        mCachedActiveClientAttribsMask   = AttributesMask();
        mCachedActiveDefaultAttribsMask  = AttributesMask();
        return;
    }

    AttributesMask activeAttribs = isGLES1 ? glState.gles1().getVertexArraysAttributeMask()
                                           : glState.getProgram()->getActiveAttribLocationsMask();

    const VertexArray *vao = glState.getVertexArray();
    ASSERT(vao);

    const AttributesMask &clientAttribs  = vao->getClientAttribsMask();
    const AttributesMask &enabledAttribs = vao->getEnabledAttributesMask();
    const AttributesMask &activeEnabled  = activeAttribs & enabledAttribs;

    mCachedActiveClientAttribsMask   = activeEnabled & clientAttribs;
    mCachedActiveBufferedAttribsMask = activeEnabled & ~clientAttribs;
    mCachedActiveDefaultAttribsMask  = activeAttribs & ~enabledAttribs;
    mCachedHasAnyEnabledClientAttrib = (clientAttribs & enabledAttribs).any();
}

void StateCache::updateVertexElementLimitsImpl(Context *context)
{
    ASSERT(context->isBufferAccessValidationEnabled());

    const VertexArray *vao = context->getState().getVertexArray();

    mCachedNonInstancedVertexElementLimit = std::numeric_limits<GLint64>::max();
    mCachedInstancedVertexElementLimit    = std::numeric_limits<GLint64>::max();

    // VAO can be null on Context startup. If we make this computation lazier we could ASSERT.
    // If there are no buffered attributes then we should not limit the draw call count.
    if (!vao || !mCachedActiveBufferedAttribsMask.any())
    {
        return;
    }

    const auto &vertexAttribs  = vao->getVertexAttributes();
    const auto &vertexBindings = vao->getVertexBindings();

    for (size_t attributeIndex : mCachedActiveBufferedAttribsMask)
    {
        const VertexAttribute &attrib = vertexAttribs[attributeIndex];

        const VertexBinding &binding = vertexBindings[attrib.bindingIndex];
        ASSERT(context->isGLES1() ||
               context->getState().getProgram()->isAttribLocationActive(attributeIndex));

        GLint64 limit = attrib.getCachedElementLimit();
        if (binding.getDivisor() > 0)
        {
            mCachedInstancedVertexElementLimit =
                std::min(mCachedInstancedVertexElementLimit, limit);
        }
        else
        {
            mCachedNonInstancedVertexElementLimit =
                std::min(mCachedNonInstancedVertexElementLimit, limit);
        }
    }
}

void StateCache::updateBasicDrawStatesError()
{
    mCachedBasicDrawStatesError = kInvalidPointer;
}

void StateCache::updateBasicDrawElementsError()
{
    mCachedBasicDrawElementsError = kInvalidPointer;
}

intptr_t StateCache::getBasicDrawStatesErrorImpl(Context *context) const
{
    ASSERT(mCachedBasicDrawStatesError == kInvalidPointer);
    mCachedBasicDrawStatesError = reinterpret_cast<intptr_t>(ValidateDrawStates(context));
    return mCachedBasicDrawStatesError;
}

intptr_t StateCache::getBasicDrawElementsErrorImpl(Context *context) const
{
    ASSERT(mCachedBasicDrawElementsError == kInvalidPointer);
    mCachedBasicDrawElementsError = reinterpret_cast<intptr_t>(ValidateDrawElementsStates(context));
    return mCachedBasicDrawElementsError;
}

void StateCache::onVertexArrayBindingChange(Context *context)
{
    updateActiveAttribsMask(context);
    updateVertexElementLimits(context);
    updateBasicDrawStatesError();
}

void StateCache::onProgramExecutableChange(Context *context)
{
    updateActiveAttribsMask(context);
    updateVertexElementLimits(context);
    updateBasicDrawStatesError();
    updateValidDrawModes(context);
}

void StateCache::onVertexArrayFormatChange(Context *context)
{
    updateVertexElementLimits(context);
}

void StateCache::onVertexArrayBufferContentsChange(Context *context)
{
    updateVertexElementLimits(context);
    updateBasicDrawStatesError();
}

void StateCache::onVertexArrayStateChange(Context *context)
{
    updateActiveAttribsMask(context);
    updateVertexElementLimits(context);
    updateBasicDrawStatesError();
}

void StateCache::onVertexArrayBufferStateChange(Context *context)
{
    updateBasicDrawStatesError();
    updateBasicDrawElementsError();
}

void StateCache::onGLES1ClientStateChange(Context *context)
{
    updateActiveAttribsMask(context);
}

void StateCache::onDrawFramebufferChange(Context *context)
{
    updateBasicDrawStatesError();
}

void StateCache::onContextCapChange(Context *context)
{
    updateBasicDrawStatesError();
}

void StateCache::onStencilStateChange(Context *context)
{
    updateBasicDrawStatesError();
}

void StateCache::onDefaultVertexAttributeChange(Context *context)
{
    updateBasicDrawStatesError();
}

void StateCache::onActiveTextureChange(Context *context)
{
    updateBasicDrawStatesError();
}

void StateCache::onQueryChange(Context *context)
{
    updateBasicDrawStatesError();
}

void StateCache::onActiveTransformFeedbackChange(Context *context)
{
    updateTransformFeedbackActiveUnpaused(context);
    updateBasicDrawStatesError();
    updateBasicDrawElementsError();
    updateValidDrawModes(context);
}

void StateCache::onUniformBufferStateChange(Context *context)
{
    updateBasicDrawStatesError();
}

void StateCache::setValidDrawModes(bool pointsOK,
                                   bool linesOK,
                                   bool trisOK,
                                   bool lineAdjOK,
                                   bool triAdjOK)
{
    mCachedValidDrawModes[PrimitiveMode::Points]                 = pointsOK;
    mCachedValidDrawModes[PrimitiveMode::Lines]                  = linesOK;
    mCachedValidDrawModes[PrimitiveMode::LineStrip]              = linesOK;
    mCachedValidDrawModes[PrimitiveMode::LineLoop]               = linesOK;
    mCachedValidDrawModes[PrimitiveMode::Triangles]              = trisOK;
    mCachedValidDrawModes[PrimitiveMode::TriangleFan]            = trisOK;
    mCachedValidDrawModes[PrimitiveMode::TriangleStrip]          = trisOK;
    mCachedValidDrawModes[PrimitiveMode::LinesAdjacency]         = lineAdjOK;
    mCachedValidDrawModes[PrimitiveMode::LineStripAdjacency]     = lineAdjOK;
    mCachedValidDrawModes[PrimitiveMode::TrianglesAdjacency]     = triAdjOK;
    mCachedValidDrawModes[PrimitiveMode::TriangleStripAdjacency] = triAdjOK;
}

void StateCache::updateValidDrawModes(Context *context)
{
    const State &state = context->getState();
    Program *program   = state.getProgram();

    if (mCachedTransformFeedbackActiveUnpaused)
    {
        TransformFeedback *curTransformFeedback = state.getCurrentTransformFeedback();

        // ES Spec 3.0 validation text:
        // When transform feedback is active and not paused, all geometric primitives generated must
        // match the value of primitiveMode passed to BeginTransformFeedback. The error
        // INVALID_OPERATION is generated by DrawArrays and DrawArraysInstanced if mode is not
        // identical to primitiveMode. The error INVALID_OPERATION is also generated by
        // DrawElements, DrawElementsInstanced, and DrawRangeElements while transform feedback is
        // active and not paused, regardless of mode. Any primitive type may be used while transform
        // feedback is paused.
        if (!context->getExtensions().geometryShader)
        {
            mCachedValidDrawModes.fill(false);
            mCachedValidDrawModes[curTransformFeedback->getPrimitiveMode()] = true;
            return;
        }

        // EXT_geometry_shader validation text:
        // When transform feedback is active and not paused, all geometric primitives generated must
        // be compatible with the value of <primitiveMode> passed to BeginTransformFeedback. If a
        // geometry shader is active, the type of primitive emitted by that shader is used instead
        // of the <mode> parameter passed to drawing commands for the purposes of this error check.
        // Any primitive type may be used while transform feedback is paused.
        bool pointsOK = curTransformFeedback->getPrimitiveMode() == PrimitiveMode::Points;
        bool linesOK  = curTransformFeedback->getPrimitiveMode() == PrimitiveMode::Lines;
        bool trisOK   = curTransformFeedback->getPrimitiveMode() == PrimitiveMode::Triangles;

        setValidDrawModes(pointsOK, linesOK, trisOK, false, false);
        return;
    }

    if (!program || !program->hasLinkedShaderStage(ShaderType::Geometry))
    {
        mCachedValidDrawModes = kValidBasicDrawModes;
        return;
    }

    ASSERT(program && program->hasLinkedShaderStage(ShaderType::Geometry));
    PrimitiveMode gsMode = program->getGeometryShaderInputPrimitiveType();

    bool pointsOK  = gsMode == PrimitiveMode::Points;
    bool linesOK   = gsMode == PrimitiveMode::Lines;
    bool trisOK    = gsMode == PrimitiveMode::Triangles;
    bool lineAdjOK = gsMode == PrimitiveMode::LinesAdjacency;
    bool triAdjOK  = gsMode == PrimitiveMode::TrianglesAdjacency;

    setValidDrawModes(pointsOK, linesOK, trisOK, lineAdjOK, triAdjOK);
}

void StateCache::updateValidBindTextureTypes(Context *context)
{
    const Extensions &exts = context->getExtensions();
    bool isGLES3           = context->getClientMajorVersion() >= 3;
    bool isGLES31          = context->getClientVersion() >= Version(3, 1);

    mCachedValidBindTextureTypes = {{
        {TextureType::_2D, true},
        {TextureType::_2DArray, isGLES3},
        {TextureType::_2DMultisample, isGLES31 || exts.textureMultisample},
        {TextureType::_2DMultisampleArray, exts.textureStorageMultisample2DArray},
        {TextureType::_3D, isGLES3},
        {TextureType::External, exts.eglImageExternal || exts.eglStreamConsumerExternal},
        {TextureType::Rectangle, exts.textureRectangle},
        {TextureType::CubeMap, true},
    }};
}

void StateCache::updateValidDrawElementsTypes(Context *context)
{
    bool supportsUint =
        (context->getClientMajorVersion() >= 3 || context->getExtensions().elementIndexUint);

    mCachedValidDrawElementsTypes = {{
        {DrawElementsType::UnsignedByte, true},
        {DrawElementsType::UnsignedShort, true},
        {DrawElementsType::UnsignedInt, supportsUint},
    }};
}

void StateCache::updateTransformFeedbackActiveUnpaused(Context *context)
{
    TransformFeedback *xfb                 = context->getState().getCurrentTransformFeedback();
    mCachedTransformFeedbackActiveUnpaused = xfb && xfb->isActive() && !xfb->isPaused();
}

void StateCache::updateVertexAttribTypesValidation(Context *context)
{
    if (context->getClientMajorVersion() <= 2)
    {
        mCachedVertexAttribTypesValidation = {{
            {VertexAttribType::Byte, VertexAttribTypeCase::Valid},
            {VertexAttribType::Short, VertexAttribTypeCase::Valid},
            {VertexAttribType::UnsignedByte, VertexAttribTypeCase::Valid},
            {VertexAttribType::UnsignedShort, VertexAttribTypeCase::Valid},
            {VertexAttribType::Float, VertexAttribTypeCase::Valid},
            {VertexAttribType::Fixed, VertexAttribTypeCase::Valid},
        }};
    }
    else
    {
        mCachedVertexAttribTypesValidation = {{
            {VertexAttribType::Byte, VertexAttribTypeCase::Valid},
            {VertexAttribType::Short, VertexAttribTypeCase::Valid},
            {VertexAttribType::Int, VertexAttribTypeCase::Valid},
            {VertexAttribType::UnsignedByte, VertexAttribTypeCase::Valid},
            {VertexAttribType::UnsignedShort, VertexAttribTypeCase::Valid},
            {VertexAttribType::UnsignedInt, VertexAttribTypeCase::Valid},
            {VertexAttribType::Float, VertexAttribTypeCase::Valid},
            {VertexAttribType::HalfFloat, VertexAttribTypeCase::Valid},
            {VertexAttribType::Fixed, VertexAttribTypeCase::Valid},
            {VertexAttribType::Int2101010, VertexAttribTypeCase::ValidSize4Only},
            {VertexAttribType::UnsignedInt2101010, VertexAttribTypeCase::ValidSize4Only},
        }};

        mCachedIntegerVertexAttribTypesValidation = {{
            {VertexAttribType::Byte, VertexAttribTypeCase::Valid},
            {VertexAttribType::Short, VertexAttribTypeCase::Valid},
            {VertexAttribType::Int, VertexAttribTypeCase::Valid},
            {VertexAttribType::UnsignedByte, VertexAttribTypeCase::Valid},
            {VertexAttribType::UnsignedShort, VertexAttribTypeCase::Valid},
            {VertexAttribType::UnsignedInt, VertexAttribTypeCase::Valid},
        }};
    }
}
}  // namespace gl
