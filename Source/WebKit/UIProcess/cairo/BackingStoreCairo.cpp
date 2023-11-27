/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2011,2014 Igalia S.L.
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
#include <WebCore/BackingStoreBackendCairoImpl.h>
#include <WebCore/CairoUtilities.h>
#include <WebCore/GraphicsContextCairo.h>
#include <WebCore/RefPtrCairo.h>
#include <cairo.h>

namespace WebKit {
using namespace WebCore;

std::unique_ptr<BackingStoreBackendCairo> BackingStore::createBackend()
{
    return makeUnique<BackingStoreBackendCairoImpl>(m_size, m_deviceScaleFactor);
}

void BackingStore::paint(cairo_t* context, const IntRect& rect)
{
    ASSERT(m_backend);

    cairo_save(context);
    cairo_set_operator(context, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(context, m_backend->surface(), 0, 0);
    cairo_rectangle(context, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_fill(context);
    cairo_restore(context);
}

void BackingStore::incorporateUpdate(ShareableBitmap* bitmap, UpdateInfo&& updateInfo)
{
    if (!m_backend)
        m_backend = createBackend();

    scroll(updateInfo.scrollRect, updateInfo.scrollOffset);

    // Paint all update rects.
    IntPoint updateRectLocation = updateInfo.updateRectBounds.location();
    GraphicsContextCairo graphicsContext(m_backend->surface());

    // When m_webPageProxy.drawsBackground() is false, bitmap contains transparent parts as a background of the webpage.
    // For such case, bitmap must be drawn using CompositeOperator::Copy to overwrite the existing surface.
    graphicsContext.setCompositeOperation(WebCore::CompositeOperator::Copy);

    for (const auto& updateRect : updateInfo.updateRects) {
        IntRect srcRect = updateRect;
        srcRect.move(-updateRectLocation.x(), -updateRectLocation.y());
        bitmap->paint(graphicsContext, deviceScaleFactor(), updateRect.location(), srcRect);
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
