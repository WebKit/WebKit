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
    size_t componentBytes;

    angle::FormatID nativeAngleFormatId;
};

// clang-format off
// NOTE(hqle): Support R16_UINT once GLES3 is complete.
constexpr std::array<IOSurfaceFormatInfo, 8> kIOSurfaceFormats = {{
    {GL_RED,      GL_UNSIGNED_BYTE,               1, angle::FormatID::R8_UNORM},
    {GL_RED,      GL_UNSIGNED_SHORT,              2, angle::FormatID::R16_UNORM},
    {GL_RG,       GL_UNSIGNED_BYTE,               2, angle::FormatID::R8G8_UNORM},
    {GL_RG,       GL_UNSIGNED_SHORT,              4, angle::FormatID::R16G16_UNORM},
    {GL_RGB,      GL_UNSIGNED_BYTE,               4, angle::FormatID::B8G8R8A8_UNORM},
    {GL_BGRA_EXT, GL_UNSIGNED_BYTE,               4, angle::FormatID::B8G8R8A8_UNORM},
    {GL_RGBA,     GL_HALF_FLOAT,                  8, angle::FormatID::R16G16B16A16_FLOAT},
    {GL_RGB10_A2, GL_UNSIGNED_INT_2_10_10_10_REV, 4, angle::FormatID::B10G10R10A2_UNORM},
}};
// clang-format on

int FindIOSurfaceFormatIndex(GLenum internalFormat, GLenum type)
{
    for (int i = 0; i < static_cast<int>(kIOSurfaceFormats.size()); ++i)
    {
        const auto &formatInfo = kIOSurfaceFormats[i];
        if (formatInfo.internalFormat == internalFormat && formatInfo.type == type)
        {
            return i;
        }
    }
    return -1;
}

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

// IOSurfaceSurfaceMtl implementation.
IOSurfaceSurfaceMtl::IOSurfaceSurfaceMtl(DisplayMtl *display,
                                         const egl::SurfaceState &state,
                                         EGLClientBuffer buffer,
                                         const egl::AttributeMap &attribs)
    : OffscreenSurfaceMtl(display, state, attribs), mIOSurface((__bridge IOSurfaceRef)(buffer))
{
    CFRetain(mIOSurface);

    mIOSurfacePlane = static_cast<int>(attribs.get(EGL_IOSURFACE_PLANE_ANGLE));

    EGLAttrib internalFormat = attribs.get(EGL_TEXTURE_INTERNAL_FORMAT_ANGLE);
    EGLAttrib type           = attribs.get(EGL_TEXTURE_TYPE_ANGLE);
    mIOSurfaceFormatIdx =
        FindIOSurfaceFormatIndex(static_cast<GLenum>(internalFormat), static_cast<GLenum>(type));
    ASSERT(mIOSurfaceFormatIdx >= 0);

    mColorFormat =
        display->getPixelFormat(kIOSurfaceFormats[mIOSurfaceFormatIdx].nativeAngleFormatId);
}
IOSurfaceSurfaceMtl::~IOSurfaceSurfaceMtl()
{
    if (mIOSurface != nullptr)
    {
        CFRelease(mIOSurface);
        mIOSurface = nullptr;
    }
}

egl::Error IOSurfaceSurfaceMtl::bindTexImage(const gl::Context *context,
                                             gl::Texture *texture,
                                             EGLint buffer)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);
    StartFrameCapture(contextMtl);

    // Initialize offscreen texture if needed:
    ANGLE_TO_EGL_TRY(ensureColorTextureCreated(context));

    return OffscreenSurfaceMtl::bindTexImage(context, texture, buffer);
}

egl::Error IOSurfaceSurfaceMtl::releaseTexImage(const gl::Context *context, EGLint buffer)
{
    egl::Error re = OffscreenSurfaceMtl::releaseTexImage(context, buffer);
    StopFrameCapture();
    return re;
}

angle::Result IOSurfaceSurfaceMtl::getAttachmentRenderTarget(
    const gl::Context *context,
    GLenum binding,
    const gl::ImageIndex &imageIndex,
    GLsizei samples,
    FramebufferAttachmentRenderTarget **rtOut)
{
    // Initialize offscreen texture if needed:
    ANGLE_TRY(ensureColorTextureCreated(context));

    return OffscreenSurfaceMtl::getAttachmentRenderTarget(context, binding, imageIndex, samples,
                                                          rtOut);
}

angle::Result IOSurfaceSurfaceMtl::ensureColorTextureCreated(const gl::Context *context)
{
    if (mColorTexture)
    {
        return angle::Result::Continue;
    }
    ContextMtl *contextMtl = mtl::GetImpl(context);
    ANGLE_MTL_OBJC_SCOPE
    {
        auto texDesc =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:mColorFormat.metalFormat
                                                               width:mSize.width
                                                              height:mSize.height
                                                           mipmapped:NO];

        texDesc.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;

        id<MTLTexture> texture =
            [contextMtl->getMetalDevice() newTextureWithDescriptor:texDesc
                                                         iosurface:mIOSurface
                                                             plane:mIOSurfacePlane];

        mColorTexture = mtl::Texture::MakeFromMetal([texture ANGLE_MTL_AUTORELEASE]);
    }

    mColorRenderTarget.set(mColorTexture, mtl::kZeroNativeMipLevel, 0, mColorFormat);

    if (kIOSurfaceFormats[mIOSurfaceFormatIdx].internalFormat == GL_RGB)
    {
        // This format has emulated alpha channel. Initialize texture's alpha channel to 1.0.
        const mtl::Format &rgbClearFormat =
            contextMtl->getPixelFormat(angle::FormatID::R8G8B8_UNORM);
        ANGLE_TRY(mtl::InitializeTextureContentsGPU(
            context, mColorTexture, rgbClearFormat,
            mtl::ImageNativeIndex::FromBaseZeroGLIndex(gl::ImageIndex::Make2D(0)),
            MTLColorWriteMaskAlpha));

        // Disable subsequent rendering to alpha channel.
        mColorTexture->setColorWritableMask(MTLColorWriteMaskAll & (~MTLColorWriteMaskAlpha));
    }

    return angle::Result::Continue;
}

// static
bool IOSurfaceSurfaceMtl::ValidateAttributes(EGLClientBuffer buffer,
                                             const egl::AttributeMap &attribs)
{
    IOSurfaceRef ioSurface = (__bridge IOSurfaceRef)(buffer);

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
}}
