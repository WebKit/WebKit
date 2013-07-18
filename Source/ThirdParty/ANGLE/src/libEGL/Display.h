//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Display.h: Defines the egl::Display class, representing the abstract
// display on which graphics are drawn. Implements EGLDisplay.
// [EGL 1.4] section 2.1.2 page 3.

#ifndef LIBEGL_DISPLAY_H_
#define LIBEGL_DISPLAY_H_

#include "common/system.h"
#include <d3d9.h>
#include <D3Dcompiler.h>

#include <set>
#include <vector>

#include "libGLESv2/Context.h"

#include "libEGL/Config.h"
#include "libEGL/ShaderCache.h"
#include "libEGL/Surface.h"

const int versionWindowsVista = MAKEWORD(0x00, 0x06);
const int versionWindows7 = MAKEWORD(0x01, 0x06);

// Return the version of the operating system in a format suitable for ordering
// comparison.
inline int getComparableOSVersion()
{
    DWORD version = GetVersion();
    int majorVersion = LOBYTE(LOWORD(version));
    int minorVersion = HIBYTE(LOWORD(version));
    return MAKEWORD(minorVersion, majorVersion);
}

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
    virtual void sync(bool block);
    virtual IDirect3DQuery9* allocateEventQuery();
    virtual void freeEventQuery(IDirect3DQuery9* query);
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
    virtual bool getDepthTextureSupport() const;
    virtual bool getOcclusionQuerySupport() const;
    virtual bool getInstancingSupport() const;
    virtual float getTextureFilterAnisotropySupport() const;
    virtual D3DPOOL getBufferPool(DWORD usage) const;
    virtual D3DPOOL getTexturePool(DWORD usage) const;

    virtual void notifyDeviceLost();
    bool isDeviceLost();

    bool isD3d9ExDevice() const { return mD3d9Ex != NULL; }
    const char *getExtensionString() const;
    const char *getVendorString() const;
    bool shareHandleSupported() const;

    virtual IDirect3DVertexShader9 *createVertexShader(const DWORD *function, size_t length);
    virtual IDirect3DPixelShader9 *createPixelShader(const DWORD *function, size_t length);

    virtual HRESULT compileShaderSource(const char* hlsl, const char* sourceName, const char* profile, DWORD flags, ID3DBlob** binary, ID3DBlob** errorMessage);

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
    D3DDISPLAYMODE mDisplayMode;
    IDirect3D9 *mD3d9;  // Always valid after successful initialization.
    IDirect3D9Ex *mD3d9Ex;  // Might be null if D3D9Ex is not supported.
    IDirect3DDevice9 *mDevice;
    IDirect3DDevice9Ex *mDeviceEx;  // Might be null if D3D9Ex is not supported.

    // A pool of event queries that are currently unused.
    std::vector<IDirect3DQuery9*> mEventQueryPool;

    VertexShaderCache mVertexShaderCache;
    PixelShaderCache mPixelShaderCache;

    D3DCAPS9 mDeviceCaps;
    D3DADAPTER_IDENTIFIER9 mAdapterIdentifier;
    HWND mDeviceWindow;

    bool mSceneStarted;
    EGLint mMaxSwapInterval;
    EGLint mMinSwapInterval;
    bool mSoftwareDevice;
    bool mSupportsNonPower2Textures;
    
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

    void initVendorString();
    std::string mVendorString;

    typedef HRESULT (WINAPI *D3DCompileFunc)(LPCVOID pSrcData,
                                             SIZE_T SrcDataSize,
                                             LPCSTR pSourceName,
                                             CONST D3D_SHADER_MACRO* pDefines,
                                             ID3DInclude* pInclude,
                                             LPCSTR pEntrypoint,
                                             LPCSTR pTarget,
                                             UINT Flags1,
                                             UINT Flags2,
                                             ID3DBlob** ppCode,
                                             ID3DBlob** ppErrorMsgs);

    HMODULE mD3dCompilerModule;
    D3DCompileFunc mD3DCompileFunc;
};
}

#endif   // LIBEGL_DISPLAY_H_
