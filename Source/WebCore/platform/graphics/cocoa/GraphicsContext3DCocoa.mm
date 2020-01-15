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

#include "config.h"

#if ENABLE(GRAPHICS_CONTEXT_3D)
#import "GraphicsContext3D.h"

#if PLATFORM(IOS_FAMILY) && !USE(ANGLE)
#import "GraphicsContext3DIOS.h"
#endif

#import "CanvasRenderingContext.h"
#import "GraphicsContext.h"
#import "GraphicsContext3DManager.h"
#import "HTMLCanvasElement.h"
#import "HostWindow.h"
#import "ImageBuffer.h"
#import "Logging.h"
#import "WebGLLayer.h"
#import "WebGLObject.h"
#import "WebGLRenderingContextBase.h"
#import <CoreGraphics/CGBitmapContext.h>
#import <sys/sysctl.h>
#import <sysexits.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/text/CString.h>

#if USE(OPENGL_ES)
#import <OpenGLES/EAGL.h>
#import <OpenGLES/EAGLDrawable.h>
#import <OpenGLES/EAGLIOSurface.h>
#import <OpenGLES/ES2/glext.h>
#import <QuartzCore/QuartzCore.h>
#import <pal/spi/ios/OpenGLESSPI.h>
#elif USE(OPENGL)
#import <IOKit/IOKitLib.h>
#import <OpenGL/gl.h>
#elif USE(ANGLE)
#define EGL_EGL_PROTOTYPES 0
// Skip the inclusion of ANGLE's explicit context entry points for now.
#define GL_ANGLE_explicit_context
#define GL_ANGLE_explicit_context_gles1
typedef void* GLeglContext;
#include <ANGLE/egl.h>
#include <ANGLE/eglext.h>
#include <ANGLE/eglext_angle.h>
#include <ANGLE/entry_points_egl.h>
#include <ANGLE/entry_points_egl_ext.h>
#include <ANGLE/entry_points_gles_2_0_autogen.h>
#include <ANGLE/entry_points_gles_ext_autogen.h>
#include <ANGLE/gl2ext.h>
#include <ANGLE/gl2ext_angle.h>
#endif

#if USE(OPENGL_ES) || USE(OPENGL)
#include "Extensions3DOpenGL.h"
#elif USE(ANGLE)
#include "Extensions3DANGLE.h"
#endif

#if PLATFORM(MAC)
#import "ScreenProperties.h"
#if USE(OPENGL) || USE(ANGLE)
#import <OpenGL/CGLRenderers.h>
#endif
#endif

namespace WebCore {

static const unsigned statusCheckThreshold = 5;

// FIXME: This class is currently empty on Mac, but will get populated as
// the restructuring in https://bugs.webkit.org/show_bug.cgi?id=66903 is done
class GraphicsContext3DPrivate {
    WTF_MAKE_FAST_ALLOCATED;
public:
    GraphicsContext3DPrivate(GraphicsContext3D*) { }
    
    ~GraphicsContext3DPrivate() { }
};

RefPtr<GraphicsContext3D> GraphicsContext3D::create(GraphicsContext3DAttributes attrs, HostWindow* hostWindow, GraphicsContext3D::Destination destination)
{
    // This implementation doesn't currently support rendering directly to the HostWindow.
    if (destination == Destination::DirectlyToHostWindow)
        return nullptr;

    // Make space for the incoming context if we're full.
    GraphicsContext3DManager::sharedManager().recycleContextIfNecessary();
    if (GraphicsContext3DManager::sharedManager().hasTooManyContexts())
        return nullptr;

    RefPtr<GraphicsContext3D> context = adoptRef(new GraphicsContext3D(attrs, hostWindow, destination));

    if (!context->m_contextObj)
        return nullptr;

    GraphicsContext3DManager::sharedManager().addContext(context.get(), hostWindow);

    return context;
}

Ref<GraphicsContext3D> GraphicsContext3D::createShared(GraphicsContext3D& sharedContext)
{
    auto hostWindow = GraphicsContext3DManager::sharedManager().hostWindowForContext(&sharedContext);
    auto context = adoptRef(*new GraphicsContext3D(sharedContext.contextAttributes(), hostWindow, sharedContext.destination(), &sharedContext));

    GraphicsContext3DManager::sharedManager().addContext(context.ptr(), hostWindow);

    return context;
}

#if PLATFORM(MAC) && (USE(OPENGL) || USE(ANGLE)) // FIXME: This probably should be just (USE(OPENGL) || USE(ANGLE)) - see <rdar://53062794>.

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

#endif // PLATFORM(MAC) && (USE(OPENGL) || USE(ANGLE))

GraphicsContext3D::GraphicsContext3D(GraphicsContext3DAttributes attrs, HostWindow* hostWindow, GraphicsContext3D::Destination destination, GraphicsContext3D* sharedContext)
    : GraphicsContext3DBase(attrs, destination, sharedContext)
    , m_private(makeUnique<GraphicsContext3DPrivate>(this))
{
    m_isForWebGL2 = attrs.isWebGL2;

#if HAVE(APPLE_GRAPHICS_CONTROL)
    m_powerPreferenceUsedForCreation = (hasLowAndHighPowerGPUs() && attrs.powerPreference == GraphicsContext3DPowerPreference::HighPerformance) ? GraphicsContext3DPowerPreference::HighPerformance : GraphicsContext3DPowerPreference::Default;
#else
    m_powerPreferenceUsedForCreation = GraphicsContext3DPowerPreference::Default;
#endif

#if !USE(ANGLE)
#if USE(OPENGL_ES)
    if (m_isForWebGL2)
        m_compiler = ANGLEWebKitBridge(SH_ESSL_OUTPUT, SH_WEBGL2_SPEC);
    else
        m_compiler = ANGLEWebKitBridge(SH_ESSL_OUTPUT);
#else
    if (m_isForWebGL2)
        m_compiler = ANGLEWebKitBridge(SH_GLSL_410_CORE_OUTPUT, SH_WEBGL2_SPEC);
#endif // USE(OPENGL_ES)
#endif // !USE(ANGLE)

#if USE(OPENGL_ES)
    UNUSED_PARAM(hostWindow);
    EAGLRenderingAPI api = m_isForWebGL2 ? kEAGLRenderingAPIOpenGLES3 : kEAGLRenderingAPIOpenGLES2;
    EAGLContext *contextObj;
    if (!sharedContext)
        contextObj = [[EAGLContext alloc] initWithAPI:api];
    else
        contextObj = [[EAGLContext alloc] initWithAPI:api sharegroup:static_cast<EAGLContext *>(sharedContext->m_contextObj).sharegroup];

    m_contextObj = static_cast<void *>(contextObj);

    makeContextCurrent();

    if (m_isForWebGL2)
        ::glEnable(GraphicsContext3D::PRIMITIVE_RESTART_FIXED_INDEX);
#elif USE(OPENGL)

    bool useMultisampling = attrs.antialias;

    Vector<CGLPixelFormatAttribute> attribs;
    CGLPixelFormatObj pixelFormatObj = 0;
    GLint numPixelFormats = 0;

    attribs.append(kCGLPFAAccelerated);
    attribs.append(kCGLPFAColorSize);
    attribs.append(static_cast<CGLPixelFormatAttribute>(32));
    attribs.append(kCGLPFADepthSize);
    attribs.append(static_cast<CGLPixelFormatAttribute>(32));

    // This attribute, while mentioning offline renderers, is actually
    // allowing us to request the integrated graphics on a dual GPU
    // system, and not force the discrete GPU.
    // See https://developer.apple.com/library/mac/technotes/tn2229/_index.html
    if (allowOfflineRenderers())
        attribs.append(kCGLPFAAllowOfflineRenderers);

    if (useMultisampling) {
        attribs.append(kCGLPFAMultisample);
        attribs.append(kCGLPFASampleBuffers);
        attribs.append(static_cast<CGLPixelFormatAttribute>(1));
        attribs.append(kCGLPFASamples);
        attribs.append(static_cast<CGLPixelFormatAttribute>(4));
    }

    if (m_isForWebGL2) {
        // FIXME: Instead of backing a WebGL2 GraphicsContext3D with a OpenGL 4 context, we should instead back it with ANGLE.
        // Use an OpenGL 4 context for now until the ANGLE backend is ready.
        attribs.append(kCGLPFAOpenGLProfile);
        attribs.append(static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_GL4_Core));
    }

    attribs.append(static_cast<CGLPixelFormatAttribute>(0));

    CGLChoosePixelFormat(attribs.data(), &pixelFormatObj, &numPixelFormats);

    if (!numPixelFormats)
        return;

    CGLContextObj cglContext = nullptr;
    CGLContextObj sharedCGLContext = sharedContext ? static_cast<CGLContextObj>(sharedContext->m_contextObj) : nullptr;

    CGLError err = CGLCreateContext(pixelFormatObj, sharedCGLContext, &cglContext);
    GLint abortOnBlacklist = 0;
    CGLSetParameter(cglContext, kCGLCPAbortOnGPURestartStatusBlacklisted, &abortOnBlacklist);

#if PLATFORM(MAC) // FIXME: This probably should be USE(OPENGL) - see <rdar://53062794>.

    auto gpuID = (hostWindow && hostWindow->displayID()) ? gpuIDForDisplay(hostWindow->displayID()) : primaryGPUID();
    setGPUByRegistryID(cglContext, pixelFormatObj, gpuID);

#else
    UNUSED_PARAM(hostWindow);
#endif

    CGLDestroyPixelFormat(pixelFormatObj);

    m_contextObj = cglContext;

    if (err != kCGLNoError || !m_contextObj) {
        // We were unable to create the context.
        m_contextObj = 0;
        return;
    }

    m_contextObj = cglContext;

    CGLSetCurrentContext(cglContext);

    // WebGL 2 expects ES 3-only PRIMITIVE_RESTART_FIXED_INDEX to be enabled; we must emulate this on non-ES 3 systems.
    if (m_isForWebGL2)
        ::glEnable(GraphicsContext3D::PRIMITIVE_RESTART);

#elif USE(ANGLE)

    m_displayObj = EGL_GetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_displayObj == EGL_NO_DISPLAY)
        return;
    EGLint majorVersion, minorVersion;
    if (EGL_Initialize(m_displayObj, &majorVersion, &minorVersion) == EGL_FALSE) {
        LOG(WebGL, "EGLDisplay Initialization failed.");
        return;
    }
    LOG(WebGL, "ANGLE initialised Major: %d Minor: %d", majorVersion, minorVersion);
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
    eglContextAttributes.append(EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE);
    eglContextAttributes.append(EGL_TRUE);

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
        gl::Enable(GraphicsContext3D::PRIMITIVE_RESTART_FIXED_INDEX);

#if PLATFORM(MAC)
    Extensions3D& extensions = getExtensions();

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
#endif // PLATFORM(MAC)

#endif // #elif USE(ANGLE)

    validateAttributes();
    attrs = contextAttributes(); // They may have changed during validation.

    // Create the WebGLLayer
    BEGIN_BLOCK_OBJC_EXCEPTIONS
        m_webGLLayer = adoptNS([[WebGLLayer alloc] initWithGraphicsContext3D:this]);
#ifndef NDEBUG
        [m_webGLLayer setName:@"WebGL Layer"];
#endif
#if USE(ANGLE)
        [m_webGLLayer setEGLDisplay:m_displayObj config:m_configObj];
#endif
    END_BLOCK_OBJC_EXCEPTIONS

#if USE(OPENGL)
    if (useMultisampling)
        ::glEnable(GL_MULTISAMPLE);
#endif

    // Create the texture that will be used for the framebuffer.
#if USE(OPENGL_ES)
    ::glGenRenderbuffers(1, &m_texture);
#elif USE(OPENGL)
    ::glGenTextures(1, &m_texture);
    // We bind to GL_TEXTURE_RECTANGLE_EXT rather than TEXTURE_2D because
    // that's what is required for a texture backed by IOSurface.
    ::glBindTexture(GL_TEXTURE_RECTANGLE_EXT, m_texture);
    ::glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    ::glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ::glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    ::glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    ::glBindTexture(GL_TEXTURE_RECTANGLE_EXT, 0);

#elif USE(ANGLE)

#if PLATFORM(MAC)
    GLenum textureType = GL_TEXTURE_RECTANGLE_ANGLE;
#else
    GLenum textureType = GL_TEXTURE_2D;
#endif

    gl::GenTextures(1, &m_texture);
    gl::BindTexture(textureType, m_texture);
    gl::TexParameteri(textureType, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl::TexParameteri(textureType, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl::TexParameteri(textureType, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(textureType, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::BindTexture(textureType, 0);
#else
#error Unsupported configuration
#endif

#if USE(OPENGL) || USE(OPENGL_ES)
    // Create the framebuffer object.
    ::glGenFramebuffersEXT(1, &m_fbo);
    ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);

    m_state.boundFBO = m_fbo;
    if (!attrs.antialias && (attrs.stencil || attrs.depth))
        ::glGenRenderbuffersEXT(1, &m_depthStencilBuffer);

    // If necessary, create another framebuffer for the multisample results.
    if (attrs.antialias) {
        ::glGenFramebuffersEXT(1, &m_multisampleFBO);
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_multisampleFBO);
        m_state.boundFBO = m_multisampleFBO;
        ::glGenRenderbuffersEXT(1, &m_multisampleColorBuffer);
        if (attrs.stencil || attrs.depth)
            ::glGenRenderbuffersEXT(1, &m_multisampleDepthStencilBuffer);
    }
#elif USE(ANGLE)
    gl::GenFramebuffers(1, &m_fbo);
    gl::BindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    m_state.boundFBO = m_fbo;

    if (!attrs.antialias && (attrs.stencil || attrs.depth))
        gl::GenRenderbuffers(1, &m_depthStencilBuffer);

    // If necessary, create another framebuffer for the multisample results.
    if (attrs.antialias) {
        gl::GenFramebuffers(1, &m_multisampleFBO);
        gl::BindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
        m_state.boundFBO = m_multisampleFBO;
        gl::GenRenderbuffers(1, &m_multisampleColorBuffer);
        if (attrs.stencil || attrs.depth)
            gl::GenRenderbuffers(1, &m_multisampleDepthStencilBuffer);
    }

#endif // USE(ANGLE)

#if !USE(ANGLE)
    // ANGLE shader compiler initialization.

    ShBuiltInResources ANGLEResources;
    sh::InitBuiltInResources(&ANGLEResources);

    getIntegerv(GraphicsContext3D::MAX_VERTEX_ATTRIBS, &ANGLEResources.MaxVertexAttribs);
    getIntegerv(GraphicsContext3D::MAX_VERTEX_UNIFORM_VECTORS, &ANGLEResources.MaxVertexUniformVectors);
    getIntegerv(GraphicsContext3D::MAX_VARYING_VECTORS, &ANGLEResources.MaxVaryingVectors);
    getIntegerv(GraphicsContext3D::MAX_VERTEX_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxVertexTextureImageUnits);
    getIntegerv(GraphicsContext3D::MAX_COMBINED_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxCombinedTextureImageUnits);
    getIntegerv(GraphicsContext3D::MAX_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxTextureImageUnits);
    getIntegerv(GraphicsContext3D::MAX_FRAGMENT_UNIFORM_VECTORS, &ANGLEResources.MaxFragmentUniformVectors);

    // Always set to 1 for OpenGL ES.
    ANGLEResources.MaxDrawBuffers = 1;
    
    GC3Dint range[2], precision;
    getShaderPrecisionFormat(GraphicsContext3D::FRAGMENT_SHADER, GraphicsContext3D::HIGH_FLOAT, range, &precision);
    ANGLEResources.FragmentPrecisionHigh = (range[0] || range[1] || precision);

    m_compiler.setResources(ANGLEResources);
#endif // !USE(ANGLE)
    
#if USE(OPENGL)
    ::glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    if (!isGLES2Compliant())
        ::glEnable(GL_POINT_SPRITE);
#endif

#if USE(OPENGL) || USE(OPENGL_ES)
    ::glClearColor(0, 0, 0, 0);
#elif USE(ANGLE)
    gl::ClearColor(0, 0, 0, 0);
#endif

    LOG(WebGL, "Created a GraphicsContext3D (%p).", this);
}

GraphicsContext3D::~GraphicsContext3D()
{
    GraphicsContext3DManager::sharedManager().removeContext(this);

    if (m_contextObj) {
        GraphicsContext3DAttributes attrs = contextAttributes();

#if USE(OPENGL_ES)
        makeContextCurrent();
        [static_cast<EAGLContext *>(m_contextObj) renderbufferStorage:GL_RENDERBUFFER fromDrawable:nil];
        ::glDeleteRenderbuffers(1, &m_texture);
#elif USE(OPENGL)
        CGLContextObj cglContext = static_cast<CGLContextObj>(m_contextObj);
        CGLSetCurrentContext(cglContext);
        ::glDeleteTextures(1, &m_texture);
#elif USE(ANGLE)
        EGL_MakeCurrent(m_displayObj, EGL_NO_SURFACE, EGL_NO_SURFACE, m_contextObj);
        gl::DeleteTextures(1, &m_texture);
#endif

#if USE(OPENGL) || USE(OPENGL_ES)
        if (attrs.antialias) {
            ::glDeleteRenderbuffersEXT(1, &m_multisampleColorBuffer);
            if (attrs.stencil || attrs.depth)
                ::glDeleteRenderbuffersEXT(1, &m_multisampleDepthStencilBuffer);
            ::glDeleteFramebuffersEXT(1, &m_multisampleFBO);
        } else {
            if (attrs.stencil || attrs.depth)
                ::glDeleteRenderbuffersEXT(1, &m_depthStencilBuffer);
        }
        ::glDeleteFramebuffersEXT(1, &m_fbo);
#elif USE(ANGLE)
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
#endif

#if USE(OPENGL_ES)
        [EAGLContext setCurrentContext:0];
        [static_cast<EAGLContext *>(m_contextObj) release];
#elif USE(OPENGL)
        CGLSetCurrentContext(0);
        CGLDestroyContext(cglContext);
#elif USE(ANGLE)
        EGL_MakeCurrent(m_displayObj, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        EGL_DestroyContext(m_displayObj, m_contextObj);
#endif
        [m_webGLLayer setContext:nullptr];
    }

    LOG(WebGL, "Destroyed a GraphicsContext3D (%p).", this);
}

#if USE(OPENGL_ES)
void GraphicsContext3D::setRenderbufferStorageFromDrawable(GC3Dsizei width, GC3Dsizei height)
{
    // We need to make a call to setBounds below to update the backing store size but we also
    // do not want to clobber the bounds set during layout.
    CGRect previousBounds = [m_webGLLayer.get() bounds];

    [m_webGLLayer setBounds:CGRectMake(0, 0, width, height)];
    [m_webGLLayer setOpaque:!contextAttributes().alpha];

    [static_cast<EAGLContext *>(m_contextObj) renderbufferStorage:GL_RENDERBUFFER fromDrawable:static_cast<id<EAGLDrawable>>(m_webGLLayer.get())];

    [m_webGLLayer setBounds:previousBounds];
}
#endif

bool GraphicsContext3D::makeContextCurrent()
{
    if (!m_contextObj)
        return false;

#if USE(OPENGL_ES)
    if ([EAGLContext currentContext] != m_contextObj)
        return [EAGLContext setCurrentContext:static_cast<EAGLContext*>(m_contextObj)];
#elif USE(OPENGL)
    CGLContextObj currentContext = CGLGetCurrentContext();
    if (currentContext != m_contextObj)
        return CGLSetCurrentContext(static_cast<CGLContextObj>(m_contextObj)) == kCGLNoError;
#elif USE(ANGLE)
    if (EGL_GetCurrentContext() != m_contextObj)
        return EGL_MakeCurrent(m_displayObj, EGL_NO_SURFACE, EGL_NO_SURFACE, m_contextObj);
#endif
    return true;
}

void GraphicsContext3D::checkGPUStatus()
{
    if (m_failNextStatusCheck) {
        LOG(WebGL, "Pretending the GPU has reset (%p). Lose the context.", this);
        m_failNextStatusCheck = false;
        forceContextLost();
#if USE(OPENGL)
        CGLSetCurrentContext(0);
#elif USE(OPENGL_ES)
        [EAGLContext setCurrentContext:0];
#elif USE(ANGLE)
        EGL_MakeCurrent(m_displayObj, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
#endif
        return;
    }

    // Only do the check every statusCheckThreshold calls.
    if (m_statusCheckCount)
        return;

    m_statusCheckCount = (m_statusCheckCount + 1) % statusCheckThreshold;

    GLint restartStatus = 0;
#if USE(OPENGL)
    CGLContextObj cglContext = static_cast<CGLContextObj>(platformGraphicsContext3D());
    CGLGetParameter(cglContext, kCGLCPGPURestartStatus, &restartStatus);
    if (restartStatus == kCGLCPGPURestartStatusBlacklisted) {
        LOG(WebGL, "The GPU has blacklisted us (%p). Terminating.", this);
        exit(EX_OSERR);
    }
    if (restartStatus == kCGLCPGPURestartStatusCaused) {
        LOG(WebGL, "The GPU has reset us (%p). Lose the context.", this);
        forceContextLost();
        CGLSetCurrentContext(0);
    }
#elif USE(OPENGL_ES)
    EAGLContext* currentContext = static_cast<EAGLContext*>(PlatformGraphicsContext3D());
    [currentContext getParameter:kEAGLCPGPURestartStatus to:&restartStatus];
    if (restartStatus == kEAGLCPGPURestartStatusCaused || restartStatus == kEAGLCPGPURestartStatusBlacklisted) {
        LOG(WebGL, "The GPU has either reset or blacklisted us (%p). Lose the context.", this);
        forceContextLost();
        [EAGLContext setCurrentContext:0];
    }
#elif USE(ANGLE)
    // FIXME: check via KHR_robustness.
    restartStatus = 0;
#endif
}

#if USE(OPENGL_ES)
void GraphicsContext3D::presentRenderbuffer()
{
    makeContextCurrent();
    if (contextAttributes().antialias)
        resolveMultisamplingIfNecessary();

    ::glFlush();
    ::glBindRenderbuffer(GL_RENDERBUFFER, m_texture);
    [static_cast<EAGLContext*>(m_contextObj) presentRenderbuffer:GL_RENDERBUFFER];
    [EAGLContext setCurrentContext:nil];
}
#endif

bool GraphicsContext3D::texImageIOSurface2D(GC3Denum target, GC3Denum internalFormat, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, IOSurfaceRef surface, GC3Duint plane)
{
#if USE(OPENGL)
    CGLContextObj cglContext = static_cast<CGLContextObj>(platformGraphicsContext3D());
    return kCGLNoError == CGLTexImageIOSurface2D(cglContext, target, internalFormat, width, height, format, type, surface, plane);
#elif USE(OPENGL_ES) && !PLATFORM(IOS_FAMILY_SIMULATOR)
    return [static_cast<EAGLContext *>(platformGraphicsContext3D()) texImageIOSurface:surface target:target internalFormat:internalFormat width:width height:height format:format type:type plane:plane];
#else
    UNUSED_PARAM(target);
    UNUSED_PARAM(internalFormat);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    UNUSED_PARAM(surface);
    UNUSED_PARAM(plane);
    return false;
#endif
}

#if USE(OPENGL)
void GraphicsContext3D::allocateIOSurfaceBackingStore(IntSize size)
{
    LOG(WebGL, "GraphicsContext3D::allocateIOSurfaceBackingStore at %d x %d. (%p)", size.width(), size.height(), this);
    [m_webGLLayer allocateIOSurfaceBackingStoreWithSize:size usingAlpha:contextAttributes().alpha];
}

void GraphicsContext3D::updateFramebufferTextureBackingStoreFromLayer()
{
    LOG(WebGL, "GraphicsContext3D::updateFramebufferTextureBackingStoreFromLayer(). (%p)", this);
    [m_webGLLayer bindFramebufferToNextAvailableSurface];
}

void GraphicsContext3D::updateCGLContext()
{
    if (!m_contextObj)
        return;

    LOG(WebGL, "Detected a mux switch or display reconfiguration. Call CGLUpdateContext. (%p)", this);

    makeContextCurrent();
    CGLUpdateContext(static_cast<CGLContextObj>(m_contextObj));
    m_hasSwitchedToHighPerformanceGPU = true;
}

#endif // USE(OPENGL)

#if USE(OPENGL) || USE(ANGLE)
void GraphicsContext3D::setContextVisibility(bool isVisible)
{
    if (m_powerPreferenceUsedForCreation == GraphicsContext3DPowerPreference::HighPerformance) {
        if (isVisible)
            GraphicsContext3DManager::sharedManager().addContextRequiringHighPerformance(this);
        else
            GraphicsContext3DManager::sharedManager().removeContextRequiringHighPerformance(this);
    }
}
#endif // USE(OPENGL) || USE(ANGLE)

#if USE(ANGLE)

#if PLATFORM(MAC)
void GraphicsContext3D::updateCGLContext()
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

void GraphicsContext3D::allocateIOSurfaceBackingStore(IntSize size)
{
#if HAVE(IOSURFACE)
    LOG(WebGL, "GraphicsContext3D::allocateIOSurfaceBackingStore at %d x %d. (%p)", size.width(), size.height(), this);
    [m_webGLLayer allocateIOSurfaceBackingStoreWithSize:size usingAlpha:contextAttributes().alpha];
#else
    UNUSED_PARAM(size);
#endif
}

void GraphicsContext3D::updateFramebufferTextureBackingStoreFromLayer()
{
#if HAVE(IOSURFACE)
    LOG(WebGL, "GraphicsContext3D::updateFramebufferTextureBackingStoreFromLayer(). (%p)", this);
    [m_webGLLayer bindFramebufferToNextAvailableSurface];
#endif
}
#endif // USE(ANGLE)

bool GraphicsContext3D::isGLES2Compliant() const
{
    return m_isForWebGL2;
}

void GraphicsContext3D::simulateContextChanged()
{
    GraphicsContext3DManager::sharedManager().updateAllContexts();
}

bool GraphicsContext3D::allowOfflineRenderers() const
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
void GraphicsContext3D::screenDidChange(PlatformDisplayID displayID)
{
    if (!m_contextObj)
        return;
#if USE(ANGLE)
    if (!m_hasSwitchedToHighPerformanceGPU) {
        EGLDeviceEXT device = nullptr;
        EGL_QueryDisplayAttribEXT(m_displayObj, EGL_DEVICE_EXT, reinterpret_cast<EGLAttrib*>(&device));
        CGLContextObj cglContext = nullptr;
        CGLPixelFormatObj pixelFormat = nullptr;
        EGL_QueryDeviceAttribEXT(device, EGL_CGL_CONTEXT_ANGLE, reinterpret_cast<EGLAttrib*>(&cglContext));
        EGL_QueryDeviceAttribEXT(device, EGL_CGL_PIXEL_FORMAT_ANGLE, reinterpret_cast<EGLAttrib*>(&pixelFormat));
        setGPUByRegistryID(cglContext, pixelFormat, gpuIDForDisplay(displayID));
    }
#else
#if USE(OPENGL)
    if (!m_hasSwitchedToHighPerformanceGPU) {
        CGLContextObj cglContext = static_cast<CGLContextObj>(m_contextObj);
        setGPUByRegistryID(cglContext, CGLGetPixelFormat(cglContext), gpuIDForDisplay(displayID));
    }
#endif
#endif // USE(ANGLE)
}
#endif // !PLATFORM(MAC)

}

#endif // ENABLE(GRAPHICS_CONTEXT_3D)
