//
// Copyright (c) 2002-2011 The ANGLE Project Authors. All rights reserved.
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
    ~Display();

    bool initialize();
    void terminate();

    virtual void startScene();
    virtual void endScene();

    static egl::Display *getDisplay(EGLNativeDisplayType displayId);

    bool getConfigs(EGLConfig *configs, const EGLint *attribList, EGLint configSize, EGLint *numConfig);
    bool getConfigAttrib(EGLConfig config, EGLint attribute, EGLint *value);

    EGLSurface createWindowSurface(HWND window, EGLConfig config, const EGLint *attribList);
    EGLSurface createOffscreenSurface(EGLConfig config, HANDLE shareHandle, const EGLint *attribList);
    EGLContext createContext(EGLConfig configHandle, const gl::Context *shareContext, bool notifyResets, bool robustAccess);

    void destroySurface(egl::Surface *surface);
    void destroyContext(gl::Context *context);

    bool isInitialized() const;
    bool isValidConfig(EGLConfig config);
    bool isValidContext(gl::Context *context);
    bool isValidSurface(egl::Surface *surface);
    bool hasExistingWindowSurface(HWND window);

    EGLint getMinSwapInterval();
    EGLint getMaxSwapInterval();

    virtual IDirect3DDevice9 *getDevice();
    virtual D3DCAPS9 getDeviceCaps();
    virtual D3DADAPTER_IDENTIFIER9 *getAdapterIdentifier();
    virtual bool testDeviceLost();
    virtual bool testDeviceResettable();
    virtual void getMultiSampleSupport(D3DFORMAT format, bool *multiSampleArray);
    virtual bool getDXT1TextureSupport();
    virtual bool getDXT3TextureSupport();
    virtual bool getDXT5TextureSupport();
    virtual bool getEventQuerySupport();
    virtual bool getFloat32TextureSupport(bool *filtering, bool *renderable);
    virtual bool getFloat16TextureSupport(bool *filtering, bool *renderable);
    virtual bool getLuminanceTextureSupport();
    virtual bool getLuminanceAlphaTextureSupport();
    virtual bool getVertexTextureSupport() const;
    virtual bool getNonPower2TextureSupport() const;
    virtual D3DPOOL getBufferPool(DWORD usage) const;
    virtual D3DPOOL getTexturePool(bool renderable) const;

    virtual void notifyDeviceLost();
    bool isDeviceLost();

    bool isD3d9ExDevice() { return mD3d9Ex != NULL; }
    const char *getExtensionString() const;

  private:
    DISALLOW_COPY_AND_ASSIGN(Display);

    Display(EGLNativeDisplayType displayId, HDC deviceContext, bool software);

    D3DPRESENT_PARAMETERS getDefaultPresentParameters();

    bool restoreLostDevice();

    EGLNativeDisplayType mDisplayId;
    const HDC mDc;

    HMODULE mD3d9Module;
    
    UINT mAdapter;
    D3DDEVTYPE mDeviceType;
    IDirect3D9 *mD3d9;  // Always valid after successful initialization.
    IDirect3D9Ex *mD3d9Ex;  // Might be null if D3D9Ex is not supported.
    IDirect3DDevice9 *mDevice;
    IDirect3DDevice9Ex *mDeviceEx;  // Might be null if D3D9Ex is not supported.
    D3DCAPS9 mDeviceCaps;
    D3DADAPTER_IDENTIFIER9 mAdapterIdentifier;
    HWND mDeviceWindow;

    bool mSceneStarted;
    EGLint mMaxSwapInterval;
    EGLint mMinSwapInterval;
    bool mSoftwareDevice;
    
    typedef std::set<Surface*> SurfaceSet;
    SurfaceSet mSurfaceSet;

    ConfigSet mConfigSet;

    typedef std::set<gl::Context*> ContextSet;
    ContextSet mContextSet;
    bool mDeviceLost;

    bool createDevice();
    void initializeDevice();
    bool resetDevice();

    void initExtensionString();
    std::string mExtensionString;
};
}

#endif   // INCLUDE_DISPLAY_H_
