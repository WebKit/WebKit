//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayGbm.h: Gbm implementation of egl::Display

#ifndef LIBANGLE_RENDERER_GL_EGL_GBM_DISPLAYGBM_H_
#define LIBANGLE_RENDERER_GL_EGL_GBM_DISPLAYGBM_H_

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <string>

#include "libANGLE/renderer/gl/egl/DisplayEGL.h"

struct gbm_device;
struct gbm_bo;

namespace gl
{
class FramebufferState;
}

namespace rx
{

class FramebufferGL;
class RendererEGL;

struct SwapControlData;

// TODO(fjhenigman) Implement swap control.  The SwapControlData struct will be used for that.
class DisplayGbm final : public DisplayEGL
{
  public:
    struct NativeWindow
    {
        int32_t x;
        int32_t y;
        int32_t width;
        int32_t height;
        int32_t borderWidth;
        int32_t borderHeight;
        int32_t visible;
        int32_t depth;
    };

    class Buffer final : angle::NonCopyable
    {
      public:
        Buffer(DisplayGbm *display,
               uint32_t useFlags,
               uint32_t gbmFormat,
               uint32_t drmFormat,
               uint32_t drmFormatFB,
               int depthBits,
               int stencilBits);

        ~Buffer();
        bool initialize(const NativeWindow *window);
        bool initialize(int32_t width, int32_t height);
        void reset();
        bool resize(int32_t width, int32_t height);
        GLuint createGLFB(const gl::Context *context);
        FramebufferGL *framebufferGL(const gl::Context *context, const gl::FramebufferState &state);
        void present(const gl::Context *context);
        uint32_t getDRMFB();
        void bindTexImage();
        GLuint getTexture();
        int32_t getWidth() const { return mWidth; }
        int32_t getHeight() const { return mHeight; }
        const NativeWindow *getNative() const { return mNative; }

      private:
        bool createRenderbuffers();

        DisplayGbm *mDisplay;
        const NativeWindow *mNative;
        int mWidth;
        int mHeight;
        const int mDepthBits;
        const int mStencilBits;
        const uint32_t mUseFlags;
        const uint32_t mGBMFormat;
        const uint32_t mDRMFormat;
        const uint32_t mDRMFormatFB;
        gbm_bo *mBO;
        int mDMABuf;
        bool mHasDRMFB;
        uint32_t mDRMFB;
        EGLImageKHR mImage;
        GLuint mColorBuffer;
        GLuint mDSBuffer;
        GLuint mTexture;
    };

    DisplayGbm(const egl::DisplayState &state);
    ~DisplayGbm() override;

    egl::Error initialize(egl::Display *display) override;
    void terminate() override;

    SurfaceImpl *createWindowSurface(const egl::SurfaceState &state,
                                     EGLNativeWindowType window,
                                     const egl::AttributeMap &attribs) override;
    SurfaceImpl *createPbufferSurface(const egl::SurfaceState &state,
                                      const egl::AttributeMap &attribs) override;

    bool isValidNativeWindow(EGLNativeWindowType window) const override;

    // TODO(fjhenigman) Implement this.
    // Swap interval can be set globally or per drawable.
    // This function will make sure the drawable's swap interval is the
    // one required so that the subsequent swapBuffers acts as expected.
    void setSwapInterval(EGLSurface drawable, SwapControlData *data);

  private:
    EGLint fixSurfaceType(EGLint surfaceType) const override;

    GLuint makeShader(GLuint type, const char *src);
    void drawBuffer(const gl::Context *context, Buffer *buffer);
    void drawWithTexture(const gl::Context *context, Buffer *buffer);
    void flushGL();
    bool hasUsableScreen(int fd);
    void presentScreen();
    static void pageFlipHandler(int fd,
                                unsigned int sequence,
                                unsigned int tv_sec,
                                unsigned int tv_usec,
                                void *data);
    void pageFlipHandler(unsigned int sequence, uint64_t tv);

    gbm_device *mGBM;
    drmModeConnectorPtr mConnector;
    drmModeModeInfoPtr mMode;
    drmModeCrtcPtr mCRTC;
    bool mSetCRTC;

    int32_t mWidth;
    int32_t mHeight;

    // Three scanout buffers cycle through four states.  The state of a buffer
    // is indicated by which of these pointers points to it.
    // TODO(fjhenigman) It might be simpler/clearer to use a ring buffer.
    Buffer *mScanning;
    Buffer *mPending;
    Buffer *mDrawing;
    Buffer *mUnused;

    GLuint mProgram;
    GLuint mVertexShader;
    GLuint mFragmentShader;
    GLuint mVertexBuffer;
    GLuint mIndexBuffer;
    GLint mCenterUniform;
    GLint mWindowSizeUniform;
    GLint mBorderSizeUniform;
    GLint mDepthUniform;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_EGL_GBM_DISPLAYGBM_H_
