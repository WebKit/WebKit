//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SurfaceMtl.mm:
//    Implements the class methods for SurfaceMtl.
//

#include "libANGLE/renderer/metal/SurfaceMtl.h"

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
#    define ANGLE_METAL_FRAME_CAPTURE_ENABLED 1
#endif

namespace rx
{

namespace
{
constexpr angle::FormatID kDefaultFrameBufferDepthFormatId   = angle::FormatID::D32_FLOAT;
constexpr angle::FormatID kDefaultFrameBufferStencilFormatId = angle::FormatID::S8_UINT;
constexpr angle::FormatID kDefaultFrameBufferDepthStencilFormatId =
    angle::FormatID::D24_UNORM_S8_UINT;

ANGLE_MTL_UNUSED
bool IsFrameCaptureEnabled()
{
#if !ANGLE_METAL_FRAME_CAPTURE_ENABLED
    return false;
#else
    // We only support frame capture programmatically if the ANGLE_METAL_FRAME_CAPTURE
    // environment flag is set. Otherwise, it will slow down the rendering. This allows user to
    // finely control whether he wants to capture the frame for particular application or not.
    auto var                  = std::getenv("ANGLE_METAL_FRAME_CAPTURE");
    static const bool enabled = var ? (strcmp(var, "1") == 0) : false;

    return enabled;
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

        NSError *error;
        if (![captureManager startCaptureWithDescriptor:captureDescriptor error:&error])
        {
            NSLog(@"Failed to start capture, error %@", error);
        }
    }
    else
#    endif  // __MAC_10_15
    {
        if (FrameCaptureDeviceScope())
        {
            [captureManager startCaptureWithDevice:metalDevice];
        }
        else
        {
            [captureManager startCaptureWithCommandQueue:metalCmdQueue];
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
}

SurfaceMtl::SurfaceMtl(DisplayMtl *display,
                       const egl::SurfaceState &state,
                       EGLNativeWindowType window,
                       const egl::AttributeMap &attribs)
    : SurfaceImpl(state), mLayer((__bridge CALayer *)(window))
{
    // NOTE(hqle): Width and height attributes is ignored for now.

    // https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf says that BGRA8Unorm is
    // only supported if depth24Stencil8PixelFormatSupported capabilitiy is YES. Yet
    // CAMetalLayer can be created with pixelFormat MTLPixelFormatBGRA8Unorm. So the mtl::Format
    // used for SurfaceMtl is initialized a bit differently from normal TextureMtl's mtl::Format.
    // It won't use format table, instead we initialize its values here to use BGRA8Unorm directly:
    mColorFormat.intendedFormatId = mColorFormat.actualFormatId = angle::FormatID::B8G8R8A8_UNORM;
    mColorFormat.metalFormat                                    = MTLPixelFormatBGRA8Unorm;

    int depthBits   = 0;
    int stencilBits = 0;
    if (state.config)
    {
        depthBits   = state.config->depthSize;
        stencilBits = state.config->stencilSize;
    }

    if (depthBits && stencilBits)
    {
        if (display->getFeatures().allowSeparatedDepthStencilBuffers.enabled)
        {
            mDepthFormat   = display->getPixelFormat(kDefaultFrameBufferDepthFormatId);
            mStencilFormat = display->getPixelFormat(kDefaultFrameBufferStencilFormatId);
        }
        else
        {
            // We must use packed depth stencil
            mUsePackedDepthStencil = true;
            mDepthFormat   = display->getPixelFormat(kDefaultFrameBufferDepthStencilFormatId);
            mStencilFormat = mDepthFormat;
        }
    }
    else if (depthBits)
    {
        mDepthFormat = display->getPixelFormat(kDefaultFrameBufferDepthFormatId);
    }
    else if (stencilBits)
    {
        mStencilFormat = display->getPixelFormat(kDefaultFrameBufferStencilFormatId);
    }
}

SurfaceMtl::~SurfaceMtl() {}

void SurfaceMtl::destroy(const egl::Display *display)
{
    mDrawableTexture = nullptr;
    mDepthTexture    = nullptr;
    mStencilTexture  = nullptr;
    mCurrentDrawable = nil;
    if (mMetalLayer && mMetalLayer.get() != mLayer)
    {
        // If we created metal layer in SurfaceMtl::initialize(),
        // we need to detach it from super layer now.
        [mMetalLayer.get() removeFromSuperlayer];
    }
    mMetalLayer = nil;
}

egl::Error SurfaceMtl::initialize(const egl::Display *display)
{
    DisplayMtl *displayMtl    = mtl::GetImpl(display);
    id<MTLDevice> metalDevice = displayMtl->getMetalDevice();

    StartFrameCapture(metalDevice, displayMtl->cmdQueue().get());

    ANGLE_MTL_OBJC_SCOPE
    {
        if ([mLayer isKindOfClass:CAMetalLayer.class])
        {
            mMetalLayer.retainAssign(static_cast<CAMetalLayer *>(mLayer));
        }
        else
        {
            mMetalLayer             = [[[CAMetalLayer alloc] init] ANGLE_MTL_AUTORELEASE];
            mMetalLayer.get().frame = mLayer.frame;
        }

        mMetalLayer.get().device          = metalDevice;
        mMetalLayer.get().pixelFormat     = mColorFormat.metalFormat;
        mMetalLayer.get().framebufferOnly = NO;  // This to allow readPixels

#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
        // Autoresize with parent layer.
        mMetalLayer.get().autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
#endif

        if (mMetalLayer.get() != mLayer)
        {
            mMetalLayer.get().contentsScale = mLayer.contentsScale;

            [mLayer addSublayer:mMetalLayer.get()];
        }

        // ensure drawableSize is set to correct value:
        checkIfLayerResized();
    }

    return egl::NoError();
}

FramebufferImpl *SurfaceMtl::createDefaultFramebuffer(const gl::Context *context,
                                                      const gl::FramebufferState &state)
{
    auto fbo = new FramebufferMtl(state, /* flipY */ true);

    return fbo;
}

egl::Error SurfaceMtl::makeCurrent(const gl::Context *context)
{
    angle::Result result = obtainNextDrawable(context);
    if (result != angle::Result::Continue)
    {
        return egl::EglBadCurrentSurface();
    }
    return egl::NoError();
}

egl::Error SurfaceMtl::unMakeCurrent(const gl::Context *context)
{
    return egl::NoError();
}

egl::Error SurfaceMtl::swap(const gl::Context *context)
{
    angle::Result result = swapImpl(context);

    if (result != angle::Result::Continue)
    {
        return egl::EglBadSurface();
    }

    return egl::NoError();
}

egl::Error SurfaceMtl::postSubBuffer(const gl::Context *context,
                                     EGLint x,
                                     EGLint y,
                                     EGLint width,
                                     EGLint height)
{
    UNIMPLEMENTED();
    return egl::EglBadAccess();
}

egl::Error SurfaceMtl::querySurfacePointerANGLE(EGLint attribute, void **value)
{
    UNIMPLEMENTED();
    return egl::EglBadAccess();
}

egl::Error SurfaceMtl::bindTexImage(const gl::Context *context, gl::Texture *texture, EGLint buffer)
{
    UNIMPLEMENTED();
    return egl::EglBadAccess();
}

egl::Error SurfaceMtl::releaseTexImage(const gl::Context *context, EGLint buffer)
{
    UNIMPLEMENTED();
    return egl::EglBadAccess();
}

egl::Error SurfaceMtl::getSyncValues(EGLuint64KHR *ust, EGLuint64KHR *msc, EGLuint64KHR *sbc)
{
    UNIMPLEMENTED();
    return egl::EglBadAccess();
}

egl::Error SurfaceMtl::getMscRate(EGLint *numerator, EGLint *denominator)
{
    UNIMPLEMENTED();
    return egl::EglBadAccess();
}

void SurfaceMtl::setSwapInterval(EGLint interval) {}

void SurfaceMtl::setFixedWidth(EGLint width)
{
    UNIMPLEMENTED();
}

void SurfaceMtl::setFixedHeight(EGLint height)
{
    UNIMPLEMENTED();
}

// width and height can change with client window resizing
EGLint SurfaceMtl::getWidth() const
{
    if (mDrawableTexture)
    {
        return static_cast<EGLint>(mDrawableTexture->width());
    }

    if (mMetalLayer)
    {
        return static_cast<EGLint>(mMetalLayer.get().drawableSize.width);
    }
    return 0;
}

EGLint SurfaceMtl::getHeight() const
{
    if (mDrawableTexture)
    {
        return static_cast<EGLint>(mDrawableTexture->height());
    }

    if (mMetalLayer)
    {
        return static_cast<EGLint>(mMetalLayer.get().drawableSize.height);
    }
    return 0;
}

EGLint SurfaceMtl::isPostSubBufferSupported() const
{
    return EGL_FALSE;
}

EGLint SurfaceMtl::getSwapBehavior() const
{
    return EGL_BUFFER_DESTROYED;
}

angle::Result SurfaceMtl::getAttachmentRenderTarget(const gl::Context *context,
                                                    GLenum binding,
                                                    const gl::ImageIndex &imageIndex,
                                                    GLsizei samples,
                                                    FramebufferAttachmentRenderTarget **rtOut)
{
    // NOTE(hqle): Support MSAA.
    ANGLE_TRY(ensureRenderTargetsCreated(context));

    switch (binding)
    {
        case GL_BACK:
            *rtOut = &mColorRenderTarget;
            break;
        case GL_DEPTH:
            *rtOut = mDepthFormat.valid() ? &mDepthRenderTarget : nullptr;
            break;
        case GL_STENCIL:
            *rtOut = mStencilFormat.valid() ? &mStencilRenderTarget : nullptr;
            break;
        case GL_DEPTH_STENCIL:
            // NOTE(hqle): ES 3.0 feature
            UNREACHABLE();
            break;
    }

    return angle::Result::Continue;
}

angle::Result SurfaceMtl::ensureRenderTargetsCreated(const gl::Context *context)
{
    if (!mDrawableTexture)
    {
        ANGLE_TRY(obtainNextDrawable(context));
    }

    return angle::Result::Continue;
}

angle::Result SurfaceMtl::ensureDepthStencilSizeCorrect(const gl::Context *context,
                                                        gl::Framebuffer::DirtyBits *fboDirtyBits)
{
    ASSERT(mDrawableTexture && mDrawableTexture->get());

    ContextMtl *contextMtl = mtl::GetImpl(context);
    auto size              = mDrawableTexture->size();

    if (mDepthFormat.valid() && (!mDepthTexture || mDepthTexture->size() != size))
    {
        ANGLE_TRY(mtl::Texture::Make2DTexture(contextMtl, mDepthFormat, size.width, size.height, 1,
                                              true, false, &mDepthTexture));

        mDepthRenderTarget.set(mDepthTexture, 0, 0, mDepthFormat);
        fboDirtyBits->set(gl::Framebuffer::DIRTY_BIT_DEPTH_ATTACHMENT);
    }

    if (mStencilFormat.valid() && (!mStencilTexture || mStencilTexture->size() != size))
    {
        if (mUsePackedDepthStencil)
        {
            mStencilTexture = mDepthTexture;
        }
        else
        {
            ANGLE_TRY(mtl::Texture::Make2DTexture(contextMtl, mStencilFormat, size.width,
                                                  size.height, 1, true, false, &mStencilTexture));
        }

        mStencilRenderTarget.set(mStencilTexture, 0, 0, mStencilFormat);
        fboDirtyBits->set(gl::Framebuffer::DIRTY_BIT_STENCIL_ATTACHMENT);
    }

    return angle::Result::Continue;
}

void SurfaceMtl::checkIfLayerResized()
{
    CGSize currentDrawableSize        = mMetalLayer.get().drawableSize;
    CGSize currentLayerSize           = mMetalLayer.get().bounds.size;
    CGFloat currentLayerContentsScale = mMetalLayer.get().contentsScale;
    CGSize expectedDrawableSize = CGSizeMake(currentLayerSize.width * currentLayerContentsScale,
                                             currentLayerSize.height * currentLayerContentsScale);
    if (currentDrawableSize.width != expectedDrawableSize.width ||
        currentDrawableSize.height != expectedDrawableSize.height)
    {
        // Resize the internal drawable texture.
        mMetalLayer.get().drawableSize = expectedDrawableSize;
    }
}

angle::Result SurfaceMtl::obtainNextDrawable(const gl::Context *context)
{
    checkIfLayerResized();

    ANGLE_MTL_OBJC_SCOPE
    {
        ContextMtl *contextMtl = mtl::GetImpl(context);

        StartFrameCapture(contextMtl);

        ANGLE_MTL_TRY(contextMtl, mMetalLayer);

        if (mDrawableTexture)
        {
            mDrawableTexture->set(nil);
        }

        mCurrentDrawable = nil;
        mCurrentDrawable.retainAssign([mMetalLayer nextDrawable]);
        if (!mCurrentDrawable)
        {
            // The GPU might be taking too long finishing its rendering to the previous frame.
            // Try again, indefinitely wait until the previous frame render finishes.
            // TODO: this may wait forever here
            mMetalLayer.get().allowsNextDrawableTimeout = NO;
            mCurrentDrawable.retainAssign([mMetalLayer nextDrawable]);
            mMetalLayer.get().allowsNextDrawableTimeout = YES;
        }

        if (!mDrawableTexture)
        {
            mDrawableTexture = mtl::Texture::MakeFromMetal(mCurrentDrawable.get().texture);
            mColorRenderTarget.set(mDrawableTexture, 0, 0, mColorFormat);
        }
        else
        {
            mDrawableTexture->set(mCurrentDrawable.get().texture);
        }

        ANGLE_MTL_LOG("Current metal drawable size=%d,%d", mDrawableTexture->width(),
                      mDrawableTexture->height());

        gl::Framebuffer::DirtyBits fboDirtyBits;
        fboDirtyBits.set(gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_0);

        // Now we have to resize depth stencil buffers if necessary.
        ANGLE_TRY(ensureDepthStencilSizeCorrect(context, &fboDirtyBits));

        // Need to notify default framebuffer to invalidate its render targets.
        // Since a new drawable texture has been obtained, also, the depth stencil
        // buffers might have been resized.
        gl::Framebuffer *defaultFbo =
            context->getFramebuffer(gl::Framebuffer::kDefaultDrawFramebufferHandle);
        if (defaultFbo)
        {
            FramebufferMtl *framebufferMtl = mtl::GetImpl(defaultFbo);
            ANGLE_TRY(framebufferMtl->syncState(context, fboDirtyBits));
        }

        return angle::Result::Continue;
    }
}

angle::Result SurfaceMtl::swapImpl(const gl::Context *context)
{
    ANGLE_TRY(ensureRenderTargetsCreated(context));

    ContextMtl *contextMtl = mtl::GetImpl(context);

    contextMtl->present(context, mCurrentDrawable);

    StopFrameCapture();

    ANGLE_TRY(obtainNextDrawable(context));

    return angle::Result::Continue;
}
}
