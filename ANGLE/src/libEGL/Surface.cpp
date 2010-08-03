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
Surface::Surface(Display *display, IDirect3DSwapChain9 *swapChain, IDirect3DSurface9 *depthStencil, const Config *config) 
    : mDisplay(display), mSwapChain(swapChain), mDepthStencil(depthStencil), mConfig(config)
{
    mBackBuffer = NULL;
    mRenderTarget = NULL;
    mFlipTexture = NULL;
    mFlipState = NULL;
    mPreFlipState = NULL;

    mPixelAspectRatio = (EGLint)(1.0 * EGL_DISPLAY_SCALING);   // FIXME: Determine actual pixel aspect ratio
    mRenderBuffer = EGL_BACK_BUFFER;
    mSwapBehavior = EGL_BUFFER_PRESERVED;

    if (mSwapChain)
    {
        mSwapChain->AddRef();
        mSwapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &mBackBuffer);

        D3DSURFACE_DESC description;
        mBackBuffer->GetDesc(&description);

        mWidth = description.Width;
        mHeight = description.Height;

        IDirect3DDevice9 *device = display->getDevice();
        HRESULT result = device->CreateRenderTarget(mWidth, mHeight, description.Format, description.MultiSampleType, description.MultiSampleQuality, FALSE, &mRenderTarget, NULL);

        if (result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY)
        {
            error(EGL_BAD_ALLOC);

            return;
        }

        ASSERT(SUCCEEDED(result));

        result = device->CreateTexture(mWidth, mHeight, 1, D3DUSAGE_RENDERTARGET, description.Format, D3DPOOL_DEFAULT, &mFlipTexture, NULL);

        if (FAILED(result))
        {
            ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);

            error(EGL_BAD_ALLOC);

            mRenderTarget->Release();

            return;
        }
    }
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

HWND Surface::getWindowHandle()
{
    if (mSwapChain)
    {
        D3DPRESENT_PARAMETERS presentParameters;
        mSwapChain->GetPresentParameters(&presentParameters);

        return presentParameters.hDeviceWindow;
    }

    return NULL;
}

void Surface::writeRecordableFlipState(IDirect3DDevice9 *device, IDirect3DTexture9 *source)
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
    device->SetTexture(0, source);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);
    device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);

    device->SetStreamSourceFreq(0, 1); // DrawPrimitiveUP only cares about stream 0, not the rest.
}

void Surface::applyFlipState(IDirect3DDevice9 *device, IDirect3DTexture9 *source)
{
    HRESULT hr;

    if (mFlipState == NULL)
    {
        // Create two state blocks both recording the states that are changed when swapping.

        // mPreFlipState will record the original state each entry.
        hr = device->BeginStateBlock();
        ASSERT(SUCCEEDED(hr));
        writeRecordableFlipState(device, source);
        hr = device->EndStateBlock(&mPreFlipState);
        ASSERT(SUCCEEDED(hr) || hr == D3DERR_OUTOFVIDEOMEMORY || hr == E_OUTOFMEMORY);

        if (SUCCEEDED(hr))
        {
            mPreFlipState->Capture();
        }

        // mFlipState will record the state for the swap operation.
        hr = device->BeginStateBlock();
        ASSERT(SUCCEEDED(hr));

        writeRecordableFlipState(device, source);

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

bool Surface::swap()
{
    if (mSwapChain)
    {
        IDirect3DDevice9 *device = mDisplay->getDevice();

        IDirect3DSurface9 *textureSurface;
        mFlipTexture->GetSurfaceLevel(0, &textureSurface);

        mDisplay->endScene();
        device->StretchRect(mRenderTarget, NULL, textureSurface, NULL, D3DTEXF_NONE);

        applyFlipState(device, mFlipTexture);

        // Render the texture upside down into the back buffer
        float quad[4][6] = {{     0 - 0.5f,       0 - 0.5f, 0.0f, 1.0f, 0.0f, 1.0f},
                            {mWidth - 0.5f,       0 - 0.5f, 0.0f, 1.0f, 1.0f, 1.0f},
                            {mWidth - 0.5f, mHeight - 0.5f, 0.0f, 1.0f, 1.0f, 0.0f},
                            {     0 - 0.5f, mHeight - 0.5f, 0.0f, 1.0f, 0.0f, 0.0f}};   // x, y, z, rhw, u, v

        mDisplay->startScene();
        device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, quad, 6 * sizeof(float));

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
