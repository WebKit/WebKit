/*
 * Copyright (C) 2009-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"

#if ENABLE(WEBGL)
#import "GraphicsContextGLCocoa.h"

#import "ANGLEHeaders.h"
#import "ANGLEUtilities.h"
#import "ANGLEUtilitiesCocoa.h"
#import "CVUtilities.h"
#import "GraphicsLayerContentsDisplayDelegate.h"
#import "IOSurfacePool.h"
#import "Logging.h"
#import "PixelBuffer.h"
#import "ProcessIdentity.h"
#import <CoreGraphics/CGBitmapContext.h>
#import <Metal/Metal.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/MetalSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/RuntimeApplicationChecks.h>
#import <wtf/StdLibExtras.h>
#import <wtf/darwin/WeakLinking.h>
#import <wtf/text/CString.h>

#if PLATFORM(IOS_FAMILY)
#import "WebCoreThread.h"
#endif

#if ENABLE(VIDEO)
#import "GraphicsContextGLCVCocoa.h"
#import "MediaPlayerPrivate.h"
#import "VideoFrameCV.h"
#endif

#if ENABLE(MEDIA_STREAM)
#import "ImageRotationSessionVT.h"
#endif

// FIXME: Checking for EGL_Initialize does not seem to be robust in recovery OS.
WTF_WEAK_LINK_FORCE_IMPORT(EGL_GetPlatformDisplayEXT);

namespace WebCore {

using GL = GraphicsContextGL;

// This variable is accessed in single-threaded manner.
// For WK1, this variable is accessed from multiple threads but always sequentially.
static GraphicsContextGLANGLE* currentContext;

static const char* const enabledANGLEMetalFeatures[] = {
    "ensureLoopForwardProgress",
    nullptr
};

static const char* const disabledANGLEMetalFeatures[] = {
    "enableInMemoryMtlLibraryCache", // This would leak all program binary objects.
    "alwaysPreferStagedTextureUploads", // This would timeout tests due to excess staging buffer allocations and fail tests on MacPro.
    "injectAsmStatementIntoLoopBodies", // Replaced by ensureLoopForwardProgress.
    nullptr
};

static bool platformSupportsMetal()
{
    auto device = adoptNS(MTLCreateSystemDefaultDevice());
    if (!device)
        return false;
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    // Old Macs, such as MacBookPro11,4 cannot use WebGL via Metal.
    // This check can be removed once they are no longer supported.
    if (![device supportsFamily:MTLGPUFamilyMac2])
        return false;
#endif
    return true;
}

static EGLDisplay initializeEGLDisplay(const GraphicsContextGLAttributes& attrs)
{
    if (!platformIsANGLEAvailable()) {
        WTFLogAlways("Failed to load ANGLE shared library.");
        return EGL_NO_DISPLAY;
    }
    // FIXME(http://webkit.org/b/238448): Why is checking EGL_Initialize not robust in recovery OS?
    if (EGL_GetPlatformDisplayEXT == NULL) { // NOLINT
        WTFLogAlways("Inconsistent weak linking for ANGLE shared library.");
        return EGL_NO_DISPLAY;
    }

#if ASSERT_ENABLED
    auto clientExtensions = unsafeSpan(EGL_QueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS));
    ASSERT(clientExtensions.data());
#endif

    Vector<EGLAttrib> displayAttributes;
    displayAttributes.append(EGL_PLATFORM_ANGLE_TYPE_ANGLE);
    displayAttributes.append(EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE);
    // These properties are defined for EGL_ANGLE_power_preference as EGLContext attributes,
    // but Metal backend uses EGLDisplay attributes.
    auto powerPreference = attrs.powerPreference;
    if (powerPreference == GraphicsContextGLPowerPreference::HighPerformance) {
        displayAttributes.append(EGL_POWER_PREFERENCE_ANGLE);
        displayAttributes.append(EGL_HIGH_POWER_ANGLE);
    } else if (powerPreference == GraphicsContextGLPowerPreference::LowPower) {
        displayAttributes.append(EGL_POWER_PREFERENCE_ANGLE);
        displayAttributes.append(EGL_LOW_POWER_ANGLE);
    }
#if PLATFORM(MAC)
    else if (attrs.windowGPUID) {
        ASSERT(WTF::contains(clientExtensions, "EGL_ANGLE_platform_angle_device_id"_span));
        // If the power preference is default, use the GPU the context window is on.
        // If the power preference is low power, and we know which GPU the context window is on,
        // most likely the lowest power is the GPU that drives the context window, as that GPU
        // is anyway already powered on.
        // EGL_PLATFORM_ANGLE_DEVICE_ID_*_ANGLE is the IOKit registry id on EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE.
        displayAttributes.append(EGL_PLATFORM_ANGLE_DEVICE_ID_HIGH_ANGLE);
        displayAttributes.append(static_cast<EGLAttrib>(attrs.windowGPUID >> 32));
        displayAttributes.append(EGL_PLATFORM_ANGLE_DEVICE_ID_LOW_ANGLE);
        displayAttributes.append(static_cast<EGLAttrib>(attrs.windowGPUID));
    }
#endif
    ASSERT(WTF::contains(clientExtensions, "EGL_ANGLE_feature_control"_span));
    displayAttributes.append(EGL_FEATURE_OVERRIDES_DISABLED_ANGLE);
    displayAttributes.append(reinterpret_cast<EGLAttrib>(disabledANGLEMetalFeatures));
    displayAttributes.append(EGL_FEATURE_OVERRIDES_ENABLED_ANGLE);
    displayAttributes.append(reinterpret_cast<EGLAttrib>(enabledANGLEMetalFeatures));
    displayAttributes.append(EGL_NONE);

    EGLDisplay display = EGL_GetPlatformDisplay(EGL_PLATFORM_ANGLE_ANGLE, reinterpret_cast<void*>(EGL_DEFAULT_DISPLAY), displayAttributes.span().data());
    EGLint majorVersion = 0;
    EGLint minorVersion = 0;
    if (EGL_Initialize(display, &majorVersion, &minorVersion) == EGL_FALSE) {
        LOG(WebGL, "EGLDisplay Initialization failed.");
        return EGL_NO_DISPLAY;
    }
    LOG(WebGL, "ANGLE initialised Major: %d Minor: %d", majorVersion, minorVersion);

#if ASSERT_ENABLED
    auto displayExtensions = unsafeSpan(EGL_QueryString(display, EGL_EXTENSIONS));
    ASSERT(WTF::contains(displayExtensions, "EGL_ANGLE_metal_shared_event_sync"_span));
#endif

    return display;
}

RefPtr<GraphicsContextGLCocoa> GraphicsContextGLCocoa::create(GraphicsContextGLAttributes&& attributes, ProcessIdentity&& resourceOwner)
{
    auto context = adoptRef(*new GraphicsContextGLCocoa(WTFMove(attributes), WTFMove(resourceOwner)));
    if (!context->initialize())
        return nullptr;
    return context;
}

GraphicsContextGLCocoa::GraphicsContextGLCocoa(GraphicsContextGLAttributes&& creationAttributes, ProcessIdentity&& resourceOwner)
    : GraphicsContextGLANGLE(WTFMove(creationAttributes))
    , m_resourceOwner(WTFMove(resourceOwner))
    , m_drawingBufferColorSpace(DestinationColorSpace::SRGB())
{
}

GraphicsContextGLCocoa::~GraphicsContextGLCocoa()
{
    if (makeContextCurrent())
        freeDrawingBuffers();
}

IOSurface* GraphicsContextGLCocoa::displayBufferSurface()
{
    return displayBuffer().surface();
}

std::tuple<GCGLenum, GCGLenum> GraphicsContextGLCocoa::externalImageTextureBindingPoint()
{
    if (m_drawingBufferTextureTarget == -1)
        EGL_GetConfigAttrib(platformDisplay(), platformConfig(), EGL_BIND_TO_TEXTURE_TARGET_ANGLE, &m_drawingBufferTextureTarget);

    switch (m_drawingBufferTextureTarget) {
    case EGL_TEXTURE_2D:
        return std::make_tuple(TEXTURE_2D, TEXTURE_BINDING_2D);
    case EGL_TEXTURE_RECTANGLE_ANGLE:
        return std::make_tuple(TEXTURE_RECTANGLE_ARB, TEXTURE_BINDING_RECTANGLE_ARB);
    }
    ASSERT_WITH_MESSAGE(false, "Invalid enum returned from EGL_GetConfigAttrib");
    return std::make_tuple(0, 0);
}

bool GraphicsContextGLCocoa::platformInitializeContext()
{
    GraphicsContextGLAttributes attributes = contextAttributes();
    m_isForWebGL2 = attributes.isWebGL2;
    if (!platformSupportsMetal())
        return false;

    m_displayObj = initializeEGLDisplay(attributes);
    if (!m_displayObj)
        return false;

    EGLint configAttributes[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0,
        EGL_STENCIL_SIZE, 0,
        EGL_NONE
    };
    EGLint numberConfigsReturned = 0;
    EGL_ChooseConfig(m_displayObj, configAttributes, &m_configObj, 1, &numberConfigsReturned);
    if (numberConfigsReturned != 1) {
        LOG(WebGL, "EGLConfig Initialization failed.");
        return false;
    }
    LOG(WebGL, "Got EGLConfig");

    EGL_BindAPI(EGL_OPENGL_ES_API);
    if (EGL_GetError() != EGL_SUCCESS) {
        LOG(WebGL, "Unable to bind to OPENGL_ES_API");
        return false;
    }

    Vector<EGLint> eglContextAttributes;
    if (m_isForWebGL2) {
        eglContextAttributes.append(EGL_CONTEXT_CLIENT_VERSION);
        eglContextAttributes.append(3);
    } else {
        eglContextAttributes.append(EGL_CONTEXT_CLIENT_VERSION);
        eglContextAttributes.append(2);
        // ANGLE will upgrade the context to ES3 automatically unless this is specified.
        eglContextAttributes.append(EGL_CONTEXT_OPENGL_BACKWARDS_COMPATIBLE_ANGLE);
        eglContextAttributes.append(EGL_FALSE);
    }
    eglContextAttributes.append(EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE);
    eglContextAttributes.append(EGL_TRUE);

    // WebGL requires that all resources are cleared at creation.
    eglContextAttributes.append(EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE);
    eglContextAttributes.append(EGL_TRUE);

    // WebGL doesn't allow client arrays.
    eglContextAttributes.append(EGL_CONTEXT_CLIENT_ARRAYS_ENABLED_ANGLE);
    eglContextAttributes.append(EGL_FALSE);
    // WebGL doesn't allow implicit creation of objects on bind.
    eglContextAttributes.append(EGL_CONTEXT_BIND_GENERATES_RESOURCE_CHROMIUM);
    eglContextAttributes.append(EGL_FALSE);

#if HAVE(TASK_IDENTITY_TOKEN)
    auto displayExtensions = unsafeSpan(EGL_QueryString(m_displayObj, EGL_EXTENSIONS));
    bool supportsOwnershipIdentity = WTF::contains(displayExtensions, "EGL_ANGLE_metal_create_context_ownership_identity"_span);
    if (m_resourceOwner && supportsOwnershipIdentity) {
        eglContextAttributes.append(EGL_CONTEXT_METAL_OWNERSHIP_IDENTITY_ANGLE);
        eglContextAttributes.append(m_resourceOwner.taskIdToken());
    }
#endif

    eglContextAttributes.append(EGL_NONE);

    m_contextObj = EGL_CreateContext(m_displayObj, m_configObj, EGL_NO_CONTEXT, eglContextAttributes.span().data());
    if (m_contextObj == EGL_NO_CONTEXT || !makeCurrent(m_displayObj, m_contextObj)) {
        LOG(WebGL, "EGLContext Initialization failed.");
        return false;
    }
    m_finishedMetalSharedEventListener = adoptNS([[MTLSharedEventListener alloc] init]);
    if (!m_finishedMetalSharedEventListener) {
        ASSERT_NOT_REACHED();
        return false;
    }
    m_finishedMetalSharedEvent = newSharedEvent(m_displayObj);
    if (!m_finishedMetalSharedEvent) {
        ASSERT_NOT_REACHED();
        return false;
    }
    return true;
}

bool GraphicsContextGLCocoa::platformInitializeExtensions()
{
#if PLATFORM(MAC)
    // For creating the EGL surface from an IOSurface.
    if (!enableExtensionsImpl({ "GL_EXT_texture_format_BGRA8888"_s }))
        return false;
#endif
#if ENABLE(WEBXR)
    auto attributes = contextAttributes();
    if (attributes.xrCompatible && !enableRequiredWebXRExtensionsImpl())
        return false;
#endif
    return true;
}

bool GraphicsContextGLCocoa::platformInitialize()
{
    // Compute platform-specific max internal framebuffer size.
    m_maxInternalFramebufferSize.clampToMinimumSize(IOSurface::maximumSize());
    return true;
}

GraphicsContextGLANGLE::~GraphicsContextGLANGLE()
{
    if (makeContextCurrent()) {
        GL_Disable(DEBUG_OUTPUT);
        if (m_texture)
            GL_DeleteTextures(1, &m_texture);
        if (m_multisampleColorBuffer)
            GL_DeleteRenderbuffers(1, &m_multisampleColorBuffer);
        if (m_multisampleDepthStencilBuffer)
            GL_DeleteRenderbuffers(1, &m_multisampleDepthStencilBuffer);
        if (m_multisampleFBO)
            GL_DeleteFramebuffers(1, &m_multisampleFBO);
        if (m_depthStencilBuffer)
            GL_DeleteRenderbuffers(1, &m_depthStencilBuffer);
        if (m_fbo)
            GL_DeleteFramebuffers(1, &m_fbo);
        if (m_preserveDrawingBufferTexture)
            GL_DeleteTextures(1, &m_preserveDrawingBufferTexture);
        if (m_preserveDrawingBufferFBO)
            GL_DeleteFramebuffers(1, &m_preserveDrawingBufferFBO);
    }
    if (m_contextObj) {
        for (auto* image : m_eglImages.values()) {
            bool result = EGL_DestroyImageKHR(m_displayObj, image);
            ASSERT_UNUSED(result, !!result);
        }
        for (auto* sync : m_eglSyncs.values()) {
            bool result = EGL_DestroySync(m_displayObj, sync);
            ASSERT_UNUSED(result, !!result);
        }
        makeCurrent(m_displayObj, EGL_NO_CONTEXT);
        EGL_DestroyContext(m_displayObj, m_contextObj);
    }
    ASSERT(currentContext != this);
    m_drawingBufferTextureTarget = -1;
}

bool GraphicsContextGLANGLE::makeContextCurrent()
{
    if (!m_contextObj)
        return false;
    if (currentContext == this)
        return true;
    if (!EGL_MakeCurrent(m_displayObj, EGL_NO_SURFACE, EGL_NO_SURFACE, m_contextObj))
        return false;
    currentContext = this;
    return true;
}

void GraphicsContextGLANGLE::checkGPUStatus()
{
    if (m_failNextStatusCheck) {
        LOG(WebGL, "Pretending the GPU has reset (%p). Lose the context.", this);
        m_failNextStatusCheck = false;
        forceContextLost();
        makeCurrent(m_displayObj, EGL_NO_CONTEXT);
        return;
    }
}

bool GraphicsContextGLCocoa::reshapeDrawingBuffer()
{
    ASSERT(!getInternalFramebufferSize().isEmpty());
    freeDrawingBuffers();
    return bindNextDrawingBuffer();
}

void GraphicsContextGLCocoa::setDrawingBufferColorSpace(const DestinationColorSpace& colorSpace)
{
    if (!makeContextCurrent())
        return;
    if (m_drawingBufferColorSpace == colorSpace)
        return;
    m_drawingBufferColorSpace = colorSpace;
    if (getInternalFramebufferSize().isEmpty())
        return;
    if (!reshapeDrawingBuffer())
        forceContextLost();
}

IOSurfacePbuffer& GraphicsContextGLCocoa::drawingBuffer()
{
    return surfaceBuffer(SurfaceBuffer::DrawingBuffer);
}

IOSurfacePbuffer& GraphicsContextGLCocoa::displayBuffer()
{
    return surfaceBuffer(SurfaceBuffer::DisplayBuffer);
}

IOSurfacePbuffer& GraphicsContextGLCocoa::surfaceBuffer(SurfaceBuffer buffer)
{
    if (buffer == SurfaceBuffer::DisplayBuffer)
        return m_drawingBuffers[(m_currentDrawingBufferIndex + maxReusedDrawingBuffers - 1u) % maxReusedDrawingBuffers];
    return m_drawingBuffers[m_currentDrawingBufferIndex % maxReusedDrawingBuffers];
}

bool GraphicsContextGLCocoa::bindNextDrawingBuffer()
{
    ASSERT(!getInternalFramebufferSize().isEmpty());
    if (auto& buffer = drawingBuffer())
        EGL_ReleaseTexImage(m_displayObj, buffer.pbuffer(), EGL_BACK_BUFFER);

    m_currentDrawingBufferIndex++;
    auto& buffer = drawingBuffer();

    if (buffer && (buffer.isInUse() || m_failNextStatusCheck)) {
        EGL_DestroySurface(m_displayObj, buffer.pbuffer());
        buffer = { };
    }
    if (m_failNextStatusCheck) {
        m_failNextStatusCheck = false;
        return false;
    }
    if (!buffer) {
        auto surface = IOSurface::create(nullptr, getInternalFramebufferSize(), m_drawingBufferColorSpace, IOSurface::Name::GraphicsContextGL);
        if (!surface)
            return false;
        if (m_resourceOwner)
            surface->setOwnershipIdentity(m_resourceOwner);

        const bool usingAlpha = contextAttributes().alpha;
        const auto size = getInternalFramebufferSize();
        const EGLint surfaceAttributes[] = {
            EGL_WIDTH, size.width(),
            EGL_HEIGHT, size.height(),
            EGL_IOSURFACE_PLANE_ANGLE, 0,
            EGL_TEXTURE_TARGET, EGLDrawingBufferTextureTargetForDrawingTarget(drawingBufferTextureTarget()),
            EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, usingAlpha ? GL_BGRA_EXT : GL_RGB,
            EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
            EGL_TEXTURE_TYPE_ANGLE, GL_UNSIGNED_BYTE,
            // Only has an effect on the iOS Simulator.
            EGL_IOSURFACE_USAGE_HINT_ANGLE, EGL_IOSURFACE_WRITE_HINT_ANGLE,
            EGL_NONE, EGL_NONE
        };
        EGLSurface pbuffer = EGL_CreatePbufferFromClientBuffer(m_displayObj, EGL_IOSURFACE_ANGLE, surface->surface(), m_configObj, surfaceAttributes);
        if (!pbuffer)
            return false;
        buffer = IOSurfacePbuffer { WTFMove(surface), pbuffer };
    }

    auto [textureTarget, textureBinding] = drawingBufferTextureBindingPoint();
    ScopedRestoreTextureBinding restoreBinding(textureBinding, textureTarget, textureTarget != TEXTURE_RECTANGLE_ARB);
    GL_BindTexture(textureTarget, m_texture);
    if (!EGL_BindTexImage(m_displayObj, buffer.pbuffer(), EGL_BACK_BUFFER)) {
        EGL_DestroySurface(m_displayObj, buffer.pbuffer());
        buffer = { };
        return false;
    }
    return true;
}

void GraphicsContextGLCocoa::freeDrawingBuffers()
{
    // Note: due to a ANGLE bug in ReleaseTexImage, the current context should be the context owning the pbuffer and the texture.
    if (auto& buffer = drawingBuffer())
        EGL_ReleaseTexImage(m_displayObj, buffer.pbuffer(), EGL_BACK_BUFFER);
    for (auto& buffer : m_drawingBuffers) {
        if (buffer) {
            EGL_DestroySurface(m_displayObj, buffer.pbuffer());
            buffer = { };
        }
    }
}

bool GraphicsContextGLANGLE::makeCurrent(GCGLDisplay display, GCGLContext context)
{
    currentContext = nullptr;
    return EGL_MakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context);
}

void* GraphicsContextGLCocoa::createPbufferAndAttachIOSurface(GCGLenum target, PbufferAttachmentUsage usage, GCGLenum internalFormat, GCGLsizei width, GCGLsizei height, GCGLenum type, IOSurfaceRef surface, GCGLuint plane)
{
    if (target != GraphicsContextGLANGLE::drawingBufferTextureTarget()) {
        LOG(WebGL, "Unknown texture target %d.", static_cast<int>(target));
        return nullptr;
    }

    auto usageHint = [&] () -> EGLint {
        if (usage == PbufferAttachmentUsage::Read)
            return EGL_IOSURFACE_READ_HINT_ANGLE;
        if (usage == PbufferAttachmentUsage::Write)
            return EGL_IOSURFACE_WRITE_HINT_ANGLE;
        return EGL_IOSURFACE_READ_HINT_ANGLE | EGL_IOSURFACE_WRITE_HINT_ANGLE;
    }();

    return WebCore::createPbufferAndAttachIOSurface(m_displayObj, m_configObj, target, usageHint, internalFormat, width, height, type, surface, plane);
}

void GraphicsContextGLCocoa::destroyPbufferAndDetachIOSurface(void* handle)
{
    WebCore::destroyPbufferAndDetachIOSurface(m_displayObj, handle);
}

#if ENABLE(WEBXR)
GCGLExternalImage GraphicsContextGLCocoa::createExternalImage(ExternalImageSource&& source, GCGLenum internalFormat, GCGLint layer)
{
    EGLDeviceEXT eglDevice = EGL_NO_DEVICE_EXT;
    if (!EGL_QueryDisplayAttribEXT(platformDisplay(), EGL_DEVICE_EXT, reinterpret_cast<EGLAttrib*>(&eglDevice))) {
        addError(GCGLErrorCode::InvalidOperation);
        return { };
    }

    id<MTLDevice> mtlDevice = nil;
    if (!EGL_QueryDeviceAttribEXT(eglDevice, EGL_METAL_DEVICE_ANGLE, reinterpret_cast<EGLAttrib*>(&mtlDevice))) {
        addError(GCGLErrorCode::InvalidOperation);
        return { };
    }

    RetainPtr<id<MTLTexture>> texture = WTF::switchOn(WTFMove(source),
    [&](EGLImageSourceIOSurfaceHandle&& ioSurface) -> RetainPtr<id> {
        auto surface = IOSurface::createFromSendRight(WTFMove(ioSurface.handle));
        if (!surface)
            return nullptr;

        auto size = surface->size();
        MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm_sRGB width:size.width() height:size.height() mipmapped:NO];
        [desc setUsage:MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget];

        auto tex = adoptNS([mtlDevice newTextureWithDescriptor:desc iosurface:surface->surface() plane:0]);
        return tex;
    },
    [&](EGLImageSourceMTLSharedTextureHandle&& sharedTexture) -> RetainPtr<id> {
#if PLATFORM(IOS_FAMILY_SIMULATOR)
        UNUSED_VARIABLE(sharedTexture);
        ASSERT_NOT_REACHED();
        return nullptr;
#else
        auto handle = adoptNS([[MTLSharedTextureHandle alloc] initWithMachPort:sharedTexture.handle.sendRight()]);
        if (!handle)
            return nullptr;

        if (mtlDevice != [handle device]) {
            LOG(WebGL, "MTLSharedTextureHandle does not have the same Metal device as platformDisplay.");
            return nullptr;
        }

        // Create a MTLTexture out of the MTLSharedTextureHandle.
        RetainPtr<id> texture = adoptNS([mtlDevice newSharedTextureWithHandle:handle.get()]);
        return texture;
        // FIXME: Does the texture have the correct usage mode?
#endif
    });

    if (!texture) {
        addError(GCGLErrorCode::InvalidOperation);
        return { };
    }

    // Create an EGLImage out of the MTLTexture
    Vector<EGLint, 6> attributes;
    attributes.appendList({ EGL_METAL_TEXTURE_ARRAY_SLICE_ANGLE, layer });
#if !PLATFORM(IOS_FAMILY_SIMULATOR)
    if (internalFormat)
        attributes.appendList({ EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, static_cast<EGLint>(internalFormat) });
#else
    UNUSED_VARIABLE(internalFormat);
#endif
    attributes.appendList({ EGL_NONE, EGL_NONE });
    auto eglImage = EGL_CreateImageKHR(platformDisplay(), EGL_NO_CONTEXT, EGL_METAL_TEXTURE_ANGLE, reinterpret_cast<EGLClientBuffer>(texture.get()), attributes.span().data());
    if (!eglImage) {
        addError(GCGLErrorCode::InvalidOperation);
        return { };
    }
    auto newName = ++m_nextExternalImageName;
    m_eglImages.add(newName, eglImage);
    return newName;
}

void GraphicsContextGLCocoa::bindExternalImage(GCGLenum target, GCGLExternalImage image)
{
    if (!makeContextCurrent())
        return;
    EGLImage eglImage = EGL_NO_IMAGE_KHR;
    if (image) {
        eglImage = m_eglImages.get(image);
        if (!eglImage) {
            addError(GCGLErrorCode::InvalidOperation);
            return;
        }
    }
    if (target == RENDERBUFFER)
        GL_EGLImageTargetRenderbufferStorageOES(RENDERBUFFER, eglImage);
    else
        GL_EGLImageTargetTexture2DOES(target, eglImage);
}

bool GraphicsContextGLCocoa::addFoveation(IntSize physicalSizeLeft, IntSize physicalSizeRight, IntSize screenSize, std::span<const GCGLfloat> horizontalSamplesLeft, std::span<const GCGLfloat> verticalSamplesLeft, std::span<const GCGLfloat> horizontalSamplesRight)
{
    m_rasterizationRateMap[PlatformXR::Layout::Shared] = newRasterizationRateMap(m_displayObj, physicalSizeLeft, physicalSizeRight, screenSize, horizontalSamplesLeft, verticalSamplesLeft, horizontalSamplesRight);
    return !!m_rasterizationRateMap[PlatformXR::Layout::Shared];
}

void GraphicsContextGLCocoa::enableFoveation(PlatformGLObject rbo)
{
#if !PLATFORM(IOS_FAMILY_SIMULATOR)
    if (!makeContextCurrent())
        return;
    GL_BindMetalRasterizationRateMapANGLE(rbo, m_rasterizationRateMap[PlatformXR::Layout::Shared].get());
    GL_Enable(GL::VARIABLE_RASTERIZATION_RATE_ANGLE);
#else
    UNUSED_PARAM(rbo);
#endif
}

void GraphicsContextGLCocoa::disableFoveation()
{
#if !PLATFORM(IOS_FAMILY_SIMULATOR)
    if (!makeContextCurrent())
        return;
    GL_Disable(GL::VARIABLE_RASTERIZATION_RATE_ANGLE);
    GL_BindMetalRasterizationRateMapANGLE(0, nullptr);
#endif
}

#if ENABLE(WEBXR)
void GraphicsContextGLCocoa::framebufferDiscard(GCGLenum target, std::span<const GCGLenum> attachments)
{
    if (!makeContextCurrent())
        return;
    GL_DiscardFramebufferEXT(target, attachments.size(), attachments.data());
}

void GraphicsContextGLCocoa::framebufferResolveRenderbuffer(GCGLenum target, GCGLenum attachment, GCGLenum renderbuffertarget, PlatformGLObject renderbuffer)
{
    if (!makeContextCurrent())
        return;
    GL_FramebufferResolveRenderbufferWEBKIT(target, attachment, renderbuffertarget, renderbuffer);
}
#endif

RetainPtr<id> GraphicsContextGLCocoa::newSharedEventWithMachPort(mach_port_t sharedEventSendRight)
{
    return WebCore::newSharedEventWithMachPort(m_displayObj, sharedEventSendRight);
}

GCGLExternalSync GraphicsContextGLCocoa::createExternalSync(ExternalSyncSource&& syncEvent)
{
    auto [syncEventHandle, signalValue] = WTFMove(syncEvent);
    auto sharedEvent = newSharedEventWithMachPort(syncEventHandle.sendRight());
    if (!sharedEvent) {
        LOG(WebGL, "Unable to create a MTLSharedEvent from the syncEvent.");
        addError(GCGLErrorCode::InvalidOperation);
        return { };
    }
    auto* eglSync = createMetalSharedEventEGLSync(sharedEvent.get(), signalValue);
    if (!eglSync) {
        addError(GCGLErrorCode::InvalidOperation);
        return { };
    }
    auto newName = ++m_nextExternalSyncName;
    m_eglSyncs.add(newName, eglSync);
    return newName;

}

bool GraphicsContextGLCocoa::enableRequiredWebXRExtensions()
{
    if (!makeContextCurrent())
        return false;
    if (enableRequiredWebXRExtensionsImpl())
        return true;
    return false;
}

bool GraphicsContextGLCocoa::enableRequiredWebXRExtensionsImpl()
{
    return enableExtensionsImpl({
        "GL_ANGLE_framebuffer_multisample"_s,
        "GL_ANGLE_framebuffer_blit"_s,
        "GL_EXT_discard_framebuffer"_s,
        "GL_EXT_sRGB"_s,
        "GL_OES_EGL_image"_s,
        "GL_OES_rgb8_rgba8"_s,
#if !PLATFORM(IOS_FAMILY_SIMULATOR)
        "GL_ANGLE_variable_rasterization_rate_metal"_s,
#endif
#if PLATFORM(VISION)
        "GL_WEBKIT_explicit_resolve_target"_s,
#endif
        "GL_NV_framebuffer_blit"_s,
    });
}
#endif

void* GraphicsContextGLCocoa::createMetalSharedEventEGLSync(id sharedEvent, uint64_t signalValue)
{
    static_assert(sizeof(EGLAttrib) == sizeof(void*), "EGLAttrib not pointer-sized!");
    auto signalValueLo = static_cast<EGLAttrib>(signalValue);
    auto signalValueHi = static_cast<EGLAttrib>(signalValue >> 32);

    // FIXME: How do we check for available extensions?
    auto display = platformDisplay();
    const EGLAttrib syncAttributes[] = {
        EGL_SYNC_METAL_SHARED_EVENT_OBJECT_ANGLE, reinterpret_cast<EGLAttrib>(sharedEvent),
        EGL_SYNC_METAL_SHARED_EVENT_SIGNAL_VALUE_LO_ANGLE, signalValueLo,
        EGL_SYNC_METAL_SHARED_EVENT_SIGNAL_VALUE_HI_ANGLE, signalValueHi,
        EGL_NONE
    };
    return EGL_CreateSync(display, EGL_SYNC_METAL_SHARED_EVENT_ANGLE, syncAttributes);
}

void GraphicsContextGLCocoa::waitUntilWorkScheduled()
{
    EGL_WaitUntilWorkScheduledANGLE(platformDisplay());
}

void GraphicsContextGLCocoa::prepareForDisplay()
{
    prepareForDisplayWithFinishedSignal([] { });
}

void GraphicsContextGLCocoa::prepareForDisplayWithFinishedSignal(Function<void()> finishedSignal)
{
    if (!makeContextCurrent()) {
        finishedSignal();
        return;
    }
    if (!drawingBuffer()) {
        finishedSignal();
        return;
    }
    prepareTexture();
    // The fence inserted by this will be scheduled because next BindTexImage will wait until scheduled.
    insertFinishedSignalOrInvoke(WTFMove(finishedSignal));
    bool success = bindNextDrawingBuffer();
    waitUntilWorkScheduled();
    if (!success)
        forceContextLost();
}


#if ENABLE(VIDEO)
GraphicsContextGLCV* GraphicsContextGLCocoa::cvContext()
{
    if (!m_cv)
        m_cv = GraphicsContextGLCVCocoa::create(*this);
    return m_cv.get();
}
#endif

RefPtr<PixelBuffer> GraphicsContextGLCocoa::readCompositedResults()
{
    auto& buffer = displayBuffer();
    if (!buffer || buffer.size() != getInternalFramebufferSize())
        return nullptr;
    // Note: We are using GL to read the IOSurface. At the time of writing, there are no convinient
    // functions to convert the IOSurface pixel data to ImageData. The image data ends up being
    // drawn to a ImageBuffer, but at the time there's no functions to construct a NativeImage
    // out of an IOSurface in such a way that drawing the NativeImage would be guaranteed leave
    // the IOSurface be unrefenced after the draw call finishes.
    ScopedTexture texture;
    auto [textureTarget, textureBinding] = drawingBufferTextureBindingPoint();
    ScopedRestoreTextureBinding restoreBinding(textureBinding, textureTarget, textureTarget != TEXTURE_RECTANGLE_ARB);
    GL_BindTexture(textureTarget, texture);
    if (!EGL_BindTexImage(m_displayObj, buffer.pbuffer(), EGL_BACK_BUFFER))
        return nullptr;
    GL_TexParameteri(textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ScopedFramebuffer fbo;
    ScopedRestoreReadFramebufferBinding fboBinding(m_isForWebGL2, m_state.boundReadFBO, fbo);
    GL_FramebufferTexture2D(fboBinding.framebufferTarget(), GL_COLOR_ATTACHMENT0, textureTarget, texture, 0);
    ASSERT(GL_CheckFramebufferStatus(fboBinding.framebufferTarget()) == GL_FRAMEBUFFER_COMPLETE);

    auto result = readPixelsForPaintResults();

    EGLBoolean releaseOk = EGL_ReleaseTexImage(m_displayObj, buffer.pbuffer(), EGL_BACK_BUFFER);
    ASSERT_UNUSED(releaseOk, releaseOk);
    return result;
}

#if ENABLE(MEDIA_STREAM) || ENABLE(WEB_CODECS)

RefPtr<VideoFrame> GraphicsContextGLCocoa::surfaceBufferToVideoFrame(SurfaceBuffer buffer)
{
    if (buffer == SurfaceBuffer::DrawingBuffer) {
        if (!makeContextCurrent())
            return nullptr;
        prepareTexture();
        waitUntilWorkScheduled();
    }

    auto& source = surfaceBuffer(buffer);
    if (!source || source.size() != getInternalFramebufferSize())
        return nullptr;
    // We will mirror and rotate the buffer explicitly. Thus the source being used is always a new one.
    auto pixelBuffer = createCVPixelBuffer(source.surface()->protectedSurface().get());
    if (!pixelBuffer)
        return nullptr;
    // Mirror and rotate the pixel buffer explicitly, as WebRTC encoders cannot mirror.
    auto size = getInternalFramebufferSize();
    if (!m_mediaSampleRotationSession || m_mediaSampleRotationSessionSize != size)
        m_mediaSampleRotationSession = makeUnique<ImageRotationSessionVT>(ImageRotationSessionVT::RotationProperties { true, false, 180 }, size, ImageRotationSessionVT::IsCGImageCompatible::No);
    auto mediaSamplePixelBuffer = m_mediaSampleRotationSession->rotate(pixelBuffer->get());
    if (!mediaSamplePixelBuffer)
        return nullptr;
    if (m_resourceOwner)
        setOwnershipIdentityForCVPixelBuffer(mediaSamplePixelBuffer.get(), m_resourceOwner);
    return VideoFrameCV::create({ }, false, VideoFrame::Rotation::None, WTFMove(mediaSamplePixelBuffer));
}
#endif

void GraphicsContextGLANGLE::platformReleaseThreadResources()
{
    currentContext = nullptr;
}

RefPtr<GraphicsLayerContentsDisplayDelegate> GraphicsContextGLCocoa::layerContentsDisplayDelegate()
{
    return nullptr;
}

void GraphicsContextGLCocoa::invalidateKnownTextureContent(GCGLuint texture)
{
    if (m_cv)
        m_cv->invalidateKnownTextureContent(texture);
}

RefPtr<NativeImage> GraphicsContextGLCocoa::copyNativeImageYFlipped(SurfaceBuffer buffer)
{
    RetainPtr<CGContextRef> cgContext;
    RefPtr<NativeImage> image;
    if (contextAttributes().premultipliedAlpha) {
        // Use the IOSurface backed image directly
        if (buffer == SurfaceBuffer::DrawingBuffer) {
            if (!makeContextCurrent())
                return nullptr;
            prepareTexture();
            waitUntilWorkScheduled();
        }
        IOSurfacePbuffer& source = surfaceBuffer(buffer);
        if (!source || source.size() != getInternalFramebufferSize())
            return nullptr;
        image = source.copyNativeImage();
    } else {
        if (!makeContextCurrent())
            return nullptr;
        // Since IOSurface-backed images only support premultiplied alpha, read
        // the image into a PixelBuffer which can be used to create a CGImage
        // that does the conversion.
        //
        // FIXME: Can the IOSurface be read into a buffer to avoid the read back via GL?
        RefPtr<PixelBuffer> pixelBuffer;
        if (buffer == SurfaceBuffer::DrawingBuffer)
            pixelBuffer = drawingBufferToPixelBuffer(FlipY::No);
        else
            pixelBuffer = readCompositedResults();
        if (!pixelBuffer)
            return nullptr;
        image = createNativeImageFromPixelBuffer(contextAttributes(), pixelBuffer.releaseNonNull());
    }
    if (!image)
        return nullptr;

    CGImageSetCachingFlags(image->platformImage().get(), kCGImageCachingTransient);
    return image;
}

void GraphicsContextGLCocoa::insertFinishedSignalOrInvoke(Function<void()> signal)
{
    static std::atomic<uint64_t> nextSignalValue;
    uint64_t signalValue = ++nextSignalValue;
    RetainPtr<id<MTLSharedEvent>> event = m_finishedMetalSharedEvent.get();
    // The block below has to be a real compiler generated block instead of BlockPtr due to a Metal bug. rdar://108035473
    __block Function<void()> blockSignal = WTFMove(signal);
    [event notifyListener:m_finishedMetalSharedEventListener.get() atValue:signalValue block:^(id<MTLSharedEvent>, uint64_t) {
        blockSignal();
    }];
    auto* eglSync = createMetalSharedEventEGLSync(event.get(), signalValue);
    if (!eglSync) [[unlikely]] {
        event.get().signaledValue = signalValue;
        ASSERT_NOT_REACHED();
        return;
    }
    bool result = EGL_DestroySync(platformDisplay(), eglSync);
    ASSERT_UNUSED(result, result);
}

#if ENABLE(VIDEO)
bool GraphicsContextGLCocoa::copyTextureFromVideoFrame(VideoFrame& videoFrame, PlatformGLObject texture, uint32_t target, int32_t level, uint32_t internalFormat, uint32_t format, uint32_t type, bool premultiplyAlpha, bool flipY)
{
    UNUSED_VARIABLE(premultiplyAlpha);
    ASSERT_UNUSED(target, target == WebCore::GraphicsContextGL::TEXTURE_2D);
    auto* contextCV = cvContext();
    if (!contextCV) {
        ASSERT(contextCV);
        return false;
    }
    RefPtr videoFrameCV = videoFrame.asVideoFrameCV();
    if (!videoFrameCV) {
        ASSERT_NOT_REACHED(); // Programming error.
        return false;
    }
    return contextCV->copyVideoSampleToTexture(*videoFrameCV, texture, level, internalFormat, format, type, WebCore::GraphicsContextGL::FlipY(flipY));
}
#endif

void GraphicsContextGLCocoa::prepareForDrawingBufferWrite()
{
    auto& buffer = drawingBuffer();
    buffer.prepareForWrite();
}

}

#endif // ENABLE(WEBGL)
