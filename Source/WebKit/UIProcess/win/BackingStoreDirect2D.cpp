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

void BackingStore::paint(GdiConnections gdiConnections, const IntRect& rect)
{
    ASSERT(m_backend);
    ASSERT(m_backend->size() == m_size);

    auto* renderTarget = m_backend->renderTarget();

    RECT viewRect;
    ::GetClientRect(gdiConnections.hwnd, &viewRect);
    renderTarget->BindDC(gdiConnections.hdc, &viewRect);

    D2D1_RECT_F destRect = rect;

    if (auto* patternBrush = m_backend->bitmapBrush()) {
        renderTarget->BeginDraw();
        renderTarget->FillRectangle(&destRect, patternBrush);
        renderTarget->EndDraw();
    }
}

void BackingStore::incorporateUpdate(ShareableBitmap* bitmap, const UpdateInfo& updateInfo)
{
    if (!m_backend)
        m_backend = createBackend();

    scroll(updateInfo.scrollRect, updateInfo.scrollOffset);

    IntPoint updateRectBoundsLocation = updateInfo.updateRectBounds.location();

    COMPtr<ID2D1Bitmap> deviceUpdateBitmap = bitmap->createDirect2DSurface(m_backend->renderTarget());

#ifndef _NDEBUG
    auto deviceBitmapSize = deviceUpdateBitmap->GetPixelSize();
    ASSERT(deviceBitmapSize.width == updateInfo.updateRectBounds.width());
    ASSERT(deviceBitmapSize.height == updateInfo.updateRectBounds.height());
#endif

    for (const auto& updateRect : updateInfo.updateRects) {
        auto currentRectLocation = IntSize(updateRect.x() - updateRectBoundsLocation.x(), updateRect.y() - updateRectBoundsLocation.y());
        auto destRectLocation = IntSize(updateRect.x(), updateRect.y());
        Direct2D::copyRectFromOneSurfaceToAnother(deviceUpdateBitmap.get(), m_backend->surface(), currentRectLocation, updateRect, destRectLocation);
    }
}

void BackingStore::scroll(const IntRect& scrollRect, const IntSize& scrollOffset)
{
    if (scrollOffset.isZero())
        return;

    ASSERT(m_backend);
    m_backend->scroll(scrollRect, scrollOffset);
}

} // namespace WebKit
