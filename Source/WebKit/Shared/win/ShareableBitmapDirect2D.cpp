/*
 * Copyright (C) 2010-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Brent Fulgham <bfulgham@webkit.org>
 * Copyright (C) 2011 Igalia S.L.
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

#include "config.h"
#include "ShareableBitmap.h"

#include "Decoder.h"
#include "Encoder.h"
#include <WebCore/BitmapImage.h>
#include <WebCore/DIBPixelData.h>
#include <WebCore/Direct2DOperations.h>
#include <WebCore/Direct2DUtilities.h>
#include <WebCore/GraphicsContextImplDirect2D.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PlatformContextDirect2D.h>
#include <d2d1_1.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <wincodec.h>
#include <wtf/ProcessID.h>


namespace WebKit {
using namespace WebCore;

static unsigned strideForWidth(unsigned width)
{
    static unsigned bitsPerPixel = Direct2D::bitsPerPixel(Direct2D::wicBitmapFormat());
    return bitsPerPixel * width / 8;
}

Checked<unsigned, RecordOverflow> ShareableBitmap::calculateBytesPerRow(WebCore::IntSize size, const Configuration& configuration)
{
    return calculateBytesPerPixel(configuration) * size.width();
}

unsigned ShareableBitmap::calculateBytesPerPixel(const Configuration&)
{
    return 4;
}

void ShareableBitmap::createSharedResource()
{
    // Don't create a shared resource if the configuration instructs us to use an existing one.
    if (m_configuration.sharedResourceHandle)
        return;

    m_surface = Direct2D::createDXGISurfaceOfSize(m_size, nullptr, true);

    COMPtr<IDXGIResource1> resourceData;
    HRESULT hr = m_surface->QueryInterface(__uuidof(IDXGIResource), reinterpret_cast<void**>(&resourceData));
    RELEASE_ASSERT(SUCCEEDED(hr));

    hr = resourceData->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr, &m_configuration.sharedResourceHandle);
    RELEASE_ASSERT(SUCCEEDED(hr));
}

void ShareableBitmap::disposeSharedResource()
{
    if (m_surfaceMutex)
        m_surfaceMutex->ReleaseSync(1);
    if (m_configuration.sharedResourceHandle)
        ::CloseHandle(m_configuration.sharedResourceHandle);
}

void ShareableBitmap::leakSharedResource()
{
    m_configuration.sharedResourceHandle = nullptr;
}

std::unique_ptr<GraphicsContext> ShareableBitmap::createGraphicsContext()
{
    // Lock the surface to the current device while rendering.
    HRESULT hr = m_surface->QueryInterface(&m_surfaceMutex);
    RELEASE_ASSERT(SUCCEEDED(hr));
    hr = m_surfaceMutex->AcquireSync(0, INFINITE);
    RELEASE_ASSERT(SUCCEEDED(hr));

    auto surfaceContext = Direct2D::createSurfaceRenderTarget(m_surface.get());
    if (!surfaceContext)
        return nullptr;

    return makeUnique<GraphicsContext>(GraphicsContextImplDirect2D::createFactory(surfaceContext.get()));
}

void ShareableBitmap::paint(GraphicsContext& context, const IntPoint& dstPoint, const IntRect& srcRect)
{
    paint(context, 1, dstPoint, srcRect);
}

void ShareableBitmap::paint(GraphicsContext& context, float scaleFactor, const IntPoint& dstPoint, const IntRect& srcRect)
{
    auto surface = createDirect2DSurface(context.platformContext()->d3dDevice(), context.platformContext()->renderTarget());

#ifndef _NDEBUG
    auto bitmapSize = surface->GetPixelSize();
    ASSERT(bitmapSize.width == m_size.width());
    ASSERT(bitmapSize.height == m_size.height());
#endif

    FloatRect destRect(dstPoint, srcRect.size());
    FloatRect srcRectScaled(srcRect);
    srcRectScaled.scale(scaleFactor);

    ASSERT(context.hasPlatformContext());
    auto& state = context.state();
    auto& platformContext = *context.platformContext();

    ImagePaintingOptions options { state.compositeOperator, state.blendMode, ImageOrientation(), state.imageInterpolationQuality };
    Direct2D::drawPlatformImage(platformContext, surface.get(), m_size, destRect, srcRectScaled, options, state.alpha, Direct2D::ShadowState(state));
}

COMPtr<ID2D1Bitmap> ShareableBitmap::createDirect2DSurface(ID3D11Device1* directXDevice, ID2D1RenderTarget* renderTarget)
{
    if (!directXDevice)
        directXDevice = Direct2D::defaultDirectXDevice();

    if (!m_bitmap) {
        COMPtr<ID3D11Texture2D> texture;
        HRESULT hr = directXDevice->OpenSharedResource1(m_configuration.sharedResourceHandle, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&texture));
        if (!SUCCEEDED(hr))
            return nullptr;

        hr = texture->QueryInterface(&m_surface);
        RELEASE_ASSERT(SUCCEEDED(hr));

        // Lock the surface to the current device while rendering.
        hr = m_surface->QueryInterface(&m_surfaceMutex);
        RELEASE_ASSERT(SUCCEEDED(hr));
        hr = m_surfaceMutex->AcquireSync(1, INFINITE);
        RELEASE_ASSERT(SUCCEEDED(hr));

        auto bitmapProperties = Direct2D::bitmapProperties();
        hr = renderTarget->CreateSharedBitmap(__uuidof(IDXGISurface1), reinterpret_cast<void*>(m_surface.get()), &bitmapProperties, &m_bitmap);
        if (!SUCCEEDED(hr))
            return nullptr;
    }

    return m_bitmap;
}

RefPtr<Image> ShareableBitmap::createImage()
{
    auto surface = createDirect2DSurface(nullptr, GraphicsContext::defaultRenderTarget());
    if (!surface)
        return nullptr;

    return BitmapImage::create(WTFMove(surface));
}

} // namespace WebKit
