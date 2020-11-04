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

#if ENABLE(GRAPHICS_CONTEXT_GL)
#import "GraphicsContextGLOpenGL.h"

#import "ExtensionsGLANGLE.h"
#import "GraphicsContextGLANGLEUtilities.h"
#import "GraphicsContextGLOpenGLManager.h"
#import "HostWindow.h"
#import "Logging.h"
#import "OpenGLSoftLinkCocoa.h"
#import "RuntimeApplicationChecks.h"
#import "WebCoreThread.h"
#import "WebGLLayer.h"
#import <CoreGraphics/CGBitmapContext.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/text/CString.h>

#if PLATFORM(MAC)
#import "ScreenProperties.h"
#import <OpenGL/CGLRenderers.h>
#endif

namespace WebCore {

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

static EGLDisplay InitializeEGLDisplay()
{
    EGLint majorVersion = 0;
    EGLint minorVersion = 0;
    EGLDisplay display;
    bool shouldInitializeWithVolatileContextSupport = !isInWebProcess();
    if (shouldInitializeWithVolatileContextSupport) {
        // For WK1 type APIs we need to set "volatile platform context" for specific
        // APIs, since client code will be able to override the thread-global context
        // that ANGLE expects.
        EGLint displayAttributes[] = {
            EGL_PLATFORM_ANGLE_DEVICE_CONTEXT_VOLATILE_EAGL_ANGLE, EGL_TRUE,
            EGL_PLATFORM_ANGLE_DEVICE_CONTEXT_VOLATILE_CGL_ANGLE, EGL_TRUE,
            EGL_NONE
        };
        display = EGL_GetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, reinterpret_cast<void *>(EGL_DEFAULT_DISPLAY), displayAttributes);
    } else
        display = EGL_GetDisplay(EGL_DEFAULT_DISPLAY);

    if (EGL_Initialize(display, &majorVersion, &minorVersion) == EGL_FALSE) {
        LOG(WebGL, "EGLDisplay Initialization failed.");
        return EGL_NO_DISPLAY;
    }
    LOG(WebGL, "ANGLE initialised Major: %d Minor: %d", majorVersion, minorVersion);
    if (shouldInitializeWithVolatileContextSupport) {
        // After initialization, EGL_DEFAULT_DISPLAY will return the platform-customized display.
        ASSERT(display == EGL_GetDisplay(EGL_DEFAULT_DISPLAY));
        ASSERT(checkVolatileContextSupportIfDeviceExists(display, "EGL_ANGLE_platform_device_context_volatile_eagl", "EGL_ANGLE_device_eagl", EGL_EAGL_CONTEXT_ANGLE));
        ASSERT(checkVolatileContextSupportIfDeviceExists(display, "EGL_ANGLE_platform_device_context_volatile_cgl", "EGL_ANGLE_device_cgl", EGL_CGL_CONTEXT_ANGLE));
    }
    return display;
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


RefPtr<GraphicsContextGLOpenGL> GraphicsContextGLOpenGL::create(GraphicsContextGLAttributes attrs, HostWindow* hostWindow, GraphicsContextGL::Destination destination)
{
    // This implementation doesn't currently support rendering directly to the HostWindow.
    if (destination == Destination::DirectlyToHostWindow)
        return nullptr;

    // Make space for the incoming context if we're full.
    GraphicsContextGLOpenGLManager::sharedManager().recycleContextIfNecessary();
    if (GraphicsContextGLOpenGLManager::sharedManager().hasTooManyContexts())
        return nullptr;

    RefPtr<GraphicsContextGLOpenGL> context = adoptRef(new GraphicsContextGLOpenGL(attrs, hostWindow, destination));

    if (!context->m_contextObj)
        return nullptr;

    GraphicsContextGLOpenGLManager::sharedManager().addContext(context.get(), hostWindow);

    return context;
}

Ref<GraphicsContextGLOpenGL> GraphicsContextGLOpenGL::createShared(GraphicsContextGLOpenGL& sharedContext)
{
    auto hostWindow = GraphicsContextGLOpenGLManager::sharedManager().hostWindowForContext(&sharedContext);
    auto context = adoptRef(*new GraphicsContextGLOpenGL(sharedContext.contextAttributes(), hostWindow, sharedContext.destination(), &sharedContext));

    GraphicsContextGLOpenGLManager::sharedManager().addContext(context.ptr(), hostWindow);

    return context;
}

#if PLATFORM(MAC) // FIXME: This probably should be just enabled - see <rdar://53062794>.

static void setGPUByRegistryID(CGLContextObj contextObj, CGLPixelFormatObj pixelFormatObj, IORegistryGPUID preferredGPUID)
{
    // When the WebProcess does not have access to the WindowServer, there is no way for OpenGL to tell which GPU is connected to a display.
    // On 10.13+, find the virtual screen that corresponds to the preferred GPU by its registryID.
    // CGLSetVirtualScreen can then be used to tell OpenGL which GPU it should be using.

    if (!contextObj || !preferredGPUID)
        return;

    GLint virtualScreenCount = 0;
    CGLError error = CGLDescribePixelFormat(pixelFormatObj, 0, kCGLPFAVirtualScreenCount, &virtualScreenCount);
    ASSERT(error == kCGLNoError);

    GLint firstAcceleratedScreen = -1;

    for (GLint virtualScreen = 0; virtualScreen < virtualScreenCount; ++virtualScreen) {
        GLint displayMask = 0;
        error = CGLDescribePixelFormat(pixelFormatObj, virtualScreen, kCGLPFADisplayMask, &displayMask);
        ASSERT(error == kCGLNoError);

        auto gpuID = gpuIDForDisplayMask(displayMask);

        if (gpuID == preferredGPUID) {
            error = CGLSetVirtualScreen(contextObj, virtualScreen);
            ASSERT(error == kCGLNoError);
            LOG(WebGL, "Context (%p) set to GPU with ID: (%lld).", contextObj, gpuID);
            return;
        }

        if (firstAcceleratedScreen < 0) {
            GLint isAccelerated = 0;
            error = CGLDescribePixelFormat(pixelFormatObj, virtualScreen, kCGLPFAAccelerated, &isAccelerated);
            ASSERT(error == kCGLNoError);
            if (isAccelerated)
                firstAcceleratedScreen = virtualScreen;
        }
    }

    // No registryID match found; set to first hardware-accelerated virtual screen.
    if (firstAcceleratedScreen >= 0) {
        error = CGLSetVirtualScreen(contextObj, firstAcceleratedScreen);
        ASSERT(error == kCGLNoError);
        LOG(WebGL, "RegistryID (%lld) not matched; Context (%p) set to virtual screen (%d).", preferredGPUID, contextObj, firstAcceleratedScreen);
    }
}

#endif // PLATFORM(MAC)

GraphicsContextGLOpenGL::GraphicsContextGLOpenGL(GraphicsContextGLAttributes attrs, HostWindow* hostWindow, GraphicsContextGL::Destination destination, GraphicsContextGLOpenGL* sharedContext)
    : GraphicsContextGL(attrs, destination, sharedContext)
{
    m_isForWebGL2 = attrs.isWebGL2;

#if HAVE(APPLE_GRAPHICS_CONTROL)
    m_powerPreferenceUsedForCreation = (hasLowAndHighPowerGPUs() && attrs.powerPreference == GraphicsContextGLPowerPreference::HighPerformance) ? GraphicsContextGLPowerPreference::HighPerformance : GraphicsContextGLPowerPreference::Default;
#else
    m_powerPreferenceUsedForCreation = GraphicsContextGLPowerPreference::Default;
#endif

    m_displayObj = InitializeEGLDisplay();
    if (m_displayObj == EGL_NO_DISPLAY)
        return;
    const char *displayExtensions = EGL_QueryString(m_displayObj, EGL_EXTENSIONS);
    LOG(WebGL, "Extensions: %s", displayExtensions);

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
    }
    // WebGL requires that all resources are cleared at creation.
    eglContextAttributes.append(EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE);
    eglContextAttributes.append(EGL_TRUE);
    // WebGL doesn't allow client arrays.
    eglContextAttributes.append(EGL_CONTEXT_CLIENT_ARRAYS_ENABLED_ANGLE);
    eglContextAttributes.append(EGL_FALSE);
    // WebGL doesn't allow implicit creation of objects on bind.
    eglContextAttributes.append(EGL_CONTEXT_BIND_GENERATES_RESOURCE_CHROMIUM);
    eglContextAttributes.append(EGL_FALSE);

    if (strstr(displayExtensions, "EGL_ANGLE_power_preference")) {
        eglContextAttributes.append(EGL_POWER_PREFERENCE_ANGLE);
        // EGL_LOW_POWER_ANGLE is the default. Change to
        // EGL_HIGH_POWER_ANGLE if desired.
        eglContextAttributes.append(EGL_LOW_POWER_ANGLE);
    }
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

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    ExtensionsGL& extensions = getExtensions();

    if (!needsEAGLOnMac()) {
        static constexpr const char* requiredExtensions[] = {
            "GL_ANGLE_texture_rectangle", // For IOSurface-backed textures.
            "GL_EXT_texture_format_BGRA8888", // For creating the EGL surface from an IOSurface.
        };

        for (size_t i = 0; i < WTF_ARRAY_LENGTH(requiredExtensions); ++i) {
            if (!extensions.supports(requiredExtensions[i])) {
                LOG(WebGL, "Missing required extension. %s", requiredExtensions[i]);
                return;
            }

            extensions.ensureEnabled(requiredExtensions[i]);
        }
    }
#endif // PLATFORM(MAC) || PLATFORM(MACCATALYST)

#if PLATFORM(MAC)
    // FIXME: It's unclear if MACCATALYST should take these steps as well, but that
    // would require the PlatformScreenMac code to be exposed to Catalyst too.
    EGLDeviceEXT device = nullptr;
    EGL_QueryDisplayAttribEXT(m_displayObj, EGL_DEVICE_EXT, reinterpret_cast<EGLAttrib*>(&device));
    CGLContextObj cglContext = nullptr;
    CGLPixelFormatObj pixelFormat = nullptr;
    EGL_QueryDeviceAttribEXT(device, EGL_CGL_CONTEXT_ANGLE, reinterpret_cast<EGLAttrib*>(&cglContext));
    EGL_QueryDeviceAttribEXT(device, EGL_CGL_PIXEL_FORMAT_ANGLE, reinterpret_cast<EGLAttrib*>(&pixelFormat));
    auto gpuID = (hostWindow && hostWindow->displayID()) ? gpuIDForDisplay(hostWindow->displayID()) : primaryGPUID();
    setGPUByRegistryID(cglContext, pixelFormat, gpuID);
#else
    UNUSED_PARAM(hostWindow);
#endif

    validateAttributes();
    attrs = contextAttributes(); // They may have changed during validation.

    // Create the WebGLLayer
    BEGIN_BLOCK_OBJC_EXCEPTIONS
        m_webGLLayer = adoptNS([[WebGLLayer alloc] initWithDevicePixelRatio:attrs.devicePixelRatio contentsOpaque:!attrs.alpha]);
#ifndef NDEBUG
        [m_webGLLayer setName:@"WebGL Layer"];
#endif
    END_BLOCK_OBJC_EXCEPTIONS

    // Create the texture that will be used for the framebuffer.
    GLenum textureTarget = IOSurfaceTextureTarget();

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

    if (m_contextObj) {
        GraphicsContextGLAttributes attrs = contextAttributes();
        makeContextCurrent(); // TODO: check result.
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

        if (m_displayBufferPbuffer) {
            EGL_ReleaseTexImage(m_displayObj, m_displayBufferPbuffer, EGL_BACK_BUFFER);
            EGL_DestroySurface(m_displayObj, m_displayBufferPbuffer);
        }
        auto recycledBuffer = [m_webGLLayer recycleBuffer];
        if (recycledBuffer.handle)
            EGL_DestroySurface(m_displayObj, recycledBuffer.handle);
        auto contentsHandle = [m_webGLLayer detachClient];
        if (contentsHandle)
            EGL_DestroySurface(m_displayObj, contentsHandle);

        EGL_MakeCurrent(m_displayObj, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        EGL_DestroyContext(m_displayObj, m_contextObj);
    }

    LOG(WebGL, "Destroyed a GraphicsContextGLOpenGL (%p).", this);
}

GCGLenum GraphicsContextGLOpenGL::IOSurfaceTextureTarget()
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

GCGLenum GraphicsContextGLOpenGL::IOSurfaceTextureTargetQuery()
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

GCGLint GraphicsContextGLOpenGL::EGLIOSurfaceTextureTarget()
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
    // ANGLE has an early out for case where nothing changes. Calling MakeCurrent
    // is important to set volatile platform context. See InitializeEGLDisplay().
    if (!EGL_MakeCurrent(m_displayObj, EGL_NO_SURFACE, EGL_NO_SURFACE, m_contextObj))
        return false;
    return true;
}

#if PLATFORM(IOS_FAMILY)
bool GraphicsContextGLOpenGL::releaseCurrentContext(ReleaseBehavior releaseBehavior)
{
    // At the moment this function is relevant only when web thread lock owns the GraphicsContextGLOpenGL current context.
    ASSERT(!WebCore::isInWebProcess());

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

        EGL_BindAPI(EGL_OPENGL_ES_API);
        EGL_MakeCurrent(m_displayObj, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
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
    if (m_powerPreferenceUsedForCreation == GraphicsContextGLPowerPreference::HighPerformance) {
        if (isVisible)
            GraphicsContextGLOpenGLManager::sharedManager().addContextRequiringHighPerformance(this);
        else
            GraphicsContextGLOpenGLManager::sharedManager().removeContextRequiringHighPerformance(this);
    }
}

#if PLATFORM(MAC)
void GraphicsContextGLOpenGL::updateCGLContext()
{
    if (!m_contextObj)
        return;

    LOG(WebGL, "Detected a mux switch or display reconfiguration. Call CGLUpdateContext. (%p)", this);

    makeContextCurrent();
    EGLDeviceEXT device = nullptr;
    EGL_QueryDisplayAttribEXT(m_displayObj, EGL_DEVICE_EXT, reinterpret_cast<EGLAttrib*>(&device));
    CGLContextObj cglContext = nullptr;
    EGL_QueryDeviceAttribEXT(device, EGL_CGL_CONTEXT_ANGLE, reinterpret_cast<EGLAttrib*>(&cglContext));

    CGLUpdateContext(cglContext);
    m_hasSwitchedToHighPerformanceGPU = true;
}
#endif

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
    auto recycledBuffer = [m_webGLLayer recycleBuffer];
    if (recycledBuffer.handle)
        EGL_DestroySurface(m_displayObj, recycledBuffer.handle);
    recycledBuffer.surface.reset();

    auto backing = WebCore::IOSurface::create(getInternalFramebufferSize(), WebCore::sRGBColorSpaceRef());
    if (!backing)
        return false;

    backing->migrateColorSpaceToProperties();

    const bool usingAlpha = contextAttributes().alpha;
    const auto size = getInternalFramebufferSize();
    const EGLint surfaceAttributes[] = {
        EGL_WIDTH, size.width(),
        EGL_HEIGHT, size.height(),
        EGL_IOSURFACE_PLANE_ANGLE, 0,
        EGL_TEXTURE_TARGET, WebCore::GraphicsContextGLOpenGL::EGLIOSurfaceTextureTarget(),
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
    GCGLenum textureTarget = IOSurfaceTextureTarget();
    ScopedRestoreTextureBinding restoreBinding(IOSurfaceTextureTargetQuery(), textureTarget, textureTarget != TEXTURE_RECTANGLE_ARB);
    gl::BindTexture(textureTarget, m_texture);
    if (!EGL_BindTexImage(m_displayObj, pbuffer, EGL_BACK_BUFFER)) {
        EGL_DestroySurface(m_displayObj, pbuffer);
        return false;
    }
    m_displayBufferPbuffer = pbuffer;
    m_displayBufferBacking = WTFMove(backing);
    return true;
}

bool GraphicsContextGLOpenGL::isGLES2Compliant() const
{
    return m_isForWebGL2;
}

void GraphicsContextGLOpenGL::simulateContextChanged()
{
    GraphicsContextGLOpenGLManager::sharedManager().updateAllContexts();
}

bool GraphicsContextGLOpenGL::allowOfflineRenderers() const
{
#if PLATFORM(MAC) && ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
    // When WindowServer access is blocked in the WebProcess, there is no way
    // for OpenGL to decide which GPU is connected to a display (online/offline).
    // OpenGL will then consider all GPUs, or renderers, as offline, which means
    // all offline renderers need to be considered when finding a pixel format.
    // In WebKit legacy, there will still be a WindowServer connection, and
    // m_displayMask will not be set in this case.
    if (primaryOpenGLDisplayMask())
        return true;
#elif PLATFORM(MACCATALYST)
    // FIXME: <rdar://53062794> We're very inconsistent about WEBPROCESS_WINDOWSERVER_BLOCKING
    // and MAC/MACCATALYST and OPENGL/OPENGLES.
    return true;
#endif
        
#if HAVE(APPLE_GRAPHICS_CONTROL)
    if (hasLowAndHighPowerGPUs())
        return true;
#endif
    
    return false;
}

#if PLATFORM(MAC)
void GraphicsContextGLOpenGL::screenDidChange(PlatformDisplayID displayID)
{
    if (!m_contextObj)
        return;
    if (!m_hasSwitchedToHighPerformanceGPU) {
        EGLDeviceEXT device = nullptr;
        EGL_QueryDisplayAttribEXT(m_displayObj, EGL_DEVICE_EXT, reinterpret_cast<EGLAttrib*>(&device));
        CGLContextObj cglContext = nullptr;
        CGLPixelFormatObj pixelFormat = nullptr;
        EGL_QueryDeviceAttribEXT(device, EGL_CGL_CONTEXT_ANGLE, reinterpret_cast<EGLAttrib*>(&cglContext));
        EGL_QueryDeviceAttribEXT(device, EGL_CGL_PIXEL_FORMAT_ANGLE, reinterpret_cast<EGLAttrib*>(&pixelFormat));
        setGPUByRegistryID(cglContext, pixelFormat, gpuIDForDisplay(displayID));
    }
}
#endif // !PLATFORM(MAC)

void GraphicsContextGLOpenGL::prepareForDisplay()
{
    if (m_layerComposited)
        return;
    if (!makeContextCurrent())
        return;
    prepareTextureImpl();

    // The IOSurface will be used from other graphics subsystem, so flush GL commands.
    gl::Flush();

    auto recycledBuffer = [m_webGLLayer recycleBuffer];

    EGL_ReleaseTexImage(m_displayObj, m_displayBufferPbuffer, EGL_BACK_BUFFER);
    [m_webGLLayer prepareForDisplayWithContents: {WTFMove(m_displayBufferBacking), m_displayBufferPbuffer}];
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
        reshapeDisplayBufferBacking();

    markLayerComposited();
}

}

#endif // ENABLE(GRAPHICS_CONTEXT_GL)
