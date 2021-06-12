/*
 * Copyright (C) 2019 Apple Inc.  All rights reserved.
 * Copyright (C) 2010 Igalia S.L.
 * Copyright (C) 2011 ProFUSION embedded systems
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Direct2DUtilities.h"

#if USE(DIRECT2D)

#include "COMPtr.h"
#include "FloatPoint.h"
#include "FloatSize.h"
#include "GraphicsContext.h"
#include "ImageDecoderDirect2D.h"
#include "IntRect.h"
#include "IntSize.h"
#include <d2d1_1.h>
#include <d3d11_1.h>
#include <shlwapi.h>
#include <wincodec.h>


namespace WebCore {

namespace Direct2D {

constexpr DXGI_FORMAT webkitTextureFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
constexpr D2D1_ALPHA_MODE webkitAlphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;

IntSize bitmapSize(IWICBitmapSource* bitmapSource)
{
    UINT width, height;
    HRESULT hr = bitmapSource->GetSize(&width, &height);
    if (!SUCCEEDED(hr))
        return { };

    return IntSize(width, height);
}

FloatSize bitmapSize(ID2D1Bitmap* bitmapSource)
{
    return bitmapSource->GetSize();
}

FloatSize bitmapResolution(IWICBitmapSource* bitmapSource)
{
    constexpr double dpiBase = 96;

    double dpiX, dpiY;
    HRESULT hr = bitmapSource->GetResolution(&dpiX, &dpiY);
    if (!SUCCEEDED(hr))
        return { };

    FloatSize result(dpiX, dpiY);
    result.scale(1.0 / dpiBase);
    return result;
}

FloatSize bitmapResolution(ID2D1Bitmap* bitmap)
{
    constexpr double dpiBase = 96;

    float dpiX, dpiY;
    bitmap->GetDpi(&dpiX, &dpiY);

    FloatSize result(dpiX, dpiY);
    result.scale(1.0 / dpiBase);
    return result;

}

FloatSize bitmapResolution(ID2D1RenderTarget* target)
{
    constexpr double dpiBase = 96;

    float dpiX, dpiY;
    target->GetDpi(&dpiX, &dpiY);

    FloatSize result(dpiX, dpiY);
    result.scale(1.0 / dpiBase);
    return result;

}
unsigned bitsPerPixel(GUID bitmapFormat)
{
    COMPtr<IWICComponentInfo> componentInfo;
    HRESULT hr = ImageDecoderDirect2D::systemImagingFactory()->CreateComponentInfo(bitmapFormat, &componentInfo);
    if (!SUCCEEDED(hr))
        return 4;

    COMPtr<IWICPixelFormatInfo> pixelFormat;
    pixelFormat.query(componentInfo);
    if (!pixelFormat)
        return 4;

    UINT bpp = 0;
    hr = pixelFormat->GetBitsPerPixel(&bpp);
    if (!SUCCEEDED(hr))
        return 4;

    return bpp;
}

COMPtr<IWICBitmap> createDirect2DImageSurfaceWithData(void* data, const IntSize& size, unsigned stride)
{
    CheckedSize numBytes = CheckedUint32(size.height()) * stride;
    if (numBytes.hasOverflowed())
        return nullptr;

    COMPtr<IWICBitmap> surface;
    HRESULT hr = ImageDecoderDirect2D::systemImagingFactory()->CreateBitmapFromMemory(size.width(), size.height(), wicBitmapFormat(), stride, static_cast<UINT>(numBytes.value()), reinterpret_cast<BYTE*>(data), &surface);
    if (!SUCCEEDED(hr))
        return nullptr;

    return surface;
}

COMPtr<IWICBitmap> createWicBitmap(const IntSize& size)
{
    COMPtr<IWICBitmap> surface;
    HRESULT hr = ImageDecoderDirect2D::systemImagingFactory()->CreateBitmap(size.width(), size.height(), wicBitmapFormat(), WICBitmapCacheOnDemand, &surface);
    if (!SUCCEEDED(hr))
        return nullptr;

    return surface;
}

D2D1_PIXEL_FORMAT pixelFormatForSoftwareManipulation()
{
    return D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
}

D2D1_PIXEL_FORMAT pixelFormat()
{
    // Since we need to interact with HDC from time-to-time, we are forced to use DXGI_FORMAT_B8G8R8A8_UNORM and D2D1_ALPHA_MODE_PREMULTIPLIED
    return D2D1::PixelFormat(webkitTextureFormat, webkitAlphaMode);
}

GUID wicBitmapFormat()
{
    // This is the WIC format compatible with DXGI_FORMAT_B8G8R8A8_UNORM. It is also supposedly the most efficient in-memory
    // representation for WIC images.
    return GUID_WICPixelFormat32bppPBGRA;
}

D2D1_BITMAP_PROPERTIES bitmapProperties()
{
    return D2D1::BitmapProperties(pixelFormat());
}

COMPtr<ID2D1Bitmap> createBitmap(ID2D1RenderTarget* renderTarget, const IntSize& size)
{
    auto bitmapCreateProperties = bitmapProperties();

    COMPtr<ID2D1Bitmap> bitmap;
    D2D1_SIZE_U bitmapSize = size;
    HRESULT hr = renderTarget->CreateBitmap(bitmapSize, bitmapCreateProperties, &bitmap);
    if (!SUCCEEDED(hr))
        return nullptr;

    return bitmap;
}

D2D1_RENDER_TARGET_PROPERTIES renderTargetProperties()
{
    return D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
        pixelFormat(), 0, 0, D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE, D2D1_FEATURE_LEVEL_DEFAULT);
}

COMPtr<ID2D1RenderTarget> createRenderTargetFromWICBitmap(IWICBitmap* bitmapSource)
{
    auto targetProperties = renderTargetProperties();

    COMPtr<ID2D1RenderTarget> bitmapContext;
    HRESULT hr = GraphicsContext::systemFactory()->CreateWicBitmapRenderTarget(bitmapSource, &targetProperties, &bitmapContext);
    if (!bitmapContext || !SUCCEEDED(hr))
        return nullptr;

    return bitmapContext;
}

COMPtr<ID2D1DCRenderTarget> createGDIRenderTarget()
{
    auto targetProperties = renderTargetProperties();

    COMPtr<ID2D1DCRenderTarget> renderTarget;
    HRESULT hr = GraphicsContext::systemFactory()->CreateDCRenderTarget(&targetProperties, &renderTarget);
    if (!renderTarget || !SUCCEEDED(hr))
        return nullptr;

    return renderTarget;
}

COMPtr<ID2D1BitmapRenderTarget> createBitmapRenderTarget(ID2D1RenderTarget* renderTarget)
{
    if (!renderTarget)
        renderTarget = GraphicsContext::defaultRenderTarget();

    COMPtr<ID2D1BitmapRenderTarget> bitmapContext;
    HRESULT hr = renderTarget->CreateCompatibleRenderTarget(nullptr, nullptr, nullptr, D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_GDI_COMPATIBLE, &bitmapContext);
    if (!SUCCEEDED(hr))
        return nullptr;

    return bitmapContext;
}

COMPtr<ID2D1BitmapRenderTarget> createBitmapRenderTargetOfSize(const IntSize& size, ID2D1RenderTarget* renderTarget, float deviceScaleFactor)
{
    UNUSED_PARAM(deviceScaleFactor);

    if (!renderTarget)
        renderTarget = GraphicsContext::defaultRenderTarget();

    COMPtr<ID2D1BitmapRenderTarget> bitmapContext;
    auto desiredSize = D2D1::SizeF(size.width(), size.height());
    D2D1_SIZE_U pixelSize = D2D1::SizeU(clampTo<unsigned>(deviceScaleFactor * size.width()), clampTo<unsigned>(deviceScaleFactor * size.height()));
    HRESULT hr = renderTarget->CreateCompatibleRenderTarget(&desiredSize, &pixelSize, nullptr, D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_GDI_COMPATIBLE, &bitmapContext);
    if (!SUCCEEDED(hr))
        return nullptr;

    return bitmapContext;
}

COMPtr<IDXGISurface1> createDXGISurfaceOfSize(const IntSize& size, ID3D11Device1* directXDevice, bool crossProcess)
{
    if (!directXDevice)
        directXDevice = Direct2D::defaultDirectXDevice();

    // Create the render target texture
    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = size.width();
    desc.Height = size.height();
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = webkitTextureFormat;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = D3D11_RESOURCE_MISC_GDI_COMPATIBLE;
    if (crossProcess)
        desc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    COMPtr<ID3D11Texture2D> texture;
    HRESULT hr = directXDevice->CreateTexture2D(&desc, nullptr, &texture);
    RELEASE_ASSERT(SUCCEEDED(hr));

    COMPtr<IDXGISurface1> surface;
    hr = texture->QueryInterface(&surface);
    RELEASE_ASSERT(SUCCEEDED(hr));

    return surface;
}

COMPtr<ID2D1RenderTarget> createSurfaceRenderTarget(IDXGISurface1* surface)
{
    auto pixelFormat = D2D1::PixelFormat(webkitTextureFormat, webkitAlphaMode);

    auto properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
        pixelFormat, 0, 0, D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE, D2D1_FEATURE_LEVEL_10);

    COMPtr<ID2D1RenderTarget> renderTarget;
    HRESULT hr = GraphicsContext::systemFactory()->CreateDxgiSurfaceRenderTarget(surface, properties, &renderTarget);
    if (!SUCCEEDED(hr))
        return nullptr;

    return renderTarget;
}

void copyRectFromOneSurfaceToAnother(ID2D1Bitmap* from, ID2D1Bitmap* to, const IntSize& sourceOffset, const IntRect& rect, const IntSize& destOffset)
{
    IntSize sourceBitmapSize = from->GetPixelSize();
    if (sourceBitmapSize.isZero())
        return;

    IntSize targetBitmapSize = to->GetPixelSize();
    if (targetBitmapSize.isZero())
        return;

    IntRect sourceRect(sourceOffset.width(), sourceOffset.height(), rect.width(), rect.height());
    IntRect targetRect(destOffset.width(), destOffset.height(), rect.width(), rect.height());

    IntRect sourceBitmapRect(IntPoint(), sourceBitmapSize);
    IntRect targetBitmapRect(IntPoint(), targetBitmapSize);

    sourceRect.intersect(sourceBitmapRect);
    targetRect.intersect(targetBitmapRect);

    D2D1_RECT_U d2dSourceRect = D2D1::RectU(sourceRect.x(), sourceRect.y(), sourceRect.x() + targetRect.width(), sourceRect.y() + targetRect.height());
    auto offset = D2D1::Point2U(destOffset.width(), destOffset.height());

    HRESULT hr = to->CopyFromBitmap(&offset, from, &d2dSourceRect);
    ASSERT(SUCCEEDED(hr));
}

void writeDiagnosticPNGToPath(ID2D1RenderTarget* renderTarget, ID2D1Bitmap* bitmap, LPCWSTR fileName)
{
    COMPtr<IWICBitmapEncoder> wicBitmapEncoder;
    HRESULT hr = ImageDecoderDirect2D::systemImagingFactory()->CreateEncoder(GUID_ContainerFormatPng, nullptr, &wicBitmapEncoder);
    ASSERT(SUCCEEDED(hr));

    DWORD mode = STGM_CREATE | STGM_READWRITE | STGM_SHARE_DENY_WRITE;
    COMPtr<IStream> stream;
    hr = ::SHCreateStreamOnFileEx(fileName, mode, FILE_ATTRIBUTE_NORMAL, TRUE, nullptr, &stream);
    ASSERT(SUCCEEDED(hr));

    hr = wicBitmapEncoder->Initialize(stream.get(), WICBitmapEncoderNoCache);
    ASSERT(SUCCEEDED(hr));

    COMPtr<IWICBitmapFrameEncode> wicFrameEncode;
    hr = wicBitmapEncoder->CreateNewFrame(&wicFrameEncode, nullptr);
    ASSERT(SUCCEEDED(hr));

    hr = wicFrameEncode->Initialize(nullptr);
    ASSERT(SUCCEEDED(hr));

    COMPtr<ID2D1DeviceContext> d2dDeviceContext;
    hr = renderTarget->QueryInterface(__uuidof(ID2D1DeviceContext), reinterpret_cast<void**>(&d2dDeviceContext));
    ASSERT(SUCCEEDED(hr));

    COMPtr<ID2D1Device> d2dDevice;
    d2dDeviceContext->GetDevice(&d2dDevice);

    COMPtr<IWICImageEncoder> imageEncoder;
    hr = ImageDecoderDirect2D::systemImagingFactory()->CreateImageEncoder(d2dDevice.get(), &imageEncoder);
    ASSERT(SUCCEEDED(hr));

    hr = imageEncoder->WriteFrame(bitmap, wicFrameEncode.get(), nullptr);
    ASSERT(SUCCEEDED(hr));

    hr = wicFrameEncode->Commit();
    ASSERT(SUCCEEDED(hr));

    hr = wicBitmapEncoder->Commit();
    ASSERT(SUCCEEDED(hr));

    hr = stream->Commit(STGC_DEFAULT);
    ASSERT(SUCCEEDED(hr));
}

static ID3D11DeviceContext1* immediateContext = nullptr;

ID3D11DeviceContext1* dxgiImmediateContext()
{
    if (!immediateContext)
        defaultDirectXDevice();

    RELEASE_ASSERT(immediateContext);
    return immediateContext;
}

ID3D11Device1* defaultDirectXDevice()
{
    static ID3D11Device1* defaultDevice1 = nullptr;

    if (!defaultDevice1) {
        int deviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifndef NDEBUG
        deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        ID3D11Device* defaultDevice = nullptr;
        D3D_FEATURE_LEVEL featureLevel = { };
        ID3D11DeviceContext* immediateDeviceContext = nullptr;
        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, deviceFlags, nullptr, 0, D3D11_SDK_VERSION, &defaultDevice, &featureLevel, &immediateDeviceContext);
        RELEASE_ASSERT(SUCCEEDED(hr));

        hr = defaultDevice->QueryInterface(&defaultDevice1);
        RELEASE_ASSERT(SUCCEEDED(hr));
        defaultDevice1->AddRef();

        hr = immediateDeviceContext->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&immediateContext));
        RELEASE_ASSERT(SUCCEEDED(hr));
        immediateContext->AddRef();
    }

    return defaultDevice1;
}

bool createDeviceAndContext(COMPtr<ID3D11Device1>& d3dDevice, COMPtr<ID3D11DeviceContext1>& immediateContext)
{
    int deviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifndef NDEBUG
    deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel = { };
    ID3D11Device* defaultDevice = nullptr;
    ID3D11DeviceContext* immediateDeviceContext = nullptr;
    HRESULT hr = ::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, deviceFlags, nullptr, 0, D3D11_SDK_VERSION, &defaultDevice, &featureLevel, &immediateDeviceContext);
    RELEASE_ASSERT(SUCCEEDED(hr));

    hr = defaultDevice->QueryInterface(&d3dDevice);
    RELEASE_ASSERT(SUCCEEDED(hr));
    defaultDevice->Release();

    hr = immediateDeviceContext->QueryInterface(&immediateContext);
    RELEASE_ASSERT(SUCCEEDED(hr));
    immediateContext->Release();
    return true;
}

COMPtr<IDXGIDevice1> toDXGIDevice(const COMPtr<ID3D11Device1>& d3dDevice)
{
    if (!d3dDevice)
        return nullptr;

    COMPtr<IDXGIDevice1> dxgiDevice;
    HRESULT hr = d3dDevice->QueryInterface(__uuidof(IDXGIDevice1), (void **)&dxgiDevice);
    if (!SUCCEEDED(hr))
        return nullptr;

    return dxgiDevice;
}

COMPtr<IDXGIFactory2> factoryForDXGIDevice(const COMPtr<IDXGIDevice1>& device)
{
    if (!device)
        return nullptr;

    COMPtr<IDXGIAdapter> adaptor;
    HRESULT hr = device->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&adaptor));
    RELEASE_ASSERT(SUCCEEDED(hr));

    COMPtr<IDXGIFactory> factory;
    hr = adaptor->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&factory));
    RELEASE_ASSERT(SUCCEEDED(hr));

    COMPtr<IDXGIFactory2> factory2;
    hr = factory->QueryInterface(&factory2); 
    RELEASE_ASSERT(SUCCEEDED(hr));
    
    return factory2;
}

COMPtr<IDXGISwapChain> swapChainOfSizeForWindowAndDevice(const WebCore::IntSize& size, HWND window, const COMPtr<ID3D11Device1>& device)
{
    if (!device)
        return nullptr;

    DXGI_SWAP_CHAIN_DESC1 swapChainDescription;
    ::ZeroMemory(&swapChainDescription, sizeof(swapChainDescription));
    swapChainDescription.Width = size.width();
    swapChainDescription.Height = size.height();
    swapChainDescription.Format = webkitTextureFormat;
    swapChainDescription.SampleDesc.Count = 1;
    swapChainDescription.SampleDesc.Quality = 0;
    swapChainDescription.BufferUsage = DXGI_USAGE_SHADER_INPUT | DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDescription.BufferCount = 1;
    swapChainDescription.Flags = DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE;

    auto factory = Direct2D::factoryForDXGIDevice(Direct2D::toDXGIDevice(device));

    COMPtr<IDXGISwapChain1> swapChain1;
    HRESULT hr = factory->CreateSwapChainForHwnd(device.get(), window, &swapChainDescription, nullptr, nullptr, &swapChain1);
    if (!SUCCEEDED(hr))
        return nullptr;

    COMPtr<IDXGISwapChain> swapChain(Query, swapChain1);
    return swapChain;
}

COMPtr<ID2D1Bitmap> createBitmapCopyFromContext(ID2D1BitmapRenderTarget* bitmapTarget)
{
    COMPtr<ID2D1Bitmap> currentCanvas;
    HRESULT hr = bitmapTarget->GetBitmap(&currentCanvas);
    if (!SUCCEEDED(hr))
        return nullptr;

    auto bitmapCreateProperties = bitmapProperties();

    COMPtr<ID2D1Bitmap> bitmap;
    D2D1_SIZE_U bitmapSize = currentCanvas->GetPixelSize();
    hr = bitmapTarget->CreateBitmap(bitmapSize, bitmapCreateProperties, &bitmap);
    if (!SUCCEEDED(hr))
        return nullptr;

    auto targetPos = D2D1::Point2U();
    D2D1_RECT_U dataRect = D2D1::RectU(0, 0, bitmapSize.width, bitmapSize.height);
    hr = bitmap->CopyFromBitmap(&targetPos, currentCanvas.get(), &dataRect);
    if (!SUCCEEDED(hr))
        return false;

    return bitmap;
}

} // namespace Direct2D

} // namespace WebCore

#endif // USE(DIRECT2D)
