//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Display.h: Defines the egl::Display class, representing the abstract
// display on which graphics are drawn. Implements EGLDisplay.
// [EGL 1.4] section 2.1.2 page 3.

#ifndef INCLUDE_DISPLAY_H_
#define INCLUDE_DISPLAY_H_

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d9.h>

#include <set>

#include "libGLESv2/Context.h"

#include "libEGL/Config.h"
#include "libEGL/Surface.h"

namespace egl
{
class Display
{
  public:
    Display(HDC deviceContext);

    ~Display();

    bool initialize();
    void terminate();

    virtual void startScene();
    virtual void endScene();

    bool getConfigs(EGLConfig *configs, const EGLint *attribList, EGLint configSize, EGLint *numConfig);
    bool getConfigAttrib(EGLConfig config, EGLint attribute, EGLint *value);

    egl::Surface *createWindowSurface(HWND window, EGLConfig config);
    EGLContext createContext(EGLConfig configHandle, const gl::Context *shareContext);

    void destroySurface(egl::Surface *surface);
    void destroyContext(gl::Context *context);

    bool isInitialized();
    bool isValidConfig(EGLConfig config);
    bool isValidContext(gl::Context *context);
    bool isValidSurface(egl::Surface *surface);
    bool hasExistingWindowSurface(HWND window);

    EGLint getMinSwapInterval();
    EGLint getMaxSwapInterval();

    virtual IDirect3DDevice9 *getDevice();
    virtual D3DCAPS9 getDeviceCaps();
    virtual void getMultiSampleSupport(D3DFORMAT format, bool *multiSampleArray);
    virtual bool getCompressedTextureSupport();
    virtual bool getEventQuerySupport();
    virtual bool getFloatTextureSupport(bool *filtering, bool *renderable);
    virtual bool getHalfFloatTextureSupport(bool *filtering, bool *renderable);
    virtual bool getLuminanceTextureSupport();
    virtual bool getLuminanceAlphaTextureSupport();
    virtual D3DPOOL getBufferPool(DWORD usage) const;

  private:
    DISALLOW_COPY_AND_ASSIGN(Display);

    D3DPRESENT_PARAMETERS getDefaultPresentParameters();

    const HDC mDc;

    HMODULE mD3d9Module;
    
    UINT mAdapter;
    D3DDEVTYPE mDeviceType;
    IDirect3D9 *mD3d9;  // Always valid after successful initialization.
    IDirect3D9Ex *mD3d9ex;  // Might be null if D3D9Ex is not supported.
    IDirect3DDevice9 *mDevice;
    D3DCAPS9 mDeviceCaps;
    HWND mDeviceWindow;

    bool mSceneStarted;
    EGLint mMaxSwapInterval;
    EGLint mMinSwapInterval;
    
    typedef std::set<Surface*> SurfaceSet;
    SurfaceSet mSurfaceSet;

    ConfigSet mConfigSet;

    typedef std::set<gl::Context*> ContextSet;
    ContextSet mContextSet;

    bool createDevice();
    bool resetDevice();
};
}

#endif   // INCLUDE_DISPLAY_H_
