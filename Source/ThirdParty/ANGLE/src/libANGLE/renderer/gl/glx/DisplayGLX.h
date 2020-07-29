//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayGLX.h: GLX implementation of egl::Display

#ifndef LIBANGLE_RENDERER_GL_GLX_DISPLAYGLX_H_
#define LIBANGLE_RENDERER_GL_GLX_DISPLAYGLX_H_

#include <string>
#include <thread>
#include <vector>

#include "common/Optional.h"
#include "libANGLE/renderer/gl/DisplayGL.h"
#include "libANGLE/renderer/gl/glx/FunctionsGLX.h"

namespace rx
{

class FunctionsGLX;
class WorkerContext;

struct SwapControlData;

class DisplayGLX : public DisplayGL
{
  public:
    DisplayGLX(const egl::DisplayState &state);
    ~DisplayGLX() override;

    egl::Error initialize(egl::Display *display) override;
    void terminate() override;

    egl::Error makeCurrent(egl::Surface *drawSurface,
                           egl::Surface *readSurface,
                           gl::Context *context) override;

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

    egl::Error validatePixmap(egl::Config *config,
                              EGLNativePixmapType pixmap,
                              const egl::AttributeMap &attributes) const override;

    ContextImpl *createContext(const gl::State &state,
                               gl::ErrorSet *errorSet,
                               const egl::Config *configuration,
                               const gl::Context *shareContext,
                               const egl::AttributeMap &attribs) override;

    egl::ConfigSet generateConfigs() override;

    bool testDeviceLost() override;
    egl::Error restoreLostDevice(const egl::Display *display) override;

    bool isValidNativeWindow(EGLNativeWindowType window) const override;

    DeviceImpl *createDevice() override;

    std::string getVendorString() const override;

    egl::Error waitClient(const gl::Context *context) override;
    egl::Error waitNative(const gl::Context *context, EGLint engine) override;

    gl::Version getMaxSupportedESVersion() const override;

    // Synchronizes with the X server, if the display has been opened by ANGLE.
    // Calling this is required at the end of every functions that does buffered
    // X calls (not for glX calls) otherwise there might be race conditions
    // between the application's display and ANGLE's one.
    void syncXCommands() const;

    // Depending on the supported GLX extension, swap interval can be set
    // globally or per drawable. This function will make sure the drawable's
    // swap interval is the one required so that the subsequent swapBuffers
    // acts as expected.
    void setSwapInterval(glx::Drawable drawable, SwapControlData *data);

    bool isValidWindowVisualId(unsigned long visualId) const;

    WorkerContext *createWorkerContext(std::string *infoLog);

    void initializeFrontendFeatures(angle::FrontendFeatures *features) const override;

    void populateFeatureList(angle::FeatureList *features) override;

  private:
    egl::Error initializeContext(glx::FBConfig config,
                                 const egl::AttributeMap &eglAttributes,
                                 glx::Context *context);

    void generateExtensions(egl::DisplayExtensions *outExtensions) const override;
    void generateCaps(egl::Caps *outCaps) const override;

    egl::Error makeCurrentSurfaceless(gl::Context *context) override;

    int getGLXFBConfigAttrib(glx::FBConfig config, int attrib) const;
    egl::Error createContextAttribs(glx::FBConfig,
                                    const Optional<gl::Version> &version,
                                    int profileMask,
                                    glx::Context *context);

    std::shared_ptr<RendererGL> mRenderer;

    std::map<int, glx::FBConfig> configIdToGLXConfig;

    EGLint mRequestedVisual;
    glx::FBConfig mContextConfig;
    std::vector<int> mAttribs;
    XVisualInfo *mVisuals;
    glx::Context mContext;
    glx::Context mSharedContext;
    std::unordered_map<std::thread::id, glx::Context> mCurrentContexts;
    // A pbuffer the context is current on during ANGLE initialization
    glx::Pbuffer mDummyPbuffer;

    std::vector<glx::Pbuffer> mWorkerPbufferPool;

    bool mUsesNewXDisplay;
    bool mIsMesa;
    bool mHasMultisample;
    bool mHasARBCreateContext;
    bool mHasARBCreateContextProfile;
    bool mHasARBCreateContextRobustness;
    bool mHasEXTCreateContextES2Profile;

    enum class SwapControl
    {
        Absent,
        EXT,
        Mesa,
        SGI,
    };
    SwapControl mSwapControl;
    int mMinSwapInterval;
    int mMaxSwapInterval;
    int mCurrentSwapInterval;

    glx::Drawable mCurrentDrawable;

    FunctionsGLX mGLX;
    Display *mXDisplay;
    egl::Display *mEGLDisplay;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_GLX_DISPLAYGLX_H_
