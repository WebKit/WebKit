//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayAndroid.h: Android implementation of egl::Display

#ifndef LIBANGLE_RENDERER_GL_EGL_ANDROID_DISPLAYANDROID_H_
#define LIBANGLE_RENDERER_GL_EGL_ANDROID_DISPLAYANDROID_H_

#include <map>
#include <string>
#include <thread>
#include <vector>

#include "libANGLE/renderer/gl/egl/DisplayEGL.h"

namespace rx
{

class RendererEGL;

class DisplayAndroid : public DisplayEGL
{
  public:
    DisplayAndroid(const egl::DisplayState &state);
    ~DisplayAndroid() override;

    egl::Error initialize(egl::Display *display) override;
    void terminate() override;

    SurfaceImpl *createWindowSurface(const egl::SurfaceState &state,
                                     EGLNativeWindowType window,
                                     const egl::AttributeMap &attribs) override;
    SurfaceImpl *createPbufferSurface(const egl::SurfaceState &state,
                                      const egl::AttributeMap &attribs) override;
    SurfaceImpl *createPbufferFromClientBuffer(const egl::SurfaceState &state,
                                               EGLenum buftype,
                                               EGLClientBuffer clientBuffer,
                                               const egl::AttributeMap &attribs) override;
    SurfaceImpl *createPixmapSurface(const egl::SurfaceState &state,
                                     NativePixmapType nativePixmap,
                                     const egl::AttributeMap &attribs) override;

    ContextImpl *createContext(const gl::State &state,
                               gl::ErrorSet *errorSet,
                               const egl::Config *configuration,
                               const gl::Context *shareContext,
                               const egl::AttributeMap &attribs) override;

    egl::ConfigSet generateConfigs() override;

    bool testDeviceLost() override;
    egl::Error restoreLostDevice(const egl::Display *display) override;

    bool isValidNativeWindow(EGLNativeWindowType window) const override;
    egl::Error validateImageClientBuffer(const gl::Context *context,
                                         EGLenum target,
                                         EGLClientBuffer clientBuffer,
                                         const egl::AttributeMap &attribs) const override;

    ExternalImageSiblingImpl *createExternalImageSibling(const gl::Context *context,
                                                         EGLenum target,
                                                         EGLClientBuffer buffer,
                                                         const egl::AttributeMap &attribs) override;

    DeviceImpl *createDevice() override;

    egl::Error waitClient(const gl::Context *context) override;
    egl::Error waitNative(const gl::Context *context, EGLint engine) override;

    egl::Error makeCurrent(egl::Surface *drawSurface,
                           egl::Surface *readSurface,
                           gl::Context *context) override;

    gl::Version getMaxSupportedESVersion() const override;

    void destroyNativeContext(EGLContext context) override;

    WorkerContext *createWorkerContext(std::string *infoLog,
                                       EGLContext sharedContext,
                                       const native_egl::AttributeVector workerAttribs) override;

  private:
    void generateExtensions(egl::DisplayExtensions *outExtensions) const override;

    egl::Error createRenderer(EGLContext shareContext,
                              bool makeNewContextCurrent,
                              std::shared_ptr<RendererEGL> *outRenderer);

    egl::Error makeCurrentSurfaceless(gl::Context *context) override;

    template <typename T>
    void getConfigAttrib(EGLConfig config, EGLint attribute, T *value) const;

    template <typename T, typename U>
    void getConfigAttribIfExtension(EGLConfig config,
                                    EGLint attribute,
                                    T *value,
                                    const char *extension,
                                    const U &defaultValue) const;

    bool mVirtualizedContexts;
    std::shared_ptr<RendererEGL> mRenderer;

    egl::AttributeMap mDisplayAttributes;

    bool mSupportsSurfaceless;

    std::vector<EGLint> mConfigAttribList;
    std::map<EGLint, EGLint> mConfigIds;
    EGLSurface mDummyPbuffer;

    struct CurrentNativeContext
    {
        EGLSurface surface = EGL_NO_SURFACE;
        EGLContext context = EGL_NO_CONTEXT;
    };
    std::unordered_map<std::thread::id, CurrentNativeContext> mCurrentNativeContext;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_EGL_ANDROID_DISPLAYANDROID_H_
