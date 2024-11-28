/*
 * Copyright (C) 2024 Sony Interactive Entertainment Inc.
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

#if USE(SKIA)

#include "UpdateInfo.h"
#include <WebCore/BitmapInfo.h>
#include <WebCore/GraphicsContextSkia.h>
#include <skia/core/SkBitmap.h>
#include <skia/core/SkCanvas.h>

namespace WebKit {

BackingStore::BackingStore(const WebCore::IntSize& size, float deviceScaleFactor)
    : m_size(size)
    , m_deviceScaleFactor(deviceScaleFactor)
{
    WebCore::IntSize scaledSize(size);
    scaledSize.scale(deviceScaleFactor);
    auto info = SkImageInfo::MakeN32Premul(scaledSize.width(), scaledSize.height(), SkColorSpace::MakeSRGB());
    m_surface = SkSurfaces::Raster(info);
    RELEASE_ASSERT(m_surface);
}

BackingStore::~BackingStore() = default;

void BackingStore::paint(PlatformPaintContextPtr cr, const WebCore::IntRect& rect)
{
#if PLATFORM(WIN)
    SkPixmap pixmap;
    if (m_surface->peekPixels(&pixmap)) {
        WebCore::IntRect scaledRect = rect;
        scaledRect.scale(m_deviceScaleFactor);
        auto bitmapInfo = WebCore::BitmapInfo::createBottomUp({ pixmap.width(), pixmap.height() });
        SetDIBitsToDevice(cr, 0, 0, pixmap.width(), pixmap.height(), 0, 0, 0, pixmap.height(), pixmap.addr(), &bitmapInfo, DIB_RGB_COLORS);
    }
#else
    m_surface->draw(cr, rect.x(), rect.y());
#endif
}

void BackingStore::incorporateUpdate(UpdateInfo&& updateInfo)
{
    ASSERT(m_size == updateInfo.viewSize);
    if (!updateInfo.bitmapHandle)
        return;

    auto bitmap = WebCore::ShareableBitmap::create(WTFMove(*updateInfo.bitmapHandle));
    if (!bitmap)
        return;

#if ASSERT_ENABLED
    WebCore::IntSize updateSize = updateInfo.updateRectBounds.size();
    updateSize.scale(m_deviceScaleFactor);
    ASSERT(bitmap->size() == updateSize);
#endif

    scroll(updateInfo.scrollRect, updateInfo.scrollOffset);

    // Paint all update rects.
    WebCore::IntPoint updateRectLocation = updateInfo.updateRectBounds.location();

    SkCanvas* canvas = m_surface->getCanvas();
    if (!canvas)
        return;

    WebCore::GraphicsContextSkia graphicsContext(*canvas, WebCore::RenderingMode::Unaccelerated, WebCore::RenderingPurpose::ShareableLocalSnapshot);
    graphicsContext.setCompositeOperation(WebCore::CompositeOperator::Copy);

    for (const auto& updateRect : updateInfo.updateRects) {
        WebCore::IntRect srcRect = updateRect;
        srcRect.move(-updateRectLocation.x(), -updateRectLocation.y());
        bitmap->paint(graphicsContext, m_deviceScaleFactor, updateRect.location(), srcRect);
    }
}

void BackingStore::scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset)
{
    if (scrollOffset.isZero())
        return;

    WebCore::IntRect targetRect = scrollRect;
    targetRect.move(scrollOffset);
    targetRect.intersect(scrollRect);
    if (targetRect.isEmpty())
        return;

    WebCore::IntSize scaledScrollOffset = scrollOffset;
    targetRect.scale(m_deviceScaleFactor);
    scaledScrollOffset.scale(m_deviceScaleFactor, m_deviceScaleFactor);

    SkBitmap bitmap;
    bitmap.allocPixels(m_surface->imageInfo().makeWH(targetRect.width(), targetRect.height()));

    if (!m_surface->readPixels(bitmap, targetRect.x() - scaledScrollOffset.width(), targetRect.y() - scaledScrollOffset.height()))
        return;

    m_surface->writePixels(bitmap, targetRect.x(), targetRect.y());
}

} // namespace WebKit

#endif // USE(SKIA)
