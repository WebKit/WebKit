//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Display.cpp: Implements the egl::Display class, representing the abstract
// display on which graphics are drawn. Implements EGLDisplay.
// [EGL 1.4] section 2.1.2 page 3.

#include "libEGL/Display.h"

#include <algorithm>
#include <vector>

#include "common/debug.h"

#include "libEGL/main.h"

#define REF_RAST 0   // Can also be enabled by defining FORCE_REF_RAST in the project's predefined macros

namespace egl
{
Display::Display(HDC deviceContext) : mDc(deviceContext)
{
    mD3d9Module = NULL;
    
    mD3d9 = NULL;
    mD3d9ex = NULL;
    mDevice = NULL;
    mDeviceWindow = NULL;

    mAdapter = D3DADAPTER_DEFAULT;

    #if REF_RAST == 1 || defined(FORCE_REF_RAST)
        mDeviceType = D3DDEVTYPE_REF;
    #else
        mDeviceType = D3DDEVTYPE_HAL;
    #endif

    mMinSwapInterval = 1;
    mMaxSwapInterval = 1;
    setSwapInterval(1);
}

Display::~Display()
{
    terminate();
}

bool Display::initialize()
{
    if (isInitialized())
    {
        return true;
    }

    mD3d9Module = LoadLibrary(TEXT("d3d9.dll"));
    if (mD3d9Module == NULL)
    {
        terminate();
        return false;
    }

    typedef IDirect3D9* (WINAPI *Direct3DCreate9Func)(UINT);
    Direct3DCreate9Func Direct3DCreate9Ptr = reinterpret_cast<Direct3DCreate9Func>(GetProcAddress(mD3d9Module, "Direct3DCreate9"));

    if (Direct3DCreate9Ptr == NULL)
    {
        terminate();
        return false;
    }

    typedef HRESULT (WINAPI *Direct3DCreate9ExFunc)(UINT, IDirect3D9Ex**);
    Direct3DCreate9ExFunc Direct3DCreate9ExPtr = reinterpret_cast<Direct3DCreate9ExFunc>(GetProcAddress(mD3d9Module, "Direct3DCreate9Ex"));

    // Use Direct3D9Ex if available. Among other things, this version is less
    // inclined to report a lost context, for example when the user switches
    // desktop. Direct3D9Ex is available in Windows Vista and later if suitable drivers are available.
    if (Direct3DCreate9ExPtr && SUCCEEDED(Direct3DCreate9ExPtr(D3D_SDK_VERSION, &mD3d9ex)))
    {
        ASSERT(mD3d9ex);
        mD3d9ex->QueryInterface(IID_IDirect3D9, reinterpret_cast<void**>(&mD3d9));
        ASSERT(mD3d9);
    }
    else
    {
        mD3d9 = Direct3DCreate9Ptr(D3D_SDK_VERSION);
    }

    if (mD3d9)
    {
        if (mDc != NULL)
        {
        //  UNIMPLEMENTED();   // FIXME: Determine which adapter index the device context corresponds to
        }

        HRESULT result = mD3d9->GetDeviceCaps(mAdapter, mDeviceType, &mDeviceCaps);

        if (result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY)
        {
            return error(EGL_BAD_ALLOC, false);
        }

        if (mDeviceCaps.PixelShaderVersion < D3DPS_VERSION(2, 0))
        {
            terminate();
            return error(EGL_NOT_INITIALIZED, false);
        }

        mMinSwapInterval = 4;
        mMaxSwapInterval = 0;

        if (mDeviceCaps.PresentationIntervals & D3DPRESENT_INTERVAL_IMMEDIATE) {mMinSwapInterval = std::min(mMinSwapInterval, 0); mMaxSwapInterval = std::max(mMaxSwapInterval, 0);}
        if (mDeviceCaps.PresentationIntervals & D3DPRESENT_INTERVAL_ONE)       {mMinSwapInterval = std::min(mMinSwapInterval, 1); mMaxSwapInterval = std::max(mMaxSwapInterval, 1);}
        if (mDeviceCaps.PresentationIntervals & D3DPRESENT_INTERVAL_TWO)       {mMinSwapInterval = std::min(mMinSwapInterval, 2); mMaxSwapInterval = std::max(mMaxSwapInterval, 2);}
        if (mDeviceCaps.PresentationIntervals & D3DPRESENT_INTERVAL_THREE)     {mMinSwapInterval = std::min(mMinSwapInterval, 3); mMaxSwapInterval = std::max(mMaxSwapInterval, 3);}
        if (mDeviceCaps.PresentationIntervals & D3DPRESENT_INTERVAL_FOUR)      {mMinSwapInterval = std::min(mMinSwapInterval, 4); mMaxSwapInterval = std::max(mMaxSwapInterval, 4);}

        const D3DFORMAT renderTargetFormats[] =
        {
            D3DFMT_A1R5G5B5,
        //  D3DFMT_A2R10G10B10,   // The color_ramp conformance test uses ReadPixels with UNSIGNED_BYTE causing it to think that rendering skipped a colour value.
            D3DFMT_A8R8G8B8,
            D3DFMT_R5G6B5,
            D3DFMT_X1R5G5B5,
            D3DFMT_X8R8G8B8
        };

        const D3DFORMAT depthStencilFormats[] =
        {
        //  D3DFMT_D16_LOCKABLE,
            D3DFMT_D32,
        //  D3DFMT_D15S1,
            D3DFMT_D24S8,
            D3DFMT_D24X8,
        //  D3DFMT_D24X4S4,
            D3DFMT_D16,
        //  D3DFMT_D32F_LOCKABLE,
        //  D3DFMT_D24FS8
        };

        D3DDISPLAYMODE currentDisplayMode;
        mD3d9->GetAdapterDisplayMode(mAdapter, &currentDisplayMode);

        ConfigSet configSet;

        for (int formatIndex = 0; formatIndex < sizeof(renderTargetFormats) / sizeof(D3DFORMAT); formatIndex++)
        {
            D3DFORMAT renderTargetFormat = renderTargetFormats[formatIndex];

            HRESULT result = mD3d9->CheckDeviceFormat(mAdapter, mDeviceType, currentDisplayMode.Format, D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE, renderTargetFormat);

            if (SUCCEEDED(result))
            {
                for (int depthStencilIndex = 0; depthStencilIndex < sizeof(depthStencilFormats) / sizeof(D3DFORMAT); depthStencilIndex++)
                {
                    D3DFORMAT depthStencilFormat = depthStencilFormats[depthStencilIndex];
                    HRESULT result = mD3d9->CheckDeviceFormat(mAdapter, mDeviceType, currentDisplayMode.Format, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, depthStencilFormat);

                    if (SUCCEEDED(result))
                    {
                        HRESULT result = mD3d9->CheckDepthStencilMatch(mAdapter, mDeviceType, currentDisplayMode.Format, renderTargetFormat, depthStencilFormat);

                        if (SUCCEEDED(result))
                        {
                            // FIXME: enumerate multi-sampling

                            configSet.add(currentDisplayMode, mMinSwapInterval, mMaxSwapInterval, renderTargetFormat, depthStencilFormat, 0);
                        }
                    }
                }
            }
        }

        // Give the sorted configs a unique ID and store them internally
        EGLint index = 1;
        for (ConfigSet::Iterator config = configSet.mSet.begin(); config != configSet.mSet.end(); config++)
        {
            Config configuration = *config;
            configuration.mConfigID = index;
            index++;

            mConfigSet.mSet.insert(configuration);
        }

        if (!createDevice())
        {
            terminate();

            return false;
        }
    }

    if (!isInitialized())
    {
        terminate();

        return false;
    }

    return true;
}

void Display::terminate()
{
    for (SurfaceSet::iterator surface = mSurfaceSet.begin(); surface != mSurfaceSet.end(); surface++)
    {
        delete *surface;
    }

    for (ContextSet::iterator context = mContextSet.begin(); context != mContextSet.end(); context++)
    {
        glDestroyContext(*context);
    }

    if (mDevice)
    {
        mDevice->Release();
        mDevice = NULL;
    }

    if (mD3d9)
    {
        mD3d9->Release();
        mD3d9 = NULL;
    }

    if (mDeviceWindow)
    {
        DestroyWindow(mDeviceWindow);
        mDeviceWindow = NULL;
    }
    
    if (mD3d9ex)
    {
        mD3d9ex->Release();
        mD3d9ex = NULL;
    }

    if (mD3d9Module)
    {
        FreeLibrary(mD3d9Module);
        mD3d9Module = NULL;
    }
}

void Display::startScene()
{
    if (!mSceneStarted)
    {
        long result = mDevice->BeginScene();
        ASSERT(SUCCEEDED(result));
        mSceneStarted = true;
    }
}

void Display::endScene()
{
    if (mSceneStarted)
    {
        long result = mDevice->EndScene();
        ASSERT(SUCCEEDED(result));
        mSceneStarted = false;
    }
}

bool Display::getConfigs(EGLConfig *configs, const EGLint *attribList, EGLint configSize, EGLint *numConfig)
{
    return mConfigSet.getConfigs(configs, attribList, configSize, numConfig);
}

bool Display::getConfigAttrib(EGLConfig config, EGLint attribute, EGLint *value)
{
    const egl::Config *configuration = mConfigSet.get(config);

    switch (attribute)
    {
      case EGL_BUFFER_SIZE:               *value = configuration->mBufferSize;             break;
      case EGL_ALPHA_SIZE:                *value = configuration->mAlphaSize;              break;
      case EGL_BLUE_SIZE:                 *value = configuration->mBlueSize;               break;
      case EGL_GREEN_SIZE:                *value = configuration->mGreenSize;              break;
      case EGL_RED_SIZE:                  *value = configuration->mRedSize;                break;
      case EGL_DEPTH_SIZE:                *value = configuration->mDepthSize;              break;
      case EGL_STENCIL_SIZE:              *value = configuration->mStencilSize;            break;
      case EGL_CONFIG_CAVEAT:             *value = configuration->mConfigCaveat;           break;
      case EGL_CONFIG_ID:                 *value = configuration->mConfigID;               break;
      case EGL_LEVEL:                     *value = configuration->mLevel;                  break;
      case EGL_NATIVE_RENDERABLE:         *value = configuration->mNativeRenderable;       break;
      case EGL_NATIVE_VISUAL_TYPE:        *value = configuration->mNativeVisualType;       break;
      case EGL_SAMPLES:                   *value = configuration->mSamples;                break;
      case EGL_SAMPLE_BUFFERS:            *value = configuration->mSampleBuffers;          break;
      case EGL_SURFACE_TYPE:              *value = configuration->mSurfaceType;            break;
      case EGL_TRANSPARENT_TYPE:          *value = configuration->mTransparentType;        break;
      case EGL_TRANSPARENT_BLUE_VALUE:    *value = configuration->mTransparentBlueValue;   break;
      case EGL_TRANSPARENT_GREEN_VALUE:   *value = configuration->mTransparentGreenValue;  break;
      case EGL_TRANSPARENT_RED_VALUE:     *value = configuration->mTransparentRedValue;    break;
      case EGL_BIND_TO_TEXTURE_RGB:       *value = configuration->mBindToTextureRGB;       break;
      case EGL_BIND_TO_TEXTURE_RGBA:      *value = configuration->mBindToTextureRGBA;      break;
      case EGL_MIN_SWAP_INTERVAL:         *value = configuration->mMinSwapInterval;        break;
      case EGL_MAX_SWAP_INTERVAL:         *value = configuration->mMaxSwapInterval;        break;
      case EGL_LUMINANCE_SIZE:            *value = configuration->mLuminanceSize;          break;
      case EGL_ALPHA_MASK_SIZE:           *value = configuration->mAlphaMaskSize;          break;
      case EGL_COLOR_BUFFER_TYPE:         *value = configuration->mColorBufferType;        break;
      case EGL_RENDERABLE_TYPE:           *value = configuration->mRenderableType;         break;
      case EGL_MATCH_NATIVE_PIXMAP:       *value = false; UNIMPLEMENTED();                 break;
      case EGL_CONFORMANT:                *value = configuration->mConformant;             break;
      default:
        return false;
    }

    return true;
}

bool Display::createDevice()
{
    static const TCHAR windowName[] = TEXT("AngleHiddenWindow");
    static const TCHAR className[] = TEXT("STATIC");

    mDeviceWindow = CreateWindowEx(WS_EX_NOACTIVATE, className, windowName, WS_DISABLED | WS_POPUP, 0, 0, 1, 1, HWND_MESSAGE, NULL, GetModuleHandle(NULL), NULL);

    D3DPRESENT_PARAMETERS presentParameters = {0};

    // The default swap chain is never actually used. Surface will create a new swap chain with the proper parameters.
    presentParameters.AutoDepthStencilFormat = D3DFMT_UNKNOWN;
    presentParameters.BackBufferCount = 1;
    presentParameters.BackBufferFormat = D3DFMT_UNKNOWN;
    presentParameters.BackBufferWidth = 1;
    presentParameters.BackBufferHeight = 1;
    presentParameters.EnableAutoDepthStencil = FALSE;
    presentParameters.Flags = 0;
    presentParameters.hDeviceWindow = mDeviceWindow;
    presentParameters.MultiSampleQuality = 0;
    presentParameters.MultiSampleType = D3DMULTISAMPLE_NONE;
    presentParameters.PresentationInterval = convertInterval(mMinSwapInterval);
    presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    presentParameters.Windowed = TRUE;

    DWORD behaviorFlags = D3DCREATE_FPU_PRESERVE | D3DCREATE_NOWINDOWCHANGES;

    HRESULT result = mD3d9->CreateDevice(mAdapter, mDeviceType, mDeviceWindow, behaviorFlags | D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE, &presentParameters, &mDevice);

    if (result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY)
    {
        return error(EGL_BAD_ALLOC, false);
    }

    if (FAILED(result))
    {
        result = mD3d9->CreateDevice(mAdapter, mDeviceType, mDeviceWindow, behaviorFlags | D3DCREATE_SOFTWARE_VERTEXPROCESSING, &presentParameters, &mDevice);

        if (result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY)
        {
            return error(EGL_BAD_ALLOC, false);
        }
    }

    ASSERT(SUCCEEDED(result));

    // Permanent non-default states
    mDevice->SetRenderState(D3DRS_POINTSPRITEENABLE, TRUE);

    mSceneStarted = false;

    return true;
}

Surface *Display::createWindowSurface(HWND window, EGLConfig config)
{
    const Config *configuration = mConfigSet.get(config);

    Surface *surface = new Surface(this, configuration, window);
    mSurfaceSet.insert(surface);

    return surface;
}

EGLContext Display::createContext(EGLConfig configHandle, const gl::Context *shareContext)
{
    const egl::Config *config = mConfigSet.get(configHandle);

    gl::Context *context = glCreateContext(config, shareContext);
    mContextSet.insert(context);

    return context;
}

void Display::destroySurface(egl::Surface *surface)
{
    delete surface;
    mSurfaceSet.erase(surface);
}

void Display::destroyContext(gl::Context *context)
{
    glDestroyContext(context);
    mContextSet.erase(context);
}

bool Display::isInitialized()
{
    return mD3d9 != NULL && mConfigSet.size() > 0;
}

bool Display::isValidConfig(EGLConfig config)
{
    return mConfigSet.get(config) != NULL;
}

bool Display::isValidContext(gl::Context *context)
{
    return mContextSet.find(context) != mContextSet.end();
}

bool Display::isValidSurface(egl::Surface *surface)
{
    return mSurfaceSet.find(surface) != mSurfaceSet.end();
}

bool Display::hasExistingWindowSurface(HWND window)
{
    for (SurfaceSet::iterator surface = mSurfaceSet.begin(); surface != mSurfaceSet.end(); surface++)
    {
        if ((*surface)->getWindowHandle() == window)
        {
            return true;
        }
    }

    return false;
}

void Display::setSwapInterval(GLint interval)
{
    mSwapInterval = interval;
    mSwapInterval = std::max(mSwapInterval, mMinSwapInterval);
    mSwapInterval = std::min(mSwapInterval, mMaxSwapInterval);

    mPresentInterval = convertInterval(mSwapInterval);
}

DWORD Display::getPresentInterval()
{
    return mPresentInterval;
}

DWORD Display::convertInterval(GLint interval)
{
    switch(interval)
    {
      case 0: return D3DPRESENT_INTERVAL_IMMEDIATE;
      case 1: return D3DPRESENT_INTERVAL_ONE;
      case 2: return D3DPRESENT_INTERVAL_TWO;
      case 3: return D3DPRESENT_INTERVAL_THREE;
      case 4: return D3DPRESENT_INTERVAL_FOUR;
      default: UNREACHABLE();
    }

    return D3DPRESENT_INTERVAL_DEFAULT;
}

IDirect3DDevice9 *Display::getDevice()
{
    return mDevice;
}

D3DCAPS9 Display::getDeviceCaps()
{
    return mDeviceCaps;
}

void Display::getMultiSampleSupport(D3DFORMAT format, bool *multiSampleArray)
{
    for (int multiSampleIndex = 0; multiSampleIndex <= D3DMULTISAMPLE_16_SAMPLES; multiSampleIndex++)
    {
        HRESULT result = mD3d9->CheckDeviceMultiSampleType(mAdapter, mDeviceType, format,
                                                           TRUE, (D3DMULTISAMPLE_TYPE)multiSampleIndex, NULL);

        multiSampleArray[multiSampleIndex] = SUCCEEDED(result);
    }
}

bool Display::getCompressedTextureSupport()
{
    D3DDISPLAYMODE currentDisplayMode;
    mD3d9->GetAdapterDisplayMode(mAdapter, &currentDisplayMode);

    return SUCCEEDED(mD3d9->CheckDeviceFormat(mAdapter, mDeviceType, currentDisplayMode.Format, 0, D3DRTYPE_TEXTURE, D3DFMT_DXT1));
}
}
