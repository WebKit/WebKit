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

#define ANGLE_TO_EGL_TRY(EXPR)                                 \
    do                                                         \
    {                                                          \
        if (ANGLE_UNLIKELY((EXPR) != angle::Result::Continue)) \
        {                                                      \
            return egl::EglBadSurface();                       \
        }                                                      \
    } while (0)

constexpr angle::FormatID kDefaultFrameBufferDepthFormatId   = angle::FormatID::D32_FLOAT;
constexpr angle::FormatID kDefaultFrameBufferStencilFormatId = angle::FormatID::S8_UINT;
constexpr angle::FormatID kDefaultFrameBufferDepthStencilFormatId =
    angle::FormatID::D24_UNORM_S8_UINT;

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

angle::Result CreateOrResizeTexture(const gl::Context *context,
                                    const mtl::Format &format,
                                    uint32_t width,
                                    uint32_t height,
                                    uint32_t samples,
                                    bool renderTargetOnly,
                                    mtl::TextureRef *textureOut)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);
    bool allowFormatView   = format.hasDepthAndStencilBits();
    if (*textureOut)
    {
        ANGLE_TRY((*textureOut)->resize(contextMtl, width, height));
    }
    else if (samples > 1)
    {
        ANGLE_TRY(mtl::Texture::Make2DMSTexture(contextMtl, format, width, height, samples,
                                                /** renderTargetOnly */ renderTargetOnly,
                                                /** allowFormatView */ allowFormatView,
                                                textureOut));
    }
    else
    {
        ANGLE_TRY(mtl::Texture::Make2DTexture(contextMtl, format, width, height, 1,
                                              /** renderTargetOnly */ renderTargetOnly,
                                              /** allowFormatView */ allowFormatView, textureOut));
    }
    return angle::Result::Continue;
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

// SurfaceMtl implementation
SurfaceMtl::SurfaceMtl(DisplayMtl *display,
                       const egl::SurfaceState &state,
                       const egl::AttributeMap &attribs)
    : SurfaceImpl(state)
{
    mRobustResourceInit =
        attribs.get(EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE, EGL_FALSE) == EGL_TRUE;
    mColorFormat = display->getPixelFormat(angle::FormatID::B8G8R8A8_UNORM);

    mSamples = state.config->samples;

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
    mColorTexture   = nullptr;
    mDepthTexture   = nullptr;
    mStencilTexture = nullptr;

    mMSColorTexture = nullptr;

    mColorRenderTarget.reset();
    mColorManualResolveRenderTarget.reset();
    mDepthRenderTarget.reset();
    mStencilRenderTarget.reset();
}

egl::Error SurfaceMtl::initialize(const egl::Display *display)
{
    return egl::NoError();
}

FramebufferImpl *SurfaceMtl::createDefaultFramebuffer(const gl::Context *context,
                                                      const gl::FramebufferState &state)
{
    auto fbo = new FramebufferMtl(state, /* flipY */ false, /* backbuffer */ nullptr);

    return fbo;
}

egl::Error SurfaceMtl::makeCurrent(const gl::Context *context)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);
    StartFrameCapture(contextMtl);

    return egl::NoError();
}

egl::Error SurfaceMtl::unMakeCurrent(const gl::Context *context)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);
    contextMtl->flushCommandBufer();

    StopFrameCapture();
    return egl::NoError();
}

egl::Error SurfaceMtl::swap(const gl::Context *context)
{
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

EGLint SurfaceMtl::getWidth() const
{
    if (mColorTexture)
    {
        return static_cast<EGLint>(mColorTexture->widthAt0());
    }
    return 0;
}

EGLint SurfaceMtl::getHeight() const
{
    if (mColorTexture)
    {
        return static_cast<EGLint>(mColorTexture->heightAt0());
    }
    return 0;
}

EGLint SurfaceMtl::isPostSubBufferSupported() const
{
    return EGL_FALSE;
}

EGLint SurfaceMtl::getSwapBehavior() const
{
    return EGL_BUFFER_PRESERVED;
}

angle::Result SurfaceMtl::initializeContents(const gl::Context *context,
                                             const gl::ImageIndex &imageIndex)
{
    ASSERT(mColorTexture);
    ContextMtl *contextMtl = mtl::GetImpl(context);

    // Use loadAction=clear
    mtl::RenderPassDesc rpDesc;
    rpDesc.sampleCount         = mColorTexture->samples();
    rpDesc.numColorAttachments = 1;

    mColorRenderTarget.toRenderPassAttachmentDesc(&rpDesc.colorAttachments[0]);
    rpDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
    MTLClearColor black                   = {};
    rpDesc.colorAttachments[0].clearColor =
        mtl::EmulatedAlphaClearColor(black, mColorTexture->getColorWritableMask());
    if (mDepthTexture)
    {
        mDepthRenderTarget.toRenderPassAttachmentDesc(&rpDesc.depthAttachment);
        rpDesc.depthAttachment.loadAction = MTLLoadActionClear;
    }
    if (mStencilTexture)
    {
        mStencilRenderTarget.toRenderPassAttachmentDesc(&rpDesc.stencilAttachment);
        rpDesc.stencilAttachment.loadAction = MTLLoadActionClear;
    }
    mtl::RenderCommandEncoder *encoder = contextMtl->getRenderPassCommandEncoder(rpDesc);
    encoder->setStoreAction(MTLStoreActionStore);

    return angle::Result::Continue;
}

angle::Result SurfaceMtl::getAttachmentRenderTarget(const gl::Context *context,
                                                    GLenum binding,
                                                    const gl::ImageIndex &imageIndex,
                                                    GLsizei samples,
                                                    FramebufferAttachmentRenderTarget **rtOut)
{
    ASSERT(mColorTexture);

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

angle::Result SurfaceMtl::ensureCompanionTexturesSizeCorrect(const gl::Context *context,
                                                             const gl::Extents &size)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);

    ASSERT(mColorTexture);

    if (mSamples > 1 && (!mMSColorTexture || mMSColorTexture->sizeAt0() != size))
    {
        mAutoResolveMSColorTexture =
            contextMtl->getDisplay()->getFeatures().allowMultisampleStoreAndResolve.enabled;
        ANGLE_TRY(CreateOrResizeTexture(context, mColorFormat, size.width, size.height, mSamples,
                                        /** renderTargetOnly */ mAutoResolveMSColorTexture,
                                        &mMSColorTexture));

        if (mAutoResolveMSColorTexture)
        {
            // Use auto MSAA resolve at the end of render pass.
            mColorRenderTarget.setImplicitMSTexture(mMSColorTexture);
        }
        else
        {
            mColorRenderTarget.setTexture(mMSColorTexture);
        }
    }

    if (mDepthFormat.valid() && (!mDepthTexture || mDepthTexture->sizeAt0() != size))
    {
        ANGLE_TRY(CreateOrResizeTexture(context, mDepthFormat, size.width, size.height, mSamples,
                                        /** renderTargetOnly */ false, &mDepthTexture));

        mDepthRenderTarget.set(mDepthTexture, mtl::kZeroNativeMipLevel, 0, mDepthFormat);
    }

    if (mStencilFormat.valid() && (!mStencilTexture || mStencilTexture->sizeAt0() != size))
    {
        if (mUsePackedDepthStencil)
        {
            mStencilTexture = mDepthTexture;
        }
        else
        {
            ANGLE_TRY(CreateOrResizeTexture(context, mStencilFormat, size.width, size.height,
                                            mSamples,
                                            /** renderTargetOnly */ false, &mStencilTexture));
        }

        mStencilRenderTarget.set(mStencilTexture, mtl::kZeroNativeMipLevel, 0, mStencilFormat);
    }

    return angle::Result::Continue;
}

angle::Result SurfaceMtl::resolveColorTextureIfNeeded(const gl::Context *context)
{
    ASSERT(mMSColorTexture);
    if (!mAutoResolveMSColorTexture)
    {
        // Manually resolve texture
        ContextMtl *contextMtl = mtl::GetImpl(context);

        mColorManualResolveRenderTarget.set(mColorTexture, mtl::kZeroNativeMipLevel, 0,
                                            mColorFormat);
        mtl::RenderCommandEncoder *encoder =
            contextMtl->getRenderTargetCommandEncoder(mColorManualResolveRenderTarget);
        ANGLE_TRY(contextMtl->getDisplay()->getUtils().blitColorWithDraw(
            context, encoder, mColorFormat.actualAngleFormat(), mMSColorTexture));
        contextMtl->endEncoding(true);
        mColorManualResolveRenderTarget.reset();
    }
    return angle::Result::Continue;
}

// WindowSurfaceMtl implementation.
WindowSurfaceMtl::WindowSurfaceMtl(DisplayMtl *display,
                                   const egl::SurfaceState &state,
                                   EGLNativeWindowType window,
                                   const egl::AttributeMap &attribs)
    : SurfaceMtl(display, state, attribs), mLayer((__bridge CALayer *)(window))
{
    // NOTE(hqle): Width and height attributes is ignored for now.
    mCurrentKnownDrawableSize = CGSizeMake(0, 0);
}

WindowSurfaceMtl::~WindowSurfaceMtl() {}

void WindowSurfaceMtl::destroy(const egl::Display *display)
{
    SurfaceMtl::destroy(display);

    mCurrentDrawable = nil;
    if (mMetalLayer && mMetalLayer.get() != mLayer)
    {
        // If we created metal layer in WindowSurfaceMtl::initialize(),
        // we need to detach it from super layer now.
        [mMetalLayer.get() removeFromSuperlayer];
    }
    mMetalLayer = nil;
}

egl::Error WindowSurfaceMtl::initialize(const egl::Display *display)
{
    egl::Error re = SurfaceMtl::initialize(display);
    if (re.isError())
    {
        return re;
    }

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
        mMetalLayer.get().framebufferOnly = NO;  // Support blitting and glReadPixels

#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
        // Autoresize with parent layer.
        mMetalLayer.get().autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
#endif

        // ensure drawableSize is set to correct value:
        mMetalLayer.get().drawableSize = mCurrentKnownDrawableSize = calcExpectedDrawableSize();

        if (mMetalLayer.get() != mLayer)
        {
            mMetalLayer.get().contentsScale = mLayer.contentsScale;

            [mLayer addSublayer:mMetalLayer.get()];
        }
    }

    return egl::NoError();
}

FramebufferImpl *WindowSurfaceMtl::createDefaultFramebuffer(const gl::Context *context,
                                                            const gl::FramebufferState &state)
{
    auto fbo = new FramebufferMtl(state, /* flipY */ true, /* backbuffer */ this);

    return fbo;
}

egl::Error WindowSurfaceMtl::swap(const gl::Context *context)
{
    ANGLE_TO_EGL_TRY(swapImpl(context));

    return egl::NoError();
}

void WindowSurfaceMtl::setSwapInterval(EGLint interval)
{
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    mMetalLayer.get().displaySyncEnabled = interval != 0;
#endif
}

// width and height can change with client window resizing
EGLint WindowSurfaceMtl::getWidth() const
{
    return static_cast<EGLint>(mCurrentKnownDrawableSize.width);
}

EGLint WindowSurfaceMtl::getHeight() const
{
    return static_cast<EGLint>(mCurrentKnownDrawableSize.height);
}

EGLint WindowSurfaceMtl::getSwapBehavior() const
{
    return EGL_BUFFER_DESTROYED;
}

angle::Result WindowSurfaceMtl::initializeContents(const gl::Context *context,
                                                   const gl::ImageIndex &imageIndex)
{
    bool newDrawable;
    ANGLE_TRY(ensureCurrentDrawableObtained(context, &newDrawable));

    if (!newDrawable)
    {
        return angle::Result::Continue;
    }

    return SurfaceMtl::initializeContents(context, imageIndex);
}

angle::Result WindowSurfaceMtl::getAttachmentRenderTarget(const gl::Context *context,
                                                          GLenum binding,
                                                          const gl::ImageIndex &imageIndex,
                                                          GLsizei samples,
                                                          FramebufferAttachmentRenderTarget **rtOut)
{
    ANGLE_TRY(ensureCurrentDrawableObtained(context, nullptr));
    ANGLE_TRY(ensureCompanionTexturesSizeCorrect(context));

    return SurfaceMtl::getAttachmentRenderTarget(context, binding, imageIndex, samples, rtOut);
}

angle::Result WindowSurfaceMtl::ensureCurrentDrawableObtained(const gl::Context *context,
                                                              bool *newDrawableOut)
{
    if (newDrawableOut)
    {
        *newDrawableOut = !mCurrentDrawable;
    }

    if (!mCurrentDrawable)
    {
        ANGLE_TRY(obtainNextDrawable(context));
    }

    return angle::Result::Continue;
}

angle::Result WindowSurfaceMtl::ensureCompanionTexturesSizeCorrect(const gl::Context *context)
{
    ASSERT(mMetalLayer);

    gl::Extents size(static_cast<int>(mMetalLayer.get().drawableSize.width),
                     static_cast<int>(mMetalLayer.get().drawableSize.height), 1);

    ANGLE_TRY(SurfaceMtl::ensureCompanionTexturesSizeCorrect(context, size));

    return angle::Result::Continue;
}

angle::Result WindowSurfaceMtl::ensureColorTextureReadyForReadPixels(const gl::Context *context)
{
    ANGLE_TRY(ensureCurrentDrawableObtained(context, nullptr));

    if (mMSColorTexture)
    {
        if (mMSColorTexture->isCPUReadMemNeedSync())
        {
            ANGLE_TRY(resolveColorTextureIfNeeded(context));
            mMSColorTexture->resetCPUReadMemNeedSync();
        }
    }

    return angle::Result::Continue;
}

CGSize WindowSurfaceMtl::calcExpectedDrawableSize() const
{
    CGSize currentLayerSize           = mMetalLayer.get().bounds.size;
    CGFloat currentLayerContentsScale = mMetalLayer.get().contentsScale;
    CGSize expectedDrawableSize = CGSizeMake(currentLayerSize.width * currentLayerContentsScale,
                                             currentLayerSize.height * currentLayerContentsScale);

    return expectedDrawableSize;
}

bool WindowSurfaceMtl::checkIfLayerResized(const gl::Context *context)
{
    if (mMetalLayer.get() != mLayer && mMetalLayer.get().contentsScale != mLayer.contentsScale)
    {
        // Parent layer's content scale has changed, update Metal layer's scale factor.
        mMetalLayer.get().contentsScale = mLayer.contentsScale;
    }

    CGSize currentLayerDrawableSize = mMetalLayer.get().drawableSize;
    CGSize expectedDrawableSize     = calcExpectedDrawableSize();

    // NOTE(hqle): We need to compare the size against mCurrentKnownDrawableSize also.
    // That is because metal framework might internally change the drawableSize property of
    // metal layer, and it might become equal to expectedDrawableSize. If that happens, we cannot
    // know whether the layer has been resized or not.
    if (currentLayerDrawableSize.width != expectedDrawableSize.width ||
        currentLayerDrawableSize.height != expectedDrawableSize.height ||
        mCurrentKnownDrawableSize.width != expectedDrawableSize.width ||
        mCurrentKnownDrawableSize.height != expectedDrawableSize.height)
    {
        // Resize the internal drawable texture.
        mMetalLayer.get().drawableSize = mCurrentKnownDrawableSize = expectedDrawableSize;

        return true;
    }

    return false;
}

angle::Result WindowSurfaceMtl::obtainNextDrawable(const gl::Context *context)
{
    ANGLE_MTL_OBJC_SCOPE
    {
        ContextMtl *contextMtl = mtl::GetImpl(context);

        ANGLE_MTL_TRY(contextMtl, mMetalLayer);

        // Check if layer was resized
        if (checkIfLayerResized(context))
        {
            contextMtl->onBackbufferResized(context, this);
        }

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

        if (!mColorTexture)
        {
            mColorTexture = mtl::Texture::MakeFromMetal(mCurrentDrawable.get().texture);
            ASSERT(!mColorRenderTarget.getTexture());
            mColorRenderTarget.setWithImplicitMSTexture(mColorTexture, mMSColorTexture,
                                                        mtl::kZeroNativeMipLevel, 0, mColorFormat);
        }
        else
        {
            mColorTexture->set(mCurrentDrawable.get().texture);
        }

        ANGLE_MTL_LOG("Current metal drawable size=%d,%d", mColorTexture->width(),
                      mColorTexture->height());

        // Now we have to resize depth stencil buffers if required.
        ANGLE_TRY(ensureCompanionTexturesSizeCorrect(context));

        return angle::Result::Continue;
    }
}

angle::Result WindowSurfaceMtl::swapImpl(const gl::Context *context)
{
    if (mCurrentDrawable)
    {
        ASSERT(mColorTexture);

        ContextMtl *contextMtl = mtl::GetImpl(context);

        if (mMSColorTexture)
        {
            ANGLE_TRY(resolveColorTextureIfNeeded(context));
        }

        contextMtl->present(context, mCurrentDrawable);

        StopFrameCapture();
        StartFrameCapture(contextMtl);

        // Invalidate current drawable
        mColorTexture->set(nil);
        mCurrentDrawable = nil;
    }

    return angle::Result::Continue;
}

// OffscreenSurfaceMtl implementation
OffscreenSurfaceMtl::OffscreenSurfaceMtl(DisplayMtl *display,
                                         const egl::SurfaceState &state,
                                         const egl::AttributeMap &attribs)
    : SurfaceMtl(display, state, attribs)
{
    mSize = gl::Extents(attribs.getAsInt(EGL_WIDTH, 1), attribs.getAsInt(EGL_HEIGHT, 1), 1);
}

OffscreenSurfaceMtl::~OffscreenSurfaceMtl() {}

void OffscreenSurfaceMtl::destroy(const egl::Display *display)
{
    SurfaceMtl::destroy(display);
}

egl::Error OffscreenSurfaceMtl::swap(const gl::Context *context)
{
    // Check for surface resize.
    ANGLE_TO_EGL_TRY(ensureTexturesSizeCorrect(context));

    return egl::NoError();
}

egl::Error OffscreenSurfaceMtl::bindTexImage(const gl::Context *context,
                                             gl::Texture *texture,
                                             EGLint buffer)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);
    contextMtl->flushCommandBufer();

    // Initialize offscreen textures if needed:
    ANGLE_TO_EGL_TRY(ensureTexturesSizeCorrect(context));

    return egl::NoError();
}

egl::Error OffscreenSurfaceMtl::releaseTexImage(const gl::Context *context, EGLint buffer)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);

    if (mMSColorTexture)
    {
        ANGLE_TO_EGL_TRY(resolveColorTextureIfNeeded(context));
    }

    // NOTE(hqle): Should we finishCommandBuffer or flush is enough?
    contextMtl->flushCommandBufer();
    return egl::NoError();
}

angle::Result OffscreenSurfaceMtl::getAttachmentRenderTarget(
    const gl::Context *context,
    GLenum binding,
    const gl::ImageIndex &imageIndex,
    GLsizei samples,
    FramebufferAttachmentRenderTarget **rtOut)
{
    // Initialize offscreen textures if needed:
    ANGLE_TRY(ensureTexturesSizeCorrect(context));

    return SurfaceMtl::getAttachmentRenderTarget(context, binding, imageIndex, samples, rtOut);
}

angle::Result OffscreenSurfaceMtl::ensureTexturesSizeCorrect(const gl::Context *context)
{
    if (!mColorTexture || mColorTexture->sizeAt0() != mSize)
    {
        ANGLE_TRY(CreateOrResizeTexture(context, mColorFormat, mSize.width, mSize.height, 1,
                                        /** renderTargetOnly */ false, &mColorTexture));

        mColorRenderTarget.set(mColorTexture, mtl::kZeroNativeMipLevel, 0, mColorFormat);
    }

    return ensureCompanionTexturesSizeCorrect(context, mSize);
}

// PBufferSurfaceMtl implementation
PBufferSurfaceMtl::PBufferSurfaceMtl(DisplayMtl *display,
                                     const egl::SurfaceState &state,
                                     const egl::AttributeMap &attribs)
    : OffscreenSurfaceMtl(display, state, attribs)
{}

void PBufferSurfaceMtl::setFixedWidth(EGLint width)
{
    mSize.width = width;
}

void PBufferSurfaceMtl::setFixedHeight(EGLint height)
{
    mSize.height = height;
}

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
}

}
