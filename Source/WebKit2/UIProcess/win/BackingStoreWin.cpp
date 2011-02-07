/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include <WebCore/BitmapInfo.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/IntRect.h>

using namespace WebCore;

namespace WebKit {

class BitmapDC {
    WTF_MAKE_NONCOPYABLE(BitmapDC);

public:
    BitmapDC(HBITMAP, HDC destinationDC);
    ~BitmapDC();

    operator HDC() const { return m_dc.get(); }

private:
    OwnPtr<HDC> m_dc;
    HBITMAP m_originalBitmap;
};

BitmapDC::BitmapDC(HBITMAP bitmap, HDC destinationDC)
    : m_dc(adoptPtr(::CreateCompatibleDC(destinationDC)))
    , m_originalBitmap(static_cast<HBITMAP>(::SelectObject(m_dc.get(), bitmap)))
{
}

BitmapDC::~BitmapDC()
{
    ::SelectObject(m_dc.get(), m_originalBitmap);
}

void BackingStore::paint(HDC dc, const IntRect& rect)
{
    ASSERT(m_bitmap);
    ::BitBlt(dc, rect.x(), rect.y(), rect.width(), rect.height(), BitmapDC(m_bitmap.get(), dc), rect.x(), rect.y(), SRCCOPY);
}

static PassOwnPtr<HBITMAP> createBitmap(const IntSize& size)
{
    // FIXME: Maybe it would be better for performance to create a device-dependent bitmap here?
    BitmapInfo info = BitmapInfo::createBottomUp(size);
    void* bits;
    return adoptPtr(::CreateDIBSection(0, &info, DIB_RGB_COLORS, &bits, 0, 0));
}

void BackingStore::incorporateUpdate(ShareableBitmap* bitmap, const UpdateInfo& updateInfo)
{
    if (!m_bitmap)
        m_bitmap = createBitmap(m_size);

    scroll(updateInfo.scrollRect, updateInfo.scrollOffset);

    IntPoint updateRectLocation = updateInfo.updateRectBounds.location();

    BitmapDC dc(m_bitmap.get(), 0);
    GraphicsContext graphicsContext(dc);

    // Paint all update rects.
    for (size_t i = 0; i < updateInfo.updateRects.size(); ++i) {
        IntRect updateRect = updateInfo.updateRects[i];
        IntRect srcRect = updateRect;
        srcRect.move(-updateRectLocation.x(), -updateRectLocation.y());

        bitmap->paint(graphicsContext, updateRect.location(), srcRect);
    }
}

void BackingStore::scroll(const IntRect& scrollRect, const IntSize& scrollOffset)
{
    if (scrollOffset.isZero())
        return;

    RECT winScrollRect = scrollRect;
    ::ScrollDC(BitmapDC(m_bitmap.get(), 0), scrollOffset.width(), scrollOffset.height(), &winScrollRect, &winScrollRect, 0, 0);
}

} // namespace WebKit
