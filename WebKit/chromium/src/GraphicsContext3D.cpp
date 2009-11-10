/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "GraphicsContext3D.h"

#include "CachedImage.h"
#include "CanvasBuffer.h"
#include "CanvasByteArray.h"
#include "CanvasFloatArray.h"
#include "CanvasFramebuffer.h"
#include "CanvasIntArray.h"
#include "CanvasObject.h"
#include "CanvasProgram.h"
#include "CanvasRenderbuffer.h"
#include "CanvasRenderingContext3D.h"
#include "CanvasShader.h"
#include "CanvasTexture.h"
#include "CanvasUnsignedByteArray.h"
#include "CString.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "NotImplemented.h"

#include <stdio.h>
#include <wtf/FastMalloc.h>

#if PLATFORM(WIN_OS)
#include <windows.h>
#endif

#include "GL/glew.h"

#if PLATFORM(CG)
#include "GraphicsContext.h"
#include <CoreGraphics/CGContext.h>
#include <CoreGraphics/CGBitmapContext.h>
#include <CoreGraphics/CGImage.h>
#include <OpenGL/OpenGL.h>
#else
#define FLIP_FRAMEBUFFER_VERTICALLY
#endif

#if PLATFORM(SKIA)
#include "NativeImageSkia.h"
#endif

#if PLATFORM(DARWIN)
#define USE_TEXTURE_RECTANGLE_FOR_FRAMEBUFFER
#endif

#if PLATFORM(LINUX)
#include <dlfcn.h>
#include "GL/glxew.h"
#endif

using namespace std;

namespace WebCore {

// GraphicsContext3DInternal -----------------------------------------------------

// Uncomment this to render to a separate window for debugging
// #define RENDER_TO_DEBUGGING_WINDOW

#define EXTRACT(val) (!val ? 0 : val->object())

class GraphicsContext3DInternal {
public:
    GraphicsContext3DInternal();
    ~GraphicsContext3DInternal();

    void checkError() const;
    bool makeContextCurrent();

    PlatformGraphicsContext3D platformGraphicsContext3D() const;
    Platform3DObject platformTexture() const;

    void reshape(int width, int height);

    void beginPaint(CanvasRenderingContext3D* context);

    bool validateTextureTarget(int target);
    bool validateTextureParameter(int param);

    void activeTexture(unsigned long texture);
    void bindBuffer(unsigned long target,
                    CanvasBuffer* buffer);
    void bindTexture(unsigned long target,
                     CanvasTexture* texture);
    void bufferDataImpl(unsigned long target, int size, const void* data, unsigned long usage);
    void disableVertexAttribArray(unsigned long index);
    void enableVertexAttribArray(unsigned long index);
    void vertexAttribPointer(unsigned long indx, int size, int type, bool normalized,
                             unsigned long stride, unsigned long offset);
    void viewportImpl(long x, long y, unsigned long width, unsigned long height);

private:
    unsigned int m_texture;
    unsigned int m_fbo;
    unsigned int m_depthBuffer;
    unsigned int m_cachedWidth, m_cachedHeight;

#ifdef FLIP_FRAMEBUFFER_VERTICALLY
    unsigned char* m_scanline;
    void flipVertically(unsigned char* framebuffer,
                        unsigned int width,
                        unsigned int height);
#endif

    // Note: we aren't currently using this information, but we will
    // need to in order to verify that all enabled vertex arrays have
    // a valid buffer bound -- to avoid crashes on certain cards.
    unsigned int m_boundArrayBuffer;
    class VertexAttribPointerState {
    public:
        VertexAttribPointerState();

        bool enabled;
        unsigned long buffer;
        unsigned long indx;
        int size;
        int type;
        bool normalized;
        unsigned long stride;
        unsigned long offset;
    };

    enum {
        NumTrackedPointerStates = 2
    };
    VertexAttribPointerState m_vertexAttribPointerState[NumTrackedPointerStates];

#if PLATFORM(SKIA)
    // If the width and height of the Canvas's backing store don't
    // match those that we were given in the most recent call to
    // reshape(), then we need an intermediate bitmap to read back the
    // frame buffer into. This seems to happen when CSS styles are
    // used to resize the Canvas.
    SkBitmap* m_resizingBitmap;
#endif

#if PLATFORM(WIN_OS)
    HWND  m_canvasWindow;
    HDC   m_canvasDC;
    HGLRC m_contextObj;
#elif PLATFORM(CG)
    CGLPBufferObj m_pbuffer;
    CGLContextObj m_contextObj;
    unsigned char* m_renderOutput;
    CGContextRef m_cgContext;
#elif PLATFORM(LINUX)
    Display* m_display;
    GLXContext m_contextObj;
    GLXPbuffer m_pbuffer;
    // In order to avoid problems caused by linking against libGL, we
    // dynamically look up all the symbols we need.
    // http://code.google.com/p/chromium/issues/detail?id=16800
    void* m_libGL;
    PFNGLXCHOOSEFBCONFIGPROC m_glXChooseFBConfig;
    PFNGLXCREATENEWCONTEXTPROC m_glXCreateNewContext;
    PFNGLXCREATEPBUFFERPROC m_glXCreatePbuffer;
    PFNGLXDESTROYPBUFFERPROC m_glXDestroyPbuffer;
    typedef Bool (* PFNGLXMAKECURRENTPROC)(Display* dpy, GLXDrawable drawable, GLXContext ctx);
    PFNGLXMAKECURRENTPROC m_glXMakeCurrent;
    typedef void (* PFNGLXDESTROYCONTEXTPROC)(Display* dpy, GLXContext ctx);
    PFNGLXDESTROYCONTEXTPROC m_glXDestroyContext;
    typedef GLXContext (* PFNGLXGETCURRENTCONTEXTPROC)(void);
    PFNGLXGETCURRENTCONTEXTPROC m_glXGetCurrentContext;
#else
    #error Must port GraphicsContext3D to your platform
#endif
};

GraphicsContext3DInternal::VertexAttribPointerState::VertexAttribPointerState()
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

#if PLATFORM(LINUX)
static void* tryLoad(const char* libName)
{
    // We use RTLD_GLOBAL semantics so that GLEW initialization works;
    // GLEW expects to be able to open the current process's handle
    // and do dlsym's of GL entry points from there.
    return dlopen(libName, RTLD_LAZY | RTLD_GLOBAL);
}
#endif

GraphicsContext3DInternal::GraphicsContext3DInternal()
    : m_texture(0)
    , m_fbo(0)
    , m_depthBuffer(0)
#ifdef FLIP_FRAMEBUFFER_VERTICALLY
    , m_scanline(0)
#endif
    , m_boundArrayBuffer(0)
#if PLATFORM(SKIA)
    , m_resizingBitmap(0)
#endif
#if PLATFORM(WIN_OS)
    , m_canvasWindow(0)
    , m_canvasDC(0)
    , m_contextObj(0)
#elif PLATFORM(CG)
    , m_pbuffer(0)
    , m_contextObj(0)
    , m_renderOutput(0)
    , m_cgContext(0)
#elif PLATFORM(LINUX)
    , m_display(0)
    , m_contextObj(0)
    , m_pbuffer(0)
    , m_glXChooseFBConfig(0)
    , m_glXCreateNewContext(0)
    , m_glXCreatePbuffer(0)
    , m_glXDestroyPbuffer(0)
    , m_glXMakeCurrent(0)
    , m_glXDestroyContext(0)
    , m_glXGetCurrentContext(0)
#else
#error Must port to your platform
#endif
{
#if PLATFORM(WIN_OS)
    WNDCLASS wc;
    if (!GetClassInfo(GetModuleHandle(0), L"CANVASGL", &wc)) {
        ZeroMemory(&wc, sizeof(WNDCLASS));
        wc.style = CS_OWNDC;
        wc.hInstance = GetModuleHandle(0);
        wc.lpfnWndProc = DefWindowProc;
        wc.lpszClassName = L"CANVASGL";

        if (!RegisterClass(&wc)) {
            printf("GraphicsContext3D: RegisterClass failed\n");
            return;
        }
    }

    m_canvasWindow = CreateWindow(L"CANVASGL", L"CANVASGL",
                                  WS_CAPTION,
                                  CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                  CW_USEDEFAULT, 0, 0, GetModuleHandle(0), 0);
    if (!m_canvasWindow) {
        printf("GraphicsContext3DInternal: CreateWindow failed\n");
        return;
    }

    // get the device context
    m_canvasDC = GetDC(m_canvasWindow);
    if (!m_canvasDC) {
        printf("GraphicsContext3DInternal: GetDC failed\n");
        return;
    }

    // find default pixel format
    PIXELFORMATDESCRIPTOR pfd;
    ZeroMemory(&pfd, sizeof(PIXELFORMATDESCRIPTOR));
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL
#ifdef RENDER_TO_DEBUGGING_WINDOW
        | PFD_DOUBLEBUFFER
#endif // RENDER_TO_DEBUGGING_WINDOW
        ;
    int pixelformat = ChoosePixelFormat(m_canvasDC, &pfd);

    // set the pixel format for the dc
    if (!SetPixelFormat(m_canvasDC, pixelformat, &pfd)) {
        printf("GraphicsContext3D: SetPixelFormat failed\n");
        return;
    }

    // create rendering context
    m_contextObj = wglCreateContext(m_canvasDC);
    if (!m_contextObj) {
        printf("GraphicsContext3D: wglCreateContext failed\n");
        return;
    }

    if (!wglMakeCurrent(m_canvasDC, m_contextObj)) {
        printf("GraphicsContext3D: wglMakeCurrent failed\n");
        return;
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
        printf("GraphicsContext3D: error choosing pixel format\n");
        return;
    }
    if (!pixelFormat) {
        printf("GraphicsContext3D: no pixel format selected\n");
        return;
    }
    CGLContextObj context;
    CGLError res = CGLCreateContext(pixelFormat, 0, &context);
    CGLDestroyPixelFormat(pixelFormat);
    if (res != kCGLNoError) {
        printf("GraphicsContext3D: error creating context\n");
        return;
    }
    CGLPBufferObj pbuffer;
    if (CGLCreatePBuffer(1, 1, GL_TEXTURE_2D, GL_RGBA, 0, &pbuffer) != kCGLNoError) {
        CGLDestroyContext(context);
        printf("GraphicsContext3D: error creating pbuffer\n");
        return;
    }
    if (CGLSetPBuffer(context, pbuffer, 0, 0, 0) != kCGLNoError) {
        CGLDestroyContext(context);
        CGLDestroyPBuffer(pbuffer);
        printf("GraphicsContext3D: error attaching pbuffer to context\n");
        return;
    }
    if (CGLSetCurrentContext(context) != kCGLNoError) {
        CGLDestroyContext(context);
        CGLDestroyPBuffer(pbuffer);
        printf("GraphicsContext3D: error making context current\n");
        return;
    }
    m_pbuffer = pbuffer;
    m_contextObj = context;
#elif PLATFORM(LINUX)
    m_display = XOpenDisplay(0);
    if (!m_display) {
        printf("GraphicsContext3D: error opening X display\n");
        return;
    }

    const char* libNames[] = {
        "/usr/lib/libGL.so.1",
        "/usr/lib32/libGL.so.1",
        "/usr/lib64/libGL.so.1",
    };
    for (int i = 0; i < sizeof(libNames) / sizeof(const char*); i++) {
        m_libGL = tryLoad(libNames[i]);
        if (m_libGL)
            break;
    }
    if (!m_libGL) {
        printf("GraphicsContext3D: error opening libGL.so.1\n");
        printf("GraphicsContext3D: tried:");
        for (int i = 0; i < sizeof(libNames) / sizeof(const char*); i++)
            printf(" %s", libNames[i]);
        return;
    }
    m_glXChooseFBConfig = (PFNGLXCHOOSEFBCONFIGPROC) dlsym(m_libGL, "glXChooseFBConfig");
    m_glXCreateNewContext = (PFNGLXCREATENEWCONTEXTPROC) dlsym(m_libGL, "glXCreateNewContext");
    m_glXCreatePbuffer = (PFNGLXCREATEPBUFFERPROC) dlsym(m_libGL, "glXCreatePbuffer");
    m_glXDestroyPbuffer = (PFNGLXDESTROYPBUFFERPROC) dlsym(m_libGL, "glXDestroyPbuffer");
    m_glXMakeCurrent = (PFNGLXMAKECURRENTPROC) dlsym(m_libGL, "glXMakeCurrent");
    m_glXDestroyContext = (PFNGLXDESTROYCONTEXTPROC) dlsym(m_libGL, "glXDestroyContext");
    m_glXGetCurrentContext = (PFNGLXGETCURRENTCONTEXTPROC) dlsym(m_libGL, "glXGetCurrentContext");
    if (!m_glXChooseFBConfig || !m_glXCreateNewContext || !m_glXCreatePbuffer
        || !m_glXDestroyPbuffer || !m_glXMakeCurrent || !m_glXDestroyContext
        || !m_glXGetCurrentContext) {
        printf("GraphicsContext3D: error looking up bootstrapping entry points\n");
        return;
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
    GLXFBConfig* config = m_glXChooseFBConfig(m_display, 0, configAttrs, &nelements);
    if (!config) {
        printf("GraphicsContext3D: glXChooseFBConfig failed\n");
        return;
    }
    if (!nelements) {
        printf("GraphicsContext3D: glXChooseFBConfig returned 0 elements\n");
        XFree(config);
        return;
    }
    GLXContext context = m_glXCreateNewContext(m_display, config[0], GLX_RGBA_TYPE, 0, True);
    if (!context) {
        printf("GraphicsContext3D: glXCreateNewContext failed\n");
        XFree(config);
        return;
    }
    int pbufferAttrs[] = {
        GLX_PBUFFER_WIDTH,
        1,
        GLX_PBUFFER_HEIGHT,
        1,
        0
    };
    GLXPbuffer pbuffer = m_glXCreatePbuffer(m_display, config[0], pbufferAttrs);
    XFree(config);
    if (!pbuffer) {
        printf("GraphicsContext3D: glxCreatePbuffer failed\n");
        return;
    }
    if (!m_glXMakeCurrent(m_display, pbuffer, context)) {
        printf("GraphicsContext3D: glXMakeCurrent failed\n");
        return;
    }
    m_contextObj = context;
    m_pbuffer = pbuffer;
#else
#error Must port to your platform
#endif

    static bool initializedGLEW = false;
    if (!initializedGLEW) {
        // Initialize GLEW and check for GL 2.0 support by the drivers.
        GLenum glewInitResult = glewInit();
        if (glewInitResult != GLEW_OK) {
            printf("GraphicsContext3D: GLEW initialization failed\n");
            return;
        }
        if (!glewIsSupported("GL_VERSION_2_0")) {
            printf("GraphicsContext3D: OpenGL 2.0 not supported\n");
            return;
        }
        initializedGLEW = true;
    }
}

GraphicsContext3DInternal::~GraphicsContext3DInternal()
{
    makeContextCurrent();
#ifndef RENDER_TO_DEBUGGING_WINDOW
    glDeleteRenderbuffersEXT(1, &m_depthBuffer);
    glDeleteTextures(1, &m_texture);
#ifdef FLIP_FRAMEBUFFER_VERTICALLY
    if (m_scanline)
        delete[] m_scanline;
#endif
    glDeleteFramebuffersEXT(1, &m_fbo);
#endif // !RENDER_TO_DEBUGGING_WINDOW
#if PLATFORM(SKIA)
    if (m_resizingBitmap)
        delete m_resizingBitmap;
#endif
#if PLATFORM(WIN_OS)
    wglMakeCurrent(0, 0);
    wglDeleteContext(m_contextObj);
    ReleaseDC(m_canvasWindow, m_canvasDC);
    DestroyWindow(m_canvasWindow);
#elif PLATFORM(CG)
    CGLSetCurrentContext(0);
    CGLDestroyContext(m_contextObj);
    CGLDestroyPBuffer(m_pbuffer);
    if (m_cgContext)
        CGContextRelease(m_cgContext);
    if (m_renderOutput)
        delete[] m_renderOutput;
#elif PLATFORM(LINUX)
    m_glXMakeCurrent(m_display, 0, 0);
    m_glXDestroyContext(m_display, m_contextObj);
    m_glXDestroyPbuffer(m_display, m_pbuffer);
    XCloseDisplay(m_display);
    dlclose(m_libGL);
#else
#error Must port to your platform
#endif
    m_contextObj = 0;
}

void GraphicsContext3DInternal::checkError() const
{
    // FIXME: This needs to only be done in the debug context. It
    // will need to throw an exception on error.
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        printf("GraphicsContext3DInternal: GL Error : %x\n", error);
        notImplemented();
    }
}

bool GraphicsContext3DInternal::makeContextCurrent()
{
#if PLATFORM(WIN_OS)
    if (wglGetCurrentContext() != m_contextObj)
        if (wglMakeCurrent(m_canvasDC, m_contextObj))
            return true;
#elif PLATFORM(CG)
    if (CGLGetCurrentContext() != m_contextObj)
        if (CGLSetCurrentContext(m_contextObj) == kCGLNoError)
            return true;
#elif PLATFORM(LINUX)
    if (m_glXGetCurrentContext() != m_contextObj)
        if (m_glXMakeCurrent(m_display, m_pbuffer, m_contextObj))
            return true;
#else
#error Must port to your platform
#endif
    return false;
}

PlatformGraphicsContext3D GraphicsContext3DInternal::platformGraphicsContext3D() const
{
    return m_contextObj;
}

Platform3DObject GraphicsContext3DInternal::platformTexture() const
{
    return m_texture;
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

void GraphicsContext3DInternal::reshape(int width, int height)
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
        // Generate the depth buffer
        glGenRenderbuffersEXT(1, &m_depthBuffer);
        checkError();
    }

    // Reallocate the color and depth buffers
    glBindTexture(target, m_texture);
    glTexImage2D(target, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glBindTexture(target, 0);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_depthBuffer);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, width, height);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);

    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, target, m_texture, 0);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_depthBuffer);
    GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    if (status != GL_FRAMEBUFFER_COMPLETE_EXT) {
        printf("GraphicsContext3D: framebuffer was incomplete\n");

        // FIXME: cleanup.
        notImplemented();
    }
#endif  // RENDER_TO_DEBUGGING_WINDOW

#ifdef FLIP_FRAMEBUFFER_VERTICALLY
    if (m_scanline) {
        delete[] m_scanline;
        m_scanline = 0;
    }
    m_scanline = new unsigned char[width * 4];
#endif  // FLIP_FRAMEBUFFER_VERTICALLY

    glClear(GL_COLOR_BUFFER_BIT);
    viewportImpl(0, 0, width, height);

#if PLATFORM(CG)
    // Need to reallocate the client-side backing store.
    // FIXME: make this more efficient.
    if (m_cgContext) {
        CGContextRelease(m_cgContext);
        m_cgContext = 0;
    }
    if (m_renderOutput) {
        delete[] m_renderOutput;
        m_renderOutput = 0;
    }
    int rowBytes = width * 4;
    m_renderOutput = new unsigned char[height * rowBytes];
    CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
    m_cgContext = CGBitmapContextCreate(m_renderOutput, width, height, 8, rowBytes,
                                        colorSpace, kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(colorSpace);
#endif  // PLATFORM(CG)
}

#ifdef FLIP_FRAMEBUFFER_VERTICALLY
void GraphicsContext3DInternal::flipVertically(unsigned char* framebuffer,
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

void GraphicsContext3DInternal::beginPaint(CanvasRenderingContext3D* context)
{
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

    HTMLCanvasElement* canvas = context->canvas();
    ImageBuffer* imageBuffer = canvas->buffer();
    unsigned char* pixels = 0;
#if PLATFORM(SKIA)
    const SkBitmap* canvasBitmap = imageBuffer->context()->platformContext()->bitmap();
    const SkBitmap* readbackBitmap = 0;
    ASSERT(canvasBitmap->config() == SkBitmap::kARGB_8888_Config);
    if (canvasBitmap->width() == m_cachedWidth && canvasBitmap->height() == m_cachedHeight) {
        // This is the fastest and most common case. We read back
        // directly into the canvas's backing store.
        readbackBitmap = canvasBitmap;
        if (m_resizingBitmap) {
            delete m_resizingBitmap;
            m_resizingBitmap = 0;
        }
    } else {
        // We need to allocate a temporary bitmap for reading back the
        // pixel data. We will then use Skia to rescale this bitmap to
        // the size of the canvas's backing store.
        if (m_resizingBitmap && (m_resizingBitmap->width() != m_cachedWidth || m_resizingBitmap->height() != m_cachedHeight)) {
            delete m_resizingBitmap;
            m_resizingBitmap = 0;
        }
        if (!m_resizingBitmap) {
            m_resizingBitmap = new SkBitmap();
            m_resizingBitmap->setConfig(SkBitmap::kARGB_8888_Config,
                                        m_cachedWidth,
                                        m_cachedHeight);
            if (!m_resizingBitmap->allocPixels()) {
                delete m_resizingBitmap;
                m_resizingBitmap = 0;
                return;
            }
        }
        readbackBitmap = m_resizingBitmap;
    }

    // Read back the frame buffer.
    SkAutoLockPixels bitmapLock(*readbackBitmap);
    pixels = static_cast<unsigned char*>(readbackBitmap->getPixels());
    glReadPixels(0, 0, m_cachedWidth, m_cachedHeight, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
#elif PLATFORM(CG)
    if (m_renderOutput) {
        ASSERT(CGBitmapContextGetWidth(m_cgContext) == m_cachedWidth);
        ASSERT(CGBitmapContextGetHeight(m_cgContext) == m_cachedHeight);
        pixels = m_renderOutput;
        glReadPixels(0, 0, m_cachedWidth, m_cachedHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    }
#else
#error Must port to your platform
#endif

#ifdef FLIP_FRAMEBUFFER_VERTICALLY
    if (pixels)
        flipVertically(pixels, m_cachedWidth, m_cachedHeight);
#endif

#if PLATFORM(SKIA)
    if (m_resizingBitmap) {
        // We need to draw the resizing bitmap into the canvas's backing store.
        SkCanvas canvas(*canvasBitmap);
        SkRect dst;
        dst.set(0, 0, canvasBitmap->width(), canvasBitmap->height());
        canvas.drawBitmapRect(*m_resizingBitmap, 0, dst);
    }
#elif PLATFORM(CG)
    if (m_renderOutput) {
        CGImageRef cgImage = CGBitmapContextCreateImage(m_cgContext);
        // CSS styling may cause the canvas's content to be resized on
        // the page. Go back to the Canvas to figure out the correct
        // width and height to draw.
        CGRect rect = CGRectMake(0, 0,
                                 context->canvas()->width(),
                                 context->canvas()->height());
        // We want to completely overwrite the previous frame's
        // rendering results.
        CGContextSetBlendMode(imageBuffer->context()->platformContext(),
                              kCGBlendModeCopy);
        CGContextDrawImage(imageBuffer->context()->platformContext(),
                           rect, cgImage);
        CGImageRelease(cgImage);
    }
#else
#error Must port to your platform
#endif

#endif  // RENDER_TO_DEBUGGING_WINDOW
}

bool GraphicsContext3DInternal::validateTextureTarget(int target)
{
    return (target == GL_TEXTURE_2D || target == GL_TEXTURE_CUBE_MAP);
}

bool GraphicsContext3DInternal::validateTextureParameter(int param)
{
    return (param == GL_TEXTURE_MAG_FILTER
         || param == GL_TEXTURE_MIN_FILTER
         || param == GL_TEXTURE_WRAP_S
         || param == GL_TEXTURE_WRAP_T);
}

void GraphicsContext3DInternal::activeTexture(unsigned long texture)
{
    // FIXME: query number of textures available.
    if (texture < GL_TEXTURE0 || texture > GL_TEXTURE0+32)
        // FIXME: raise exception.
        return;

    makeContextCurrent();
    glActiveTexture(texture);
}

void GraphicsContext3DInternal::bindBuffer(unsigned long target,
                                           CanvasBuffer* buffer)
{
    makeContextCurrent();
    GLuint bufID = EXTRACT(buffer);
    if (target == GL_ARRAY_BUFFER)
        m_boundArrayBuffer = bufID;
    glBindBuffer(target, bufID);
}

// If we didn't have to hack GL_TEXTURE_WRAP_R for cube maps,
// we could just use:
// GL_SAME_METHOD_2_X2(BindTexture, bindTexture, unsigned long, CanvasTexture*)
void GraphicsContext3DInternal::bindTexture(unsigned long target,
                                            CanvasTexture* texture)
{
    makeContextCurrent();
    unsigned int textureObject = EXTRACT(texture);

    glBindTexture(target, textureObject);

    // FIXME: GL_TEXTURE_WRAP_R isn't exposed in the OpenGL ES 2.0
    // API. On desktop OpenGL implementations it seems necessary to
    // set this wrap mode to GL_CLAMP_TO_EDGE to get correct behavior
    // of cube maps.
    if (texture) {
        if (target == GL_TEXTURE_CUBE_MAP) {
            if (!texture->isCubeMapRWrapModeInitialized()) {
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
                texture->setCubeMapRWrapModeInitialized(true);
            }
        } else
            texture->setCubeMapRWrapModeInitialized(false);
    }
}

void GraphicsContext3DInternal::bufferDataImpl(unsigned long target, int size, const void* data, unsigned long usage)
{
    makeContextCurrent();
    // FIXME: make this verification more efficient.
    GLint binding = 0;
    GLenum binding_target = GL_ARRAY_BUFFER_BINDING;
    if (target == GL_ELEMENT_ARRAY_BUFFER)
        binding_target = GL_ELEMENT_ARRAY_BUFFER_BINDING;
    glGetIntegerv(binding_target, &binding);
    if (binding <= 0) {
        // FIXME: raise exception.
        // LogMessagef(("bufferData: no buffer bound"));
        return;
    }

    glBufferData(target,
                   size,
                   data,
                   usage);
}

void GraphicsContext3DInternal::disableVertexAttribArray(unsigned long index)
{
    makeContextCurrent();
    if (index < NumTrackedPointerStates)
        m_vertexAttribPointerState[index].enabled = false;
    glDisableVertexAttribArray(index);
}

void GraphicsContext3DInternal::enableVertexAttribArray(unsigned long index)
{
    makeContextCurrent();
    if (index < NumTrackedPointerStates)
        m_vertexAttribPointerState[index].enabled = true;
    glEnableVertexAttribArray(index);
}

void GraphicsContext3DInternal::vertexAttribPointer(unsigned long indx, int size, int type, bool normalized,
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

void GraphicsContext3DInternal::viewportImpl(long x, long y, unsigned long width, unsigned long height)
{
    glViewport(x, y, width, height);
}

// GraphicsContext3D -----------------------------------------------------

/* Helper macros for when we're just wrapping a gl method, so that
 * we can avoid having to type this 500 times.  Note that these MUST
 * NOT BE USED if we need to check any of the parameters.
 */

#define GL_SAME_METHOD_0(glname, name)                                         \
void GraphicsContext3D::name()                                                 \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname();                                                              \
}

#define GL_SAME_METHOD_1(glname, name, t1)                                     \
void GraphicsContext3D::name(t1 a1)                                            \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1);                                                            \
}

#define GL_SAME_METHOD_1_X(glname, name, t1)                                   \
void GraphicsContext3D::name(t1 a1)                                            \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(EXTRACT(a1));                                                   \
}

#define GL_SAME_METHOD_2(glname, name, t1, t2)                                 \
void GraphicsContext3D::name(t1 a1, t2 a2)                                     \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, a2);                                                        \
}

#define GL_SAME_METHOD_2_X12(glname, name, t1, t2)                             \
void GraphicsContext3D::name(t1 a1, t2 a2)                                     \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(EXTRACT(a1), EXTRACT(a2));                                      \
}

#define GL_SAME_METHOD_2_X2(glname, name, t1, t2)                              \
void GraphicsContext3D::name(t1 a1, t2 a2)                                     \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, EXTRACT(a2));                                               \
}

#define GL_SAME_METHOD_3(glname, name, t1, t2, t3)                             \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3)                              \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, a2, a3);                                                    \
}

#define GL_SAME_METHOD_3_X12(glname, name, t1, t2, t3)                         \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3)                              \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(EXTRACT(a1), EXTRACT(a2), a3);                                  \
}

#define GL_SAME_METHOD_3_X2(glname, name, t1, t2, t3)                          \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3)                              \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, EXTRACT(a2), a3);                                           \
}

#define GL_SAME_METHOD_4(glname, name, t1, t2, t3, t4)                         \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4)                       \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, a2, a3, a4);                                                \
}

#define GL_SAME_METHOD_4_X4(glname, name, t1, t2, t3, t4)                      \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4)                       \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, a2, a3, EXTRACT(a4));                                       \
}

#define GL_SAME_METHOD_5(glname, name, t1, t2, t3, t4, t5)                     \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5)                \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, a2, a3, a4, a5);                                            \
}

#define GL_SAME_METHOD_5_X4(glname, name, t1, t2, t3, t4, t5)                  \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5)                \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, a2, a3, EXTRACT(a4), a5);                                   \
}

#define GL_SAME_METHOD_6(glname, name, t1, t2, t3, t4, t5, t6)                 \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6)         \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, a2, a3, a4, a5, a6);                                        \
}

#define GL_SAME_METHOD_8(glname, name, t1, t2, t3, t4, t5, t6, t7, t8)         \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8)   \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, a2, a3, a4, a5, a6, a7, a8);                                \
}

PassOwnPtr<GraphicsContext3D> GraphicsContext3D::create()
{
    PassOwnPtr<GraphicsContext3D> context = new GraphicsContext3D();
    // FIXME: add error checking
    return context;
}

GraphicsContext3D::GraphicsContext3D()
    : m_currentWidth(0)
    , m_currentHeight(0)
    , m_internal(new GraphicsContext3DInternal())
{
}

GraphicsContext3D::~GraphicsContext3D()
{
}

PlatformGraphicsContext3D GraphicsContext3D::platformGraphicsContext3D() const
{
    return m_internal->platformGraphicsContext3D();
}

Platform3DObject GraphicsContext3D::platformTexture() const
{
    return m_internal->platformTexture();
}

void GraphicsContext3D::checkError() const
{
    m_internal->checkError();
}

void GraphicsContext3D::makeContextCurrent()
{
    m_internal->makeContextCurrent();
}

void GraphicsContext3D::reshape(int width, int height)
{
    if (width == m_currentWidth && height == m_currentHeight)
        return;

    m_currentWidth = width;
    m_currentHeight = height;

    m_internal->reshape(width, height);
}

void GraphicsContext3D::beginPaint(CanvasRenderingContext3D* context)
{
    m_internal->beginPaint(context);
}

void GraphicsContext3D::endPaint()
{
}

int GraphicsContext3D::sizeInBytes(int type)
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
    default: // FIXME: default cases are discouraged in WebKit.
        return 0;
    }
}

unsigned GraphicsContext3D::createBuffer()
{
    makeContextCurrent();
    GLuint o;
    glGenBuffers(1, &o);
    return o;
}

unsigned GraphicsContext3D::createFramebuffer()
{
    makeContextCurrent();
    GLuint o;
    glGenFramebuffers(1, &o);
    return o;
}

unsigned GraphicsContext3D::createProgram()
{
    makeContextCurrent();
    return glCreateProgram();
}

unsigned GraphicsContext3D::createRenderbuffer()
{
    makeContextCurrent();
    GLuint o;
    glGenRenderbuffers(1, &o);
    return o;
}

unsigned GraphicsContext3D::createShader(ShaderType type)
{
    makeContextCurrent();
    return glCreateShader((type == FRAGMENT_SHADER) ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);
}

unsigned GraphicsContext3D::createTexture()
{
    makeContextCurrent();
    GLuint o;
    glGenTextures(1, &o);
    return o;
}

void GraphicsContext3D::deleteBuffer(unsigned buffer)
{
    makeContextCurrent();
    glDeleteBuffers(1, &buffer);
}

void GraphicsContext3D::deleteFramebuffer(unsigned framebuffer)
{
    makeContextCurrent();
    glDeleteFramebuffers(1, &framebuffer);
}

void GraphicsContext3D::deleteProgram(unsigned program)
{
    makeContextCurrent();
    glDeleteProgram(program);
}

void GraphicsContext3D::deleteRenderbuffer(unsigned renderbuffer)
{
    makeContextCurrent();
    glDeleteRenderbuffers(1, &renderbuffer);
}

void GraphicsContext3D::deleteShader(unsigned shader)
{
    makeContextCurrent();
    glDeleteShader(shader);
}

void GraphicsContext3D::deleteTexture(unsigned texture)
{
    makeContextCurrent();
    glDeleteTextures(1, &texture);
}

void GraphicsContext3D::activeTexture(unsigned long texture)
{
    m_internal->activeTexture(texture);
}

GL_SAME_METHOD_2_X12(AttachShader, attachShader, CanvasProgram*, CanvasShader*)

void GraphicsContext3D::bindAttribLocation(CanvasProgram* program,
                                           unsigned long index,
                                           const String& name)
{
    if (!program)
        return;
    makeContextCurrent();
    glBindAttribLocation(EXTRACT(program), index, name.utf8().data());
}

void GraphicsContext3D::bindBuffer(unsigned long target,
                                   CanvasBuffer* buffer)
{
    m_internal->bindBuffer(target, buffer);
}

GL_SAME_METHOD_2_X2(BindFramebuffer, bindFramebuffer, unsigned long, CanvasFramebuffer*)

GL_SAME_METHOD_2_X2(BindRenderbuffer, bindRenderbuffer, unsigned long, CanvasRenderbuffer*)

// If we didn't have to hack GL_TEXTURE_WRAP_R for cube maps,
// we could just use:
// GL_SAME_METHOD_2_X2(BindTexture, bindTexture, unsigned long, CanvasTexture*)
void GraphicsContext3D::bindTexture(unsigned long target,
                                    CanvasTexture* texture)
{
    m_internal->bindTexture(target, texture);
}

GL_SAME_METHOD_4(BlendColor, blendColor, double, double, double, double)

GL_SAME_METHOD_1(BlendEquation, blendEquation, unsigned long)

GL_SAME_METHOD_2(BlendEquationSeparate, blendEquationSeparate, unsigned long, unsigned long)

GL_SAME_METHOD_2(BlendFunc, blendFunc, unsigned long, unsigned long)

GL_SAME_METHOD_4(BlendFuncSeparate, blendFuncSeparate, unsigned long, unsigned long, unsigned long, unsigned long)

void GraphicsContext3D::bufferData(unsigned long target, int size, unsigned long usage)
{
    m_internal->bufferDataImpl(target, size, 0, usage);
}

void GraphicsContext3D::bufferData(unsigned long target, CanvasArray* array, unsigned long usage)
{
    m_internal->bufferDataImpl(target, array->sizeInBytes(), array->baseAddress(), usage);
}

void GraphicsContext3D::bufferSubData(unsigned long target, long offset, CanvasArray* array)
{
    if (!array || !array->length())
        return;

    makeContextCurrent();
    // FIXME: make this verification more efficient.
    GLint binding = 0;
    GLenum binding_target = GL_ARRAY_BUFFER_BINDING;
    if (target == GL_ELEMENT_ARRAY_BUFFER)
        binding_target = GL_ELEMENT_ARRAY_BUFFER_BINDING;
    glGetIntegerv(binding_target, &binding);
    if (binding <= 0) {
        // FIXME: raise exception.
        // LogMessagef(("bufferSubData: no buffer bound"));
        return;
    }
    glBufferSubData(target, offset, array->sizeInBytes(), array->baseAddress());
}

unsigned long GraphicsContext3D::checkFramebufferStatus(unsigned long target)
{
    makeContextCurrent();
    return glCheckFramebufferStatus(target);
}

GL_SAME_METHOD_1(Clear, clear, unsigned long)

GL_SAME_METHOD_4(ClearColor, clearColor, double, double, double, double)

GL_SAME_METHOD_1(ClearDepth, clearDepth, double)

GL_SAME_METHOD_1(ClearStencil, clearStencil, long)

GL_SAME_METHOD_4(ColorMask, colorMask, bool, bool, bool, bool)

GL_SAME_METHOD_1_X(CompileShader, compileShader, CanvasShader*)

GL_SAME_METHOD_8(CopyTexImage2D, copyTexImage2D, unsigned long, long, unsigned long, long, long, unsigned long, unsigned long, long)

GL_SAME_METHOD_8(CopyTexSubImage2D, copyTexSubImage2D, unsigned long, long, long, long, long, long, unsigned long, unsigned long)

GL_SAME_METHOD_1(CullFace, cullFace, unsigned long)

GL_SAME_METHOD_1(DepthFunc, depthFunc, unsigned long)

GL_SAME_METHOD_1(DepthMask, depthMask, bool)

GL_SAME_METHOD_2(DepthRange, depthRange, double, double)

void GraphicsContext3D::detachShader(CanvasProgram* program, CanvasShader* shader)
{
    if (!program || !shader)
        return;

    makeContextCurrent();
    glDetachShader(EXTRACT(program), EXTRACT(shader));
}

GL_SAME_METHOD_1(Disable, disable, unsigned long)

void GraphicsContext3D::disableVertexAttribArray(unsigned long index)
{
    m_internal->disableVertexAttribArray(index);
}

void GraphicsContext3D::drawArrays(unsigned long mode, long first, long count)
{
    switch (mode) {
    case GL_TRIANGLES:
    case GL_TRIANGLE_STRIP:
    case GL_TRIANGLE_FAN:
    case GL_POINTS:
    case GL_LINE_STRIP:
    case GL_LINE_LOOP:
    case GL_LINES:
        break;
    default: // FIXME: default cases are discouraged in WebKit.
        // FIXME: output log message, raise exception.
        // LogMessage(NS_LITERAL_CSTRING("drawArrays: invalid mode"));
        // return NS_ERROR_DOM_SYNTAX_ERR;
        return;
    }

    if (first+count < first || first+count < count) {
        // FIXME: output log message, raise exception.
        // LogMessage(NS_LITERAL_CSTRING("drawArrays: overflow in first+count"));
        // return NS_ERROR_INVALID_ARG;
        return;
    }

    // FIXME: validate against currently bound buffer.
    // if (!ValidateBuffers(first+count))
    //     return NS_ERROR_INVALID_ARG;

    makeContextCurrent();
    glDrawArrays(mode, first, count);
}

void GraphicsContext3D::drawElements(unsigned long mode, unsigned long count, unsigned long type, long offset)
{
    makeContextCurrent();
    // FIXME: make this verification more efficient.
    GLint binding = 0;
    GLenum binding_target = GL_ELEMENT_ARRAY_BUFFER_BINDING;
    glGetIntegerv(binding_target, &binding);
    if (binding <= 0) {
        // FIXME: raise exception.
        // LogMessagef(("bufferData: no buffer bound"));
        return;
    }
    glDrawElements(mode, count, type,
                   reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

GL_SAME_METHOD_1(Enable, enable, unsigned long)

void GraphicsContext3D::enableVertexAttribArray(unsigned long index)
{
    m_internal->enableVertexAttribArray(index);
}

GL_SAME_METHOD_0(Finish, finish)

GL_SAME_METHOD_0(Flush, flush)

GL_SAME_METHOD_4_X4(FramebufferRenderbuffer, framebufferRenderbuffer, unsigned long, unsigned long, unsigned long, CanvasRenderbuffer*)

GL_SAME_METHOD_5_X4(FramebufferTexture2D, framebufferTexture2D, unsigned long, unsigned long, unsigned long, CanvasTexture*, long)

GL_SAME_METHOD_1(FrontFace, frontFace, unsigned long)

void GraphicsContext3D::generateMipmap(unsigned long target)
{
    makeContextCurrent();
    if (glGenerateMipmapEXT)
        glGenerateMipmapEXT(target);
    // FIXME: provide alternative code path? This will be unpleasant
    // to implement if glGenerateMipmapEXT is not available -- it will
    // require a texture readback and re-upload.
}

bool GraphicsContext3D::getActiveAttrib(CanvasProgram* program, unsigned long index, ActiveInfo& info)
{
    if (!program)
        return false;
    GLint maxNameLength = -1;
    glGetProgramiv(EXTRACT(program), GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxNameLength);
    if (maxNameLength < 0)
        return false;
    GLchar* name = 0;
    if (!tryFastMalloc(maxNameLength * sizeof(GLchar)).getValue(name))
        return false;
    GLsizei length = 0;
    GLint size = -1;
    GLenum type = 0;
    glGetActiveAttrib(EXTRACT(program), index, maxNameLength,
                      &length, &size, &type, name);
    if (size < 0) {
        fastFree(name);
        return false;
    }
    info.name = String(name, length);
    info.type = type;
    info.size = size;
    fastFree(name);
    return true;
}

bool GraphicsContext3D::getActiveUniform(CanvasProgram* program, unsigned long index, ActiveInfo& info)
{
    if (!program)
        return false;
    GLint maxNameLength = -1;
    glGetProgramiv(EXTRACT(program), GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxNameLength);
    if (maxNameLength < 0)
        return false;
    GLchar* name = 0;
    if (!tryFastMalloc(maxNameLength * sizeof(GLchar)).getValue(name))
        return false;
    GLsizei length = 0;
    GLint size = -1;
    GLenum type = 0;
    glGetActiveUniform(EXTRACT(program), index, maxNameLength,
                       &length, &size, &type, name);
    if (size < 0) {
        fastFree(name);
        return false;
    }
    info.name = String(name, length);
    info.type = type;
    info.size = size;
    fastFree(name);
    return true;
}

int GraphicsContext3D::getAttribLocation(CanvasProgram* program, const String& name)
{
    if (!program)
        return -1;

    makeContextCurrent();
    return glGetAttribLocation(EXTRACT(program), name.utf8().data());
}

bool GraphicsContext3D::getBoolean(unsigned long pname)
{
    makeContextCurrent();
    GLboolean val;
    // FIXME: validate pname to ensure it returns only a single value.
    glGetBooleanv(pname, &val);
    return static_cast<bool>(val);
}

PassRefPtr<CanvasUnsignedByteArray> GraphicsContext3D::getBooleanv(unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return 0;
}

int GraphicsContext3D::getBufferParameteri(unsigned long target, unsigned long pname)
{
    makeContextCurrent();
    GLint data;
    glGetBufferParameteriv(target, pname, &data);
    return static_cast<int>(data);
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getBufferParameteriv(unsigned long target, unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return 0;
}

unsigned long GraphicsContext3D::getError()
{
    makeContextCurrent();
    return glGetError();
}

float GraphicsContext3D::getFloat(unsigned long pname)
{
    makeContextCurrent();
    GLfloat val;
    // FIXME: validate pname to ensure it returns only a single value.
    glGetFloatv(pname, &val);
    return static_cast<float>(val);
}

PassRefPtr<CanvasFloatArray> GraphicsContext3D::getFloatv(unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return 0;
}

int GraphicsContext3D::getFramebufferAttachmentParameteri(unsigned long target,
                                                          unsigned long attachment,
                                                          unsigned long pname)
{
    makeContextCurrent();
    GLint data;
    glGetFramebufferAttachmentParameteriv(target, attachment, pname, &data);
    return static_cast<int>(data);
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getFramebufferAttachmentParameteriv(unsigned long target,
                                                                                  unsigned long attachment,
                                                                                  unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return 0;
}

int GraphicsContext3D::getInteger(unsigned long pname)
{
    makeContextCurrent();
    GLint val;
    // FIXME: validate pname to ensure it returns only a single value.
    glGetIntegerv(pname, &val);
    return static_cast<int>(val);
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getIntegerv(unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return 0;
}

int GraphicsContext3D::getProgrami(CanvasProgram* program,
                                   unsigned long pname)
{
    makeContextCurrent();
    GLint param;
    glGetProgramiv(EXTRACT(program), pname, &param);
    return static_cast<int>(param);
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getProgramiv(CanvasProgram* program,
                                                           unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return 0;
}

String GraphicsContext3D::getProgramInfoLog(CanvasProgram* program)
{
    makeContextCurrent();
    GLuint programID = EXTRACT(program);
    GLint logLength;
    glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &logLength);
    if (!logLength)
        return String();
    GLchar* log = 0;
    if (!tryFastMalloc(logLength * sizeof(GLchar)).getValue(log))
        return String();
    GLsizei returnedLogLength;
    glGetProgramInfoLog(programID, logLength, &returnedLogLength, log);
    ASSERT(logLength == returnedLogLength + 1);
    String res = String(log, returnedLogLength);
    fastFree(log);
    return res;
}

int GraphicsContext3D::getRenderbufferParameteri(unsigned long target,
                                                 unsigned long pname)
{
    makeContextCurrent();
    GLint param;
    glGetRenderbufferParameteriv(target, pname, &param);
    return static_cast<int>(param);
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getRenderbufferParameteriv(unsigned long target,
                                                                         unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return 0;
}

int GraphicsContext3D::getShaderi(CanvasShader* shader,
                                  unsigned long pname)
{
    makeContextCurrent();
    GLint param;
    glGetShaderiv(EXTRACT(shader), pname, &param);
    return static_cast<int>(param);
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getShaderiv(CanvasShader* shader,
                                                          unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return 0;
}

String GraphicsContext3D::getShaderInfoLog(CanvasShader* shader)
{
    makeContextCurrent();
    GLuint shaderID = EXTRACT(shader);
    GLint logLength;
    glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &logLength);
    if (!logLength)
        return String();
    GLchar* log = 0;
    if (!tryFastMalloc(logLength * sizeof(GLchar)).getValue(log))
        return String();
    GLsizei returnedLogLength;
    glGetShaderInfoLog(shaderID, logLength, &returnedLogLength, log);
    ASSERT(logLength == returnedLogLength + 1);
    String res = String(log, returnedLogLength);
    fastFree(log);
    return res;
}

String GraphicsContext3D::getShaderSource(CanvasShader* shader)
{
    makeContextCurrent();
    GLuint shaderID = EXTRACT(shader);
    GLint logLength;
    glGetShaderiv(shaderID, GL_SHADER_SOURCE_LENGTH, &logLength);
    if (!logLength)
        return String();
    GLchar* log = 0;
    if (!tryFastMalloc(logLength * sizeof(GLchar)).getValue(log))
        return String();
    GLsizei returnedLogLength;
    glGetShaderSource(shaderID, logLength, &returnedLogLength, log);
    ASSERT(logLength == returnedLogLength + 1);
    String res = String(log, returnedLogLength);
    fastFree(log);
    return res;
}

String GraphicsContext3D::getString(unsigned long name)
{
    makeContextCurrent();
    return String(reinterpret_cast<const char*>(glGetString(name)));
}

float GraphicsContext3D::getTexParameterf(unsigned long target, unsigned long pname)
{
    makeContextCurrent();
    if (!m_internal->validateTextureTarget(target)) {
        // FIXME: throw exception.
        return 0;
    }

    if (!m_internal->validateTextureParameter(pname)) {
        // FIXME: throw exception.
        return 0;
    }

    GLfloat param;
    glGetTexParameterfv(target, pname, &param);
    return static_cast<float>(param);
}

PassRefPtr<CanvasFloatArray> GraphicsContext3D::getTexParameterfv(unsigned long target, unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return 0;
}

int GraphicsContext3D::getTexParameteri(unsigned long target, unsigned long pname)
{
    makeContextCurrent();
    if (!m_internal->validateTextureTarget(target)) {
        // FIXME: throw exception.
        return 0;
    }

    if (!m_internal->validateTextureParameter(pname)) {
        // FIXME: throw exception.
        return 0;
    }

    GLint param;
    glGetTexParameteriv(target, pname, &param);
    return static_cast<int>(param);
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getTexParameteriv(unsigned long target, unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return 0;
}

float GraphicsContext3D::getUniformf(CanvasProgram* program, long location)
{
    // FIXME: implement.
    notImplemented();
    return 0;
}

PassRefPtr<CanvasFloatArray> GraphicsContext3D::getUniformfv(CanvasProgram* program, long location)
{
    // FIXME: implement.
    notImplemented();
    return 0;
}

int GraphicsContext3D::getUniformi(CanvasProgram* program, long location)
{
    // FIXME: implement.
    notImplemented();
    return 0;
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getUniformiv(CanvasProgram* program, long location)
{
    // FIXME: implement.
    notImplemented();
    return 0;
}

long GraphicsContext3D::getUniformLocation(CanvasProgram* program, const String& name)
{
    if (!program)
        return -1;

    makeContextCurrent();
    return glGetUniformLocation(EXTRACT(program), name.utf8().data());
}

float GraphicsContext3D::getVertexAttribf(unsigned long index,
                                          unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return 0;
}

PassRefPtr<CanvasFloatArray> GraphicsContext3D::getVertexAttribfv(unsigned long index,
                                                                  unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return 0;
}

int GraphicsContext3D::getVertexAttribi(unsigned long index,
                                        unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return 0;
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getVertexAttribiv(unsigned long index,
                                                                unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return 0;
}

long GraphicsContext3D::getVertexAttribOffset(unsigned long index, unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return 0;
}

GL_SAME_METHOD_2(Hint, hint, unsigned long, unsigned long);

bool GraphicsContext3D::isBuffer(CanvasBuffer* buffer)
{
    makeContextCurrent();
    return glIsBuffer(EXTRACT(buffer));
}

bool GraphicsContext3D::isEnabled(unsigned long cap)
{
    makeContextCurrent();
    return glIsEnabled(cap);
}

bool GraphicsContext3D::isFramebuffer(CanvasFramebuffer* framebuffer)
{
    makeContextCurrent();
    return glIsFramebuffer(EXTRACT(framebuffer));
}

bool GraphicsContext3D::isProgram(CanvasProgram* program)
{
    makeContextCurrent();
    return glIsProgram(EXTRACT(program));
}

bool GraphicsContext3D::isRenderbuffer(CanvasRenderbuffer* renderbuffer)
{
    makeContextCurrent();
    return glIsRenderbuffer(EXTRACT(renderbuffer));
}

bool GraphicsContext3D::isShader(CanvasShader* shader)
{
    makeContextCurrent();
    return glIsShader(EXTRACT(shader));
}

bool GraphicsContext3D::isTexture(CanvasTexture* texture)
{
    makeContextCurrent();
    return glIsTexture(EXTRACT(texture));
}

GL_SAME_METHOD_1(LineWidth, lineWidth, double)

GL_SAME_METHOD_1_X(LinkProgram, linkProgram, CanvasProgram*)

void GraphicsContext3D::pixelStorei(unsigned long pname, long param)
{
    if (pname != GL_PACK_ALIGNMENT && pname != GL_UNPACK_ALIGNMENT) {
        // FIXME: Create a fake GL error and throw an exception.
        return;
    }

    makeContextCurrent();
    glPixelStorei(pname, param);
}

GL_SAME_METHOD_2(PolygonOffset, polygonOffset, double, double)

PassRefPtr<CanvasArray> GraphicsContext3D::readPixels(long x, long y,
                                                      unsigned long width, unsigned long height,
                                                      unsigned long format, unsigned long type) {
    // FIXME: support more pixel formats and types.
    if (!((format == GL_RGBA) && (type == GL_UNSIGNED_BYTE)))
        return 0;

    // FIXME: take into account pack alignment.
    RefPtr<CanvasUnsignedByteArray> array = CanvasUnsignedByteArray::create(width * height * 4);
    glReadPixels(x, y, width, height, format, type, array->baseAddress());
    return array;
}

void GraphicsContext3D::releaseShaderCompiler()
{
}

GL_SAME_METHOD_4(RenderbufferStorage, renderbufferStorage, unsigned long, unsigned long, unsigned long, unsigned long)

GL_SAME_METHOD_2(SampleCoverage, sampleCoverage, double, bool)

GL_SAME_METHOD_4(Scissor, scissor, long, long, unsigned long, unsigned long)

void GraphicsContext3D::shaderSource(CanvasShader* shader, const String& source)
{
    makeContextCurrent();
    CString str = source.utf8();
    const char* data = str.data();
    GLint length = str.length();
    glShaderSource(EXTRACT(shader), 1, &data, &length);
}

GL_SAME_METHOD_3(StencilFunc, stencilFunc, unsigned long, long, unsigned long)

GL_SAME_METHOD_4(StencilFuncSeparate, stencilFuncSeparate, unsigned long, unsigned long, long, unsigned long)

GL_SAME_METHOD_1(StencilMask, stencilMask, unsigned long)

GL_SAME_METHOD_2(StencilMaskSeparate, stencilMaskSeparate, unsigned long, unsigned long)

GL_SAME_METHOD_3(StencilOp, stencilOp, unsigned long, unsigned long, unsigned long)

GL_SAME_METHOD_4(StencilOpSeparate, stencilOpSeparate, unsigned long, unsigned long, unsigned long, unsigned long)

int GraphicsContext3D::texImage2D(unsigned target,
                                  unsigned level,
                                  unsigned internalformat,
                                  unsigned width,
                                  unsigned height,
                                  unsigned border,
                                  unsigned format,
                                  unsigned type,
                                  CanvasArray* pixels)
{
    // FIXME: must do validation similar to JOGL's to ensure that
    // the incoming array is of the appropriate length.
    glTexImage2D(target,
                 level,
                 internalformat,
                 width,
                 height,
                 border,
                 format,
                 type,
                 pixels->baseAddress());
    return 0;
}

int GraphicsContext3D::texImage2D(unsigned target,
                                  unsigned level,
                                  unsigned internalformat,
                                  unsigned width,
                                  unsigned height,
                                  unsigned border,
                                  unsigned format,
                                  unsigned type,
                                  ImageData* pixels)
{
    // FIXME: implement.
    notImplemented();
    return -1;
}

// Remove premultiplied alpha from color channels.
// FIXME: this is lossy. Must retrieve original values from HTMLImageElement.
static void unmultiplyAlpha(unsigned char* rgbaData, int numPixels)
{
    for (int j = 0; j < numPixels; j++) {
        float b = rgbaData[4*j+0] / 255.0f;
        float g = rgbaData[4*j+1] / 255.0f;
        float r = rgbaData[4*j+2] / 255.0f;
        float a = rgbaData[4*j+3] / 255.0f;
        if (a > 0.0f) {
            b /= a;
            g /= a;
            r /= a;
            b = (b > 1.0f) ? 1.0f : b;
            g = (g > 1.0f) ? 1.0f : g;
            r = (r > 1.0f) ? 1.0f : r;
            rgbaData[4*j+0] = (unsigned char) (b * 255.0f);
            rgbaData[4*j+1] = (unsigned char) (g * 255.0f);
            rgbaData[4*j+2] = (unsigned char) (r * 255.0f);
        }
    }
}

// FIXME: this must be changed to refer to the original image data
// rather than unmultiplying the alpha channel.
static int texImage2DHelper(unsigned target, unsigned level,
                            int width, int height,
                            int rowBytes,
                            bool flipY,
                            bool premultiplyAlpha,
                            GLenum format,
                            bool skipAlpha,
                            unsigned char* pixels)
{
    ASSERT(format == GL_RGBA || format == GL_BGRA);
    GLint internalFormat = GL_RGBA8;
    if (skipAlpha) {
        internalFormat = GL_RGB8;
        // Ignore the alpha channel
        premultiplyAlpha = true;
    }
    if (flipY) {
        // Need to flip images vertically. To avoid making a copy of
        // the entire image, we perform a ton of glTexSubImage2D
        // calls. FIXME: should rethink this strategy for efficiency.
        glTexImage2D(target, level, internalFormat,
                     width,
                     height,
                     0,
                     format,
                     GL_UNSIGNED_BYTE,
                     0);
        unsigned char* row = 0;
        bool allocatedRow = false;
        if (!premultiplyAlpha) {
            row = new unsigned char[rowBytes];
            allocatedRow = true;
        }
        for (int i = 0; i < height; i++) {
            if (premultiplyAlpha)
                row = pixels + (rowBytes * i);
            else {
                memcpy(row, pixels + (rowBytes * i), rowBytes);
                unmultiplyAlpha(row, width);
            }
            glTexSubImage2D(target, level, 0, height - i - 1,
                            width, 1,
                            format,
                            GL_UNSIGNED_BYTE,
                            row);
        }
        if (allocatedRow)
            delete[] row;
    } else {
        // The pixels of cube maps' faces are defined with a top-down
        // scanline ordering, unlike GL_TEXTURE_2D, so when uploading
        // these, the above vertical flip is the wrong thing to do.
        if (premultiplyAlpha)
            glTexImage2D(target, level, internalFormat,
                         width,
                         height,
                         0,
                         format,
                         GL_UNSIGNED_BYTE,
                         pixels);
        else {
            glTexImage2D(target, level, internalFormat,
                         width,
                         height,
                         0,
                         format,
                         GL_UNSIGNED_BYTE,
                         0);
            unsigned char* row = new unsigned char[rowBytes];
            for (int i = 0; i < height; i++) {
                memcpy(row, pixels + (rowBytes * i), rowBytes);
                unmultiplyAlpha(row, width);
                glTexSubImage2D(target, level, 0, i,
                                width, 1,
                                format,
                                GL_UNSIGNED_BYTE,
                                row);
            }
            delete[] row;
        }
    }
    return 0;
}

int GraphicsContext3D::texImage2D(unsigned target, unsigned level, Image* image,
                                  bool flipY, bool premultiplyAlpha)
{
    ASSERT(image);

    int res = -1;
#if PLATFORM(SKIA)
    NativeImageSkia* skiaImage = image->nativeImageForCurrentFrame();
    if (!skiaImage) {
        ASSERT_NOT_REACHED();
        return -1;
    }
    SkBitmap::Config skiaConfig = skiaImage->config();
    // FIXME: must support more image configurations.
    if (skiaConfig != SkBitmap::kARGB_8888_Config) {
        ASSERT_NOT_REACHED();
        return -1;
    }
    SkBitmap& skiaImageRef = *skiaImage;
    SkAutoLockPixels lock(skiaImageRef);
    int width = skiaImage->width();
    int height = skiaImage->height();
    unsigned char* pixels =
        reinterpret_cast<unsigned char*>(skiaImage->getPixels());
    int rowBytes = skiaImage->rowBytes();
    res = texImage2DHelper(target, level,
                           width, height,
                           rowBytes,
                           flipY, premultiplyAlpha,
                           GL_BGRA,
                           false,
                           pixels);
#elif PLATFORM(CG)
    CGImageRef cgImage = image->nativeImageForCurrentFrame();
    if (!cgImage) {
        ASSERT_NOT_REACHED();
        return -1;
    }
    int width = CGImageGetWidth(cgImage);
    int height = CGImageGetHeight(cgImage);
    int rowBytes = width * 4;
    CGImageAlphaInfo info = CGImageGetAlphaInfo(cgImage);
    bool skipAlpha = (info == kCGImageAlphaNone
                   || info == kCGImageAlphaNoneSkipLast
                   || info == kCGImageAlphaNoneSkipFirst);
    unsigned char* imageData = new unsigned char[height * rowBytes];
    CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
    CGContextRef tmpContext = CGBitmapContextCreate(imageData, width, height, 8, rowBytes,
                                                    colorSpace,
                                                    kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(colorSpace);
    CGContextDrawImage(tmpContext,
                       CGRectMake(0, 0, static_cast<CGFloat>(width), static_cast<CGFloat>(height)),
                       cgImage);
    CGContextRelease(tmpContext);
    res = texImage2DHelper(target, level, width, height, rowBytes,
                           flipY, premultiplyAlpha, GL_RGBA, skipAlpha, imageData);
    delete[] imageData;
#else
#error Must port to your platform
#endif
    return res;
}

int GraphicsContext3D::texImage2D(unsigned target, unsigned level, HTMLVideoElement* video,
                                  bool flipY, bool premultiplyAlpha)
{
    // FIXME: implement.
    notImplemented();
    return -1;
}

GL_SAME_METHOD_3(TexParameterf, texParameterf, unsigned, unsigned, float);

GL_SAME_METHOD_3(TexParameteri, texParameteri, unsigned, unsigned, int);

int GraphicsContext3D::texSubImage2D(unsigned target,
                                     unsigned level,
                                     unsigned xoffset,
                                     unsigned yoffset,
                                     unsigned width,
                                     unsigned height,
                                     unsigned format,
                                     unsigned type,
                                     CanvasArray* pixels)
{
    // FIXME: implement.
    notImplemented();
    return -1;
}

int GraphicsContext3D::texSubImage2D(unsigned target,
                                     unsigned level,
                                     unsigned xoffset,
                                     unsigned yoffset,
                                     unsigned width,
                                     unsigned height,
                                     unsigned format,
                                     unsigned type,
                                     ImageData* pixels)
{
    // FIXME: implement.
    notImplemented();
    return -1;
}

int GraphicsContext3D::texSubImage2D(unsigned target,
                                     unsigned level,
                                     unsigned xoffset,
                                     unsigned yoffset,
                                     unsigned width,
                                     unsigned height,
                                     Image* image,
                                     bool flipY,
                                     bool premultiplyAlpha)
{
    // FIXME: implement.
    notImplemented();
    return -1;
}

int GraphicsContext3D::texSubImage2D(unsigned target,
                                     unsigned level,
                                     unsigned xoffset,
                                     unsigned yoffset,
                                     unsigned width,
                                     unsigned height,
                                     HTMLVideoElement* video,
                                     bool flipY,
                                     bool premultiplyAlpha)
{
    // FIXME: implement.
    notImplemented();
    return -1;
}

GL_SAME_METHOD_2(Uniform1f, uniform1f, long, float)

void GraphicsContext3D::uniform1fv(long location, float* v, int size)
{
    makeContextCurrent();
    glUniform1fv(location, size, v);
}

GL_SAME_METHOD_2(Uniform1i, uniform1i, long, int)

void GraphicsContext3D::uniform1iv(long location, int* v, int size)
{
    makeContextCurrent();
    glUniform1iv(location, size, v);
}

GL_SAME_METHOD_3(Uniform2f, uniform2f, long, float, float)

void GraphicsContext3D::uniform2fv(long location, float* v, int size)
{
    makeContextCurrent();
    glUniform2fv(location, size, v);
}

GL_SAME_METHOD_3(Uniform2i, uniform2i, long, int, int)

void GraphicsContext3D::uniform2iv(long location, int* v, int size)
{
    makeContextCurrent();
    glUniform2iv(location, size, v);
}

GL_SAME_METHOD_4(Uniform3f, uniform3f, long, float, float, float)

void GraphicsContext3D::uniform3fv(long location, float* v, int size)
{
    makeContextCurrent();
    glUniform3fv(location, size, v);
}

GL_SAME_METHOD_4(Uniform3i, uniform3i, long, int, int, int)

void GraphicsContext3D::uniform3iv(long location, int* v, int size)
{
    makeContextCurrent();
    glUniform3iv(location, size, v);
}

GL_SAME_METHOD_5(Uniform4f, uniform4f, long, float, float, float, float)

void GraphicsContext3D::uniform4fv(long location, float* v, int size)
{
    makeContextCurrent();
    glUniform4fv(location, size, v);
}

GL_SAME_METHOD_5(Uniform4i, uniform4i, long, int, int, int, int)

void GraphicsContext3D::uniform4iv(long location, int* v, int size)
{
    makeContextCurrent();
    glUniform4iv(location, size, v);
}

void GraphicsContext3D::uniformMatrix2fv(long location, bool transpose, float* value, int size)
{
    makeContextCurrent();
    glUniformMatrix2fv(location, size, transpose, value);
}

void GraphicsContext3D::uniformMatrix3fv(long location, bool transpose, float* value, int size)
{
    makeContextCurrent();
    glUniformMatrix3fv(location, size, transpose, value);
}

void GraphicsContext3D::uniformMatrix4fv(long location, bool transpose, float* value, int size)
{
    makeContextCurrent();
    glUniformMatrix4fv(location, size, transpose, value);
}

GL_SAME_METHOD_1_X(UseProgram, useProgram, CanvasProgram*)

GL_SAME_METHOD_1_X(ValidateProgram, validateProgram, CanvasProgram*)

GL_SAME_METHOD_2(VertexAttrib1f, vertexAttrib1f, unsigned long, float)

void GraphicsContext3D::vertexAttrib1fv(unsigned long indx, float* values)
{
    makeContextCurrent();
    glVertexAttrib1fv(indx, values);
}

GL_SAME_METHOD_3(VertexAttrib2f, vertexAttrib2f, unsigned long, float, float)

void GraphicsContext3D::vertexAttrib2fv(unsigned long indx, float* values)
{
    makeContextCurrent();
    glVertexAttrib2fv(indx, values);
}

GL_SAME_METHOD_4(VertexAttrib3f, vertexAttrib3f, unsigned long, float, float, float)

void GraphicsContext3D::vertexAttrib3fv(unsigned long indx, float* values)
{
    makeContextCurrent();
    glVertexAttrib3fv(indx, values);
}

GL_SAME_METHOD_5(VertexAttrib4f, vertexAttrib4f, unsigned long, float, float, float, float)

void GraphicsContext3D::vertexAttrib4fv(unsigned long indx, float* values)
{
    makeContextCurrent();
    glVertexAttrib4fv(indx, values);
}

void GraphicsContext3D::vertexAttribPointer(unsigned long indx, int size, int type, bool normalized,
                                            unsigned long stride, unsigned long offset)
{
    m_internal->vertexAttribPointer(indx, size, type, normalized, stride, offset);
}

void GraphicsContext3D::viewport(long x, long y, unsigned long width, unsigned long height)
{
    makeContextCurrent();
    m_internal->viewportImpl(x, y, width, height);
}

}

#endif // ENABLE(3D_CANVAS)
