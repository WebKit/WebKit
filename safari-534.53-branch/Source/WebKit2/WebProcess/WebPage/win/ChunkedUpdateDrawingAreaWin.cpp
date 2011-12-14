/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "ChunkedUpdateDrawingArea.h"

#include "UpdateChunk.h"
#include "WebPage.h"
#include <WebCore/BitmapInfo.h>
#include <WebCore/GraphicsContext.h>

using namespace WebCore;

namespace WebKit {

void ChunkedUpdateDrawingArea::paintIntoUpdateChunk(UpdateChunk* updateChunk)
{
    OwnPtr<HDC> hdc = adoptPtr(::CreateCompatibleDC(0));

    void* bits;
    BitmapInfo bmp = BitmapInfo::createBottomUp(updateChunk->rect().size());
    OwnPtr<HBITMAP> hbmp = adoptPtr(::CreateDIBSection(0, &bmp, DIB_RGB_COLORS, &bits, updateChunk->memory(), 0));

    HBITMAP hbmpOld = static_cast<HBITMAP>(::SelectObject(hdc.get(), hbmp.get()));

    GraphicsContext gc(hdc.get());
    gc.save();

    // FIXME: Is this white fill needed?
    RECT rect = updateChunk->rect();
    ::FillRect(hdc.get(), &rect, (HBRUSH)::GetStockObject(WHITE_BRUSH));
    gc.translate(-updateChunk->rect().x(), -updateChunk->rect().y());

    m_webPage->drawRect(gc, updateChunk->rect());

    gc.restore();

    // Re-select the old HBITMAP
    ::SelectObject(hdc.get(), hbmpOld);
}

} // namespace WebKit
