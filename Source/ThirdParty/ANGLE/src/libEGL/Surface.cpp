//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Surface.cpp: Implements the egl::Surface class, representing a drawing surface
// such as the client area of a window, including any back buffers.
// Implements EGLSurface and related functionality. [EGL 1.4] section 2.2 page 3.

#include <tchar.h>

#include "libEGL/Surface.h"

#include "common/debug.h"
#include "libGLESv2/Texture.h"

#include "libEGL/main.h"
#include "libEGL/Display.h"

#include <dwmapi.h>

namespace egl
{

namespace
{
const int versionWindowsVista = MAKEWORD(0x00, 0x06);
const int versionWindows7 = MAKEWORD(0x01, 0x06);

// Return the version of the operating system in a format suitable for ordering
// comparison.
int getComparableOSVersion()
{
    DWORD version = GetVersion();
    int majorVersion = LOBYTE(LOWORD(version));
    int minorVersion = HIBYTE(LOWORD(version));
    return MAKEWORD(minorVersion, majorVersion);
}
}

Surface::Surface(Display *display, const Config *config, HWND window, EGLint postSubBufferSupported) 
    : mDisplay(display), mConfig(config), mWindow(window), mPostSubBufferSupported(postSubBufferSupported)
{
    mSwapChain = NULL;
    mBackBuffer = NULL;
    mDepthStencil = NULL;
    mRenderTarget = NULL;
    mOffscreenTexture = NULL;
    mShareHandle = NULL;
    mTexture = NULL;
    mTextureFormat = EGL_NO_TEXTURE;
    mTextureTarget = EGL_NO_TEXTURE;

    mPixelAspectRatio = (EGLint)(1.0 * EGL_DISPLAY_SCALING);   // FIXME: Determine actual pixel aspect ratio
    mRenderBuffer = EGL_BACK_BUFFER;
    mSwapBehavior = EGL_BUFFER_PRESERVED;
    mSwapInterval = -1;
    setSwapInterval(1);

    subclassWindow();
}

Surface::Surface(Display *display, const Config *config, HANDLE shareHandle, EGLint width, EGLint height, EGLenum textureFormat, EGLenum textureType)
    : mDisplay(display), mWindow(NULL), mConfig(config), mShareHandle(shareHandle), mWidth(width), mHeight(height), mPostSubBufferSupported(EGL_FALSE)
{
    mSwapChain = NULL;
    mBackBuffer = NULL;
    mDepthStencil = NULL;
    mRenderTarget = NULL;
    mOffscreenTexture = NULL;
    mWindowSubclassed = false;
    mTexture = NULL;
    mTextureFormat = textureFormat;
    mTextureTarget = textureType;

    mPixelAspectRatio = (EGLint)(1.0 * EGL_DISPLAY_SCALING);   // FIXME: Determine actual pixel aspect ratio
    mRenderBuffer = EGL_BACK_BUFFER;
    mSwapBehavior = EGL_BUFFER_PRESERVED;
    mSwapInterval = -1;
    setSwapInterval(1);
}

Surface::~Surface()
{
    unsubclassWindow();
    release();
}

bool Surface::initialize()
{
    ASSERT(!mSwapChain && !mOffscreenTexture && !mDepthStencil);

    if (!resetSwapChain())
      return false;

    // Modify present parameters for this window, if we are composited,
    // to minimize the amount of queuing done by DWM between our calls to
    // present and the actual screen.
    if (mWindow && (getComparableOSVersion() >= versionWindowsVista)) {
      BOOL isComposited;
      HRESULT result = DwmIsCompositionEnabled(&isComposited);
      if (SUCCEEDED(result) && isComposited) {
        DWM_PRESENT_PARAMETERS presentParams;
        memset(&presentParams, 0, sizeof(presentParams));
        presentParams.cbSize = sizeof(DWM_PRESENT_PARAMETERS);
        presentParams.cBuffer = 2;

        result = DwmSetPresentParameters(mWindow, &presentParams);
        if (FAILED(result))
          ERR("Unable to set present parameters: 0x%08X", result);
      }
    }

    return true;
}

void Surface::release()
{
    if (mSwapChain)
    {
        mSwapChain->Release();
        mSwapChain = NULL;
    }

    if (mBackBuffer)
    {
        mBackBuffer->Release();
        mBackBuffer = NULL;
    }

    if (mDepthStencil)
    {
        mDepthStencil->Release();
        mDepthStencil = NULL;
    }

    if (mRenderTarget)
    {
        mRenderTarget->Release();
        mRenderTarget = NULL;
    }

    if (mOffscreenTexture)
    {
        mOffscreenTexture->Release();
        mOffscreenTexture = NULL;
    }

    if (mTexture)
    {
        mTexture->releaseTexImage();
        mTexture = NULL;
    }

    mShareHandle = NULL;
}

bool Surface::resetSwapChain()
{
    if (!mWindow)
    {
        return resetSwapChain(mWidth, mHeight);
    }

    RECT windowRect;
    if (!GetClientRect(getWindowHandle(), &windowRect))
    {
        ASSERT(false);

        ERR("Could not retrieve the window dimensions");
        return false;
    }

    return resetSwapChain(windowRect.right - windowRect.left, windowRect.bottom - windowRect.top);
}

bool Surface::resetSwapChain(int backbufferWidth, int backbufferHeight)
{
    IDirect3DDevice9 *device = mDisplay->getDevice();

    if (device == NULL)
    {
        return false;
    }

    // Evict all non-render target textures to system memory and release all resources
    // before reallocating them to free up as much video memory as possible.
    device->EvictManagedResources();

    D3DPRESENT_PARAMETERS presentParameters = {0};
    HRESULT result;

    presentParameters.AutoDepthStencilFormat = mConfig->mDepthStencilFormat;
    // We set BackBufferCount = 1 even when we use D3DSWAPEFFECT_FLIPEX.
    // We do this because DirectX docs are a bit vague whether to set this to 1
    // or 2. The runtime seems to accept 1, so we speculate that either it is
    // forcing it to 2 without telling us, or better, doing something smart
    // behind the scenes knowing that we don't need more.
    presentParameters.BackBufferCount = 1;
    presentParameters.BackBufferFormat = mConfig->mRenderTargetFormat;
    presentParameters.EnableAutoDepthStencil = FALSE;
    presentParameters.Flags = 0;
    presentParameters.hDeviceWindow = getWindowHandle();
    presentParameters.MultiSampleQuality = 0;                  // FIXME: Unimplemented
    presentParameters.MultiSampleType = D3DMULTISAMPLE_NONE;   // FIXME: Unimplemented
    presentParameters.PresentationInterval = mPresentInterval;
    presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    presentParameters.Windowed = TRUE;
    presentParameters.BackBufferWidth = backbufferWidth;
    presentParameters.BackBufferHeight = backbufferHeight;

    // Release specific resources to free up memory for the new render target, while the
    // old render target still exists for the purpose of preserving its contents.
    if (mSwapChain)
    {
        mSwapChain->Release();
        mSwapChain = NULL;
    }

    if (mBackBuffer)
    {
        mBackBuffer->Release();
        mBackBuffer = NULL;
    }

    if (mOffscreenTexture)
    {
        mOffscreenTexture->Release();
        mOffscreenTexture = NULL;
    }

    if (mDepthStencil)
    {
        mDepthStencil->Release();
        mDepthStencil = NULL;
    }

    mShareHandle = NULL;
    HANDLE *pShareHandle = NULL;
    if (!mWindow && mDisplay->shareHandleSupported())
    {
        pShareHandle = &mShareHandle;
    }

    result = device->CreateTexture(presentParameters.BackBufferWidth, presentParameters.BackBufferHeight, 1, D3DUSAGE_RENDERTARGET,
                                   presentParameters.BackBufferFormat, D3DPOOL_DEFAULT, &mOffscreenTexture, pShareHandle);
    if (FAILED(result))
    {
        ERR("Could not create offscreen texture: %08lX", result);
        release();

        if(isDeviceLostError(result))
        {
            mDisplay->notifyDeviceLost();
            return false;
        }
        else
        {
            return error(EGL_BAD_ALLOC, false);
        }
    }

    IDirect3DSurface9 *oldRenderTarget = mRenderTarget;

    result = mOffscreenTexture->GetSurfaceLevel(0, &mRenderTarget);
    ASSERT(SUCCEEDED(result));

    if (oldRenderTarget)
    {
        RECT rect =
        {
            0, 0,
            mWidth, mHeight
        };

        if (rect.right > static_cast<LONG>(presentParameters.BackBufferWidth))
        {
            rect.right = presentParameters.BackBufferWidth;
        }

        if (rect.bottom > static_cast<LONG>(presentParameters.BackBufferHeight))
        {
            rect.bottom = presentParameters.BackBufferHeight;
        }

        mDisplay->endScene();

        result = device->StretchRect(oldRenderTarget, &rect, mRenderTarget, &rect, D3DTEXF_NONE);
        ASSERT(SUCCEEDED(result));

        oldRenderTarget->Release();
    }

    if (mWindow)
    {
        result = device->CreateAdditionalSwapChain(&presentParameters, &mSwapChain);

        if (FAILED(result))
        {
            ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY || result == D3DERR_INVALIDCALL || result == D3DERR_DEVICELOST);

            ERR("Could not create additional swap chains or offscreen surfaces: %08lX", result);
            release();

            if(isDeviceLostError(result))
            {
                mDisplay->notifyDeviceLost();
                return false;
            }
            else
            {
                return error(EGL_BAD_ALLOC, false);
            }
        }

        result = mSwapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &mBackBuffer);
        ASSERT(SUCCEEDED(result));
    }

    if (mConfig->mDepthStencilFormat != D3DFMT_UNKNOWN)
    {
        result = device->CreateDepthStencilSurface(presentParameters.BackBufferWidth, presentParameters.BackBufferHeight,
                                                   presentParameters.AutoDepthStencilFormat, presentParameters.MultiSampleType,
                                                   presentParameters.MultiSampleQuality, FALSE, &mDepthStencil, NULL);

        if (FAILED(result))
        {
            ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY || result == D3DERR_INVALIDCALL);

            ERR("Could not create depthstencil surface for new swap chain: 0x%08X", result);
            release();

            if(isDeviceLostError(result))
            {
                mDisplay->notifyDeviceLost();
                return false;
            }
            else
            {
                return error(EGL_BAD_ALLOC, false);
            }
        }
    }

    mWidth = presentParameters.BackBufferWidth;
    mHeight = presentParameters.BackBufferHeight;

    mPresentIntervalDirty = false;
    return true;
}

bool Surface::swapRect(EGLint x, EGLint y, EGLint width, EGLint height)
{
    if (!mSwapChain)
    {
        return true;
    }

    if (x + width > mWidth)
    {
        width = mWidth - x;
    }

    if (y + height > mHeight)
    {
        height = mHeight - y;
    }

    if (width == 0 || height == 0)
    {
        return true;
    }

    IDirect3DDevice9 *device = mDisplay->getDevice();

    // Disable all pipeline operations
    device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
    device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_RED);
    device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    device->SetPixelShader(NULL);
    device->SetVertexShader(NULL);

    device->SetRenderTarget(0, mBackBuffer);
    device->SetDepthStencilSurface(NULL);

    device->SetTexture(0, mOffscreenTexture);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);

    D3DVIEWPORT9 viewport = {0, 0, mWidth, mHeight, 0.0f, 1.0f};
    device->SetViewport(&viewport);

    float x1 = x - 0.5f;
    float y1 = (mHeight - y - height) - 0.5f;
    float x2 = (x + width) - 0.5f;
    float y2 = (mHeight - y) - 0.5f;

    float u1 = x / float(mWidth);
    float v1 = y / float(mHeight);
    float u2 = (x + width) / float(mWidth);
    float v2 = (y + height) / float(mHeight);

    float quad[4][6] = {{x1, y1, 0.0f, 1.0f, u1, v2},
                        {x2, y1, 0.0f, 1.0f, u2, v2},
                        {x2, y2, 0.0f, 1.0f, u2, v1},
                        {x1, y2, 0.0f, 1.0f, u1, v1}};   // x, y, z, rhw, u, v

    mDisplay->startScene();
    device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, quad, 6 * sizeof(float));
    mDisplay->endScene();

    device->SetTexture(0, NULL);

    RECT rect =
    {
        x, mHeight - y - height,
        x + width, mHeight - y
    };

    HRESULT result = mSwapChain->Present(&rect, &rect, NULL, NULL, 0);

    gl::Context *context = static_cast<gl::Context*>(glGetCurrentContext());
    if (context)
    {
        context->markAllStateDirty();
    }

    if (isDeviceLostError(result))
    {
        mDisplay->notifyDeviceLost();
        return false;
    }

    if (result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY || result == D3DERR_DRIVERINTERNALERROR)
    {
        return error(EGL_BAD_ALLOC, false);
    }

    ASSERT(SUCCEEDED(result));

    checkForOutOfDateSwapChain();

    return true;
}

HWND Surface::getWindowHandle()
{
    return mWindow;
}


#define kSurfaceProperty _TEXT("Egl::SurfaceOwner")
#define kParentWndProc _TEXT("Egl::SurfaceParentWndProc")

static LRESULT CALLBACK SurfaceWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
  if (message == WM_SIZE)
  {
      Surface* surf = reinterpret_cast<Surface*>(GetProp(hwnd, kSurfaceProperty));
      if(surf)
      {
          surf->checkForOutOfDateSwapChain();
      }
  }
  WNDPROC prevWndFunc = reinterpret_cast<WNDPROC >(GetProp(hwnd, kParentWndProc));
  return CallWindowProc(prevWndFunc, hwnd, message, wparam, lparam);
}

void Surface::subclassWindow()
{
    if (!mWindow)
    {
        return;
    }

    DWORD processId;
    DWORD threadId = GetWindowThreadProcessId(mWindow, &processId);
    if (processId != GetCurrentProcessId() || threadId != GetCurrentThreadId())
    {
        return;
    }

    SetLastError(0);
    LONG_PTR oldWndProc = SetWindowLongPtr(mWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(SurfaceWindowProc));
    if(oldWndProc == 0 && GetLastError() != ERROR_SUCCESS)
    {
        mWindowSubclassed = false;
        return;
    }

    SetProp(mWindow, kSurfaceProperty, reinterpret_cast<HANDLE>(this));
    SetProp(mWindow, kParentWndProc, reinterpret_cast<HANDLE>(oldWndProc));
    mWindowSubclassed = true;
}

void Surface::unsubclassWindow()
{
    if(!mWindowSubclassed)
    {
        return;
    }

    // un-subclass
    LONG_PTR parentWndFunc = reinterpret_cast<LONG_PTR>(GetProp(mWindow, kParentWndProc));

    // Check the windowproc is still SurfaceWindowProc.
    // If this assert fails, then it is likely the application has subclassed the
    // hwnd as well and did not unsubclass before destroying its EGL context. The
    // application should be modified to either subclass before initializing the
    // EGL context, or to unsubclass before destroying the EGL context.
    if(parentWndFunc)
    {
        LONG_PTR prevWndFunc = SetWindowLongPtr(mWindow, GWLP_WNDPROC, parentWndFunc);
        ASSERT(prevWndFunc == reinterpret_cast<LONG_PTR>(SurfaceWindowProc));
    }

    RemoveProp(mWindow, kSurfaceProperty);
    RemoveProp(mWindow, kParentWndProc);
    mWindowSubclassed = false;
}

bool Surface::checkForOutOfDateSwapChain()
{
    RECT client;
    if (!GetClientRect(getWindowHandle(), &client))
    {
        ASSERT(false);
        return false;
    }

    // Grow the buffer now, if the window has grown. We need to grow now to avoid losing information.
    int clientWidth = client.right - client.left;
    int clientHeight = client.bottom - client.top;
    bool sizeDirty = clientWidth != getWidth() || clientHeight != getHeight();

    if (sizeDirty || mPresentIntervalDirty)
    {
        resetSwapChain(clientWidth, clientHeight);
        if (static_cast<egl::Surface*>(getCurrentDrawSurface()) == this)
        {
            glMakeCurrent(glGetCurrentContext(), static_cast<egl::Display*>(getCurrentDisplay()), this);
        }

        return true;
    }
    return false;
}

DWORD Surface::convertInterval(EGLint interval)
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

bool Surface::swap()
{
    return swapRect(0, 0, mWidth, mHeight);
}

bool Surface::postSubBuffer(EGLint x, EGLint y, EGLint width, EGLint height)
{
    if (!mPostSubBufferSupported)
    {
        // Spec is not clear about how this should be handled.
        return true;
    }
    
    return swapRect(x, y, width, height);
}

EGLint Surface::getWidth() const
{
    return mWidth;
}

EGLint Surface::getHeight() const
{
    return mHeight;
}

EGLint Surface::isPostSubBufferSupported() const
{
    return mPostSubBufferSupported;
}

// Increments refcount on surface.
// caller must Release() the returned surface
IDirect3DSurface9 *Surface::getRenderTarget()
{
    if (mRenderTarget)
    {
        mRenderTarget->AddRef();
    }

    return mRenderTarget;
}

// Increments refcount on surface.
// caller must Release() the returned surface
IDirect3DSurface9 *Surface::getDepthStencil()
{
    if (mDepthStencil)
    {
        mDepthStencil->AddRef();
    }

    return mDepthStencil;
}

IDirect3DTexture9 *Surface::getOffscreenTexture()
{
    if (mOffscreenTexture)
    {
        mOffscreenTexture->AddRef();
    }

    return mOffscreenTexture;
}

void Surface::setSwapInterval(EGLint interval)
{
    if (mSwapInterval == interval)
    {
        return;
    }
    
    mSwapInterval = interval;
    mSwapInterval = std::max(mSwapInterval, mDisplay->getMinSwapInterval());
    mSwapInterval = std::min(mSwapInterval, mDisplay->getMaxSwapInterval());

    mPresentInterval = convertInterval(mSwapInterval);
    mPresentIntervalDirty = true;
}

EGLenum Surface::getTextureFormat() const
{
    return mTextureFormat;
}

EGLenum Surface::getTextureTarget() const
{
    return mTextureTarget;
}

void Surface::setBoundTexture(gl::Texture2D *texture)
{
    mTexture = texture;
}

gl::Texture2D *Surface::getBoundTexture() const
{
    return mTexture;
}

D3DFORMAT Surface::getFormat() const
{
    return mConfig->mRenderTargetFormat;
}
}
