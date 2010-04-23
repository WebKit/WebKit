/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(3D_CANVAS)

#include <stdio.h>
#include <string.h>

#include "WebGraphicsContext3DDefaultImpl.h"

#include "NotImplemented.h"

#if OS(LINUX)
#include <dlfcn.h>
#endif

namespace WebKit {

// Uncomment this to render to a separate window for debugging
// #define RENDER_TO_DEBUGGING_WINDOW

#if OS(DARWIN)
#define USE_TEXTURE_RECTANGLE_FOR_FRAMEBUFFER
#endif

bool WebGraphicsContext3DDefaultImpl::s_initializedGLEW = false;

#if OS(LINUX)
WebGraphicsContext3DDefaultImpl::GLConnection* WebGraphicsContext3DDefaultImpl::s_gl = 0;

WebGraphicsContext3DDefaultImpl::GLConnection* WebGraphicsContext3DDefaultImpl::GLConnection::create()
{
    Display* dpy = XOpenDisplay(0);
    if (!dpy) {
        printf("GraphicsContext3D: error opening X display\n");
        return 0;
    }

    // We use RTLD_GLOBAL semantics so that GLEW initialization works;
    // GLEW expects to be able to open the current process's handle
    // and do dlsym's of GL entry points from there.
    void* libGL = dlopen("libGL.so.1", RTLD_LAZY | RTLD_GLOBAL);
    if (!libGL) {
        XCloseDisplay(dpy);
        printf("GraphicsContext3D: error opening libGL.so.1: %s\n", dlerror());
        return 0;
    }

    PFNGLXCHOOSEFBCONFIGPROC chooseFBConfig = (PFNGLXCHOOSEFBCONFIGPROC) dlsym(libGL, "glXChooseFBConfig");
    PFNGLXCREATENEWCONTEXTPROC createNewContext = (PFNGLXCREATENEWCONTEXTPROC) dlsym(libGL, "glXCreateNewContext");
    PFNGLXCREATEPBUFFERPROC createPbuffer = (PFNGLXCREATEPBUFFERPROC) dlsym(libGL, "glXCreatePbuffer");
    PFNGLXDESTROYPBUFFERPROC destroyPbuffer = (PFNGLXDESTROYPBUFFERPROC) dlsym(libGL, "glXDestroyPbuffer");
    PFNGLXMAKECURRENTPROC makeCurrent = (PFNGLXMAKECURRENTPROC) dlsym(libGL, "glXMakeCurrent");
    PFNGLXDESTROYCONTEXTPROC destroyContext = (PFNGLXDESTROYCONTEXTPROC) dlsym(libGL, "glXDestroyContext");
    PFNGLXGETCURRENTCONTEXTPROC getCurrentContext = (PFNGLXGETCURRENTCONTEXTPROC) dlsym(libGL, "glXGetCurrentContext");
    if (!chooseFBConfig || !createNewContext || !createPbuffer
        || !destroyPbuffer || !makeCurrent || !destroyContext
        || !getCurrentContext) {
        XCloseDisplay(dpy);
        dlclose(libGL);
        printf("GraphicsContext3D: error looking up bootstrapping entry points\n");
        return 0;
    }
    return new GLConnection(dpy,
                            libGL,
                            chooseFBConfig,
                            createNewContext,
                            createPbuffer,
                            destroyPbuffer,
                            makeCurrent,
                            destroyContext,
                            getCurrentContext);
}

WebGraphicsContext3DDefaultImpl::GLConnection::~GLConnection()
{
    XCloseDisplay(m_display);
    dlclose(m_libGL);
}

#endif // OS(LINUX)

WebGraphicsContext3DDefaultImpl::VertexAttribPointerState::VertexAttribPointerState()
    : enabled(false)
    , buffer(0)
    , indx(0)
    , size(0)
    , type(0)
    , normalized(false)
    , stride(0)
    , offset(0)
{
}

WebGraphicsContext3DDefaultImpl::WebGraphicsContext3DDefaultImpl()
    : m_initialized(false)
    , m_texture(0)
    , m_fbo(0)
    , m_depthStencilBuffer(0)
    , m_multisampleFBO(0)
    , m_multisampleDepthStencilBuffer(0)
    , m_multisampleColorBuffer(0)
    , m_boundFBO(0)
#ifdef FLIP_FRAMEBUFFER_VERTICALLY
    , m_scanline(0)
#endif
    , m_boundArrayBuffer(0)
#if OS(WINDOWS)
    , m_canvasWindow(0)
    , m_canvasDC(0)
    , m_contextObj(0)
#elif PLATFORM(CG)
    , m_pbuffer(0)
    , m_contextObj(0)
    , m_renderOutput(0)
#elif OS(LINUX)
    , m_contextObj(0)
    , m_pbuffer(0)
#else
#error Must port to your platform
#endif
{
}

WebGraphicsContext3DDefaultImpl::~WebGraphicsContext3DDefaultImpl()
{
    if (m_initialized) {
        makeContextCurrent();
#ifndef RENDER_TO_DEBUGGING_WINDOW
        if (m_attributes.antialias) {
            glDeleteRenderbuffersEXT(1, &m_multisampleColorBuffer);
            if (m_attributes.depth || m_attributes.stencil)
                glDeleteRenderbuffersEXT(1, &m_multisampleDepthStencilBuffer);
            glDeleteFramebuffersEXT(1, &m_multisampleFBO);
        } else {
            if (m_attributes.depth || m_attributes.stencil)
                glDeleteRenderbuffersEXT(1, &m_depthStencilBuffer);
        }
        glDeleteTextures(1, &m_texture);
#ifdef FLIP_FRAMEBUFFER_VERTICALLY
        if (m_scanline)
            delete[] m_scanline;
#endif
        glDeleteFramebuffersEXT(1, &m_fbo);
#endif // !RENDER_TO_DEBUGGING_WINDOW
#if OS(WINDOWS)
        wglMakeCurrent(0, 0);
        wglDeleteContext(m_contextObj);
        ReleaseDC(m_canvasWindow, m_canvasDC);
        DestroyWindow(m_canvasWindow);
#elif PLATFORM(CG)
        CGLSetCurrentContext(0);
        CGLDestroyContext(m_contextObj);
        CGLDestroyPBuffer(m_pbuffer);
        if (m_renderOutput)
            delete[] m_renderOutput;
#elif OS(LINUX)
        s_gl->makeCurrent(0, 0);
        s_gl->destroyContext(m_contextObj);
        s_gl->destroyPbuffer(m_pbuffer);
#else
#error Must port to your platform
#endif
        m_contextObj = 0;
    }
}

bool WebGraphicsContext3DDefaultImpl::initialize(WebGraphicsContext3D::Attributes attributes)
{
#if OS(WINDOWS)
    WNDCLASS wc;
    if (!GetClassInfo(GetModuleHandle(0), L"CANVASGL", &wc)) {
        ZeroMemory(&wc, sizeof(WNDCLASS));
        wc.style = CS_OWNDC;
        wc.hInstance = GetModuleHandle(0);
        wc.lpfnWndProc = DefWindowProc;
        wc.lpszClassName = L"CANVASGL";

        if (!RegisterClass(&wc)) {
            printf("WebGraphicsContext3DDefaultImpl: RegisterClass failed\n");
            return false;
        }
    }

    m_canvasWindow = CreateWindow(L"CANVASGL", L"CANVASGL",
                                  WS_CAPTION,
                                  CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                  CW_USEDEFAULT, 0, 0, GetModuleHandle(0), 0);
    if (!m_canvasWindow) {
        printf("WebGraphicsContext3DDefaultImpl: CreateWindow failed\n");
        return false;
    }

    // get the device context
    m_canvasDC = GetDC(m_canvasWindow);
    if (!m_canvasDC) {
        printf("WebGraphicsContext3DDefaultImpl: GetDC failed\n");
        return false;
    }

    // find default pixel format
    PIXELFORMATDESCRIPTOR pfd;
    ZeroMemory(&pfd, sizeof(PIXELFORMATDESCRIPTOR));
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
#ifdef RENDER_TO_DEBUGGING_WINDOW
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
#else
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
#endif
    int pixelformat = ChoosePixelFormat(m_canvasDC, &pfd);

    // set the pixel format for the dc
    if (!SetPixelFormat(m_canvasDC, pixelformat, &pfd)) {
        printf("WebGraphicsContext3DDefaultImpl: SetPixelFormat failed\n");
        return false;
    }

    // create rendering context
    m_contextObj = wglCreateContext(m_canvasDC);
    if (!m_contextObj) {
        printf("WebGraphicsContext3DDefaultImpl: wglCreateContext failed\n");
        return false;
    }

    if (!wglMakeCurrent(m_canvasDC, m_contextObj)) {
        printf("WebGraphicsContext3DDefaultImpl: wglMakeCurrent failed\n");
        return false;
    }

#ifdef RENDER_TO_DEBUGGING_WINDOW
    typedef BOOL (WINAPI * PFNWGLSWAPINTERVALEXTPROC) (int interval);
    PFNWGLSWAPINTERVALEXTPROC setSwapInterval = 0;
    setSwapInterval = (PFNWGLSWAPINTERVALEXTPROC) wglGetProcAddress("wglSwapIntervalEXT");
    if (setSwapInterval)
        setSwapInterval(1);
#endif // RENDER_TO_DEBUGGING_WINDOW

#elif PLATFORM(CG)
    // Create a 1x1 pbuffer and associated context to bootstrap things
    CGLPixelFormatAttribute attribs[] = {
        (CGLPixelFormatAttribute) kCGLPFAPBuffer,
        (CGLPixelFormatAttribute) 0
    };
    CGLPixelFormatObj pixelFormat;
    GLint numPixelFormats;
    if (CGLChoosePixelFormat(attribs, &pixelFormat, &numPixelFormats) != kCGLNoError) {
        printf("WebGraphicsContext3DDefaultImpl: error choosing pixel format\n");
        return false;
    }
    if (!pixelFormat) {
        printf("WebGraphicsContext3DDefaultImpl: no pixel format selected\n");
        return false;
    }
    CGLContextObj context;
    CGLError res = CGLCreateContext(pixelFormat, 0, &context);
    CGLDestroyPixelFormat(pixelFormat);
    if (res != kCGLNoError) {
        printf("WebGraphicsContext3DDefaultImpl: error creating context\n");
        return false;
    }
    CGLPBufferObj pbuffer;
    if (CGLCreatePBuffer(1, 1, GL_TEXTURE_2D, GL_RGBA, 0, &pbuffer) != kCGLNoError) {
        CGLDestroyContext(context);
        printf("WebGraphicsContext3DDefaultImpl: error creating pbuffer\n");
        return false;
    }
    if (CGLSetPBuffer(context, pbuffer, 0, 0, 0) != kCGLNoError) {
        CGLDestroyContext(context);
        CGLDestroyPBuffer(pbuffer);
        printf("WebGraphicsContext3DDefaultImpl: error attaching pbuffer to context\n");
        return false;
    }
    if (CGLSetCurrentContext(context) != kCGLNoError) {
        CGLDestroyContext(context);
        CGLDestroyPBuffer(pbuffer);
        printf("WebGraphicsContext3DDefaultImpl: error making context current\n");
        return false;
    }
    m_pbuffer = pbuffer;
    m_contextObj = context;
#elif OS(LINUX)
    if (!s_gl) {
        s_gl = GLConnection::create();
        if (!s_gl)
            return false;
    }

    int configAttrs[] = {
        GLX_DRAWABLE_TYPE,
        GLX_PBUFFER_BIT,
        GLX_RENDER_TYPE,
        GLX_RGBA_BIT,
        GLX_DOUBLEBUFFER,
        0,
        0
    };
    int nelements = 0;
    GLXFBConfig* config = s_gl->chooseFBConfig(0, configAttrs, &nelements);
    if (!config) {
        printf("WebGraphicsContext3DDefaultImpl: glXChooseFBConfig failed\n");
        return false;
    }
    if (!nelements) {
        printf("WebGraphicsContext3DDefaultImpl: glXChooseFBConfig returned 0 elements\n");
        XFree(config);
        return false;
    }
    GLXContext context = s_gl->createNewContext(config[0], GLX_RGBA_TYPE, 0, True);
    if (!context) {
        printf("WebGraphicsContext3DDefaultImpl: glXCreateNewContext failed\n");
        XFree(config);
        return false;
    }
    int pbufferAttrs[] = {
        GLX_PBUFFER_WIDTH,
        1,
        GLX_PBUFFER_HEIGHT,
        1,
        0
    };
    GLXPbuffer pbuffer = s_gl->createPbuffer(config[0], pbufferAttrs);
    XFree(config);
    if (!pbuffer) {
        printf("WebGraphicsContext3DDefaultImpl: glxCreatePbuffer failed\n");
        return false;
    }
    if (!s_gl->makeCurrent(pbuffer, context)) {
        printf("WebGraphicsContext3DDefaultImpl: glXMakeCurrent failed\n");
        return false;
    }
    m_contextObj = context;
    m_pbuffer = pbuffer;
#else
#error Must port to your platform
#endif

    if (!s_initializedGLEW) {
        // Initialize GLEW and check for GL 2.0 support by the drivers.
        GLenum glewInitResult = glewInit();
        if (glewInitResult != GLEW_OK) {
            printf("WebGraphicsContext3DDefaultImpl: GLEW initialization failed\n");
            return false;
        }
        if (!glewIsSupported("GL_VERSION_2_0")) {
            printf("WebGraphicsContext3DDefaultImpl: OpenGL 2.0 not supported\n");
            return false;
        }
        s_initializedGLEW = true;
    }

    m_attributes = attributes;
    validateAttributes();

    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    m_initialized = true;
    return true;
}

void WebGraphicsContext3DDefaultImpl::validateAttributes()
{
    const char* extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));

    if (m_attributes.stencil) {
        if (strstr(extensions, "GL_EXT_packed_depth_stencil")) {
            if (!m_attributes.depth)
                m_attributes.depth = true;
        } else
            m_attributes.stencil = false;
    }
    if (m_attributes.antialias) {
        bool isValidVendor = true;
#if PLATFORM(CG)
        // Currently in Mac we only turn on antialias if vendor is NVIDIA.
        const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        if (!strstr(vendor, "NVIDIA"))
            isValidVendor = false;
#endif
        if (!isValidVendor || !strstr(extensions, "GL_EXT_framebuffer_multisample"))
            m_attributes.antialias = false;
    }
    // FIXME: instead of enforcing premultipliedAlpha = true, implement the
    // correct behavior when premultipliedAlpha = false is requested.
    m_attributes.premultipliedAlpha = true;
}

bool WebGraphicsContext3DDefaultImpl::makeContextCurrent()
{
#if OS(WINDOWS)
    if (wglGetCurrentContext() != m_contextObj)
        if (wglMakeCurrent(m_canvasDC, m_contextObj))
            return true;
#elif PLATFORM(CG)
    if (CGLGetCurrentContext() != m_contextObj)
        if (CGLSetCurrentContext(m_contextObj) == kCGLNoError)
            return true;
#elif OS(LINUX)
    if (s_gl->getCurrentContext() != m_contextObj)
        if (s_gl->makeCurrent(m_pbuffer, m_contextObj))
            return true;
#else
#error Must port to your platform
#endif
    return false;
}

int WebGraphicsContext3DDefaultImpl::width()
{
    return m_cachedWidth;
}

int WebGraphicsContext3DDefaultImpl::height()
{
    return m_cachedHeight;
}

int WebGraphicsContext3DDefaultImpl::sizeInBytes(int type)
{
    switch (type) {
    case GL_BYTE:
        return sizeof(GLbyte);
    case GL_UNSIGNED_BYTE:
        return sizeof(GLubyte);
    case GL_SHORT:
        return sizeof(GLshort);
    case GL_UNSIGNED_SHORT:
        return sizeof(GLushort);
    case GL_INT:
        return sizeof(GLint);
    case GL_UNSIGNED_INT:
        return sizeof(GLuint);
    case GL_FLOAT:
        return sizeof(GLfloat);
    }
    return 0;
}

bool WebGraphicsContext3DDefaultImpl::isGLES2Compliant()
{
    return false;
}

static int createTextureObject(GLenum target)
{
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(target, texture);
    glTexParameterf(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return texture;
}

void WebGraphicsContext3DDefaultImpl::reshape(int width, int height)
{
#ifdef RENDER_TO_DEBUGGING_WINDOW
    SetWindowPos(m_canvasWindow, HWND_TOP, 0, 0, width, height,
                 SWP_NOMOVE);
    ShowWindow(m_canvasWindow, SW_SHOW);
#endif

    m_cachedWidth = width;
    m_cachedHeight = height;
    makeContextCurrent();

#ifndef RENDER_TO_DEBUGGING_WINDOW
#ifdef USE_TEXTURE_RECTANGLE_FOR_FRAMEBUFFER
    // GL_TEXTURE_RECTANGLE_ARB is the best supported render target on Mac OS X
    GLenum target = GL_TEXTURE_RECTANGLE_ARB;
#else
    GLenum target = GL_TEXTURE_2D;
#endif
    if (!m_texture) {
        // Generate the texture object
        m_texture = createTextureObject(target);
        // Generate the framebuffer object
        glGenFramebuffersEXT(1, &m_fbo);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
        m_boundFBO = m_fbo;
        if (m_attributes.depth || m_attributes.stencil)
            glGenRenderbuffersEXT(1, &m_depthStencilBuffer);
        // Generate the multisample framebuffer object
        if (m_attributes.antialias) {
            glGenFramebuffersEXT(1, &m_multisampleFBO);
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_multisampleFBO);
            m_boundFBO = m_multisampleFBO;
            glGenRenderbuffersEXT(1, &m_multisampleColorBuffer);
            if (m_attributes.depth || m_attributes.stencil)
                glGenRenderbuffersEXT(1, &m_multisampleDepthStencilBuffer);
        }
    }

    GLint internalColorFormat, colorFormat, internalDepthStencilFormat = 0;
    if (m_attributes.alpha) {
        internalColorFormat = GL_RGBA8;
        colorFormat = GL_RGBA;
    } else {
        internalColorFormat = GL_RGB8;
        colorFormat = GL_RGB;
    }
    if (m_attributes.stencil || m_attributes.depth) {
        // We don't allow the logic where stencil is required and depth is not.
        // See GraphicsContext3DInternal constructor.
        if (m_attributes.stencil && m_attributes.depth)
            internalDepthStencilFormat = GL_DEPTH24_STENCIL8_EXT;
        else
            internalDepthStencilFormat = GL_DEPTH_COMPONENT;
    }

    bool mustRestoreFBO = false;

    // Resize multisampling FBO
    if (m_attributes.antialias) {
        GLint maxSampleCount;
        glGetIntegerv(GL_MAX_SAMPLES_EXT, &maxSampleCount);
        GLint sampleCount = std::min(8, maxSampleCount);
        if (m_boundFBO != m_multisampleFBO) {
            mustRestoreFBO = true;
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_multisampleFBO);
        }
        glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_multisampleColorBuffer);
        glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, sampleCount, internalColorFormat, width, height);
        glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, m_multisampleColorBuffer);
        if (m_attributes.stencil || m_attributes.depth) {
            glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_multisampleDepthStencilBuffer);
            glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, sampleCount, internalDepthStencilFormat, width, height);
            if (m_attributes.stencil)
                glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_multisampleDepthStencilBuffer);
            if (m_attributes.depth)
                glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_multisampleDepthStencilBuffer);
        }
        glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
        GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
        if (status != GL_FRAMEBUFFER_COMPLETE_EXT) {
            printf("GraphicsContext3D: multisampling framebuffer was incomplete\n");

            // FIXME: cleanup.
            notImplemented();
        }
    }

    // Resize regular FBO
    if (m_boundFBO != m_fbo) {
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
        mustRestoreFBO = true;
    }
    glBindTexture(target, m_texture);
    glTexImage2D(target, 0, internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, target, m_texture, 0);
    glBindTexture(target, 0);
    if (!m_attributes.antialias && (m_attributes.stencil || m_attributes.depth)) {
        glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_depthStencilBuffer);
        glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, internalDepthStencilFormat, width, height);
        if (m_attributes.stencil)
            glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_depthStencilBuffer);
        if (m_attributes.depth)
            glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_depthStencilBuffer);
        glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
    }
    GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    if (status != GL_FRAMEBUFFER_COMPLETE_EXT) {
        printf("WebGraphicsContext3DDefaultImpl: framebuffer was incomplete\n");

        // FIXME: cleanup.
        notImplemented();
    }

    if (mustRestoreFBO)
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_boundFBO);
#endif // RENDER_TO_DEBUGGING_WINDOW

#ifdef FLIP_FRAMEBUFFER_VERTICALLY
    if (m_scanline) {
        delete[] m_scanline;
        m_scanline = 0;
    }
    m_scanline = new unsigned char[width * 4];
#endif // FLIP_FRAMEBUFFER_VERTICALLY

    GLbitfield clearMask = GL_COLOR_BUFFER_BIT;
    if (m_attributes.stencil)
        clearMask |= GL_STENCIL_BUFFER_BIT;
    if (m_attributes.depth)
        clearMask |= GL_DEPTH_BUFFER_BIT;
    glClear(clearMask);
}

#ifdef FLIP_FRAMEBUFFER_VERTICALLY
void WebGraphicsContext3DDefaultImpl::flipVertically(unsigned char* framebuffer,
                                                     unsigned int width,
                                                     unsigned int height)
{
    unsigned char* scanline = m_scanline;
    if (!scanline)
        return;
    unsigned int rowBytes = width * 4;
    unsigned int count = height / 2;
    for (unsigned int i = 0; i < count; i++) {
        unsigned char* rowA = framebuffer + i * rowBytes;
        unsigned char* rowB = framebuffer + (height - i - 1) * rowBytes;
        // FIXME: this is where the multiplication of the alpha
        // channel into the color buffer will need to occur if the
        // user specifies the "premultiplyAlpha" flag in the context
        // creation attributes.
        memcpy(scanline, rowB, rowBytes);
        memcpy(rowB, rowA, rowBytes);
        memcpy(rowA, scanline, rowBytes);
    }
}
#endif

bool WebGraphicsContext3DDefaultImpl::readBackFramebuffer(unsigned char* pixels, size_t bufferSize)
{
    if (bufferSize != static_cast<size_t>(4 * width() * height()))
        return false;

    makeContextCurrent();

#ifdef RENDER_TO_DEBUGGING_WINDOW
    SwapBuffers(m_canvasDC);
#else
    // Earlier versions of this code used the GPU to flip the
    // framebuffer vertically before reading it back for compositing
    // via software. This code was quite complicated, used a lot of
    // GPU memory, and didn't provide an obvious speedup. Since this
    // vertical flip is only a temporary solution anyway until Chrome
    // is fully GPU composited, it wasn't worth the complexity.

    bool mustRestoreFBO;
    if (m_attributes.antialias) {
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_multisampleFBO);
        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_fbo);
        glBlitFramebufferEXT(0, 0, m_cachedWidth, m_cachedHeight, 0, 0, m_cachedWidth, m_cachedHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
        mustRestoreFBO = true;
    } else {
        if (m_boundFBO != m_fbo) {
            mustRestoreFBO = true;
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
        }
    }

    GLint packAlignment = 4;
    bool mustRestorePackAlignment = false;
    glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment);
    if (packAlignment > 4) {
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        mustRestorePackAlignment = true;
    }

#if PLATFORM(SKIA)
    glReadPixels(0, 0, m_cachedWidth, m_cachedHeight, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
#elif PLATFORM(CG)
    glReadPixels(0, 0, m_cachedWidth, m_cachedHeight, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels);
#else
#error Must port to your platform
#endif

    if (mustRestorePackAlignment)
        glPixelStorei(GL_PACK_ALIGNMENT, packAlignment);

    if (mustRestoreFBO)
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_boundFBO);

#ifdef FLIP_FRAMEBUFFER_VERTICALLY
    if (pixels)
        flipVertically(pixels, m_cachedWidth, m_cachedHeight);
#endif

#endif // RENDER_TO_DEBUGGING_WINDOW
    return true;
}

void WebGraphicsContext3DDefaultImpl::synthesizeGLError(unsigned long error)
{
    m_syntheticErrors.add(error);
}

// Helper macros to reduce the amount of code.

#define DELEGATE_TO_GL(name, glname)                                           \
void WebGraphicsContext3DDefaultImpl::name()                                   \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname();                                                              \
}

#define DELEGATE_TO_GL_1(name, glname, t1)                                     \
void WebGraphicsContext3DDefaultImpl::name(t1 a1)                              \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1);                                                            \
}

#define DELEGATE_TO_GL_1R(name, glname, t1, rt)                                \
rt WebGraphicsContext3DDefaultImpl::name(t1 a1)                                \
{                                                                              \
    makeContextCurrent();                                                      \
    return gl##glname(a1);                                                     \
}

#define DELEGATE_TO_GL_2(name, glname, t1, t2)                                 \
void WebGraphicsContext3DDefaultImpl::name(t1 a1, t2 a2)                       \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, a2);                                                        \
}

#define DELEGATE_TO_GL_2R(name, glname, t1, t2, rt)                            \
rt WebGraphicsContext3DDefaultImpl::name(t1 a1, t2 a2)                         \
{                                                                              \
    makeContextCurrent();                                                      \
    return gl##glname(a1, a2);                                                 \
}

#define DELEGATE_TO_GL_3(name, glname, t1, t2, t3)                             \
void WebGraphicsContext3DDefaultImpl::name(t1 a1, t2 a2, t3 a3)                \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, a2, a3);                                                    \
}

#define DELEGATE_TO_GL_4(name, glname, t1, t2, t3, t4)                         \
void WebGraphicsContext3DDefaultImpl::name(t1 a1, t2 a2, t3 a3, t4 a4)         \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, a2, a3, a4);                                                \
}

#define DELEGATE_TO_GL_5(name, glname, t1, t2, t3, t4, t5)                     \
void WebGraphicsContext3DDefaultImpl::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5)  \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, a2, a3, a4, a5);                                            \
}

#define DELEGATE_TO_GL_6(name, glname, t1, t2, t3, t4, t5, t6)                 \
void WebGraphicsContext3DDefaultImpl::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, a2, a3, a4, a5, a6);                                        \
}

#define DELEGATE_TO_GL_7(name, glname, t1, t2, t3, t4, t5, t6, t7)             \
void WebGraphicsContext3DDefaultImpl::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7) \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, a2, a3, a4, a5, a6, a7);                                    \
}

#define DELEGATE_TO_GL_8(name, glname, t1, t2, t3, t4, t5, t6, t7, t8)         \
void WebGraphicsContext3DDefaultImpl::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8) \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, a2, a3, a4, a5, a6, a7, a8);                                \
}

#define DELEGATE_TO_GL_9(name, glname, t1, t2, t3, t4, t5, t6, t7, t8, t9)     \
void WebGraphicsContext3DDefaultImpl::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8, t9 a9) \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, a2, a3, a4, a5, a6, a7, a8, a9);                            \
}

void WebGraphicsContext3DDefaultImpl::activeTexture(unsigned long texture)
{
    // FIXME: query number of textures available.
    if (texture < GL_TEXTURE0 || texture > GL_TEXTURE0+32)
        // FIXME: raise exception.
        return;

    makeContextCurrent();
    glActiveTexture(texture);
}

DELEGATE_TO_GL_2(attachShader, AttachShader, WebGLId, WebGLId)

DELEGATE_TO_GL_3(bindAttribLocation, BindAttribLocation, WebGLId, unsigned long, const char*)

void WebGraphicsContext3DDefaultImpl::bindBuffer(unsigned long target, WebGLId buffer)
{
    makeContextCurrent();
    if (target == GL_ARRAY_BUFFER)
        m_boundArrayBuffer = buffer;
    glBindBuffer(target, buffer);
}

void WebGraphicsContext3DDefaultImpl::bindFramebuffer(unsigned long target, WebGLId framebuffer)
{
    makeContextCurrent();
    if (!framebuffer)
        framebuffer = (m_attributes.antialias ? m_multisampleFBO : m_fbo);
    if (framebuffer != m_boundFBO) {
        glBindFramebufferEXT(target, framebuffer);
        m_boundFBO = framebuffer;
    }
}

DELEGATE_TO_GL_2(bindRenderbuffer, BindRenderbufferEXT, unsigned long, WebGLId)

DELEGATE_TO_GL_2(bindTexture, BindTexture, unsigned long, WebGLId)

DELEGATE_TO_GL_4(blendColor, BlendColor, double, double, double, double)

DELEGATE_TO_GL_1(blendEquation, BlendEquation, unsigned long)

DELEGATE_TO_GL_2(blendEquationSeparate, BlendEquationSeparate, unsigned long, unsigned long)

DELEGATE_TO_GL_2(blendFunc, BlendFunc, unsigned long, unsigned long)

DELEGATE_TO_GL_4(blendFuncSeparate, BlendFuncSeparate, unsigned long, unsigned long, unsigned long, unsigned long)

DELEGATE_TO_GL_4(bufferData, BufferData, unsigned long, int, const void*, unsigned long)

DELEGATE_TO_GL_4(bufferSubData, BufferSubData, unsigned long, long, int, const void*)

DELEGATE_TO_GL_1R(checkFramebufferStatus, CheckFramebufferStatusEXT, unsigned long, unsigned long)

DELEGATE_TO_GL_1(clear, Clear, unsigned long)

DELEGATE_TO_GL_4(clearColor, ClearColor, double, double, double, double)

DELEGATE_TO_GL_1(clearDepth, ClearDepth, double)

DELEGATE_TO_GL_1(clearStencil, ClearStencil, long)

DELEGATE_TO_GL_4(colorMask, ColorMask, bool, bool, bool, bool)

DELEGATE_TO_GL_1(compileShader, CompileShader, WebGLId)

void WebGraphicsContext3DDefaultImpl::copyTexImage2D(unsigned long target, long level, unsigned long internalformat,
                                                     long x, long y, unsigned long width, unsigned long height, long border)
{
    makeContextCurrent();
#ifndef RENDER_TO_DEBUGGING_WINDOW
    if (m_attributes.antialias && m_boundFBO == m_multisampleFBO) {
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_multisampleFBO);
        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_fbo);
        glBlitFramebufferEXT(x, y, x + width, y + height, x, y, x + width, y + height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
    }
#endif
    glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
#ifndef RENDER_TO_DEBUGGING_WINDOW
    if (m_attributes.antialias && m_boundFBO == m_multisampleFBO)
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_boundFBO);
#endif
}

void WebGraphicsContext3DDefaultImpl::copyTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset,
                                                        long x, long y, unsigned long width, unsigned long height)
{
    makeContextCurrent();
#ifndef RENDER_TO_DEBUGGING_WINDOW
    if (m_attributes.antialias && m_boundFBO == m_multisampleFBO) {
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_multisampleFBO);
        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_fbo);
        glBlitFramebufferEXT(x, y, x + width, y + height, x, y, x + width, y + height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
    }
#endif
    glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
#ifndef RENDER_TO_DEBUGGING_WINDOW
    if (m_attributes.antialias && m_boundFBO == m_multisampleFBO)
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_boundFBO);
#endif
}

DELEGATE_TO_GL_1(cullFace, CullFace, unsigned long)

DELEGATE_TO_GL_1(depthFunc, DepthFunc, unsigned long)

DELEGATE_TO_GL_1(depthMask, DepthMask, bool)

DELEGATE_TO_GL_2(depthRange, DepthRange, double, double)

DELEGATE_TO_GL_2(detachShader, DetachShader, WebGLId, WebGLId)

DELEGATE_TO_GL_1(disable, Disable, unsigned long)

void WebGraphicsContext3DDefaultImpl::disableVertexAttribArray(unsigned long index)
{
    makeContextCurrent();
    if (index < NumTrackedPointerStates)
        m_vertexAttribPointerState[index].enabled = false;
    glDisableVertexAttribArray(index);
}

DELEGATE_TO_GL_3(drawArrays, DrawArrays, unsigned long, long, long)

void WebGraphicsContext3DDefaultImpl::drawElements(unsigned long mode, unsigned long count, unsigned long type, long offset)
{
    makeContextCurrent();
    glDrawElements(mode, count, type,
                   reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

DELEGATE_TO_GL_1(enable, Enable, unsigned long)

void WebGraphicsContext3DDefaultImpl::enableVertexAttribArray(unsigned long index)
{
    makeContextCurrent();
    if (index < NumTrackedPointerStates)
        m_vertexAttribPointerState[index].enabled = true;
    glEnableVertexAttribArray(index);
}

DELEGATE_TO_GL(finish, Finish)

DELEGATE_TO_GL(flush, Flush)

void WebGraphicsContext3DDefaultImpl::framebufferRenderbuffer(unsigned long target, unsigned long attachment,
                                                              unsigned long renderbuffertarget, WebGLId buffer)
{
    makeContextCurrent();
    if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
        glFramebufferRenderbufferEXT(target, GL_DEPTH_ATTACHMENT, renderbuffertarget, buffer);
        glFramebufferRenderbufferEXT(target, GL_STENCIL_ATTACHMENT, renderbuffertarget, buffer);
    } else
        glFramebufferRenderbufferEXT(target, attachment, renderbuffertarget, buffer);
}

DELEGATE_TO_GL_5(framebufferTexture2D, FramebufferTexture2DEXT, unsigned long, unsigned long, unsigned long, WebGLId, long)

DELEGATE_TO_GL_1(frontFace, FrontFace, unsigned long)

void WebGraphicsContext3DDefaultImpl::generateMipmap(unsigned long target)
{
    makeContextCurrent();
    if (glGenerateMipmapEXT)
        glGenerateMipmapEXT(target);
    // FIXME: provide alternative code path? This will be unpleasant
    // to implement if glGenerateMipmapEXT is not available -- it will
    // require a texture readback and re-upload.
}

bool WebGraphicsContext3DDefaultImpl::getActiveAttrib(WebGLId program, unsigned long index, ActiveInfo& info)
{
    if (!program) {
        synthesizeGLError(GL_INVALID_VALUE);
        return false;
    }
    GLint maxNameLength = -1;
    glGetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxNameLength);
    if (maxNameLength < 0)
        return false;
    GLchar* name = 0;
    if (!tryFastMalloc(maxNameLength * sizeof(GLchar)).getValue(name)) {
        synthesizeGLError(GL_OUT_OF_MEMORY);
        return false;
    }
    GLsizei length = 0;
    GLint size = -1;
    GLenum type = 0;
    glGetActiveAttrib(program, index, maxNameLength,
                      &length, &size, &type, name);
    if (size < 0) {
        fastFree(name);
        return false;
    }
    info.name = WebString::fromUTF8(name, length);
    info.type = type;
    info.size = size;
    fastFree(name);
    return true;
}

bool WebGraphicsContext3DDefaultImpl::getActiveUniform(WebGLId program, unsigned long index, ActiveInfo& info)
{
    GLint maxNameLength = -1;
    glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxNameLength);
    if (maxNameLength < 0)
        return false;
    GLchar* name = 0;
    if (!tryFastMalloc(maxNameLength * sizeof(GLchar)).getValue(name)) {
        synthesizeGLError(GL_OUT_OF_MEMORY);
        return false;
    }
    GLsizei length = 0;
    GLint size = -1;
    GLenum type = 0;
    glGetActiveUniform(program, index, maxNameLength,
                       &length, &size, &type, name);
    if (size < 0) {
        fastFree(name);
        return false;
    }
    info.name = WebString::fromUTF8(name, length);
    info.type = type;
    info.size = size;
    fastFree(name);
    return true;
}

DELEGATE_TO_GL_2R(getAttribLocation, GetAttribLocation, WebGLId, const char*, int)

DELEGATE_TO_GL_2(getBooleanv, GetBooleanv, unsigned long, unsigned char*)

DELEGATE_TO_GL_3(getBufferParameteriv, GetBufferParameteriv, unsigned long, unsigned long, int*)

WebGraphicsContext3D::Attributes WebGraphicsContext3DDefaultImpl::getContextAttributes()
{
    return m_attributes;
}

unsigned long WebGraphicsContext3DDefaultImpl::getError()
{
    if (m_syntheticErrors.size() > 0) {
        ListHashSet<unsigned long>::iterator iter = m_syntheticErrors.begin();
        unsigned long err = *iter;
        m_syntheticErrors.remove(iter);
        return err;
    }

    makeContextCurrent();
    return glGetError();
}

DELEGATE_TO_GL_2(getFloatv, GetFloatv, unsigned long, float*)

void WebGraphicsContext3DDefaultImpl::getFramebufferAttachmentParameteriv(unsigned long target, unsigned long attachment,
                                                                          unsigned long pname, int* value)
{
    makeContextCurrent();
    if (attachment == GL_DEPTH_STENCIL_ATTACHMENT)
        attachment = GL_DEPTH_ATTACHMENT; // Or GL_STENCIL_ATTACHMENT, either works.
    glGetFramebufferAttachmentParameterivEXT(target, attachment, pname, value);
}

void WebGraphicsContext3DDefaultImpl::getIntegerv(unsigned long pname, int* value)
{
    // Need to emulate IMPLEMENTATION_COLOR_READ_FORMAT/TYPE for GL.  Any valid
    // combination should work, but GL_RGB/GL_UNSIGNED_BYTE might be the most
    // useful for desktop WebGL users.
    makeContextCurrent();
    switch (pname) {
    case 0x8B9B: // IMPLEMENTATION_COLOR_READ_FORMAT
        *value = GL_RGB;
        break;
    case 0x8B9A: // IMPLEMENTATION_COLOR_READ_TYPE
        *value = GL_UNSIGNED_BYTE;
        break;
    default:
        glGetIntegerv(pname, value);
    }
}

DELEGATE_TO_GL_3(getProgramiv, GetProgramiv, WebGLId, unsigned long, int*)

WebString WebGraphicsContext3DDefaultImpl::getProgramInfoLog(WebGLId program)
{
    makeContextCurrent();
    GLint logLength;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
    if (!logLength)
        return WebString();
    GLchar* log = 0;
    if (!tryFastMalloc(logLength * sizeof(GLchar)).getValue(log))
        return WebString();
    GLsizei returnedLogLength;
    glGetProgramInfoLog(program, logLength, &returnedLogLength, log);
    ASSERT(logLength == returnedLogLength + 1);
    WebString res = WebString::fromUTF8(log, returnedLogLength);
    fastFree(log);
    return res;
}

DELEGATE_TO_GL_3(getRenderbufferParameteriv, GetRenderbufferParameterivEXT, unsigned long, unsigned long, int*)

DELEGATE_TO_GL_3(getShaderiv, GetShaderiv, WebGLId, unsigned long, int*)

WebString WebGraphicsContext3DDefaultImpl::getShaderInfoLog(WebGLId shader)
{
    makeContextCurrent();
    GLint logLength;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    if (!logLength)
        return WebString();
    GLchar* log = 0;
    if (!tryFastMalloc(logLength * sizeof(GLchar)).getValue(log))
        return WebString();
    GLsizei returnedLogLength;
    glGetShaderInfoLog(shader, logLength, &returnedLogLength, log);
    ASSERT(logLength == returnedLogLength + 1);
    WebString res = WebString::fromUTF8(log, returnedLogLength);
    fastFree(log);
    return res;
}

WebString WebGraphicsContext3DDefaultImpl::getShaderSource(WebGLId shader)
{
    makeContextCurrent();
    GLint logLength;
    glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &logLength);
    if (!logLength)
        return WebString();
    GLchar* log = 0;
    if (!tryFastMalloc(logLength * sizeof(GLchar)).getValue(log))
        return WebString();
    GLsizei returnedLogLength;
    glGetShaderSource(shader, logLength, &returnedLogLength, log);
    ASSERT(logLength == returnedLogLength + 1);
    WebString res = WebString::fromUTF8(log, returnedLogLength);
    fastFree(log);
    return res;
}

WebString WebGraphicsContext3DDefaultImpl::getString(unsigned long name)
{
    makeContextCurrent();
    return WebString::fromUTF8(reinterpret_cast<const char*>(glGetString(name)));
}

DELEGATE_TO_GL_3(getTexParameterfv, GetTexParameterfv, unsigned long, unsigned long, float*)

DELEGATE_TO_GL_3(getTexParameteriv, GetTexParameteriv, unsigned long, unsigned long, int*)

DELEGATE_TO_GL_3(getUniformfv, GetUniformfv, WebGLId, long, float*)

DELEGATE_TO_GL_3(getUniformiv, GetUniformiv, WebGLId, long, int*)

DELEGATE_TO_GL_2R(getUniformLocation, GetUniformLocation, WebGLId, const char*, long)

DELEGATE_TO_GL_3(getVertexAttribfv, GetVertexAttribfv, unsigned long, unsigned long, float*)

DELEGATE_TO_GL_3(getVertexAttribiv, GetVertexAttribiv, unsigned long, unsigned long, int*)

long WebGraphicsContext3DDefaultImpl::getVertexAttribOffset(unsigned long index, unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return 0;
}

DELEGATE_TO_GL_2(hint, Hint, unsigned long, unsigned long)

DELEGATE_TO_GL_1R(isBuffer, IsBuffer, WebGLId, bool)

DELEGATE_TO_GL_1R(isEnabled, IsEnabled, unsigned long, bool)

DELEGATE_TO_GL_1R(isFramebuffer, IsFramebuffer, WebGLId, bool)

DELEGATE_TO_GL_1R(isProgram, IsProgram, WebGLId, bool)

DELEGATE_TO_GL_1R(isRenderbuffer, IsRenderbuffer, WebGLId, bool)

DELEGATE_TO_GL_1R(isShader, IsShader, WebGLId, bool)

DELEGATE_TO_GL_1R(isTexture, IsTexture, WebGLId, bool)

DELEGATE_TO_GL_1(lineWidth, LineWidth, double)

DELEGATE_TO_GL_1(linkProgram, LinkProgram, WebGLId)

DELEGATE_TO_GL_2(pixelStorei, PixelStorei, unsigned long, long)

DELEGATE_TO_GL_2(polygonOffset, PolygonOffset, double, double)

void WebGraphicsContext3DDefaultImpl::readPixels(long x, long y, unsigned long width, unsigned long height, unsigned long format, unsigned long type, void* pixels)
{
    // FIXME: remove the two glFlush calls when the driver bug is fixed, i.e.,
    // all previous rendering calls should be done before reading pixels.
    glFlush();
#ifndef RENDER_TO_DEBUGGING_WINDOW
    if (m_attributes.antialias && m_boundFBO == m_multisampleFBO) {
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_multisampleFBO);
        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_fbo);
        glBlitFramebufferEXT(x, y, x + width, y + height, x, y, x + width, y + height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
        glFlush();
    }
#endif
    glReadPixels(x, y, width, height, format, type, pixels);
#ifndef RENDER_TO_DEBUGGING_WINDOW
    if (m_attributes.antialias && m_boundFBO == m_multisampleFBO)
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_boundFBO);
#endif
}

void WebGraphicsContext3DDefaultImpl::releaseShaderCompiler()
{
}

void WebGraphicsContext3DDefaultImpl::renderbufferStorage(unsigned long target,
                                                          unsigned long internalformat,
                                                          unsigned long width,
                                                          unsigned long height)
{
    makeContextCurrent();
    if (internalformat == GL_DEPTH_STENCIL)
        internalformat = GL_DEPTH24_STENCIL8_EXT;
    else if (internalformat == GL_DEPTH_COMPONENT16)
        internalformat = GL_DEPTH_COMPONENT;
    glRenderbufferStorageEXT(target, internalformat, width, height);
}

DELEGATE_TO_GL_2(sampleCoverage, SampleCoverage, double, bool)

DELEGATE_TO_GL_4(scissor, Scissor, long, long, unsigned long, unsigned long)

void WebGraphicsContext3DDefaultImpl::shaderSource(WebGLId shader, const char* string)
{
    makeContextCurrent();
    GLint length = strlen(string);
    glShaderSource(shader, 1, &string, &length);
}

DELEGATE_TO_GL_3(stencilFunc, StencilFunc, unsigned long, long, unsigned long)

DELEGATE_TO_GL_4(stencilFuncSeparate, StencilFuncSeparate, unsigned long, unsigned long, long, unsigned long)

DELEGATE_TO_GL_1(stencilMask, StencilMask, unsigned long)

DELEGATE_TO_GL_2(stencilMaskSeparate, StencilMaskSeparate, unsigned long, unsigned long)

DELEGATE_TO_GL_3(stencilOp, StencilOp, unsigned long, unsigned long, unsigned long)

DELEGATE_TO_GL_4(stencilOpSeparate, StencilOpSeparate, unsigned long, unsigned long, unsigned long, unsigned long)

DELEGATE_TO_GL_9(texImage2D, TexImage2D, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, const void*)

DELEGATE_TO_GL_3(texParameterf, TexParameterf, unsigned, unsigned, float);

DELEGATE_TO_GL_3(texParameteri, TexParameteri, unsigned, unsigned, int);

DELEGATE_TO_GL_9(texSubImage2D, TexSubImage2D, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, const void*)

DELEGATE_TO_GL_2(uniform1f, Uniform1f, long, float)

DELEGATE_TO_GL_3(uniform1fv, Uniform1fv, long, int, float*)

DELEGATE_TO_GL_2(uniform1i, Uniform1i, long, int)

DELEGATE_TO_GL_3(uniform1iv, Uniform1iv, long, int, int*)

DELEGATE_TO_GL_3(uniform2f, Uniform2f, long, float, float)

DELEGATE_TO_GL_3(uniform2fv, Uniform2fv, long, int, float*)

DELEGATE_TO_GL_3(uniform2i, Uniform2i, long, int, int)

DELEGATE_TO_GL_3(uniform2iv, Uniform2iv, long, int, int*)

DELEGATE_TO_GL_4(uniform3f, Uniform3f, long, float, float, float)

DELEGATE_TO_GL_3(uniform3fv, Uniform3fv, long, int, float*)

DELEGATE_TO_GL_4(uniform3i, Uniform3i, long, int, int, int)

DELEGATE_TO_GL_3(uniform3iv, Uniform3iv, long, int, int*)

DELEGATE_TO_GL_5(uniform4f, Uniform4f, long, float, float, float, float)

DELEGATE_TO_GL_3(uniform4fv, Uniform4fv, long, int, float*)

DELEGATE_TO_GL_5(uniform4i, Uniform4i, long, int, int, int, int)

DELEGATE_TO_GL_3(uniform4iv, Uniform4iv, long, int, int*)

DELEGATE_TO_GL_4(uniformMatrix2fv, UniformMatrix2fv, long, int, bool, const float*)

DELEGATE_TO_GL_4(uniformMatrix3fv, UniformMatrix3fv, long, int, bool, const float*)

DELEGATE_TO_GL_4(uniformMatrix4fv, UniformMatrix4fv, long, int, bool, const float*)

DELEGATE_TO_GL_1(useProgram, UseProgram, WebGLId)

DELEGATE_TO_GL_1(validateProgram, ValidateProgram, WebGLId)

DELEGATE_TO_GL_2(vertexAttrib1f, VertexAttrib1f, unsigned long, float)

DELEGATE_TO_GL_2(vertexAttrib1fv, VertexAttrib1fv, unsigned long, const float*)

DELEGATE_TO_GL_3(vertexAttrib2f, VertexAttrib2f, unsigned long, float, float)

DELEGATE_TO_GL_2(vertexAttrib2fv, VertexAttrib2fv, unsigned long, const float*)

DELEGATE_TO_GL_4(vertexAttrib3f, VertexAttrib3f, unsigned long, float, float, float)

DELEGATE_TO_GL_2(vertexAttrib3fv, VertexAttrib3fv, unsigned long, const float*)

DELEGATE_TO_GL_5(vertexAttrib4f, VertexAttrib4f, unsigned long, float, float, float, float)

DELEGATE_TO_GL_2(vertexAttrib4fv, VertexAttrib4fv, unsigned long, const float*)

void WebGraphicsContext3DDefaultImpl::vertexAttribPointer(unsigned long indx, int size, int type, bool normalized,
                                                          unsigned long stride, unsigned long offset)
{
    makeContextCurrent();

    if (m_boundArrayBuffer <= 0) {
        // FIXME: raise exception.
        // LogMessagef(("bufferData: no buffer bound"));
        return;
    }

    if (indx < NumTrackedPointerStates) {
        VertexAttribPointerState& state = m_vertexAttribPointerState[indx];
        state.buffer = m_boundArrayBuffer;
        state.indx = indx;
        state.size = size;
        state.type = type;
        state.normalized = normalized;
        state.stride = stride;
        state.offset = offset;
    }

    glVertexAttribPointer(indx, size, type, normalized, stride,
                          reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

DELEGATE_TO_GL_4(viewport, Viewport, long, long, unsigned long, unsigned long)

unsigned WebGraphicsContext3DDefaultImpl::createBuffer()
{
    makeContextCurrent();
    GLuint o;
    glGenBuffers(1, &o);
    return o;
}

unsigned WebGraphicsContext3DDefaultImpl::createFramebuffer()
{
    makeContextCurrent();
    GLuint o = 0;
    glGenFramebuffersEXT(1, &o);
    return o;
}

unsigned WebGraphicsContext3DDefaultImpl::createProgram()
{
    makeContextCurrent();
    return glCreateProgram();
}

unsigned WebGraphicsContext3DDefaultImpl::createRenderbuffer()
{
    makeContextCurrent();
    GLuint o;
    glGenRenderbuffersEXT(1, &o);
    return o;
}

DELEGATE_TO_GL_1R(createShader, CreateShader, unsigned long, unsigned);

unsigned WebGraphicsContext3DDefaultImpl::createTexture()
{
    makeContextCurrent();
    GLuint o;
    glGenTextures(1, &o);
    return o;
}

void WebGraphicsContext3DDefaultImpl::deleteBuffer(unsigned buffer)
{
    makeContextCurrent();
    glDeleteBuffers(1, &buffer);
}

void WebGraphicsContext3DDefaultImpl::deleteFramebuffer(unsigned framebuffer)
{
    makeContextCurrent();
    glDeleteFramebuffersEXT(1, &framebuffer);
}

void WebGraphicsContext3DDefaultImpl::deleteProgram(unsigned program)
{
    makeContextCurrent();
    glDeleteProgram(program);
}

void WebGraphicsContext3DDefaultImpl::deleteRenderbuffer(unsigned renderbuffer)
{
    makeContextCurrent();
    glDeleteRenderbuffersEXT(1, &renderbuffer);
}

void WebGraphicsContext3DDefaultImpl::deleteShader(unsigned shader)
{
    makeContextCurrent();
    glDeleteShader(shader);
}

void WebGraphicsContext3DDefaultImpl::deleteTexture(unsigned texture)
{
    makeContextCurrent();
    glDeleteTextures(1, &texture);
}

} // namespace WebKit

#endif // ENABLE(3D_CANVAS)
