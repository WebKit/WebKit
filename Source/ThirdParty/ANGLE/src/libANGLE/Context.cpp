//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Context.cpp: Implements the gl::Context class, managing all GL state and performing
// rendering operations. It is the GLES2 specific implementation of EGLContext.
#include "libANGLE/Context.inl.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <iterator>
#include <sstream>
#include <vector>

#include "common/PackedEnums.h"
#include "common/angle_version_info.h"
#include "common/matrix_utils.h"
#include "common/platform.h"
#include "common/system_utils.h"
#include "common/utilities.h"
#include "image_util/loadimage.h"
#include "libANGLE/Buffer.h"
#include "libANGLE/Compiler.h"
#include "libANGLE/Display.h"
#include "libANGLE/Fence.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/MemoryObject.h"
#include "libANGLE/PixelLocalStorage.h"
#include "libANGLE/Program.h"
#include "libANGLE/ProgramPipeline.h"
#include "libANGLE/Query.h"
#include "libANGLE/Renderbuffer.h"
#include "libANGLE/ResourceManager.h"
#include "libANGLE/Sampler.h"
#include "libANGLE/Semaphore.h"
#include "libANGLE/Surface.h"
#include "libANGLE/Texture.h"
#include "libANGLE/TransformFeedback.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/capture/FrameCapture.h"
#include "libANGLE/capture/serialize.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/queryconversions.h"
#include "libANGLE/queryutils.h"
#include "libANGLE/renderer/DisplayImpl.h"
#include "libANGLE/renderer/Format.h"
#include "libANGLE/validationES.h"

#if defined(ANGLE_PLATFORM_APPLE)
#    include <dispatch/dispatch.h>
#    include "common/tls.h"
#endif

namespace gl
{
namespace
{

egl::ShareGroup *AllocateOrGetShareGroup(egl::Display *display, const gl::Context *shareContext)
{
    if (shareContext)
    {
        egl::ShareGroup *shareGroup = shareContext->getState().getShareGroup();
        shareGroup->addRef();
        return shareGroup;
    }
    else
    {
        return new egl::ShareGroup(display->getImplementation());
    }
}

template <typename T>
angle::Result GetQueryObjectParameter(const Context *context, Query *query, GLenum pname, T *params)
{
    if (!query)
    {
        // Some applications call into glGetQueryObjectuiv(...) prior to calling glBeginQuery(...)
        // This wouldn't be an issue since the validation layer will handle such a usecases but when
        // the app enables EGL_KHR_create_context_no_error extension, we skip the validation layer.
        switch (pname)
        {
            case GL_QUERY_RESULT_EXT:
                *params = 0;
                break;
            case GL_QUERY_RESULT_AVAILABLE_EXT:
                *params = GL_FALSE;
                break;
            default:
                UNREACHABLE();
                return angle::Result::Stop;
        }
        return angle::Result::Continue;
    }

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

bool GetBackwardCompatibleContext(const egl::AttributeMap &attribs)
{
    return attribs.get(EGL_CONTEXT_OPENGL_BACKWARDS_COMPATIBLE_ANGLE, EGL_TRUE) == EGL_TRUE;
}

bool GetWebGLContext(const egl::AttributeMap &attribs)
{
    return (attribs.get(EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE, EGL_FALSE) == EGL_TRUE);
}

Version GetClientVersion(egl::Display *display,
                         const egl::AttributeMap &attribs,
                         const EGLenum clientType)
{
    Version requestedVersion =
        Version(GetClientMajorVersion(attribs), GetClientMinorVersion(attribs));
    if (GetBackwardCompatibleContext(attribs))
    {
        if (clientType == EGL_OPENGL_API)
        {
            Optional<gl::Version> maxSupportedDesktopVersion =
                display->getImplementation()->getMaxSupportedDesktopVersion();
            if (maxSupportedDesktopVersion.valid())
                return std::max(maxSupportedDesktopVersion.value(), requestedVersion);
            else
                return requestedVersion;
        }
        else if (requestedVersion.major == 1)
        {
            // If the user requests an ES1 context, we cannot return an ES 2+ context.
            return Version(1, 1);
        }
        else
        {
            // Always up the version to at least the max conformant version this display supports.
            // Only return a higher client version if requested.
            const Version conformantVersion = std::max(
                display->getImplementation()->getMaxConformantESVersion(), requestedVersion);
            // Limit the WebGL context to at most version 3.1
            const bool isWebGL = GetWebGLContext(attribs);
            return isWebGL ? std::min(conformantVersion, Version(3, 1)) : conformantVersion;
        }
    }
    else
    {
        return requestedVersion;
    }
}

EGLint GetProfileMask(const egl::AttributeMap &attribs)
{
    return attribs.getAsInt(EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT);
}

GLenum GetResetStrategy(const egl::AttributeMap &attribs)
{
    EGLAttrib resetStrategyExt =
        attribs.get(EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT, EGL_NO_RESET_NOTIFICATION);
    EGLAttrib resetStrategyCore =
        attribs.get(EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY, resetStrategyExt);

    switch (resetStrategyCore)
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
    EGLAttrib robustAccessExt  = attribs.get(EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT, EGL_FALSE);
    EGLAttrib robustAccessCore = attribs.get(EGL_CONTEXT_OPENGL_ROBUST_ACCESS, robustAccessExt);

    bool attribRobustAccess = (robustAccessCore == EGL_TRUE);
    bool contextFlagsRobustAccess =
        ((attribs.get(EGL_CONTEXT_FLAGS_KHR, 0) & EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR) != 0);

    return (attribRobustAccess || contextFlagsRobustAccess);
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

bool GetRobustResourceInit(egl::Display *display, const egl::AttributeMap &attribs)
{
    const angle::FrontendFeatures &frontendFeatures = display->getFrontendFeatures();
    return (frontendFeatures.forceRobustResourceInit.enabled ||
            attribs.get(EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE, EGL_FALSE) == EGL_TRUE);
}

EGLenum GetContextPriority(const egl::AttributeMap &attribs)
{
    return static_cast<EGLenum>(
        attribs.getAsInt(EGL_CONTEXT_PRIORITY_LEVEL_IMG, EGL_CONTEXT_PRIORITY_MEDIUM_IMG));
}

bool GetProtectedContent(const egl::AttributeMap &attribs)
{
    return static_cast<bool>(attribs.getAsInt(EGL_PROTECTED_CONTENT_EXT, EGL_FALSE));
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

enum SubjectIndexes : angle::SubjectIndex
{
    kTexture0SubjectIndex       = 0,
    kTextureMaxSubjectIndex     = kTexture0SubjectIndex + IMPLEMENTATION_MAX_ACTIVE_TEXTURES,
    kImage0SubjectIndex         = kTextureMaxSubjectIndex,
    kImageMaxSubjectIndex       = kImage0SubjectIndex + IMPLEMENTATION_MAX_IMAGE_UNITS,
    kUniformBuffer0SubjectIndex = kImageMaxSubjectIndex,
    kUniformBufferMaxSubjectIndex =
        kUniformBuffer0SubjectIndex + IMPLEMENTATION_MAX_UNIFORM_BUFFER_BINDINGS,
    kAtomicCounterBuffer0SubjectIndex = kUniformBufferMaxSubjectIndex,
    kAtomicCounterBufferMaxSubjectIndex =
        kAtomicCounterBuffer0SubjectIndex + IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS,
    kShaderStorageBuffer0SubjectIndex = kAtomicCounterBufferMaxSubjectIndex,
    kShaderStorageBufferMaxSubjectIndex =
        kShaderStorageBuffer0SubjectIndex + IMPLEMENTATION_MAX_SHADER_STORAGE_BUFFER_BINDINGS,
    kSampler0SubjectIndex    = kShaderStorageBufferMaxSubjectIndex,
    kSamplerMaxSubjectIndex  = kSampler0SubjectIndex + IMPLEMENTATION_MAX_ACTIVE_TEXTURES,
    kVertexArraySubjectIndex = kSamplerMaxSubjectIndex,
    kReadFramebufferSubjectIndex,
    kDrawFramebufferSubjectIndex,
    kProgramPipelineSubjectIndex,
};

bool IsClearBufferEnabled(const FramebufferState &fbState, GLenum buffer, GLint drawbuffer)
{
    return buffer != GL_COLOR || fbState.getEnabledDrawBuffers()[drawbuffer];
}

bool IsEmptyScissor(const State &glState)
{
    if (!glState.isScissorTestEnabled())
    {
        return false;
    }

    const Extents &dimensions = glState.getDrawFramebuffer()->getExtents();
    Rectangle framebufferArea(0, 0, dimensions.width, dimensions.height);
    return !ClipRectangle(framebufferArea, glState.getScissor(), nullptr);
}

bool IsColorMaskedOut(const BlendStateExt &blendStateExt, const GLint drawbuffer)
{
    ASSERT(static_cast<size_t>(drawbuffer) < blendStateExt.getDrawBufferCount());
    return blendStateExt.getColorMaskIndexed(static_cast<size_t>(drawbuffer)) == 0;
}

bool GetIsExternal(const egl::AttributeMap &attribs)
{
    return (attribs.get(EGL_EXTERNAL_CONTEXT_ANGLE, EGL_FALSE) == EGL_TRUE);
}

bool GetSaveAndRestoreState(const egl::AttributeMap &attribs)
{
    return (attribs.get(EGL_EXTERNAL_CONTEXT_SAVE_STATE_ANGLE, EGL_FALSE) == EGL_TRUE);
}

void GetPerfMonitorString(const std::string &name,
                          GLsizei bufSize,
                          GLsizei *length,
                          GLchar *stringOut)
{
    GLsizei numCharsWritten = std::min(bufSize, static_cast<GLsizei>(name.size()));

    if (length)
    {
        if (bufSize == 0)
        {
            *length = static_cast<GLsizei>(name.size());
        }
        else
        {
            // Excludes null terminator.
            ASSERT(numCharsWritten > 0);
            *length = numCharsWritten - 1;
        }
    }

    if (stringOut)
    {
        memcpy(stringOut, name.c_str(), numCharsWritten);
    }
}

bool CanSupportAEP(const gl::Version &version, const gl::Extensions &extensions)
{
    // From the GL_ANDROID_extension_pack_es31a extension spec:
    // OpenGL ES 3.1 and GLSL ES 3.10 are required.
    // The following extensions are required:
    // * KHR_debug
    // * KHR_texture_compression_astc_ldr
    // * KHR_blend_equation_advanced
    // * OES_sample_shading
    // * OES_sample_variables
    // * OES_shader_image_atomic
    // * OES_shader_multisample_interpolation
    // * OES_texture_stencil8
    // * OES_texture_storage_multisample_2d_array
    // * EXT_copy_image
    // * EXT_draw_buffers_indexed
    // * EXT_geometry_shader
    // * EXT_gpu_shader5
    // * EXT_primitive_bounding_box
    // * EXT_shader_io_blocks
    // * EXT_tessellation_shader
    // * EXT_texture_border_clamp
    // * EXT_texture_buffer
    // * EXT_texture_cube_map_array
    // * EXT_texture_sRGB_decode
    return (version >= ES_3_1 && extensions.debugKHR && extensions.textureCompressionAstcLdrKHR &&
            extensions.blendEquationAdvancedKHR && extensions.sampleShadingOES &&
            extensions.sampleVariablesOES && extensions.shaderImageAtomicOES &&
            extensions.shaderMultisampleInterpolationOES && extensions.textureStencil8OES &&
            extensions.textureStorageMultisample2dArrayOES && extensions.copyImageEXT &&
            extensions.drawBuffersIndexedEXT && extensions.geometryShaderEXT &&
            extensions.gpuShader5EXT && extensions.primitiveBoundingBoxEXT &&
            extensions.shaderIoBlocksEXT && extensions.tessellationShaderEXT &&
            extensions.textureBorderClampEXT && extensions.textureBufferEXT &&
            extensions.textureCubeMapArrayEXT && extensions.textureSRGBDecodeEXT);
}
}  // anonymous namespace

#if defined(ANGLE_PLATFORM_APPLE)
// TODO(angleproject:6479): Due to a bug in Apple's dyld loader, `thread_local` will cause
// excessive memory use. Temporarily avoid it by using pthread's thread
// local storage instead.
static TLSIndex GetCurrentValidContextTLSIndex()
{
    static TLSIndex CurrentValidContextIndex = TLS_INVALID_INDEX;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
      ASSERT(CurrentValidContextIndex == TLS_INVALID_INDEX);
      CurrentValidContextIndex = CreateTLSIndex(nullptr);
    });
    return CurrentValidContextIndex;
}
Context *GetCurrentValidContextTLS()
{
    TLSIndex CurrentValidContextIndex = GetCurrentValidContextTLSIndex();
    ASSERT(CurrentValidContextIndex != TLS_INVALID_INDEX);
    return static_cast<Context *>(GetTLSValue(CurrentValidContextIndex));
}
void SetCurrentValidContextTLS(Context *context)
{
    TLSIndex CurrentValidContextIndex = GetCurrentValidContextTLSIndex();
    ASSERT(CurrentValidContextIndex != TLS_INVALID_INDEX);
    SetTLSValue(CurrentValidContextIndex, context);
}
#else
thread_local Context *gCurrentValidContext = nullptr;
#endif

Context::Context(egl::Display *display,
                 const egl::Config *config,
                 const Context *shareContext,
                 TextureManager *shareTextures,
                 SemaphoreManager *shareSemaphores,
                 MemoryProgramCache *memoryProgramCache,
                 MemoryShaderCache *memoryShaderCache,
                 const EGLenum clientType,
                 const egl::AttributeMap &attribs,
                 const egl::DisplayExtensions &displayExtensions,
                 const egl::ClientExtensions &clientExtensions)
    : mState(shareContext ? &shareContext->mState : nullptr,
             AllocateOrGetShareGroup(display, shareContext),
             shareTextures,
             shareSemaphores,
             &mOverlay,
             clientType,
             GetClientVersion(display, attribs, clientType),
             GetProfileMask(attribs),
             GetDebug(attribs),
             GetBindGeneratesResource(attribs),
             GetClientArraysEnabled(attribs),
             GetRobustResourceInit(display, attribs),
             memoryProgramCache != nullptr,
             GetContextPriority(attribs),
             GetRobustAccess(attribs),
             GetProtectedContent(attribs)),
      mShared(shareContext != nullptr || shareTextures != nullptr || shareSemaphores != nullptr),
      mSkipValidation(GetNoError(attribs)),
      mDisplayTextureShareGroup(shareTextures != nullptr),
      mDisplaySemaphoreShareGroup(shareSemaphores != nullptr),
      mErrors(this),
      mImplementation(display->getImplementation()
                          ->createContext(mState, &mErrors, config, shareContext, attribs)),
      mLabel(nullptr),
      mCompiler(),
      mConfig(config),
      mHasBeenCurrent(false),
      mContextLost(false),
      mResetStatus(GraphicsResetStatus::NoError),
      mContextLostForced(false),
      mResetStrategy(GetResetStrategy(attribs)),
      mSurfacelessSupported(displayExtensions.surfacelessContext),
      mCurrentDrawSurface(static_cast<egl::Surface *>(EGL_NO_SURFACE)),
      mCurrentReadSurface(static_cast<egl::Surface *>(EGL_NO_SURFACE)),
      mDisplay(display),
      mWebGLContext(GetWebGLContext(attribs)),
      mBufferAccessValidationEnabled(false),
      mExtensionsEnabled(GetExtensionsEnabled(attribs, mWebGLContext)),
      mMemoryProgramCache(memoryProgramCache),
      mMemoryShaderCache(memoryShaderCache),
      mVertexArrayObserverBinding(this, kVertexArraySubjectIndex),
      mDrawFramebufferObserverBinding(this, kDrawFramebufferSubjectIndex),
      mReadFramebufferObserverBinding(this, kReadFramebufferSubjectIndex),
      mProgramPipelineObserverBinding(this, kProgramPipelineSubjectIndex),
      mFrameCapture(new angle::FrameCapture),
      mRefCount(0),
      mOverlay(mImplementation.get()),
      mIsExternal(GetIsExternal(attribs)),
      mSaveAndRestoreState(GetSaveAndRestoreState(attribs)),
      mIsDestroyed(false)
{
    for (angle::SubjectIndex uboIndex = kUniformBuffer0SubjectIndex;
         uboIndex < kUniformBufferMaxSubjectIndex; ++uboIndex)
    {
        mUniformBufferObserverBindings.emplace_back(this, uboIndex);
    }

    for (angle::SubjectIndex acbIndex = kAtomicCounterBuffer0SubjectIndex;
         acbIndex < kAtomicCounterBufferMaxSubjectIndex; ++acbIndex)
    {
        mAtomicCounterBufferObserverBindings.emplace_back(this, acbIndex);
    }

    for (angle::SubjectIndex ssboIndex = kShaderStorageBuffer0SubjectIndex;
         ssboIndex < kShaderStorageBufferMaxSubjectIndex; ++ssboIndex)
    {
        mShaderStorageBufferObserverBindings.emplace_back(this, ssboIndex);
    }

    for (angle::SubjectIndex samplerIndex = kSampler0SubjectIndex;
         samplerIndex < kSamplerMaxSubjectIndex; ++samplerIndex)
    {
        mSamplerObserverBindings.emplace_back(this, samplerIndex);
    }

    for (angle::SubjectIndex imageIndex = kImage0SubjectIndex; imageIndex < kImageMaxSubjectIndex;
         ++imageIndex)
    {
        mImageObserverBindings.emplace_back(this, imageIndex);
    }

    // Implementations now require the display to be set at context creation.
    ASSERT(mDisplay);
}

egl::Error Context::initialize()
{
    if (!mImplementation)
        return egl::Error(EGL_NOT_INITIALIZED, "native context creation failed");
    return egl::NoError();
}

void Context::initializeDefaultResources()
{
    mImplementation->setMemoryProgramCache(mMemoryProgramCache);

    initCaps();

    mState.initialize(this);

    mDefaultFramebuffer = std::make_unique<Framebuffer>(this, mImplementation.get());

    mFenceNVHandleAllocator.setBaseHandle(0);

    // [OpenGL ES 2.0.24] section 3.7 page 83:
    // In the initial state, TEXTURE_2D and TEXTURE_CUBE_MAP have two-dimensional
    // and cube map texture state vectors respectively associated with them.
    // In order that access to these initial textures not be lost, they are treated as texture
    // objects all of whose names are 0.

    Texture *zeroTexture2D = new Texture(mImplementation.get(), {0}, TextureType::_2D);
    mZeroTextures[TextureType::_2D].set(this, zeroTexture2D);

    Texture *zeroTextureCube = new Texture(mImplementation.get(), {0}, TextureType::CubeMap);
    mZeroTextures[TextureType::CubeMap].set(this, zeroTextureCube);

    if (getClientVersion() >= Version(3, 0) || mSupportedExtensions.texture3DOES)
    {
        Texture *zeroTexture3D = new Texture(mImplementation.get(), {0}, TextureType::_3D);
        mZeroTextures[TextureType::_3D].set(this, zeroTexture3D);
    }
    if (getClientVersion() >= Version(3, 0))
    {
        Texture *zeroTexture2DArray =
            new Texture(mImplementation.get(), {0}, TextureType::_2DArray);
        mZeroTextures[TextureType::_2DArray].set(this, zeroTexture2DArray);
    }
    if (getClientVersion() >= Version(3, 1) || mSupportedExtensions.textureMultisampleANGLE)
    {
        Texture *zeroTexture2DMultisample =
            new Texture(mImplementation.get(), {0}, TextureType::_2DMultisample);
        mZeroTextures[TextureType::_2DMultisample].set(this, zeroTexture2DMultisample);
    }
    if (getClientVersion() >= Version(3, 1))
    {
        Texture *zeroTexture2DMultisampleArray =
            new Texture(mImplementation.get(), {0}, TextureType::_2DMultisampleArray);
        mZeroTextures[TextureType::_2DMultisampleArray].set(this, zeroTexture2DMultisampleArray);

        for (int i = 0; i < mState.mCaps.maxAtomicCounterBufferBindings; i++)
        {
            bindBufferRange(BufferBinding::AtomicCounter, i, {0}, 0, 0);
        }

        for (int i = 0; i < mState.mCaps.maxShaderStorageBufferBindings; i++)
        {
            bindBufferRange(BufferBinding::ShaderStorage, i, {0}, 0, 0);
        }
    }

    if ((getClientType() != EGL_OPENGL_API && getClientVersion() >= Version(3, 2)) ||
        mSupportedExtensions.textureCubeMapArrayAny())
    {
        Texture *zeroTextureCubeMapArray =
            new Texture(mImplementation.get(), {0}, TextureType::CubeMapArray);
        mZeroTextures[TextureType::CubeMapArray].set(this, zeroTextureCubeMapArray);
    }

    if ((getClientType() != EGL_OPENGL_API && getClientVersion() >= Version(3, 2)) ||
        mSupportedExtensions.textureBufferAny())
    {
        Texture *zeroTextureBuffer = new Texture(mImplementation.get(), {0}, TextureType::Buffer);
        mZeroTextures[TextureType::Buffer].set(this, zeroTextureBuffer);
    }

    if (mSupportedExtensions.textureRectangleANGLE)
    {
        Texture *zeroTextureRectangle =
            new Texture(mImplementation.get(), {0}, TextureType::Rectangle);
        mZeroTextures[TextureType::Rectangle].set(this, zeroTextureRectangle);
    }

    if (mSupportedExtensions.EGLImageExternalOES ||
        mSupportedExtensions.EGLStreamConsumerExternalNV)
    {
        Texture *zeroTextureExternal =
            new Texture(mImplementation.get(), {0}, TextureType::External);
        mZeroTextures[TextureType::External].set(this, zeroTextureExternal);
    }

    // This may change native TEXTURE_2D, TEXTURE_EXTERNAL_OES and TEXTURE_RECTANGLE,
    // binding states. Ensure state manager is aware of this when binding
    // this texture type.
    if (mSupportedExtensions.videoTextureWEBGL)
    {
        Texture *zeroTextureVideoImage =
            new Texture(mImplementation.get(), {0}, TextureType::VideoImage);
        mZeroTextures[TextureType::VideoImage].set(this, zeroTextureVideoImage);
    }

    mState.initializeZeroTextures(this, mZeroTextures);

    ANGLE_CONTEXT_TRY(mImplementation->initialize());

    // Add context into the share group
    mState.getShareGroup()->addSharedContext(this);

    bindVertexArray({0});

    if (getClientVersion() >= Version(3, 0))
    {
        // [OpenGL ES 3.0.2] section 2.14.1 pg 85:
        // In the initial state, a default transform feedback object is bound and treated as
        // a transform feedback object with a name of zero. That object is bound any time
        // BindTransformFeedback is called with id of zero
        bindTransformFeedback(GL_TRANSFORM_FEEDBACK, {0});
    }

    for (auto type : angle::AllEnums<BufferBinding>())
    {
        bindBuffer(type, {0});
    }

    bindRenderbuffer(GL_RENDERBUFFER, {0});

    for (int i = 0; i < mState.mCaps.maxUniformBufferBindings; i++)
    {
        bindBufferRange(BufferBinding::Uniform, i, {0}, 0, -1);
    }

    // Initialize GLES1 renderer if appropriate.
    if (getClientVersion() < Version(2, 0))
    {
        mGLES1Renderer.reset(new GLES1Renderer());
    }

    // Initialize dirty bit masks
    mAllDirtyBits.set();

    mDrawDirtyObjects.set(State::DIRTY_OBJECT_ACTIVE_TEXTURES);
    mDrawDirtyObjects.set(State::DIRTY_OBJECT_DRAW_FRAMEBUFFER);
    mDrawDirtyObjects.set(State::DIRTY_OBJECT_VERTEX_ARRAY);
    mDrawDirtyObjects.set(State::DIRTY_OBJECT_TEXTURES);
    mDrawDirtyObjects.set(State::DIRTY_OBJECT_PROGRAM);
    mDrawDirtyObjects.set(State::DIRTY_OBJECT_PROGRAM_PIPELINE_OBJECT);
    mDrawDirtyObjects.set(State::DIRTY_OBJECT_SAMPLERS);
    mDrawDirtyObjects.set(State::DIRTY_OBJECT_IMAGES);

    mTexImageDirtyBits.set(State::DIRTY_BIT_UNPACK_STATE);
    mTexImageDirtyBits.set(State::DIRTY_BIT_UNPACK_BUFFER_BINDING);
    mTexImageDirtyBits.set(State::DIRTY_BIT_EXTENDED);
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

    // We sync the draw Framebuffer manually in prepareForClear to allow the clear calls to do
    // more custom handling for robust resource init.

    mBlitDirtyBits.set(State::DIRTY_BIT_SCISSOR_TEST_ENABLED);
    mBlitDirtyBits.set(State::DIRTY_BIT_SCISSOR);
    mBlitDirtyBits.set(State::DIRTY_BIT_FRAMEBUFFER_SRGB_WRITE_CONTROL_MODE);
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
    mComputeDirtyObjects.set(State::DIRTY_OBJECT_ACTIVE_TEXTURES);
    mComputeDirtyObjects.set(State::DIRTY_OBJECT_TEXTURES);
    mComputeDirtyObjects.set(State::DIRTY_OBJECT_PROGRAM);
    mComputeDirtyObjects.set(State::DIRTY_OBJECT_PROGRAM_PIPELINE_OBJECT);
    mComputeDirtyObjects.set(State::DIRTY_OBJECT_IMAGES);
    mComputeDirtyObjects.set(State::DIRTY_OBJECT_SAMPLERS);

    mCopyImageDirtyBits.set(State::DIRTY_BIT_READ_FRAMEBUFFER_BINDING);
    mCopyImageDirtyObjects.set(State::DIRTY_OBJECT_READ_FRAMEBUFFER);

    mReadInvalidateDirtyBits.set(State::DIRTY_BIT_READ_FRAMEBUFFER_BINDING);
    mDrawInvalidateDirtyBits.set(State::DIRTY_BIT_DRAW_FRAMEBUFFER_BINDING);

    // The implementation's internal load/store programs for EXT_shader_pixel_pixel_local_storage
    // only need the draw framebuffer to be synced. The remaining state is managed internally.
    mPixelLocalStorageEXTEnableDisableDirtyBits.set(State::DIRTY_BIT_DRAW_FRAMEBUFFER_BINDING);
    mPixelLocalStorageEXTEnableDisableDirtyObjects.set(State::DIRTY_OBJECT_DRAW_FRAMEBUFFER);

    mOverlay.init();
}

egl::Error Context::onDestroy(const egl::Display *display)
{
    if (!mHasBeenCurrent)
    {
        // The context is never current, so default resources are not allocated.
        return egl::NoError();
    }

    // eglDestoryContext() must have been called for this Context and there must not be any Threads
    // that still have it current.
    ASSERT(mIsDestroyed == true && mRefCount == 0);

    // Dump frame capture if enabled.
    getShareGroup()->getFrameCaptureShared()->onDestroyContext(this);

    // Remove context from the capture share group
    getShareGroup()->removeSharedContext(this);

    if (mGLES1Renderer)
    {
        mGLES1Renderer->onDestroy(this, &mState);
    }

    ANGLE_TRY(unMakeCurrent(display));

    mDefaultFramebuffer->onDestroy(this);
    mDefaultFramebuffer.reset();

    for (auto fence : mFenceNVMap)
    {
        if (fence.second)
        {
            fence.second->onDestroy(this);
        }
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
    // mProgramPipelineManager must be before mShaderProgramManager to give each
    // PPO the chance to release any references they have to the Programs that
    // are bound to them before the Programs are released()'ed.
    mState.mProgramPipelineManager->release(this);
    mState.mShaderProgramManager->release(this);
    mState.mTextureManager->release(this);
    mState.mRenderbufferManager->release(this);
    mState.mSamplerManager->release(this);
    mState.mSyncManager->release(this);
    mState.mFramebufferManager->release(this);
    mState.mMemoryObjectManager->release(this);
    mState.mSemaphoreManager->release(this);

    mImplementation->onDestroy(this);

    // Backend requires implementation to be destroyed first to close down all the objects
    mState.mShareGroup->release(display);

    mOverlay.destroy(this);

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

egl::Error Context::makeCurrent(egl::Display *display,
                                egl::Surface *drawSurface,
                                egl::Surface *readSurface)
{
    mDisplay = display;

    if (!mHasBeenCurrent)
    {
        initializeDefaultResources();
        initRendererString();
        initVersionStrings();
        initExtensionStrings();

        int width  = 0;
        int height = 0;
        if (drawSurface != nullptr)
        {
            width  = drawSurface->getWidth();
            height = drawSurface->getHeight();
        }

        mState.setViewportParams(0, 0, width, height);
        mState.setScissorParams(0, 0, width, height);

        mHasBeenCurrent = true;
    }

    ANGLE_TRY(unsetDefaultFramebuffer());

    getShareGroup()->getFrameCaptureShared()->onMakeCurrent(this, drawSurface);

    // TODO(jmadill): Rework this when we support ContextImpl
    mState.setAllDirtyBits();
    mState.setAllDirtyObjects();

    ANGLE_TRY(setDefaultFramebuffer(drawSurface, readSurface));

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
    ANGLE_TRY(angle::ResultToEGL(mImplementation->onUnMakeCurrent(this)));

    ANGLE_TRY(unsetDefaultFramebuffer());

    // Return the scratch buffers to the display so they can be shared with other contexts while
    // this one is not current.
    if (mScratchBuffer.valid())
    {
        mDisplay->returnScratchBuffer(mScratchBuffer.release());
    }
    if (mZeroFilledBuffer.valid())
    {
        mDisplay->returnZeroFilledBuffer(mZeroFilledBuffer.release());
    }

    return egl::NoError();
}

BufferID Context::createBuffer()
{
    return mState.mBufferManager->createBuffer();
}

GLuint Context::createProgram()
{
    return mState.mShaderProgramManager->createProgram(mImplementation.get()).value;
}

GLuint Context::createShader(ShaderType type)
{
    return mState.mShaderProgramManager
        ->createShader(mImplementation.get(), mState.mLimitations, type)
        .value;
}

TextureID Context::createTexture()
{
    return mState.mTextureManager->createTexture();
}

RenderbufferID Context::createRenderbuffer()
{
    return mState.mRenderbufferManager->createRenderbuffer();
}

// Returns an unused framebuffer name
FramebufferID Context::createFramebuffer()
{
    return mState.mFramebufferManager->createFramebuffer();
}

void Context::genFencesNV(GLsizei n, FenceNVID *fences)
{
    for (int i = 0; i < n; i++)
    {
        GLuint handle = mFenceNVHandleAllocator.allocate();
        mFenceNVMap.assign({handle}, new FenceNV(mImplementation.get()));
        fences[i] = {handle};
    }
}

ProgramPipelineID Context::createProgramPipeline()
{
    return mState.mProgramPipelineManager->createProgramPipeline();
}

GLuint Context::createShaderProgramv(ShaderType type, GLsizei count, const GLchar *const *strings)
{
    const ShaderProgramID shaderID = PackParam<ShaderProgramID>(createShader(type));
    if (shaderID.value)
    {
        Shader *shaderObject = getShader(shaderID);
        ASSERT(shaderObject);
        shaderObject->setSource(count, strings, nullptr);
        shaderObject->compile(this);
        const ShaderProgramID programID = PackParam<ShaderProgramID>(createProgram());
        if (programID.value)
        {
            gl::Program *programObject = getProgramNoResolveLink(programID);
            ASSERT(programObject);

            if (shaderObject->isCompiled(this))
            {
                // As per Khronos issue 2261:
                // https://gitlab.khronos.org/Tracker/vk-gl-cts/issues/2261
                // We must wait to mark the program separable until it's successfully compiled.
                programObject->setSeparable(true);

                programObject->attachShader(shaderObject);

                if (programObject->link(this) != angle::Result::Continue)
                {
                    deleteShader(shaderID);
                    deleteProgram(programID);
                    return 0u;
                }
                if (onProgramLink(programObject) != angle::Result::Continue)
                {
                    deleteShader(shaderID);
                    deleteProgram(programID);
                    return 0u;
                }

                programObject->detachShader(this, shaderObject);
            }

            InfoLog &programInfoLog = programObject->getExecutable().getInfoLog();
            programInfoLog << shaderObject->getInfoLogString();
        }

        deleteShader(shaderID);

        return programID.value;
    }

    return 0u;
}

MemoryObjectID Context::createMemoryObject()
{
    return mState.mMemoryObjectManager->createMemoryObject(mImplementation.get());
}

SemaphoreID Context::createSemaphore()
{
    return mState.mSemaphoreManager->createSemaphore(mImplementation.get());
}

void Context::deleteBuffer(BufferID bufferName)
{
    Buffer *buffer = mState.mBufferManager->getBuffer(bufferName);
    if (buffer)
    {
        detachBuffer(buffer);
    }

    mState.mBufferManager->deleteObject(this, bufferName);
}

void Context::deleteShader(ShaderProgramID shader)
{
    mState.mShaderProgramManager->deleteShader(this, shader);
}

void Context::deleteProgram(ShaderProgramID program)
{
    mState.mShaderProgramManager->deleteProgram(this, program);
}

void Context::deleteTexture(TextureID texture)
{
    if (mState.mTextureManager->getTexture(texture))
    {
        detachTexture(texture);
    }

    mState.mTextureManager->deleteObject(this, texture);
}

void Context::deleteRenderbuffer(RenderbufferID renderbuffer)
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

void Context::deleteProgramPipeline(ProgramPipelineID pipelineID)
{
    ProgramPipeline *pipeline = mState.mProgramPipelineManager->getProgramPipeline(pipelineID);
    if (pipeline)
    {
        detachProgramPipeline(pipelineID);
    }

    mState.mProgramPipelineManager->deleteObject(this, pipelineID);
}

void Context::deleteMemoryObject(MemoryObjectID memoryObject)
{
    mState.mMemoryObjectManager->deleteMemoryObject(this, memoryObject);
}

void Context::deleteSemaphore(SemaphoreID semaphore)
{
    mState.mSemaphoreManager->deleteSemaphore(this, semaphore);
}

// GL_CHROMIUM_lose_context
void Context::loseContext(GraphicsResetStatus current, GraphicsResetStatus other)
{
    // TODO(geofflang): mark the rest of the share group lost. Requires access to the entire share
    // group from a context. http://anglebug.com/3379
    markContextLost(current);
}

void Context::deleteFramebuffer(FramebufferID framebufferID)
{
    // We are responsible for deleting the GL objects from the Framebuffer's pixel local storage.
    std::unique_ptr<PixelLocalStorage> plsToDelete;

    Framebuffer *framebuffer = mState.mFramebufferManager->getFramebuffer(framebufferID);
    if (framebuffer)
    {
        plsToDelete = framebuffer->detachPixelLocalStorage();
        detachFramebuffer(framebufferID);
    }

    mState.mFramebufferManager->deleteObject(this, framebufferID);

    // Delete the pixel local storage GL objects after the framebuffer, in order to avoid any
    // potential trickyness with orphaning.
    if (plsToDelete)
    {
        plsToDelete->deleteContextObjects(this);
    }
}

void Context::deleteFencesNV(GLsizei n, const FenceNVID *fences)
{
    for (int i = 0; i < n; i++)
    {
        FenceNVID fence = fences[i];

        FenceNV *fenceObject = nullptr;
        if (mFenceNVMap.erase(fence, &fenceObject))
        {
            mFenceNVHandleAllocator.release(fence.value);
            if (fenceObject)
            {
                fenceObject->onDestroy(this);
            }
            delete fenceObject;
        }
    }
}

Buffer *Context::getBuffer(BufferID handle) const
{
    return mState.mBufferManager->getBuffer(handle);
}

Renderbuffer *Context::getRenderbuffer(RenderbufferID handle) const
{
    return mState.mRenderbufferManager->getRenderbuffer(handle);
}

EGLenum Context::getContextPriority() const
{
    return egl::ToEGLenum(mImplementation->getContextPriority());
}

Sync *Context::getSync(GLsync handle) const
{
    return mState.mSyncManager->getSync(static_cast<GLuint>(reinterpret_cast<uintptr_t>(handle)));
}

VertexArray *Context::getVertexArray(VertexArrayID handle) const
{
    return mVertexArrayMap.query(handle);
}

Sampler *Context::getSampler(SamplerID handle) const
{
    return mState.mSamplerManager->getSampler(handle);
}

TransformFeedback *Context::getTransformFeedback(TransformFeedbackID handle) const
{
    return mTransformFeedbackMap.query(handle);
}

ProgramPipeline *Context::getProgramPipeline(ProgramPipelineID handle) const
{
    return mState.mProgramPipelineManager->getProgramPipeline(handle);
}

gl::LabeledObject *Context::getLabeledObject(GLenum identifier, GLuint name) const
{
    switch (identifier)
    {
        case GL_BUFFER:
        case GL_BUFFER_OBJECT_EXT:
            return getBuffer({name});
        case GL_SHADER:
        case GL_SHADER_OBJECT_EXT:
            return getShader({name});
        case GL_PROGRAM:
        case GL_PROGRAM_OBJECT_EXT:
            return getProgramNoResolveLink({name});
        case GL_VERTEX_ARRAY:
        case GL_VERTEX_ARRAY_OBJECT_EXT:
            return getVertexArray({name});
        case GL_QUERY:
        case GL_QUERY_OBJECT_EXT:
            return getQuery({name});
        case GL_TRANSFORM_FEEDBACK:
            return getTransformFeedback({name});
        case GL_SAMPLER:
            return getSampler({name});
        case GL_TEXTURE:
            return getTexture({name});
        case GL_RENDERBUFFER:
            return getRenderbuffer({name});
        case GL_FRAMEBUFFER:
            return getFramebuffer({name});
        case GL_PROGRAM_PIPELINE:
        case GL_PROGRAM_PIPELINE_OBJECT_EXT:
            return getProgramPipeline({name});
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
    ANGLE_CONTEXT_TRY(object->setLabel(this, labelName));

    // TODO(jmadill): Determine if the object is dirty based on 'name'. Conservatively assume the
    // specified object is active until we do this.
    mState.setObjectDirty(identifier);
}

void Context::labelObject(GLenum type, GLuint object, GLsizei length, const GLchar *label)
{
    gl::LabeledObject *obj = getLabeledObject(type, object);
    ASSERT(obj != nullptr);

    std::string labelName = "";
    if (label != nullptr)
    {
        size_t labelLength = length == 0 ? strlen(label) : length;
        labelName          = std::string(label, labelLength);
    }
    ANGLE_CONTEXT_TRY(obj->setLabel(this, labelName));
    mState.setObjectDirty(type);
}

void Context::objectPtrLabel(const void *ptr, GLsizei length, const GLchar *label)
{
    gl::LabeledObject *object = getLabeledObjectFromPtr(ptr);
    ASSERT(object != nullptr);

    std::string labelName = GetObjectLabelFromPointer(length, label);
    ANGLE_CONTEXT_TRY(object->setLabel(this, labelName));
}

void Context::getObjectLabel(GLenum identifier,
                             GLuint name,
                             GLsizei bufSize,
                             GLsizei *length,
                             GLchar *label)
{
    gl::LabeledObject *object = getLabeledObject(identifier, name);
    ASSERT(object != nullptr);

    const std::string &objectLabel = object->getLabel();
    GetObjectLabelBase(objectLabel, bufSize, length, label);
}

void Context::getObjectPtrLabel(const void *ptr, GLsizei bufSize, GLsizei *length, GLchar *label)
{
    gl::LabeledObject *object = getLabeledObjectFromPtr(ptr);
    ASSERT(object != nullptr);

    const std::string &objectLabel = object->getLabel();
    GetObjectLabelBase(objectLabel, bufSize, length, label);
}

GLboolean Context::isSampler(SamplerID samplerName) const
{
    return mState.mSamplerManager->isSampler(samplerName);
}

void Context::bindTexture(TextureType target, TextureID handle)
{
    // Some apps enable KHR_create_context_no_error but pass in an invalid texture type.
    // Workaround this by silently returning in such situations.
    if (target == TextureType::InvalidEnum)
    {
        return;
    }

    Texture *texture = nullptr;
    if (handle.value == 0)
    {
        texture = mZeroTextures[target].get();
    }
    else
    {
        texture =
            mState.mTextureManager->checkTextureAllocation(mImplementation.get(), handle, target);
    }

    ASSERT(texture);
    // Early return if rebinding the same texture
    if (texture == mState.getTargetTexture(target))
    {
        return;
    }

    mState.setSamplerTexture(this, target, texture);
    mStateCache.onActiveTextureChange(this);
}

void Context::bindReadFramebuffer(FramebufferID framebufferHandle)
{
    Framebuffer *framebuffer = mState.mFramebufferManager->checkFramebufferAllocation(
        mImplementation.get(), this, framebufferHandle);
    mState.setReadFramebufferBinding(framebuffer);
    mReadFramebufferObserverBinding.bind(framebuffer);
}

void Context::bindDrawFramebuffer(FramebufferID framebufferHandle)
{
    Framebuffer *framebuffer = mState.mFramebufferManager->checkFramebufferAllocation(
        mImplementation.get(), this, framebufferHandle);
    mState.setDrawFramebufferBinding(framebuffer);
    mDrawFramebufferObserverBinding.bind(framebuffer);
    mStateCache.onDrawFramebufferChange(this);
}

void Context::bindVertexArray(VertexArrayID vertexArrayHandle)
{
    VertexArray *vertexArray = checkVertexArrayAllocation(vertexArrayHandle);
    mState.setVertexArrayBinding(this, vertexArray);
    mVertexArrayObserverBinding.bind(vertexArray);
    mStateCache.onVertexArrayBindingChange(this);
}

void Context::bindVertexBuffer(GLuint bindingIndex,
                               BufferID bufferHandle,
                               GLintptr offset,
                               GLsizei stride)
{
    Buffer *buffer =
        mState.mBufferManager->checkBufferAllocation(mImplementation.get(), bufferHandle);
    mState.bindVertexBuffer(this, bindingIndex, buffer, offset, stride);
    mStateCache.onVertexArrayStateChange(this);
}

void Context::bindSampler(GLuint textureUnit, SamplerID samplerHandle)
{
    ASSERT(textureUnit < static_cast<GLuint>(mState.mCaps.maxCombinedTextureImageUnits));
    Sampler *sampler =
        mState.mSamplerManager->checkSamplerAllocation(mImplementation.get(), samplerHandle);

    // Early return if rebinding the same sampler
    if (sampler == mState.getSampler(textureUnit))
    {
        return;
    }

    mState.setSamplerBinding(this, textureUnit, sampler);
    mSamplerObserverBindings[textureUnit].bind(sampler);
    mStateCache.onActiveTextureChange(this);
}

void Context::bindImageTexture(GLuint unit,
                               TextureID texture,
                               GLint level,
                               GLboolean layered,
                               GLint layer,
                               GLenum access,
                               GLenum format)
{
    Texture *tex = mState.mTextureManager->getTexture(texture);
    mState.setImageUnit(this, unit, tex, level, layered, layer, access, format);
    mImageObserverBindings[unit].bind(tex);
}

void Context::useProgram(ShaderProgramID program)
{
    ANGLE_CONTEXT_TRY(mState.setProgram(this, getProgramResolveLink(program)));
    mStateCache.onProgramExecutableChange(this);
}

void Context::useProgramStages(ProgramPipelineID pipeline,
                               GLbitfield stages,
                               ShaderProgramID program)
{
    Program *shaderProgram = getProgramNoResolveLink(program);
    ProgramPipeline *programPipeline =
        mState.mProgramPipelineManager->checkProgramPipelineAllocation(mImplementation.get(),
                                                                       pipeline);

    ASSERT(programPipeline);
    ANGLE_CONTEXT_TRY(programPipeline->useProgramStages(this, stages, shaderProgram));
}

void Context::bindTransformFeedback(GLenum target, TransformFeedbackID transformFeedbackHandle)
{
    ASSERT(target == GL_TRANSFORM_FEEDBACK);
    TransformFeedback *transformFeedback =
        checkTransformFeedbackAllocation(transformFeedbackHandle);
    mState.setTransformFeedbackBinding(this, transformFeedback);
    mStateCache.onActiveTransformFeedbackChange(this);
}

void Context::bindProgramPipeline(ProgramPipelineID pipelineHandle)
{
    ProgramPipeline *pipeline = mState.mProgramPipelineManager->checkProgramPipelineAllocation(
        mImplementation.get(), pipelineHandle);
    ANGLE_CONTEXT_TRY(mState.setProgramPipelineBinding(this, pipeline));
    mStateCache.onProgramExecutableChange(this);
    mProgramPipelineObserverBinding.bind(pipeline);
}

void Context::beginQuery(QueryType target, QueryID query)
{
    Query *queryObject = getOrCreateQuery(query, target);
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

void Context::queryCounter(QueryID id, QueryType target)
{
    ASSERT(target == QueryType::Timestamp);

    Query *queryObject = getOrCreateQuery(id, target);
    ASSERT(queryObject);

    ANGLE_CONTEXT_TRY(queryObject->queryCounter(this));
}

void Context::getQueryiv(QueryType target, GLenum pname, GLint *params)
{
    switch (pname)
    {
        case GL_CURRENT_QUERY_EXT:
            params[0] = mState.getActiveQueryId(target).value;
            break;
        case GL_QUERY_COUNTER_BITS_EXT:
            switch (target)
            {
                case QueryType::TimeElapsed:
                    params[0] = getCaps().queryCounterBitsTimeElapsed;
                    break;
                case QueryType::Timestamp:
                    params[0] = getCaps().queryCounterBitsTimestamp;
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

void Context::getQueryObjectiv(QueryID id, GLenum pname, GLint *params)
{
    ANGLE_CONTEXT_TRY(GetQueryObjectParameter(this, getQuery(id), pname, params));
}

void Context::getQueryObjectivRobust(QueryID id,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLint *params)
{
    getQueryObjectiv(id, pname, params);
}

void Context::getQueryObjectuiv(QueryID id, GLenum pname, GLuint *params)
{
    ANGLE_CONTEXT_TRY(GetQueryObjectParameter(this, getQuery(id), pname, params));
}

void Context::getQueryObjectuivRobust(QueryID id,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLuint *params)
{
    getQueryObjectuiv(id, pname, params);
}

void Context::getQueryObjecti64v(QueryID id, GLenum pname, GLint64 *params)
{
    ANGLE_CONTEXT_TRY(GetQueryObjectParameter(this, getQuery(id), pname, params));
}

void Context::getQueryObjecti64vRobust(QueryID id,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       GLint64 *params)
{
    getQueryObjecti64v(id, pname, params);
}

void Context::getQueryObjectui64v(QueryID id, GLenum pname, GLuint64 *params)
{
    ANGLE_CONTEXT_TRY(GetQueryObjectParameter(this, getQuery(id), pname, params));
}

void Context::getQueryObjectui64vRobust(QueryID id,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLuint64 *params)
{
    getQueryObjectui64v(id, pname, params);
}

Framebuffer *Context::getFramebuffer(FramebufferID handle) const
{
    return mState.mFramebufferManager->getFramebuffer(handle);
}

FenceNV *Context::getFenceNV(FenceNVID handle) const
{
    return mFenceNVMap.query(handle);
}

Query *Context::getOrCreateQuery(QueryID handle, QueryType type)
{
    if (!mQueryMap.contains(handle))
    {
        return nullptr;
    }

    Query *query = mQueryMap.query(handle);
    if (!query)
    {
        ASSERT(type != QueryType::InvalidEnum);
        query = new Query(mImplementation.get(), type, handle);
        query->addRef();
        mQueryMap.assign(handle, query);
    }
    return query;
}

Query *Context::getQuery(QueryID handle) const
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
        mCompiler.set(this, new Compiler(mImplementation.get(), mState, mDisplay));
    }
    return mCompiler.get();
}

void Context::getBooleanvImpl(GLenum pname, GLboolean *params) const
{
    switch (pname)
    {
        case GL_SHADER_COMPILER:
            *params = GL_TRUE;
            break;
        case GL_CONTEXT_ROBUST_ACCESS_EXT:
            *params = ConvertToGLBoolean(mState.hasRobustAccess());
            break;

        default:
            mState.getBooleanv(pname, params);
            break;
    }
}

void Context::getFloatvImpl(GLenum pname, GLfloat *params) const
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
            ASSERT(mState.mExtensions.textureFilterAnisotropicEXT);
            *params = mState.mCaps.maxTextureAnisotropy;
            break;
        case GL_MAX_TEXTURE_LOD_BIAS:
            *params = mState.mCaps.maxLODBias;
            break;
        case GL_MIN_FRAGMENT_INTERPOLATION_OFFSET:
            *params = mState.mCaps.minInterpolationOffset;
            break;
        case GL_MAX_FRAGMENT_INTERPOLATION_OFFSET:
            *params = mState.mCaps.maxInterpolationOffset;
            break;
        case GL_PRIMITIVE_BOUNDING_BOX:
            params[0] = mState.mBoundingBoxMinX;
            params[1] = mState.mBoundingBoxMinY;
            params[2] = mState.mBoundingBoxMinZ;
            params[3] = mState.mBoundingBoxMinW;
            params[4] = mState.mBoundingBoxMaxX;
            params[5] = mState.mBoundingBoxMaxY;
            params[6] = mState.mBoundingBoxMaxZ;
            params[7] = mState.mBoundingBoxMaxW;
            break;
        default:
            mState.getFloatv(pname, params);
            break;
    }
}

void Context::getIntegervImpl(GLenum pname, GLint *params) const
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
            *params = mState.mCaps.maxVaryingVectors * 4;
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

        // Desktop client flags
        case GL_CONTEXT_FLAGS:
        {
            GLint contextFlags = 0;
            if (mState.hasProtectedContent())
            {
                contextFlags |= GL_CONTEXT_FLAG_PROTECTED_CONTENT_BIT_EXT;
            }

            if (mState.isDebugContext())
            {
                contextFlags |= GL_CONTEXT_FLAG_DEBUG_BIT_KHR;
            }

            if (mState.hasRobustAccess())
            {
                contextFlags |= GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT;
            }
            *params = contextFlags;
        }
        break;
        case GL_CONTEXT_PROFILE_MASK:
            ASSERT(getClientType() == EGL_OPENGL_API);
            *params = mState.getProfileMask();
            break;

        // GL_ANGLE_request_extension
        case GL_NUM_REQUESTABLE_EXTENSIONS_ANGLE:
            *params = static_cast<GLint>(mRequestableExtensionStrings.size());
            break;

        // GL_KHR_debug
        case GL_MAX_DEBUG_MESSAGE_LENGTH:
            *params = mState.mCaps.maxDebugMessageLength;
            break;
        case GL_MAX_DEBUG_LOGGED_MESSAGES:
            *params = mState.mCaps.maxDebugLoggedMessages;
            break;
        case GL_MAX_DEBUG_GROUP_STACK_DEPTH:
            *params = mState.mCaps.maxDebugGroupStackDepth;
            break;
        case GL_MAX_LABEL_LENGTH:
            *params = mState.mCaps.maxLabelLength;
            break;

        // GL_OVR_multiview2
        case GL_MAX_VIEWS_OVR:
            *params = mState.mCaps.maxViews;
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
        // GL_EXT_tessellation_shader
        case GL_MAX_PATCH_VERTICES_EXT:
            *params = mState.mCaps.maxPatchVertices;
            break;
        case GL_MAX_TESS_GEN_LEVEL_EXT:
            *params = mState.mCaps.maxTessGenLevel;
            break;
        case GL_MAX_TESS_CONTROL_UNIFORM_COMPONENTS_EXT:
            *params = mState.mCaps.maxShaderUniformComponents[ShaderType::TessControl];
            break;
        case GL_MAX_TESS_EVALUATION_UNIFORM_COMPONENTS_EXT:
            *params = mState.mCaps.maxShaderUniformComponents[ShaderType::TessEvaluation];
            break;
        case GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS_EXT:
            *params = mState.mCaps.maxShaderTextureImageUnits[ShaderType::TessControl];
            break;
        case GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS_EXT:
            *params = mState.mCaps.maxShaderTextureImageUnits[ShaderType::TessEvaluation];
            break;
        case GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS_EXT:
            *params = mState.mCaps.maxTessControlOutputComponents;
            break;
        case GL_MAX_TESS_PATCH_COMPONENTS_EXT:
            *params = mState.mCaps.maxTessPatchComponents;
            break;
        case GL_MAX_TESS_CONTROL_TOTAL_OUTPUT_COMPONENTS_EXT:
            *params = mState.mCaps.maxTessControlTotalOutputComponents;
            break;
        case GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS_EXT:
            *params = mState.mCaps.maxTessEvaluationOutputComponents;
            break;
        case GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS_EXT:
            *params = mState.mCaps.maxShaderUniformBlocks[ShaderType::TessControl];
            break;
        case GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS_EXT:
            *params = mState.mCaps.maxShaderUniformBlocks[ShaderType::TessEvaluation];
            break;
        case GL_MAX_TESS_CONTROL_INPUT_COMPONENTS_EXT:
            *params = mState.mCaps.maxTessControlInputComponents;
            break;
        case GL_MAX_TESS_EVALUATION_INPUT_COMPONENTS_EXT:
            *params = mState.mCaps.maxTessEvaluationInputComponents;
            break;
        case GL_MAX_COMBINED_TESS_CONTROL_UNIFORM_COMPONENTS_EXT:
            *params = static_cast<GLint>(
                mState.mCaps.maxCombinedShaderUniformComponents[ShaderType::TessControl]);
            break;
        case GL_MAX_COMBINED_TESS_EVALUATION_UNIFORM_COMPONENTS_EXT:
            *params = static_cast<GLint>(
                mState.mCaps.maxCombinedShaderUniformComponents[ShaderType::TessEvaluation]);
            break;
        case GL_MAX_TESS_CONTROL_ATOMIC_COUNTER_BUFFERS_EXT:
            *params = mState.mCaps.maxShaderAtomicCounterBuffers[ShaderType::TessControl];
            break;
        case GL_MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS_EXT:
            *params = mState.mCaps.maxShaderAtomicCounterBuffers[ShaderType::TessEvaluation];
            break;
        case GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS_EXT:
            *params = mState.mCaps.maxShaderAtomicCounters[ShaderType::TessControl];
            break;
        case GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS_EXT:
            *params = mState.mCaps.maxShaderAtomicCounters[ShaderType::TessEvaluation];
            break;
        case GL_MAX_TESS_CONTROL_IMAGE_UNIFORMS_EXT:
            *params = mState.mCaps.maxShaderImageUniforms[ShaderType::TessControl];
            break;
        case GL_MAX_TESS_EVALUATION_IMAGE_UNIFORMS_EXT:
            *params = mState.mCaps.maxShaderImageUniforms[ShaderType::TessEvaluation];
            break;
        case GL_MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS_EXT:
            *params = mState.mCaps.maxShaderStorageBlocks[ShaderType::TessControl];
            break;
        case GL_MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS_EXT:
            *params = mState.mCaps.maxShaderStorageBlocks[ShaderType::TessEvaluation];
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

        // case GL_MAX_CLIP_DISTANCES_EXT:  Conflict enum value
        case GL_MAX_CLIP_PLANES:
            if (getClientVersion().major >= 2)
            {
                // GL_APPLE_clip_distance/GL_EXT_clip_cull_distance
                *params = mState.mCaps.maxClipDistances;
            }
            else
            {
                *params = mState.mCaps.maxClipPlanes;
            }
            break;
        case GL_MAX_CULL_DISTANCES_EXT:
            *params = mState.mCaps.maxCullDistances;
            break;
        case GL_MAX_COMBINED_CLIP_AND_CULL_DISTANCES_EXT:
            *params = mState.mCaps.maxCombinedClipAndCullDistances;
            break;
        // GLES1 emulation: Vertex attribute queries
        case GL_VERTEX_ARRAY_BUFFER_BINDING:
        case GL_NORMAL_ARRAY_BUFFER_BINDING:
        case GL_COLOR_ARRAY_BUFFER_BINDING:
        case GL_POINT_SIZE_ARRAY_BUFFER_BINDING_OES:
        case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING:
            getIntegerVertexAttribImpl(pname, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, params);
            break;
        case GL_VERTEX_ARRAY_STRIDE:
        case GL_NORMAL_ARRAY_STRIDE:
        case GL_COLOR_ARRAY_STRIDE:
        case GL_POINT_SIZE_ARRAY_STRIDE_OES:
        case GL_TEXTURE_COORD_ARRAY_STRIDE:
            getIntegerVertexAttribImpl(pname, GL_VERTEX_ATTRIB_ARRAY_STRIDE, params);
            break;
        case GL_VERTEX_ARRAY_SIZE:
        case GL_COLOR_ARRAY_SIZE:
        case GL_TEXTURE_COORD_ARRAY_SIZE:
            getIntegerVertexAttribImpl(pname, GL_VERTEX_ATTRIB_ARRAY_SIZE, params);
            break;
        case GL_VERTEX_ARRAY_TYPE:
        case GL_COLOR_ARRAY_TYPE:
        case GL_NORMAL_ARRAY_TYPE:
        case GL_POINT_SIZE_ARRAY_TYPE_OES:
        case GL_TEXTURE_COORD_ARRAY_TYPE:
            getIntegerVertexAttribImpl(pname, GL_VERTEX_ATTRIB_ARRAY_TYPE, params);
            break;

        // GL_KHR_parallel_shader_compile
        case GL_MAX_SHADER_COMPILER_THREADS_KHR:
            *params = mState.getMaxShaderCompilerThreads();
            break;

        // GL_EXT_blend_func_extended
        case GL_MAX_DUAL_SOURCE_DRAW_BUFFERS_EXT:
            *params = mState.mCaps.maxDualSourceDrawBuffers;
            break;

        // OES_shader_multisample_interpolation
        case GL_FRAGMENT_INTERPOLATION_OFFSET_BITS_OES:
            *params = mState.mCaps.subPixelInterpolationOffsetBits;
            break;

        // GL_OES_texture_buffer
        case GL_MAX_TEXTURE_BUFFER_SIZE:
            *params = mState.mCaps.maxTextureBufferSize;
            break;
        case GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT:
            *params = mState.mCaps.textureBufferOffsetAlignment;
            break;

        // GL_EXT_clip_control
        case GL_CLIP_ORIGIN_EXT:
            *params = mState.mClipControlOrigin;
            break;
        case GL_CLIP_DEPTH_MODE_EXT:
            *params = mState.mClipControlDepth;
            break;

        // ANGLE_shader_pixel_local_storage
        case GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE:
            *params = mState.mCaps.maxPixelLocalStoragePlanes;
            break;
        case GL_MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE_ANGLE:
            *params = mState.mCaps.maxColorAttachmentsWithActivePixelLocalStorage;
            break;
        case GL_MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES_ANGLE:
            *params = mState.mCaps.maxCombinedDrawBuffersAndPixelLocalStoragePlanes;
            break;

        default:
            ANGLE_CONTEXT_TRY(mState.getIntegerv(this, pname, params));
            break;
    }
}

void Context::getIntegerVertexAttribImpl(GLenum pname, GLenum attribpname, GLint *params) const
{
    getVertexAttribivImpl(static_cast<GLuint>(vertexArrayIndex(ParamToVertexArrayType(pname))),
                          attribpname, params);
}

void Context::getInteger64vImpl(GLenum pname, GLint64 *params) const
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

void Context::getPointerv(GLenum pname, void **params)
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
                mState.getIntegeri_v(this, target, index, data);
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

void Context::texBuffer(TextureType target, GLenum internalformat, BufferID buffer)
{
    ASSERT(target == TextureType::Buffer);

    Texture *texture  = getTextureByType(target);
    Buffer *bufferObj = mState.mBufferManager->getBuffer(buffer);
    ANGLE_CONTEXT_TRY(texture->setBuffer(this, bufferObj, internalformat));
}

void Context::texBufferRange(TextureType target,
                             GLenum internalformat,
                             BufferID buffer,
                             GLintptr offset,
                             GLsizeiptr size)
{
    ASSERT(target == TextureType::Buffer);

    Texture *texture  = getTextureByType(target);
    Buffer *bufferObj = mState.mBufferManager->getBuffer(buffer);
    ANGLE_CONTEXT_TRY(texture->setBufferRange(this, bufferObj, internalformat, offset, size));
}

void Context::getTexParameterfv(TextureType target, GLenum pname, GLfloat *params)
{
    const Texture *const texture = getTextureByType(target);
    QueryTexParameterfv(this, texture, pname, params);
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
    QueryTexParameteriv(this, texture, pname, params);
}

void Context::getTexParameterIiv(TextureType target, GLenum pname, GLint *params)
{
    const Texture *const texture = getTextureByType(target);
    QueryTexParameterIiv(this, texture, pname, params);
}

void Context::getTexParameterIuiv(TextureType target, GLenum pname, GLuint *params)
{
    const Texture *const texture = getTextureByType(target);
    QueryTexParameterIuiv(this, texture, pname, params);
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
    // Some apps enable KHR_create_context_no_error but pass in an invalid texture type.
    // Workaround this by silently returning in such situations.
    if (target == TextureType::InvalidEnum)
    {
        return;
    }

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
        ANGLE_CONTEXT_TRY(mImplementation->handleNoopDrawEvent());
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    ANGLE_CONTEXT_TRY(
        mImplementation->drawArraysInstanced(this, mode, first, count, instanceCount));
    MarkTransformFeedbackBufferUsage(this, count, instanceCount);
    MarkShaderStorageUsage(this);
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
        ANGLE_CONTEXT_TRY(mImplementation->handleNoopDrawEvent());
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    ANGLE_CONTEXT_TRY(
        mImplementation->drawElementsInstanced(this, mode, count, type, indices, instances));
    MarkShaderStorageUsage(this);
}

void Context::drawElementsBaseVertex(PrimitiveMode mode,
                                     GLsizei count,
                                     DrawElementsType type,
                                     const void *indices,
                                     GLint basevertex)
{
    // No-op if count draws no primitives for given mode
    if (noopDraw(mode, count))
    {
        ANGLE_CONTEXT_TRY(mImplementation->handleNoopDrawEvent());
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    ANGLE_CONTEXT_TRY(
        mImplementation->drawElementsBaseVertex(this, mode, count, type, indices, basevertex));
    MarkShaderStorageUsage(this);
}

void Context::drawElementsInstancedBaseVertex(PrimitiveMode mode,
                                              GLsizei count,
                                              DrawElementsType type,
                                              const void *indices,
                                              GLsizei instancecount,
                                              GLint basevertex)
{
    // No-op if count draws no primitives for given mode
    if (noopDrawInstanced(mode, count, instancecount))
    {
        ANGLE_CONTEXT_TRY(mImplementation->handleNoopDrawEvent());
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    ANGLE_CONTEXT_TRY(mImplementation->drawElementsInstancedBaseVertex(
        this, mode, count, type, indices, instancecount, basevertex));
    MarkShaderStorageUsage(this);
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
        ANGLE_CONTEXT_TRY(mImplementation->handleNoopDrawEvent());
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    ANGLE_CONTEXT_TRY(
        mImplementation->drawRangeElements(this, mode, start, end, count, type, indices));
    MarkShaderStorageUsage(this);
}

void Context::drawRangeElementsBaseVertex(PrimitiveMode mode,
                                          GLuint start,
                                          GLuint end,
                                          GLsizei count,
                                          DrawElementsType type,
                                          const void *indices,
                                          GLint basevertex)
{
    // No-op if count draws no primitives for given mode
    if (noopDraw(mode, count))
    {
        ANGLE_CONTEXT_TRY(mImplementation->handleNoopDrawEvent());
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    ANGLE_CONTEXT_TRY(mImplementation->drawRangeElementsBaseVertex(this, mode, start, end, count,
                                                                   type, indices, basevertex));
    MarkShaderStorageUsage(this);
}

void Context::drawArraysIndirect(PrimitiveMode mode, const void *indirect)
{
    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    ANGLE_CONTEXT_TRY(mImplementation->drawArraysIndirect(this, mode, indirect));
    MarkShaderStorageUsage(this);
}

void Context::drawElementsIndirect(PrimitiveMode mode, DrawElementsType type, const void *indirect)
{
    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    ANGLE_CONTEXT_TRY(mImplementation->drawElementsIndirect(this, mode, type, indirect));
    MarkShaderStorageUsage(this);
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
    ANGLE_CONTEXT_TRY(mImplementation->insertEventMarker(length, marker));
}

void Context::pushGroupMarker(GLsizei length, const char *marker)
{
    ASSERT(mImplementation);

    if (marker == nullptr)
    {
        // From the EXT_debug_marker spec,
        // "If <marker> is null then an empty string is pushed on the stack."
        ANGLE_CONTEXT_TRY(mImplementation->pushGroupMarker(length, ""));
    }
    else
    {
        ANGLE_CONTEXT_TRY(mImplementation->pushGroupMarker(length, marker));
    }
}

void Context::popGroupMarker()
{
    ASSERT(mImplementation);
    ANGLE_CONTEXT_TRY(mImplementation->popGroupMarker());
}

void Context::bindUniformLocation(ShaderProgramID program,
                                  UniformLocation location,
                                  const GLchar *name)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);

    programObject->bindUniformLocation(location, name);
}

void Context::coverageModulation(GLenum components)
{
    mState.setCoverageModulation(components);
}

GLuint Context::getProgramResourceIndex(ShaderProgramID program,
                                        GLenum programInterface,
                                        const GLchar *name)
{
    const Program *programObject = getProgramResolveLink(program);
    return QueryProgramResourceIndex(programObject, programInterface, name);
}

void Context::getProgramResourceName(ShaderProgramID program,
                                     GLenum programInterface,
                                     GLuint index,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLchar *name)
{
    const Program *programObject = getProgramResolveLink(program);
    QueryProgramResourceName(this, programObject, programInterface, index, bufSize, length, name);
}

GLint Context::getProgramResourceLocation(ShaderProgramID program,
                                          GLenum programInterface,
                                          const GLchar *name)
{
    const Program *programObject = getProgramResolveLink(program);
    return QueryProgramResourceLocation(programObject, programInterface, name);
}

void Context::getProgramResourceiv(ShaderProgramID program,
                                   GLenum programInterface,
                                   GLuint index,
                                   GLsizei propCount,
                                   const GLenum *props,
                                   GLsizei bufSize,
                                   GLsizei *length,
                                   GLint *params)
{
    const Program *programObject = getProgramResolveLink(program);
    QueryProgramResourceiv(programObject, programInterface, {index}, propCount, props, bufSize,
                           length, params);
}

void Context::getProgramInterfaceiv(ShaderProgramID program,
                                    GLenum programInterface,
                                    GLenum pname,
                                    GLint *params)
{
    const Program *programObject = getProgramResolveLink(program);
    QueryProgramInterfaceiv(programObject, programInterface, pname, params);
}

void Context::getProgramInterfaceivRobust(ShaderProgramID program,
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

void Context::validationError(angle::EntryPoint entryPoint,
                              GLenum errorCode,
                              const char *message) const
{
    const_cast<Context *>(this)->mErrors.validationError(entryPoint, errorCode, message);
}

void Context::validationErrorF(angle::EntryPoint entryPoint,
                               GLenum errorCode,
                               const char *format,
                               ...) const
{
    va_list vargs;
    va_start(vargs, format);
    constexpr size_t kMessageSize = 256;
    char message[kMessageSize];
    int r = vsnprintf(message, kMessageSize, format, vargs);
    va_end(vargs);

    if (r > 0)
    {
        validationError(entryPoint, errorCode, message);
    }
    else
    {
        validationError(entryPoint, errorCode, format);
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
    setContextLost();
}

void Context::setContextLost()
{
    mContextLost = true;

    // Stop skipping validation, since many implementation entrypoint assume they can't
    // be called when lost, or with null object arguments, etc.
    mSkipValidation = false;

    // Make sure we update TLS.
#if defined(ANGLE_PLATFORM_APPLE)
    SetCurrentValidContextTLS(nullptr);
#else
    gCurrentValidContext = nullptr;
#endif
}

GLenum Context::getGraphicsResetStatus()
{
    // Even if the application doesn't want to know about resets, we want to know
    // as it will allow us to skip all the calls.
    if (mResetStrategy == GL_NO_RESET_NOTIFICATION_EXT)
    {
        if (!isContextLost() && mImplementation->getResetStatus() != GraphicsResetStatus::NoError)
        {
            setContextLost();
        }

        // EXT_robustness, section 2.6: If the reset notification behavior is
        // NO_RESET_NOTIFICATION_EXT, then the implementation will never deliver notification of
        // reset events, and GetGraphicsResetStatusEXT will always return NO_ERROR.
        return GL_NO_ERROR;
    }

    // The GL_EXT_robustness spec says that if a reset is encountered, a reset
    // status should be returned at least once, and GL_NO_ERROR should be returned
    // once the device has finished resetting.
    if (!isContextLost())
    {
        ASSERT(mResetStatus == GraphicsResetStatus::NoError);
        mResetStatus = mImplementation->getResetStatus();

        if (mResetStatus != GraphicsResetStatus::NoError)
        {
            setContextLost();
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

bool Context::isResetNotificationEnabled() const
{
    return (mResetStrategy == GL_LOSE_CONTEXT_ON_RESET_EXT);
}

bool Context::isRobustnessEnabled() const
{
    return mState.hasRobustAccess();
}

const egl::Config *Context::getConfig() const
{
    return mConfig;
}

EGLenum Context::getClientType() const
{
    return mState.getClientType();
}

EGLenum Context::getRenderBuffer() const
{
    const Framebuffer *framebuffer =
        mState.mFramebufferManager->getFramebuffer(Framebuffer::kDefaultDrawFramebufferHandle);
    if (framebuffer == nullptr)
    {
        return EGL_NONE;
    }

    const FramebufferAttachment *backAttachment = framebuffer->getAttachment(this, GL_BACK);
    ASSERT(backAttachment != nullptr);
    return backAttachment->getSurface()->getRenderBuffer();
}

VertexArray *Context::checkVertexArrayAllocation(VertexArrayID vertexArrayHandle)
{
    // Only called after a prior call to Gen.
    VertexArray *vertexArray = getVertexArray(vertexArrayHandle);
    if (!vertexArray)
    {
        vertexArray =
            new VertexArray(mImplementation.get(), vertexArrayHandle,
                            mState.mCaps.maxVertexAttributes, mState.mCaps.maxVertexAttribBindings);
        vertexArray->setBufferAccessValidationEnabled(mBufferAccessValidationEnabled);

        mVertexArrayMap.assign(vertexArrayHandle, vertexArray);
    }

    return vertexArray;
}

TransformFeedback *Context::checkTransformFeedbackAllocation(
    TransformFeedbackID transformFeedbackHandle)
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

bool Context::isVertexArrayGenerated(VertexArrayID vertexArray) const
{
    ASSERT(mVertexArrayMap.contains({0}));
    return mVertexArrayMap.contains(vertexArray);
}

bool Context::isTransformFeedbackGenerated(TransformFeedbackID transformFeedback) const
{
    ASSERT(mTransformFeedbackMap.contains({0}));
    return mTransformFeedbackMap.contains(transformFeedback);
}

void Context::detachTexture(TextureID texture)
{
    // The State cannot unbind image observers itself, they are owned by the Context
    Texture *tex = mState.mTextureManager->getTexture(texture);
    for (auto &imageBinding : mImageObserverBindings)
    {
        if (imageBinding.getSubject() == tex)
        {
            imageBinding.reset();
        }
    }

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

void Context::detachFramebuffer(FramebufferID framebuffer)
{
    // Framebuffer detachment is handled by Context, because 0 is a valid
    // Framebuffer object, and a pointer to it must be passed from Context
    // to State at binding time.

    // [OpenGL ES 2.0.24] section 4.4 page 107:
    // If a framebuffer that is currently bound to the target FRAMEBUFFER is deleted, it is as
    // though BindFramebuffer had been executed with the target of FRAMEBUFFER and framebuffer of
    // zero.

    if (mState.removeReadFramebufferBinding(framebuffer) && framebuffer.value != 0)
    {
        bindReadFramebuffer({0});
    }

    if (mState.removeDrawFramebufferBinding(framebuffer) && framebuffer.value != 0)
    {
        bindDrawFramebuffer({0});
    }
}

void Context::detachRenderbuffer(RenderbufferID renderbuffer)
{
    mState.detachRenderbuffer(this, renderbuffer);
}

void Context::detachVertexArray(VertexArrayID vertexArray)
{
    // Vertex array detachment is handled by Context, because 0 is a valid
    // VAO, and a pointer to it must be passed from Context to State at
    // binding time.

    // [OpenGL ES 3.0.2] section 2.10 page 43:
    // If a vertex array object that is currently bound is deleted, the binding
    // for that object reverts to zero and the default vertex array becomes current.
    if (mState.removeVertexArrayBinding(this, vertexArray))
    {
        bindVertexArray({0});
    }
}

void Context::detachTransformFeedback(TransformFeedbackID transformFeedback)
{
    // Transform feedback detachment is handled by Context, because 0 is a valid
    // transform feedback, and a pointer to it must be passed from Context to State at
    // binding time.

    // The OpenGL specification doesn't mention what should happen when the currently bound
    // transform feedback object is deleted. Since it is a container object, we treat it like
    // VAOs and FBOs and set the current bound transform feedback back to 0.
    if (mState.removeTransformFeedbackBinding(this, transformFeedback))
    {
        bindTransformFeedback(GL_TRANSFORM_FEEDBACK, {0});
        mStateCache.onActiveTransformFeedbackChange(this);
    }
}

void Context::detachSampler(SamplerID sampler)
{
    mState.detachSampler(this, sampler);
}

void Context::detachProgramPipeline(ProgramPipelineID pipeline)
{
    mState.detachProgramPipeline(this, pipeline);
}

void Context::vertexAttribDivisor(GLuint index, GLuint divisor)
{
    mState.setVertexAttribDivisor(this, index, divisor);
    mStateCache.onVertexArrayStateChange(this);
}

void Context::samplerParameteri(SamplerID sampler, GLenum pname, GLint param)
{
    Sampler *const samplerObject =
        mState.mSamplerManager->checkSamplerAllocation(mImplementation.get(), sampler);
    SetSamplerParameteri(this, samplerObject, pname, param);
}

void Context::samplerParameteriv(SamplerID sampler, GLenum pname, const GLint *param)
{
    Sampler *const samplerObject =
        mState.mSamplerManager->checkSamplerAllocation(mImplementation.get(), sampler);
    SetSamplerParameteriv(this, samplerObject, pname, param);
}

void Context::samplerParameterIiv(SamplerID sampler, GLenum pname, const GLint *param)
{
    Sampler *const samplerObject =
        mState.mSamplerManager->checkSamplerAllocation(mImplementation.get(), sampler);
    SetSamplerParameterIiv(this, samplerObject, pname, param);
}

void Context::samplerParameterIuiv(SamplerID sampler, GLenum pname, const GLuint *param)
{
    Sampler *const samplerObject =
        mState.mSamplerManager->checkSamplerAllocation(mImplementation.get(), sampler);
    SetSamplerParameterIuiv(this, samplerObject, pname, param);
}

void Context::samplerParameterivRobust(SamplerID sampler,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       const GLint *param)
{
    samplerParameteriv(sampler, pname, param);
}

void Context::samplerParameterIivRobust(SamplerID sampler,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        const GLint *param)
{
    UNIMPLEMENTED();
}

void Context::samplerParameterIuivRobust(SamplerID sampler,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         const GLuint *param)
{
    UNIMPLEMENTED();
}

void Context::samplerParameterf(SamplerID sampler, GLenum pname, GLfloat param)
{
    Sampler *const samplerObject =
        mState.mSamplerManager->checkSamplerAllocation(mImplementation.get(), sampler);
    SetSamplerParameterf(this, samplerObject, pname, param);
}

void Context::samplerParameterfv(SamplerID sampler, GLenum pname, const GLfloat *param)
{
    Sampler *const samplerObject =
        mState.mSamplerManager->checkSamplerAllocation(mImplementation.get(), sampler);
    SetSamplerParameterfv(this, samplerObject, pname, param);
}

void Context::samplerParameterfvRobust(SamplerID sampler,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       const GLfloat *param)
{
    samplerParameterfv(sampler, pname, param);
}

void Context::getSamplerParameteriv(SamplerID sampler, GLenum pname, GLint *params)
{
    const Sampler *const samplerObject =
        mState.mSamplerManager->checkSamplerAllocation(mImplementation.get(), sampler);
    QuerySamplerParameteriv(samplerObject, pname, params);
}

void Context::getSamplerParameterIiv(SamplerID sampler, GLenum pname, GLint *params)
{
    const Sampler *const samplerObject =
        mState.mSamplerManager->checkSamplerAllocation(mImplementation.get(), sampler);
    QuerySamplerParameterIiv(samplerObject, pname, params);
}

void Context::getSamplerParameterIuiv(SamplerID sampler, GLenum pname, GLuint *params)
{
    const Sampler *const samplerObject =
        mState.mSamplerManager->checkSamplerAllocation(mImplementation.get(), sampler);
    QuerySamplerParameterIuiv(samplerObject, pname, params);
}

void Context::getSamplerParameterivRobust(SamplerID sampler,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLint *params)
{
    getSamplerParameteriv(sampler, pname, params);
}

void Context::getSamplerParameterIivRobust(SamplerID sampler,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLint *params)
{
    UNIMPLEMENTED();
}

void Context::getSamplerParameterIuivRobust(SamplerID sampler,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLuint *params)
{
    UNIMPLEMENTED();
}

void Context::getSamplerParameterfv(SamplerID sampler, GLenum pname, GLfloat *params)
{
    const Sampler *const samplerObject =
        mState.mSamplerManager->checkSamplerAllocation(mImplementation.get(), sampler);
    QuerySamplerParameterfv(samplerObject, pname, params);
}

void Context::getSamplerParameterfvRobust(SamplerID sampler,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLfloat *params)
{
    getSamplerParameterfv(sampler, pname, params);
}

void Context::programParameteri(ShaderProgramID program, GLenum pname, GLint value)
{
    gl::Program *programObject = getProgramResolveLink(program);
    SetProgramParameteri(programObject, pname, value);
}

void Context::initRendererString()
{
    std::ostringstream frontendRendererString;
    std::string vendorString(mDisplay->getBackendVendorString());
    std::string rendererString(mDisplay->getBackendRendererDescription());
    std::string versionString(mDisplay->getBackendVersionString(!isWebGL()));
    // Commas are used as a separator in ANGLE's renderer string, so remove commas from each
    // element.
    vendorString.erase(std::remove(vendorString.begin(), vendorString.end(), ','),
                       vendorString.end());
    rendererString.erase(std::remove(rendererString.begin(), rendererString.end(), ','),
                         rendererString.end());
    versionString.erase(std::remove(versionString.begin(), versionString.end(), ','),
                        versionString.end());
    frontendRendererString << "ANGLE (";
    frontendRendererString << vendorString;
    frontendRendererString << ", ";
    frontendRendererString << rendererString;
    frontendRendererString << ", ";
    frontendRendererString << versionString;
    frontendRendererString << ")";

    mRendererString = MakeStaticString(frontendRendererString.str());
}

void Context::initVersionStrings()
{
    const Version &clientVersion = getClientVersion();

    std::ostringstream versionString;
    if (getClientType() == EGL_OPENGL_ES_API)
    {
        versionString << "OpenGL ES ";
    }
    versionString << clientVersion.major << "." << clientVersion.minor << ".0 (ANGLE "
                  << angle::GetANGLEVersionString() << ")";
    mVersionString = MakeStaticString(versionString.str());

    std::ostringstream shadingLanguageVersionString;
    if (getClientType() == EGL_OPENGL_ES_API)
    {
        shadingLanguageVersionString << "OpenGL ES GLSL ES ";
    }
    else
    {
        ASSERT(getClientType() == EGL_OPENGL_API);
        shadingLanguageVersionString << "OpenGL GLSL ";
    }
    shadingLanguageVersionString << (clientVersion.major == 2 ? 1 : clientVersion.major) << "."
                                 << clientVersion.minor << "0 (ANGLE "
                                 << angle::GetANGLEVersionString() << ")";
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

const GLubyte *Context::getString(GLenum name)
{
    return static_cast<const Context *>(this)->getString(name);
}

const GLubyte *Context::getStringi(GLenum name, GLuint index)
{
    return static_cast<const Context *>(this)->getStringi(name, index);
}

const GLubyte *Context::getString(GLenum name) const
{
    switch (name)
    {
        case GL_VENDOR:
            return reinterpret_cast<const GLubyte *>(mDisplay->getVendorString().c_str());

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

        case GL_SERIALIZED_CONTEXT_STRING_ANGLE:
            if (angle::SerializeContextToString(this, &mCachedSerializedStateString) ==
                angle::Result::Continue)
            {
                return reinterpret_cast<const GLubyte *>(mCachedSerializedStateString.c_str());
            }
            else
            {
                return nullptr;
            }

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

bool Context::isExtensionRequestable(const char *name) const
{
    const ExtensionInfoMap &extensionInfos = GetExtensionInfoMap();
    auto extension                         = extensionInfos.find(name);

    return extension != extensionInfos.end() && extension->second.Requestable &&
           mSupportedExtensions.*(extension->second.ExtensionsMember);
}

bool Context::isExtensionDisablable(const char *name) const
{
    const ExtensionInfoMap &extensionInfos = GetExtensionInfoMap();
    auto extension                         = extensionInfos.find(name);

    return extension != extensionInfos.end() && extension->second.Disablable &&
           mSupportedExtensions.*(extension->second.ExtensionsMember);
}

void Context::requestExtension(const char *name)
{
    setExtensionEnabled(name, true);
}
void Context::disableExtension(const char *name)
{
    setExtensionEnabled(name, false);
}

void Context::setExtensionEnabled(const char *name, bool enabled)
{
    // OVR_multiview is implicitly enabled when OVR_multiview2 is enabled
    if (strcmp(name, "GL_OVR_multiview2") == 0)
    {
        setExtensionEnabled("GL_OVR_multiview", enabled);
    }
    const ExtensionInfoMap &extensionInfos = GetExtensionInfoMap();
    ASSERT(extensionInfos.find(name) != extensionInfos.end());
    const auto &extension = extensionInfos.at(name);
    ASSERT(extension.Requestable);
    ASSERT(isExtensionRequestable(name));

    if (mState.mExtensions.*(extension.ExtensionsMember) == enabled)
    {
        // No change
        return;
    }

    mState.mExtensions.*(extension.ExtensionsMember) = enabled;

    reinitializeAfterExtensionsChanged();
}

void Context::reinitializeAfterExtensionsChanged()
{
    updateCaps();
    initExtensionStrings();

    // Release the shader compiler so it will be re-created with the requested extensions enabled.
    releaseShaderCompiler();

    // Invalidate all textures and framebuffer. Some extensions make new formats renderable or
    // sampleable.
    mState.mTextureManager->signalAllTexturesDirty();
    for (auto &zeroTexture : mZeroTextures)
    {
        if (zeroTexture.get() != nullptr)
        {
            zeroTexture->signalDirtyStorage(InitState::Initialized);
        }
    }

    mState.mFramebufferManager->invalidateFramebufferCompletenessCache();
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

    // TODO: http://anglebug.com/7232: Handle PPOs
    ANGLE_CONTEXT_TRY(transformFeedback->begin(this, primitiveMode, mState.getProgram()));
    mStateCache.onActiveTransformFeedbackChange(this);
}

bool Context::hasActiveTransformFeedback(ShaderProgramID program) const
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

    if (getClientVersion() < ES_2_0)
    {
        // Default extensions for GLES1
        supportedExtensions.pointSizeArrayOES        = true;
        supportedExtensions.textureCubeMapOES        = true;
        supportedExtensions.pointSpriteOES           = true;
        supportedExtensions.drawTextureOES           = true;
        supportedExtensions.framebufferObjectOES     = true;
        supportedExtensions.parallelShaderCompileKHR = false;
        supportedExtensions.texture3DOES             = false;
        supportedExtensions.clipDistanceAPPLE        = false;
    }

    if (getClientVersion() < ES_3_0)
    {
        // Disable ES3+ extensions
        supportedExtensions.colorBufferFloatEXT          = false;
        supportedExtensions.EGLImageExternalEssl3OES     = false;
        supportedExtensions.multiviewOVR                 = false;
        supportedExtensions.multiview2OVR                = false;
        supportedExtensions.multiviewMultisampleANGLE    = false;
        supportedExtensions.copyTexture3dANGLE           = false;
        supportedExtensions.textureMultisampleANGLE      = false;
        supportedExtensions.drawBuffersIndexedEXT        = false;
        supportedExtensions.drawBuffersIndexedOES        = false;
        supportedExtensions.EGLImageArrayEXT             = false;
        supportedExtensions.textureFormatSRGBOverrideEXT = false;

        // Support GL_EXT_texture_norm16 on non-WebGL ES2 contexts. This is needed for R16/RG16
        // texturing for HDR video playback in Chromium which uses ES2 for compositor contexts.
        // Remove this workaround after Chromium migrates to ES3 for compositor contexts.
        if (mWebGLContext || getClientVersion() < ES_2_0)
        {
            supportedExtensions.textureNorm16EXT = false;
        }

        // Requires immutable textures
        supportedExtensions.yuvInternalFormatANGLE = false;

        // Require ESSL 3.0
        supportedExtensions.shaderMultisampleInterpolationOES  = false;
        supportedExtensions.shaderNoperspectiveInterpolationNV = false;
        supportedExtensions.sampleVariablesOES                 = false;

        // Require ES 3.1 but could likely be exposed on 3.0
        supportedExtensions.textureCubeMapArrayEXT = false;
        supportedExtensions.textureCubeMapArrayOES = false;

        // Require RED and RG formats
        supportedExtensions.textureSRGBR8EXT  = false;
        supportedExtensions.textureSRGBRG8EXT = false;

        // Requires glCompressedTexImage3D
        supportedExtensions.textureCompressionAstcOES = false;

        // Don't expose GL_EXT_texture_sRGB_decode without sRGB texture support
        if (!supportedExtensions.sRGBEXT)
        {
            supportedExtensions.textureSRGBDecodeEXT = false;
        }

        // Don't expose GL_OES_texture_float_linear without full legacy float texture support
        // The renderer may report OES_texture_float_linear without OES_texture_float
        // This is valid in a GLES 3.0 context, but not in a GLES 2.0 context
        if (!(supportedExtensions.textureFloatOES && supportedExtensions.textureHalfFloatOES))
        {
            supportedExtensions.textureFloatLinearOES     = false;
            supportedExtensions.textureHalfFloatLinearOES = false;
        }

        // Because of the difference in the SNORM to FLOAT conversion formula
        // between GLES 2.0 and 3.0, vertex type 10_10_10_2 is disabled
        // when the context version is lower than 3.0
        supportedExtensions.vertexType1010102OES = false;

        // GL_EXT_EGL_image_storage requires ESSL3
        supportedExtensions.EGLImageStorageEXT = false;

        // GL_EXT_YUV_target requires ESSL3
        supportedExtensions.YUVTargetEXT = false;

        // GL_EXT_clip_cull_distance requires ESSL3
        supportedExtensions.clipCullDistanceEXT = false;

        // ANGLE_shader_pixel_local_storage requires ES3
        supportedExtensions.shaderPixelLocalStorageANGLE         = false;
        supportedExtensions.shaderPixelLocalStorageCoherentANGLE = false;
    }

    if (getClientVersion() < ES_3_1)
    {
        // Disable ES3.1+ extensions
        supportedExtensions.geometryShaderEXT       = false;
        supportedExtensions.geometryShaderOES       = false;
        supportedExtensions.gpuShader5EXT           = false;
        supportedExtensions.primitiveBoundingBoxEXT = false;
        supportedExtensions.shaderImageAtomicOES    = false;
        supportedExtensions.shaderIoBlocksEXT       = false;
        supportedExtensions.shaderIoBlocksOES       = false;
        supportedExtensions.tessellationShaderEXT   = false;
        supportedExtensions.textureBufferEXT        = false;
        supportedExtensions.textureBufferOES        = false;

        // TODO(http://anglebug.com/2775): Multisample arrays could be supported on ES 3.0 as well
        // once 2D multisample texture extension is exposed there.
        supportedExtensions.textureStorageMultisample2dArrayOES = false;
    }

    if (getClientVersion() > ES_2_0)
    {
        // FIXME(geofflang): Don't support EXT_sRGB in non-ES2 contexts
        // supportedExtensions.sRGB = false;

        // If colorBufferFloatEXT is disabled but colorBufferHalfFloatEXT is enabled, then we will
        // expose some floating-point formats as color buffer targets but reject blits between
        // fixed-point and floating-point formats (this behavior is only enabled in
        // colorBufferFloatEXT, and must be rejected if only colorBufferHalfFloatEXT is enabled).
        // dEQP does not check for this, and will assume that floating-point and fixed-point formats
        // can be blit onto each other if the format is available.
        // We require colorBufferFloatEXT to be present in order to enable colorBufferHalfFloatEXT,
        // so that blitting is always allowed if the requested formats are exposed and have the
        // correct feature capabilities. WebGL 2 wants to support colorBufferHalfFloatEXT without
        // colorBufferFloatEXT.
        if (!supportedExtensions.colorBufferFloatEXT && !mWebGLContext)
        {
            supportedExtensions.colorBufferHalfFloatEXT = false;
        }

        // Disable support for CHROMIUM_color_buffer_float_rgb[a] in ES 3.0+, these extensions are
        // non-conformant in ES 3.0 and superseded by EXT_color_buffer_float.
        supportedExtensions.colorBufferFloatRgbCHROMIUM  = false;
        supportedExtensions.colorBufferFloatRgbaCHROMIUM = false;
    }

    if (getFrontendFeatures().disableDrawBuffersIndexed.enabled)
    {
        supportedExtensions.drawBuffersIndexedEXT = false;
        supportedExtensions.drawBuffersIndexedOES = false;
    }

    if (getFrontendFeatures().disableAnisotropicFiltering.enabled)
    {
        supportedExtensions.textureFilterAnisotropicEXT = false;
    }

    if (!getFrontendFeatures().emulatePixelLocalStorage.enabled)
    {
        supportedExtensions.shaderPixelLocalStorageANGLE         = false;
        supportedExtensions.shaderPixelLocalStorageCoherentANGLE = false;
    }

    // Some extensions are always available because they are implemented in the GL layer.
    supportedExtensions.bindUniformLocationCHROMIUM   = true;
    supportedExtensions.vertexArrayObjectOES          = true;
    supportedExtensions.bindGeneratesResourceCHROMIUM = true;
    supportedExtensions.clientArraysANGLE             = true;
    supportedExtensions.requestExtensionANGLE         = true;
    supportedExtensions.multiDrawANGLE                = true;

    // Enable the no error extension if the context was created with the flag.
    supportedExtensions.noErrorKHR = mSkipValidation;

    // Enable surfaceless to advertise we'll have the correct behavior when there is no default FBO
    supportedExtensions.surfacelessContextOES = mSurfacelessSupported;

    // Explicitly enable GL_KHR_debug
    supportedExtensions.debugKHR = true;

    // Explicitly enable GL_EXT_debug_label
    supportedExtensions.debugLabelEXT = true;

    // Explicitly enable GL_ANGLE_robust_client_memory if the context supports validation.
    supportedExtensions.robustClientMemoryANGLE = !mSkipValidation;

    // Determine robust resource init availability from EGL.
    supportedExtensions.robustResourceInitializationANGLE = mState.isRobustResourceInitEnabled();

    // mState.mExtensions.robustBufferAccessBehaviorKHR is true only if robust access is true and
    // the backend supports it.
    supportedExtensions.robustBufferAccessBehaviorKHR =
        mState.hasRobustAccess() && supportedExtensions.robustBufferAccessBehaviorKHR;

    // Enable the cache control query unconditionally.
    supportedExtensions.programCacheControlANGLE = true;

    // If EGL_KHR_fence_sync is not enabled, don't expose GL_OES_EGL_sync.
    ASSERT(mDisplay);
    if (!mDisplay->getExtensions().fenceSync)
    {
        supportedExtensions.EGLSyncOES = false;
    }

    if (mDisplay->getExtensions().robustnessVideoMemoryPurgeNV)
    {
        supportedExtensions.robustnessVideoMemoryPurgeNV = true;
    }

    supportedExtensions.memorySizeANGLE = true;

    // GL_CHROMIUM_lose_context is implemented in the frontend
    supportedExtensions.loseContextCHROMIUM = true;

    // The ASTC texture extensions have dependency requirements.
    if (supportedExtensions.textureCompressionAstcHdrKHR ||
        supportedExtensions.textureCompressionAstcSliced3dKHR)
    {
        // GL_KHR_texture_compression_astc_hdr cannot be exposed without also exposing
        // GL_KHR_texture_compression_astc_ldr
        ASSERT(supportedExtensions.textureCompressionAstcLdrKHR);
    }

    if (supportedExtensions.textureCompressionAstcOES)
    {
        // GL_OES_texture_compression_astc cannot be exposed without also exposing
        // GL_KHR_texture_compression_astc_ldr and GL_KHR_texture_compression_astc_hdr
        ASSERT(supportedExtensions.textureCompressionAstcLdrKHR);
        ASSERT(supportedExtensions.textureCompressionAstcHdrKHR);
    }

    // GL_KHR_protected_textures
    // If EGL_KHR_protected_content is not supported then GL_EXT_protected_texture
    // can not be supported.
    if (!mDisplay->getExtensions().protectedContentEXT)
    {
        supportedExtensions.protectedTexturesEXT = false;
    }

    // GL_ANGLE_get_tex_level_parameter is implemented in the front-end
    supportedExtensions.getTexLevelParameterANGLE = true;

    // Always enabled. Will return a default string if capture is not enabled.
    supportedExtensions.getSerializedContextStringANGLE = true;

    // Performance counter queries are always supported. Different groups exist on each back-end.
    supportedExtensions.performanceMonitorAMD = true;

    // GL_ANDROID_extension_pack_es31a
    supportedExtensions.extensionPackEs31aANDROID =
        CanSupportAEP(getClientVersion(), supportedExtensions);

    return supportedExtensions;
}

void Context::initCaps()
{
    mState.mCaps = mImplementation->getNativeCaps();

    // TODO (http://anglebug.com/6010): mSupportedExtensions should not be modified here
    mSupportedExtensions = generateSupportedExtensions();

    if (!mDisplay->getFrontendFeatures().allowCompressedFormats.enabled)
    {
        INFO() << "Limiting compressed format support.\n";

        mSupportedExtensions.compressedEACR11SignedTextureOES                = false;
        mSupportedExtensions.compressedEACR11UnsignedTextureOES              = false;
        mSupportedExtensions.compressedEACRG11SignedTextureOES               = false;
        mSupportedExtensions.compressedEACRG11UnsignedTextureOES             = false;
        mSupportedExtensions.compressedETC1RGB8SubTextureEXT                 = false;
        mSupportedExtensions.compressedETC1RGB8TextureOES                    = false;
        mSupportedExtensions.compressedETC2PunchthroughARGBA8TextureOES      = false;
        mSupportedExtensions.compressedETC2PunchthroughASRGB8AlphaTextureOES = false;
        mSupportedExtensions.compressedETC2RGB8TextureOES                    = false;
        mSupportedExtensions.compressedETC2RGBA8TextureOES                   = false;
        mSupportedExtensions.compressedETC2SRGB8Alpha8TextureOES             = false;
        mSupportedExtensions.compressedETC2SRGB8TextureOES                   = false;
        mSupportedExtensions.compressedTextureEtcANGLE                       = false;
        mSupportedExtensions.textureCompressionPvrtcIMG                      = false;
        mSupportedExtensions.pvrtcSRGBEXT                                    = false;
        mSupportedExtensions.copyCompressedTextureCHROMIUM                   = false;
        mSupportedExtensions.textureCompressionAstcHdrKHR                    = false;
        mSupportedExtensions.textureCompressionAstcLdrKHR                    = false;
        mSupportedExtensions.textureCompressionAstcOES                       = false;
        mSupportedExtensions.textureCompressionBptcEXT                       = false;
        mSupportedExtensions.textureCompressionDxt1EXT                       = false;
        mSupportedExtensions.textureCompressionDxt3ANGLE                     = false;
        mSupportedExtensions.textureCompressionDxt5ANGLE                     = false;
        mSupportedExtensions.textureCompressionRgtcEXT                       = false;
        mSupportedExtensions.textureCompressionS3tcSrgbEXT                   = false;
        mSupportedExtensions.textureCompressionAstcSliced3dKHR               = false;
        mSupportedExtensions.textureFilteringHintCHROMIUM                    = false;

        mState.mCaps.compressedTextureFormats.clear();
    }

    mState.mExtensions = mSupportedExtensions;

    mState.mLimitations = mImplementation->getNativeLimitations();

    // GLES1 emulation: Initialize caps (Table 6.20 / 6.22 in the ES 1.1 spec)
    if (getClientType() == EGL_OPENGL_API || getClientVersion() < Version(2, 0))
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

    mState.mCaps.maxDebugMessageLength   = 1024;
    mState.mCaps.maxDebugLoggedMessages  = 1024;
    mState.mCaps.maxDebugGroupStackDepth = 1024;
    mState.mCaps.maxLabelLength          = 1024;

    if (getClientVersion() < Version(3, 0))
    {
        mState.mCaps.maxViews = 1u;
    }

#if 0
// This logging can generate a lot of spam in test suites that create many contexts
#    define ANGLE_LOG_LIMITED_CAP(cap, limit)                                               \
        INFO() << "Limiting " << #cap << " to implementation limit " << (limit) << " (was " \
               << (cap) << ")."
#else
#    define ANGLE_LOG_LIMITED_CAP(cap, limit)
#endif

#define ANGLE_LIMIT_CAP(cap, limit)            \
    do                                         \
    {                                          \
        if ((cap) > (limit))                   \
        {                                      \
            ANGLE_LOG_LIMITED_CAP(cap, limit); \
            (cap) = (limit);                   \
        }                                      \
    } while (0)

    // Apply/Verify implementation limits
    ANGLE_LIMIT_CAP(mState.mCaps.maxDrawBuffers, IMPLEMENTATION_MAX_DRAW_BUFFERS);
    ANGLE_LIMIT_CAP(mState.mCaps.maxColorAttachments, IMPLEMENTATION_MAX_DRAW_BUFFERS);
    ANGLE_LIMIT_CAP(mState.mCaps.maxVertexAttributes, MAX_VERTEX_ATTRIBS);
    ANGLE_LIMIT_CAP(mState.mCaps.maxVertexAttribStride,
                    static_cast<GLint>(limits::kMaxVertexAttribStride));

    ASSERT(mState.mCaps.minAliasedPointSize >= 1.0f);

    if (getClientVersion() < ES_3_1)
    {
        mState.mCaps.maxVertexAttribBindings = mState.mCaps.maxVertexAttributes;
    }
    else
    {
        ANGLE_LIMIT_CAP(mState.mCaps.maxVertexAttribBindings, MAX_VERTEX_ATTRIB_BINDINGS);
    }

    if (mWebGLContext && getLimitations().limitWebglMaxTextureSizeTo4096)
    {
        constexpr GLint kMaxTextureSize = 4096;
        ANGLE_LIMIT_CAP(mState.mCaps.max2DTextureSize, kMaxTextureSize);
        ANGLE_LIMIT_CAP(mState.mCaps.max3DTextureSize, kMaxTextureSize);
        ANGLE_LIMIT_CAP(mState.mCaps.maxCubeMapTextureSize, kMaxTextureSize);
        ANGLE_LIMIT_CAP(mState.mCaps.maxArrayTextureLayers, kMaxTextureSize);
        ANGLE_LIMIT_CAP(mState.mCaps.maxRectangleTextureSize, kMaxTextureSize);
    }

    ANGLE_LIMIT_CAP(mState.mCaps.max2DTextureSize, IMPLEMENTATION_MAX_2D_TEXTURE_SIZE);
    ANGLE_LIMIT_CAP(mState.mCaps.maxCubeMapTextureSize, IMPLEMENTATION_MAX_CUBE_MAP_TEXTURE_SIZE);
    ANGLE_LIMIT_CAP(mState.mCaps.max3DTextureSize, IMPLEMENTATION_MAX_3D_TEXTURE_SIZE);
    ANGLE_LIMIT_CAP(mState.mCaps.maxArrayTextureLayers, IMPLEMENTATION_MAX_2D_ARRAY_TEXTURE_LAYERS);
    ANGLE_LIMIT_CAP(mState.mCaps.maxRectangleTextureSize, IMPLEMENTATION_MAX_2D_TEXTURE_SIZE);

    ANGLE_LIMIT_CAP(mState.mCaps.maxShaderUniformBlocks[ShaderType::Vertex],
                    IMPLEMENTATION_MAX_VERTEX_SHADER_UNIFORM_BUFFERS);
    ANGLE_LIMIT_CAP(mState.mCaps.maxShaderUniformBlocks[ShaderType::Geometry],
                    IMPLEMENTATION_MAX_GEOMETRY_SHADER_UNIFORM_BUFFERS);
    ANGLE_LIMIT_CAP(mState.mCaps.maxShaderUniformBlocks[ShaderType::Fragment],
                    IMPLEMENTATION_MAX_FRAGMENT_SHADER_UNIFORM_BUFFERS);
    ANGLE_LIMIT_CAP(mState.mCaps.maxShaderUniformBlocks[ShaderType::Compute],
                    IMPLEMENTATION_MAX_COMPUTE_SHADER_UNIFORM_BUFFERS);
    ANGLE_LIMIT_CAP(mState.mCaps.maxCombinedUniformBlocks,
                    IMPLEMENTATION_MAX_COMBINED_SHADER_UNIFORM_BUFFERS);
    ANGLE_LIMIT_CAP(mState.mCaps.maxUniformBufferBindings,
                    IMPLEMENTATION_MAX_UNIFORM_BUFFER_BINDINGS);

    ANGLE_LIMIT_CAP(mState.mCaps.maxVertexOutputComponents, IMPLEMENTATION_MAX_VARYING_VECTORS * 4);
    ANGLE_LIMIT_CAP(mState.mCaps.maxFragmentInputComponents,
                    IMPLEMENTATION_MAX_VARYING_VECTORS * 4);

    ANGLE_LIMIT_CAP(mState.mCaps.maxTransformFeedbackInterleavedComponents,
                    IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS);
    ANGLE_LIMIT_CAP(mState.mCaps.maxTransformFeedbackSeparateAttributes,
                    IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS);
    ANGLE_LIMIT_CAP(mState.mCaps.maxTransformFeedbackSeparateComponents,
                    IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS);

    if (getClientVersion() < ES_3_2 && !mState.mExtensions.tessellationShaderEXT)
    {
        ANGLE_LIMIT_CAP(mState.mCaps.maxCombinedTextureImageUnits,
                        IMPLEMENTATION_MAX_ES31_ACTIVE_TEXTURES);
    }
    else
    {
        ANGLE_LIMIT_CAP(mState.mCaps.maxCombinedTextureImageUnits,
                        IMPLEMENTATION_MAX_ACTIVE_TEXTURES);
    }

    for (ShaderType shaderType : AllShaderTypes())
    {
        ANGLE_LIMIT_CAP(mState.mCaps.maxShaderTextureImageUnits[shaderType],
                        IMPLEMENTATION_MAX_SHADER_TEXTURES);
    }

    ANGLE_LIMIT_CAP(mState.mCaps.maxImageUnits, IMPLEMENTATION_MAX_IMAGE_UNITS);
    ANGLE_LIMIT_CAP(mState.mCaps.maxCombinedImageUniforms, IMPLEMENTATION_MAX_IMAGE_UNITS);
    for (ShaderType shaderType : AllShaderTypes())
    {
        ANGLE_LIMIT_CAP(mState.mCaps.maxShaderImageUniforms[shaderType],
                        IMPLEMENTATION_MAX_IMAGE_UNITS);
    }

    for (ShaderType shaderType : AllShaderTypes())
    {
        ANGLE_LIMIT_CAP(mState.mCaps.maxShaderAtomicCounterBuffers[shaderType],
                        IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS);
    }
    ANGLE_LIMIT_CAP(mState.mCaps.maxAtomicCounterBufferBindings,
                    IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS);
    ANGLE_LIMIT_CAP(mState.mCaps.maxCombinedAtomicCounterBuffers,
                    IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS);

    for (ShaderType shaderType : AllShaderTypes())
    {
        ANGLE_LIMIT_CAP(mState.mCaps.maxShaderStorageBlocks[shaderType],
                        IMPLEMENTATION_MAX_SHADER_STORAGE_BUFFER_BINDINGS);
    }
    ANGLE_LIMIT_CAP(mState.mCaps.maxShaderStorageBufferBindings,
                    IMPLEMENTATION_MAX_SHADER_STORAGE_BUFFER_BINDINGS);
    ANGLE_LIMIT_CAP(mState.mCaps.maxCombinedShaderStorageBlocks,
                    IMPLEMENTATION_MAX_SHADER_STORAGE_BUFFER_BINDINGS);

    ANGLE_LIMIT_CAP(mState.mCaps.maxClipDistances, IMPLEMENTATION_MAX_CLIP_DISTANCES);

    ANGLE_LIMIT_CAP(mState.mCaps.maxFramebufferLayers, IMPLEMENTATION_MAX_FRAMEBUFFER_LAYERS);

    ANGLE_LIMIT_CAP(mState.mCaps.maxSampleMaskWords, IMPLEMENTATION_MAX_SAMPLE_MASK_WORDS);
    ANGLE_LIMIT_CAP(mState.mCaps.maxSamples, IMPLEMENTATION_MAX_SAMPLES);
    ANGLE_LIMIT_CAP(mState.mCaps.maxFramebufferSamples, IMPLEMENTATION_MAX_SAMPLES);
    ANGLE_LIMIT_CAP(mState.mCaps.maxColorTextureSamples, IMPLEMENTATION_MAX_SAMPLES);
    ANGLE_LIMIT_CAP(mState.mCaps.maxDepthTextureSamples, IMPLEMENTATION_MAX_SAMPLES);
    ANGLE_LIMIT_CAP(mState.mCaps.maxIntegerSamples, IMPLEMENTATION_MAX_SAMPLES);

    ANGLE_LIMIT_CAP(mState.mCaps.maxViews, IMPLEMENTATION_ANGLE_MULTIVIEW_MAX_VIEWS);

    ANGLE_LIMIT_CAP(mState.mCaps.maxDualSourceDrawBuffers,
                    IMPLEMENTATION_MAX_DUAL_SOURCE_DRAW_BUFFERS);

    // WebGL compatibility
    mState.mExtensions.webglCompatibilityANGLE = mWebGLContext;
    for (const auto &extensionInfo : GetExtensionInfoMap())
    {
        // If the user has requested that extensions start disabled and they are requestable,
        // disable them.
        if (!mExtensionsEnabled && extensionInfo.second.Requestable)
        {
            mState.mExtensions.*(extensionInfo.second.ExtensionsMember) = false;
        }
    }

    // Hide emulated ETC1 extension from WebGL contexts.
    if (mWebGLContext && getLimitations().emulatedEtc1)
    {
        mSupportedExtensions.compressedETC1RGB8SubTextureEXT = false;
        mSupportedExtensions.compressedETC1RGB8TextureOES    = false;
    }

    if (getLimitations().emulatedAstc)
    {
        // Hide emulated ASTC extension from WebGL contexts.
        if (mWebGLContext)
        {
            mSupportedExtensions.textureCompressionAstcLdrKHR = false;
            mState.mExtensions.textureCompressionAstcLdrKHR   = false;
        }
#if !defined(ANGLE_HAS_ASTCENC)
        // Don't expose emulated ASTC when it's not built.
        mSupportedExtensions.textureCompressionAstcLdrKHR = false;
        mState.mExtensions.textureCompressionAstcLdrKHR   = false;
#endif
    }

    // If we're capturing application calls for replay, apply some feature limits to increase
    // portability of the trace.
    if (getShareGroup()->getFrameCaptureShared()->enabled() ||
        getFrontendFeatures().enableCaptureLimits.enabled)
    {
        INFO() << "Limit some features because "
               << (getShareGroup()->getFrameCaptureShared()->enabled()
                       ? "FrameCapture is enabled"
                       : "FrameCapture limits were forced")
               << std::endl;

        if (!getFrontendFeatures().enableProgramBinaryForCapture.enabled)
        {
            // Some apps insist on being able to use glProgramBinary. For those, we'll allow the
            // extension to remain on. Otherwise, force the extension off.
            INFO() << "Disabling GL_OES_get_program_binary for trace portability";
            mDisplay->overrideFrontendFeatures({"disable_program_binary"}, true);
        }

        // Set to the most common limit per gpuinfo.org. Required for several platforms we test.
        constexpr GLint maxImageUnits = 8;
        INFO() << "Limiting image unit count to " << maxImageUnits;
        ANGLE_LIMIT_CAP(mState.mCaps.maxImageUnits, maxImageUnits);
        for (ShaderType shaderType : AllShaderTypes())
        {
            ANGLE_LIMIT_CAP(mState.mCaps.maxShaderImageUniforms[shaderType], maxImageUnits);
        }

        // Set a large uniform buffer offset alignment that works on multiple platforms.
        // The offset used by the trace needs to be divisible by the device's actual value.
        // Values seen during development: ARM (16), Intel (32), Qualcomm (128), Nvidia (256)
        constexpr GLint uniformBufferOffsetAlignment = 256;
        ASSERT(uniformBufferOffsetAlignment % mState.mCaps.uniformBufferOffsetAlignment == 0);
        INFO() << "Setting uniform buffer offset alignment to " << uniformBufferOffsetAlignment;
        mState.mCaps.uniformBufferOffsetAlignment = uniformBufferOffsetAlignment;

        // Also limit texture buffer offset alignment, if enabled
        if (mState.mExtensions.textureBufferAny())
        {
            constexpr GLint textureBufferOffsetAlignment =
                gl::limits::kMinTextureBufferOffsetAlignment;
            ASSERT(textureBufferOffsetAlignment % mState.mCaps.textureBufferOffsetAlignment == 0);
            INFO() << "Setting texture buffer offset alignment to " << textureBufferOffsetAlignment;
            mState.mCaps.textureBufferOffsetAlignment = textureBufferOffsetAlignment;
        }

        INFO() << "Disabling GL_EXT_map_buffer_range and GL_OES_mapbuffer during capture, which "
                  "are not supported on some native drivers";
        mState.mExtensions.mapBufferRangeEXT = false;
        mState.mExtensions.mapbufferOES      = false;

        INFO() << "Disabling GL_CHROMIUM_bind_uniform_location during capture, which is not "
                  "supported on native drivers";
        mState.mExtensions.bindUniformLocationCHROMIUM = false;

        INFO() << "Disabling GL_NV_shader_noperspective_interpolation during capture, which is not "
                  "supported on some native drivers";
        mState.mExtensions.shaderNoperspectiveInterpolationNV = false;

        INFO() << "Disabling GL_NV_framebuffer_blit during capture, which is not "
                  "supported on some native drivers";
        mState.mExtensions.framebufferBlitNV = false;

        // NVIDIA's Vulkan driver only supports 4 draw buffers
        constexpr GLint maxDrawBuffers = 4;
        INFO() << "Limiting draw buffer count to " << maxDrawBuffers;
        ANGLE_LIMIT_CAP(mState.mCaps.maxDrawBuffers, maxDrawBuffers);

        // Unity based applications are sending down GL streams with undefined behavior.
        // Disabling EGL_KHR_create_context_no_error (which enables a new EGL attrib) prevents that,
        // but we don't have the infrastructure for disabling EGL extensions yet.
        // Instead, disable GL_KHR_no_error (which disables exposing the GL extension), which
        // prevents writing invalid calls to the capture.
        INFO() << "Enabling validation to prevent invalid calls from being captured. This "
                  "effectively disables GL_KHR_no_error and enables GL_ANGLE_robust_client_memory.";
        mSkipValidation                            = false;
        mState.mExtensions.noErrorKHR              = mSkipValidation;
        mState.mExtensions.robustClientMemoryANGLE = !mSkipValidation;

        INFO() << "Disabling GL_OES_depth32 during capture, which is not widely supported on "
                  "mobile";
        mState.mExtensions.depth32OES = false;

        // Pixel 4 (Qualcomm) only supports 6 atomic counter buffer bindings.
        constexpr GLint maxAtomicCounterBufferBindings = 6;
        INFO() << "Limiting max atomic counter buffer bindings to "
               << maxAtomicCounterBufferBindings;
        ANGLE_LIMIT_CAP(mState.mCaps.maxAtomicCounterBufferBindings,
                        maxAtomicCounterBufferBindings);
        for (gl::ShaderType shaderType : gl::AllShaderTypes())
        {
            ANGLE_LIMIT_CAP(mState.mCaps.maxShaderAtomicCounterBuffers[shaderType],
                            maxAtomicCounterBufferBindings);
        }

        // SwiftShader only supports 12 shader storage buffer bindings.
        constexpr GLint maxShaderStorageBufferBindings = 12;
        INFO() << "Limiting max shader storage buffer bindings to "
               << maxShaderStorageBufferBindings;
        ANGLE_LIMIT_CAP(mState.mCaps.maxShaderStorageBufferBindings,
                        maxShaderStorageBufferBindings);
        for (gl::ShaderType shaderType : gl::AllShaderTypes())
        {
            ANGLE_LIMIT_CAP(mState.mCaps.maxShaderStorageBlocks[shaderType],
                            maxShaderStorageBufferBindings);
        }

        // Pixel 4 only supports GL_MAX_SAMPLES of 4
        constexpr GLint maxSamples = 4;
        INFO() << "Limiting GL_MAX_SAMPLES to " << maxSamples;
        ANGLE_LIMIT_CAP(mState.mCaps.maxSamples, maxSamples);

        // Test if we require shadow memory for coherent buffer tracking
        getShareGroup()->getFrameCaptureShared()->determineMemoryProtectionSupport(this);
    }

    // Disable support for OES_get_program_binary
    if (mDisplay->getFrontendFeatures().disableProgramBinary.enabled)
    {
        mState.mExtensions.getProgramBinaryOES = false;
        mState.mCaps.shaderBinaryFormats.clear();
        mState.mCaps.programBinaryFormats.clear();
        mMemoryProgramCache = nullptr;
    }

    if (mSupportedExtensions.shaderPixelLocalStorageANGLE)
    {
        int maxDrawableAttachments =
            std::min(mState.mCaps.maxDrawBuffers, mState.mCaps.maxColorAttachments);
        switch (mImplementation->getNativePixelLocalStorageType())
        {
            case ShPixelLocalStorageType::ImageStoreNativeFormats:
            case ShPixelLocalStorageType::ImageStoreR32PackedFormats:
                mState.mCaps.maxPixelLocalStoragePlanes =
                    mState.mCaps.maxShaderImageUniforms[ShaderType::Fragment];
                ANGLE_LIMIT_CAP(mState.mCaps.maxPixelLocalStoragePlanes,
                                IMPLEMENTATION_MAX_PIXEL_LOCAL_STORAGE_PLANES);
                mState.mCaps.maxColorAttachmentsWithActivePixelLocalStorage =
                    mState.mCaps.maxColorAttachments;
                mState.mCaps.maxCombinedDrawBuffersAndPixelLocalStoragePlanes = std::min<GLint>(
                    mState.mCaps.maxPixelLocalStoragePlanes +
                        std::min(mState.mCaps.maxDrawBuffers, mState.mCaps.maxColorAttachments),
                    mState.mCaps.maxCombinedShaderOutputResources);
                break;

            case ShPixelLocalStorageType::FramebufferFetch:
                mState.mCaps.maxPixelLocalStoragePlanes = maxDrawableAttachments;
                ANGLE_LIMIT_CAP(mState.mCaps.maxPixelLocalStoragePlanes,
                                IMPLEMENTATION_MAX_PIXEL_LOCAL_STORAGE_PLANES);
                if (!mSupportedExtensions.drawBuffersIndexedAny())
                {
                    // When pixel local storage is implemented as framebuffer attachments, we need
                    // to disable color masks and blending to its attachments. If the backend
                    // context doesn't have indexed blend and color mask support, then we will have
                    // have to disable them globally. This also means the application can't have its
                    // own draw buffers while PLS is active.
                    mState.mCaps.maxColorAttachmentsWithActivePixelLocalStorage = 0;
                }
                else
                {
                    mState.mCaps.maxColorAttachmentsWithActivePixelLocalStorage =
                        maxDrawableAttachments - 1;
                }
                mState.mCaps.maxCombinedDrawBuffersAndPixelLocalStoragePlanes =
                    maxDrawableAttachments;
                break;

            case ShPixelLocalStorageType::PixelLocalStorageEXT:
                mState.mCaps.maxPixelLocalStoragePlanes =
                    mState.mCaps.maxShaderPixelLocalStorageFastSizeEXT / 4;
                ASSERT(mState.mCaps.maxPixelLocalStoragePlanes >= 4);
                ANGLE_LIMIT_CAP(mState.mCaps.maxPixelLocalStoragePlanes,
                                IMPLEMENTATION_MAX_PIXEL_LOCAL_STORAGE_PLANES);
                // EXT_shader_pixel_local_storage doesn't support rendering to color attachments.
                mState.mCaps.maxColorAttachmentsWithActivePixelLocalStorage = 0;
                mState.mCaps.maxCombinedDrawBuffersAndPixelLocalStoragePlanes =
                    mState.mCaps.maxPixelLocalStoragePlanes;
                break;

            case ShPixelLocalStorageType::NotSupported:
                UNREACHABLE();
                break;
        }
    }

#undef ANGLE_LIMIT_CAP
#undef ANGLE_LOG_CAP_LIMIT

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
        formatCaps.blendable =
            formatCaps.blendable && formatInfo.blendSupport(getClientVersion(), mState.mExtensions);

        // OpenGL ES does not support multisampling with non-rendererable formats
        // OpenGL ES 3.0 or prior does not support multisampling with integer formats
        if (!formatCaps.renderbuffer ||
            (getClientVersion() < ES_3_1 && !mState.mExtensions.textureMultisampleANGLE &&
             formatInfo.isInt()))
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
            if (!formatInfo.isInt() && formatInfo.isRequiredRenderbufferFormat(getClientVersion()))
            {
                ASSERT(getClientVersion() < ES_3_0 || formatMaxSamples >= 4);
                mState.mCaps.maxSamples =
                    std::min(static_cast<GLuint>(mState.mCaps.maxSamples), formatMaxSamples);
            }

            // Handle GLES 3.1 MAX_*_SAMPLES values similarly to MAX_SAMPLES.
            if (getClientVersion() >= ES_3_1 || mState.mExtensions.textureMultisampleANGLE)
            {
                // GLES 3.1 section 9.2.5: "Implementations must support creation of renderbuffers
                // in these required formats with up to the value of MAX_SAMPLES multisamples, with
                // the exception that the signed and unsigned integer formats are required only to
                // support creation of renderbuffers with up to the value of MAX_INTEGER_SAMPLES
                // multisamples, which must be at least one."
                if (formatInfo.isInt())
                {
                    mState.mCaps.maxIntegerSamples = std::min(
                        static_cast<GLuint>(mState.mCaps.maxIntegerSamples), formatMaxSamples);
                }

                // GLES 3.1 section 19.3.1.
                if (formatCaps.texturable)
                {
                    if (formatInfo.depthBits > 0)
                    {
                        mState.mCaps.maxDepthTextureSamples =
                            std::min(static_cast<GLuint>(mState.mCaps.maxDepthTextureSamples),
                                     formatMaxSamples);
                    }
                    else if (formatInfo.redBits > 0)
                    {
                        mState.mCaps.maxColorTextureSamples =
                            std::min(static_cast<GLuint>(mState.mCaps.maxColorTextureSamples),
                                     formatMaxSamples);
                    }
                }
            }
        }

        if (formatCaps.texturable && (formatInfo.compressed || formatInfo.paletted))
        {
            mState.mCaps.compressedTextureFormats.push_back(sizedInternalFormat);
        }

        mState.mTextureCaps.insert(sizedInternalFormat, formatCaps);
    }

    // If program binary is disabled, blank out the memory cache pointer.
    if (!mSupportedExtensions.getProgramBinaryOES)
    {
        mMemoryProgramCache = nullptr;
    }

    // Compute which buffer types are allowed
    mValidBufferBindings.reset();
    mValidBufferBindings.set(BufferBinding::ElementArray);
    mValidBufferBindings.set(BufferBinding::Array);

    if (mState.mExtensions.pixelBufferObjectNV || getClientVersion() >= ES_3_0)
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

    if (getClientVersion() >= ES_3_2 || mState.mExtensions.textureBufferAny())
    {
        mValidBufferBindings.set(BufferBinding::Texture);
    }

    // Reinitialize some dirty bits that depend on extensions.
    if (mState.isRobustResourceInitEnabled())
    {
        mDrawDirtyObjects.set(State::DIRTY_OBJECT_DRAW_ATTACHMENTS);
        mDrawDirtyObjects.set(State::DIRTY_OBJECT_TEXTURES_INIT);
        mDrawDirtyObjects.set(State::DIRTY_OBJECT_IMAGES_INIT);
        mBlitDirtyObjects.set(State::DIRTY_OBJECT_DRAW_ATTACHMENTS);
        mBlitDirtyObjects.set(State::DIRTY_OBJECT_READ_ATTACHMENTS);
        mComputeDirtyObjects.set(State::DIRTY_OBJECT_TEXTURES_INIT);
        mComputeDirtyObjects.set(State::DIRTY_OBJECT_IMAGES_INIT);
        mReadPixelsDirtyObjects.set(State::DIRTY_OBJECT_READ_ATTACHMENTS);
        mCopyImageDirtyBits.set(State::DIRTY_BIT_READ_FRAMEBUFFER_BINDING);
        mCopyImageDirtyObjects.set(State::DIRTY_OBJECT_READ_ATTACHMENTS);
    }

    // We need to validate buffer bounds if we are in a WebGL or robust access context and the
    // back-end does not support robust buffer access behaviour.
    mBufferAccessValidationEnabled = (!mSupportedExtensions.robustBufferAccessBehaviorKHR &&
                                      (mState.isWebGL() || mState.hasRobustAccess()));

    // Cache this in the VertexArrays. They need to check it in state change notifications.
    for (auto vaoIter : mVertexArrayMap)
    {
        VertexArray *vao = vaoIter.second;
        vao->setBufferAccessValidationEnabled(mBufferAccessValidationEnabled);
    }

    // Reinitialize state cache after extension changes.
    mStateCache.initialize(this);
}

bool Context::noopDrawInstanced(PrimitiveMode mode, GLsizei count, GLsizei instanceCount) const
{
    return (instanceCount == 0) || noopDraw(mode, count);
}

angle::Result Context::prepareForClear(GLbitfield mask)
{
    // Sync the draw framebuffer manually after the clear attachments.
    ANGLE_TRY(mState.getDrawFramebuffer()->ensureClearAttachmentsInitialized(this, mask));
    return syncStateForClear();
}

angle::Result Context::prepareForClearBuffer(GLenum buffer, GLint drawbuffer)
{
    // Sync the draw framebuffer manually after the clear attachments.
    ANGLE_TRY(mState.getDrawFramebuffer()->ensureClearBufferAttachmentsInitialized(this, buffer,
                                                                                   drawbuffer));
    return syncStateForClear();
}

ANGLE_INLINE angle::Result Context::prepareForCopyImage()
{
    ANGLE_TRY(syncDirtyObjects(mCopyImageDirtyObjects, Command::CopyImage));
    return syncDirtyBits(mCopyImageDirtyBits, Command::CopyImage);
}

ANGLE_INLINE angle::Result Context::prepareForDispatch()
{
    // Converting a PPO from graphics to compute requires re-linking it.
    // The compute shader must have successfully linked before being included in the PPO, so no link
    // errors that would have been caught during validation should be possible when re-linking the
    // PPO with the compute shader.
    Program *program          = mState.getProgram();
    ProgramPipeline *pipeline = mState.getProgramPipeline();
    if (!program && pipeline)
    {
        // Linking the PPO can't fail due to a validation error within the compute program,
        // since it successfully linked already in order to become part of the PPO in the first
        // place.
        pipeline->resolveLink(this);
        ANGLE_CHECK(this, pipeline->isLinked(), "Program pipeline link failed",
                    GL_INVALID_OPERATION);
    }

    ANGLE_TRY(syncDirtyObjects(mComputeDirtyObjects, Command::Dispatch));
    return syncDirtyBits(mComputeDirtyBits, Command::Dispatch);
}

angle::Result Context::prepareForInvalidate(GLenum target)
{
    // Only sync the FBO that's being invalidated.  Per the GLES3 spec, GL_FRAMEBUFFER is equivalent
    // to GL_DRAW_FRAMEBUFFER for the purposes of invalidation.
    GLenum effectiveTarget = target;
    if (effectiveTarget == GL_FRAMEBUFFER)
    {
        effectiveTarget = GL_DRAW_FRAMEBUFFER;
    }
    ANGLE_TRY(mState.syncDirtyObject(this, effectiveTarget));
    return syncDirtyBits(effectiveTarget == GL_READ_FRAMEBUFFER ? mReadInvalidateDirtyBits
                                                                : mDrawInvalidateDirtyBits,
                         Command::Invalidate);
}

angle::Result Context::syncState(const State::DirtyBits &bitMask,
                                 const State::DirtyObjects &objectMask,
                                 Command command)
{
    ANGLE_TRY(syncDirtyObjects(objectMask, command));
    ANGLE_TRY(syncDirtyBits(bitMask, command));
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

    // Note that blitting is called against draw framebuffer.
    // See the code in gl::Context::blitFramebuffer.
    if ((mask & GL_COLOR_BUFFER_BIT) && !drawFramebuffer->hasEnabledDrawBuffer())
    {
        mask &= ~GL_COLOR_BUFFER_BIT;
    }

    if ((mask & GL_STENCIL_BUFFER_BIT) &&
        drawFramebuffer->getState().getStencilAttachment() == nullptr)
    {
        mask &= ~GL_STENCIL_BUFFER_BIT;
    }

    if ((mask & GL_DEPTH_BUFFER_BIT) && drawFramebuffer->getState().getDepthAttachment() == nullptr)
    {
        mask &= ~GL_DEPTH_BUFFER_BIT;
    }

    // Early out if none of the specified attachments exist or are enabled.
    if (mask == 0)
    {
        ANGLE_PERF_WARNING(mState.getDebug(), GL_DEBUG_SEVERITY_LOW,
                           "BlitFramebuffer called for non-existing buffers");
        return;
    }

    Rectangle srcArea(srcX0, srcY0, srcX1 - srcX0, srcY1 - srcY0);
    Rectangle dstArea(dstX0, dstY0, dstX1 - dstX0, dstY1 - dstY0);

    if (dstArea.width == 0 || dstArea.height == 0)
    {
        return;
    }

    ANGLE_CONTEXT_TRY(syncStateForBlit(mask));
    ANGLE_CONTEXT_TRY(drawFramebuffer->blit(this, srcArea, dstArea, mask, filter));
}

void Context::blitFramebufferNV(GLint srcX0,
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
    blitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void Context::clear(GLbitfield mask)
{
    if (mState.isRasterizerDiscardEnabled())
    {
        return;
    }

    // Noop empty scissors.
    if (IsEmptyScissor(mState))
    {
        return;
    }

    // Remove clear bits that are ineffective. An effective clear changes at least one fragment. If
    // color/depth/stencil masks make the clear ineffective we skip it altogether.

    // If all color channels in all draw buffers are masked, don't attempt to clear color.
    if (mState.allActiveDrawBufferChannelsMasked())
    {
        mask &= ~GL_COLOR_BUFFER_BIT;
    }

    // If depth write is disabled, don't attempt to clear depth.
    if (mState.getDrawFramebuffer()->getDepthAttachment() == nullptr ||
        !mState.getDepthStencilState().depthMask)
    {
        mask &= ~GL_DEPTH_BUFFER_BIT;
    }

    // If all stencil bits are masked, don't attempt to clear stencil.
    if (mState.getDrawFramebuffer()->getStencilAttachment() == nullptr ||
        (angle::BitMask<uint32_t>(
             mState.getDrawFramebuffer()->getStencilAttachment()->getStencilSize()) &
         mState.getDepthStencilState().stencilWritemask) == 0)
    {
        mask &= ~GL_STENCIL_BUFFER_BIT;
    }

    if (mask == 0)
    {
        ANGLE_PERF_WARNING(mState.getDebug(), GL_DEBUG_SEVERITY_LOW,
                           "Clear called for non-existing buffers");
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForClear(mask));
    ANGLE_CONTEXT_TRY(mState.getDrawFramebuffer()->clear(this, mask));
}

bool Context::isClearBufferMaskedOut(GLenum buffer, GLint drawbuffer) const
{
    switch (buffer)
    {
        case GL_COLOR:
            return IsColorMaskedOut(mState.getBlendStateExt(), drawbuffer);
        case GL_DEPTH:
            return mState.getDepthStencilState().isDepthMaskedOut();
        case GL_STENCIL:
            return mState.getDepthStencilState().isStencilMaskedOut();
        case GL_DEPTH_STENCIL:
            return mState.getDepthStencilState().isDepthMaskedOut() &&
                   mState.getDepthStencilState().isStencilMaskedOut();
        default:
            UNREACHABLE();
            return true;
    }
}

bool Context::noopClearBuffer(GLenum buffer, GLint drawbuffer) const
{
    Framebuffer *framebufferObject = mState.getDrawFramebuffer();

    return !IsClearBufferEnabled(framebufferObject->getState(), buffer, drawbuffer) ||
           mState.isRasterizerDiscardEnabled() || isClearBufferMaskedOut(buffer, drawbuffer) ||
           IsEmptyScissor(mState);
}

void Context::clearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *values)
{
    if (noopClearBuffer(buffer, drawbuffer))
    {
        return;
    }

    Framebuffer *framebufferObject          = mState.getDrawFramebuffer();
    const FramebufferAttachment *attachment = nullptr;
    if (buffer == GL_DEPTH)
    {
        attachment = framebufferObject->getDepthAttachment();
    }
    else if (buffer == GL_COLOR &&
             static_cast<size_t>(drawbuffer) < framebufferObject->getNumColorAttachments())
    {
        attachment = framebufferObject->getColorAttachment(drawbuffer);
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
    if (noopClearBuffer(buffer, drawbuffer))
    {
        return;
    }

    Framebuffer *framebufferObject          = mState.getDrawFramebuffer();
    const FramebufferAttachment *attachment = nullptr;
    if (buffer == GL_COLOR &&
        static_cast<size_t>(drawbuffer) < framebufferObject->getNumColorAttachments())
    {
        attachment = framebufferObject->getColorAttachment(drawbuffer);
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
    if (noopClearBuffer(buffer, drawbuffer))
    {
        return;
    }

    Framebuffer *framebufferObject          = mState.getDrawFramebuffer();
    const FramebufferAttachment *attachment = nullptr;
    if (buffer == GL_STENCIL)
    {
        attachment = framebufferObject->getStencilAttachment();
    }
    else if (buffer == GL_COLOR &&
             static_cast<size_t>(drawbuffer) < framebufferObject->getNumColorAttachments())
    {
        attachment = framebufferObject->getColorAttachment(drawbuffer);
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
    if (noopClearBuffer(buffer, drawbuffer))
    {
        return;
    }

    Framebuffer *framebufferObject = mState.getDrawFramebuffer();
    ASSERT(framebufferObject);

    // If a buffer is not present, the clear has no effect
    if (framebufferObject->getDepthAttachment() == nullptr &&
        framebufferObject->getStencilAttachment() == nullptr)
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
    PixelPackState packState = mState.getPackState();
    Buffer *packBuffer       = mState.getTargetBuffer(gl::BufferBinding::PixelPack);
    ANGLE_CONTEXT_TRY(readFBO->readPixels(this, area, format, type, packState, packBuffer, pixels));
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
    ANGLE_CONTEXT_TRY(prepareForCopyImage());

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

    ANGLE_CONTEXT_TRY(prepareForCopyImage());

    Offset destOffset(xoffset, yoffset, 0);
    Rectangle sourceArea(x, y, width, height);

    ImageIndex index = ImageIndex::MakeFromTarget(target, level, 1);

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

    ANGLE_CONTEXT_TRY(prepareForCopyImage());

    Offset destOffset(xoffset, yoffset, zoffset);
    Rectangle sourceArea(x, y, width, height);

    ImageIndex index = ImageIndex::MakeFromType(TextureTargetToType(target), level, zoffset);

    Framebuffer *framebuffer = mState.getReadFramebuffer();
    Texture *texture         = getTextureByTarget(target);
    ANGLE_CONTEXT_TRY(texture->copySubImage(this, index, destOffset, sourceArea, framebuffer));
}

void Context::copyImageSubData(GLuint srcName,
                               GLenum srcTarget,
                               GLint srcLevel,
                               GLint srcX,
                               GLint srcY,
                               GLint srcZ,
                               GLuint dstName,
                               GLenum dstTarget,
                               GLint dstLevel,
                               GLint dstX,
                               GLint dstY,
                               GLint dstZ,
                               GLsizei srcWidth,
                               GLsizei srcHeight,
                               GLsizei srcDepth)
{
    // if copy region is zero, the copy is a successful no-op
    if ((srcWidth == 0) || (srcHeight == 0) || (srcDepth == 0))
    {
        return;
    }

    if (srcTarget == GL_RENDERBUFFER)
    {
        // Source target is a Renderbuffer
        Renderbuffer *readBuffer = getRenderbuffer(PackParam<RenderbufferID>(srcName));
        if (dstTarget == GL_RENDERBUFFER)
        {
            // Destination target is a Renderbuffer
            Renderbuffer *writeBuffer = getRenderbuffer(PackParam<RenderbufferID>(dstName));

            // Copy Renderbuffer to Renderbuffer
            ANGLE_CONTEXT_TRY(writeBuffer->copyRenderbufferSubData(
                this, readBuffer, srcLevel, srcX, srcY, srcZ, dstLevel, dstX, dstY, dstZ, srcWidth,
                srcHeight, srcDepth));
        }
        else
        {
            // Destination target is a Texture
            ASSERT(dstTarget == GL_TEXTURE_2D || dstTarget == GL_TEXTURE_2D_ARRAY ||
                   dstTarget == GL_TEXTURE_3D || dstTarget == GL_TEXTURE_CUBE_MAP);

            Texture *writeTexture = getTexture(PackParam<TextureID>(dstName));
            ANGLE_CONTEXT_TRY(syncTextureForCopy(writeTexture));

            // Copy Renderbuffer to Texture
            ANGLE_CONTEXT_TRY(writeTexture->copyRenderbufferSubData(
                this, readBuffer, srcLevel, srcX, srcY, srcZ, dstLevel, dstX, dstY, dstZ, srcWidth,
                srcHeight, srcDepth));
        }
    }
    else
    {
        // Source target is a Texture
        ASSERT(srcTarget == GL_TEXTURE_2D || srcTarget == GL_TEXTURE_2D_ARRAY ||
               srcTarget == GL_TEXTURE_3D || srcTarget == GL_TEXTURE_CUBE_MAP);

        Texture *readTexture = getTexture(PackParam<TextureID>(srcName));
        ANGLE_CONTEXT_TRY(syncTextureForCopy(readTexture));

        if (dstTarget == GL_RENDERBUFFER)
        {
            // Destination target is a Renderbuffer
            Renderbuffer *writeBuffer = getRenderbuffer(PackParam<RenderbufferID>(dstName));

            // Copy Texture to Renderbuffer
            ANGLE_CONTEXT_TRY(writeBuffer->copyTextureSubData(this, readTexture, srcLevel, srcX,
                                                              srcY, srcZ, dstLevel, dstX, dstY,
                                                              dstZ, srcWidth, srcHeight, srcDepth));
        }
        else
        {
            // Destination target is a Texture
            ASSERT(dstTarget == GL_TEXTURE_2D || dstTarget == GL_TEXTURE_2D_ARRAY ||
                   dstTarget == GL_TEXTURE_3D || dstTarget == GL_TEXTURE_CUBE_MAP);

            Texture *writeTexture = getTexture(PackParam<TextureID>(dstName));
            ANGLE_CONTEXT_TRY(syncTextureForCopy(writeTexture));

            // Copy Texture to Texture
            ANGLE_CONTEXT_TRY(writeTexture->copyTextureSubData(
                this, readTexture, srcLevel, srcX, srcY, srcZ, dstLevel, dstX, dstY, dstZ, srcWidth,
                srcHeight, srcDepth));
        }
    }
}

void Context::framebufferTexture2D(GLenum target,
                                   GLenum attachment,
                                   TextureTarget textarget,
                                   TextureID texture,
                                   GLint level)
{
    Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (texture.value != 0)
    {
        Texture *textureObj = getTexture(texture);
        ImageIndex index    = ImageIndex::MakeFromTarget(textarget, level, 1);
        framebuffer->setAttachment(this, GL_TEXTURE, attachment, index, textureObj);
    }
    else
    {
        framebuffer->resetAttachment(this, attachment);
    }

    mState.setObjectDirty(target);
}

void Context::framebufferTexture3D(GLenum target,
                                   GLenum attachment,
                                   TextureTarget textargetPacked,
                                   TextureID texture,
                                   GLint level,
                                   GLint zoffset)
{
    Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (texture.value != 0)
    {
        Texture *textureObj = getTexture(texture);
        ImageIndex index    = ImageIndex::Make3D(level, zoffset);
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
                                      RenderbufferID renderbuffer)
{
    Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (renderbuffer.value != 0)
    {
        Renderbuffer *renderbufferObject = getRenderbuffer(renderbuffer);
        GLsizei rbSamples                = renderbufferObject->getState().getSamples();

        framebuffer->setAttachmentMultisample(this, GL_RENDERBUFFER, attachment, gl::ImageIndex(),
                                              renderbufferObject, rbSamples);
    }
    else
    {
        framebuffer->resetAttachment(this, attachment);
    }

    mState.setObjectDirty(target);
}

void Context::framebufferTextureLayer(GLenum target,
                                      GLenum attachment,
                                      TextureID texture,
                                      GLint level,
                                      GLint layer)
{
    Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (texture.value != 0)
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
                                          TextureID texture,
                                          GLint level,
                                          GLint baseViewIndex,
                                          GLsizei numViews)
{
    Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (texture.value != 0)
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

void Context::framebufferTexture(GLenum target, GLenum attachment, TextureID texture, GLint level)
{
    Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (texture.value != 0)
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
    // The specification isn't clear what should be done when the framebuffer isn't complete.
    // We threat it the same way as GLES3 glInvalidateFramebuffer.
    invalidateFramebuffer(target, numAttachments, attachments);
}

void Context::invalidateFramebuffer(GLenum target,
                                    GLsizei numAttachments,
                                    const GLenum *attachments)
{
    Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    // No-op incomplete FBOs.
    if (!framebuffer->isComplete(this))
    {
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForInvalidate(target));
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
    Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (!framebuffer->isComplete(this))
    {
        return;
    }

    Rectangle area(x, y, width, height);
    ANGLE_CONTEXT_TRY(prepareForInvalidate(target));
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

    gl::Buffer *unpackBuffer = mState.getTargetBuffer(gl::BufferBinding::PixelUnpack);

    Extents size(width, height, 1);
    Texture *texture = getTextureByTarget(target);
    ANGLE_CONTEXT_TRY(texture->setImage(this, mState.getUnpackState(), unpackBuffer, target, level,
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

    gl::Buffer *unpackBuffer = mState.getTargetBuffer(gl::BufferBinding::PixelUnpack);

    Extents size(width, height, depth);
    Texture *texture = getTextureByTarget(target);
    ANGLE_CONTEXT_TRY(texture->setImage(this, mState.getUnpackState(), unpackBuffer, target, level,
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
    // From OpenGL ES 3 spec: All pixel storage modes are ignored when decoding a compressed texture
    // image. So we use an empty PixelUnpackState.
    ANGLE_CONTEXT_TRY(texture->setCompressedImage(this, PixelUnpackState(), target, level,
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
    // From OpenGL ES 3 spec: All pixel storage modes are ignored when decoding a compressed texture
    // image. So we use an empty PixelUnpackState.
    ANGLE_CONTEXT_TRY(texture->setCompressedImage(this, PixelUnpackState(), target, level,
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
    // From OpenGL ES 3 spec: All pixel storage modes are ignored when decoding a compressed texture
    // image. So we use an empty PixelUnpackState.
    ANGLE_CONTEXT_TRY(texture->setCompressedSubImage(this, PixelUnpackState(), target, level, area,
                                                     format, imageSize,
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
    // From OpenGL ES 3 spec: All pixel storage modes are ignored when decoding a compressed texture
    // image. So we use an empty PixelUnpackState.
    ANGLE_CONTEXT_TRY(texture->setCompressedSubImage(this, PixelUnpackState(), target, level, area,
                                                     format, imageSize,
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

void Context::copyTexture(TextureID sourceId,
                          GLint sourceLevel,
                          TextureTarget destTarget,
                          TextureID destId,
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

void Context::copySubTexture(TextureID sourceId,
                             GLint sourceLevel,
                             TextureTarget destTarget,
                             TextureID destId,
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

void Context::copyTexture3D(TextureID sourceId,
                            GLint sourceLevel,
                            TextureTarget destTarget,
                            TextureID destId,
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

void Context::copySubTexture3D(TextureID sourceId,
                               GLint sourceLevel,
                               TextureTarget destTarget,
                               TextureID destId,
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

void Context::compressedCopyTexture(TextureID sourceId, TextureID destId)
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

    // TODO: (anglebug.com/7821): Modify return value in entry point layer
    angle::FrameCaptureShared *frameCaptureShared = getShareGroup()->getFrameCaptureShared();
    if (frameCaptureShared->enabled())
    {
        return frameCaptureShared->maybeGetShadowMemoryPointer(buffer, length, access);
    }
    else
    {
        return buffer->getMapPointer();
    }
}

void Context::flushMappedBufferRange(BufferBinding /*target*/,
                                     GLintptr /*offset*/,
                                     GLsizeiptr /*length*/)
{
    // We do not currently support a non-trivial implementation of FlushMappedBufferRange
}

angle::Result Context::syncStateForReadPixels()
{
    return syncState(mReadPixelsDirtyBits, mReadPixelsDirtyObjects, Command::ReadPixels);
}

angle::Result Context::syncStateForTexImage()
{
    return syncState(mTexImageDirtyBits, mTexImageDirtyObjects, Command::TexImage);
}

angle::Result Context::syncStateForBlit(GLbitfield mask)
{
    uint32_t commandMask = 0;
    if ((mask & GL_COLOR_BUFFER_BIT) != 0)
    {
        commandMask |= CommandBlitBufferColor;
    }
    if ((mask & GL_DEPTH_BUFFER_BIT) != 0)
    {
        commandMask |= CommandBlitBufferDepth;
    }
    if ((mask & GL_STENCIL_BUFFER_BIT) != 0)
    {
        commandMask |= CommandBlitBufferStencil;
    }

    Command command = static_cast<Command>(static_cast<uint32_t>(Command::Blit) + commandMask);

    return syncState(mBlitDirtyBits, mBlitDirtyObjects, command);
}

angle::Result Context::syncStateForClear()
{
    return syncState(mClearDirtyBits, mClearDirtyObjects, Command::Clear);
}

angle::Result Context::syncTextureForCopy(Texture *texture)
{
    ASSERT(texture);
    // Sync texture not active but scheduled for a copy
    if (texture->hasAnyDirtyBit())
    {
        return texture->syncState(this, Command::Other);
    }

    return angle::Result::Continue;
}

void Context::activeShaderProgram(ProgramPipelineID pipeline, ShaderProgramID program)
{
    Program *shaderProgram = getProgramNoResolveLink(program);
    ProgramPipeline *programPipeline =
        mState.mProgramPipelineManager->checkProgramPipelineAllocation(mImplementation.get(),
                                                                       pipeline);
    ASSERT(programPipeline);

    programPipeline->activeShaderProgram(shaderProgram);
}

void Context::activeTexture(GLenum texture)
{
    mState.setActiveSampler(texture - GL_TEXTURE0);
}

void Context::blendBarrier()
{
    mImplementation->blendBarrier();
}

void Context::blendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    mState.setBlendColor(red, green, blue, alpha);
}

void Context::blendEquation(GLenum mode)
{
    mState.setBlendEquation(mode, mode);

    mStateCache.onBlendEquationChange(this);
}

void Context::blendEquationi(GLuint buf, GLenum mode)
{
    mState.setBlendEquationIndexed(mode, mode, buf);

    mStateCache.onBlendEquationChange(this);
}

void Context::blendEquationSeparate(GLenum modeRGB, GLenum modeAlpha)
{
    mState.setBlendEquation(modeRGB, modeAlpha);
}

void Context::blendEquationSeparatei(GLuint buf, GLenum modeRGB, GLenum modeAlpha)
{
    mState.setBlendEquationIndexed(modeRGB, modeAlpha, buf);
}

void Context::blendFunc(GLenum sfactor, GLenum dfactor)
{
    mState.setBlendFactors(sfactor, dfactor, sfactor, dfactor);
}

void Context::blendFunci(GLuint buf, GLenum src, GLenum dst)
{
    mState.setBlendFactorsIndexed(src, dst, src, dst, buf);

    if (mState.noSimultaneousConstantColorAndAlphaBlendFunc())
    {
        mStateCache.onBlendFuncIndexedChange(this);
    }
}

void Context::blendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
    mState.setBlendFactors(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void Context::blendFuncSeparatei(GLuint buf,
                                 GLenum srcRGB,
                                 GLenum dstRGB,
                                 GLenum srcAlpha,
                                 GLenum dstAlpha)
{
    mState.setBlendFactorsIndexed(srcRGB, dstRGB, srcAlpha, dstAlpha, buf);

    if (mState.noSimultaneousConstantColorAndAlphaBlendFunc())
    {
        mStateCache.onBlendFuncIndexedChange(this);
    }
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
    mStateCache.onColorMaskChange(this);
}

void Context::colorMaski(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a)
{
    mState.setColorMaskIndexed(ConvertToBool(r), ConvertToBool(g), ConvertToBool(b),
                               ConvertToBool(a), index);
    mStateCache.onColorMaskChange(this);
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

void Context::clipControl(GLenum origin, GLenum depth)
{
    mState.setClipControl(origin, depth);
}

void Context::disable(GLenum cap)
{
    mState.setEnableFeature(cap, false);
    mStateCache.onContextCapChange(this);
}

void Context::disablei(GLenum target, GLuint index)
{
    mState.setEnableFeatureIndexed(target, false, index);
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

void Context::enablei(GLenum target, GLuint index)
{
    mState.setEnableFeatureIndexed(target, true, index);
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
        case GL_TEXTURE_FILTERING_HINT_CHROMIUM:
            mState.setTextureFilteringHint(mode);
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
            ASSERT((getClientMajorVersion() >= 3) || getExtensions().unpackSubimageEXT);
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
            ASSERT((getClientMajorVersion() >= 3) || getExtensions().unpackSubimageEXT);
            mState.setUnpackSkipRows(param);
            break;

        case GL_UNPACK_SKIP_PIXELS:
            ASSERT((getClientMajorVersion() >= 3) || getExtensions().unpackSubimageEXT);
            mState.setUnpackSkipPixels(param);
            break;

        case GL_PACK_ROW_LENGTH:
            ASSERT((getClientMajorVersion() >= 3) || getExtensions().packSubimageNV);
            mState.setPackRowLength(param);
            break;

        case GL_PACK_SKIP_ROWS:
            ASSERT((getClientMajorVersion() >= 3) || getExtensions().packSubimageNV);
            mState.setPackSkipRows(param);
            break;

        case GL_PACK_SKIP_PIXELS:
            ASSERT((getClientMajorVersion() >= 3) || getExtensions().packSubimageNV);
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

void Context::shadingRateQCOM(GLenum rate)
{
    mState.setShadingRate(rate);
}

void Context::stencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask)
{
    GLint clampedRef = gl::clamp(ref, 0, std::numeric_limits<uint8_t>::max());
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK)
    {
        mState.setStencilParams(func, clampedRef, mask);
    }

    if (face == GL_BACK || face == GL_FRONT_AND_BACK)
    {
        mState.setStencilBackParams(func, clampedRef, mask);
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
    mState.setVertexBindingDivisor(this, bindingIndex, divisor);
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

void Context::getVertexAttribivImpl(GLuint index, GLenum pname, GLint *params) const
{
    const VertexAttribCurrentValueData &currentValues =
        getState().getVertexAttribCurrentValue(index);
    const VertexArray *vao = getState().getVertexArray();
    QueryVertexAttribiv(vao->getVertexAttribute(index), vao->getBindingFromAttribIndex(index),
                        currentValues, pname, params);
}

void Context::getVertexAttribiv(GLuint index, GLenum pname, GLint *params)
{
    return getVertexAttribivImpl(index, pname, params);
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
    mState.getDebug().insertMessage(source, type, id, severity, std::move(msg), gl::LOG_INFO,
                                    angle::EntryPoint::GLDebugMessageInsert);
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
    ANGLE_CONTEXT_TRY(mImplementation->pushDebugGroup(this, source, id, msg));
    mState.getDebug().pushGroup(source, id, std::move(msg));
}

angle::Result Context::handleNoopDrawEvent()
{
    return (mImplementation->handleNoopDrawEvent());
}

void Context::popDebugGroup()
{
    mState.getDebug().popGroup();
    ANGLE_CONTEXT_TRY(mImplementation->popDebugGroup(this));
}

void Context::primitiveBoundingBox(GLfloat minX,
                                   GLfloat minY,
                                   GLfloat minZ,
                                   GLfloat minW,
                                   GLfloat maxX,
                                   GLfloat maxY,
                                   GLfloat maxZ,
                                   GLfloat maxW)
{
    mState.mBoundingBoxMinX = minX;
    mState.mBoundingBoxMinY = minY;
    mState.mBoundingBoxMinZ = minZ;
    mState.mBoundingBoxMinW = minW;
    mState.mBoundingBoxMaxX = maxX;
    mState.mBoundingBoxMaxY = maxY;
    mState.mBoundingBoxMaxZ = maxZ;
    mState.mBoundingBoxMaxW = maxW;
}

void Context::bufferStorage(BufferBinding target,
                            GLsizeiptr size,
                            const void *data,
                            GLbitfield flags)
{
    Buffer *buffer = mState.getTargetBuffer(target);
    ASSERT(buffer);
    ANGLE_CONTEXT_TRY(buffer->bufferStorage(this, target, size, data, flags));
}

void Context::bufferStorageExternal(BufferBinding target,
                                    GLintptr offset,
                                    GLsizeiptr size,
                                    GLeglClientBufferEXT clientBuffer,
                                    GLbitfield flags)
{
    Buffer *buffer = mState.getTargetBuffer(target);
    ASSERT(buffer);

    ANGLE_CONTEXT_TRY(buffer->bufferStorageExternal(this, target, size, clientBuffer, flags));
}

void Context::namedBufferStorageExternal(GLuint buffer,
                                         GLintptr offset,
                                         GLsizeiptr size,
                                         GLeglClientBufferEXT clientBuffer,
                                         GLbitfield flags)
{
    UNIMPLEMENTED();
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

void Context::attachShader(ShaderProgramID program, ShaderProgramID shader)
{
    Program *programObject = mState.mShaderProgramManager->getProgram(program);
    Shader *shaderObject   = mState.mShaderProgramManager->getShader(shader);
    ASSERT(programObject && shaderObject);
    programObject->attachShader(shaderObject);
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

void Context::bindAttribLocation(ShaderProgramID program, GLuint index, const GLchar *name)
{
    // Ideally we could share the program query with the validation layer if possible.
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->bindAttributeLocation(index, name);
}

void Context::bindBufferBase(BufferBinding target, GLuint index, BufferID buffer)
{
    bindBufferRange(target, index, buffer, 0, 0);
}

void Context::bindBufferRange(BufferBinding target,
                              GLuint index,
                              BufferID buffer,
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
    else if (target == BufferBinding::AtomicCounter)
    {
        mAtomicCounterBufferObserverBindings[index].bind(object);
        mStateCache.onAtomicCounterBufferStateChange(this);
    }
    else if (target == BufferBinding::ShaderStorage)
    {
        mShaderStorageBufferObserverBindings[index].bind(object);
        mStateCache.onShaderStorageBufferStateChange(this);
    }
    else
    {
        mStateCache.onBufferBindingChange(this);
    }
}

void Context::bindFramebuffer(GLenum target, FramebufferID framebuffer)
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

void Context::bindRenderbuffer(GLenum target, RenderbufferID renderbuffer)
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

void Context::texImage2DExternal(TextureTarget target,
                                 GLint level,
                                 GLint internalformat,
                                 GLsizei width,
                                 GLsizei height,
                                 GLint border,
                                 GLenum format,
                                 GLenum type)
{
    Extents size(width, height, 1);
    Texture *texture = getTextureByTarget(target);
    ANGLE_CONTEXT_TRY(
        texture->setImageExternal(this, target, level, internalformat, size, format, type));
}

void Context::invalidateTexture(TextureType target)
{
    mImplementation->invalidateTexture(target);
    mState.invalidateTextureBindings(target);
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
    renderbufferStorageMultisampleImpl(target, samples, internalformat, width, height,
                                       MultisamplingMode::Regular);
}

void Context::renderbufferStorageMultisampleEXT(GLenum target,
                                                GLsizei samples,
                                                GLenum internalformat,
                                                GLsizei width,
                                                GLsizei height)
{
    renderbufferStorageMultisampleImpl(target, samples, internalformat, width, height,
                                       MultisamplingMode::MultisampledRenderToTexture);
}

void Context::renderbufferStorageMultisampleImpl(GLenum target,
                                                 GLsizei samples,
                                                 GLenum internalformat,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 MultisamplingMode mode)
{
    // Hack for the special WebGL 1 "DEPTH_STENCIL" internal format.
    GLenum convertedInternalFormat = getConvertedRenderbufferFormat(internalformat);

    Renderbuffer *renderbuffer = mState.getCurrentRenderbuffer();
    ANGLE_CONTEXT_TRY(renderbuffer->setStorageMultisample(this, samples, convertedInternalFormat,
                                                          width, height, mode));
}

void Context::framebufferTexture2DMultisample(GLenum target,
                                              GLenum attachment,
                                              TextureTarget textarget,
                                              TextureID texture,
                                              GLint level,
                                              GLsizei samples)
{
    Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (texture.value != 0)
    {
        Texture *textureObj = getTexture(texture);
        ImageIndex index    = ImageIndex::MakeFromTarget(textarget, level, 1);
        framebuffer->setAttachmentMultisample(this, GL_TEXTURE, attachment, index, textureObj,
                                              samples);
    }
    else
    {
        framebuffer->resetAttachment(this, attachment);
    }

    mState.setObjectDirty(target);
}

void Context::getSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *values)
{
    const Sync *syncObject = nullptr;
    if (!isContextLost())
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
    if (!mScratchBuffer.valid())
    {
        mScratchBuffer = mDisplay->requestScratchBuffer();
    }

    ASSERT(mScratchBuffer.valid());
    return mScratchBuffer.value().get(requstedSizeBytes, scratchBufferOut);
}

angle::ScratchBuffer *Context::getScratchBuffer() const
{
    if (!mScratchBuffer.valid())
    {
        mScratchBuffer = mDisplay->requestScratchBuffer();
    }

    ASSERT(mScratchBuffer.valid());
    return &mScratchBuffer.value();
}

bool Context::getZeroFilledBuffer(size_t requstedSizeBytes,
                                  angle::MemoryBuffer **zeroBufferOut) const
{
    if (!mZeroFilledBuffer.valid())
    {
        mZeroFilledBuffer = mDisplay->requestZeroFilledBuffer();
    }

    ASSERT(mZeroFilledBuffer.valid());
    return mZeroFilledBuffer.value().getInitialized(requstedSizeBytes, zeroBufferOut, 0);
}

void Context::dispatchCompute(GLuint numGroupsX, GLuint numGroupsY, GLuint numGroupsZ)
{
    if (numGroupsX == 0u || numGroupsY == 0u || numGroupsZ == 0u)
    {
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForDispatch());

    angle::Result result =
        mImplementation->dispatchCompute(this, numGroupsX, numGroupsY, numGroupsZ);

    // This must be called before convertPpoToComputeOrDraw() so it uses the PPO's compute values
    // before convertPpoToComputeOrDraw() reverts the PPO back to graphics.
    MarkShaderStorageUsage(this);

    if (ANGLE_UNLIKELY(IsError(result)))
    {
        return;
    }
}

void Context::dispatchComputeIndirect(GLintptr indirect)
{
    ANGLE_CONTEXT_TRY(prepareForDispatch());
    ANGLE_CONTEXT_TRY(mImplementation->dispatchComputeIndirect(this, indirect));

    MarkShaderStorageUsage(this);
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
    ANGLE_CONTEXT_TRY(mImplementation->multiDrawArrays(this, mode, firsts, counts, drawcount));
}

void Context::multiDrawArraysInstanced(PrimitiveMode mode,
                                       const GLint *firsts,
                                       const GLsizei *counts,
                                       const GLsizei *instanceCounts,
                                       GLsizei drawcount)
{
    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    ANGLE_CONTEXT_TRY(mImplementation->multiDrawArraysInstanced(this, mode, firsts, counts,
                                                                instanceCounts, drawcount));
}

void Context::multiDrawArraysIndirect(PrimitiveMode mode,
                                      const void *indirect,
                                      GLsizei drawcount,
                                      GLsizei stride)
{
    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    ANGLE_CONTEXT_TRY(
        mImplementation->multiDrawArraysIndirect(this, mode, indirect, drawcount, stride));
    MarkShaderStorageUsage(this);
}

void Context::multiDrawElements(PrimitiveMode mode,
                                const GLsizei *counts,
                                DrawElementsType type,
                                const GLvoid *const *indices,
                                GLsizei drawcount)
{
    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    ANGLE_CONTEXT_TRY(
        mImplementation->multiDrawElements(this, mode, counts, type, indices, drawcount));
}

void Context::multiDrawElementsInstanced(PrimitiveMode mode,
                                         const GLsizei *counts,
                                         DrawElementsType type,
                                         const GLvoid *const *indices,
                                         const GLsizei *instanceCounts,
                                         GLsizei drawcount)
{
    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    ANGLE_CONTEXT_TRY(mImplementation->multiDrawElementsInstanced(this, mode, counts, type, indices,
                                                                  instanceCounts, drawcount));
}

void Context::multiDrawElementsIndirect(PrimitiveMode mode,
                                        DrawElementsType type,
                                        const void *indirect,
                                        GLsizei drawcount,
                                        GLsizei stride)
{
    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    ANGLE_CONTEXT_TRY(
        mImplementation->multiDrawElementsIndirect(this, mode, type, indirect, drawcount, stride));
    MarkShaderStorageUsage(this);
}

void Context::drawArraysInstancedBaseInstance(PrimitiveMode mode,
                                              GLint first,
                                              GLsizei count,
                                              GLsizei instanceCount,
                                              GLuint baseInstance)
{
    if (noopDrawInstanced(mode, count, instanceCount))
    {
        ANGLE_CONTEXT_TRY(mImplementation->handleNoopDrawEvent());
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    Program *programObject = mState.getLinkedProgram(this);

    const bool hasBaseInstance = programObject && programObject->hasBaseInstanceUniform();
    if (hasBaseInstance)
    {
        programObject->setBaseInstanceUniform(baseInstance);
    }

    rx::ResetBaseVertexBaseInstance resetUniforms(programObject, false, hasBaseInstance);

    // The input gl_InstanceID does not follow the baseinstance. gl_InstanceID always falls on
    // the half-open range [0, instancecount). No need to set other stuff. Except for Vulkan.

    ANGLE_CONTEXT_TRY(mImplementation->drawArraysInstancedBaseInstance(
        this, mode, first, count, instanceCount, baseInstance));
    MarkTransformFeedbackBufferUsage(this, count, 1);
}

void Context::drawArraysInstancedBaseInstanceANGLE(PrimitiveMode mode,
                                                   GLint first,
                                                   GLsizei count,
                                                   GLsizei instanceCount,
                                                   GLuint baseInstance)
{
    drawArraysInstancedBaseInstance(mode, first, count, instanceCount, baseInstance);
}

void Context::drawElementsInstancedBaseInstance(PrimitiveMode mode,
                                                GLsizei count,
                                                DrawElementsType type,
                                                const void *indices,
                                                GLsizei instanceCount,
                                                GLuint baseInstance)
{
    drawElementsInstancedBaseVertexBaseInstance(mode, count, type, indices, instanceCount, 0,
                                                baseInstance);
}

void Context::drawElementsInstancedBaseVertexBaseInstance(PrimitiveMode mode,
                                                          GLsizei count,
                                                          DrawElementsType type,
                                                          const GLvoid *indices,
                                                          GLsizei instanceCount,
                                                          GLint baseVertex,
                                                          GLuint baseInstance)
{
    if (noopDrawInstanced(mode, count, instanceCount))
    {
        ANGLE_CONTEXT_TRY(mImplementation->handleNoopDrawEvent());
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    Program *programObject = mState.getLinkedProgram(this);

    const bool hasBaseVertex = programObject && programObject->hasBaseVertexUniform();
    if (hasBaseVertex)
    {
        programObject->setBaseVertexUniform(baseVertex);
    }

    const bool hasBaseInstance = programObject && programObject->hasBaseInstanceUniform();
    if (hasBaseInstance)
    {
        programObject->setBaseInstanceUniform(baseInstance);
    }

    rx::ResetBaseVertexBaseInstance resetUniforms(programObject, hasBaseVertex, hasBaseInstance);

    ANGLE_CONTEXT_TRY(mImplementation->drawElementsInstancedBaseVertexBaseInstance(
        this, mode, count, type, indices, instanceCount, baseVertex, baseInstance));
}

void Context::drawElementsInstancedBaseVertexBaseInstanceANGLE(PrimitiveMode mode,
                                                               GLsizei count,
                                                               DrawElementsType type,
                                                               const GLvoid *indices,
                                                               GLsizei instanceCount,
                                                               GLint baseVertex,
                                                               GLuint baseInstance)
{
    drawElementsInstancedBaseVertexBaseInstance(mode, count, type, indices, instanceCount,
                                                baseVertex, baseInstance);
}

void Context::multiDrawArraysInstancedBaseInstance(PrimitiveMode mode,
                                                   const GLint *firsts,
                                                   const GLsizei *counts,
                                                   const GLsizei *instanceCounts,
                                                   const GLuint *baseInstances,
                                                   GLsizei drawcount)
{
    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    ANGLE_CONTEXT_TRY(mImplementation->multiDrawArraysInstancedBaseInstance(
        this, mode, firsts, counts, instanceCounts, baseInstances, drawcount));
}

void Context::multiDrawElementsInstancedBaseVertexBaseInstance(PrimitiveMode mode,
                                                               const GLsizei *counts,
                                                               DrawElementsType type,
                                                               const GLvoid *const *indices,
                                                               const GLsizei *instanceCounts,
                                                               const GLint *baseVertices,
                                                               const GLuint *baseInstances,
                                                               GLsizei drawcount)
{
    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    ANGLE_CONTEXT_TRY(mImplementation->multiDrawElementsInstancedBaseVertexBaseInstance(
        this, mode, counts, type, indices, instanceCounts, baseVertices, baseInstances, drawcount));
}

void Context::provokingVertex(ProvokingVertexConvention provokeMode)
{
    mState.setProvokingVertex(provokeMode);
}

GLenum Context::checkFramebufferStatus(GLenum target)
{
    Framebuffer *framebuffer = mState.getTargetFramebuffer(target);
    ASSERT(framebuffer);
    return framebuffer->checkStatus(this).status;
}

void Context::compileShader(ShaderProgramID shader)
{
    Shader *shaderObject = GetValidShader(this, angle::EntryPoint::GLCompileShader, shader);
    if (!shaderObject)
    {
        return;
    }
    shaderObject->compile(this);
}

void Context::deleteBuffers(GLsizei n, const BufferID *buffers)
{
    for (int i = 0; i < n; i++)
    {
        deleteBuffer(buffers[i]);
    }
}

void Context::deleteFramebuffers(GLsizei n, const FramebufferID *framebuffers)
{
    for (int i = 0; i < n; i++)
    {
        if (framebuffers[i].value != 0)
        {
            deleteFramebuffer(framebuffers[i]);
        }
    }
}

void Context::deleteRenderbuffers(GLsizei n, const RenderbufferID *renderbuffers)
{
    for (int i = 0; i < n; i++)
    {
        deleteRenderbuffer(renderbuffers[i]);
    }
}

void Context::deleteTextures(GLsizei n, const TextureID *textures)
{
    for (int i = 0; i < n; i++)
    {
        if (textures[i].value != 0)
        {
            deleteTexture(textures[i]);
        }
    }
}

void Context::detachShader(ShaderProgramID program, ShaderProgramID shader)
{
    Program *programObject = getProgramNoResolveLink(program);
    ASSERT(programObject);

    Shader *shaderObject = getShader(shader);
    ASSERT(shaderObject);

    programObject->detachShader(this, shaderObject);
}

void Context::genBuffers(GLsizei n, BufferID *buffers)
{
    for (int i = 0; i < n; i++)
    {
        buffers[i] = createBuffer();
    }
}

void Context::genFramebuffers(GLsizei n, FramebufferID *framebuffers)
{
    for (int i = 0; i < n; i++)
    {
        framebuffers[i] = createFramebuffer();
    }
}

void Context::genRenderbuffers(GLsizei n, RenderbufferID *renderbuffers)
{
    for (int i = 0; i < n; i++)
    {
        renderbuffers[i] = createRenderbuffer();
    }
}

void Context::genTextures(GLsizei n, TextureID *textures)
{
    for (int i = 0; i < n; i++)
    {
        textures[i] = createTexture();
    }
}

void Context::getActiveAttrib(ShaderProgramID program,
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

void Context::getActiveUniform(ShaderProgramID program,
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

void Context::getAttachedShaders(ShaderProgramID program,
                                 GLsizei maxcount,
                                 GLsizei *count,
                                 ShaderProgramID *shaders)
{
    Program *programObject = getProgramNoResolveLink(program);
    ASSERT(programObject);
    programObject->getAttachedShaders(maxcount, count, shaders);
}

GLint Context::getAttribLocation(ShaderProgramID program, const GLchar *name)
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
    GLenum nativeType      = GL_NONE;
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

void Context::getProgramiv(ShaderProgramID program, GLenum pname, GLint *params)
{
    // Don't resolve link if checking the link completion status.
    Program *programObject = getProgramNoResolveLink(program);
    if (!isContextLost() && pname != GL_COMPLETION_STATUS_KHR)
    {
        programObject = getProgramResolveLink(program);
    }
    ASSERT(programObject);
    QueryProgramiv(this, programObject, pname, params);
}

void Context::getProgramivRobust(ShaderProgramID program,
                                 GLenum pname,
                                 GLsizei bufSize,
                                 GLsizei *length,
                                 GLint *params)
{
    getProgramiv(program, pname, params);
}

void Context::getProgramPipelineiv(ProgramPipelineID pipeline, GLenum pname, GLint *params)
{
    ProgramPipeline *programPipeline = nullptr;
    if (!mContextLost)
    {
        programPipeline = getProgramPipeline(pipeline);
    }
    QueryProgramPipelineiv(this, programPipeline, pname, params);
}

MemoryObject *Context::getMemoryObject(MemoryObjectID handle) const
{
    return mState.mMemoryObjectManager->getMemoryObject(handle);
}

Semaphore *Context::getSemaphore(SemaphoreID handle) const
{
    return mState.mSemaphoreManager->getSemaphore(handle);
}

void Context::getProgramInfoLog(ShaderProgramID program,
                                GLsizei bufsize,
                                GLsizei *length,
                                GLchar *infolog)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->getExecutable().getInfoLog(bufsize, length, infolog);
}

void Context::getProgramPipelineInfoLog(ProgramPipelineID pipeline,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLchar *infoLog)
{
    ProgramPipeline *programPipeline = getProgramPipeline(pipeline);
    if (programPipeline)
    {
        programPipeline->getExecutable().getInfoLog(bufSize, length, infoLog);
    }
    else
    {
        *length  = 0;
        *infoLog = '\0';
    }
}

void Context::getShaderiv(ShaderProgramID shader, GLenum pname, GLint *params)
{
    Shader *shaderObject = nullptr;
    if (!isContextLost())
    {
        shaderObject = getShader(shader);
        ASSERT(shaderObject);
    }
    QueryShaderiv(this, shaderObject, pname, params);
}

void Context::getShaderivRobust(ShaderProgramID shader,
                                GLenum pname,
                                GLsizei bufSize,
                                GLsizei *length,
                                GLint *params)
{
    getShaderiv(shader, pname, params);
}

void Context::getShaderInfoLog(ShaderProgramID shader,
                               GLsizei bufsize,
                               GLsizei *length,
                               GLchar *infolog)
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

void Context::getShaderSource(ShaderProgramID shader,
                              GLsizei bufsize,
                              GLsizei *length,
                              GLchar *source)
{
    Shader *shaderObject = getShader(shader);
    ASSERT(shaderObject);
    shaderObject->getSource(bufsize, length, source);
}

void Context::getUniformfv(ShaderProgramID program, UniformLocation location, GLfloat *params)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->getUniformfv(this, location, params);
}

void Context::getUniformfvRobust(ShaderProgramID program,
                                 UniformLocation location,
                                 GLsizei bufSize,
                                 GLsizei *length,
                                 GLfloat *params)
{
    getUniformfv(program, location, params);
}

void Context::getUniformiv(ShaderProgramID program, UniformLocation location, GLint *params)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->getUniformiv(this, location, params);
}

void Context::getUniformivRobust(ShaderProgramID program,
                                 UniformLocation location,
                                 GLsizei bufSize,
                                 GLsizei *length,
                                 GLint *params)
{
    getUniformiv(program, location, params);
}

GLint Context::getUniformLocation(ShaderProgramID program, const GLchar *name)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    return programObject->getUniformLocation(name).value;
}

GLboolean Context::isBuffer(BufferID buffer) const
{
    if (buffer.value == 0)
    {
        return GL_FALSE;
    }

    return ConvertToGLBoolean(getBuffer(buffer));
}

GLboolean Context::isEnabled(GLenum cap) const
{
    return mState.getEnableFeature(cap);
}

GLboolean Context::isEnabledi(GLenum target, GLuint index) const
{
    return mState.getEnableFeatureIndexed(target, index);
}

GLboolean Context::isFramebuffer(FramebufferID framebuffer) const
{
    if (framebuffer.value == 0)
    {
        return GL_FALSE;
    }

    return ConvertToGLBoolean(getFramebuffer(framebuffer));
}

GLboolean Context::isProgram(ShaderProgramID program) const
{
    if (program.value == 0)
    {
        return GL_FALSE;
    }

    return ConvertToGLBoolean(getProgramNoResolveLink(program));
}

GLboolean Context::isRenderbuffer(RenderbufferID renderbuffer) const
{
    if (renderbuffer.value == 0)
    {
        return GL_FALSE;
    }

    return ConvertToGLBoolean(getRenderbuffer(renderbuffer));
}

GLboolean Context::isShader(ShaderProgramID shader) const
{
    if (shader.value == 0)
    {
        return GL_FALSE;
    }

    return ConvertToGLBoolean(getShader(shader));
}

GLboolean Context::isTexture(TextureID texture) const
{
    if (texture.value == 0)
    {
        return GL_FALSE;
    }

    return ConvertToGLBoolean(getTexture(texture));
}

void Context::linkProgram(ShaderProgramID program)
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
                           const ShaderProgramID *shaders,
                           GLenum binaryformat,
                           const void *binary,
                           GLsizei length)
{
    // No binary shader formats are supported.
    UNIMPLEMENTED();
}

void Context::bindFragDataLocationIndexed(ShaderProgramID program,
                                          GLuint colorNumber,
                                          GLuint index,
                                          const char *name)
{
    Program *programObject = getProgramNoResolveLink(program);
    programObject->bindFragmentOutputLocation(colorNumber, name);
    programObject->bindFragmentOutputIndex(index, name);
}

void Context::bindFragDataLocation(ShaderProgramID program, GLuint colorNumber, const char *name)
{
    bindFragDataLocationIndexed(program, colorNumber, 0u, name);
}

int Context::getFragDataIndex(ShaderProgramID program, const char *name)
{
    Program *programObject = getProgramResolveLink(program);
    return programObject->getFragDataIndex(name);
}

int Context::getProgramResourceLocationIndex(ShaderProgramID program,
                                             GLenum programInterface,
                                             const char *name)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programInterface == GL_PROGRAM_OUTPUT);
    return programObject->getFragDataIndex(name);
}

void Context::shaderSource(ShaderProgramID shader,
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

void Context::patchParameteri(GLenum pname, GLint value)
{
    switch (pname)
    {
        case GL_PATCH_VERTICES:
            mState.setPatchVertices(value);
            break;
        default:
            break;
    }
}

Program *Context::getActiveLinkedProgram() const
{
    Program *program = mState.getLinkedProgram(this);
    if (!program)
    {
        ProgramPipeline *programPipelineObject = mState.getProgramPipeline();
        if (programPipelineObject)
        {
            program = programPipelineObject->getLinkedActiveShaderProgram(this);
        }
    }

    return program;
}

void Context::uniform1f(UniformLocation location, GLfloat x)
{
    Program *program = getActiveLinkedProgram();
    program->setUniform1fv(location, 1, &x);
}

void Context::uniform1fv(UniformLocation location, GLsizei count, const GLfloat *v)
{
    Program *program = getActiveLinkedProgram();
    program->setUniform1fv(location, count, v);
}

void Context::setUniform1iImpl(Program *program,
                               UniformLocation location,
                               GLsizei count,
                               const GLint *v)
{
    program->setUniform1iv(this, location, count, v);
}

void Context::onSamplerUniformChange(size_t textureUnitIndex)
{
    mState.onActiveTextureChange(this, textureUnitIndex);
    mStateCache.onActiveTextureChange(this);
}

void Context::uniform1i(UniformLocation location, GLint x)
{
    Program *program = getActiveLinkedProgram();
    setUniform1iImpl(program, location, 1, &x);
}

void Context::uniform1iv(UniformLocation location, GLsizei count, const GLint *v)
{
    Program *program = getActiveLinkedProgram();
    setUniform1iImpl(program, location, count, v);
}

void Context::uniform2f(UniformLocation location, GLfloat x, GLfloat y)
{
    GLfloat xy[2]    = {x, y};
    Program *program = getActiveLinkedProgram();
    program->setUniform2fv(location, 1, xy);
}

void Context::uniform2fv(UniformLocation location, GLsizei count, const GLfloat *v)
{
    Program *program = getActiveLinkedProgram();
    program->setUniform2fv(location, count, v);
}

void Context::uniform2i(UniformLocation location, GLint x, GLint y)
{
    GLint xy[2]      = {x, y};
    Program *program = getActiveLinkedProgram();
    program->setUniform2iv(location, 1, xy);
}

void Context::uniform2iv(UniformLocation location, GLsizei count, const GLint *v)
{
    Program *program = getActiveLinkedProgram();
    program->setUniform2iv(location, count, v);
}

void Context::uniform3f(UniformLocation location, GLfloat x, GLfloat y, GLfloat z)
{
    GLfloat xyz[3]   = {x, y, z};
    Program *program = getActiveLinkedProgram();
    program->setUniform3fv(location, 1, xyz);
}

void Context::uniform3fv(UniformLocation location, GLsizei count, const GLfloat *v)
{
    Program *program = getActiveLinkedProgram();
    program->setUniform3fv(location, count, v);
}

void Context::uniform3i(UniformLocation location, GLint x, GLint y, GLint z)
{
    GLint xyz[3]     = {x, y, z};
    Program *program = getActiveLinkedProgram();
    program->setUniform3iv(location, 1, xyz);
}

void Context::uniform3iv(UniformLocation location, GLsizei count, const GLint *v)
{
    Program *program = getActiveLinkedProgram();
    program->setUniform3iv(location, count, v);
}

void Context::uniform4f(UniformLocation location, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    GLfloat xyzw[4]  = {x, y, z, w};
    Program *program = getActiveLinkedProgram();
    program->setUniform4fv(location, 1, xyzw);
}

void Context::uniform4fv(UniformLocation location, GLsizei count, const GLfloat *v)
{
    Program *program = getActiveLinkedProgram();
    program->setUniform4fv(location, count, v);
}

void Context::uniform4i(UniformLocation location, GLint x, GLint y, GLint z, GLint w)
{
    GLint xyzw[4]    = {x, y, z, w};
    Program *program = getActiveLinkedProgram();
    program->setUniform4iv(location, 1, xyzw);
}

void Context::uniform4iv(UniformLocation location, GLsizei count, const GLint *v)
{
    Program *program = getActiveLinkedProgram();
    program->setUniform4iv(location, count, v);
}

void Context::uniformMatrix2fv(UniformLocation location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value)
{
    Program *program = getActiveLinkedProgram();
    program->setUniformMatrix2fv(location, count, transpose, value);
}

void Context::uniformMatrix3fv(UniformLocation location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value)
{
    Program *program = getActiveLinkedProgram();
    program->setUniformMatrix3fv(location, count, transpose, value);
}

void Context::uniformMatrix4fv(UniformLocation location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value)
{
    Program *program = getActiveLinkedProgram();
    program->setUniformMatrix4fv(location, count, transpose, value);
}

void Context::validateProgram(ShaderProgramID program)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->validate(mState.mCaps);
}

void Context::validateProgramPipeline(ProgramPipelineID pipeline)
{
    // GLES spec 3.2, Section 7.4 "Program Pipeline Objects"
    // If pipeline is a name that has been generated (without subsequent deletion) by
    // GenProgramPipelines, but refers to a program pipeline object that has not been
    // previously bound, the GL first creates a new state vector in the same manner as
    // when BindProgramPipeline creates a new program pipeline object.
    //
    // void BindProgramPipeline( uint pipeline );
    // pipeline is the program pipeline object name. The resulting program pipeline
    // object is a new state vector, comprising all the state and with the same initial values
    // listed in table 21.20.
    //
    // If we do not have a pipeline object that's been created with glBindProgramPipeline, we leave
    // VALIDATE_STATUS at it's default false value without generating a pipeline object.
    if (!getProgramPipeline(pipeline))
    {
        return;
    }

    ProgramPipeline *programPipeline =
        mState.mProgramPipelineManager->checkProgramPipelineAllocation(mImplementation.get(),
                                                                       pipeline);
    ASSERT(programPipeline);

    programPipeline->validate(this);
}

void Context::getProgramBinary(ShaderProgramID program,
                               GLsizei bufSize,
                               GLsizei *length,
                               GLenum *binaryFormat,
                               void *binary)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject != nullptr);

    ANGLE_CONTEXT_TRY(programObject->saveBinary(this, binaryFormat, binary, bufSize, length));
}

void Context::programBinary(ShaderProgramID program,
                            GLenum binaryFormat,
                            const void *binary,
                            GLsizei length)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject != nullptr);

    ANGLE_CONTEXT_TRY(programObject->loadBinary(this, binaryFormat, binary, length));
    ANGLE_CONTEXT_TRY(onProgramLink(programObject));
}

void Context::uniform1ui(UniformLocation location, GLuint v0)
{
    Program *program = getActiveLinkedProgram();
    program->setUniform1uiv(location, 1, &v0);
}

void Context::uniform2ui(UniformLocation location, GLuint v0, GLuint v1)
{
    Program *program  = getActiveLinkedProgram();
    const GLuint xy[] = {v0, v1};
    program->setUniform2uiv(location, 1, xy);
}

void Context::uniform3ui(UniformLocation location, GLuint v0, GLuint v1, GLuint v2)
{
    Program *program   = getActiveLinkedProgram();
    const GLuint xyz[] = {v0, v1, v2};
    program->setUniform3uiv(location, 1, xyz);
}

void Context::uniform4ui(UniformLocation location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
    Program *program    = getActiveLinkedProgram();
    const GLuint xyzw[] = {v0, v1, v2, v3};
    program->setUniform4uiv(location, 1, xyzw);
}

void Context::uniform1uiv(UniformLocation location, GLsizei count, const GLuint *value)
{
    Program *program = getActiveLinkedProgram();
    program->setUniform1uiv(location, count, value);
}
void Context::uniform2uiv(UniformLocation location, GLsizei count, const GLuint *value)
{
    Program *program = getActiveLinkedProgram();
    program->setUniform2uiv(location, count, value);
}

void Context::uniform3uiv(UniformLocation location, GLsizei count, const GLuint *value)
{
    Program *program = getActiveLinkedProgram();
    program->setUniform3uiv(location, count, value);
}

void Context::uniform4uiv(UniformLocation location, GLsizei count, const GLuint *value)
{
    Program *program = getActiveLinkedProgram();
    program->setUniform4uiv(location, count, value);
}

void Context::genQueries(GLsizei n, QueryID *ids)
{
    for (GLsizei i = 0; i < n; i++)
    {
        QueryID handle = QueryID{mQueryHandleAllocator.allocate()};
        mQueryMap.assign(handle, nullptr);
        ids[i] = handle;
    }
}

void Context::deleteQueries(GLsizei n, const QueryID *ids)
{
    for (int i = 0; i < n; i++)
    {
        QueryID query = ids[i];

        Query *queryObject = nullptr;
        if (mQueryMap.erase(query, &queryObject))
        {
            mQueryHandleAllocator.release(query.value);
            if (queryObject)
            {
                queryObject->release(this);
            }
        }
    }
}

bool Context::isQueryGenerated(QueryID query) const
{
    return mQueryMap.contains(query);
}

GLboolean Context::isQuery(QueryID id) const
{
    return ConvertToGLBoolean(getQuery(id) != nullptr);
}

void Context::uniformMatrix2x3fv(UniformLocation location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value)
{
    Program *program = getActiveLinkedProgram();
    program->setUniformMatrix2x3fv(location, count, transpose, value);
}

void Context::uniformMatrix3x2fv(UniformLocation location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value)
{
    Program *program = getActiveLinkedProgram();
    program->setUniformMatrix3x2fv(location, count, transpose, value);
}

void Context::uniformMatrix2x4fv(UniformLocation location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value)
{
    Program *program = getActiveLinkedProgram();
    program->setUniformMatrix2x4fv(location, count, transpose, value);
}

void Context::uniformMatrix4x2fv(UniformLocation location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value)
{
    Program *program = getActiveLinkedProgram();
    program->setUniformMatrix4x2fv(location, count, transpose, value);
}

void Context::uniformMatrix3x4fv(UniformLocation location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value)
{
    Program *program = getActiveLinkedProgram();
    program->setUniformMatrix3x4fv(location, count, transpose, value);
}

void Context::uniformMatrix4x3fv(UniformLocation location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value)
{
    Program *program = getActiveLinkedProgram();
    program->setUniformMatrix4x3fv(location, count, transpose, value);
}

void Context::deleteVertexArrays(GLsizei n, const VertexArrayID *arrays)
{
    for (int arrayIndex = 0; arrayIndex < n; arrayIndex++)
    {
        VertexArrayID vertexArray = arrays[arrayIndex];

        if (arrays[arrayIndex].value != 0)
        {
            VertexArray *vertexArrayObject = nullptr;
            if (mVertexArrayMap.erase(vertexArray, &vertexArrayObject))
            {
                if (vertexArrayObject != nullptr)
                {
                    detachVertexArray(vertexArray);
                    vertexArrayObject->onDestroy(this);
                }

                mVertexArrayHandleAllocator.release(vertexArray.value);
            }
        }
    }
}

void Context::genVertexArrays(GLsizei n, VertexArrayID *arrays)
{
    for (int arrayIndex = 0; arrayIndex < n; arrayIndex++)
    {
        VertexArrayID vertexArray = {mVertexArrayHandleAllocator.allocate()};
        mVertexArrayMap.assign(vertexArray, nullptr);
        arrays[arrayIndex] = vertexArray;
    }
}

GLboolean Context::isVertexArray(VertexArrayID array) const
{
    if (array.value == 0)
    {
        return GL_FALSE;
    }

    VertexArray *vao = getVertexArray(array);
    return ConvertToGLBoolean(vao != nullptr);
}

void Context::endTransformFeedback()
{
    TransformFeedback *transformFeedback = mState.getCurrentTransformFeedback();
    ANGLE_CONTEXT_TRY(transformFeedback->end(this));
    mStateCache.onActiveTransformFeedbackChange(this);
}

void Context::transformFeedbackVaryings(ShaderProgramID program,
                                        GLsizei count,
                                        const GLchar *const *varyings,
                                        GLenum bufferMode)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setTransformFeedbackVaryings(count, varyings, bufferMode);
}

void Context::getTransformFeedbackVarying(ShaderProgramID program,
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

void Context::deleteTransformFeedbacks(GLsizei n, const TransformFeedbackID *ids)
{
    for (int i = 0; i < n; i++)
    {
        TransformFeedbackID transformFeedback = ids[i];
        if (transformFeedback.value == 0)
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

            mTransformFeedbackHandleAllocator.release(transformFeedback.value);
        }
    }
}

void Context::genTransformFeedbacks(GLsizei n, TransformFeedbackID *ids)
{
    for (int i = 0; i < n; i++)
    {
        TransformFeedbackID transformFeedback = {mTransformFeedbackHandleAllocator.allocate()};
        mTransformFeedbackMap.assign(transformFeedback, nullptr);
        ids[i] = transformFeedback;
    }
}

GLboolean Context::isTransformFeedback(TransformFeedbackID id) const
{
    if (id.value == 0)
    {
        // The 3.0.4 spec [section 6.1.11] states that if ID is zero, IsTransformFeedback
        // returns FALSE
        return GL_FALSE;
    }

    const TransformFeedback *transformFeedback = getTransformFeedback(id);
    return ConvertToGLBoolean(transformFeedback != nullptr);
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

void Context::getUniformuiv(ShaderProgramID program, UniformLocation location, GLuint *params)
{
    const Program *programObject = getProgramResolveLink(program);
    programObject->getUniformuiv(this, location, params);
}

void Context::getUniformuivRobust(ShaderProgramID program,
                                  UniformLocation location,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLuint *params)
{
    getUniformuiv(program, location, params);
}

GLint Context::getFragDataLocation(ShaderProgramID program, const GLchar *name)
{
    const Program *programObject = getProgramResolveLink(program);
    return programObject->getFragDataLocation(name);
}

void Context::getUniformIndices(ShaderProgramID program,
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

void Context::getActiveUniformsiv(ShaderProgramID program,
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

GLuint Context::getUniformBlockIndex(ShaderProgramID program, const GLchar *uniformBlockName)
{
    const Program *programObject = getProgramResolveLink(program);
    return programObject->getUniformBlockIndex(uniformBlockName);
}

void Context::getActiveUniformBlockiv(ShaderProgramID program,
                                      UniformBlockIndex uniformBlockIndex,
                                      GLenum pname,
                                      GLint *params)
{
    const Program *programObject = getProgramResolveLink(program);
    QueryActiveUniformBlockiv(programObject, uniformBlockIndex, pname, params);
}

void Context::getActiveUniformBlockivRobust(ShaderProgramID program,
                                            UniformBlockIndex uniformBlockIndex,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLint *params)
{
    getActiveUniformBlockiv(program, uniformBlockIndex, pname, params);
}

void Context::getActiveUniformBlockName(ShaderProgramID program,
                                        UniformBlockIndex uniformBlockIndex,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLchar *uniformBlockName)
{
    const Program *programObject = getProgramResolveLink(program);
    programObject->getActiveUniformBlockName(this, uniformBlockIndex, bufSize, length,
                                             uniformBlockName);
}

void Context::uniformBlockBinding(ShaderProgramID program,
                                  UniformBlockIndex uniformBlockIndex,
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

GLboolean Context::isSync(GLsync sync) const
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

void Context::genSamplers(GLsizei count, SamplerID *samplers)
{
    for (int i = 0; i < count; i++)
    {
        samplers[i] = mState.mSamplerManager->createSampler();
    }
}

void Context::deleteSamplers(GLsizei count, const SamplerID *samplers)
{
    for (int i = 0; i < count; i++)
    {
        SamplerID sampler = samplers[i];

        if (mState.mSamplerManager->getSampler(sampler))
        {
            detachSampler(sampler);
        }

        mState.mSamplerManager->deleteObject(this, sampler);
    }
}

void Context::minSampleShading(GLfloat value)
{
    mState.setMinSampleShading(value);
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

void Context::programUniform1i(ShaderProgramID program, UniformLocation location, GLint v0)
{
    programUniform1iv(program, location, 1, &v0);
}

void Context::programUniform2i(ShaderProgramID program,
                               UniformLocation location,
                               GLint v0,
                               GLint v1)
{
    GLint xy[2] = {v0, v1};
    programUniform2iv(program, location, 1, xy);
}

void Context::programUniform3i(ShaderProgramID program,
                               UniformLocation location,
                               GLint v0,
                               GLint v1,
                               GLint v2)
{
    GLint xyz[3] = {v0, v1, v2};
    programUniform3iv(program, location, 1, xyz);
}

void Context::programUniform4i(ShaderProgramID program,
                               UniformLocation location,
                               GLint v0,
                               GLint v1,
                               GLint v2,
                               GLint v3)
{
    GLint xyzw[4] = {v0, v1, v2, v3};
    programUniform4iv(program, location, 1, xyzw);
}

void Context::programUniform1ui(ShaderProgramID program, UniformLocation location, GLuint v0)
{
    programUniform1uiv(program, location, 1, &v0);
}

void Context::programUniform2ui(ShaderProgramID program,
                                UniformLocation location,
                                GLuint v0,
                                GLuint v1)
{
    GLuint xy[2] = {v0, v1};
    programUniform2uiv(program, location, 1, xy);
}

void Context::programUniform3ui(ShaderProgramID program,
                                UniformLocation location,
                                GLuint v0,
                                GLuint v1,
                                GLuint v2)
{
    GLuint xyz[3] = {v0, v1, v2};
    programUniform3uiv(program, location, 1, xyz);
}

void Context::programUniform4ui(ShaderProgramID program,
                                UniformLocation location,
                                GLuint v0,
                                GLuint v1,
                                GLuint v2,
                                GLuint v3)
{
    GLuint xyzw[4] = {v0, v1, v2, v3};
    programUniform4uiv(program, location, 1, xyzw);
}

void Context::programUniform1f(ShaderProgramID program, UniformLocation location, GLfloat v0)
{
    programUniform1fv(program, location, 1, &v0);
}

void Context::programUniform2f(ShaderProgramID program,
                               UniformLocation location,
                               GLfloat v0,
                               GLfloat v1)
{
    GLfloat xy[2] = {v0, v1};
    programUniform2fv(program, location, 1, xy);
}

void Context::programUniform3f(ShaderProgramID program,
                               UniformLocation location,
                               GLfloat v0,
                               GLfloat v1,
                               GLfloat v2)
{
    GLfloat xyz[3] = {v0, v1, v2};
    programUniform3fv(program, location, 1, xyz);
}

void Context::programUniform4f(ShaderProgramID program,
                               UniformLocation location,
                               GLfloat v0,
                               GLfloat v1,
                               GLfloat v2,
                               GLfloat v3)
{
    GLfloat xyzw[4] = {v0, v1, v2, v3};
    programUniform4fv(program, location, 1, xyzw);
}

void Context::programUniform1iv(ShaderProgramID program,
                                UniformLocation location,
                                GLsizei count,
                                const GLint *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    setUniform1iImpl(programObject, location, count, value);
}

void Context::programUniform2iv(ShaderProgramID program,
                                UniformLocation location,
                                GLsizei count,
                                const GLint *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniform2iv(location, count, value);
}

void Context::programUniform3iv(ShaderProgramID program,
                                UniformLocation location,
                                GLsizei count,
                                const GLint *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniform3iv(location, count, value);
}

void Context::programUniform4iv(ShaderProgramID program,
                                UniformLocation location,
                                GLsizei count,
                                const GLint *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniform4iv(location, count, value);
}

void Context::programUniform1uiv(ShaderProgramID program,
                                 UniformLocation location,
                                 GLsizei count,
                                 const GLuint *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniform1uiv(location, count, value);
}

void Context::programUniform2uiv(ShaderProgramID program,
                                 UniformLocation location,
                                 GLsizei count,
                                 const GLuint *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniform2uiv(location, count, value);
}

void Context::programUniform3uiv(ShaderProgramID program,
                                 UniformLocation location,
                                 GLsizei count,
                                 const GLuint *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniform3uiv(location, count, value);
}

void Context::programUniform4uiv(ShaderProgramID program,
                                 UniformLocation location,
                                 GLsizei count,
                                 const GLuint *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniform4uiv(location, count, value);
}

void Context::programUniform1fv(ShaderProgramID program,
                                UniformLocation location,
                                GLsizei count,
                                const GLfloat *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniform1fv(location, count, value);
}

void Context::programUniform2fv(ShaderProgramID program,
                                UniformLocation location,
                                GLsizei count,
                                const GLfloat *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniform2fv(location, count, value);
}

void Context::programUniform3fv(ShaderProgramID program,
                                UniformLocation location,
                                GLsizei count,
                                const GLfloat *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniform3fv(location, count, value);
}

void Context::programUniform4fv(ShaderProgramID program,
                                UniformLocation location,
                                GLsizei count,
                                const GLfloat *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniform4fv(location, count, value);
}

void Context::programUniformMatrix2fv(ShaderProgramID program,
                                      UniformLocation location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniformMatrix2fv(location, count, transpose, value);
}

void Context::programUniformMatrix3fv(ShaderProgramID program,
                                      UniformLocation location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniformMatrix3fv(location, count, transpose, value);
}

void Context::programUniformMatrix4fv(ShaderProgramID program,
                                      UniformLocation location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniformMatrix4fv(location, count, transpose, value);
}

void Context::programUniformMatrix2x3fv(ShaderProgramID program,
                                        UniformLocation location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniformMatrix2x3fv(location, count, transpose, value);
}

void Context::programUniformMatrix3x2fv(ShaderProgramID program,
                                        UniformLocation location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniformMatrix3x2fv(location, count, transpose, value);
}

void Context::programUniformMatrix2x4fv(ShaderProgramID program,
                                        UniformLocation location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniformMatrix2x4fv(location, count, transpose, value);
}

void Context::programUniformMatrix4x2fv(ShaderProgramID program,
                                        UniformLocation location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniformMatrix4x2fv(location, count, transpose, value);
}

void Context::programUniformMatrix3x4fv(ShaderProgramID program,
                                        UniformLocation location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);
    programObject->setUniformMatrix3x4fv(location, count, transpose, value);
}

void Context::programUniformMatrix4x3fv(ShaderProgramID program,
                                        UniformLocation location,
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

void Context::genProgramPipelines(GLsizei count, ProgramPipelineID *pipelines)
{
    for (int i = 0; i < count; i++)
    {
        pipelines[i] = createProgramPipeline();
    }
}

void Context::deleteProgramPipelines(GLsizei count, const ProgramPipelineID *pipelines)
{
    for (int i = 0; i < count; i++)
    {
        if (pipelines[i].value != 0)
        {
            deleteProgramPipeline(pipelines[i]);
        }
    }
}

GLboolean Context::isProgramPipeline(ProgramPipelineID pipeline) const
{
    if (pipeline.value == 0)
    {
        return GL_FALSE;
    }

    if (getProgramPipeline(pipeline))
    {
        return GL_TRUE;
    }

    return GL_FALSE;
}

void Context::finishFenceNV(FenceNVID fence)
{
    FenceNV *fenceObject = getFenceNV(fence);

    ASSERT(fenceObject && fenceObject->isSet());
    ANGLE_CONTEXT_TRY(fenceObject->finish(this));
}

void Context::getFenceivNV(FenceNVID fence, GLenum pname, GLint *params)
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

void Context::getTranslatedShaderSource(ShaderProgramID shader,
                                        GLsizei bufsize,
                                        GLsizei *length,
                                        GLchar *source)
{
    Shader *shaderObject = getShader(shader);
    ASSERT(shaderObject);
    shaderObject->getTranslatedSourceWithDebugInfo(this, bufsize, length, source);
}

void Context::getnUniformfv(ShaderProgramID program,
                            UniformLocation location,
                            GLsizei bufSize,
                            GLfloat *params)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);

    programObject->getUniformfv(this, location, params);
}

void Context::getnUniformfvRobust(ShaderProgramID program,
                                  UniformLocation location,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLfloat *params)
{
    UNIMPLEMENTED();
}

void Context::getnUniformiv(ShaderProgramID program,
                            UniformLocation location,
                            GLsizei bufSize,
                            GLint *params)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);

    programObject->getUniformiv(this, location, params);
}

void Context::getnUniformuiv(ShaderProgramID program,
                             UniformLocation location,
                             GLsizei bufSize,
                             GLuint *params)
{
    Program *programObject = getProgramResolveLink(program);
    ASSERT(programObject);

    programObject->getUniformuiv(this, location, params);
}

void Context::getnUniformivRobust(ShaderProgramID program,
                                  UniformLocation location,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLint *params)
{
    UNIMPLEMENTED();
}

void Context::getnUniformuivRobust(ShaderProgramID program,
                                   UniformLocation location,
                                   GLsizei bufSize,
                                   GLsizei *length,
                                   GLuint *params)
{
    UNIMPLEMENTED();
}

GLboolean Context::isFenceNV(FenceNVID fence) const
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

void Context::setFenceNV(FenceNVID fence, GLenum condition)
{
    ASSERT(condition == GL_ALL_COMPLETED_NV);

    FenceNV *fenceObject = getFenceNV(fence);
    ASSERT(fenceObject != nullptr);
    ANGLE_CONTEXT_TRY(fenceObject->set(this, condition));
}

GLboolean Context::testFenceNV(FenceNVID fence)
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

void Context::deleteMemoryObjects(GLsizei n, const MemoryObjectID *memoryObjects)
{
    for (int i = 0; i < n; i++)
    {
        deleteMemoryObject(memoryObjects[i]);
    }
}

GLboolean Context::isMemoryObject(MemoryObjectID memoryObject) const
{
    if (memoryObject.value == 0)
    {
        return GL_FALSE;
    }

    return ConvertToGLBoolean(getMemoryObject(memoryObject));
}

void Context::createMemoryObjects(GLsizei n, MemoryObjectID *memoryObjects)
{
    for (int i = 0; i < n; i++)
    {
        memoryObjects[i] = createMemoryObject();
    }
}

void Context::memoryObjectParameteriv(MemoryObjectID memory, GLenum pname, const GLint *params)
{
    MemoryObject *memoryObject = getMemoryObject(memory);
    ASSERT(memoryObject);
    ANGLE_CONTEXT_TRY(SetMemoryObjectParameteriv(this, memoryObject, pname, params));
}

void Context::getMemoryObjectParameteriv(MemoryObjectID memory, GLenum pname, GLint *params)
{
    const MemoryObject *memoryObject = getMemoryObject(memory);
    ASSERT(memoryObject);
    QueryMemoryObjectParameteriv(memoryObject, pname, params);
}

void Context::texStorageMem2D(TextureType target,
                              GLsizei levels,
                              GLenum internalFormat,
                              GLsizei width,
                              GLsizei height,
                              MemoryObjectID memory,
                              GLuint64 offset)
{
    texStorageMemFlags2D(target, levels, internalFormat, width, height, memory, offset, 0,
                         std::numeric_limits<uint32_t>::max(), nullptr);
}

void Context::texStorageMem2DMultisample(TextureType target,
                                         GLsizei samples,
                                         GLenum internalFormat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLboolean fixedSampleLocations,
                                         MemoryObjectID memory,
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
                              MemoryObjectID memory,
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
                                         MemoryObjectID memory,
                                         GLuint64 offset)
{
    UNIMPLEMENTED();
}

void Context::bufferStorageMem(TextureType target,
                               GLsizeiptr size,
                               MemoryObjectID memory,
                               GLuint64 offset)
{
    UNIMPLEMENTED();
}

void Context::importMemoryFd(MemoryObjectID memory, GLuint64 size, HandleType handleType, GLint fd)
{
    MemoryObject *memoryObject = getMemoryObject(memory);
    ASSERT(memoryObject != nullptr);
    ANGLE_CONTEXT_TRY(memoryObject->importFd(this, size, handleType, fd));
}

void Context::texStorageMemFlags2D(TextureType target,
                                   GLsizei levels,
                                   GLenum internalFormat,
                                   GLsizei width,
                                   GLsizei height,
                                   MemoryObjectID memory,
                                   GLuint64 offset,
                                   GLbitfield createFlags,
                                   GLbitfield usageFlags,
                                   const void *imageCreateInfoPNext)
{
    MemoryObject *memoryObject = getMemoryObject(memory);
    ASSERT(memoryObject);
    Extents size(width, height, 1);
    Texture *texture = getTextureByType(target);
    ANGLE_CONTEXT_TRY(texture->setStorageExternalMemory(this, target, levels, internalFormat, size,
                                                        memoryObject, offset, createFlags,
                                                        usageFlags, imageCreateInfoPNext));
}

void Context::texStorageMemFlags2DMultisample(TextureType target,
                                              GLsizei samples,
                                              GLenum internalFormat,
                                              GLsizei width,
                                              GLsizei height,
                                              GLboolean fixedSampleLocations,
                                              MemoryObjectID memory,
                                              GLuint64 offset,
                                              GLbitfield createFlags,
                                              GLbitfield usageFlags,
                                              const void *imageCreateInfoPNext)
{
    UNIMPLEMENTED();
}

void Context::texStorageMemFlags3D(TextureType target,
                                   GLsizei levels,
                                   GLenum internalFormat,
                                   GLsizei width,
                                   GLsizei height,
                                   GLsizei depth,
                                   MemoryObjectID memory,
                                   GLuint64 offset,
                                   GLbitfield createFlags,
                                   GLbitfield usageFlags,
                                   const void *imageCreateInfoPNext)
{
    UNIMPLEMENTED();
}

void Context::texStorageMemFlags3DMultisample(TextureType target,
                                              GLsizei samples,
                                              GLenum internalFormat,
                                              GLsizei width,
                                              GLsizei height,
                                              GLsizei depth,
                                              GLboolean fixedSampleLocations,
                                              MemoryObjectID memory,
                                              GLuint64 offset,
                                              GLbitfield createFlags,
                                              GLbitfield usageFlags,
                                              const void *imageCreateInfoPNext)
{
    UNIMPLEMENTED();
}

void Context::importMemoryZirconHandle(MemoryObjectID memory,
                                       GLuint64 size,
                                       HandleType handleType,
                                       GLuint handle)
{
    MemoryObject *memoryObject = getMemoryObject(memory);
    ASSERT(memoryObject != nullptr);
    ANGLE_CONTEXT_TRY(memoryObject->importZirconHandle(this, size, handleType, handle));
}

void Context::genSemaphores(GLsizei n, SemaphoreID *semaphores)
{
    for (int i = 0; i < n; i++)
    {
        semaphores[i] = createSemaphore();
    }
}

void Context::deleteSemaphores(GLsizei n, const SemaphoreID *semaphores)
{
    for (int i = 0; i < n; i++)
    {
        deleteSemaphore(semaphores[i]);
    }
}

GLboolean Context::isSemaphore(SemaphoreID semaphore) const
{
    if (semaphore.value == 0)
    {
        return GL_FALSE;
    }

    return ConvertToGLBoolean(getSemaphore(semaphore));
}

void Context::semaphoreParameterui64v(SemaphoreID semaphore, GLenum pname, const GLuint64 *params)
{
    UNIMPLEMENTED();
}

void Context::getSemaphoreParameterui64v(SemaphoreID semaphore, GLenum pname, GLuint64 *params)
{
    UNIMPLEMENTED();
}

void Context::acquireTextures(GLuint numTextures,
                              const TextureID *textureIds,
                              const GLenum *layouts)
{
    TextureBarrierVector textureBarriers(numTextures);
    for (size_t i = 0; i < numTextures; i++)
    {
        textureBarriers[i].texture = getTexture(textureIds[i]);
        textureBarriers[i].layout  = layouts[i];
    }
    ANGLE_CONTEXT_TRY(mImplementation->acquireTextures(this, textureBarriers));
}

void Context::releaseTextures(GLuint numTextures, const TextureID *textureIds, GLenum *layouts)
{
    TextureBarrierVector textureBarriers(numTextures);
    for (size_t i = 0; i < numTextures; i++)
    {
        textureBarriers[i].texture = getTexture(textureIds[i]);
    }
    ANGLE_CONTEXT_TRY(mImplementation->releaseTextures(this, &textureBarriers));
    for (size_t i = 0; i < numTextures; i++)
    {
        layouts[i] = textureBarriers[i].layout;
    }
}

void Context::waitSemaphore(SemaphoreID semaphoreHandle,
                            GLuint numBufferBarriers,
                            const BufferID *buffers,
                            GLuint numTextureBarriers,
                            const TextureID *textures,
                            const GLenum *srcLayouts)
{
    Semaphore *semaphore = getSemaphore(semaphoreHandle);
    ASSERT(semaphore);

    BufferBarrierVector bufferBarriers(numBufferBarriers);
    for (GLuint bufferBarrierIdx = 0; bufferBarrierIdx < numBufferBarriers; bufferBarrierIdx++)
    {
        bufferBarriers[bufferBarrierIdx] = getBuffer(buffers[bufferBarrierIdx]);
    }

    TextureBarrierVector textureBarriers(numTextureBarriers);
    for (GLuint textureBarrierIdx = 0; textureBarrierIdx < numTextureBarriers; textureBarrierIdx++)
    {
        textureBarriers[textureBarrierIdx].texture = getTexture(textures[textureBarrierIdx]);
        textureBarriers[textureBarrierIdx].layout  = srcLayouts[textureBarrierIdx];
    }

    ANGLE_CONTEXT_TRY(semaphore->wait(this, bufferBarriers, textureBarriers));
}

void Context::signalSemaphore(SemaphoreID semaphoreHandle,
                              GLuint numBufferBarriers,
                              const BufferID *buffers,
                              GLuint numTextureBarriers,
                              const TextureID *textures,
                              const GLenum *dstLayouts)
{
    Semaphore *semaphore = getSemaphore(semaphoreHandle);
    ASSERT(semaphore);

    BufferBarrierVector bufferBarriers(numBufferBarriers);
    for (GLuint bufferBarrierIdx = 0; bufferBarrierIdx < numBufferBarriers; bufferBarrierIdx++)
    {
        bufferBarriers[bufferBarrierIdx] = getBuffer(buffers[bufferBarrierIdx]);
    }

    TextureBarrierVector textureBarriers(numTextureBarriers);
    for (GLuint textureBarrierIdx = 0; textureBarrierIdx < numTextureBarriers; textureBarrierIdx++)
    {
        textureBarriers[textureBarrierIdx].texture = getTexture(textures[textureBarrierIdx]);
        textureBarriers[textureBarrierIdx].layout  = dstLayouts[textureBarrierIdx];
    }

    ANGLE_CONTEXT_TRY(semaphore->signal(this, bufferBarriers, textureBarriers));
}

void Context::importSemaphoreFd(SemaphoreID semaphore, HandleType handleType, GLint fd)
{
    Semaphore *semaphoreObject = getSemaphore(semaphore);
    ASSERT(semaphoreObject != nullptr);
    ANGLE_CONTEXT_TRY(semaphoreObject->importFd(this, handleType, fd));
}

void Context::importSemaphoreZirconHandle(SemaphoreID semaphore,
                                          HandleType handleType,
                                          GLuint handle)
{
    Semaphore *semaphoreObject = getSemaphore(semaphore);
    ASSERT(semaphoreObject != nullptr);
    ANGLE_CONTEXT_TRY(semaphoreObject->importZirconHandle(this, handleType, handle));
}

void Context::framebufferMemorylessPixelLocalStorage(GLint plane, GLenum internalformat)
{
    Framebuffer *framebuffer = mState.getDrawFramebuffer();
    ASSERT(framebuffer);
    PixelLocalStorage &pls = framebuffer->getPixelLocalStorage(this);

    if (internalformat == GL_NONE)
    {
        pls.deinitialize(this, plane);
    }
    else
    {
        pls.setMemoryless(this, plane, internalformat);
    }
}

void Context::framebufferTexturePixelLocalStorage(GLint plane,
                                                  TextureID backingtexture,
                                                  GLint level,
                                                  GLint layer)
{
    Framebuffer *framebuffer = mState.getDrawFramebuffer();
    ASSERT(framebuffer);
    PixelLocalStorage &pls = framebuffer->getPixelLocalStorage(this);

    if (backingtexture.value == 0)
    {
        pls.deinitialize(this, plane);
    }
    else
    {
        Texture *tex = getTexture(backingtexture);
        ASSERT(tex);  // Validation guarantees this.
        pls.setTextureBacked(this, plane, tex, level, layer);
    }
}

void Context::framebufferPixelLocalClearValuefv(GLint plane, const GLfloat value[])
{
    Framebuffer *framebuffer = mState.getDrawFramebuffer();
    ASSERT(framebuffer);
    PixelLocalStorage &pls = framebuffer->getPixelLocalStorage(this);
    pls.setClearValuef(plane, value);
}

void Context::framebufferPixelLocalClearValueiv(GLint plane, const GLint value[])
{
    Framebuffer *framebuffer = mState.getDrawFramebuffer();
    ASSERT(framebuffer);
    PixelLocalStorage &pls = framebuffer->getPixelLocalStorage(this);
    pls.setClearValuei(plane, value);
}

void Context::framebufferPixelLocalClearValueuiv(GLint plane, const GLuint value[])
{
    Framebuffer *framebuffer = mState.getDrawFramebuffer();
    ASSERT(framebuffer);
    PixelLocalStorage &pls = framebuffer->getPixelLocalStorage(this);
    pls.setClearValueui(plane, value);
}

void Context::beginPixelLocalStorage(GLsizei n, const GLenum loadops[])
{
    Framebuffer *framebuffer = mState.getDrawFramebuffer();
    ASSERT(framebuffer);
    PixelLocalStorage &pls = framebuffer->getPixelLocalStorage(this);

    pls.begin(this, n, loadops);
    mState.setPixelLocalStorageActivePlanes(n);
}

void Context::endPixelLocalStorage(GLsizei n, const GLenum storeops[])
{
    Framebuffer *framebuffer = mState.getDrawFramebuffer();
    ASSERT(framebuffer);
    PixelLocalStorage &pls = framebuffer->getPixelLocalStorage(this);

    pls.end(this, storeops);
    mState.setPixelLocalStorageActivePlanes(0);
}

void Context::pixelLocalStorageBarrier()
{
    if (getExtensions().shaderPixelLocalStorageCoherentANGLE)
    {
        return;
    }

    Framebuffer *framebuffer = mState.getDrawFramebuffer();
    ASSERT(framebuffer);
    PixelLocalStorage &pls = framebuffer->getPixelLocalStorage(this);

    pls.barrier(this);
}

void Context::getFramebufferPixelLocalStorageParameterfv(GLint plane, GLenum pname, GLfloat *params)
{
    Framebuffer *framebuffer = mState.getDrawFramebuffer();
    ASSERT(framebuffer);
    PixelLocalStorage &pls = framebuffer->getPixelLocalStorage(this);

    switch (pname)
    {
        case GL_PIXEL_LOCAL_CLEAR_VALUE_FLOAT_ANGLE:
            pls.getPlane(plane).getClearValuef(params);
            break;
    }
}

void Context::getFramebufferPixelLocalStorageParameteriv(GLint plane, GLenum pname, GLint *params)
{
    Framebuffer *framebuffer = mState.getDrawFramebuffer();
    ASSERT(framebuffer);
    PixelLocalStorage &pls = framebuffer->getPixelLocalStorage(this);

    switch (pname)
    {
        // GL_ANGLE_shader_pixel_local_storage.
        case GL_PIXEL_LOCAL_FORMAT_ANGLE:
        case GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE:
        case GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE:
        case GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE:
            *params = pls.getPlane(plane).getIntegeri(this, pname);
            break;
        case GL_PIXEL_LOCAL_CLEAR_VALUE_INT_ANGLE:
            pls.getPlane(plane).getClearValuei(params);
            break;
        case GL_PIXEL_LOCAL_CLEAR_VALUE_UNSIGNED_INT_ANGLE:
        {
            GLuint valueui[4];
            pls.getPlane(plane).getClearValueui(valueui);
            memcpy(params, valueui, sizeof(valueui));
            break;
        }
    }
}

void Context::eGLImageTargetTexStorage(GLenum target, egl::ImageID image, const GLint *attrib_list)
{
    Texture *texture        = getTextureByType(FromGLenum<TextureType>(target));
    egl::Image *imageObject = mDisplay->getImage(image);
    ANGLE_CONTEXT_TRY(texture->setStorageEGLImageTarget(this, FromGLenum<TextureType>(target),
                                                        imageObject, attrib_list));
}

void Context::eGLImageTargetTextureStorage(GLuint texture,
                                           egl::ImageID image,
                                           const GLint *attrib_list)
{}

void Context::eGLImageTargetTexture2D(TextureType target, egl::ImageID image)
{
    Texture *texture        = getTextureByType(target);
    egl::Image *imageObject = mDisplay->getImage(image);
    ANGLE_CONTEXT_TRY(texture->setEGLImageTarget(this, target, imageObject));
}

void Context::eGLImageTargetRenderbufferStorage(GLenum target, egl::ImageID image)
{
    Renderbuffer *renderbuffer = mState.getCurrentRenderbuffer();
    egl::Image *imageObject    = mDisplay->getImage(image);
    ANGLE_CONTEXT_TRY(renderbuffer->setStorageEGLImageTarget(this, imageObject));
}

void Context::framebufferFetchBarrier()
{
    mImplementation->framebufferFetchBarrier();
}

void Context::texStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width)
{
    UNIMPLEMENTED();
}

bool Context::getQueryParameterInfo(GLenum pname, GLenum *type, unsigned int *numParams) const
{
    return GetQueryParameterInfo(mState, pname, type, numParams);
}

bool Context::getIndexedQueryParameterInfo(GLenum target,
                                           GLenum *type,
                                           unsigned int *numParams) const
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

    if (mSupportedExtensions.drawBuffersIndexedAny())
    {
        switch (target)
        {
            case GL_BLEND_SRC_RGB:
            case GL_BLEND_SRC_ALPHA:
            case GL_BLEND_DST_RGB:
            case GL_BLEND_DST_ALPHA:
            case GL_BLEND_EQUATION_RGB:
            case GL_BLEND_EQUATION_ALPHA:
            {
                *type      = GL_INT;
                *numParams = 1;
                return true;
            }
            case GL_COLOR_WRITEMASK:
            {
                *type      = GL_BOOL;
                *numParams = 4;
                return true;
            }
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

Program *Context::getProgramNoResolveLink(ShaderProgramID handle) const
{
    return mState.mShaderProgramManager->getProgram(handle);
}

Shader *Context::getShader(ShaderProgramID handle) const
{
    return mState.mShaderProgramManager->getShader(handle);
}

const angle::FrontendFeatures &Context::getFrontendFeatures() const
{
    return mDisplay->getFrontendFeatures();
}

bool Context::isRenderbufferGenerated(RenderbufferID renderbuffer) const
{
    return mState.mRenderbufferManager->isHandleGenerated(renderbuffer);
}

bool Context::isFramebufferGenerated(FramebufferID framebuffer) const
{
    return mState.mFramebufferManager->isHandleGenerated(framebuffer);
}

bool Context::isProgramPipelineGenerated(ProgramPipelineID pipeline) const
{
    return mState.mProgramPipelineManager->isHandleGenerated(pipeline);
}

bool Context::usingDisplayTextureShareGroup() const
{
    return mDisplayTextureShareGroup;
}

bool Context::usingDisplaySemaphoreShareGroup() const
{
    return mDisplaySemaphoreShareGroup;
}

GLenum Context::getConvertedRenderbufferFormat(GLenum internalformat) const
{
    if (isWebGL() && mState.mClientVersion.major == 2 && internalformat == GL_DEPTH_STENCIL)
    {
        return GL_DEPTH24_STENCIL8;
    }
    if (getClientType() == EGL_OPENGL_API && internalformat == GL_DEPTH_COMPONENT)
    {
        return GL_DEPTH_COMPONENT24;
    }
    return internalformat;
}

void Context::maxShaderCompilerThreads(GLuint count)
{
    // A count of zero specifies a request for no parallel compiling or linking.  This is handled in
    // getShaderCompileThreadPool.  Otherwise the count itself has no effect as the pool is shared
    // between contexts.
    mState.setMaxShaderCompilerThreads(count);
    mImplementation->setMaxShaderCompilerThreads(count);
}

void Context::framebufferParameteriMESA(GLenum target, GLenum pname, GLint param)
{
    framebufferParameteri(target, pname, param);
}

void Context::getFramebufferParameterivMESA(GLenum target, GLenum pname, GLint *params)
{
    getFramebufferParameteriv(target, pname, params);
}

bool Context::isGLES1() const
{
    return mState.isGLES1();
}

std::shared_ptr<angle::WorkerThreadPool> Context::getShaderCompileThreadPool() const
{
    if (mState.mExtensions.parallelShaderCompileKHR && mState.getMaxShaderCompilerThreads() > 0)
    {
        return mDisplay->getMultiThreadPool();
    }
    return mDisplay->getSingleThreadPool();
}

std::shared_ptr<angle::WorkerThreadPool> Context::getWorkerThreadPool() const
{
    return mDisplay->getMultiThreadPool();
}

void Context::onSubjectStateChange(angle::SubjectIndex index, angle::SubjectMessage message)
{
    switch (index)
    {
        case kVertexArraySubjectIndex:
            switch (message)
            {
                case angle::SubjectMessage::ContentsChanged:
                    mState.setObjectDirty(GL_VERTEX_ARRAY);
                    mStateCache.onVertexArrayBufferContentsChange(this);
                    break;
                case angle::SubjectMessage::SubjectMapped:
                case angle::SubjectMessage::SubjectUnmapped:
                case angle::SubjectMessage::BindingChanged:
                    mStateCache.onVertexArrayBufferStateChange(this);
                    break;
                default:
                    break;
            }
            break;

        case kReadFramebufferSubjectIndex:
            switch (message)
            {
                case angle::SubjectMessage::DirtyBitsFlagged:
                    mState.setReadFramebufferDirty();
                    break;
                case angle::SubjectMessage::SurfaceChanged:
                    mState.setReadFramebufferBindingDirty();
                    break;
                default:
                    UNREACHABLE();
                    break;
            }
            break;

        case kDrawFramebufferSubjectIndex:
            switch (message)
            {
                case angle::SubjectMessage::DirtyBitsFlagged:
                    mState.setDrawFramebufferDirty();
                    mStateCache.onDrawFramebufferChange(this);
                    break;
                case angle::SubjectMessage::SurfaceChanged:
                    mState.setDrawFramebufferBindingDirty();
                    break;
                default:
                    UNREACHABLE();
                    break;
            }
            break;

        case kProgramPipelineSubjectIndex:
            switch (message)
            {
                case angle::SubjectMessage::SubjectChanged:
                    ANGLE_CONTEXT_TRY(mState.onProgramPipelineExecutableChange(this));
                    mStateCache.onProgramExecutableChange(this);
                    break;
                case angle::SubjectMessage::ProgramRelinked:
                    ANGLE_CONTEXT_TRY(mState.mProgramPipeline->link(this));
                    break;
                default:
                    UNREACHABLE();
                    break;
            }
            break;

        default:
            if (index < kTextureMaxSubjectIndex)
            {
                if (message != angle::SubjectMessage::ContentsChanged &&
                    message != angle::SubjectMessage::BindingChanged)
                {
                    mState.onActiveTextureStateChange(this, index);
                    mStateCache.onActiveTextureChange(this);
                }
            }
            else if (index < kImageMaxSubjectIndex)
            {
                mState.onImageStateChange(this, index - kImage0SubjectIndex);
                if (message == angle::SubjectMessage::ContentsChanged)
                {
                    mState.mDirtyBits.set(State::DirtyBitType::DIRTY_BIT_IMAGE_BINDINGS);
                }
            }
            else if (index < kUniformBufferMaxSubjectIndex)
            {
                mState.onUniformBufferStateChange(index - kUniformBuffer0SubjectIndex);
                mStateCache.onUniformBufferStateChange(this);
            }
            else if (index < kAtomicCounterBufferMaxSubjectIndex)
            {
                mState.onAtomicCounterBufferStateChange(index - kAtomicCounterBuffer0SubjectIndex);
                mStateCache.onAtomicCounterBufferStateChange(this);
            }
            else if (index < kShaderStorageBufferMaxSubjectIndex)
            {
                mState.onShaderStorageBufferStateChange(index - kShaderStorageBuffer0SubjectIndex);
                mStateCache.onShaderStorageBufferStateChange(this);
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
            programObject->onStateChange(angle::SubjectMessage::ProgramRelinked);
        }
        mStateCache.onProgramExecutableChange(this);
    }

    return angle::Result::Continue;
}

egl::Error Context::setDefaultFramebuffer(egl::Surface *drawSurface, egl::Surface *readSurface)
{
    ASSERT(mCurrentDrawSurface == nullptr);
    ASSERT(mCurrentReadSurface == nullptr);

    mCurrentDrawSurface = drawSurface;
    mCurrentReadSurface = readSurface;

    if (drawSurface != nullptr)
    {
        ANGLE_TRY(drawSurface->makeCurrent(this));
    }

    ANGLE_TRY(mDefaultFramebuffer->setSurfaces(this, drawSurface, readSurface));

    if (readSurface && (drawSurface != readSurface))
    {
        ANGLE_TRY(readSurface->makeCurrent(this));
    }

    // Update default framebuffer, the binding of the previous default
    // framebuffer (or lack of) will have a nullptr.
    mState.mFramebufferManager->setDefaultFramebuffer(mDefaultFramebuffer.get());
    if (mState.getDrawFramebuffer() == nullptr)
    {
        bindDrawFramebuffer(mDefaultFramebuffer->id());
    }
    if (mState.getReadFramebuffer() == nullptr)
    {
        bindReadFramebuffer(mDefaultFramebuffer->id());
    }

    return egl::NoError();
}

egl::Error Context::unsetDefaultFramebuffer()
{
    Framebuffer *defaultFramebuffer =
        mState.mFramebufferManager->getFramebuffer(Framebuffer::kDefaultDrawFramebufferHandle);

    if (defaultFramebuffer)
    {
        // Remove the default framebuffer
        if (defaultFramebuffer == mState.getReadFramebuffer())
        {
            mState.setReadFramebufferBinding(nullptr);
            mReadFramebufferObserverBinding.bind(nullptr);
        }

        if (defaultFramebuffer == mState.getDrawFramebuffer())
        {
            mState.setDrawFramebufferBinding(nullptr);
            mDrawFramebufferObserverBinding.bind(nullptr);
        }

        ANGLE_TRY(defaultFramebuffer->unsetSurfaces(this));
        mState.mFramebufferManager->setDefaultFramebuffer(nullptr);
    }

    // Always unset the current surface, even if setIsCurrent fails.
    egl::Surface *drawSurface = mCurrentDrawSurface;
    egl::Surface *readSurface = mCurrentReadSurface;
    mCurrentDrawSurface       = nullptr;
    mCurrentReadSurface       = nullptr;
    if (drawSurface)
    {
        ANGLE_TRY(drawSurface->unMakeCurrent(this));
    }
    if (drawSurface != readSurface)
    {
        ANGLE_TRY(readSurface->unMakeCurrent(this));
    }

    return egl::NoError();
}

void Context::onPreSwap()
{
    // Dump frame capture if enabled.
    getShareGroup()->getFrameCaptureShared()->onEndFrame(this);
}

void Context::getTexImage(TextureTarget target,
                          GLint level,
                          GLenum format,
                          GLenum type,
                          void *pixels)
{
    Texture *texture   = getTextureByTarget(target);
    Buffer *packBuffer = mState.getTargetBuffer(BufferBinding::PixelPack);
    ANGLE_CONTEXT_TRY(texture->getTexImage(this, mState.getPackState(), packBuffer, target, level,
                                           format, type, pixels));
}

void Context::getCompressedTexImage(TextureTarget target, GLint level, void *pixels)
{
    Texture *texture   = getTextureByTarget(target);
    Buffer *packBuffer = mState.getTargetBuffer(BufferBinding::PixelPack);
    ANGLE_CONTEXT_TRY(texture->getCompressedTexImage(this, mState.getPackState(), packBuffer,
                                                     target, level, pixels));
}

void Context::getRenderbufferImage(GLenum target, GLenum format, GLenum type, void *pixels)
{
    Renderbuffer *renderbuffer = mState.getCurrentRenderbuffer();
    Buffer *packBuffer         = mState.getTargetBuffer(BufferBinding::PixelPack);
    ANGLE_CONTEXT_TRY(renderbuffer->getRenderbufferImage(this, mState.getPackState(), packBuffer,
                                                         format, type, pixels));
}

void Context::logicOpANGLE(LogicalOperation opcodePacked)
{
    mState.setLogicOp(opcodePacked);
}

egl::Error Context::releaseHighPowerGPU()
{
    return mImplementation->releaseHighPowerGPU(this);
}

egl::Error Context::reacquireHighPowerGPU()
{
    return mImplementation->reacquireHighPowerGPU(this);
}

void Context::onGPUSwitch()
{
    // Re-initialize the renderer string, which just changed, and
    // which must be visible to applications.
    initRendererString();
}

std::mutex &Context::getProgramCacheMutex() const
{
    return mDisplay->getProgramCacheMutex();
}

bool Context::supportsGeometryOrTesselation() const
{
    return mState.getClientVersion() == ES_3_2 || mState.getExtensions().geometryShaderAny() ||
           mState.getExtensions().tessellationShaderEXT;
}

void Context::dirtyAllState()
{
    mState.setAllDirtyBits();
    mState.setAllDirtyObjects();
    mState.gles1().setAllDirty();
}

void Context::finishImmutable() const
{
    ANGLE_CONTEXT_TRY(mImplementation->finish(this));
}

void Context::beginPerfMonitor(GLuint monitor) {}

void Context::deletePerfMonitors(GLsizei n, GLuint *monitors) {}

void Context::endPerfMonitor(GLuint monitor) {}

void Context::genPerfMonitors(GLsizei n, GLuint *monitors)
{
    for (GLsizei monitorIndex = 0; monitorIndex < n; ++monitorIndex)
    {
        monitors[n] = static_cast<GLuint>(monitorIndex);
    }
}

void Context::getPerfMonitorCounterData(GLuint monitor,
                                        GLenum pname,
                                        GLsizei dataSize,
                                        GLuint *data,
                                        GLint *bytesWritten)
{
    using namespace angle;
    const PerfMonitorCounterGroups &perfMonitorGroups = mImplementation->getPerfMonitorCounters();
    GLint byteCount                                   = 0;
    switch (pname)
    {
        case GL_PERFMON_RESULT_AVAILABLE_AMD:
        {
            *data = GL_TRUE;
            byteCount += sizeof(GLuint);
            break;
        }
        case GL_PERFMON_RESULT_SIZE_AMD:
        {
            GLuint resultSize = 0;
            for (const PerfMonitorCounterGroup &group : perfMonitorGroups)
            {
                resultSize += sizeof(PerfMonitorTriplet) * group.counters.size();
            }
            *data = resultSize;
            byteCount += sizeof(GLuint);
            break;
        }
        case GL_PERFMON_RESULT_AMD:
        {
            PerfMonitorTriplet *resultsOut = reinterpret_cast<PerfMonitorTriplet *>(data);
            GLsizei maxResults             = dataSize / (3 * sizeof(GLuint));
            GLsizei resultCount            = 0;
            for (size_t groupIndex = 0;
                 groupIndex < perfMonitorGroups.size() && resultCount < maxResults; ++groupIndex)
            {
                const PerfMonitorCounterGroup &group = perfMonitorGroups[groupIndex];
                for (size_t counterIndex = 0;
                     counterIndex < group.counters.size() && resultCount < maxResults;
                     ++counterIndex)
                {
                    const PerfMonitorCounter &counter = group.counters[counterIndex];
                    PerfMonitorTriplet &triplet       = resultsOut[resultCount++];
                    triplet.counter                   = static_cast<GLuint>(counterIndex);
                    triplet.group                     = static_cast<GLuint>(groupIndex);
                    triplet.value                     = counter.value;
                }
            }
            byteCount += sizeof(PerfMonitorTriplet) * resultCount;
            break;
        }
        default:
            UNREACHABLE();
    }

    if (bytesWritten)
    {
        *bytesWritten = byteCount;
    }
}

void Context::getPerfMonitorCounterInfo(GLuint group, GLuint counter, GLenum pname, void *data)
{
    using namespace angle;
    const PerfMonitorCounterGroups &perfMonitorGroups = mImplementation->getPerfMonitorCounters();
    ASSERT(group < perfMonitorGroups.size());
    const PerfMonitorCounters &counters = perfMonitorGroups[group].counters;
    ASSERT(counter < counters.size());

    switch (pname)
    {
        case GL_COUNTER_TYPE_AMD:
        {
            GLenum *dataOut = reinterpret_cast<GLenum *>(data);
            *dataOut        = GL_UNSIGNED_INT;
            break;
        }
        case GL_COUNTER_RANGE_AMD:
        {
            GLuint *dataOut = reinterpret_cast<GLuint *>(data);
            dataOut[0]      = 0;
            dataOut[1]      = std::numeric_limits<GLuint>::max();
            break;
        }
        default:
            UNREACHABLE();
    }
}

void Context::getPerfMonitorCounterString(GLuint group,
                                          GLuint counter,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLchar *counterString)
{
    using namespace angle;
    const PerfMonitorCounterGroups &perfMonitorGroups = mImplementation->getPerfMonitorCounters();
    ASSERT(group < perfMonitorGroups.size());
    const PerfMonitorCounters &counters = perfMonitorGroups[group].counters;
    ASSERT(counter < counters.size());
    GetPerfMonitorString(counters[counter].name, bufSize, length, counterString);
}

void Context::getPerfMonitorCounters(GLuint group,
                                     GLint *numCounters,
                                     GLint *maxActiveCounters,
                                     GLsizei counterSize,
                                     GLuint *counters)
{
    using namespace angle;
    const PerfMonitorCounterGroups &perfMonitorGroups = mImplementation->getPerfMonitorCounters();
    ASSERT(group < perfMonitorGroups.size());
    const PerfMonitorCounters &groupCounters = perfMonitorGroups[group].counters;

    if (numCounters)
    {
        *numCounters = static_cast<GLint>(groupCounters.size());
    }

    if (maxActiveCounters)
    {
        *maxActiveCounters = static_cast<GLint>(groupCounters.size());
    }

    if (counters)
    {
        GLsizei maxCounterIndex = std::min(counterSize, static_cast<GLsizei>(groupCounters.size()));
        for (GLsizei counterIndex = 0; counterIndex < maxCounterIndex; ++counterIndex)
        {
            counters[counterIndex] = static_cast<GLuint>(counterIndex);
        }
    }
}

void Context::getPerfMonitorGroupString(GLuint group,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLchar *groupString)
{
    using namespace angle;
    const PerfMonitorCounterGroups &perfMonitorGroups = mImplementation->getPerfMonitorCounters();
    ASSERT(group < perfMonitorGroups.size());
    GetPerfMonitorString(perfMonitorGroups[group].name, bufSize, length, groupString);
}

void Context::getPerfMonitorGroups(GLint *numGroups, GLsizei groupsSize, GLuint *groups)
{
    using namespace angle;
    const PerfMonitorCounterGroups &perfMonitorGroups = mImplementation->getPerfMonitorCounters();

    if (numGroups)
    {
        *numGroups = static_cast<GLint>(perfMonitorGroups.size());
    }

    GLuint maxGroupIndex =
        std::min<GLuint>(groupsSize, static_cast<GLuint>(perfMonitorGroups.size()));
    for (GLuint groupIndex = 0; groupIndex < maxGroupIndex; ++groupIndex)
    {
        groups[groupIndex] = groupIndex;
    }
}

void Context::selectPerfMonitorCounters(GLuint monitor,
                                        GLboolean enable,
                                        GLuint group,
                                        GLint numCounters,
                                        GLuint *counterList)
{}

const angle::PerfMonitorCounterGroups &Context::getPerfMonitorCounterGroups() const
{
    return mImplementation->getPerfMonitorCounters();
}

void Context::drawPixelLocalStorageEXTEnable(GLsizei n,
                                             const PixelLocalStoragePlane planes[],
                                             const GLenum loadops[])
{
    ASSERT(mImplementation->getNativePixelLocalStorageType() ==
           ShPixelLocalStorageType::PixelLocalStorageEXT);
    ANGLE_CONTEXT_TRY(syncState(mPixelLocalStorageEXTEnableDisableDirtyBits,
                                mPixelLocalStorageEXTEnableDisableDirtyObjects, Command::Draw));
    ANGLE_CONTEXT_TRY(mImplementation->drawPixelLocalStorageEXTEnable(this, n, planes, loadops));
}

void Context::drawPixelLocalStorageEXTDisable(const PixelLocalStoragePlane planes[],
                                              const GLenum storeops[])
{
    ASSERT(mImplementation->getNativePixelLocalStorageType() ==
           ShPixelLocalStorageType::PixelLocalStorageEXT);
    ANGLE_CONTEXT_TRY(syncState(mPixelLocalStorageEXTEnableDisableDirtyBits,
                                mPixelLocalStorageEXTEnableDisableDirtyObjects, Command::Draw));
    ANGLE_CONTEXT_TRY(mImplementation->drawPixelLocalStorageEXTDisable(this, planes, storeops));
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
    if (errorCode == GL_OUT_OF_MEMORY &&
        mContext->getGraphicsResetStrategy() == GL_LOSE_CONTEXT_ON_RESET_EXT &&
        mContext->getDisplay()->getFrontendFeatures().loseContextOnOutOfMemory.enabled)
    {
        mContext->markContextLost(GraphicsResetStatus::UnknownContextReset);
    }

    std::stringstream errorStream;
    errorStream << "Error: " << gl::FmtHex(errorCode) << ", in " << file << ", " << function << ":"
                << line << ". " << message;

    std::string formattedMessage = errorStream.str();

    // Process the error, but log it with WARN severity so it shows up in logs.
    ASSERT(errorCode != GL_NO_ERROR);
    mErrors.insert(errorCode);

    mContext->getState().getDebug().insertMessage(
        GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, errorCode, GL_DEBUG_SEVERITY_HIGH,
        std::move(formattedMessage), gl::LOG_WARN, angle::EntryPoint::Invalid);
}

void ErrorSet::validationError(angle::EntryPoint entryPoint, GLenum errorCode, const char *message)
{
    ASSERT(errorCode != GL_NO_ERROR);
    mErrors.insert(errorCode);

    mContext->getState().getDebug().insertMessage(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR,
                                                  errorCode, GL_DEBUG_SEVERITY_HIGH, message,
                                                  gl::LOG_INFO, entryPoint);
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
      mCachedProgramPipelineError(kInvalidPointer),
      mCachedTransformFeedbackActiveUnpaused(false),
      mCachedCanDraw(false)
{
    mCachedValidDrawModes.fill(false);
}

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
    updateCanDraw(context);
}

void StateCache::updateActiveAttribsMask(Context *context)
{
    bool isGLES1         = context->isGLES1();
    const State &glState = context->getState();

    if (!isGLES1 && !glState.getProgramExecutable())
    {
        mCachedActiveBufferedAttribsMask = AttributesMask();
        mCachedActiveClientAttribsMask   = AttributesMask();
        mCachedActiveDefaultAttribsMask  = AttributesMask();
        return;
    }

    AttributesMask activeAttribs =
        isGLES1 ? glState.gles1().getActiveAttributesMask()
                : glState.getProgramExecutable()->getActiveAttribLocationsMask();

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
               context->getState().getProgramExecutable()->isAttribLocationActive(attributeIndex));

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

void StateCache::updateProgramPipelineError()
{
    mCachedProgramPipelineError = kInvalidPointer;
}

void StateCache::updateBasicDrawElementsError()
{
    mCachedBasicDrawElementsError = kInvalidPointer;
}

intptr_t StateCache::getBasicDrawStatesErrorImpl(const Context *context) const
{
    ASSERT(mCachedBasicDrawStatesError == kInvalidPointer);
    mCachedBasicDrawStatesError = reinterpret_cast<intptr_t>(ValidateDrawStates(context));
    return mCachedBasicDrawStatesError;
}

intptr_t StateCache::getProgramPipelineErrorImpl(const Context *context) const
{
    ASSERT(mCachedProgramPipelineError == kInvalidPointer);
    mCachedProgramPipelineError = reinterpret_cast<intptr_t>(ValidateProgramPipeline(context));
    return mCachedProgramPipelineError;
}

intptr_t StateCache::getBasicDrawElementsErrorImpl(const Context *context) const
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
    updateBasicDrawElementsError();
}

void StateCache::onProgramExecutableChange(Context *context)
{
    updateActiveAttribsMask(context);
    updateVertexElementLimits(context);
    updateBasicDrawStatesError();
    updateProgramPipelineError();
    updateValidDrawModes(context);
    updateActiveShaderStorageBufferIndices(context);
    updateActiveImageUnitIndices(context);
    updateCanDraw(context);
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
    updateBasicDrawElementsError();
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

void StateCache::onAtomicCounterBufferStateChange(Context *context)
{
    updateBasicDrawStatesError();
}

void StateCache::onShaderStorageBufferStateChange(Context *context)
{
    updateBasicDrawStatesError();
}

void StateCache::onColorMaskChange(Context *context)
{
    updateBasicDrawStatesError();
}

void StateCache::onBlendFuncIndexedChange(Context *context)
{
    updateBasicDrawStatesError();
}

void StateCache::onBlendEquationChange(Context *context)
{
    updateBasicDrawStatesError();
}

void StateCache::setValidDrawModes(bool pointsOK,
                                   bool linesOK,
                                   bool trisOK,
                                   bool lineAdjOK,
                                   bool triAdjOK,
                                   bool patchOK)
{
    mCachedValidDrawModes[PrimitiveMode::Points]                 = pointsOK;
    mCachedValidDrawModes[PrimitiveMode::Lines]                  = linesOK;
    mCachedValidDrawModes[PrimitiveMode::LineLoop]               = linesOK;
    mCachedValidDrawModes[PrimitiveMode::LineStrip]              = linesOK;
    mCachedValidDrawModes[PrimitiveMode::Triangles]              = trisOK;
    mCachedValidDrawModes[PrimitiveMode::TriangleStrip]          = trisOK;
    mCachedValidDrawModes[PrimitiveMode::TriangleFan]            = trisOK;
    mCachedValidDrawModes[PrimitiveMode::LinesAdjacency]         = lineAdjOK;
    mCachedValidDrawModes[PrimitiveMode::LineStripAdjacency]     = lineAdjOK;
    mCachedValidDrawModes[PrimitiveMode::TrianglesAdjacency]     = triAdjOK;
    mCachedValidDrawModes[PrimitiveMode::TriangleStripAdjacency] = triAdjOK;
    mCachedValidDrawModes[PrimitiveMode::Patches]                = patchOK;
}

void StateCache::updateValidDrawModes(Context *context)
{
    const State &state = context->getState();

    const ProgramExecutable *programExecutable = context->getState().getProgramExecutable();

    // If tessellation is active primitive mode must be GL_PATCHES.
    if (programExecutable && programExecutable->hasLinkedTessellationShader())
    {
        setValidDrawModes(false, false, false, false, false, true);
        return;
    }

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
        if (!context->getExtensions().geometryShaderAny() &&
            !context->getExtensions().tessellationShaderEXT && context->getClientVersion() < ES_3_2)
        {
            mCachedValidDrawModes.fill(false);
            mCachedValidDrawModes[curTransformFeedback->getPrimitiveMode()] = true;
            return;
        }
    }

    if (!programExecutable || !programExecutable->hasLinkedShaderStage(ShaderType::Geometry))
    {
        // All draw modes are valid, since drawing without a program does not generate an error and
        // and operations requiring a GS will trigger other validation errors.
        // `patchOK = false` due to checking above already enabling it if a TS is present.
        setValidDrawModes(true, true, true, true, true, false);
        return;
    }

    PrimitiveMode gsMode = programExecutable->getGeometryShaderInputPrimitiveType();
    bool pointsOK        = gsMode == PrimitiveMode::Points;
    bool linesOK         = gsMode == PrimitiveMode::Lines;
    bool trisOK          = gsMode == PrimitiveMode::Triangles;
    bool lineAdjOK       = gsMode == PrimitiveMode::LinesAdjacency;
    bool triAdjOK        = gsMode == PrimitiveMode::TrianglesAdjacency;

    setValidDrawModes(pointsOK, linesOK, trisOK, lineAdjOK, triAdjOK, false);
}

void StateCache::updateValidBindTextureTypes(Context *context)
{
    const Extensions &exts = context->getExtensions();
    bool isGLES3           = context->getClientMajorVersion() >= 3;
    bool isGLES31          = context->getClientVersion() >= Version(3, 1);

    mCachedValidBindTextureTypes = {{
        {TextureType::_2D, true},
        {TextureType::_2DArray, isGLES3},
        {TextureType::_2DMultisample, isGLES31 || exts.textureMultisampleANGLE},
        {TextureType::_2DMultisampleArray, exts.textureStorageMultisample2dArrayOES},
        {TextureType::_3D, isGLES3 || exts.texture3DOES},
        {TextureType::External, exts.EGLImageExternalOES || exts.EGLStreamConsumerExternalNV},
        {TextureType::Rectangle, exts.textureRectangleANGLE},
        {TextureType::CubeMap, true},
        {TextureType::CubeMapArray, exts.textureCubeMapArrayAny()},
        {TextureType::VideoImage, exts.videoTextureWEBGL},
        {TextureType::Buffer, exts.textureBufferAny()},
    }};
}

void StateCache::updateValidDrawElementsTypes(Context *context)
{
    bool supportsUint =
        (context->getClientMajorVersion() >= 3 || context->getExtensions().elementIndexUintOES);

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
    VertexAttribTypeCase halfFloatValidity = (context->getExtensions().vertexHalfFloatOES)
                                                 ? VertexAttribTypeCase::Valid
                                                 : VertexAttribTypeCase::Invalid;

    VertexAttribTypeCase vertexType1010102Validity = (context->getExtensions().vertexType1010102OES)
                                                         ? VertexAttribTypeCase::ValidSize3or4
                                                         : VertexAttribTypeCase::Invalid;

    if (context->getClientMajorVersion() <= 2)
    {
        mCachedVertexAttribTypesValidation = {{
            {VertexAttribType::Byte, VertexAttribTypeCase::Valid},
            {VertexAttribType::Short, VertexAttribTypeCase::Valid},
            {VertexAttribType::UnsignedByte, VertexAttribTypeCase::Valid},
            {VertexAttribType::UnsignedShort, VertexAttribTypeCase::Valid},
            {VertexAttribType::Float, VertexAttribTypeCase::Valid},
            {VertexAttribType::Fixed, VertexAttribTypeCase::Valid},
            {VertexAttribType::HalfFloatOES, halfFloatValidity},
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
            {VertexAttribType::HalfFloatOES, halfFloatValidity},
            {VertexAttribType::UnsignedInt2101010, VertexAttribTypeCase::ValidSize4Only},
            {VertexAttribType::Int1010102, vertexType1010102Validity},
            {VertexAttribType::UnsignedInt1010102, vertexType1010102Validity},
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

void StateCache::updateActiveShaderStorageBufferIndices(Context *context)
{
    mCachedActiveShaderStorageBufferIndices.reset();
    const ProgramExecutable *executable = context->getState().getProgramExecutable();
    if (executable)
    {
        for (const InterfaceBlock &block : executable->getShaderStorageBlocks())
        {
            mCachedActiveShaderStorageBufferIndices.set(block.binding);
        }
    }
}

void StateCache::updateActiveImageUnitIndices(Context *context)
{
    mCachedActiveImageUnitIndices.reset();
    const ProgramExecutable *executable = context->getState().getProgramExecutable();
    if (executable)
    {
        for (const ImageBinding &imageBinding : executable->getImageBindings())
        {
            for (GLuint binding : imageBinding.boundImageUnits)
            {
                mCachedActiveImageUnitIndices.set(binding);
            }
        }
    }
}

void StateCache::updateCanDraw(Context *context)
{
    mCachedCanDraw =
        (context->isGLES1() || (context->getState().getProgramExecutable() &&
                                context->getState().getProgramExecutable()->hasVertexShader()));
}
}  // namespace gl
