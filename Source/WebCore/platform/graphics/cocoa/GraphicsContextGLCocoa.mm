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
#import "RuntimeApplicationChecks.h"
#import <CoreGraphics/CGBitmapContext.h>
#import <Metal/Metal.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/MetalSPI.h>
#import <wtf/BlockObjCExceptions.h>
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

// In isCurrentContextPredictable() == true case this variable is accessed in single-threaded manner.
// In isCurrentContextPredictable() == false case this variable is accessed from multiple threads but always sequentially
// and it always contains nullptr and nullptr is always written to it.
static GraphicsContextGLANGLE* currentContext;

static const char* const disabledANGLEMetalFeatures[] = {
    "enableInMemoryMtlLibraryCache", // This would leak all program binary objects.
    "alwaysPreferStagedTextureUploads", // This would timeout tests due to excess staging buffer allocations and fail tests on MacPro.
    nullptr
};

static bool isCurrentContextPredictable()
{
    static bool value = isInWebProcess() || isInGPUProcess();
    return value;
}

#if ASSERT_ENABLED
// Returns true if we have volatile context extension for the particular API or
// if the particular API is not used.
static bool checkVolatileContextSupportIfDeviceExists(EGLDisplay display, const char* deviceContextVolatileExtension,
    const char* deviceContextExtension, EGLint deviceContextType)
{
    const char *clientExtensions = EGL_QueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (clientExtensions && strstr(clientExtensions, deviceContextVolatileExtension))
        return true;
    EGLDeviceEXT device = EGL_NO_DEVICE_EXT;
    if (!EGL_QueryDisplayAttribEXT(display, EGL_DEVICE_EXT, reinterpret_cast<EGLAttrib*>(&device)))
        return true;
    if (device == EGL_NO_DEVICE_EXT)
        return true;
    const char* deviceExtensions = EGL_QueryDeviceStringEXT(device, EGL_EXTENSIONS);
    if (!deviceExtensions || !strstr(deviceExtensions, deviceContextExtension))
        return true;
    void* deviceContext = nullptr;
    if (!EGL_QueryDeviceAttribEXT(device, deviceContextType, reinterpret_cast<EGLAttrib*>(&deviceContext)))
        return true;
    return !deviceContext;
}
#endif

static bool platformSupportsMetal()
{
    auto device = adoptNS(MTLCreateSystemDefaultDevice());

    if (device) {
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
        // Old Macs, such as MacBookPro11,4 cannot use WebGL via Metal.
        // This check can be removed once they are no longer supported.
        return [device supportsFamily:MTLGPUFamilyMac2];
#else
        return true;
#endif
    }

    return false;
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
    const char* clientExtensions = EGL_QueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    ASSERT(clientExtensions);
#endif

    Vector<EGLAttrib> displayAttributes;

    // FIXME: This should come in from the GraphicsContextGLAttributes.
    bool shouldInitializeWithVolatileContextSupport = !isCurrentContextPredictable();
    if (shouldInitializeWithVolatileContextSupport) {
        // For WK1 type APIs we need to set "volatile platform context" for specific
        // APIs, since client code will be able to override the thread-global context
        // that ANGLE expects.
        displayAttributes.append(EGL_PLATFORM_ANGLE_DEVICE_CONTEXT_VOLATILE_EAGL_ANGLE);
        displayAttributes.append(EGL_TRUE);
        displayAttributes.append(EGL_PLATFORM_ANGLE_DEVICE_CONTEXT_VOLATILE_CGL_ANGLE);
        displayAttributes.append(EGL_TRUE);
    }

    LOG(WebGL, "Attempting to use ANGLE's %s backend.", attrs.useMetal ? "Metal" : "OpenGL");
    if (attrs.useMetal) {
        displayAttributes.append(EGL_PLATFORM_ANGLE_TYPE_ANGLE);
        displayAttributes.append(EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE);
        // These properties are defined for EGL_ANGLE_power_preference as EGLContext attributes,
        // but Metal backend uses EGLDisplay attributes.
        auto powerPreference = attrs.effectivePowerPreference();
        if (powerPreference == GraphicsContextGLAttributes::PowerPreference::HighPerformance) {
            displayAttributes.append(EGL_POWER_PREFERENCE_ANGLE);
            displayAttributes.append(EGL_HIGH_POWER_ANGLE);
        } else {
            if (powerPreference == GraphicsContextGLAttributes::PowerPreference::LowPower) {
                displayAttributes.append(EGL_POWER_PREFERENCE_ANGLE);
                displayAttributes.append(EGL_LOW_POWER_ANGLE);
            }
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
            ASSERT(strstr(clientExtensions, "EGL_ANGLE_platform_angle_device_id"));
            // If the power preference is default, use the GPU the context window is on.
            // If the power preference is low power, and we know which GPU the context window is on,
            // most likely the lowest power is the GPU that drives the context window, as that GPU
            // is anyway already powered on.
            if (attrs.windowGPUID) {
                // EGL_PLATFORM_ANGLE_DEVICE_ID_*_ANGLE is the IOKit registry id on EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE.
                displayAttributes.append(EGL_PLATFORM_ANGLE_DEVICE_ID_HIGH_ANGLE);
                displayAttributes.append(static_cast<EGLAttrib>(attrs.windowGPUID >> 32));
                displayAttributes.append(EGL_PLATFORM_ANGLE_DEVICE_ID_LOW_ANGLE);
                displayAttributes.append(static_cast<EGLAttrib>(attrs.windowGPUID));
            }
#endif
            ASSERT(strstr(clientExtensions, "EGL_ANGLE_feature_control"));
            displayAttributes.append(EGL_FEATURE_OVERRIDES_DISABLED_ANGLE);
            displayAttributes.append(reinterpret_cast<EGLAttrib>(disabledANGLEMetalFeatures));
        }
    }
    displayAttributes.append(EGL_NONE);

    EGLDisplay display = EGL_GetPlatformDisplay(EGL_PLATFORM_ANGLE_ANGLE, reinterpret_cast<void*>(EGL_DEFAULT_DISPLAY), displayAttributes.data());
    EGLint majorVersion = 0;
    EGLint minorVersion = 0;
    if (EGL_Initialize(display, &majorVersion, &minorVersion) == EGL_FALSE) {
        LOG(WebGL, "EGLDisplay Initialization failed.");
        return EGL_NO_DISPLAY;
    }
    LOG(WebGL, "ANGLE initialised Major: %d Minor: %d", majorVersion, minorVersion);
    if (shouldInitializeWithVolatileContextSupport) {
        ASSERT(checkVolatileContextSupportIfDeviceExists(display, "EGL_ANGLE_platform_device_context_volatile_eagl", "EGL_ANGLE_device_eagl", EGL_EAGL_CONTEXT_ANGLE));
        ASSERT(checkVolatileContextSupportIfDeviceExists(display, "EGL_ANGLE_platform_device_context_volatile_cgl", "EGL_ANGLE_device_cgl", EGL_CGL_CONTEXT_ANGLE));
    }

#if ASSERT_ENABLED && ENABLE(WEBXR)
    const char* displayExtensions = EGL_QueryString(display, EGL_EXTENSIONS);
    ASSERT(strstr(displayExtensions, "EGL_ANGLE_metal_shared_event_sync"));
#endif

    return display;
}

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
static bool needsEAGLOnMac()
{
#if PLATFORM(MACCATALYST) && CPU(ARM64)
    return true;
#else
    return false;
#endif
}
#endif

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
    freeDrawingBuffers();
}

IOSurface* GraphicsContextGLCocoa::displayBufferSurface()
{
    return displayBuffer().surface.get();
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
    m_isForWebGL2 = attributes.webGLVersion == GraphicsContextGLWebGLVersion::WebGL2;
    if (attributes.useMetal && !platformSupportsMetal()) {
        attributes.useMetal = false;
        setContextAttributes(attributes);
    }

#if ENABLE(WEBXR)
    if (attributes.xrCompatible) {
        // FIXME: It's almost certain that any connected headset will require the high-power GPU,
        // which is the same GPU we need this context to use. However, this is not guaranteed, and
        // there is also the chance that there are multiple GPUs. Given that you can request the
        // GraphicsContextGL before initializing the WebXR session, we'll need some way to
        // migrate the context to the appropriate GPU when the code here does not work.
        LOG(WebGL, "WebXR compatible context requested. This will also trigger a request for the high-power GPU.");
        attributes.forceRequestForHighPerformanceGPU = true;
        setContextAttributes(attributes);
    }
#endif

    m_displayObj = initializeEGLDisplay(attributes);
    if (!m_displayObj)
        return false;

#if PLATFORM(MAC)
    if (!attributes.useMetal) {
        // For OpenGL, EGL_ANGLE_power_preference is used. The context is initialized with the
        // default, low-power device. For high-performance contexts, we request the high-performance
        // GPU in setContextVisibility. When the request is fullfilled by the system, we get the
        // display reconfiguration callback. Upon this, we update the CGL contexts inside ANGLE.
        const char *displayExtensions = EGL_QueryString(m_displayObj, EGL_EXTENSIONS);
        bool supportsPowerPreference = strstr(displayExtensions, "EGL_ANGLE_power_preference");
        if (!supportsPowerPreference) {
            attributes.forceRequestForHighPerformanceGPU = false;
            if (attributes.powerPreference == GraphicsContextGLPowerPreference::HighPerformance) {
                attributes.powerPreference = GraphicsContextGLPowerPreference::Default;
            }
            setContextAttributes(attributes);
        }
    }
#endif

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
    auto displayExtensions = EGL_QueryString(m_displayObj, EGL_EXTENSIONS);
    bool supportsOwnershipIdentity = strstr(displayExtensions, "EGL_ANGLE_metal_create_context_ownership_identity");
    if (attributes.useMetal && m_resourceOwner && supportsOwnershipIdentity) {
        eglContextAttributes.append(EGL_CONTEXT_METAL_OWNERSHIP_IDENTITY_ANGLE);
        eglContextAttributes.append(m_resourceOwner.taskIdToken());
    }
#endif

    eglContextAttributes.append(EGL_NONE);

    m_contextObj = EGL_CreateContext(m_displayObj, m_configObj, EGL_NO_CONTEXT, eglContextAttributes.data());
    if (m_contextObj == EGL_NO_CONTEXT || !makeCurrent(m_displayObj, m_contextObj)) {
        LOG(WebGL, "EGLContext Initialization failed.");
        return false;
    }
    if (attributes.useMetal) {
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
    }
    return true;
}

bool GraphicsContextGLCocoa::platformInitialize()
{
    auto attributes = contextAttributes();
    if (m_isForWebGL2)
        GL_Enable(GraphicsContextGL::PRIMITIVE_RESTART_FIXED_INDEX);

    Vector<ASCIILiteral, 4> requiredExtensions;
    if (m_isForWebGL2) {
        // For WebGL 2.0 occlusion queries to work.
        requiredExtensions.append("GL_EXT_occlusion_query_boolean"_s);
    }
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    if (!needsEAGLOnMac()) {
        // For IOSurface-backed textures.
        if (!attributes.useMetal)
            requiredExtensions.append("GL_ANGLE_texture_rectangle"_s);
        // For creating the EGL surface from an IOSurface.
        requiredExtensions.append("GL_EXT_texture_format_BGRA8888"_s);
    }
#endif // PLATFORM(MAC) || PLATFORM(MACCATALYST)
#if ENABLE(WEBXR)
    if (attributes.xrCompatible && !enableRequiredWebXRExtensions())
        return false;
#endif
    if (m_isForWebGL2)
        requiredExtensions.append("GL_ANGLE_framebuffer_multisample"_s);

    for (auto& extension : requiredExtensions) {
        if (!supportsExtension(extension)) {
            LOG(WebGL, "Missing required extension. %s", extension.characters());
            return false;
        }
        ensureExtensionEnabled(extension);
    }
    if (attributes.useMetal) {
        // GraphicsContextGLANGLE uses sync objects to throttle display on Metal implementations.
        // OpenGL sync objects are not signaling upon completion on Catalina-era drivers, so
        // OpenGL cannot use this method of throttling. OpenGL drivers typically implement
        // some sort of internal throttling.
        if (supportsExtension("GL_ARB_sync"_s))
            ensureExtensionEnabled("GL_ARB_sync"_s);
    }
    validateAttributes();
    attributes = contextAttributes(); // They may have changed during validation.

    // Create the texture that will be used for the framebuffer.
    GLenum textureTarget = drawingBufferTextureTarget();

    GL_GenTextures(1, &m_texture);
    GL_BindTexture(textureTarget, m_texture);
    GL_TexParameteri(textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GL_TexParameteri(textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GL_TexParameteri(textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL_TexParameteri(textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GL_BindTexture(textureTarget, 0);

    GL_GenFramebuffers(1, &m_fbo);
    GL_BindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    m_state.boundDrawFBO = m_state.boundReadFBO = m_fbo;

    if (!attributes.antialias && (attributes.stencil || attributes.depth))
        GL_GenRenderbuffers(1, &m_depthStencilBuffer);

    // If necessary, create another framebuffer for the multisample results.
    if (attributes.antialias) {
        GL_GenFramebuffers(1, &m_multisampleFBO);
        GL_BindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
        m_state.boundDrawFBO = m_state.boundReadFBO = m_multisampleFBO;
        GL_GenRenderbuffers(1, &m_multisampleColorBuffer);
        if (attributes.stencil || attributes.depth)
            GL_GenRenderbuffers(1, &m_multisampleDepthStencilBuffer);
    } else if (attributes.preserveDrawingBuffer) {
        // If necessary, create another texture to handle preserveDrawingBuffer:true without
        // antialiasing.
        GL_GenTextures(1, &m_preserveDrawingBufferTexture);
        GL_BindTexture(GL_TEXTURE_2D, m_preserveDrawingBufferTexture);
        GL_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        GL_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        GL_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        GL_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        GL_BindTexture(GL_TEXTURE_2D, 0);
        // Create an FBO with which to perform BlitFramebuffer from one texture to the other.
        GL_GenFramebuffers(1, &m_preserveDrawingBufferFBO);
    }

    GL_ClearColor(0, 0, 0, 0);

#if PLATFORM(MAC)
    if (!attributes.useMetal && attributes.effectivePowerPreference() == GraphicsContextGLPowerPreference::HighPerformance)
        m_switchesGPUOnDisplayReconfiguration = true;
#endif
    return GraphicsContextGLANGLE::platformInitialize();
}

GraphicsContextGLANGLE::~GraphicsContextGLANGLE()
{
    if (makeContextCurrent()) {
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
    // Calling MakeCurrent is important to set volatile platform context. See initializeEGLDisplay().
    if (!EGL_MakeCurrent(m_displayObj, EGL_NO_SURFACE, EGL_NO_SURFACE, m_contextObj))
        return false;
    if (isCurrentContextPredictable())
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

void GraphicsContextGLCocoa::setContextVisibility(bool isVisible)
{
#if PLATFORM(MAC)
    if (!m_switchesGPUOnDisplayReconfiguration)
        return;
    if (isVisible)
        m_highPerformanceGPURequest = ScopedHighPerformanceGPURequest::acquire();
    else
        m_highPerformanceGPURequest = { };
#else
    UNUSED_PARAM(isVisible);
#endif
}

#if PLATFORM(MAC)
void GraphicsContextGLCocoa::updateContextOnDisplayReconfiguration()
{
    if (m_switchesGPUOnDisplayReconfiguration)
        EGL_HandleGPUSwitchANGLE(m_displayObj);
}
#endif

bool GraphicsContextGLCocoa::reshapeDrawingBuffer()
{
    ASSERT(!getInternalFramebufferSize().isEmpty());
    freeDrawingBuffers();
    return bindNextDrawingBuffer();
}

void GraphicsContextGLCocoa::setDrawingBufferColorSpace(const DestinationColorSpace& colorSpace)
{
    if (m_drawingBufferColorSpace == colorSpace)
        return;
    m_drawingBufferColorSpace = colorSpace;
    if (getInternalFramebufferSize().isEmpty())
        return;
    if (!reshapeDrawingBuffer())
        forceContextLost();
}

GraphicsContextGLCocoa::IOSurfacePbuffer& GraphicsContextGLCocoa::drawingBuffer()
{
    return surfaceBuffer(SurfaceBuffer::DrawingBuffer);
}

GraphicsContextGLCocoa::IOSurfacePbuffer& GraphicsContextGLCocoa::displayBuffer()
{
    return surfaceBuffer(SurfaceBuffer::DisplayBuffer);
}

GraphicsContextGLCocoa::IOSurfacePbuffer& GraphicsContextGLCocoa::surfaceBuffer(SurfaceBuffer buffer)
{
    if (buffer == SurfaceBuffer::DisplayBuffer)
        return m_drawingBuffers[(m_currentDrawingBufferIndex + maxReusedDrawingBuffers - 1u) % maxReusedDrawingBuffers];
    return m_drawingBuffers[m_currentDrawingBufferIndex % maxReusedDrawingBuffers];
}

bool GraphicsContextGLCocoa::bindNextDrawingBuffer()
{
    ASSERT(!getInternalFramebufferSize().isEmpty());
    if (drawingBuffer())
        EGL_ReleaseTexImage(m_displayObj, drawingBuffer().pbuffer, EGL_BACK_BUFFER);

    m_currentDrawingBufferIndex++;
    auto& buffer = drawingBuffer();

    if (buffer && (buffer.surface->isInUse() || m_failNextStatusCheck)) {
        EGL_DestroySurface(m_displayObj, buffer.pbuffer);
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
        buffer = { WTFMove(surface), pbuffer };
    }

    auto [textureTarget, textureBinding] = drawingBufferTextureBindingPoint();
    ScopedRestoreTextureBinding restoreBinding(textureBinding, textureTarget, textureTarget != TEXTURE_RECTANGLE_ARB);
    GL_BindTexture(textureTarget, m_texture);
    if (!EGL_BindTexImage(m_displayObj, buffer.pbuffer, EGL_BACK_BUFFER)) {
        EGL_DestroySurface(m_displayObj, buffer.pbuffer);
        buffer = { };
        return false;
    }
    return true;
}

void GraphicsContextGLCocoa::freeDrawingBuffers()
{
    if (drawingBuffer())
        EGL_ReleaseTexImage(m_displayObj, drawingBuffer().pbuffer, EGL_BACK_BUFFER);
    for (auto& buffer : m_drawingBuffers) {
        if (buffer) {
            EGL_DestroySurface(m_displayObj, buffer.pbuffer);
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

std::optional<GraphicsContextGL::EGLImageAttachResult> GraphicsContextGLCocoa::createAndBindEGLImage(GCGLenum target, EGLImageSource source)
{
    EGLDeviceEXT eglDevice = EGL_NO_DEVICE_EXT;
    if (!EGL_QueryDisplayAttribEXT(platformDisplay(), EGL_DEVICE_EXT, reinterpret_cast<EGLAttrib*>(&eglDevice)))
        return std::nullopt;

    id<MTLDevice> mtlDevice = nil;
    if (!EGL_QueryDeviceAttribEXT(eglDevice, EGL_METAL_DEVICE_ANGLE, reinterpret_cast<EGLAttrib*>(&mtlDevice)))
        return std::nullopt;

    RetainPtr<id<MTLTexture>> texture = WTF::switchOn(WTFMove(source),
    [&](EGLImageSourceIOSurfaceHandle&& ioSurface) -> RetainPtr<id> {
        auto surface = IOSurface::createFromSendRight(WTFMove(ioSurface.handle));
        if (!surface)
            return { };

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
        return { };
#else
        auto handle = adoptNS([[MTLSharedTextureHandle alloc] initWithMachPort:sharedTexture.handle.sendRight()]);
        if (!handle)
            return { };

        if (mtlDevice != [handle device]) {
            LOG(WebGL, "MTLSharedTextureHandle does not have the same Metal device as platformDisplay.");
            return { };
        }

        // Create a MTLTexture out of the MTLSharedTextureHandle.
        RetainPtr<id> texture = adoptNS([mtlDevice newSharedTextureWithHandle:handle.get()]);
        return texture;
        // FIXME: Does the texture have the correct usage mode?
#endif
    });

    if (!texture)
        return std::nullopt;

    // Create an EGLImage out of the MTLTexture
    constexpr EGLint emptyAttributes[] = { EGL_NONE };
    auto eglImage = EGL_CreateImageKHR(platformDisplay(), EGL_NO_CONTEXT, EGL_METAL_TEXTURE_ANGLE, reinterpret_cast<EGLClientBuffer>(texture.get()), emptyAttributes);
    if (!eglImage)
        return std::nullopt;

    // Tell the currently bound texture to use the EGLImage.
    if (target == RENDERBUFFER)
        GL_EGLImageTargetRenderbufferStorageOES(RENDERBUFFER, eglImage);
    else
        GL_EGLImageTargetTexture2DOES(target, eglImage);

    GCGLuint textureWidth = [texture width];
    GCGLuint textureHeight = [texture height];

    return std::make_tuple(eglImage, IntSize(textureWidth, textureHeight));
}

RetainPtr<id> GraphicsContextGLCocoa::newSharedEventWithMachPort(mach_port_t sharedEventSendRight)
{
    return WebCore::newSharedEventWithMachPort(m_displayObj, sharedEventSendRight);
}

GCEGLSync GraphicsContextGLCocoa::createEGLSync(ExternalEGLSyncEvent syncEvent)
{
    auto [syncEventHandle, signalValue] = WTFMove(syncEvent);
    auto sharedEvent = newSharedEventWithMachPort(syncEventHandle.sendRight());
    if (!sharedEvent) {
        LOG(WebGL, "Unable to create a MTLSharedEvent from the syncEvent in createEGLSync.");
        return nullptr;
    }

    return createEGLSync(sharedEvent.get(), signalValue);
}

bool GraphicsContextGLCocoa::enableRequiredWebXRExtensions()
{
#if ENABLE(WEBXR)
    String requiredExtensions[] = {
        "GL_ANGLE_framebuffer_multisample"_str,
        "GL_ANGLE_framebuffer_blit"_str,
        "GL_EXT_sRGB"_str,
        "GL_OES_EGL_image"_str,
        "GL_OES_rgb8_rgba8"_str
    };

    for (const auto& ext : requiredExtensions) {
        if (!supportsExtension(ext))
            return false;
        ensureExtensionEnabled(ext);
    }
#endif
    return true;
}

GCEGLSync GraphicsContextGLCocoa::createEGLSync(id sharedEvent, uint64_t signalValue)
{
    COMPILE_ASSERT(sizeof(EGLAttrib) == sizeof(void*), "EGLAttrib not pointer-sized!");
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
    if (contextAttributes().useMetal)
        EGL_WaitUntilWorkScheduledANGLE(platformDisplay());
    else
        GL_Flush();
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
    if (m_layerComposited) {
        // FIXME: It makes no sense that this happens. Tracking this state should be moved to caller, WebGLRendenderingContextBase.
        // https://bugs.webkit.org/show_bug.cgi?id=219342
        insertFinishedSignalOrInvoke(WTFMove(finishedSignal));
        waitUntilWorkScheduled();
        return;
    }
    if (!drawingBuffer())
        return;
    prepareTexture();
    // The fence inserted by this will be scheduled because next BindTexImage will wait until scheduled.
    insertFinishedSignalOrInvoke(WTFMove(finishedSignal));
    if (!bindNextDrawingBuffer()) {
        // If the allocation failed, BindTexImage did not run. The fence must be scheduled.
        waitUntilWorkScheduled();
        forceContextLost();
        return;
    }
    markLayerComposited();
}


#if ENABLE(VIDEO)
GraphicsContextGLCV* GraphicsContextGLCocoa::asCV()
{
    if (!m_cv)
        m_cv = GraphicsContextGLCVCocoa::create(*this);
    return m_cv.get();
}
#endif

RefPtr<PixelBuffer> GraphicsContextGLCocoa::readCompositedResults()
{
    auto& buffer = displayBuffer();
    if (!buffer || buffer.surface->size() != getInternalFramebufferSize())
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
    if (!EGL_BindTexImage(m_displayObj, buffer.pbuffer, EGL_BACK_BUFFER))
        return nullptr;
    GL_TexParameteri(textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ScopedFramebuffer fbo;
    ScopedRestoreReadFramebufferBinding fboBinding(m_isForWebGL2, m_state.boundReadFBO, fbo);
    GL_FramebufferTexture2D(fboBinding.framebufferTarget(), GL_COLOR_ATTACHMENT0, textureTarget, texture, 0);
    ASSERT(GL_CheckFramebufferStatus(fboBinding.framebufferTarget()) == GL_FRAMEBUFFER_COMPLETE);

    auto result = readPixelsForPaintResults();

    EGLBoolean releaseOk = EGL_ReleaseTexImage(m_displayObj, buffer.pbuffer, EGL_BACK_BUFFER);
    ASSERT_UNUSED(releaseOk, releaseOk);
    return result;
}

#if ENABLE(MEDIA_STREAM) || ENABLE(WEB_CODECS)

RefPtr<VideoFrame> GraphicsContextGLCocoa::surfaceBufferToVideoFrame(SurfaceBuffer buffer)
{
    if (!makeContextCurrent())
        return nullptr;
    if (buffer == SurfaceBuffer::DrawingBuffer) {
        prepareTexture();
        waitUntilWorkScheduled();
    }
    auto& source = surfaceBuffer(buffer);
    if (!source || source.surface->size() != getInternalFramebufferSize())
        return nullptr;
    // We will mirror and rotate the buffer explicitly. Thus the source being used is always a new one.
    auto pixelBuffer = createCVPixelBuffer(source.surface->surface());
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

#if ENABLE(VIDEO)
bool GraphicsContextGLCocoa::copyTextureFromMedia(MediaPlayer& player, PlatformGLObject outputTexture, GCGLenum outputTarget, GCGLint level, GCGLenum internalFormat, GCGLenum format, GCGLenum type, bool premultiplyAlpha, bool flipY)
{
    auto videoFrame = player.videoFrameForCurrentTime();
    if (!videoFrame)
        return false;
    auto videoFrameCV = videoFrame->asVideoFrameCV();
    if (!videoFrameCV)
        return false;
    auto contextCV = asCV();
    if (!contextCV)
        return false;

    UNUSED_VARIABLE(premultiplyAlpha);
    ASSERT_UNUSED(outputTarget, outputTarget == GraphicsContextGL::TEXTURE_2D);
    return contextCV->copyVideoSampleToTexture(*videoFrameCV, outputTexture, level, internalFormat, format, type, GraphicsContextGL::FlipY(flipY));
}
#endif

RefPtr<GraphicsLayerContentsDisplayDelegate> GraphicsContextGLCocoa::layerContentsDisplayDelegate()
{
    return nullptr;
}

void GraphicsContextGLCocoa::invalidateKnownTextureContent(GCGLuint texture)
{
    if (m_cv)
        m_cv->invalidateKnownTextureContent(texture);
}

void GraphicsContextGLCocoa::withBufferAsNativeImage(SurfaceBuffer buffer, Function<void(NativeImage&)> func)
{
    RetainPtr<CGContextRef> cgContext;
    RefPtr<NativeImage> image;
    if (contextAttributes().premultipliedAlpha) {
        // Use the IOSurface backed image directly
        if (!makeContextCurrent())
            return;
        if (buffer == SurfaceBuffer::DrawingBuffer) {
            prepareTexture();
            waitUntilWorkScheduled();
        }
        IOSurfacePbuffer& source = surfaceBuffer(buffer);
        if (!source || source.surface->size() != getInternalFramebufferSize())
            return;
        cgContext = source.surface->createPlatformContext();
        if (cgContext)
            image = NativeImage::create(source.surface->createImage(cgContext.get()));
    } else {
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
            return;
        image = createNativeImageFromPixelBuffer(contextAttributes(), pixelBuffer.releaseNonNull());
    }

    if (!image)
        return;

    CGImageSetCachingFlags(image->platformImage().get(), kCGImageCachingTransient);
    func(*image);
}

void GraphicsContextGLCocoa::insertFinishedSignalOrInvoke(Function<void()> signal)
{
    if (!contextAttributes().useMetal) {
        signal();
        return;
    }
    static std::atomic<uint64_t> nextSignalValue;
    uint64_t signalValue = ++nextSignalValue;
    id<MTLSharedEvent> event = m_finishedMetalSharedEvent.get();
    // The block below has to be a real compiler generated block instead of BlockPtr due to a Metal bug. rdar://108035473
    __block Function<void()> blockSignal = WTFMove(signal);
    [event notifyListener:m_finishedMetalSharedEventListener.get() atValue:signalValue block:^(id<MTLSharedEvent>, uint64_t) {
        blockSignal();
    }];
    auto* sync = createEGLSync(event, signalValue);
    if (UNLIKELY(!sync)) {
        event.signaledValue = signalValue;
        ASSERT_NOT_REACHED();
        return;
    }
    destroyEGLSync(sync);
}

}

#endif // ENABLE(WEBGL)
