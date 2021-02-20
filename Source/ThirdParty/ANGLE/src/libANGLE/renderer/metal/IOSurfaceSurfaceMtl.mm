//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// IOSurfaceSurfaceMtl.mm:
//    Implements the class methods for IOSurfaceSurfaceMtl.
//

#include "libANGLE/renderer/metal/IOSurfaceSurfaceMtl.h"

#include <TargetConditionals.h>

#include "libANGLE/Display.h"
#include "libANGLE/Surface.h"
#include "libANGLE/renderer/metal/ContextMtl.h"
#include "libANGLE/renderer/metal/DisplayMtl.h"
#include "libANGLE/renderer/metal/FrameBufferMtl.h"
#include "libANGLE/renderer/metal/mtl_format_utils.h"

// Compiler can turn on programmatical frame capture in release build by defining
// ANGLE_METAL_FRAME_CAPTURE flag.
#if defined(NDEBUG) && !defined(ANGLE_METAL_FRAME_CAPTURE)
#    define ANGLE_METAL_FRAME_CAPTURE_ENABLED 0
#else
#    define ANGLE_METAL_FRAME_CAPTURE_ENABLED ANGLE_WITH_MODERN_METAL_API
#endif
namespace rx
{

namespace
{

struct IOSurfaceFormatInfo
{
    GLenum internalFormat;
    GLenum type;
    angle::FormatID pixelFormat;
    angle::FormatID internalPixelFormat;
};

static const IOSurfaceFormatInfo kIOSurfaceFormats[] = {
    {GL_RED, GL_UNSIGNED_BYTE, angle::FormatID::R8_UNORM, angle::FormatID::R8_UNORM},
    {GL_R16UI, GL_UNSIGNED_SHORT, angle::FormatID::R16G16_UINT, angle::FormatID::R16G16_UINT},
    {GL_RG, GL_UNSIGNED_BYTE, angle::FormatID::R8G8_UNORM, angle::FormatID::R8G8_UNORM},
    {GL_RGB, GL_UNSIGNED_BYTE, angle::FormatID::R8G8B8A8_UNORM, angle::FormatID::B8G8R8A8_UNORM},
    {GL_BGRA_EXT, GL_UNSIGNED_BYTE, angle::FormatID::B8G8R8A8_UNORM,
     angle::FormatID::B8G8R8A8_UNORM},
    {GL_RGB10_A2, GL_UNSIGNED_INT_2_10_10_10_REV, angle::FormatID::R10G10B10A2_UNORM,
     angle::FormatID::R10G10B10A2_UNORM},
    {GL_RGBA, GL_HALF_FLOAT, angle::FormatID::R16G16B16A16_FLOAT,
     angle::FormatID::R16G16B16A16_FLOAT},
};

int FindIOSurfaceFormatIndex(GLenum internalFormat, GLenum type)
{
    for (size_t i = 0; i < static_cast<size_t>(ArraySize(kIOSurfaceFormats)); ++i)
    {
        const auto &formatInfo = kIOSurfaceFormats[i];
        if (formatInfo.internalFormat == internalFormat && formatInfo.type == type)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// TODO(jcunningham) : refactor this and surfacemtl to reduce copy+pasted code

ANGLE_MTL_UNUSED
bool IsFrameCaptureEnabled()
{
#if !ANGLE_METAL_FRAME_CAPTURE_ENABLED
    return false;
#else
    // We only support frame capture programmatically if the ANGLE_METAL_FRAME_CAPTURE
    // environment flag is set. Otherwise, it will slow down the rendering. This allows user to
    // finely control whether they want to capture the frame for particular application or not.
    auto var                  = std::getenv("ANGLE_METAL_FRAME_CAPTURE");
    static const bool enabled = var ? (strcmp(var, "1") == 0) : false;

    return enabled;
#endif
}

ANGLE_MTL_UNUSED
std::string GetMetalCaptureFile()
{
#if !ANGLE_METAL_FRAME_CAPTURE_ENABLED
    return "";
#else
    auto var                   = std::getenv("ANGLE_METAL_FRAME_CAPTURE_FILE");
    const std::string filePath = var ? var : "";

    return filePath;
#endif
}

ANGLE_MTL_UNUSED
size_t MaxAllowedFrameCapture()
{
#if !ANGLE_METAL_FRAME_CAPTURE_ENABLED
    return 0;
#else
    auto var                      = std::getenv("ANGLE_METAL_FRAME_CAPTURE_MAX");
    static const size_t maxFrames = var ? std::atoi(var) : 100;

    return maxFrames;
#endif
}

ANGLE_MTL_UNUSED
size_t MinAllowedFrameCapture()
{
#if !ANGLE_METAL_FRAME_CAPTURE_ENABLED
    return 0;
#else
    auto var                     = std::getenv("ANGLE_METAL_FRAME_CAPTURE_MIN");
    static const size_t minFrame = var ? std::atoi(var) : 0;

    return minFrame;
#endif
}

ANGLE_MTL_UNUSED
bool FrameCaptureDeviceScope()
{
#if !ANGLE_METAL_FRAME_CAPTURE_ENABLED
    return false;
#else
    auto var                      = std::getenv("ANGLE_METAL_FRAME_CAPTURE_SCOPE");
    static const bool scopeDevice = var ? (strcmp(var, "device") == 0) : false;

    return scopeDevice;
#endif
}

ANGLE_MTL_UNUSED
std::atomic<size_t> gFrameCaptured(0);

ANGLE_MTL_UNUSED
void StartFrameCapture(id<MTLDevice> metalDevice, id<MTLCommandQueue> metalCmdQueue)
{
#if ANGLE_METAL_FRAME_CAPTURE_ENABLED
    if (!IsFrameCaptureEnabled())
    {
        return;
    }

    if (gFrameCaptured >= MaxAllowedFrameCapture())
    {
        return;
    }

    MTLCaptureManager *captureManager = [MTLCaptureManager sharedCaptureManager];
    if (captureManager.isCapturing)
    {
        return;
    }

    gFrameCaptured++;

    if (gFrameCaptured < MinAllowedFrameCapture())
    {
        return;
    }

#    ifdef __MAC_10_15
    if (ANGLE_APPLE_AVAILABLE_XCI(10.15, 13.0, 13))
    {
        MTLCaptureDescriptor *captureDescriptor = [[MTLCaptureDescriptor alloc] init];
        captureDescriptor.captureObject         = metalDevice;
        const std::string filePath              = GetMetalCaptureFile();
        if (filePath != "")
        {
            const std::string numberedPath =
                filePath + std::to_string(gFrameCaptured - 1) + ".gputrace";
            captureDescriptor.destination = MTLCaptureDestinationGPUTraceDocument;
            captureDescriptor.outputURL =
                [NSURL fileURLWithPath:[NSString stringWithUTF8String:numberedPath.c_str()]
                           isDirectory:false];
        }
        else
        {
            // This will pause execution only if application is being debugged inside Xcode
            captureDescriptor.destination = MTLCaptureDestinationDeveloperTools;
        }

        NSError *error;
        if (![captureManager startCaptureWithDescriptor:captureDescriptor error:&error])
        {
            NSLog(@"Failed to start capture, error %@", error);
        }
    }
    else
#    endif  // __MAC_10_15
        if (ANGLE_APPLE_AVAILABLE_XCI(10.15, 13.0, 13))
    {
        MTLCaptureDescriptor *captureDescriptor = [[MTLCaptureDescriptor alloc] init];
        captureDescriptor.captureObject         = metalDevice;

        NSError *error;
        if (![captureManager startCaptureWithDescriptor:captureDescriptor error:&error])
        {
            NSLog(@"Failed to start capture, error %@", error);
        }
    }
#endif  // ANGLE_METAL_FRAME_CAPTURE_ENABLED
}

void StartFrameCapture(ContextMtl *context)
{
    StartFrameCapture(context->getMetalDevice(), context->cmdQueue().get());
}

void StopFrameCapture()
{
#if ANGLE_METAL_FRAME_CAPTURE_ENABLED
    if (!IsFrameCaptureEnabled())
    {
        return;
    }
    MTLCaptureManager *captureManager = [MTLCaptureManager sharedCaptureManager];
    if (captureManager.isCapturing)
    {
        [captureManager stopCapture];
    }
#endif
}

}  // anonymous namespace

IOSurfaceSurfaceMtl::IOSurfaceSurfaceMtl(DisplayMtl *display,
                                         const egl::SurfaceState &state,
                                         EGLClientBuffer buffer,
                                         const egl::AttributeMap &attribs)
    : SurfaceMtlProtocol(state)
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
    mFormat         = display->getPixelFormat(kIOSurfaceFormats[mFormatIndex].pixelFormat);
    mInternalFormat = display->getPixelFormat(kIOSurfaceFormats[mFormatIndex].internalPixelFormat);
    mGLInternalFormat = kIOSurfaceFormats[mFormatIndex].internalFormat;
}

IOSurfaceSurfaceMtl::~IOSurfaceSurfaceMtl()
{
    StopFrameCapture();
    if (mIOSurface != nullptr)
    {
        CFRelease(mIOSurface);
        mIOSurface = nullptr;
    }
}

void IOSurfaceSurfaceMtl::destroy(const egl::Display *display) {}

egl::Error IOSurfaceSurfaceMtl::initialize(const egl::Display *display)
{
    DisplayMtl *displayMtl    = mtl::GetImpl(display);
    id<MTLDevice> metalDevice = displayMtl->getMetalDevice();

    StartFrameCapture(metalDevice, displayMtl->cmdQueue().get());
    return egl::NoError();
}

FramebufferImpl *IOSurfaceSurfaceMtl::createDefaultFramebuffer(const gl::Context *context,
                                                               const gl::FramebufferState &state)
{
    auto fbo = new FramebufferMtl(state, /* flipY */ true, /* backbuffer */ this);
    return fbo;
}

egl::Error IOSurfaceSurfaceMtl::makeCurrent(const gl::Context *context)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);
    StartFrameCapture(contextMtl);
    return egl::NoError();
}

egl::Error IOSurfaceSurfaceMtl::unMakeCurrent(const gl::Context *context)
{
    StopFrameCapture();
    return egl::NoError();
}

egl::Error IOSurfaceSurfaceMtl::swap(const gl::Context *context)
{
    StopFrameCapture();
    ContextMtl *contextMtl = mtl::GetImpl(context);
    StartFrameCapture(contextMtl);
    return egl::NoError();
}

egl::Error IOSurfaceSurfaceMtl::postSubBuffer(const gl::Context *context,
                                              EGLint x,
                                              EGLint y,
                                              EGLint width,
                                              EGLint height)
{
    UNIMPLEMENTED();
    return egl::EglBadAccess();
}

egl::Error IOSurfaceSurfaceMtl::querySurfacePointerANGLE(EGLint attribute, void **value)
{
    UNIMPLEMENTED();
    return egl::EglBadAccess();
}

egl::Error IOSurfaceSurfaceMtl::bindTexImage(const gl::Context *context,
                                             gl::Texture *texture,
                                             EGLint buffer)
{

    angle::Result res = createBackingTexture(context);
    if (res == angle::Result::Continue)
        return egl::NoError();
    else
        return egl::EglBadAccess();
}

egl::Error IOSurfaceSurfaceMtl::releaseTexImage(const gl::Context *context, EGLint buffer)
{
    // This will need implementation
    ContextMtl *contextMtl = mtl::GetImpl(context);
    if (contextMtl->flush(context) == angle::Result::Continue)
    {
        mIOSurfaceTextureCreated = false;
        return egl::NoError();
    }
    return egl::EglBadAccess();
}

egl::Error IOSurfaceSurfaceMtl::getSyncValues(EGLuint64KHR *ust,
                                              EGLuint64KHR *msc,
                                              EGLuint64KHR *sbc)
{
    UNIMPLEMENTED();
    return egl::EglBadAccess();
}

egl::Error IOSurfaceSurfaceMtl::getMscRate(EGLint *numerator, EGLint *denominator)
{
    UNIMPLEMENTED();
    return egl::EglBadAccess();
}

void IOSurfaceSurfaceMtl::setSwapInterval(EGLint interval)
{
    UNIMPLEMENTED();
}

void IOSurfaceSurfaceMtl::setFixedWidth(EGLint width)
{
    UNIMPLEMENTED();
}

void IOSurfaceSurfaceMtl::setFixedHeight(EGLint height)
{
    UNIMPLEMENTED();
}

// width and height can change with client window resizing
EGLint IOSurfaceSurfaceMtl::getWidth() const
{
    return static_cast<EGLint>(mWidth);
}

EGLint IOSurfaceSurfaceMtl::getHeight() const
{
    return static_cast<EGLint>(mHeight);
}

EGLint IOSurfaceSurfaceMtl::isPostSubBufferSupported() const
{
    return EGL_FALSE;
}

EGLint IOSurfaceSurfaceMtl::getSwapBehavior() const
{
    return EGL_BUFFER_DESTROYED;
}

angle::Result IOSurfaceSurfaceMtl::getAttachmentRenderTarget(
    const gl::Context *context,
    GLenum binding,
    const gl::ImageIndex &imageIndex,
    GLsizei samples,
    FramebufferAttachmentRenderTarget **rtOut)
{
    ANGLE_TRY(ensureTextureCreated(context));
    *rtOut = &mRenderTarget;
    return angle::Result::Continue;
}

angle::Result IOSurfaceSurfaceMtl::ensureCurrentDrawableObtained(const gl::Context *context, bool *newDrawableOut)
{
    return angle::Result::Continue;
}
angle::Result IOSurfaceSurfaceMtl::ensureCurrentDrawableObtained(const gl::Context *context)
{
    return angle::Result::Continue;
}

angle::Result IOSurfaceSurfaceMtl::createBackingTexture(const gl::Context *context)
{
    mtl::TextureRef mTemporarySurfaceRef;
    ANGLE_TRY(createTexture(context, mFormat, mWidth, mHeight, false, &mTemporarySurfaceRef));
    // Create texture view around the base format of the texture.
    mIOSurfaceTexture =
        mTemporarySurfaceRef->createViewWithDifferentFormat(MTLPixelFormatBGRA8Unorm);
    mRenderTarget.set(mIOSurfaceTexture, mtl::kZeroNativeMipLevel, 0, mInternalFormat);

    if (mGLInternalFormat == GL_RGB)
    {
        // Disable subsequent rendering to alpha channel.
        // TODO: Investigate if this allows the higher level alpha masks can
        // be disabled once this backend is live.
        mIOSurfaceTexture->setColorWritableMask(MTLColorWriteMaskAll & (~MTLColorWriteMaskAlpha));
    }

    mIOSurfaceTextureCreated = true;
    return angle::Result::Continue;
}

angle::Result IOSurfaceSurfaceMtl::ensureTextureCreated(const gl::Context *context)
{
    if (!mIOSurfaceTextureCreated)
    {
        return createBackingTexture(context);
    }
    return angle::Result::Continue;
}

mtl::TextureRef IOSurfaceSurfaceMtl::getTexture(const gl::Context *context)
{
    if (ensureCurrentDrawableObtained(context) == angle::Result::Continue)
    {
        return mIOSurfaceTexture;
    }
    return nullptr;
}

angle::Result IOSurfaceSurfaceMtl::createTexture(const gl::Context *context,
                                                 const mtl::Format &format,
                                                 uint32_t width,
                                                 uint32_t height,
                                                 bool renderTargetOnly,
                                                 mtl::TextureRef *textureOut)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);
    ANGLE_TRY(mtl::Texture::MakeIOSurfaceTexture(contextMtl, format, width, height, mIOSurface,
                                                 mPlane, textureOut));
    return angle::Result::Continue;
}
}
