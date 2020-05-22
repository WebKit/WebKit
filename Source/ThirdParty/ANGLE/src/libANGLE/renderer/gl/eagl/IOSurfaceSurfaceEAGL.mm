//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// IOSurfaceSurfaceEAGL.mm: an implementation of PBuffers created from IOSurfaces using
//                          EGL_ANGLE_iosurface_client_buffer

#import "common/platform.h"

#if defined(ANGLE_PLATFORM_IOS) && !defined(ANGLE_PLATFORM_MACCATALYST)

#    import "libANGLE/renderer/gl/eagl/IOSurfaceSurfaceEAGL.h"

#    import "common/debug.h"
#    import "libANGLE/AttributeMap.h"
#    import "libANGLE/renderer/gl/BlitGL.h"
#    import "libANGLE/renderer/gl/FramebufferGL.h"
#    import "libANGLE/renderer/gl/FunctionsGL.h"
#    import "libANGLE/renderer/gl/RendererGL.h"
#    import "libANGLE/renderer/gl/StateManagerGL.h"
#    import "libANGLE/renderer/gl/TextureGL.h"
#    import "libANGLE/renderer/gl/eagl/DisplayEAGL.h"

#    import <OpenGLES/EAGL.h>
#    import <OpenGLES/EAGLDrawable.h>
#    import <OpenGLES/EAGLIOSurface.h>

namespace rx
{

namespace
{

struct IOSurfaceFormatInfo
{
    GLenum internalFormat;
    GLenum type;

    size_t componentBytes;

    GLenum nativeInternalFormat;
    GLenum nativeFormat;
    GLenum nativeType;
};

// clang-format off

static const IOSurfaceFormatInfo kIOSurfaceFormats[] = {
    {GL_RED,      GL_UNSIGNED_BYTE,  1, GL_RED,  GL_RED,  GL_UNSIGNED_BYTE },
    {GL_R16UI,    GL_UNSIGNED_SHORT, 2, GL_RED,  GL_RED,  GL_UNSIGNED_SHORT},
    {GL_RG,       GL_UNSIGNED_BYTE,  2, GL_RG,   GL_RG,   GL_UNSIGNED_BYTE },
    {GL_RGB,      GL_UNSIGNED_BYTE,  4, GL_RGBA, GL_BGRA, GL_UNSIGNED_BYTE },
    {GL_BGRA_EXT, GL_UNSIGNED_BYTE,  4, GL_RGBA, GL_BGRA, GL_UNSIGNED_BYTE },
    {GL_RGBA,     GL_HALF_FLOAT,     8, GL_RGBA, GL_RGBA, GL_HALF_FLOAT    },
};

// clang-format on

int FindIOSurfaceFormatIndex(GLenum internalFormat, GLenum type)
{
    for (int i = 0; i < static_cast<int>(ArraySize(kIOSurfaceFormats)); ++i)
    {
        const auto &formatInfo = kIOSurfaceFormats[i];
        if (formatInfo.internalFormat == internalFormat && formatInfo.type == type)
        {
            return i;
        }
    }
    return -1;
}

}  // anonymous namespace

IOSurfaceSurfaceEAGL::IOSurfaceSurfaceEAGL(const egl::SurfaceState &state,
                                           EAGLContextObj cglContext,
                                           EGLClientBuffer buffer,
                                           const egl::AttributeMap &attribs)
    : SurfaceGL(state),
      mEAGLContext(cglContext),
      mIOSurface(nullptr),
      mWidth(0),
      mHeight(0),
      mPlane(0),
      mRowStrideInPixels(0),
      mFormatIndex(-1),
      mAlphaInitialized(false)
{
    // Keep reference to the IOSurface so it doesn't get deleted while the pbuffer exists.
    mIOSurface = reinterpret_cast<IOSurfaceRef>(buffer);
    CFRetain(mIOSurface);

    // Extract attribs useful for the call to EAGLTexImageIOSurface2D
    mWidth  = static_cast<int>(attribs.get(EGL_WIDTH));
    mHeight = static_cast<int>(attribs.get(EGL_HEIGHT));
    mPlane  = static_cast<int>(attribs.get(EGL_IOSURFACE_PLANE_ANGLE));
    // Hopefully the number of bytes per row is always an integer number of pixels. We use
    // glReadPixels to fill the IOSurface in the simulator and it can only support strides that are
    // an integer number of pixels.
    ASSERT(IOSurfaceGetBytesPerRowOfPlane(mIOSurface, mPlane) %
               IOSurfaceGetBytesPerElementOfPlane(mIOSurface, mPlane) ==
           0);
    mRowStrideInPixels = static_cast<int>(IOSurfaceGetBytesPerRowOfPlane(mIOSurface, mPlane) /
                                          IOSurfaceGetBytesPerElementOfPlane(mIOSurface, mPlane));

    EGLAttrib internalFormat = attribs.get(EGL_TEXTURE_INTERNAL_FORMAT_ANGLE);
    EGLAttrib type           = attribs.get(EGL_TEXTURE_TYPE_ANGLE);
    mFormatIndex =
        FindIOSurfaceFormatIndex(static_cast<GLenum>(internalFormat), static_cast<GLenum>(type));
    ASSERT(mFormatIndex >= 0);

    mAlphaInitialized = !hasEmulatedAlphaChannel();

#    if defined(ANGLE_PLATFORM_IOS_SIMULATOR)
    mBoundTextureID = 0;
    EGLAttrib usageHint =
        attribs.get(EGL_IOSURFACE_USAGE_HINT_ANGLE,
                    EGL_IOSURFACE_READ_HINT_ANGLE | EGL_IOSURFACE_WRITE_HINT_ANGLE);
    mUploadFromIOSurface = ((usageHint & EGL_IOSURFACE_READ_HINT_ANGLE) != 0);
    mReadbackToIOSurface = ((usageHint & EGL_IOSURFACE_WRITE_HINT_ANGLE) != 0);
#    endif
}

IOSurfaceSurfaceEAGL::~IOSurfaceSurfaceEAGL()
{
    if (mIOSurface != nullptr)
    {
        CFRelease(mIOSurface);
        mIOSurface = nullptr;
    }
}

egl::Error IOSurfaceSurfaceEAGL::initialize(const egl::Display *display)
{
    return egl::NoError();
}

egl::Error IOSurfaceSurfaceEAGL::makeCurrent(const gl::Context *context)
{
    return egl::NoError();
}

egl::Error IOSurfaceSurfaceEAGL::unMakeCurrent(const gl::Context *context)
{
    GetFunctionsGL(context)->flush();
    return egl::NoError();
}

egl::Error IOSurfaceSurfaceEAGL::swap(const gl::Context *context)
{
    return egl::NoError();
}

egl::Error IOSurfaceSurfaceEAGL::postSubBuffer(const gl::Context *context,
                                               EGLint x,
                                               EGLint y,
                                               EGLint width,
                                               EGLint height)
{
    UNREACHABLE();
    return egl::NoError();
}

egl::Error IOSurfaceSurfaceEAGL::querySurfacePointerANGLE(EGLint attribute, void **value)
{
    UNREACHABLE();
    return egl::NoError();
}

egl::Error IOSurfaceSurfaceEAGL::bindTexImage(const gl::Context *context,
                                              gl::Texture *texture,
                                              EGLint)
{
    StateManagerGL *stateManager = GetStateManagerGL(context);

    const TextureGL *textureGL = GetImplAs<TextureGL>(texture);
    GLuint textureID           = textureGL->getTextureID();
    stateManager->bindTexture(gl::TextureType::_2D, textureID);
    const auto &format = kIOSurfaceFormats[mFormatIndex];

#    if !defined(ANGLE_PLATFORM_IOS_SIMULATOR)
    if (![mEAGLContext texImageIOSurface:mIOSurface
                                  target:GL_TEXTURE_2D
                          internalFormat:format.nativeInternalFormat
                                   width:mWidth
                                  height:mHeight
                                  format:format.nativeFormat
                                    type:format.nativeType
                                   plane:mPlane])
    {
        return egl::EglContextLost() << "EAGLContext texImageIOSurface2D failed.";
    }

    if (IsError(initializeAlphaChannel(context, textureID)))
    {
        return egl::EglContextLost() << "Failed to initialize IOSurface alpha channel.";
    }
#    else   // !defined(ANGLE_PLATFORM_IOS_SIMULATOR)
    const FunctionsGL *functions = GetFunctionsGL(context);

    IOSurfaceLock(mIOSurface, getIOSurfaceLockOptions(), nullptr);
    void *textureData = nullptr;
    if (mUploadFromIOSurface)
    {
        // TODO(kbr): possibly more state to be set here, including setting any
        // pixel unpack buffer to 0 when using ES 3.0 contexts.
        gl::PixelUnpackState defaultUnpackState;
        stateManager->setPixelUnpackState(defaultUnpackState);
        textureData = IOSurfaceGetBaseAddress(mIOSurface);
    }

    // TODO(kbr): consider trying to optimize away texture reallocations by
    // keeping track of which textures have already been allocated.
    functions->texImage2D(GL_TEXTURE_2D, 0, format.nativeInternalFormat, mWidth, mHeight, 0,
                          format.nativeFormat, format.nativeType, textureData);

    mBoundTextureID = textureID;
#    endif  // !defined(ANGLE_PLATFORM_IOS_SIMULATOR)

    return egl::NoError();
}

egl::Error IOSurfaceSurfaceEAGL::releaseTexImage(const gl::Context *context, EGLint buffer)
{
    const FunctionsGL *functions = GetFunctionsGL(context);
#    if !defined(ANGLE_PLATFORM_IOS_SIMULATOR)
    functions->flush();
#    else   // !defined(ANGLE_PLATFORM_IOS_SIMULATOR)
    if (mReadbackToIOSurface)
    {
        StateManagerGL *stateManager = GetStateManagerGL(context);
        GLuint tempFBO               = 0;
        functions->genFramebuffers(1, &tempFBO);
        stateManager->bindFramebuffer(GL_FRAMEBUFFER, tempFBO);
        functions->framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                        mBoundTextureID, 0);
        gl::PixelPackState state;
        state.rowLength = mRowStrideInPixels;
        state.alignment = 1;
        stateManager->setPixelPackState(state);
        // TODO(kbr): possibly more state to be set here, including setting any
        // pixel pack buffer to 0 when using ES 3.0 contexts.
        const auto &format = kIOSurfaceFormats[mFormatIndex];
        functions->readPixels(0, 0, mWidth, mHeight, format.nativeFormat, format.nativeType,
                              IOSurfaceGetBaseAddress(mIOSurface));
    }

    IOSurfaceUnlock(mIOSurface, getIOSurfaceLockOptions(), nullptr);
#    endif  // !defined(ANGLE_PLATFORM_IOS_SIMULATOR)

    return egl::NoError();
}

void IOSurfaceSurfaceEAGL::setSwapInterval(EGLint interval)
{
    UNREACHABLE();
}

EGLint IOSurfaceSurfaceEAGL::getWidth() const
{
    return mWidth;
}

EGLint IOSurfaceSurfaceEAGL::getHeight() const
{
    return mHeight;
}

EGLint IOSurfaceSurfaceEAGL::isPostSubBufferSupported() const
{
    UNREACHABLE();
    return EGL_FALSE;
}

EGLint IOSurfaceSurfaceEAGL::getSwapBehavior() const
{
    // N/A because you can't MakeCurrent an IOSurface, return any valid value.
    return EGL_BUFFER_PRESERVED;
}

// static
bool IOSurfaceSurfaceEAGL::validateAttributes(EGLClientBuffer buffer,
                                              const egl::AttributeMap &attribs)
{
    IOSurfaceRef ioSurface = reinterpret_cast<IOSurfaceRef>(buffer);

    // The plane must exist for this IOSurface. IOSurfaceGetPlaneCount can return 0 for non-planar
    // ioSurfaces but we will treat non-planar like it is a single plane.
    size_t surfacePlaneCount = std::max(size_t(1), IOSurfaceGetPlaneCount(ioSurface));
    EGLAttrib plane          = attribs.get(EGL_IOSURFACE_PLANE_ANGLE);
    if (plane < 0 || static_cast<size_t>(plane) >= surfacePlaneCount)
    {
        return false;
    }

    // The width height specified must be at least (1, 1) and at most the plane size
    EGLAttrib width  = attribs.get(EGL_WIDTH);
    EGLAttrib height = attribs.get(EGL_HEIGHT);
    if (width <= 0 || static_cast<size_t>(width) > IOSurfaceGetWidthOfPlane(ioSurface, plane) ||
        height <= 0 || static_cast<size_t>(height) > IOSurfaceGetHeightOfPlane(ioSurface, plane))
    {
        return false;
    }

    // Find this IOSurface format
    EGLAttrib internalFormat = attribs.get(EGL_TEXTURE_INTERNAL_FORMAT_ANGLE);
    EGLAttrib type           = attribs.get(EGL_TEXTURE_TYPE_ANGLE);

    int formatIndex =
        FindIOSurfaceFormatIndex(static_cast<GLenum>(internalFormat), static_cast<GLenum>(type));

    if (formatIndex < 0)
    {
        return false;
    }

    // Check that the format matches this IOSurface plane
    if (IOSurfaceGetBytesPerElementOfPlane(ioSurface, plane) !=
        kIOSurfaceFormats[formatIndex].componentBytes)
    {
        return false;
    }

    return true;
}

// Wraps a FramebufferGL to hook the destroy function to delete the texture associated with the
// framebuffer.
class IOSurfaceFramebuffer : public FramebufferGL
{
  public:
    IOSurfaceFramebuffer(const gl::FramebufferState &data,
                         GLuint id,
                         GLuint textureId,
                         bool isDefault,
                         bool emulatedAlpha)
        : FramebufferGL(data, id, isDefault, emulatedAlpha), mTextureId(textureId)
    {}
    void destroy(const gl::Context *context) override
    {
        GetFunctionsGL(context)->deleteTextures(1, &mTextureId);
        FramebufferGL::destroy(context);
    }

  private:
    GLuint mTextureId;
};

FramebufferImpl *IOSurfaceSurfaceEAGL::createDefaultFramebuffer(const gl::Context *context,
                                                                const gl::FramebufferState &state)
{
    const FunctionsGL *functions = GetFunctionsGL(context);
    StateManagerGL *stateManager = GetStateManagerGL(context);

    GLuint texture = 0;
    functions->genTextures(1, &texture);
    stateManager->bindTexture(gl::TextureType::_2D, texture);

#    if !defined(ANGLE_PLATFORM_IOS_SIMULATOR)
    const auto &format = kIOSurfaceFormats[mFormatIndex];

    if (![mEAGLContext texImageIOSurface:mIOSurface
                                  target:GL_TEXTURE_2D
                          internalFormat:format.nativeInternalFormat
                                   width:mWidth
                                  height:mHeight
                                  format:format.nativeFormat
                                    type:format.nativeType
                                   plane:mPlane])
    {
        ERR() << "[EAGLContext texImageIOSurface] failed";
        return nullptr;
    }
#    else   // !defined(ANGLE_PLATFORM_IOS_SIMULATOR)
    ERR() << "IOSurfaces with OpenGL ES not supported on iOS Simulator";
#    endif  // !defined(ANGLE_PLATFORM_IOS_SIMULATOR)

    if (IsError(initializeAlphaChannel(context, texture)))
    {
        ERR() << "Failed to initialize IOSurface alpha channel.";
        return nullptr;
    }

    GLuint framebuffer = 0;
    functions->genFramebuffers(1, &framebuffer);
    stateManager->bindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    stateManager->bindTexture(gl::TextureType::_2D, texture);
    functions->framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture,
                                    0);

    return new IOSurfaceFramebuffer(state, framebuffer, texture, true, hasEmulatedAlphaChannel());
}

angle::Result IOSurfaceSurfaceEAGL::initializeAlphaChannel(const gl::Context *context,
                                                           GLuint texture)
{
    if (mAlphaInitialized)
    {
        return angle::Result::Continue;
    }

    BlitGL *blitter = GetBlitGL(context);
    ANGLE_TRY(blitter->clearRenderableTextureAlphaToOne(context, texture,
                                                        gl::TextureTarget::Rectangle, 0));
    mAlphaInitialized = true;
    return angle::Result::Continue;
}

bool IOSurfaceSurfaceEAGL::hasEmulatedAlphaChannel() const
{
    const auto &format = kIOSurfaceFormats[mFormatIndex];
    return format.internalFormat == GL_RGB;
}

#    if defined(ANGLE_PLATFORM_IOS_SIMULATOR)
IOSurfaceLockOptions IOSurfaceSurfaceEAGL::getIOSurfaceLockOptions() const
{
    IOSurfaceLockOptions options = 0;
    if (!mReadbackToIOSurface)
    {
        options |= kIOSurfaceLockReadOnly;
    }
    return options;
}
#    endif  // defined(ANGLE_PLATFORM_IOS_SIMULATOR)

}  // namespace rx

#endif  // defined(ANGLE_PLATFORM_IOS)
