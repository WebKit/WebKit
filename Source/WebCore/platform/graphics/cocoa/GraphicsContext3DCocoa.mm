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

#if PLATFORM(IOS_FAMILY)
#import "GraphicsContext3DIOS.h"
#endif

#import "CanvasRenderingContext.h"
#import "Extensions3DOpenGL.h"
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
#else
#import <IOKit/IOKitLib.h>
#import <OpenGL/CGLRenderers.h>
#import <OpenGL/gl.h>
#endif

#if PLATFORM(MAC)
#import "ScreenProperties.h"
#endif

namespace WebCore {

static const unsigned statusCheckThreshold = 5;

// FIXME: This class is currently empty on Mac, but will get populated as
// the restructuring in https://bugs.webkit.org/show_bug.cgi?id=66903 is done
class GraphicsContext3DPrivate {
public:
    GraphicsContext3DPrivate(GraphicsContext3D*) { }
    
    ~GraphicsContext3DPrivate() { }
};

#if USE(OPENGL)

static void setPixelFormat(Vector<CGLPixelFormatAttribute>& attribs, int colorBits, int depthBits, bool accelerated, bool supersample, bool closest, bool antialias, bool isWebGL2, bool allowOfflineRenderers)
{
    attribs.clear();
    
    attribs.append(kCGLPFAColorSize);
    attribs.append(static_cast<CGLPixelFormatAttribute>(colorBits));
    attribs.append(kCGLPFADepthSize);
    attribs.append(static_cast<CGLPixelFormatAttribute>(depthBits));

    // This attribute, while mentioning offline renderers, is actually
    // allowing us to request the integrated graphics on a dual GPU
    // system, and not force the discrete GPU.
    // See https://developer.apple.com/library/mac/technotes/tn2229/_index.html
    if (allowOfflineRenderers)
        attribs.append(kCGLPFAAllowOfflineRenderers);

    if (accelerated)
        attribs.append(kCGLPFAAccelerated);
    else {
        attribs.append(kCGLPFARendererID);
        attribs.append(static_cast<CGLPixelFormatAttribute>(kCGLRendererGenericFloatID));
    }
        
    if (supersample && !antialias)
        attribs.append(kCGLPFASupersample);

    if (closest)
        attribs.append(kCGLPFAClosestPolicy);

    if (antialias) {
        attribs.append(kCGLPFAMultisample);
        attribs.append(kCGLPFASampleBuffers);
        attribs.append(static_cast<CGLPixelFormatAttribute>(1));
        attribs.append(kCGLPFASamples);
        attribs.append(static_cast<CGLPixelFormatAttribute>(4));
    }

    if (isWebGL2) {
        // FIXME: Instead of backing a WebGL2 GraphicsContext3D with a OpenGL 4 context, we should instead back it with ANGLE.
        // Use an OpenGL 4 context for now until the ANGLE backend is ready.
        attribs.append(kCGLPFAOpenGLProfile);
        attribs.append(static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_GL4_Core));
    }
        
    attribs.append(static_cast<CGLPixelFormatAttribute>(0));
}

#endif // USE(OPENGL)

RefPtr<GraphicsContext3D> GraphicsContext3D::create(GraphicsContext3DAttributes attrs, HostWindow* hostWindow, GraphicsContext3D::RenderStyle renderStyle)
{
    // This implementation doesn't currently support rendering directly to the HostWindow.
    if (renderStyle == RenderDirectlyToHostWindow)
        return nullptr;

    // Make space for the incoming context if we're full.
    GraphicsContext3DManager::sharedManager().recycleContextIfNecessary();
    if (GraphicsContext3DManager::sharedManager().hasTooManyContexts())
        return nullptr;

    RefPtr<GraphicsContext3D> context = adoptRef(new GraphicsContext3D(attrs, hostWindow, renderStyle));

    if (!context->m_contextObj)
        return nullptr;

    GraphicsContext3DManager::sharedManager().addContext(context.get(), hostWindow);

    return context;
}

Ref<GraphicsContext3D> GraphicsContext3D::createShared(GraphicsContext3D& sharedContext)
{
    auto hostWindow = GraphicsContext3DManager::sharedManager().hostWindowForContext(&sharedContext);
    auto context = adoptRef(*new GraphicsContext3D(sharedContext.getContextAttributes(), hostWindow, sharedContext.m_renderStyle, &sharedContext));

    GraphicsContext3DManager::sharedManager().addContext(context.ptr(), hostWindow);

    return context;
}

#if PLATFORM(MAC)

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
static void setGPUByRegistryID(PlatformGraphicsContext3D contextObj, CGLPixelFormatObj pixelFormatObj, IORegistryGPUID preferredGPUID)
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
#else // __MAC_OS_X_VERSION_MIN_REQUIRED < 101300
static void setGPUByDisplayMask(PlatformGraphicsContext3D contextObj, CGLPixelFormatObj pixelFormatObj, uint32_t preferredDisplayMask)
{
    // A common case for multiple GPUs, external GPUs, is not supported before macOS 10.13.4.
    // In the rarer case where there are still multiple displays plugged into multiple GPUs, this should still work.
    // See code example at https://developer.apple.com/library/content/technotes/tn2229/_index.html#//apple_ref/doc/uid/DTS40008924-CH1-SUBSECTION7
    // FIXME: Window server is not blocked before 10.14. There might be a more straightforward way to detect the correct GPU.

    if (!contextObj || !preferredDisplayMask)
        return;

    GLint virtualScreenCount = 0;
    CGLError error = CGLDescribePixelFormat(pixelFormatObj, 0, kCGLPFAVirtualScreenCount, &virtualScreenCount);
    ASSERT(error == kCGLNoError);

    for (GLint virtualScreen = 0; virtualScreen < virtualScreenCount; ++virtualScreen) {
        GLint displayMask = 0;
        error = CGLDescribePixelFormat(pixelFormatObj, virtualScreen, kCGLPFADisplayMask, &displayMask);
        ASSERT(error == kCGLNoError);
        if (error != kCGLNoError)
            continue;

        if (displayMask & preferredDisplayMask) {
            error = CGLSetVirtualScreen(contextObj, virtualScreen);
            ASSERT(error == kCGLNoError);
            return;
        }
    }
}
#endif

#endif // !PLATFORM(MAC)

GraphicsContext3D::GraphicsContext3D(GraphicsContext3DAttributes attrs, HostWindow* hostWindow, GraphicsContext3D::RenderStyle, GraphicsContext3D* sharedContext)
    : m_attrs(attrs)
    , m_private(std::make_unique<GraphicsContext3DPrivate>(this))
{
#if PLATFORM(IOS_FAMILY)
    if (m_attrs.isWebGL2)
        m_compiler = ANGLEWebKitBridge(SH_ESSL_OUTPUT, SH_WEBGL2_SPEC);
    else
        m_compiler = ANGLEWebKitBridge(SH_ESSL_OUTPUT);
#else
    if (m_attrs.isWebGL2)
        m_compiler = ANGLEWebKitBridge(SH_GLSL_410_CORE_OUTPUT, SH_WEBGL2_SPEC);
#endif

#if USE(OPENGL_ES)
    UNUSED_PARAM(hostWindow);
    EAGLRenderingAPI api = m_attrs.isWebGL2 ? kEAGLRenderingAPIOpenGLES3 : kEAGLRenderingAPIOpenGLES2;
    if (!sharedContext)
        m_contextObj = [[EAGLContext alloc] initWithAPI:api];
    else
        m_contextObj = [[EAGLContext alloc] initWithAPI:api sharegroup:sharedContext->m_contextObj.sharegroup];
    makeContextCurrent();

    if (m_attrs.isWebGL2)
        ::glEnable(GraphicsContext3D::PRIMITIVE_RESTART_FIXED_INDEX);
#else
    Vector<CGLPixelFormatAttribute> attribs;
    CGLPixelFormatObj pixelFormatObj = 0;
    GLint numPixelFormats = 0;
    
    // If we're configured to demand the software renderer, we'll
    // do so. We attempt to create contexts in this order:
    //
    // 1) 32 bit RGBA/32 bit depth/supersampled
    // 2) 32 bit RGBA/32 bit depth
    // 3) 32 bit RGBA/16 bit depth
    //
    // If we were not forced into software mode already, our final attempt is
    // to try that:
    //
    // 4) closest to 32 bit RGBA/16 bit depth/software renderer
    //
    // If none of that works, we simply fail and set m_contextObj to 0.

    bool useMultisampling = m_attrs.antialias;

#if HAVE(APPLE_GRAPHICS_CONTROL)
    m_powerPreferenceUsedForCreation = (hasLowAndHighPowerGPUs() && attrs.powerPreference == GraphicsContext3DPowerPreference::HighPerformance) ? GraphicsContext3DPowerPreference::HighPerformance : GraphicsContext3DPowerPreference::Default;
#else
    m_powerPreferenceUsedForCreation = GraphicsContext3DPowerPreference::Default;
#endif

    setPixelFormat(attribs, 32, 32, !attrs.forceSoftwareRenderer, true, false, useMultisampling, attrs.isWebGL2, allowOfflineRenderers());
    CGLChoosePixelFormat(attribs.data(), &pixelFormatObj, &numPixelFormats);

    if (!numPixelFormats) {
        setPixelFormat(attribs, 32, 32, !attrs.forceSoftwareRenderer, false, false, useMultisampling, attrs.isWebGL2, allowOfflineRenderers());
        CGLChoosePixelFormat(attribs.data(), &pixelFormatObj, &numPixelFormats);

        if (!numPixelFormats) {
            setPixelFormat(attribs, 32, 16, !attrs.forceSoftwareRenderer, false, false, useMultisampling, attrs.isWebGL2, allowOfflineRenderers());
            CGLChoosePixelFormat(attribs.data(), &pixelFormatObj, &numPixelFormats);

            if (!attrs.forceSoftwareRenderer && !numPixelFormats) {
                setPixelFormat(attribs, 32, 16, false, false, true, false, attrs.isWebGL2, allowOfflineRenderers());
                CGLChoosePixelFormat(attribs.data(), &pixelFormatObj, &numPixelFormats);
                useMultisampling = false;
            }
        }
    }

    if (!numPixelFormats)
        return;

    CGLError err = CGLCreateContext(pixelFormatObj, sharedContext ? sharedContext->m_contextObj : nullptr, &m_contextObj);
    GLint abortOnBlacklist = 0;
    CGLSetParameter(m_contextObj, kCGLCPAbortOnGPURestartStatusBlacklisted, &abortOnBlacklist);
    
#if PLATFORM(MAC)

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
    auto gpuID = (hostWindow && hostWindow->displayID()) ? gpuIDForDisplay(hostWindow->displayID()) : primaryGPUID();
    setGPUByRegistryID(m_contextObj, pixelFormatObj, gpuID);
#else
    if (auto displayMask = primaryOpenGLDisplayMask()) {
        if (hostWindow && hostWindow->displayID())
            displayMask = displayMaskForDisplay(hostWindow->displayID());
        setGPUByDisplayMask(m_contextObj, pixelFormatObj, displayMask);
    }
#endif

#else
    UNUSED_PARAM(hostWindow);
#endif

    CGLDestroyPixelFormat(pixelFormatObj);
    
    if (err != kCGLNoError || !m_contextObj) {
        // We were unable to create the context.
        m_contextObj = 0;
        return;
    }

    m_isForWebGL2 = attrs.isWebGL2;

    CGLSetCurrentContext(m_contextObj);

    // WebGL 2 expects ES 3-only PRIMITIVE_RESTART_FIXED_INDEX to be enabled; we must emulate this on non-ES 3 systems.
    if (m_isForWebGL2)
        ::glEnable(GraphicsContext3D::PRIMITIVE_RESTART);

#endif // !USE(OPENGL_ES)
    
    validateAttributes();

    // Create the WebGLLayer
    BEGIN_BLOCK_OBJC_EXCEPTIONS
        m_webGLLayer = adoptNS([[WebGLLayer alloc] initWithGraphicsContext3D:this]);
#ifndef NDEBUG
        [m_webGLLayer setName:@"WebGL Layer"];
#endif
    END_BLOCK_OBJC_EXCEPTIONS

#if USE(OPENGL)
    if (useMultisampling)
        ::glEnable(GL_MULTISAMPLE);
#endif

    // Create the texture that will be used for the framebuffer.
#if USE(OPENGL_ES)
    ::glGenRenderbuffers(1, &m_texture);
#else
    ::glGenTextures(1, &m_texture);
    // We bind to GL_TEXTURE_RECTANGLE_EXT rather than TEXTURE_2D because
    // that's what is required for a texture backed by IOSurface.
    ::glBindTexture(GL_TEXTURE_RECTANGLE_EXT, m_texture);
    ::glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    ::glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ::glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    ::glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    ::glBindTexture(GL_TEXTURE_RECTANGLE_EXT, 0);
#endif

    // Create the framebuffer object.
    ::glGenFramebuffersEXT(1, &m_fbo);
    ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);

    m_state.boundFBO = m_fbo;
    if (!m_attrs.antialias && (m_attrs.stencil || m_attrs.depth))
        ::glGenRenderbuffersEXT(1, &m_depthStencilBuffer);

    // If necessary, create another framebuffer for the multisample results.
    if (m_attrs.antialias) {
        ::glGenFramebuffersEXT(1, &m_multisampleFBO);
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_multisampleFBO);
        m_state.boundFBO = m_multisampleFBO;
        ::glGenRenderbuffersEXT(1, &m_multisampleColorBuffer);
        if (m_attrs.stencil || m_attrs.depth)
            ::glGenRenderbuffersEXT(1, &m_multisampleDepthStencilBuffer);
    }
    
    // ANGLE initialization.

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
    
#if USE(OPENGL)
    ::glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    if (!isGLES2Compliant())
        ::glEnable(GL_POINT_SPRITE);
#endif

    ::glClearColor(0, 0, 0, 0);

    LOG(WebGL, "Created a GraphicsContext3D (%p).", this);
}

GraphicsContext3D::~GraphicsContext3D()
{
    GraphicsContext3DManager::sharedManager().removeContext(this);

    if (m_contextObj) {
#if USE(OPENGL_ES)
        makeContextCurrent();
        [m_contextObj renderbufferStorage:GL_RENDERBUFFER fromDrawable:nil];
        ::glDeleteRenderbuffers(1, &m_texture);
#else
        CGLSetCurrentContext(m_contextObj);
        ::glDeleteTextures(1, &m_texture);
#endif
        if (m_attrs.antialias) {
            ::glDeleteRenderbuffersEXT(1, &m_multisampleColorBuffer);
            if (m_attrs.stencil || m_attrs.depth)
                ::glDeleteRenderbuffersEXT(1, &m_multisampleDepthStencilBuffer);
            ::glDeleteFramebuffersEXT(1, &m_multisampleFBO);
        } else {
            if (m_attrs.stencil || m_attrs.depth)
                ::glDeleteRenderbuffersEXT(1, &m_depthStencilBuffer);
        }
        ::glDeleteFramebuffersEXT(1, &m_fbo);
#if USE(OPENGL_ES)
        [EAGLContext setCurrentContext:0];
        [static_cast<EAGLContext*>(m_contextObj) release];
#else
        CGLSetCurrentContext(0);
        CGLDestroyContext(m_contextObj);
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
    [m_webGLLayer setOpaque:!m_attrs.alpha];

    [m_contextObj renderbufferStorage:GL_RENDERBUFFER fromDrawable:static_cast<id<EAGLDrawable>>(m_webGLLayer.get())];

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
#else
    CGLContextObj currentContext = CGLGetCurrentContext();
    if (currentContext != m_contextObj)
        return CGLSetCurrentContext(m_contextObj) == kCGLNoError;
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
#else
        [EAGLContext setCurrentContext:0];
#endif
        return;
    }

    // Only do the check every statusCheckThreshold calls.
    if (m_statusCheckCount)
        return;

    m_statusCheckCount = (m_statusCheckCount + 1) % statusCheckThreshold;

    GLint restartStatus = 0;
#if USE(OPENGL)
    CGLGetParameter(platformGraphicsContext3D(), kCGLCPGPURestartStatus, &restartStatus);
    if (restartStatus == kCGLCPGPURestartStatusBlacklisted) {
        LOG(WebGL, "The GPU has blacklisted us (%p). Terminating.", this);
        exit(EX_OSERR);
    }
    if (restartStatus == kCGLCPGPURestartStatusCaused) {
        LOG(WebGL, "The GPU has reset us (%p). Lose the context.", this);
        forceContextLost();
        CGLSetCurrentContext(0);
    }
#else
    EAGLContext* currentContext = static_cast<EAGLContext*>(PlatformGraphicsContext3D());
    [currentContext getParameter:kEAGLCPGPURestartStatus to:&restartStatus];
    if (restartStatus == kEAGLCPGPURestartStatusCaused || restartStatus == kEAGLCPGPURestartStatusBlacklisted) {
        LOG(WebGL, "The GPU has either reset or blacklisted us (%p). Lose the context.", this);
        forceContextLost();
        [EAGLContext setCurrentContext:0];
    }
#endif
}

#if USE(OPENGL_ES)
void GraphicsContext3D::presentRenderbuffer()
{
    makeContextCurrent();
    if (m_attrs.antialias)
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
    return kCGLNoError == CGLTexImageIOSurface2D(platformGraphicsContext3D(), target, internalFormat, width, height, format, type, surface, plane);
#elif USE(OPENGL_ES) && !PLATFORM(IOS_FAMILY_SIMULATOR)
    return [platformGraphicsContext3D() texImageIOSurface:surface target:target internalFormat:internalFormat width:width height:height format:format type:type plane:plane];
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
    [m_webGLLayer allocateIOSurfaceBackingStoreWithSize:size usingAlpha:m_attrs.alpha];
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
    CGLUpdateContext(m_contextObj);
    m_hasSwitchedToHighPerformanceGPU = true;
}

void GraphicsContext3D::setContextVisibility(bool isVisible)
{
    if (m_powerPreferenceUsedForCreation == GraphicsContext3DPowerPreference::HighPerformance) {
        if (isVisible)
            GraphicsContext3DManager::sharedManager().addContextRequiringHighPerformance(this);
        else
            GraphicsContext3DManager::sharedManager().removeContextRequiringHighPerformance(this);
    }
}
#endif

bool GraphicsContext3D::isGLES2Compliant() const
{
    return m_isForWebGL2;
}

void GraphicsContext3D::setContextLostCallback(std::unique_ptr<ContextLostCallback>)
{
}

void GraphicsContext3D::setErrorMessageCallback(std::unique_ptr<ErrorMessageCallback>)
{
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
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
    if (!m_hasSwitchedToHighPerformanceGPU)
        setGPUByRegistryID(m_contextObj, CGLGetPixelFormat(m_contextObj), gpuIDForDisplay(displayID));
#else
    setGPUByDisplayMask(m_contextObj, CGLGetPixelFormat(m_contextObj), displayMaskForDisplay(displayID));
#endif
}
#endif // !PLATFORM(MAC)

}

#endif // ENABLE(GRAPHICS_CONTEXT_3D)
