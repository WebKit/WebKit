//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Display.cpp: Implements the egl::Display class, representing the abstract
// display on which graphics are drawn. Implements EGLDisplay.
// [EGL 1.4] section 2.1.2 page 3.

#include "libEGL/Display.h"

#include <algorithm>
#include <map>
#include <vector>

#include "common/debug.h"
#include "libGLESv2/mathutil.h"
#include "libGLESv2/utilities.h"

#include "libEGL/main.h"

// Can also be enabled by defining FORCE_REF_RAST in the project's predefined macros
#define REF_RAST 0

// The "Debug This Pixel..." feature in PIX often fails when using the
// D3D9Ex interfaces.  In order to get debug pixel to work on a Vista/Win 7
// machine, define "ANGLE_ENABLE_D3D9EX=0" in your project file.
#if !defined(ANGLE_ENABLE_D3D9EX)
// Enables use of the IDirect3D9Ex interface, when available
#define ANGLE_ENABLE_D3D9EX 1
#endif // !defined(ANGLE_ENABLE_D3D9EX)

namespace egl
{
namespace
{
    typedef std::map<EGLNativeDisplayType, Display*> DisplayMap; 
    DisplayMap displays;
}

egl::Display *Display::getDisplay(EGLNativeDisplayType displayId)
{
    if (displays.find(displayId) != displays.end())
    {
        return displays[displayId];
    }

    egl::Display *display = NULL;

    if (displayId == EGL_DEFAULT_DISPLAY)
    {
        display = new egl::Display(displayId, (HDC)NULL, false);
    }
    else if (displayId == EGL_SOFTWARE_DISPLAY_ANGLE)
    {
        display = new egl::Display(displayId, (HDC)NULL, true);
    }
    else
    {
        // FIXME: Check if displayId is a valid display device context

        display = new egl::Display(displayId, (HDC)displayId, false);
    }

    displays[displayId] = display;
    return display;
}

Display::Display(EGLNativeDisplayType displayId, HDC deviceContext, bool software) : mDc(deviceContext)
{
    mD3d9Module = NULL;
    
    mD3d9 = NULL;
    mD3d9Ex = NULL;
    mDevice = NULL;
    mDeviceEx = NULL;
    mDeviceWindow = NULL;

    mAdapter = D3DADAPTER_DEFAULT;

    #if REF_RAST == 1 || defined(FORCE_REF_RAST)
        mDeviceType = D3DDEVTYPE_REF;
    #else
        mDeviceType = D3DDEVTYPE_HAL;
    #endif

    mMinSwapInterval = 1;
    mMaxSwapInterval = 1;
    mSoftwareDevice = software;
    mDisplayId = displayId;
    mDeviceLost = false;
}

Display::~Display()
{
    terminate();

    DisplayMap::iterator thisDisplay = displays.find(mDisplayId);

    if (thisDisplay != displays.end())
    {
      displays.erase(thisDisplay);
    }
}

bool Display::initialize()
{
    if (isInitialized())
    {
        return true;
    }

    if (mSoftwareDevice)
    {
      mD3d9Module = GetModuleHandle(TEXT("swiftshader_d3d9.dll"));
    } 
    else
    {
      mD3d9Module = GetModuleHandle(TEXT("d3d9.dll"));
    }
    if (mD3d9Module == NULL)
    {
        terminate();
        return false;
    }

    typedef HRESULT (WINAPI *Direct3DCreate9ExFunc)(UINT, IDirect3D9Ex**);
    Direct3DCreate9ExFunc Direct3DCreate9ExPtr = reinterpret_cast<Direct3DCreate9ExFunc>(GetProcAddress(mD3d9Module, "Direct3DCreate9Ex"));

    // Use Direct3D9Ex if available. Among other things, this version is less
    // inclined to report a lost context, for example when the user switches
    // desktop. Direct3D9Ex is available in Windows Vista and later if suitable drivers are available.
    if (ANGLE_ENABLE_D3D9EX && Direct3DCreate9ExPtr && SUCCEEDED(Direct3DCreate9ExPtr(D3D_SDK_VERSION, &mD3d9Ex)))
    {
        ASSERT(mD3d9Ex);
        mD3d9Ex->QueryInterface(IID_IDirect3D9, reinterpret_cast<void**>(&mD3d9));
        ASSERT(mD3d9);
    }
    else
    {
        mD3d9 = Direct3DCreate9(D3D_SDK_VERSION);
    }

    if (mD3d9)
    {
        if (mDc != NULL)
        {
        //  UNIMPLEMENTED();   // FIXME: Determine which adapter index the device context corresponds to
        }

        HRESULT result;
        
        // Give up on getting device caps after about one second.
        for (int i = 0; i < 10; ++i)
        {
            result = mD3d9->GetDeviceCaps(mAdapter, mDeviceType, &mDeviceCaps);
            
            if (SUCCEEDED(result))
            {
                break;
            }
            else if (result == D3DERR_NOTAVAILABLE)
            {
                Sleep(100);   // Give the driver some time to initialize/recover
            }
            else if (FAILED(result))   // D3DERR_OUTOFVIDEOMEMORY, E_OUTOFMEMORY, D3DERR_INVALIDDEVICE, or another error we can't recover from
            {
                terminate();
                return error(EGL_BAD_ALLOC, false);
            }
        }

        if (mDeviceCaps.PixelShaderVersion < D3DPS_VERSION(2, 0))
        {
            terminate();
            return error(EGL_NOT_INITIALIZED, false);
        }

        // When DirectX9 is running with an older DirectX8 driver, a StretchRect from a regular texture to a render target texture is not supported.
        // This is required by Texture2D::convertToRenderTarget.
        if ((mDeviceCaps.DevCaps2 & D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES) == 0)
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

        mD3d9->GetAdapterIdentifier(mAdapter, 0, &mAdapterIdentifier);

        const D3DFORMAT renderTargetFormats[] =
        {
            D3DFMT_A1R5G5B5,
        //  D3DFMT_A2R10G10B10,   // The color_ramp conformance test uses ReadPixels with UNSIGNED_BYTE causing it to think that rendering skipped a colour value.
            D3DFMT_A8R8G8B8,
            D3DFMT_R5G6B5,
        //  D3DFMT_X1R5G5B5,      // Has no compatible OpenGL ES renderbuffer format
            D3DFMT_X8R8G8B8
        };

        const D3DFORMAT depthStencilFormats[] =
        {
            D3DFMT_UNKNOWN,
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
                    HRESULT result = D3D_OK;
                    
                    if(depthStencilFormat != D3DFMT_UNKNOWN)
                    {
                        result = mD3d9->CheckDeviceFormat(mAdapter, mDeviceType, currentDisplayMode.Format, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, depthStencilFormat);
                    }

                    if (SUCCEEDED(result))
                    {
                        if(depthStencilFormat != D3DFMT_UNKNOWN)
                        {
                            result = mD3d9->CheckDepthStencilMatch(mAdapter, mDeviceType, currentDisplayMode.Format, renderTargetFormat, depthStencilFormat);
                        }

                        if (SUCCEEDED(result))
                        {
                            // FIXME: enumerate multi-sampling

                            configSet.add(currentDisplayMode, mMinSwapInterval, mMaxSwapInterval, renderTargetFormat, depthStencilFormat, 0,
                                          mDeviceCaps.MaxTextureWidth, mDeviceCaps.MaxTextureHeight);
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
    }

    if (!isInitialized())
    {
        terminate();

        return false;
    }

    initExtensionString();

    static const TCHAR windowName[] = TEXT("AngleHiddenWindow");
    static const TCHAR className[] = TEXT("STATIC");

    mDeviceWindow = CreateWindowEx(WS_EX_NOACTIVATE, className, windowName, WS_DISABLED | WS_POPUP, 0, 0, 1, 1, HWND_MESSAGE, NULL, GetModuleHandle(NULL), NULL);

    if (!createDevice())
    {
        terminate();
        return false;
    }

    return true;
}

void Display::terminate()
{
    while (!mSurfaceSet.empty())
    {
        destroySurface(*mSurfaceSet.begin());
    }

    while (!mContextSet.empty())
    {
        destroyContext(*mContextSet.begin());
    }

    while (!mEventQueryPool.empty())
    {
        mEventQueryPool.back()->Release();
        mEventQueryPool.pop_back();
    }

    if (mDevice)
    {
        // If the device is lost, reset it first to prevent leaving the driver in an unstable state
        if (testDeviceLost())
        {
            resetDevice();
        }

        mDevice->Release();
        mDevice = NULL;
    }

    if (mDeviceEx)
    {
        mDeviceEx->Release();
        mDeviceEx = NULL;
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
    
    if (mD3d9Ex)
    {
        mD3d9Ex->Release();
        mD3d9Ex = NULL;
    }

    if (mD3d9Module)
    {
        mD3d9Module = NULL;
    }
}

void Display::startScene()
{
    if (!mSceneStarted)
    {
        long result = mDevice->BeginScene();
        if (SUCCEEDED(result)) {
            // This is defensive checking against the device being
            // lost at unexpected times.
            mSceneStarted = true;
        }
    }
}

void Display::endScene()
{
    if (mSceneStarted)
    {
        // EndScene can fail if the device was lost, for example due
        // to a TDR during a draw call.
        mDevice->EndScene();
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
      case EGL_MAX_PBUFFER_WIDTH:         *value = configuration->mMaxPBufferWidth;        break;
      case EGL_MAX_PBUFFER_HEIGHT:        *value = configuration->mMaxPBufferHeight;       break;
      case EGL_MAX_PBUFFER_PIXELS:        *value = configuration->mMaxPBufferPixels;       break;
      default:
        return false;
    }

    return true;
}

bool Display::createDevice()
{
    D3DPRESENT_PARAMETERS presentParameters = getDefaultPresentParameters();
    DWORD behaviorFlags = D3DCREATE_FPU_PRESERVE | D3DCREATE_NOWINDOWCHANGES;

    HRESULT result = mD3d9->CreateDevice(mAdapter, mDeviceType, mDeviceWindow, behaviorFlags | D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE, &presentParameters, &mDevice);

    if (result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY || result == D3DERR_DEVICELOST)
    {
        return error(EGL_BAD_ALLOC, false);
    }

    if (FAILED(result))
    {
        result = mD3d9->CreateDevice(mAdapter, mDeviceType, mDeviceWindow, behaviorFlags | D3DCREATE_SOFTWARE_VERTEXPROCESSING, &presentParameters, &mDevice);

        if (FAILED(result))
        {
            ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY || result == D3DERR_NOTAVAILABLE || result == D3DERR_DEVICELOST);
            return error(EGL_BAD_ALLOC, false);
        }
    }

    if (mD3d9Ex)
    {
        result = mDevice->QueryInterface(IID_IDirect3DDevice9Ex, (void**) &mDeviceEx);
        ASSERT(SUCCEEDED(result));
    }

    initializeDevice();

    return true;
}

// do any one-time device initialization
// NOTE: this is also needed after a device lost/reset
// to reset the scene status and ensure the default states are reset.
void Display::initializeDevice()
{
    // Permanent non-default states
    mDevice->SetRenderState(D3DRS_POINTSPRITEENABLE, TRUE);
    mDevice->SetRenderState(D3DRS_LASTPIXEL, FALSE);

    mSceneStarted = false;
}

bool Display::resetDevice()
{
    D3DPRESENT_PARAMETERS presentParameters = getDefaultPresentParameters();

    HRESULT result = D3D_OK;
    bool lost = testDeviceLost();
    int attempts = 3;    

    while (lost && attempts > 0)
    {
        if (mDeviceEx)
        {
            Sleep(500);   // Give the graphics driver some CPU time
            result = mDeviceEx->ResetEx(&presentParameters, NULL);
        }
        else
        {
            result = mDevice->TestCooperativeLevel();
            
            while (result == D3DERR_DEVICELOST)
            {
                Sleep(100);   // Give the graphics driver some CPU time
                result = mDevice->TestCooperativeLevel();
            }

            if (result == D3DERR_DEVICENOTRESET)
            {
                result = mDevice->Reset(&presentParameters);
            }
        }

        lost = testDeviceLost();
        attempts --;
    }

    if (FAILED(result))
    {
        ERR("Reset/ResetEx failed multiple times: 0x%08X", result);
        return error(EGL_BAD_ALLOC, false);
    }

    // reset device defaults
    initializeDevice();

    return true;
}

EGLSurface Display::createWindowSurface(HWND window, EGLConfig config, const EGLint *attribList)
{
    const Config *configuration = mConfigSet.get(config);
    EGLint postSubBufferSupported = EGL_FALSE;

    if (attribList)
    {
        while (*attribList != EGL_NONE)
        {
            switch (attribList[0])
            {
              case EGL_RENDER_BUFFER:
                switch (attribList[1])
                {
                  case EGL_BACK_BUFFER:
                    break;
                  case EGL_SINGLE_BUFFER:
                    return error(EGL_BAD_MATCH, EGL_NO_SURFACE);   // Rendering directly to front buffer not supported
                  default:
                    return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
                }
                break;
              case EGL_POST_SUB_BUFFER_SUPPORTED_NV:
                postSubBufferSupported = attribList[1];
                break;
              case EGL_VG_COLORSPACE:
                return error(EGL_BAD_MATCH, EGL_NO_SURFACE);
              case EGL_VG_ALPHA_FORMAT:
                return error(EGL_BAD_MATCH, EGL_NO_SURFACE);
              default:
                return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
            }

            attribList += 2;
        }
    }

    if (hasExistingWindowSurface(window))
    {
        return error(EGL_BAD_ALLOC, EGL_NO_SURFACE);
    }

    if (testDeviceLost()) 
    {
        if (!restoreLostDevice())
            return EGL_NO_SURFACE;
    }

    Surface *surface = new Surface(this, configuration, window, postSubBufferSupported);

    if (!surface->initialize())
    {
        delete surface;
        return EGL_NO_SURFACE;
    }

    mSurfaceSet.insert(surface);

    return success(surface);
}

EGLSurface Display::createOffscreenSurface(EGLConfig config, HANDLE shareHandle, const EGLint *attribList)
{
    EGLint width = 0, height = 0;
    EGLenum textureFormat = EGL_NO_TEXTURE;
    EGLenum textureTarget = EGL_NO_TEXTURE;
    const Config *configuration = mConfigSet.get(config);

    if (attribList)
    {
        while (*attribList != EGL_NONE)
        {
            switch (attribList[0])
            {
              case EGL_WIDTH:
                width = attribList[1];
                break;
              case EGL_HEIGHT:
                height = attribList[1];
                break;
              case EGL_LARGEST_PBUFFER:
                if (attribList[1] != EGL_FALSE)
                  UNIMPLEMENTED(); // FIXME
                break;
              case EGL_TEXTURE_FORMAT:
                switch (attribList[1])
                {
                  case EGL_NO_TEXTURE:
                  case EGL_TEXTURE_RGB:
                  case EGL_TEXTURE_RGBA:
                    textureFormat = attribList[1];
                    break;
                  default:
                    return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
                }
                break;
              case EGL_TEXTURE_TARGET:
                switch (attribList[1])
                {
                  case EGL_NO_TEXTURE:
                  case EGL_TEXTURE_2D:
                    textureTarget = attribList[1];
                    break;
                  default:
                    return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
                }
                break;
              case EGL_MIPMAP_TEXTURE:
                if (attribList[1] != EGL_FALSE)
                  return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
                break;
              case EGL_VG_COLORSPACE:
                return error(EGL_BAD_MATCH, EGL_NO_SURFACE);
              case EGL_VG_ALPHA_FORMAT:
                return error(EGL_BAD_MATCH, EGL_NO_SURFACE);
              default:
                return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
            }

            attribList += 2;
        }
    }

    if (width < 0 || height < 0)
    {
        return error(EGL_BAD_PARAMETER, EGL_NO_SURFACE);
    }

    if (width == 0 || height == 0)
    {
        return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
    }

    if (textureFormat != EGL_NO_TEXTURE && !getNonPower2TextureSupport() && (!gl::isPow2(width) || !gl::isPow2(height)))
    {
        return error(EGL_BAD_MATCH, EGL_NO_SURFACE);
    }

    if ((textureFormat != EGL_NO_TEXTURE && textureTarget == EGL_NO_TEXTURE) ||
        (textureFormat == EGL_NO_TEXTURE && textureTarget != EGL_NO_TEXTURE))
    {
        return error(EGL_BAD_MATCH, EGL_NO_SURFACE);
    }

    if (!(configuration->mSurfaceType & EGL_PBUFFER_BIT))
    {
        return error(EGL_BAD_MATCH, EGL_NO_SURFACE);
    }

    if ((textureFormat == EGL_TEXTURE_RGB && configuration->mBindToTextureRGB != EGL_TRUE) ||
        (textureFormat == EGL_TEXTURE_RGBA && configuration->mBindToTextureRGBA != EGL_TRUE))
    {
        return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
    }

    if (testDeviceLost()) 
    {
        if (!restoreLostDevice())
            return EGL_NO_SURFACE;
    }

    Surface *surface = new Surface(this, configuration, shareHandle, width, height, textureFormat, textureTarget);

    if (!surface->initialize())
    {
        delete surface;
        return EGL_NO_SURFACE;
    }

    mSurfaceSet.insert(surface);

    return success(surface);
}

EGLContext Display::createContext(EGLConfig configHandle, const gl::Context *shareContext, bool notifyResets, bool robustAccess)
{
    if (!mDevice)
    {
        if (!createDevice())
        {
            return NULL;
        }
    }
    else if (testDeviceLost())   // Lost device
    {
        if (!restoreLostDevice())
            return NULL;
    }

    const egl::Config *config = mConfigSet.get(configHandle);

    gl::Context *context = glCreateContext(config, shareContext, notifyResets, robustAccess);
    mContextSet.insert(context);
    mDeviceLost = false;

    return context;
}

bool Display::restoreLostDevice()
{
    for (ContextSet::iterator ctx = mContextSet.begin(); ctx != mContextSet.end(); ctx++)
    {
        if ((*ctx)->isResetNotificationEnabled())
            return false;   // If reset notifications have been requested, application must delete all contexts first
    }
 
    // Release surface resources to make the Reset() succeed
    for (SurfaceSet::iterator surface = mSurfaceSet.begin(); surface != mSurfaceSet.end(); surface++)
    {
        (*surface)->release();
    }

    while (!mEventQueryPool.empty())
    {
        mEventQueryPool.back()->Release();
        mEventQueryPool.pop_back();
    }

    if (!resetDevice())
    {
        return false;
    }

    // Restore any surfaces that may have been lost
    for (SurfaceSet::iterator surface = mSurfaceSet.begin(); surface != mSurfaceSet.end(); surface++)
    {
        (*surface)->resetSwapChain();
    }

    return true;
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

void Display::notifyDeviceLost()
{
    for (ContextSet::iterator context = mContextSet.begin(); context != mContextSet.end(); context++)
    {
        (*context)->markContextLost();
    }
    mDeviceLost = true;
    error(EGL_CONTEXT_LOST);
}

bool Display::isDeviceLost()
{
    return mDeviceLost;
}

bool Display::isInitialized() const
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

EGLint Display::getMinSwapInterval()
{
    return mMinSwapInterval;
}

EGLint Display::getMaxSwapInterval()
{
    return mMaxSwapInterval;
}

IDirect3DDevice9 *Display::getDevice()
{
    if (!mDevice)
    {
        if (!createDevice())
        {
            return NULL;
        }
    }

    return mDevice;
}

D3DCAPS9 Display::getDeviceCaps()
{
    return mDeviceCaps;
}

D3DADAPTER_IDENTIFIER9 *Display::getAdapterIdentifier()
{
    return &mAdapterIdentifier;
}

bool Display::testDeviceLost()
{
    if (mDeviceEx)
    {
        return FAILED(mDeviceEx->CheckDeviceState(NULL));
    }
    else if (mDevice)
    {
        return FAILED(mDevice->TestCooperativeLevel());
    }

    return false;   // No device yet, so no reset required
}

bool Display::testDeviceResettable()
{
    HRESULT status = D3D_OK;

    if (mDeviceEx)
    {
        status = mDeviceEx->CheckDeviceState(NULL);
    }
    else if (mDevice)
    {
        status = mDevice->TestCooperativeLevel();
    }

    switch (status)
    {
      case D3DERR_DEVICENOTRESET:
      case D3DERR_DEVICEHUNG:
        return true;
      default:
        return false;
    }
}

void Display::sync(bool block)
{
    HRESULT result;

    IDirect3DQuery9* query = allocateEventQuery();
    if (!query)
    {
        return;
    }

    result = query->Issue(D3DISSUE_END);
    ASSERT(SUCCEEDED(result));

    do
    {
        result = query->GetData(NULL, 0, D3DGETDATA_FLUSH);

        if(block && result == S_FALSE)
        {
            // Keep polling, but allow other threads to do something useful first
            Sleep(0);
            // explicitly check for device loss
            // some drivers seem to return S_FALSE even if the device is lost
            // instead of D3DERR_DEVICELOST like they should
            if (testDeviceLost())
            {
                result = D3DERR_DEVICELOST;
            }
        }
    }
    while(block && result == S_FALSE);

    freeEventQuery(query);

    if (isDeviceLostError(result))
    {
        notifyDeviceLost();
    }
}

IDirect3DQuery9* Display::allocateEventQuery()
{
    IDirect3DQuery9 *query = NULL;

    if (mEventQueryPool.empty())
    {
        HRESULT result = mDevice->CreateQuery(D3DQUERYTYPE_EVENT, &query);
        ASSERT(SUCCEEDED(result));
    }
    else
    {
        query = mEventQueryPool.back();
        mEventQueryPool.pop_back();
    }

    return query;
}

void Display::freeEventQuery(IDirect3DQuery9* query)
{
    if (mEventQueryPool.size() > 1000)
    {
        query->Release();
    }
    else
    {
        mEventQueryPool.push_back(query);
    }
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

bool Display::getDXT1TextureSupport()
{
    D3DDISPLAYMODE currentDisplayMode;
    mD3d9->GetAdapterDisplayMode(mAdapter, &currentDisplayMode);

    return SUCCEEDED(mD3d9->CheckDeviceFormat(mAdapter, mDeviceType, currentDisplayMode.Format, 0, D3DRTYPE_TEXTURE, D3DFMT_DXT1));
}

bool Display::getDXT3TextureSupport()
{
    D3DDISPLAYMODE currentDisplayMode;
    mD3d9->GetAdapterDisplayMode(mAdapter, &currentDisplayMode);

    return SUCCEEDED(mD3d9->CheckDeviceFormat(mAdapter, mDeviceType, currentDisplayMode.Format, 0, D3DRTYPE_TEXTURE, D3DFMT_DXT3));
}

bool Display::getDXT5TextureSupport()
{
    D3DDISPLAYMODE currentDisplayMode;
    mD3d9->GetAdapterDisplayMode(mAdapter, &currentDisplayMode);

    return SUCCEEDED(mD3d9->CheckDeviceFormat(mAdapter, mDeviceType, currentDisplayMode.Format, 0, D3DRTYPE_TEXTURE, D3DFMT_DXT5));
}

bool Display::getFloat32TextureSupport(bool *filtering, bool *renderable)
{
    D3DDISPLAYMODE currentDisplayMode;
    mD3d9->GetAdapterDisplayMode(mAdapter, &currentDisplayMode);

    *filtering = SUCCEEDED(mD3d9->CheckDeviceFormat(mAdapter, mDeviceType, currentDisplayMode.Format, D3DUSAGE_QUERY_FILTER, 
                                                    D3DRTYPE_TEXTURE, D3DFMT_A32B32G32R32F)) &&
                 SUCCEEDED(mD3d9->CheckDeviceFormat(mAdapter, mDeviceType, currentDisplayMode.Format, D3DUSAGE_QUERY_FILTER,
                                                    D3DRTYPE_CUBETEXTURE, D3DFMT_A32B32G32R32F));
    
    *renderable = SUCCEEDED(mD3d9->CheckDeviceFormat(mAdapter, mDeviceType, currentDisplayMode.Format, D3DUSAGE_RENDERTARGET,
                                                     D3DRTYPE_TEXTURE, D3DFMT_A32B32G32R32F))&&
                  SUCCEEDED(mD3d9->CheckDeviceFormat(mAdapter, mDeviceType, currentDisplayMode.Format, D3DUSAGE_RENDERTARGET,
                                                     D3DRTYPE_CUBETEXTURE, D3DFMT_A32B32G32R32F));

    if (!*filtering && !*renderable)
    {
        return SUCCEEDED(mD3d9->CheckDeviceFormat(mAdapter, mDeviceType, currentDisplayMode.Format, 0, 
                                                  D3DRTYPE_TEXTURE, D3DFMT_A32B32G32R32F)) &&
               SUCCEEDED(mD3d9->CheckDeviceFormat(mAdapter, mDeviceType, currentDisplayMode.Format, 0,
                                                  D3DRTYPE_CUBETEXTURE, D3DFMT_A32B32G32R32F));
    }
    else
    {
        return true;
    }
}

bool Display::getFloat16TextureSupport(bool *filtering, bool *renderable)
{
    D3DDISPLAYMODE currentDisplayMode;
    mD3d9->GetAdapterDisplayMode(mAdapter, &currentDisplayMode);

    *filtering = SUCCEEDED(mD3d9->CheckDeviceFormat(mAdapter, mDeviceType, currentDisplayMode.Format, D3DUSAGE_QUERY_FILTER, 
                                                    D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16F)) &&
                 SUCCEEDED(mD3d9->CheckDeviceFormat(mAdapter, mDeviceType, currentDisplayMode.Format, D3DUSAGE_QUERY_FILTER,
                                                    D3DRTYPE_CUBETEXTURE, D3DFMT_A16B16G16R16F));
    
    *renderable = SUCCEEDED(mD3d9->CheckDeviceFormat(mAdapter, mDeviceType, currentDisplayMode.Format, D3DUSAGE_RENDERTARGET, 
                                                    D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16F)) &&
                 SUCCEEDED(mD3d9->CheckDeviceFormat(mAdapter, mDeviceType, currentDisplayMode.Format, D3DUSAGE_RENDERTARGET,
                                                    D3DRTYPE_CUBETEXTURE, D3DFMT_A16B16G16R16F));

    if (!*filtering && !*renderable)
    {
        return SUCCEEDED(mD3d9->CheckDeviceFormat(mAdapter, mDeviceType, currentDisplayMode.Format, 0, 
                                                  D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16F)) &&
               SUCCEEDED(mD3d9->CheckDeviceFormat(mAdapter, mDeviceType, currentDisplayMode.Format, 0,
                                                  D3DRTYPE_CUBETEXTURE, D3DFMT_A16B16G16R16F));
    }
    else
    {
        return true;
    }
}

bool Display::getLuminanceTextureSupport()
{
    D3DDISPLAYMODE currentDisplayMode;
    mD3d9->GetAdapterDisplayMode(mAdapter, &currentDisplayMode);

    return SUCCEEDED(mD3d9->CheckDeviceFormat(mAdapter, mDeviceType, currentDisplayMode.Format, 0, D3DRTYPE_TEXTURE, D3DFMT_L8));
}

bool Display::getLuminanceAlphaTextureSupport()
{
    D3DDISPLAYMODE currentDisplayMode;
    mD3d9->GetAdapterDisplayMode(mAdapter, &currentDisplayMode);

    return SUCCEEDED(mD3d9->CheckDeviceFormat(mAdapter, mDeviceType, currentDisplayMode.Format, 0, D3DRTYPE_TEXTURE, D3DFMT_A8L8));
}

D3DPOOL Display::getBufferPool(DWORD usage) const
{
    if (mD3d9Ex != NULL)
    {
        return D3DPOOL_DEFAULT;
    }
    else
    {
        if (!(usage & D3DUSAGE_DYNAMIC))
        {
            return D3DPOOL_MANAGED;
        }
    }

    return D3DPOOL_DEFAULT;
}

D3DPOOL Display::getTexturePool(bool renderable) const
{
    if (mD3d9Ex != NULL)
    {
        return D3DPOOL_DEFAULT;
    }
    else
    {
        if (!renderable)
        {
            return D3DPOOL_MANAGED;
        }
    }

    return D3DPOOL_DEFAULT;
}

bool Display::getEventQuerySupport()
{
    IDirect3DQuery9 *query = allocateEventQuery();
    if (query)
    {
        freeEventQuery(query);
        return true;
    }
    else
    {
        return false;
    }
}

D3DPRESENT_PARAMETERS Display::getDefaultPresentParameters()
{
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
    presentParameters.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
    presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    presentParameters.Windowed = TRUE;

    return presentParameters;
}

void Display::initExtensionString()
{
    HMODULE swiftShader = GetModuleHandle(TEXT("swiftshader_d3d9.dll"));

    mExtensionString = "";

    // Multi-vendor (EXT) extensions
    mExtensionString += "EGL_EXT_create_context_robustness ";

    // ANGLE-specific extensions
    if (shareHandleSupported())
    {
        mExtensionString += "EGL_ANGLE_d3d_share_handle_client_buffer ";
    }

    mExtensionString += "EGL_ANGLE_query_surface_pointer ";

    if (swiftShader)
    {
        mExtensionString += "EGL_ANGLE_software_display ";
    }

    if (shareHandleSupported())
    {
        mExtensionString += "EGL_ANGLE_surface_d3d_texture_2d_share_handle ";
    }

    mExtensionString += "EGL_NV_post_sub_buffer";

    std::string::size_type end = mExtensionString.find_last_not_of(' ');
    if (end != std::string::npos)
    {
        mExtensionString.resize(end+1);
    }
}

const char *Display::getExtensionString() const
{
    return mExtensionString.c_str();
}

bool Display::shareHandleSupported() const 
{
    // PIX doesn't seem to support using share handles, so disable them.
    return isD3d9ExDevice() && !gl::perfActive();
}

// Only Direct3D 10 ready devices support all the necessary vertex texture formats.
// We test this using D3D9 by checking support for the R16F format.
bool Display::getVertexTextureSupport() const
{
    if (!isInitialized() || mDeviceCaps.PixelShaderVersion < D3DPS_VERSION(3, 0))
    {
        return false;
    }

    D3DDISPLAYMODE currentDisplayMode;
    mD3d9->GetAdapterDisplayMode(mAdapter, &currentDisplayMode);

    HRESULT result = mD3d9->CheckDeviceFormat(mAdapter, mDeviceType, currentDisplayMode.Format, D3DUSAGE_QUERY_VERTEXTEXTURE, D3DRTYPE_TEXTURE, D3DFMT_R16F);

    return SUCCEEDED(result);
}

bool Display::getNonPower2TextureSupport() const
{
    return !(mDeviceCaps.TextureCaps & D3DPTEXTURECAPS_POW2) &&
           !(mDeviceCaps.TextureCaps & D3DPTEXTURECAPS_CUBEMAP_POW2) &&
           !(mDeviceCaps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL);
}

bool Display::getOcclusionQuerySupport() const
{
    if (!isInitialized())
    {
        return false;
    }

    IDirect3DQuery9 *query = NULL;
    HRESULT result = mDevice->CreateQuery(D3DQUERYTYPE_OCCLUSION, &query);
    
    if (SUCCEEDED(result) && query)
    {
        query->Release();
        return true;
    }
    else
    {
        return false;
    }
}

bool Display::getInstancingSupport() const
{
    return mDeviceCaps.PixelShaderVersion >= D3DPS_VERSION(3, 0); 
}

}
