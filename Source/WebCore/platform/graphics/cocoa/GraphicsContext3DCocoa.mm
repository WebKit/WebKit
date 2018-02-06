/*
 * Copyright (C) 2009-2017 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS)
#import "GraphicsContext3DIOS.h"
#endif

#import "CanvasRenderingContext.h"
#import "Extensions3DOpenGL.h"
#import "GraphicsContext.h"
#import "HTMLCanvasElement.h"
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

#if PLATFORM(IOS)
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

namespace WebCore {

static const unsigned statusCheckThreshold = 5;

#if PLATFORM(MAC)

enum {
    kAGCOpen,
    kAGCClose
};

static io_connect_t attachToAppleGraphicsControl()
{
    mach_port_t masterPort;

    if (IOMasterPort(MACH_PORT_NULL, &masterPort) != KERN_SUCCESS)
        return MACH_PORT_NULL;

    CFDictionaryRef classToMatch = IOServiceMatching("AppleGraphicsControl");
    if (!classToMatch)
        return MACH_PORT_NULL;

    kern_return_t kernResult;
    io_iterator_t iterator;
    if ((kernResult = IOServiceGetMatchingServices(masterPort, classToMatch, &iterator)) != KERN_SUCCESS)
        return MACH_PORT_NULL;

    io_service_t serviceObject = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (!serviceObject)
        return MACH_PORT_NULL;

    io_connect_t dataPort;
    IOObjectRetain(serviceObject);
    kernResult = IOServiceOpen(serviceObject, mach_task_self(), 0, &dataPort);
    IOObjectRelease(serviceObject);

    return (kernResult == KERN_SUCCESS) ? dataPort : MACH_PORT_NULL;
}

static bool hasMuxCapability()
{
    io_connect_t dataPort = attachToAppleGraphicsControl();

    if (dataPort == MACH_PORT_NULL)
        return false;

    bool result;
    if (IOConnectCallScalarMethod(dataPort, kAGCOpen, nullptr, 0, nullptr, nullptr) == KERN_SUCCESS) {
        IOConnectCallScalarMethod(dataPort, kAGCClose, nullptr, 0, nullptr, nullptr);
        result = true;
    } else
        result = false;

    IOServiceClose(dataPort);

    if (result) {
        // This is detecting Mac hardware with an Intel g575 GPU, which
        // we don't want to make available to muxing.
        // Based on information from Apple's OpenGL team, such devices
        // have four or fewer processors.
        // <rdar://problem/30060378>
        int names[2] = { CTL_HW, HW_NCPU };
        int cpuCount;
        size_t cpuCountLength = sizeof(cpuCount);
        sysctl(names, 2, &cpuCount, &cpuCountLength, nullptr, 0);
        result = cpuCount > 4;
    }
    
    return result;
}

static bool hasMuxableGPU()
{
    static bool canMux = hasMuxCapability();
    return canMux;
}

#endif

const unsigned MaxContexts = 16;

class GraphicsContext3DManager {
public:
    GraphicsContext3DManager()
        : m_disableHighPerformanceGPUTimer(*this, &GraphicsContext3DManager::disableHighPerformanceGPUTimerFired)
    {
    }

    void addContext(GraphicsContext3D*);
    void removeContext(GraphicsContext3D*);

    void addContextRequiringHighPerformance(GraphicsContext3D*);
    void removeContextRequiringHighPerformance(GraphicsContext3D*);

    void recycleContextIfNecessary();
    bool hasTooManyContexts() const { return m_contexts.size() >= MaxContexts; }

    void updateAllContexts();

private:
    void updateHighPerformanceState();
    void disableHighPerformanceGPUTimerFired();

    Vector<GraphicsContext3D*> m_contexts;
    HashSet<GraphicsContext3D*> m_contextsRequiringHighPerformance;

    Timer m_disableHighPerformanceGPUTimer;

#if PLATFORM(MAC)
    CGLPixelFormatObj m_pixelFormatObj { nullptr };
#endif
};

static GraphicsContext3DManager& manager()
{
    static NeverDestroyed<GraphicsContext3DManager> s_manager;
    return s_manager;
}

#if PLATFORM(MAC)
static void displayWasReconfigured(CGDirectDisplayID, CGDisplayChangeSummaryFlags flags, void*)
{
    if (flags & kCGDisplaySetModeFlag)
        manager().updateAllContexts();
}
#endif

void GraphicsContext3DManager::updateAllContexts()
{
#if PLATFORM(MAC)
    for (auto* context : m_contexts) {
        context->updateCGLContext();
        context->dispatchContextChangedNotification();
    }
#endif
}

void GraphicsContext3DManager::addContext(GraphicsContext3D* context)
{
    ASSERT(context);
    if (!context)
        return;

#if PLATFORM(MAC)
    if (!m_contexts.size())
        CGDisplayRegisterReconfigurationCallback(displayWasReconfigured, nullptr);
#endif

    ASSERT(!m_contexts.contains(context));
    m_contexts.append(context);
}

void GraphicsContext3DManager::removeContext(GraphicsContext3D* context)
{
    ASSERT(m_contexts.contains(context));
    m_contexts.removeFirst(context);
    removeContextRequiringHighPerformance(context);

#if PLATFORM(MAC)
    if (!m_contexts.size())
        CGDisplayRemoveReconfigurationCallback(displayWasReconfigured, nullptr);
#endif
}

void GraphicsContext3DManager::addContextRequiringHighPerformance(GraphicsContext3D* context)
{
    ASSERT(context);
    if (!context)
        return;

    ASSERT(m_contexts.contains(context));
    ASSERT(!m_contextsRequiringHighPerformance.contains(context));

    LOG(WebGL, "This context (%p) requires the high-performance GPU.", context);
    m_contextsRequiringHighPerformance.add(context);

    updateHighPerformanceState();
}

void GraphicsContext3DManager::removeContextRequiringHighPerformance(GraphicsContext3D* context)
{
    if (!m_contextsRequiringHighPerformance.contains(context))
        return;

    LOG(WebGL, "This context (%p) no longer requires the high-performance GPU.", context);
    m_contextsRequiringHighPerformance.remove(context);

    updateHighPerformanceState();
}

void GraphicsContext3DManager::updateHighPerformanceState()
{
#if PLATFORM(MAC)
    if (!hasMuxableGPU())
        return;

    if (m_contextsRequiringHighPerformance.size()) {

        if (m_disableHighPerformanceGPUTimer.isActive()) {
            LOG(WebGL, "Cancel pending timer for turning off high-performance GPU.");
            m_disableHighPerformanceGPUTimer.stop();
        }

        if (!m_pixelFormatObj) {
            LOG(WebGL, "Turning on high-performance GPU.");

            CGLPixelFormatAttribute attributes[] = { kCGLPFAAccelerated, kCGLPFAColorSize, static_cast<CGLPixelFormatAttribute>(32), static_cast<CGLPixelFormatAttribute>(0) };
            GLint numPixelFormats = 0;
            CGLChoosePixelFormat(attributes, &m_pixelFormatObj, &numPixelFormats);
        }

    } else if (m_pixelFormatObj) {
        // Don't immediately turn off the high-performance GPU. The user might be
        // swapping back and forth between tabs or windows, and we don't want to cause
        // churn if we can avoid it.
        if (!m_disableHighPerformanceGPUTimer.isActive()) {
            LOG(WebGL, "Set a timer to turn off high-performance GPU.");
            // FIXME: Expose this value as a Setting, which would require this class
            // to reference a frame, page or document.
            static const Seconds timeToKeepHighPerformanceGPUAlive { 10_s };
            m_disableHighPerformanceGPUTimer.startOneShot(timeToKeepHighPerformanceGPUAlive);
        }
    }
#endif
}

void GraphicsContext3DManager::disableHighPerformanceGPUTimerFired()
{
#if PLATFORM(MAC)
    if (!m_contextsRequiringHighPerformance.size() && m_pixelFormatObj) {
        LOG(WebGL, "Turning off high-performance GPU.");
        CGLReleasePixelFormat(m_pixelFormatObj);
        m_pixelFormatObj = nullptr;
    }
#endif
}

void GraphicsContext3DManager::recycleContextIfNecessary()
{
    if (hasTooManyContexts()) {
        LOG(WebGL, "Manager recycled context (%p).", m_contexts.at(0));
        m_contexts.at(0)->recycleContext();
    }
}

// FIXME: This class is currently empty on Mac, but will get populated as
// the restructuring in https://bugs.webkit.org/show_bug.cgi?id=66903 is done
class GraphicsContext3DPrivate {
public:
    GraphicsContext3DPrivate(GraphicsContext3D*) { }
    
    ~GraphicsContext3DPrivate() { }
};

#if PLATFORM(MAC)

static void setPixelFormat(Vector<CGLPixelFormatAttribute>& attribs, int colorBits, int depthBits, bool accelerated, bool supersample, bool closest, bool antialias, bool useGLES3)
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
    if (hasMuxableGPU())
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

    if (useGLES3) {
        // FIXME: Instead of backing a WebGL2 GraphicsContext3D with a OpenGL 3.2 context, we should instead back it with ANGLE.
        // Use an OpenGL 3.2 context for now until the ANGLE backend is ready.
        attribs.append(kCGLPFAOpenGLProfile);
        attribs.append(static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_3_2_Core));
    }
        
    attribs.append(static_cast<CGLPixelFormatAttribute>(0));
}
#endif // !PLATFORM(IOS)

RefPtr<GraphicsContext3D> GraphicsContext3D::create(GraphicsContext3DAttributes attrs, HostWindow* hostWindow, GraphicsContext3D::RenderStyle renderStyle)
{
    // This implementation doesn't currently support rendering directly to the HostWindow.
    if (renderStyle == RenderDirectlyToHostWindow)
        return nullptr;

    // Make space for the incoming context if we're full.
    manager().recycleContextIfNecessary();
    if (manager().hasTooManyContexts())
        return nullptr;

    RefPtr<GraphicsContext3D> context = adoptRef(new GraphicsContext3D(attrs, hostWindow, renderStyle));

    if (!context->m_contextObj)
        return nullptr;

    manager().addContext(context.get());

    return context;
}

Ref<GraphicsContext3D> GraphicsContext3D::createShared(GraphicsContext3D& sharedContext)
{
    auto context = adoptRef(*new GraphicsContext3D(sharedContext.getContextAttributes(), nullptr, sharedContext.m_renderStyle, &sharedContext));

    manager().addContext(context.ptr());

    return context;
}

GraphicsContext3D::GraphicsContext3D(GraphicsContext3DAttributes attrs, HostWindow*, GraphicsContext3D::RenderStyle, GraphicsContext3D* sharedContext)
    : m_attrs(attrs)
#if PLATFORM(IOS)
    , m_compiler(SH_ESSL_OUTPUT)
#endif
    , m_private(std::make_unique<GraphicsContext3DPrivate>(this))
{
#if PLATFORM(IOS)
    EAGLRenderingAPI api = m_attrs.useGLES3 ? kEAGLRenderingAPIOpenGLES3 : kEAGLRenderingAPIOpenGLES2;
    if (!sharedContext)
        m_contextObj = [[EAGLContext alloc] initWithAPI:api];
    else
        m_contextObj = [[EAGLContext alloc] initWithAPI:api sharegroup:sharedContext->m_contextObj.sharegroup];
    makeContextCurrent();
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

    m_powerPreferenceUsedForCreation = (hasMuxableGPU() && attrs.powerPreference == GraphicsContext3DPowerPreference::HighPerformance) ? GraphicsContext3DPowerPreference::HighPerformance : GraphicsContext3DPowerPreference::Default;

    setPixelFormat(attribs, 32, 32, !attrs.forceSoftwareRenderer, true, false, useMultisampling, attrs.useGLES3);
    CGLChoosePixelFormat(attribs.data(), &pixelFormatObj, &numPixelFormats);

    if (!numPixelFormats) {
        setPixelFormat(attribs, 32, 32, !attrs.forceSoftwareRenderer, false, false, useMultisampling, attrs.useGLES3);
        CGLChoosePixelFormat(attribs.data(), &pixelFormatObj, &numPixelFormats);

        if (!numPixelFormats) {
            setPixelFormat(attribs, 32, 16, !attrs.forceSoftwareRenderer, false, false, useMultisampling, attrs.useGLES3);
            CGLChoosePixelFormat(attribs.data(), &pixelFormatObj, &numPixelFormats);

            if (!attrs.forceSoftwareRenderer && !numPixelFormats) {
                setPixelFormat(attribs, 32, 16, false, false, true, false, attrs.useGLES3);
                CGLChoosePixelFormat(attribs.data(), &pixelFormatObj, &numPixelFormats);
                useMultisampling = false;
            }
        }
    }

    if (!numPixelFormats)
        return;

    CGLError err = CGLCreateContext(pixelFormatObj, sharedContext ? sharedContext->m_contextObj : nullptr, &m_contextObj);
    GLint abortOnBlacklist = 0;
#if PLATFORM(MAC)
    CGLSetParameter(m_contextObj, kCGLCPAbortOnGPURestartStatusBlacklisted, &abortOnBlacklist);
#elif PLATFORM(IOS)
    CGLSetParameter(m_contextObj, kEAGLCPAbortOnGPURestartStatusBlacklisted, &abortOnBlacklist);
#endif

    CGLDestroyPixelFormat(pixelFormatObj);
    
    if (err != kCGLNoError || !m_contextObj) {
        // We were unable to create the context.
        m_contextObj = 0;
        return;
    }

    m_isForWebGL2 = attrs.useGLES3;

    // Set the current context to the one given to us.
    CGLSetCurrentContext(m_contextObj);

#endif // !PLATFORM(IOS)
    
    validateAttributes();

    // Create the WebGLLayer
    BEGIN_BLOCK_OBJC_EXCEPTIONS
        m_webGLLayer = adoptNS([[WebGLLayer alloc] initWithGraphicsContext3D:this]);
#ifndef NDEBUG
        [m_webGLLayer setName:@"WebGL Layer"];
#endif
    END_BLOCK_OBJC_EXCEPTIONS

#if !PLATFORM(IOS)
    if (useMultisampling)
        ::glEnable(GL_MULTISAMPLE);
#endif

    // Create the texture that will be used for the framebuffer.
#if PLATFORM(IOS)
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
    
#if !PLATFORM(IOS)
    ::glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    if (!isGLES2Compliant())
        ::glEnable(GL_POINT_SPRITE);
#endif

    ::glClearColor(0, 0, 0, 0);

    LOG(WebGL, "Created a GraphicsContext3D (%p).", this);
}

GraphicsContext3D::~GraphicsContext3D()
{
    manager().removeContext(this);

    if (m_contextObj) {
#if PLATFORM(IOS)
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
#if PLATFORM(IOS)
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

#if PLATFORM(IOS)
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

#if PLATFORM(IOS)
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
#if PLATFORM(MAC)
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
#if PLATFORM(MAC)
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

#if PLATFORM(IOS)
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
#if PLATFORM(MAC)
    return kCGLNoError == CGLTexImageIOSurface2D(platformGraphicsContext3D(), target, internalFormat, width, height, format, type, surface, plane);
#elif PLATFORM(IOS) && !PLATFORM(IOS_SIMULATOR)
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

#if PLATFORM(MAC)
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
}

void GraphicsContext3D::setContextVisibility(bool isVisible)
{
    if (m_powerPreferenceUsedForCreation == GraphicsContext3DPowerPreference::HighPerformance) {
        if (isVisible)
            manager().addContextRequiringHighPerformance(this);
        else
            manager().removeContextRequiringHighPerformance(this);
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
    manager().updateAllContexts();
}

}

#endif // ENABLE(GRAPHICS_CONTEXT_3D)
