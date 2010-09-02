//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Surface.cpp: Implements the egl::Surface class, representing a drawing surface
// such as the client area of a window, including any back buffers.
// Implements EGLSurface and related functionality. [EGL 1.4] section 2.2 page 3.

#include "libEGL/Surface.h"

#include "common/debug.h"

#include "libEGL/main.h"
#include "libEGL/Display.h"

namespace egl
{
Surface::Surface(Display *display, const Config *config, HWND window) 
    : mDisplay(display), mConfig(config), mWindow(window)
{
    mSwapChain = NULL;
    mDepthStencil = NULL;
    mBackBuffer = NULL;
    mRenderTarget = NULL;
    mFlipTexture = NULL;
    mFlipState = NULL;
    mPreFlipState = NULL;

    mPixelAspectRatio = (EGLint)(1.0 * EGL_DISPLAY_SCALING);   // FIXME: Determine actual pixel aspect ratio
    mRenderBuffer = EGL_BACK_BUFFER;
    mSwapBehavior = EGL_BUFFER_PRESERVED;

    resetSwapChain();
}

Surface::~Surface()
{
    if (mSwapChain)
    {
        mSwapChain->Release();
    }

    if (mBackBuffer)
    {
        mBackBuffer->Release();
    }

    if (mRenderTarget)
    {
        mRenderTarget->Release();
    }

    if (mDepthStencil)
    {
        mDepthStencil->Release();
    }

    if (mFlipTexture)
    {
        mFlipTexture->Release();
    }

    if (mFlipState)
    {
        mFlipState->Release();
    }

    if (mPreFlipState)
    {
        mPreFlipState->Release();
    }
}

void Surface::resetSwapChain()
{
    IDirect3DDevice9 *device = mDisplay->getDevice();

    D3DPRESENT_PARAMETERS presentParameters = {0};

    presentParameters.AutoDepthStencilFormat = mConfig->mDepthStencilFormat;
    presentParameters.BackBufferCount = 1;
    presentParameters.BackBufferFormat = mConfig->mRenderTargetFormat;
    presentParameters.EnableAutoDepthStencil = FALSE;
    presentParameters.Flags = 0;
    presentParameters.hDeviceWindow = getWindowHandle();
    presentParameters.MultiSampleQuality = 0;                  // FIXME: Unimplemented
    presentParameters.MultiSampleType = D3DMULTISAMPLE_NONE;   // FIXME: Unimplemented
    presentParameters.PresentationInterval = Display::convertInterval(mConfig->mMinSwapInterval);
    presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    presentParameters.Windowed = TRUE;

    RECT windowRect;
    if (!GetClientRect(getWindowHandle(), &windowRect))
    {
        ASSERT(false);
        return;
    }

    presentParameters.BackBufferWidth = windowRect.right - windowRect.left;
    presentParameters.BackBufferHeight = windowRect.bottom - windowRect.top;

    IDirect3DSwapChain9 *swapChain = NULL;
    HRESULT result = device->CreateAdditionalSwapChain(&presentParameters, &swapChain);

    if (FAILED(result))
    {
        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);

        ERR("Could not create additional swap chains: %08lX", result);
        return error(EGL_BAD_ALLOC);
    }

    IDirect3DSurface9 *depthStencilSurface = NULL;
    result = device->CreateDepthStencilSurface(presentParameters.BackBufferWidth, presentParameters.BackBufferHeight,
                                               presentParameters.AutoDepthStencilFormat, presentParameters.MultiSampleType,
                                               presentParameters.MultiSampleQuality, FALSE, &depthStencilSurface, NULL);

    if (FAILED(result))
    {
        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);

        swapChain->Release();

        ERR("Could not create depthstencil surface for new swap chain: %08lX", result);
        return error(EGL_BAD_ALLOC);
    }

    IDirect3DSurface9 *renderTarget = NULL;
    result = device->CreateRenderTarget(presentParameters.BackBufferWidth, presentParameters.BackBufferHeight, presentParameters.BackBufferFormat,
                                        presentParameters.MultiSampleType, presentParameters.MultiSampleQuality, FALSE, &renderTarget, NULL);

    if (FAILED(result))
    {
        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);

        swapChain->Release();
        depthStencilSurface->Release();

        ERR("Could not create render target surface for new swap chain: %08lX", result);
        return error(EGL_BAD_ALLOC);
    }

    ASSERT(SUCCEEDED(result));

    IDirect3DTexture9 *flipTexture = NULL;
    result = device->CreateTexture(presentParameters.BackBufferWidth, presentParameters.BackBufferHeight, 1, D3DUSAGE_RENDERTARGET,
                                   presentParameters.BackBufferFormat, D3DPOOL_DEFAULT, &flipTexture, NULL);

    if (FAILED(result))
    {
        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);

        swapChain->Release();
        depthStencilSurface->Release();
        renderTarget->Release();

        ERR("Could not create flip texture for new swap chain: %08lX", result);
        return error(EGL_BAD_ALLOC);
    }

    IDirect3DSurface9 *backBuffer = NULL;
    swapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);

    if (mSwapChain) mSwapChain->Release();
    if (mDepthStencil) mDepthStencil->Release();
    if (mBackBuffer) mBackBuffer->Release();
    if (mRenderTarget) mRenderTarget->Release();
    if (mFlipTexture) mFlipTexture->Release();

    mWidth = presentParameters.BackBufferWidth;
    mHeight = presentParameters.BackBufferHeight;

    mSwapChain = swapChain;
    mDepthStencil = depthStencilSurface;
    mBackBuffer = backBuffer;
    mRenderTarget = renderTarget;
    mFlipTexture = flipTexture;

    // The flip state block recorded mFlipTexture so it is now invalid.
    releaseRecordedState(device);
}

HWND Surface::getWindowHandle()
{
    return mWindow;
}

void Surface::writeRecordableFlipState(IDirect3DDevice9 *device)
{
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
    device->SetPixelShader(NULL);
    device->SetVertexShader(NULL);

    // Just sample the texture
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    device->SetTexture(0, NULL); // The actual texture will change after resizing. But the pre-flip state block must save/restore the texture.
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);
    device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);

    device->SetStreamSourceFreq(0, 1); // DrawPrimitiveUP only cares about stream 0, not the rest.
}

void Surface::applyFlipState(IDirect3DDevice9 *device)
{
    HRESULT hr;

    if (mFlipState == NULL)
    {
        // Create two state blocks both recording the states that are changed when swapping.

        // mPreFlipState will record the original state each entry.
        hr = device->BeginStateBlock();
        ASSERT(SUCCEEDED(hr));
        writeRecordableFlipState(device);
        hr = device->EndStateBlock(&mPreFlipState);
        ASSERT(SUCCEEDED(hr) || hr == D3DERR_OUTOFVIDEOMEMORY || hr == E_OUTOFMEMORY);

        if (SUCCEEDED(hr))
        {
            mPreFlipState->Capture();
        }

        // mFlipState will record the state for the swap operation.
        hr = device->BeginStateBlock();
        ASSERT(SUCCEEDED(hr));

        writeRecordableFlipState(device);

        hr = device->EndStateBlock(&mFlipState);
        ASSERT(SUCCEEDED(hr) || hr == D3DERR_OUTOFVIDEOMEMORY || hr == E_OUTOFMEMORY);

        if (FAILED(hr))
        {
            mFlipState = NULL;
            mPreFlipState->Release();
            mPreFlipState = NULL;
        }
        else
        {
            hr = mFlipState->Apply();
            ASSERT(SUCCEEDED(hr));
        }
    }
    else
    {
        hr = mPreFlipState->Capture();
        ASSERT(SUCCEEDED(hr));
        hr = mFlipState->Apply();
        ASSERT(SUCCEEDED(hr));
    }

    device->GetRenderTarget(0, &mPreFlipBackBuffer);
    device->GetDepthStencilSurface(&mPreFlipDepthStencil);

    device->SetRenderTarget(0, mBackBuffer);
    device->SetDepthStencilSurface(NULL);
}

void Surface::restoreState(IDirect3DDevice9 *device)
{
    mPreFlipState->Apply();

    device->SetRenderTarget(0, mPreFlipBackBuffer);
    device->SetDepthStencilSurface(mPreFlipDepthStencil);

    if (mPreFlipBackBuffer)
    {
        mPreFlipBackBuffer->Release();
        mPreFlipBackBuffer = NULL;
    }

    if (mPreFlipDepthStencil)
    {
        mPreFlipDepthStencil->Release();
        mPreFlipDepthStencil = NULL;
    }
}

// On the next flip, this will cause the state to be recorded from scratch.
// In particular we need to do this if the flip texture changes.
void Surface::releaseRecordedState(IDirect3DDevice9 *device)
{
    if (mFlipState)
    {
        mFlipState->Release();
        mFlipState = NULL;
    }

    if (mPreFlipState)
    {
        mPreFlipState->Release();
        mPreFlipState = NULL;
    }
}

bool Surface::checkForWindowResize()
{
    RECT client;
    if (!GetClientRect(getWindowHandle(), &client))
    {
        ASSERT(false);
        return false;
    }

    if (getWidth() != client.right - client.left || getHeight() != client.bottom - client.top)
    {
        resetSwapChain();

        if (static_cast<egl::Surface*>(getCurrentDrawSurface()) == this)
        {
            glMakeCurrent(glGetCurrentContext(), static_cast<egl::Display*>(getCurrentDisplay()), this);
        }

        return true;
    }

    return false;
}

bool Surface::swap()
{
    if (mSwapChain)
    {
        IDirect3DTexture9 *flipTexture = mFlipTexture;
        flipTexture->AddRef();

        IDirect3DSurface9 *renderTarget = mRenderTarget;
        renderTarget->AddRef();

        EGLint oldWidth = mWidth;
        EGLint oldHeight = mHeight;

        checkForWindowResize();

        IDirect3DDevice9 *device = mDisplay->getDevice();

        IDirect3DSurface9 *textureSurface;
        flipTexture->GetSurfaceLevel(0, &textureSurface);

        mDisplay->endScene();
        device->StretchRect(renderTarget, NULL, textureSurface, NULL, D3DTEXF_NONE);
        renderTarget->Release();

        applyFlipState(device);
        device->SetTexture(0, flipTexture);

        float xscale = (float)mWidth / oldWidth;
        float yscale = (float)mHeight / oldHeight;

        // Render the texture upside down into the back buffer
        // Texcoords are chosen to pin a potentially resized image into the upper-left corner without scaling.
        float quad[4][6] = {{     0 - 0.5f,       0 - 0.5f, 0.0f, 1.0f, 0.0f,   1.0f       },
                            {mWidth - 0.5f,       0 - 0.5f, 0.0f, 1.0f, xscale, 1.0f       },
                            {mWidth - 0.5f, mHeight - 0.5f, 0.0f, 1.0f, xscale, 1.0f-yscale},
                            {     0 - 0.5f, mHeight - 0.5f, 0.0f, 1.0f, 0.0f,   1.0f-yscale}};   // x, y, z, rhw, u, v

        mDisplay->startScene();
        device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, quad, 6 * sizeof(float));

        flipTexture->Release();
        textureSurface->Release();

        restoreState(device);

        mDisplay->endScene();
        HRESULT result = mSwapChain->Present(NULL, NULL, NULL, NULL, mDisplay->getPresentInterval());

        if (result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY || result == D3DERR_DRIVERINTERNALERROR)
        {
            return error(EGL_BAD_ALLOC, false);
        }

        if (result == D3DERR_DEVICELOST)
        {
            return error(EGL_CONTEXT_LOST, false);
        }

        ASSERT(SUCCEEDED(result));

    }

    return true;
}

EGLint Surface::getWidth() const
{
    return mWidth;
}

EGLint Surface::getHeight() const
{
    return mHeight;
}

IDirect3DSurface9 *Surface::getRenderTarget()
{
    if (mRenderTarget)
    {
        mRenderTarget->AddRef();
    }

    return mRenderTarget;
}

IDirect3DSurface9 *Surface::getDepthStencil()
{
    if (mDepthStencil)
    {
        mDepthStencil->AddRef();
    }

    return mDepthStencil;
}
}
