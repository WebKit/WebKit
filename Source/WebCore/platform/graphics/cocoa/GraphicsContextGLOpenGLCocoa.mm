/*
 * Copyright (C) 2009-2018 Apple Inc. All rights reserved.
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
#import "GraphicsContextGLOpenGL.h"

#import "ExtensionsGLANGLE.h"
#import "GraphicsContextGLANGLEUtilities.h"
#import "GraphicsContextGLIOSurfaceSwapChain.h"
#import "GraphicsContextGLOpenGLManager.h"
#import "Logging.h"
#import "RuntimeApplicationChecks.h"
#import "WebGLLayer.h"
#import <CoreGraphics/CGBitmapContext.h>
#import <Metal/Metal.h>
#import <pal/spi/cocoa/MetalSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/darwin/WeakLinking.h>
#import <wtf/text/CString.h>

#if PLATFORM(IOS_FAMILY)
#import "WebCoreThread.h"
#endif

#if ENABLE(VIDEO) && USE(AVFOUNDATION)
#include "GraphicsContextGLCV.h"
#endif

WTF_WEAK_LINK_FORCE_IMPORT(EGL_Initialize);

namespace WebCore {

// In isCurrentContextPredictable() == true case this variable is accessed in single-threaded manner.
// In isCurrentContextPredictable() == false case this variable is accessed from multiple threads but always sequentially
// and it always contains nullptr and nullptr is always written to it.
static GraphicsContextGLOpenGL* currentContext;

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

static bool platformSupportsMetal(bool isWebGL2)
{
    auto device = MTLCreateSystemDefaultDevice();

    if (device) {
#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR)
        // A8 devices (iPad Mini 4, iPad Air 2) cannot use WebGL2 via Metal.
        // This check can be removed once they are no longer supported.
        if (isWebGL2)
            return [device supportsFamily:MTLGPUFamilyApple3];
#else
        UNUSED_PARAM(isWebGL2);
#endif
        return true;
    }
    
    return false;
}

static ScopedEGLDefaultDisplay InitializeEGLDisplay(const GraphicsContextGLAttributes& attrs)
{
    EGLint majorVersion = 0;
    EGLint minorVersion = 0;
    EGLDisplay display;

    Vector<EGLint> displayAttributes;

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
    }

    if (attrs.powerPreference != GraphicsContextGLAttributes::PowerPreference::Default || attrs.forceRequestForHighPerformanceGPU) {
        displayAttributes.append(EGL_POWER_PREFERENCE_ANGLE);
        if (attrs.powerPreference == GraphicsContextGLAttributes::PowerPreference::LowPower && !attrs.forceRequestForHighPerformanceGPU) {
            LOG(WebGL, "Requesting low power GPU.");
            displayAttributes.append(EGL_LOW_POWER_ANGLE);
        } else {
            ASSERT(attrs.powerPreference == GraphicsContextGLAttributes::PowerPreference::HighPerformance || attrs.forceRequestForHighPerformanceGPU);
            LOG(WebGL, "Requesting high power GPU if available.");
            displayAttributes.append(EGL_HIGH_POWER_ANGLE);
        }
    }
    displayAttributes.append(EGL_NONE);
    display = EGL_GetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, reinterpret_cast<void*>(EGL_DEFAULT_DISPLAY), displayAttributes.data());

    if (EGL_Initialize(display, &majorVersion, &minorVersion) == EGL_FALSE) {
        LOG(WebGL, "EGLDisplay Initialization failed.");
        return { };
    }
    LOG(WebGL, "ANGLE initialised Major: %d Minor: %d", majorVersion, minorVersion);
    if (shouldInitializeWithVolatileContextSupport) {
        // After initialization, EGL_DEFAULT_DISPLAY will return the platform-customized display.
        ASSERT(display == EGL_GetDisplay(EGL_DEFAULT_DISPLAY));
        ASSERT(checkVolatileContextSupportIfDeviceExists(display, "EGL_ANGLE_platform_device_context_volatile_eagl", "EGL_ANGLE_device_eagl", EGL_EAGL_CONTEXT_ANGLE));
        ASSERT(checkVolatileContextSupportIfDeviceExists(display, "EGL_ANGLE_platform_device_context_volatile_cgl", "EGL_ANGLE_device_cgl", EGL_CGL_CONTEXT_ANGLE));
    }
    return ScopedEGLDefaultDisplay::adoptInitializedDisplay(display);
}

static const unsigned statusCheckThreshold = 5;

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

static bool isANGLEAvailable()
{
    return !!EGL_Initialize;
}

RefPtr<GraphicsContextGLOpenGL> GraphicsContextGLOpenGL::create(GraphicsContextGLAttributes attrs, HostWindow* hostWindow)
{
    // If ANGLE is not loaded, we can fail immediately.
    if (!isANGLEAvailable()) {
        WTFLogAlways("ANGLE shared library was not loaded. Can't make GraphicsContextGL.");
        return nullptr;
    }

    // Make space for the incoming context if we're full.
    GraphicsContextGLOpenGLManager::sharedManager().recycleContextIfNecessary();
    if (GraphicsContextGLOpenGLManager::sharedManager().hasTooManyContexts())
        return nullptr;

    RefPtr<GraphicsContextGLOpenGL> context = adoptRef(new GraphicsContextGLOpenGL(attrs, hostWindow, nullptr, nullptr));

    if (!context->m_contextObj)
        return nullptr;

    GraphicsContextGLOpenGLManager::sharedManager().addContext(context.get());

    return context;
}

Ref<GraphicsContextGLOpenGL> GraphicsContextGLOpenGL::createShared(GraphicsContextGLOpenGL& sharedContext)
{

    auto context = adoptRef(*new GraphicsContextGLOpenGL(sharedContext.contextAttributes(), nullptr, &sharedContext, nullptr));

    GraphicsContextGLOpenGLManager::sharedManager().addContext(context.ptr());

    return context;
}

Ref<GraphicsContextGLOpenGL> GraphicsContextGLOpenGL::createForGPUProcess(const GraphicsContextGLAttributes& attrs, GraphicsContextGLIOSurfaceSwapChain* swapChain)
{
    return adoptRef(*new GraphicsContextGLOpenGL(attrs, nullptr, nullptr, swapChain));
}

GraphicsContextGLOpenGL::GraphicsContextGLOpenGL(GraphicsContextGLAttributes attrs, HostWindow*, GraphicsContextGLOpenGL* sharedContext, GraphicsContextGLIOSurfaceSwapChain* swapChain)
    : GraphicsContextGL(attrs, sharedContext)
{
    m_isForWebGL2 = attrs.webGLVersion == GraphicsContextGLWebGLVersion::WebGL2;
    if (attrs.useMetal && !platformSupportsMetal(m_isForWebGL2)) {
        attrs.useMetal = false;
        setContextAttributes(attrs);
    }

#if ENABLE(WEBXR)
    if (attrs.xrCompatible) {
        // FIXME: It's almost certain that any connected headset will require the high-power GPU,
        // which is the same GPU we need this context to use. However, this is not guaranteed, and
        // there is also the chance that there are multiple GPUs. Given that you can request the
        // GraphicsContextGL before initializing the WebXR session, we'll need some way to
        // migrate the context to the appropriate GPU when the code here does not work.
        LOG(WebGL, "WebXR compatible context requested. This will also trigger a request for the high-power GPU.");
        attrs.forceRequestForHighPerformanceGPU = true;
        setContextAttributes(attrs);
    }
#endif

    m_displayObj = InitializeEGLDisplay(attrs);
    if (!m_displayObj)
        return;

    bool supportsPowerPreference = false;
#if PLATFORM(MAC)
    const char *displayExtensions = EGL_QueryString(m_displayObj, EGL_EXTENSIONS);
    m_supportsPowerPreference = strstr(displayExtensions, "EGL_ANGLE_power_preference");
    supportsPowerPreference = m_supportsPowerPreference;
#endif
    if (!supportsPowerPreference && attrs.powerPreference == GraphicsContextGLPowerPreference::HighPerformance) {
        attrs.powerPreference = GraphicsContextGLPowerPreference::Default;
        setContextAttributes(attrs);
    }

    EGLint configAttributes[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLint numberConfigsReturned = 0;
    EGL_ChooseConfig(m_displayObj, configAttributes, &m_configObj, 1, &numberConfigsReturned);
    if (numberConfigsReturned != 1) {
        LOG(WebGL, "EGLConfig Initialization failed.");
        return;
    }
    LOG(WebGL, "Got EGLConfig");

    EGL_BindAPI(EGL_OPENGL_ES_API);
    if (EGL_GetError() != EGL_SUCCESS) {
        LOG(WebGL, "Unable to bind to OPENGL_ES_API");
        return;
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
    if (!sharedContext) {
        // The shared context is only non-null when creating a context
        // on behalf of the VideoTextureCopier. WebGL-specific rendering
        // feedback loop validation does not work in multi-context
        // scenarios, and must be disabled for the VideoTextureCopier's
        // context.
        eglContextAttributes.append(EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE);
        eglContextAttributes.append(EGL_TRUE);
        // WebGL requires that all resources are cleared at creation.
        // FIXME: performing robust resource initialization in the VideoTextureCopier adds a large amount of overhead
        // so it would be nice to avoid that there (we should always be touching every pixel as we copy).
        eglContextAttributes.append(EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE);
        eglContextAttributes.append(EGL_TRUE);
    }

    // WebGL doesn't allow client arrays.
    eglContextAttributes.append(EGL_CONTEXT_CLIENT_ARRAYS_ENABLED_ANGLE);
    eglContextAttributes.append(EGL_FALSE);
    // WebGL doesn't allow implicit creation of objects on bind.
    eglContextAttributes.append(EGL_CONTEXT_BIND_GENERATES_RESOURCE_CHROMIUM);
    eglContextAttributes.append(EGL_FALSE);

    eglContextAttributes.append(EGL_NONE);

    m_contextObj = EGL_CreateContext(m_displayObj, m_configObj, sharedContext ? static_cast<EGLContext>(sharedContext->m_contextObj) : EGL_NO_CONTEXT, eglContextAttributes.data());
    if (m_contextObj == EGL_NO_CONTEXT) {
        LOG(WebGL, "EGLContext Initialization failed.");
        return;
    }
    LOG(WebGL, "Got EGLContext");

    EGL_MakeCurrent(m_displayObj, EGL_NO_SURFACE, EGL_NO_SURFACE, m_contextObj);

    if (m_isForWebGL2)
        gl::Enable(GraphicsContextGL::PRIMITIVE_RESTART_FIXED_INDEX);

    Vector<ASCIILiteral, 4> requiredExtensions;
    if (m_isForWebGL2) {
        // For WebGL 2.0 occlusion queries to work.
        requiredExtensions.append("GL_EXT_occlusion_query_boolean"_s);
    }
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    if (!needsEAGLOnMac()) {
        // For IOSurface-backed textures.
        requiredExtensions.append("GL_ANGLE_texture_rectangle"_s);
        // For creating the EGL surface from an IOSurface.
        requiredExtensions.append("GL_EXT_texture_format_BGRA8888"_s);
    }
#endif // PLATFORM(MAC) || PLATFORM(MACCATALYST)
#if ENABLE(WEBXR) && !PLATFORM(IOS_FAMILY_SIMULATOR)
    if (contextAttributes().xrCompatible)
        requiredExtensions.append("GL_OES_EGL_image"_s);
#endif
    ExtensionsGL& extensions = getExtensions();
    for (auto& extension : requiredExtensions) {
        if (!extensions.supports(extension)) {
            LOG(WebGL, "Missing required extension. %s", extension.characters());
            return;
        }
        extensions.ensureEnabled(extension);
    }
    if (contextAttributes().useMetal) {
        // The implementation uses GLsync objects. Enable the functionality for WebGL 1.0 contexts
        // that use OpenGL ES 2.0.
        if (extensions.supports("GL_ARB_sync"_s)) {
            attrs.hasFenceSync = true;
            extensions.ensureEnabled("GL_ARB_sync"_s);
            setContextAttributes(attrs);
        }
    }
    validateAttributes();
    attrs = contextAttributes(); // They may have changed during validation.

    if (swapChain)
        m_swapChain = swapChain;
    else {
    BEGIN_BLOCK_OBJC_EXCEPTIONS
        m_webGLLayer = adoptNS([[WebGLLayer alloc] initWithDevicePixelRatio:attrs.devicePixelRatio contentsOpaque:!attrs.alpha]);
#ifndef NDEBUG
        [m_webGLLayer setName:@"WebGL Layer"];
#endif
    END_BLOCK_OBJC_EXCEPTIONS
        m_swapChain = &[m_webGLLayer swapChain];
    }

    // Create the texture that will be used for the framebuffer.
    GLenum textureTarget = drawingBufferTextureTarget();

    gl::GenTextures(1, &m_texture);
    gl::BindTexture(textureTarget, m_texture);
    gl::TexParameteri(textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl::TexParameteri(textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl::TexParameteri(textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::BindTexture(textureTarget, 0);

    gl::GenFramebuffers(1, &m_fbo);
    gl::BindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    m_state.boundDrawFBO = m_state.boundReadFBO = m_fbo;

    if (!attrs.antialias && (attrs.stencil || attrs.depth))
        gl::GenRenderbuffers(1, &m_depthStencilBuffer);

    // If necessary, create another framebuffer for the multisample results.
    if (attrs.antialias) {
        gl::GenFramebuffers(1, &m_multisampleFBO);
        gl::BindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
        m_state.boundDrawFBO = m_state.boundReadFBO = m_multisampleFBO;
        gl::GenRenderbuffers(1, &m_multisampleColorBuffer);
        if (attrs.stencil || attrs.depth)
            gl::GenRenderbuffers(1, &m_multisampleDepthStencilBuffer);
    } else if (attrs.preserveDrawingBuffer) {
        // If necessary, create another texture to handle preserveDrawingBuffer:true without
        // antialiasing.
        gl::GenTextures(1, &m_preserveDrawingBufferTexture);
        gl::BindTexture(GL_TEXTURE_2D, m_preserveDrawingBufferTexture);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl::BindTexture(GL_TEXTURE_2D, 0);
        // Create an FBO with which to perform BlitFramebuffer from one texture to the other.
        gl::GenFramebuffers(1, &m_preserveDrawingBufferFBO);
    }

    gl::ClearColor(0, 0, 0, 0);

    LOG(WebGL, "Created a GraphicsContextGLOpenGL (%p).", this);
}

GraphicsContextGLOpenGL::~GraphicsContextGLOpenGL()
{
    GraphicsContextGLOpenGLManager::sharedManager().removeContext(this);
    if (makeContextCurrent()) {
        GraphicsContextGLAttributes attrs = contextAttributes();
        gl::DeleteTextures(1, &m_texture);

        if (attrs.antialias) {
            gl::DeleteRenderbuffers(1, &m_multisampleColorBuffer);
            if (attrs.stencil || attrs.depth)
                gl::DeleteRenderbuffers(1, &m_multisampleDepthStencilBuffer);
            gl::DeleteFramebuffers(1, &m_multisampleFBO);
        } else {
            if (attrs.stencil || attrs.depth)
                gl::DeleteRenderbuffers(1, &m_depthStencilBuffer);
        }
        gl::DeleteFramebuffers(1, &m_fbo);
        if (m_preserveDrawingBufferTexture)
            gl::DeleteTextures(1, &m_preserveDrawingBufferTexture);
        if (m_preserveDrawingBufferFBO)
            gl::DeleteFramebuffers(1, &m_preserveDrawingBufferFBO);
        // If fences are not enabled, this loop will not execute.
        for (auto& fence : m_frameCompletionFences)
            fence.reset();
    } else {
        for (auto& fence : m_frameCompletionFences)
            fence.abandon();
    }
    if (m_displayBufferPbuffer) {
        EGL_DestroySurface(m_displayObj, m_displayBufferPbuffer);
        auto recycledBuffer = m_swapChain->recycleBuffer();
        if (recycledBuffer.handle)
            EGL_DestroySurface(m_displayObj, recycledBuffer.handle);
        auto contentsHandle = m_swapChain->detachClient();
        if (contentsHandle)
            EGL_DestroySurface(m_displayObj, contentsHandle);
    }
    if (m_contextObj) {
        clearCurrentContext();
        EGL_DestroyContext(m_displayObj, m_contextObj);
    } else
        ASSERT(currentContext != this);
    LOG(WebGL, "Destroyed a GraphicsContextGLOpenGL (%p).", this);
}

GCGLenum GraphicsContextGLOpenGL::drawingBufferTextureTarget()
{
#if PLATFORM(MACCATALYST)
    if (needsEAGLOnMac())
        return TEXTURE_2D;
    return TEXTURE_RECTANGLE_ARB;
#elif PLATFORM(MAC)
    return TEXTURE_RECTANGLE_ARB;
#else
    return TEXTURE_2D;
#endif
}

GCGLenum GraphicsContextGLOpenGL::drawingBufferTextureTargetQuery()
{
#if PLATFORM(MACCATALYST)
    if (needsEAGLOnMac())
        return TEXTURE_BINDING_2D;
    return TEXTURE_BINDING_RECTANGLE_ARB;
#elif PLATFORM(MAC)
    return TEXTURE_BINDING_RECTANGLE_ARB;
#else
    return TEXTURE_BINDING_2D;
#endif
}

GCGLint GraphicsContextGLOpenGL::EGLDrawingBufferTextureTarget()
{
#if PLATFORM(MACCATALYST)
    if (needsEAGLOnMac())
        return EGL_TEXTURE_2D;
    return EGL_TEXTURE_RECTANGLE_ANGLE;
#elif PLATFORM(MAC)
    return EGL_TEXTURE_RECTANGLE_ANGLE;
#else
    return EGL_TEXTURE_2D;
#endif
}

bool GraphicsContextGLOpenGL::makeContextCurrent()
{
    if (!m_contextObj)
        return false;
    // If there is no drawing buffer, we failed to allocate one during preparing for display.
    // The exception is the case when the context is used before reshaping.
    if (!m_displayBufferBacking && !getInternalFramebufferSize().isEmpty())
        return false;
    if (currentContext == this)
        return true;
    // Calling MakeCurrent is important to set volatile platform context. See InitializeEGLDisplay().
    if (!EGL_MakeCurrent(m_displayObj, EGL_NO_SURFACE, EGL_NO_SURFACE, m_contextObj))
        return false;
    if (isCurrentContextPredictable())
        currentContext = this;
    return true;
}

void GraphicsContextGLOpenGL::clearCurrentContext()
{
    EGLBoolean result = EGL_MakeCurrent(m_displayObj, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    ASSERT_UNUSED(result, result);
    currentContext = nullptr;
}

#if PLATFORM(IOS_FAMILY)
bool GraphicsContextGLOpenGL::releaseCurrentContext(ReleaseBehavior releaseBehavior)
{
    if (!isANGLEAvailable())
        return true;

    // At the moment this function is relevant only when web thread lock owns the GraphicsContextGLOpenGL current context.
    ASSERT(!isCurrentContextPredictable());

    if (!EGL_BindAPI(EGL_OPENGL_ES_API))
        return false;
    if (EGL_GetCurrentContext() == EGL_NO_CONTEXT)
        return true;

    // At the time of writing, ANGLE does not flush on MakeCurrent. Since we are
    // potentially switching threads, we should flush.
    // Note: Here we assume also that ANGLE has only one platform context -- otherwise
    // we would need to flush each EGL context that has been used.
    gl::Flush();

    // Unset the EGL current context, since the next access might be from another thread, and the
    // context cannot be current on multiple threads.

    if (releaseBehavior == ReleaseBehavior::ReleaseThreadResources) {
        // Called when we do not know if we will ever see another call from this thread again.
        // Unset the EGL current context by releasing whole EGL thread state.
        // Theoretically ReleaseThread can reset the bound API, so the rest of the code mentions BindAPI/QueryAPI.
        return EGL_ReleaseThread();
    }
    // On WebKit owned threads, WebKit is able to choose the time for the EGL cleanup.
    // This is why we only unset the context.
    // Note: At the time of writing the EGL cleanup is chosen to be not done at all.
    EGLDisplay display = EGL_GetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY)
        return false;
    return EGL_MakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}
#endif

void GraphicsContextGLOpenGL::checkGPUStatus()
{
    if (m_failNextStatusCheck) {
        LOG(WebGL, "Pretending the GPU has reset (%p). Lose the context.", this);
        m_failNextStatusCheck = false;
        forceContextLost();
        clearCurrentContext();
        return;
    }

    // Only do the check every statusCheckThreshold calls.
    if (m_statusCheckCount)
        return;

    m_statusCheckCount = (m_statusCheckCount + 1) % statusCheckThreshold;

    GLint restartStatus = 0;
    // FIXME: check via KHR_robustness.
    restartStatus = 0;
}

void GraphicsContextGLOpenGL::setContextVisibility(bool isVisible)
{
#if PLATFORM(MAC)
    if (contextAttributes().powerPreference != GraphicsContextGLPowerPreference::HighPerformance)
        return;
    if (isVisible)
        m_highPerformanceGPURequest = ScopedHighPerformanceGPURequest::acquire();
    else
        m_highPerformanceGPURequest = { };
#else
    UNUSED_PARAM(isVisible);
#endif
}

void GraphicsContextGLOpenGL::displayWasReconfigured()
{
#if PLATFORM(MAC)
    if (m_supportsPowerPreference)
        EGL_HandleGPUSwitchANGLE(m_displayObj);
#endif
    dispatchContextChangedNotification();
}

bool GraphicsContextGLOpenGL::reshapeDisplayBufferBacking()
{
    ASSERT(!getInternalFramebufferSize().isEmpty());
    // Reset the current backbuffer now before allocating a new one in order to slightly reduce memory pressure.
    if (m_displayBufferBacking) {
        m_displayBufferBacking.reset();
        EGL_ReleaseTexImage(m_displayObj, m_displayBufferPbuffer, EGL_BACK_BUFFER);
        EGL_DestroySurface(m_displayObj, m_displayBufferPbuffer);
        m_displayBufferPbuffer = EGL_NO_SURFACE;
    }
    // Reset the future recycled buffer now, because it most likely will not be reusable at the time it will be reused.
    if (m_swapChain) {
        auto recycledBuffer = m_swapChain->recycleBuffer();
        if (recycledBuffer.handle)
            EGL_DestroySurface(m_displayObj, recycledBuffer.handle);
        recycledBuffer.surface.reset();
    }
    return allocateAndBindDisplayBufferBacking();
}

bool GraphicsContextGLOpenGL::allocateAndBindDisplayBufferBacking()
{
    ASSERT(!getInternalFramebufferSize().isEmpty());
    auto backing = IOSurface::create(getInternalFramebufferSize(), DestinationColorSpace::SRGB());
    if (!backing)
        return false;

    backing->migrateColorSpaceToProperties();

    const bool usingAlpha = contextAttributes().alpha;
    const auto size = getInternalFramebufferSize();
    const EGLint surfaceAttributes[] = {
        EGL_WIDTH, size.width(),
        EGL_HEIGHT, size.height(),
        EGL_IOSURFACE_PLANE_ANGLE, 0,
        EGL_TEXTURE_TARGET, WebCore::GraphicsContextGLOpenGL::EGLDrawingBufferTextureTarget(),
        EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, usingAlpha ? GL_BGRA_EXT : GL_RGB,
        EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
        EGL_TEXTURE_TYPE_ANGLE, GL_UNSIGNED_BYTE,
        // Only has an effect on the iOS Simulator.
        EGL_IOSURFACE_USAGE_HINT_ANGLE, EGL_IOSURFACE_WRITE_HINT_ANGLE,
        EGL_NONE, EGL_NONE
    };
    EGLSurface pbuffer = EGL_CreatePbufferFromClientBuffer(m_displayObj, EGL_IOSURFACE_ANGLE, backing->surface(), m_configObj, surfaceAttributes);
    if (!pbuffer)
        return false;
    return bindDisplayBufferBacking(WTFMove(backing), pbuffer);
}

bool GraphicsContextGLOpenGL::bindDisplayBufferBacking(std::unique_ptr<IOSurface> backing, void* pbuffer)
{
    GCGLenum textureTarget = drawingBufferTextureTarget();
    ScopedRestoreTextureBinding restoreBinding(drawingBufferTextureTargetQuery(), textureTarget, textureTarget != TEXTURE_RECTANGLE_ARB);
    gl::BindTexture(textureTarget, m_texture);
    if (!EGL_BindTexImage(m_displayObj, pbuffer, EGL_BACK_BUFFER)) {
        EGL_DestroySurface(m_displayObj, pbuffer);
        return false;
    }
    m_displayBufferPbuffer = pbuffer;
    m_displayBufferBacking = WTFMove(backing);
    return true;
}

void* GraphicsContextGLOpenGL::createPbufferAndAttachIOSurface(GCGLenum target, PbufferAttachmentUsage usage, GCGLenum internalFormat, GCGLsizei width, GCGLsizei height, GCGLenum type, IOSurfaceRef surface, GCGLuint plane)
{
    if (target != GraphicsContextGL::TEXTURE_RECTANGLE_ARB && target != GraphicsContextGL::TEXTURE_2D) {
        LOG(WebGL, "Unknown texture target %d.", static_cast<int>(target));
        return nullptr;
    }

    auto eglTextureTarget = [&] () -> EGLint {
        if (target == GraphicsContextGL::TEXTURE_RECTANGLE_ARB)
            return EGL_TEXTURE_RECTANGLE_ANGLE;
        return EGL_TEXTURE_2D;
    }();

    if (eglTextureTarget != GraphicsContextGLOpenGL::EGLDrawingBufferTextureTarget()) {
        LOG(WebGL, "Mismatch in EGL texture target: %d should be %d.", static_cast<int>(target), GraphicsContextGLOpenGL::EGLDrawingBufferTextureTarget());
        return nullptr;
    }

    auto usageHintAngle = [&] () -> EGLint {
        if (usage == PbufferAttachmentUsage::Read)
            return EGL_IOSURFACE_READ_HINT_ANGLE;
        if (usage == PbufferAttachmentUsage::Write)
            return EGL_IOSURFACE_WRITE_HINT_ANGLE;
        return EGL_IOSURFACE_READ_HINT_ANGLE | EGL_IOSURFACE_WRITE_HINT_ANGLE;
    }();

    const EGLint surfaceAttributes[] = {
        EGL_WIDTH, width,
        EGL_HEIGHT, height,
        EGL_IOSURFACE_PLANE_ANGLE, static_cast<EGLint>(plane),
        EGL_TEXTURE_TARGET, static_cast<EGLint>(eglTextureTarget),
        EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, static_cast<EGLint>(internalFormat),
        EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
        EGL_TEXTURE_TYPE_ANGLE, static_cast<EGLint>(type),
        // Only has an effect on the iOS Simulator.
        EGL_IOSURFACE_USAGE_HINT_ANGLE, usageHintAngle,
        EGL_NONE, EGL_NONE
    };

    auto display = platformDisplay();
    EGLSurface pbuffer = EGL_CreatePbufferFromClientBuffer(display, EGL_IOSURFACE_ANGLE, surface, platformConfig(), surfaceAttributes);
    if (!pbuffer) {
        LOG(WebGL, "EGL_CreatePbufferFromClientBuffer failed.");
        return nullptr;
    }

    if (!EGL_BindTexImage(display, pbuffer, EGL_BACK_BUFFER)) {
        LOG(WebGL, "EGL_BindTexImage failed.");
        EGL_DestroySurface(display, pbuffer);
        return nullptr;
    }

    return pbuffer;
}

void GraphicsContextGLOpenGL::destroyPbufferAndDetachIOSurface(void* handle)
{
    auto display = platformDisplay();
    EGL_ReleaseTexImage(display, handle, EGL_BACK_BUFFER);
    EGL_DestroySurface(display, handle);
}

#if !PLATFORM(IOS_FAMILY_SIMULATOR)
void* GraphicsContextGLOpenGL::attachIOSurfaceToSharedTexture(GCGLenum target, IOSurface* surface)
{
    constexpr EGLint emptyAttributes[] = { EGL_NONE };

    // Create a MTLTexture out of the IOSurface.
    // FIXME: We need to use the same device that ANGLE is using, which might not be the default.

    RetainPtr<MTLSharedTextureHandle> handle = adoptNS([[MTLSharedTextureHandle alloc] initWithIOSurface:surface->surface() label:@"WebXR"]);
    if (!handle) {
        LOG(WebGL, "Unable to create a MTLSharedTextureHandle from the IOSurface in attachIOSurfaceToTexture.");
        return nullptr;
    }

    if (!handle.get().device) {
        LOG(WebGL, "MTLSharedTextureHandle does not have a Metal device in attachIOSurfaceToTexture.");
        return nullptr;
    }

    auto texture = adoptNS([handle.get().device newSharedTextureWithHandle:handle.get()]);
    if (!texture) {
        LOG(WebGL, "Unable to create a MTLSharedTexture from the texture handle in attachIOSurfaceToTexture.");
        return nullptr;
    }

    // FIXME: Does the texture have the correct usage mode?

    // Create an EGLImage out of the MTLTexture
    auto display = platformDisplay();
    auto eglImage = EGL_CreateImageKHR(display, EGL_NO_CONTEXT, EGL_METAL_TEXTURE_ANGLE, reinterpret_cast<EGLClientBuffer>(texture.get()), emptyAttributes);
    if (!eglImage) {
        LOG(WebGL, "Unable to create an EGLImage from the Metal handle in attachIOSurfaceToTexture.");
        return nullptr;
    }

    // Tell the currently bound texture to use the EGLImage.
    gl::EGLImageTargetTexture2DOES(target, eglImage);

    return eglImage;
}

void GraphicsContextGLOpenGL::detachIOSurfaceFromSharedTexture(void* handle)
{
    auto display = platformDisplay();
    EGL_DestroyImageKHR(display, handle);
}
#endif

bool GraphicsContextGLOpenGL::isGLES2Compliant() const
{
    return m_isForWebGL2;
}

void GraphicsContextGLOpenGL::simulateEventForTesting(SimulatedEventForTesting event)
{
    if (event == SimulatedEventForTesting::ContextChange) {
        GraphicsContextGLOpenGLManager::sharedManager().displayWasReconfigured();
        return;
    }
    if (event == SimulatedEventForTesting::GPUStatusFailure) {
        m_failNextStatusCheck = true;
        return;
    }
}

void GraphicsContextGLOpenGL::prepareForDisplay()
{
    if (m_layerComposited)
        return;
    if (!makeContextCurrent())
        return;
    prepareTextureImpl();

    // The IOSurface will be used from other graphics subsystem, so flush GL commands.
    gl::Flush();

    auto recycledBuffer = m_swapChain->recycleBuffer();

    EGL_ReleaseTexImage(m_displayObj, m_displayBufferPbuffer, EGL_BACK_BUFFER);
    m_swapChain->present({ WTFMove(m_displayBufferBacking), m_displayBufferPbuffer });
    m_displayBufferPbuffer = EGL_NO_SURFACE;

    bool hasNewBacking = false;
    if (recycledBuffer.surface && recycledBuffer.surface->size() == getInternalFramebufferSize()) {
        hasNewBacking = bindDisplayBufferBacking(WTFMove(recycledBuffer.surface), recycledBuffer.handle);
        recycledBuffer.handle = nullptr;
    }
    recycledBuffer.surface.reset();
    if (recycledBuffer.handle)
        EGL_DestroySurface(m_displayObj, recycledBuffer.handle);

    // Error will be handled by next call to makeContextCurrent() which will notice lack of display buffer.
    if (!hasNewBacking)
        allocateAndBindDisplayBufferBacking();

    markLayerComposited();

    if (contextAttributes().useMetal && contextAttributes().hasFenceSync) {
        // OpenGL sync objects are not signaling upon completion on Catalina-era drivers.
        // OpenGL drivers typically implement some sort of internal throttling.
        bool success = waitAndUpdateOldestFrame();
        UNUSED_VARIABLE(success); // FIXME: implement context lost.
    }
}

std::optional<PixelBuffer> GraphicsContextGLOpenGL::readCompositedResults()
{
    auto& displayBuffer = m_swapChain->displayBuffer();
    if (!displayBuffer.surface || !displayBuffer.handle)
        return std::nullopt;
    if (displayBuffer.surface->size() != getInternalFramebufferSize())
        return std::nullopt;
    // Note: We are using GL to read the IOSurface. At the time of writing, there are no convinient
    // functions to convert the IOSurface pixel data to ImageData. The image data ends up being
    // drawn to a ImageBuffer, but at the time there's no functions to construct a NativeImage
    // out of an IOSurface in such a way that drawing the NativeImage would be guaranteed leave
    // the IOSurface be unrefenced after the draw call finishes.
    ScopedTexture texture;
    GCGLenum textureTarget = drawingBufferTextureTarget();
    ScopedRestoreTextureBinding restoreBinding(drawingBufferTextureTargetQuery(), textureTarget, textureTarget != TEXTURE_RECTANGLE_ARB);
    gl::BindTexture(textureTarget, texture);
    if (!EGL_BindTexImage(m_displayObj, displayBuffer.handle, EGL_BACK_BUFFER))
        return std::nullopt;
    gl::TexParameteri(textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ScopedFramebuffer fbo;
    ScopedRestoreReadFramebufferBinding fboBinding(m_isForWebGL2, m_state.boundReadFBO, fbo);
    gl::FramebufferTexture2D(fboBinding.framebufferTarget(), GL_COLOR_ATTACHMENT0, textureTarget, texture, 0);
    ASSERT(gl::CheckFramebufferStatus(fboBinding.framebufferTarget()) == GL_FRAMEBUFFER_COMPLETE);

    auto result = readPixelsForPaintResults();

    EGLBoolean releaseOk = EGL_ReleaseTexImage(m_displayObj, displayBuffer.handle, EGL_BACK_BUFFER);
    ASSERT_UNUSED(releaseOk, releaseOk);
    return result;
}

void GraphicsContextGLOpenGL::releaseAllResourcesIfUnused()
{
    ScopedEGLDefaultDisplay::releaseAllResourcesIfUnused();
}

}

#endif // ENABLE(WEBGL)
