//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayWGL.h: WGL implementation of egl::Display

#ifndef LIBANGLE_RENDERER_GL_WGL_DISPLAYWGL_H_
#define LIBANGLE_RENDERER_GL_WGL_DISPLAYWGL_H_

#include "libANGLE/renderer/gl/DisplayGL.h"

#include <GL/wglext.h>

namespace rx
{

class FunctionsWGL;

class DisplayWGL : public DisplayGL
{
  public:
    DisplayWGL();
    ~DisplayWGL() override;

    egl::Error initialize(egl::Display *display) override;
    void terminate() override;

    // Surface creation
    SurfaceImpl *createWindowSurface(const egl::Config *configuration,
                                     EGLNativeWindowType window,
                                     const egl::AttributeMap &attribs) override;
    SurfaceImpl *createPbufferSurface(const egl::Config *configuration,
                                      const egl::AttributeMap &attribs) override;
    SurfaceImpl *createPbufferFromClientBuffer(const egl::Config *configuration,
                                               EGLClientBuffer shareHandle,
                                               const egl::AttributeMap &attribs) override;
    SurfaceImpl *createPixmapSurface(const egl::Config *configuration,
                                     NativePixmapType nativePixmap,
                                     const egl::AttributeMap &attribs) override;

    egl::ConfigSet generateConfigs() const override;

    bool isDeviceLost() const override;
    bool testDeviceLost() override;
    egl::Error restoreLostDevice() override;

    bool isValidNativeWindow(EGLNativeWindowType window) const override;

    egl::Error getDevice(DeviceImpl **device) override;

    std::string getVendorString() const override;

    egl::Error waitClient() const override;
    egl::Error waitNative(EGLint engine,
                          egl::Surface *drawSurface,
                          egl::Surface *readSurface) const override;

    egl::Error getDriverVersion(std::string *version) const override;

    egl::Error registerD3DDevice(IUnknown *device, HANDLE *outHandle);
    void releaseD3DDevice(HANDLE handle);

  private:
    const FunctionsGL *getFunctionsGL() const override;

    egl::Error initializeD3DDevice();

    void generateExtensions(egl::DisplayExtensions *outExtensions) const override;
    void generateCaps(egl::Caps *outCaps) const override;

    HMODULE mOpenGLModule;

    FunctionsWGL *mFunctionsWGL;
    FunctionsGL *mFunctionsGL;

    ATOM mWindowClass;
    HWND mWindow;
    HDC mDeviceContext;
    int mPixelFormat;
    HGLRC mWGLContext;

    bool mUseDXGISwapChains;
    HMODULE mDxgiModule;
    HMODULE mD3d11Module;
    HANDLE mD3D11DeviceHandle;
    ID3D11Device *mD3D11Device;

    struct D3DObjectHandle
    {
        HANDLE handle;
        size_t refCount;
    };
    std::map<IUnknown *, D3DObjectHandle> mRegisteredD3DDevices;

    egl::Display *mDisplay;
};

}

#endif // LIBANGLE_RENDERER_GL_WGL_DISPLAYWGL_H_
