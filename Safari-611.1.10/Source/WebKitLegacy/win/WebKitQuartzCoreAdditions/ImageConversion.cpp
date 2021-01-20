/*
* Copyright (C) 2017 Apple Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
* THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "ImageConversion.h"

#include "Image.h"
#include <d3d9.h>
#include <wtf/RefPtr.h>
#include <wtf/win/GDIObject.h>

namespace WKQCA {

static HRESULT getBackBufferRectAsRenderTarget(IDirect3DDevice9* device, IDirect3DSwapChain9* swapChain, const CRect& rect, D3DFORMAT format, CComPtr<IDirect3DSurface9>& outRenderTarget)
{
    CComPtr<IDirect3DSurface9> backBuffer;
    HRESULT hr = swapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
    if (FAILED(hr))
        return hr;

    CComPtr<IDirect3DSurface9> renderTarget;
    hr = device->CreateRenderTarget(rect.Width(), rect.Height(), format, D3DMULTISAMPLE_NONE, 0, FALSE, &renderTarget, 0);
    if (FAILED(hr))
        return hr;

    hr = device->StretchRect(backBuffer, &rect, renderTarget, 0, D3DTEXF_NONE);
    if (FAILED(hr))
        return hr;

    outRenderTarget.Attach(renderTarget.Detach());
    return S_OK;
}

static HRESULT getSystemMemoryCopy(IDirect3DDevice9* device, IDirect3DSurface9* renderTarget, CComPtr<IDirect3DSurface9>& outSystemMemorySurface)
{
    D3DSURFACE_DESC desc;
    HRESULT hr = renderTarget->GetDesc(&desc);
    if (FAILED(hr))
        return hr;

    CComPtr<IDirect3DSurface9> systemMemorySurface;
    hr = device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &systemMemorySurface, 0);
    if (FAILED(hr))
        return hr;

    hr = device->GetRenderTargetData(renderTarget, systemMemorySurface);
    if (FAILED(hr))
        return hr;

    outSystemMemorySurface.Attach(systemMemorySurface.Detach());
    return S_OK;
}

static bool copyRectToBitmap(HDC sourceDC, const CRect& rect, HBITMAP destinationBitmap)
{
    auto bitmapDC = adoptGDIObject(::CreateCompatibleDC(sourceDC));
    if (!bitmapDC)
        return false;

    HBITMAP oldBitmap = static_cast<HBITMAP>(::SelectObject(bitmapDC.get(), destinationBitmap));
    if (!oldBitmap)
        return false;

    if (!::BitBlt(bitmapDC.get(), 0, 0, rect.Width(), rect.Height(), sourceDC, rect.left, rect.top, SRCCOPY))
        return false;

    return ::SelectObject(bitmapDC.get(), oldBitmap);
}

static HRESULT getImageCopy(IDirect3DSurface9* systemMemorySurface, RefPtr<Image>& outImage)
{
    D3DSURFACE_DESC desc;
    HRESULT hr = systemMemorySurface->GetDesc(&desc);
    if (FAILED(hr))
        return hr;

    CSize surfaceSize(desc.Width, desc.Height);

    auto image = Image::create(surfaceSize);
    if (!image)
        return E_FAIL;

    auto bitmap = image->createDIB();

    HDC surfaceDC;
    hr = systemMemorySurface->GetDC(&surfaceDC);
    if (FAILED(hr))
        return hr;

    bool success = copyRectToBitmap(surfaceDC, CRect(CPoint(0, 0), surfaceSize), bitmap.get());

    hr = systemMemorySurface->ReleaseDC(surfaceDC);
    if (FAILED(hr))
        return hr;

    if (!success)
        return E_FAIL;

    outImage = WTFMove(image);
    return S_OK;
}

HRESULT getBackBufferRectAsImage(IDirect3DDevice9* device, IDirect3DSwapChain9* swapChain, const CRect& rect, RefPtr<Image>& outImage)
{
    // We have to read the back buffer's bits off the GPU and into system memory. But copying from
    // the GPU to system memory is slow, so we want to copy as few bits as possible. Direct3D only
    // allows an entire surface to be read into system memory, not just a subrect, so we first copy
    // the subrect we care about to a second, smaller surface, then read that second surface into
    // system memory.

    // We explicitly specify D3DFMT_X8R8G8B8 to match the format of Image's bits. This should cause
    // any color conversion to happen on the GPU before we read the bits back into system memory.
    CComPtr<IDirect3DSurface9> renderTarget;
    HRESULT hr = getBackBufferRectAsRenderTarget(device, swapChain, rect, D3DFMT_X8R8G8B8, renderTarget);
    if (FAILED(hr))
        return hr;

    CComPtr<IDirect3DSurface9> systemMemorySurface;
    hr = getSystemMemoryCopy(device, renderTarget, systemMemorySurface);
    if (FAILED(hr))
        return hr;

    return getImageCopy(systemMemorySurface, outImage);
}

} // namespace WKQCA
