/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollbarThemeChromium.h"

#include "NotImplemented.h"
#include "PlatformContextSkia.h"
#include "PlatformMouseEvent.h"
#include "Scrollbar.h"
#include "TransformationMatrix.h"

namespace WebCore {

int ScrollbarThemeChromium::scrollbarThickness(ScrollbarControlSize controlSize)
{
    return 15;
}

bool ScrollbarThemeChromium::invalidateOnMouseEnterExit()
{
    notImplemented();
    return false;
}

void ScrollbarThemeChromium::paintTrackPiece(GraphicsContext* gc, Scrollbar* scrollbar,
                                             const IntRect& rect, ScrollbarPart partType)
{
    const int right = rect.x() + rect.width();
    const int bottom = rect.y() + rect.height();
    SkCanvas* const canvas = gc->platformContext()->canvas();

    SkPaint paint;
    paint.setARGB(0xff, 0xe3, 0xdd, 0xd8);
    SkRect skrect;
    skrect.set(rect.x(), rect.y(), right, bottom);
    canvas->drawRect(skrect, paint);

    paint.setARGB(0xff, 0xc5, 0xba, 0xb0);
    canvas->drawLine(rect.x(), rect.y(), rect.x(), bottom, paint);
    canvas->drawLine(right, rect.y(), right, bottom, paint);
    canvas->drawLine(rect.x(), rect.y(), right, rect.y(), paint);
    canvas->drawLine(rect.x(), bottom, right, bottom, paint);
}

void ScrollbarThemeChromium::paintButton(GraphicsContext* gc, Scrollbar* scrollbar,
                                         const IntRect& rect, ScrollbarPart part)
{
    // We don't use buttons
}

void ScrollbarThemeChromium::paintThumb(GraphicsContext* gc, Scrollbar* scrollbar, const IntRect& rect)
{
    const bool hovered = scrollbar->hoveredPart() == ThumbPart;
    const int right = rect.x() + rect.width();
    const int bottom = rect.y() + rect.height();
    const int midx = rect.x() + rect.width() / 2;
    const int midy = rect.y() + rect.height() / 2;
    const bool vertical = scrollbar->orientation() == VerticalScrollbar;
    SkCanvas* const canvas = gc->platformContext()->canvas();

    SkPaint paint;
    if (hovered)
        paint.setARGB(0xff, 0xff, 0xff, 0xff);
    else
        paint.setARGB(0xff, 0xf4, 0xf2, 0xef);

    SkRect skrect;
    if (vertical) {
      skrect.set(rect.x(), rect.y(), midx + 1, bottom);
    } else {
      skrect.set(rect.x(), rect.y(), right, midy + 1);
    }
    canvas->drawRect(skrect, paint);

    if (hovered) {
      paint.setARGB(0xff, 0xf4, 0xf2, 0xef);
    } else {
      paint.setARGB(0xff, 0xea, 0xe5, 0xe0);
    }
    if (vertical) {
      skrect.set(midx + 1, rect.y(), right, bottom);
    } else {
      skrect.set(rect.x(), midy + 1, right, bottom);
    }
    canvas->drawRect(skrect, paint);

    paint.setARGB(0xff, 0x9d, 0x96, 0x8e);
    canvas->drawLine(rect.x(), rect.y(), rect.x(), bottom, paint);
    canvas->drawLine(right, rect.y(), right, bottom, paint);
    canvas->drawLine(rect.x(), rect.y(), right, rect.y(), paint);
    canvas->drawLine(rect.x(), bottom, right, bottom, paint);

    if (rect.height() > 10 && rect.width() > 10) {
      paint.setARGB(0xff, 0x9d, 0x96, 0x8e);
      canvas->drawLine(midx - 1, midy, midx + 3, midy, paint);
      canvas->drawLine(midx - 1, midy - 3, midx + 3, midy - 3, paint);
      canvas->drawLine(midx - 1, midy + 3, midx + 3, midy + 3, paint);
    }
}

} // namespace WebCore
