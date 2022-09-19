//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayGbm.cpp: Gbm implementation of egl::Display

#include "libANGLE/renderer/gl/egl/gbm/DisplayGbm.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/time.h>
#include <unistd.h>

#include <EGL/eglext.h>

#include <drm_fourcc.h>
#include <gbm.h>

#include "common/debug.h"
#include "libANGLE/Config.h"
#include "libANGLE/Display.h"
#include "libANGLE/Surface.h"
#include "libANGLE/renderer/gl/ContextGL.h"
#include "libANGLE/renderer/gl/FramebufferGL.h"
#include "libANGLE/renderer/gl/RendererGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"
#include "libANGLE/renderer/gl/egl/ContextEGL.h"
#include "libANGLE/renderer/gl/egl/DisplayEGL.h"
#include "libANGLE/renderer/gl/egl/FunctionsEGLDL.h"
#include "libANGLE/renderer/gl/egl/gbm/SurfaceGbm.h"
#include "platform/PlatformMethods.h"

// ARM-specific extension needed to make Mali GPU behave - not in any
// published header file.
#ifndef EGL_SYNC_PRIOR_COMMANDS_IMPLICIT_EXTERNAL_ARM
#    define EGL_SYNC_PRIOR_COMMANDS_IMPLICIT_EXTERNAL_ARM 0x328A
#endif

#ifndef EGL_NO_CONFIG_MESA
#    define EGL_NO_CONFIG_MESA ((EGLConfig)0)
#endif

namespace
{

EGLint UnsignedToSigned(uint32_t u)
{
    return *reinterpret_cast<const EGLint *>(&u);
}

drmModeModeInfoPtr ChooseMode(drmModeConnectorPtr conn)
{
    drmModeModeInfoPtr mode = nullptr;
    ASSERT(conn);
    ASSERT(conn->connection == DRM_MODE_CONNECTED);
    // use first preferred mode if any, else end up with last mode in list
    for (int i = 0; i < conn->count_modes; ++i)
    {
        mode = conn->modes + i;
        if (mode->type & DRM_MODE_TYPE_PREFERRED)
        {
            break;
        }
    }
    return mode;
}

int ChooseCRTC(int fd, drmModeConnectorPtr conn)
{
    for (int i = 0; i < conn->count_encoders; ++i)
    {
        drmModeEncoderPtr enc = drmModeGetEncoder(fd, conn->encoders[i]);
        unsigned long crtcs   = enc->possible_crtcs;
        drmModeFreeEncoder(enc);
        if (crtcs)
        {
            return __builtin_ctzl(crtcs);
        }
    }
    return -1;
}
}  // namespace

namespace rx
{

DisplayGbm::Buffer::Buffer(DisplayGbm *display,
                           uint32_t useFlags,
                           uint32_t gbmFormat,
                           uint32_t drmFormat,
                           uint32_t drmFormatFB,
                           int depthBits,
                           int stencilBits)
    : mDisplay(display),
      mNative(nullptr),
      mWidth(0),
      mHeight(0),
      mDepthBits(depthBits),
      mStencilBits(stencilBits),
      mUseFlags(useFlags),
      mGBMFormat(gbmFormat),
      mDRMFormat(drmFormat),
      mDRMFormatFB(drmFormatFB),
      mBO(nullptr),
      mDMABuf(-1),
      mHasDRMFB(false),
      mDRMFB(0),
      mImage(EGL_NO_IMAGE_KHR),
      mColorBuffer(0),
      mDSBuffer(0),
      mTexture(0),
      mFramebufferID(0)
{}

DisplayGbm::Buffer::~Buffer()
{
    reset();

    const FunctionsGL *gl = mDisplay->getRenderer()->getFunctions();
    gl->deleteFramebuffers(1, &mFramebufferID);
    mFramebufferID = 0;
    gl->deleteRenderbuffers(1, &mColorBuffer);
    mColorBuffer = 0;
    gl->deleteRenderbuffers(1, &mDSBuffer);
    mDSBuffer = 0;
}

void DisplayGbm::Buffer::reset()
{
    if (mHasDRMFB)
    {
        int fd = gbm_device_get_fd(mDisplay->mGBM);
        drmModeRmFB(fd, mDRMFB);
        mHasDRMFB = false;
    }

    // Make sure to keep the color and depth stencil buffers alive so they maintain the same GL IDs
    // if they are bound to any emulated default framebuffer.

    if (mImage != EGL_NO_IMAGE_KHR)
    {
        mDisplay->getFunctionsEGL()->destroyImageKHR(mImage);
        mImage = EGL_NO_IMAGE_KHR;
    }

    if (mTexture)
    {
        const FunctionsGL *gl = mDisplay->getRenderer()->getFunctions();
        gl->deleteTextures(1, &mTexture);
        mTexture = 0;
    }

    if (mDMABuf >= 0)
    {
        close(mDMABuf);
        mDMABuf = -1;
    }

    if (mBO)
    {
        gbm_bo_destroy(mBO);
        mBO = nullptr;
    }
}

bool DisplayGbm::Buffer::resize(int32_t width, int32_t height)
{
    if (mWidth == width && mHeight == height)
    {
        return true;
    }

    reset();

    if (width <= 0 || height <= 0)
    {
        return true;
    }

    mBO = gbm_bo_create(mDisplay->mGBM, width, height, mGBMFormat, mUseFlags);
    if (!mBO)
    {
        return false;
    }

    mDMABuf = gbm_bo_get_fd(mBO);
    if (mDMABuf < 0)
    {
        return false;
    }

    // clang-format off
    const EGLint attr[] =
    {
        EGL_WIDTH, width,
        EGL_HEIGHT, height,
        EGL_LINUX_DRM_FOURCC_EXT, UnsignedToSigned(mDRMFormat),
        EGL_DMA_BUF_PLANE0_FD_EXT, mDMABuf,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, UnsignedToSigned(gbm_bo_get_stride(mBO)),
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
        EGL_NONE,
    };
    // clang-format on

    mImage = mDisplay->getFunctionsEGL()->createImageKHR(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
                                                         nullptr, attr);
    if (mImage == EGL_NO_IMAGE_KHR)
    {
        return false;
    }

    const FunctionsGL *gl = mDisplay->getRenderer()->getFunctions();
    StateManagerGL *sm    = mDisplay->getRenderer()->getStateManager();

    // Update the storage of the renderbuffers but don't generate new IDs. This will update all
    // framebuffers they are bound to.
    sm->bindRenderbuffer(GL_RENDERBUFFER, mColorBuffer);
    gl->eGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, mImage);

    if (mDepthBits || mStencilBits)
    {
        sm->bindRenderbuffer(GL_RENDERBUFFER, mDSBuffer);
        gl->renderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, width, height);
    }

    mWidth  = width;
    mHeight = height;
    return true;
}

bool DisplayGbm::Buffer::initialize(const NativeWindow *native)
{
    mNative = native;
    return createRenderbuffers() && resize(native->width, native->height);
}

bool DisplayGbm::Buffer::initialize(int width, int height)
{
    return createRenderbuffers() && resize(width, height);
}

void DisplayGbm::Buffer::bindTexImage()
{
    const FunctionsGL *gl = mDisplay->getRenderer()->getFunctions();
    gl->eGLImageTargetTexture2DOES(GL_TEXTURE_2D, mImage);
}

GLuint DisplayGbm::Buffer::getTexture()
{
    // TODO(fjhenigman) Try not to create a new texture every time.  That already works on Intel
    // and should work on Mali with proper fences.
    const FunctionsGL *gl = mDisplay->getRenderer()->getFunctions();
    StateManagerGL *sm    = mDisplay->getRenderer()->getStateManager();

    gl->genTextures(1, &mTexture);
    sm->bindTexture(gl::TextureType::_2D, mTexture);
    gl->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    ASSERT(mImage != EGL_NO_IMAGE_KHR);
    gl->eGLImageTargetTexture2DOES(GL_TEXTURE_2D, mImage);
    return mTexture;
}

uint32_t DisplayGbm::Buffer::getDRMFB()
{
    if (!mHasDRMFB)
    {
        int fd              = gbm_device_get_fd(mDisplay->mGBM);
        uint32_t handles[4] = {gbm_bo_get_handle(mBO).u32};
        uint32_t pitches[4] = {gbm_bo_get_stride(mBO)};
        uint32_t offsets[4] = {0};
        if (drmModeAddFB2(fd, mWidth, mHeight, mDRMFormatFB, handles, pitches, offsets, &mDRMFB, 0))
        {
            WARN() << "drmModeAddFB2 failed: " << errno << " " << strerror(errno);
        }
        else
        {
            mHasDRMFB = true;
        }
    }

    return mDRMFB;
}

GLuint DisplayGbm::Buffer::createGLFB(const gl::Context *context)
{
    const FunctionsGL *functions = GetFunctionsGL(context);
    StateManagerGL *stateManager = GetStateManagerGL(context);

    GLuint framebuffer = 0;
    functions->genFramebuffers(1, &framebuffer);
    stateManager->bindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    functions->framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER,
                                       mColorBuffer);

    if (mDepthBits)
    {
        functions->framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                           mDSBuffer);
    }

    if (mStencilBits)
    {
        functions->framebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                           mDSBuffer);
    }

    return framebuffer;
}

egl::Error DisplayGbm::Buffer::attachToFramebuffer(const gl::Context *context,
                                                   FramebufferGL *framebuffer)
{
    ASSERT(framebuffer->getFramebufferID() == 0);
    if (mFramebufferID == 0)
    {
        mFramebufferID = createGLFB(context);
    }
    framebuffer->setFramebufferID(mFramebufferID);
    return egl::NoError();
}

egl::Error DisplayGbm::Buffer::detachFromFramebuffer(const gl::Context *context,
                                                     FramebufferGL *framebuffer)
{
    ASSERT(framebuffer->getFramebufferID() == mFramebufferID);
    framebuffer->setFramebufferID(0);
    return egl::NoError();
}

void DisplayGbm::Buffer::present(const gl::Context *context)
{
    if (mNative)
    {
        if (mNative->visible)
        {
            mDisplay->drawBuffer(context, this);
        }
        resize(mNative->width, mNative->height);
    }
}

bool DisplayGbm::Buffer::createRenderbuffers()
{
    const FunctionsGL *gl = mDisplay->getRenderer()->getFunctions();
    StateManagerGL *sm    = mDisplay->getRenderer()->getStateManager();

    gl->genRenderbuffers(1, &mColorBuffer);
    sm->bindRenderbuffer(GL_RENDERBUFFER, mColorBuffer);

    if (mDepthBits || mStencilBits)
    {
        gl->genRenderbuffers(1, &mDSBuffer);
        sm->bindRenderbuffer(GL_RENDERBUFFER, mDSBuffer);
    }

    return true;
}

DisplayGbm::DisplayGbm(const egl::DisplayState &state)
    : DisplayEGL(state),
      mGBM(nullptr),
      mConnector(nullptr),
      mMode(nullptr),
      mCRTC(nullptr),
      mSetCRTC(true),
      mWidth(1280),
      mHeight(1024),
      mScanning(nullptr),
      mPending(nullptr),
      mDrawing(nullptr),
      mUnused(nullptr),
      mProgram(0),
      mVertexShader(0),
      mFragmentShader(0),
      mVertexBuffer(0),
      mIndexBuffer(0),
      mCenterUniform(0),
      mWindowSizeUniform(0),
      mBorderSizeUniform(0),
      mDepthUniform(0)
{}

DisplayGbm::~DisplayGbm() {}

bool DisplayGbm::hasUsableScreen(int fd)
{
    drmModeResPtr resources = drmModeGetResources(fd);
    if (!resources)
    {
        return false;
    }
    if (resources->count_connectors < 1)
    {
        drmModeFreeResources(resources);
        return false;
    }

    mConnector = nullptr;
    for (int i = 0; i < resources->count_connectors; ++i)
    {
        drmModeFreeConnector(mConnector);
        mConnector = drmModeGetConnector(fd, resources->connectors[i]);
        if (!mConnector || mConnector->connection != DRM_MODE_CONNECTED)
        {
            continue;
        }
        mMode = ChooseMode(mConnector);
        if (!mMode)
        {
            continue;
        }
        int n = ChooseCRTC(fd, mConnector);
        if (n < 0)
        {
            continue;
        }
        mCRTC = drmModeGetCrtc(fd, resources->crtcs[n]);
        if (mCRTC)
        {
            // found a screen
            mGBM = gbm_create_device(fd);
            if (mGBM)
            {
                mWidth  = mMode->hdisplay;
                mHeight = mMode->vdisplay;
                drmModeFreeResources(resources);
                return true;
            }
            // can't use this screen
            drmModeFreeCrtc(mCRTC);
            mCRTC = nullptr;
        }
    }

    drmModeFreeResources(resources);
    return false;
}

egl::Error DisplayGbm::initialize(egl::Display *display)
{
    int fd;
    char deviceName[30];

    for (int i = 0; i < 64; ++i)
    {
        snprintf(deviceName, sizeof(deviceName), "/dev/dri/card%d", i);
        fd = open(deviceName, O_RDWR | O_CLOEXEC);
        if (fd >= 0)
        {
            if (hasUsableScreen(fd))
            {
                break;
            }
            close(fd);
        }
    }

    if (!mGBM)
    {
        // there's no usable screen so try to proceed without one
        for (int i = 128; i < 192; ++i)
        {
            snprintf(deviceName, sizeof(deviceName), "/dev/dri/renderD%d", i);
            fd = open(deviceName, O_RDWR | O_CLOEXEC);
            if (fd >= 0)
            {
                mGBM = gbm_create_device(fd);
                if (mGBM)
                {
                    break;
                }
                close(fd);
            }
        }
    }

    if (!mGBM)
    {
        return egl::EglNotInitialized() << "Could not open drm device.";
    }

    return DisplayEGL::initialize(display);
}

void DisplayGbm::pageFlipHandler(int fd,
                                 unsigned int sequence,
                                 unsigned int tv_sec,
                                 unsigned int tv_usec,
                                 void *data)
{
    DisplayGbm *display = reinterpret_cast<DisplayGbm *>(data);
    uint64_t tv         = tv_sec;
    display->pageFlipHandler(sequence, tv * 1000000 + tv_usec);
}

void DisplayGbm::pageFlipHandler(unsigned int sequence, uint64_t tv)
{
    ASSERT(mPending);
    mUnused   = mScanning;
    mScanning = mPending;
    mPending  = nullptr;
}

void DisplayGbm::presentScreen()
{
    if (!mCRTC)
    {
        // no monitor
        return;
    }

    // see if pending flip has finished, without blocking
    int fd = gbm_device_get_fd(mGBM);
    if (mPending)
    {
        pollfd pfd;
        pfd.fd     = fd;
        pfd.events = POLLIN;
        if (poll(&pfd, 1, 0) < 0)
        {
            WARN() << "poll failed: " << errno << " " << strerror(errno);
        }
        if (pfd.revents & POLLIN)
        {
            drmEventContext event;
            event.version           = DRM_EVENT_CONTEXT_VERSION;
            event.page_flip_handler = pageFlipHandler;
            drmHandleEvent(fd, &event);
        }
    }

    // if pending flip has finished, schedule next one
    if (!mPending && mDrawing)
    {
        flushGL();
        if (mSetCRTC)
        {
            if (drmModeSetCrtc(fd, mCRTC->crtc_id, mDrawing->getDRMFB(), 0, 0,
                               &mConnector->connector_id, 1, mMode))
            {
                WARN() << "set crtc failed: " << errno << " " << strerror(errno);
            }
            mSetCRTC = false;
        }
        if (drmModePageFlip(fd, mCRTC->crtc_id, mDrawing->getDRMFB(), DRM_MODE_PAGE_FLIP_EVENT,
                            this))
        {
            WARN() << "page flip failed: " << errno << " " << strerror(errno);
        }
        mPending = mDrawing;
        mDrawing = nullptr;
    }
}

GLuint DisplayGbm::makeShader(GLuint type, const char *src)
{
    const FunctionsGL *gl = getRenderer()->getFunctions();
    GLuint shader         = gl->createShader(type);
    gl->shaderSource(shader, 1, &src, nullptr);
    gl->compileShader(shader);

    GLchar buf[999];
    GLsizei len;
    GLint compiled;
    gl->getShaderInfoLog(shader, sizeof(buf), &len, buf);
    gl->getShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE)
    {
        WARN() << "DisplayGbm shader compilation error: " << buf;
    }

    return shader;
}

void DisplayGbm::drawWithTexture(const gl::Context *context, Buffer *buffer)
{
    const FunctionsGL *gl = getRenderer()->getFunctions();
    StateManagerGL *sm    = getRenderer()->getStateManager();

    if (!mProgram)
    {
        const GLchar *vertexSource =
            "#version 100\n"
            "attribute vec3 vertex;\n"
            "uniform vec2 center;\n"
            "uniform vec2 windowSize;\n"
            "uniform vec2 borderSize;\n"
            "uniform float depth;\n"
            "varying vec3 texCoord;\n"
            "void main()\n"
            "{\n"
            "    vec2 pos = vertex.xy * (windowSize + borderSize * vertex.z);\n"
            "    gl_Position = vec4(center + pos, depth, 1);\n"
            "    texCoord = vec3(pos / windowSize * vec2(.5, -.5) + vec2(.5, .5), vertex.z);\n"
            "}\n";

        const GLchar *fragmentSource =
            "#version 100\n"
            "precision mediump float;\n"
            "uniform sampler2D tex;\n"
            "varying vec3 texCoord;\n"
            "void main()\n"
            "{\n"
            "    if (texCoord.z > 0.)\n"
            "    {\n"
            "        float c = abs((texCoord.z * 2.) - 1.);\n"
            "        gl_FragColor = vec4(c, c, c, 1);\n"
            "    }\n"
            "    else\n"
            "    {\n"
            "        gl_FragColor = texture2D(tex, texCoord.xy);\n"
            "    }\n"
            "}\n";

        mVertexShader   = makeShader(GL_VERTEX_SHADER, vertexSource);
        mFragmentShader = makeShader(GL_FRAGMENT_SHADER, fragmentSource);
        mProgram        = gl->createProgram();
        gl->attachShader(mProgram, mVertexShader);
        gl->attachShader(mProgram, mFragmentShader);
        gl->bindAttribLocation(mProgram, 0, "vertex");
        gl->linkProgram(mProgram);
        GLint linked;
        gl->getProgramiv(mProgram, GL_LINK_STATUS, &linked);
        if (!linked)
        {
            WARN() << "shader link failed: cannot display buffer";
            return;
        }
        mCenterUniform     = gl->getUniformLocation(mProgram, "center");
        mWindowSizeUniform = gl->getUniformLocation(mProgram, "windowSize");
        mBorderSizeUniform = gl->getUniformLocation(mProgram, "borderSize");
        mDepthUniform      = gl->getUniformLocation(mProgram, "depth");
        GLint texUniform   = gl->getUniformLocation(mProgram, "tex");
        sm->useProgram(mProgram);
        gl->uniform1i(texUniform, 0);

        // clang-format off
        const GLfloat vertices[] =
        {
             // window corners, and window border inside corners
             1, -1, 0,
            -1, -1, 0,
             1,  1, 0,
            -1,  1, 0,
             // window border outside corners
             1, -1, 1,
            -1, -1, 1,
             1,  1, 1,
            -1,  1, 1,
        };
        // clang-format on
        gl->genBuffers(1, &mVertexBuffer);
        sm->bindBuffer(gl::BufferBinding::Array, mVertexBuffer);
        gl->bufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // window border triangle strip
        const GLuint borderStrip[] = {5, 0, 4, 2, 6, 3, 7, 1, 5, 0};

        gl->genBuffers(1, &mIndexBuffer);
        sm->bindBuffer(gl::BufferBinding::ElementArray, mIndexBuffer);
        gl->bufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(borderStrip), borderStrip, GL_STATIC_DRAW);
    }
    else
    {
        sm->useProgram(mProgram);
        sm->bindBuffer(gl::BufferBinding::Array, mVertexBuffer);
        sm->bindBuffer(gl::BufferBinding::ElementArray, mIndexBuffer);
    }

    // convert from pixels to "-1 to 1" space
    const NativeWindow *n = buffer->getNative();
    double x              = n->x * 2. / mWidth - 1;
    double y              = n->y * 2. / mHeight - 1;
    double halfw          = n->width * 1. / mWidth;
    double halfh          = n->height * 1. / mHeight;
    double borderw        = n->borderWidth * 2. / mWidth;
    double borderh        = n->borderHeight * 2. / mHeight;

    gl->uniform2f(mCenterUniform, x + halfw, y + halfh);
    gl->uniform2f(mWindowSizeUniform, halfw, halfh);
    gl->uniform2f(mBorderSizeUniform, borderw, borderh);
    gl->uniform1f(mDepthUniform, n->depth / 1e6);

    sm->setBlendEnabled(false);
    sm->setCullFaceEnabled(false);
    sm->setStencilTestEnabled(false);
    sm->setScissorTestEnabled(false);
    sm->setDepthTestEnabled(true);
    sm->setColorMask(true, true, true, true);
    sm->setDepthMask(true);
    sm->setDepthRange(0, 1);
    sm->setDepthFunc(GL_LESS);
    sm->setViewport(gl::Rectangle(0, 0, mWidth, mHeight));
    sm->activeTexture(0);
    GLuint tex = buffer->getTexture();
    sm->bindTexture(gl::TextureType::_2D, tex);
    gl->vertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    gl->enableVertexAttribArray(0);
    GLuint fbo = mDrawing->createGLFB(context);
    sm->bindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    gl->drawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gl->drawElements(GL_TRIANGLE_STRIP, 10, GL_UNSIGNED_INT, 0);
    sm->deleteTexture(tex);
    sm->deleteFramebuffer(fbo);
}

void DisplayGbm::drawBuffer(const gl::Context *context, Buffer *buffer)
{
    if (!mDrawing)
    {
        // get buffer on which to draw window
        if (mUnused)
        {
            mDrawing = mUnused;
            mUnused  = nullptr;
        }
        else
        {
            mDrawing = new Buffer(this, GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT,
                                  GBM_FORMAT_ARGB8888, DRM_FORMAT_ARGB8888, DRM_FORMAT_XRGB8888,
                                  true, true);  // XXX shouldn't need stencil
            if (!mDrawing || !mDrawing->initialize(mWidth, mHeight))
            {
                return;
            }
        }

        const FunctionsGL *gl = getRenderer()->getFunctions();
        StateManagerGL *sm    = getRenderer()->getStateManager();

        GLuint fbo = mDrawing->createGLFB(context);
        sm->bindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
        sm->setClearColor(gl::ColorF(0, 0, 0, 1));
        sm->setClearDepth(1);
        sm->setScissorTestEnabled(false);
        sm->setColorMask(true, true, true, true);
        sm->setDepthMask(true);
        gl->clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        sm->deleteFramebuffer(fbo);
    }

    drawWithTexture(context, buffer);
    presentScreen();
}

void DisplayGbm::flushGL()
{
    const FunctionsGL *gl = getRenderer()->getFunctions();
    gl->flush();
    if (getFunctionsEGL()->hasExtension("EGL_KHR_fence_sync"))
    {
        const EGLint attrib[] = {EGL_SYNC_CONDITION_KHR,
                                 EGL_SYNC_PRIOR_COMMANDS_IMPLICIT_EXTERNAL_ARM, EGL_NONE};
        EGLSyncKHR fence      = getFunctionsEGL()->createSyncKHR(EGL_SYNC_FENCE_KHR, attrib);
        if (fence)
        {
            // TODO(fjhenigman) Figure out the right way to use fences on Mali GPU
            // to maximize throughput and avoid hangs when a program is interrupted.
            // This busy wait was an attempt to reduce hangs when interrupted by SIGINT,
            // but we still get some.
            for (;;)
            {
                EGLint r =
                    getFunctionsEGL()->clientWaitSyncKHR(fence, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, 0);
                if (r != EGL_TIMEOUT_EXPIRED_KHR)
                {
                    break;
                }
                usleep(99);
            }
            getFunctionsEGL()->destroySyncKHR(fence);
            return;
        }
    }
}

void DisplayGbm::terminate()
{
    SafeDelete(mScanning);
    SafeDelete(mPending);
    SafeDelete(mDrawing);
    SafeDelete(mUnused);

    if (mProgram)
    {
        const FunctionsGL *gl = getRenderer()->getFunctions();
        gl->deleteProgram(mProgram);
        gl->deleteShader(mVertexShader);
        gl->deleteShader(mFragmentShader);
        gl->deleteBuffers(1, &mVertexBuffer);
        gl->deleteBuffers(1, &mIndexBuffer);
        mProgram = 0;
    }

    DisplayEGL::terminate();

    drmModeFreeConnector(mConnector);
    mConnector = nullptr;
    mMode      = nullptr;
    drmModeFreeCrtc(mCRTC);
    mCRTC = nullptr;

    if (mGBM)
    {
        int fd = gbm_device_get_fd(mGBM);
        gbm_device_destroy(mGBM);
        mGBM = nullptr;
        close(fd);
    }
}

SurfaceImpl *DisplayGbm::createWindowSurface(const egl::SurfaceState &state,
                                             EGLNativeWindowType window,
                                             const egl::AttributeMap &attribs)
{
    Buffer *buffer = new Buffer(this, GBM_BO_USE_RENDERING, GBM_FORMAT_ARGB8888,
                                DRM_FORMAT_ARGB8888, DRM_FORMAT_XRGB8888, true, true);
    if (!buffer || !buffer->initialize(reinterpret_cast<const NativeWindow *>(window)))
    {
        return nullptr;
    }
    return new SurfaceGbm(state, buffer);
}

SurfaceImpl *DisplayGbm::createPbufferSurface(const egl::SurfaceState &state,
                                              const egl::AttributeMap &attribs)
{
    EGLAttrib width  = attribs.get(EGL_WIDTH, 0);
    EGLAttrib height = attribs.get(EGL_HEIGHT, 0);
    Buffer *buffer   = new Buffer(this, GBM_BO_USE_RENDERING, GBM_FORMAT_ARGB8888,
                                  DRM_FORMAT_ARGB8888, DRM_FORMAT_XRGB8888, true, true);
    if (!buffer || !buffer->initialize(static_cast<int>(width), static_cast<int>(height)))
    {
        return nullptr;
    }
    return new SurfaceGbm(state, buffer);
}

bool DisplayGbm::isValidNativeWindow(EGLNativeWindowType window) const
{
    return true;
}

void DisplayGbm::setSwapInterval(EGLSurface drawable, SwapControlData *data)
{
    ASSERT(data != nullptr);
}

EGLint DisplayGbm::fixSurfaceType(EGLint surfaceType) const
{
    EGLint type = DisplayEGL::fixSurfaceType(surfaceType);
    // Ozone native surfaces don't support EGL_WINDOW_BIT,
    // but ANGLE uses renderbuffers to emulate windows
    return type | EGL_WINDOW_BIT;
}

}  // namespace rx
