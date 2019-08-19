/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "BackingStore.h"

#include "ShareableBitmap.h"
#include "UpdateInfo.h"
#include "WebPageProxy.h"
#include <WebCore/BackingStoreBackendDirect2DImpl.h>
#include <WebCore/COMPtr.h>
#include <WebCore/Direct2DUtilities.h>
#include <WebCore/GraphicsContextImplDirect2D.h>
#include <WebCore/PlatformContextDirect2D.h>
#include <d2d1.h>

namespace WebKit {
using namespace WebCore;

std::unique_ptr<BackingStoreBackendDirect2D> BackingStore::createBackend()
{
    return makeUnique<BackingStoreBackendDirect2DImpl>(m_size, m_deviceScaleFactor);
}

void BackingStore::paint(ID2D1RenderTarget* renderTarget, const IntRect& rect)
{
    renderTarget->BeginDraw();

    ASSERT(m_backend);

    auto bitmapBrushProperties = D2D1::BitmapBrushProperties();
    auto brushProperties = D2D1::BrushProperties();

    COMPtr<ID2D1BitmapBrush> patternBrush;
    HRESULT hr = renderTarget->CreateBitmapBrush(m_backend->surface(), &bitmapBrushProperties, &brushProperties, &patternBrush);
    ASSERT(SUCCEEDED(hr));
    if (!SUCCEEDED(hr))
        return;

    D2D1_RECT_F destRect(rect);
    renderTarget->FillRectangle(&destRect, patternBrush.get());
    renderTarget->EndDraw();
}

void BackingStore::incorporateUpdate(ShareableBitmap* bitmap, const UpdateInfo& updateInfo)
{
    WTFLogAlways("BackingStore::incorporateUpdate");
    if (!m_backend)
        m_backend = createBackend();

    scroll(updateInfo.scrollRect, updateInfo.scrollOffset);

    // Paint all update rects.
    IntPoint updateRectLocation = updateInfo.updateRectBounds.location();

    COMPtr<ID2D1BitmapRenderTarget> bitmapRenderTarget;
    HRESULT hr = m_backend->renderTarget()->CreateCompatibleRenderTarget(&bitmapRenderTarget);
    GraphicsContext graphicsContext(GraphicsContextImplDirect2D::createFactory(bitmapRenderTarget.get()));

    // When m_webPageProxy.drawsBackground() is false, bitmap contains transparent parts as a background of the webpage.
    // For such case, bitmap must be drawn using CompositeCopy to overwrite the existing surface.
    graphicsContext.setCompositeOperation(WebCore::CompositeCopy);

    for (const auto& updateRect : updateInfo.updateRects) {
        IntRect srcRect = updateRect;
        srcRect.move(-updateRectLocation.x(), -updateRectLocation.y());
        bitmap->paint(graphicsContext, deviceScaleFactor(), updateRect.location(), srcRect);
    }

    COMPtr<ID2D1Bitmap> output;
    hr = bitmapRenderTarget->GetBitmap(&output);
    D2D1_POINT_2U destPoint = D2D1::Point2U();
    auto size = Direct2D::bitmapSize(output.get());
    D2D1_RECT_U destRect = D2D1::RectU(0, 0, size.width(), size.height());
    hr = m_backend->surface()->CopyFromBitmap(&destPoint, output.get(), &destRect);
}

void BackingStore::scroll(const IntRect& scrollRect, const IntSize& scrollOffset)
{
    if (scrollOffset.isZero())
        return;

    WTFLogAlways("BackingStore::scroll");

    ASSERT(m_backend);
    m_backend->scroll(scrollRect, scrollOffset);
}

} // namespace WebKit
