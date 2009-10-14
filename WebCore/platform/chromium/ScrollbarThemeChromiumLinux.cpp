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
#include "ScrollbarThemeChromiumLinux.h"

#include "PlatformContextSkia.h"
#include "PlatformMouseEvent.h"
#include "Scrollbar.h"
#include "TransformationMatrix.h"

namespace WebCore {

ScrollbarTheme* ScrollbarTheme::nativeTheme()
{
    static ScrollbarThemeChromiumLinux theme;
    return &theme;
}

int ScrollbarThemeChromiumLinux::scrollbarThickness(ScrollbarControlSize controlSize)
{
    return 15;
}

static void drawVertLine(SkCanvas* canvas, int x, int y1, int y2, const SkPaint& paint)
{
    SkIRect skrect;
    skrect.set(x, y1, x + 1, y2 + 1);
    canvas->drawIRect(skrect, paint);
}

static void drawHorizLine(SkCanvas* canvas, int x1, int x2, int y, const SkPaint& paint)
{
    SkIRect skrect;
    skrect.set(x1, y, x2 + 1, y + 1);
    canvas->drawIRect(skrect, paint);
}

static void drawBox(SkCanvas* canvas, const IntRect& rect, const SkPaint& paint)
{
    const int right = rect.x() + rect.width() - 1;
    const int bottom = rect.y() + rect.height() - 1;
    drawHorizLine(canvas, rect.x(), right, rect.y(), paint);
    drawVertLine(canvas, right, rect.y(), bottom, paint);
    drawHorizLine(canvas, rect.x(), right, bottom, paint);
    drawVertLine(canvas, rect.x(), rect.y(), bottom, paint);
}

IntRect ScrollbarThemeChromium::trackRect(Scrollbar* scrollbar, bool)
{
    IntSize bs = buttonSize(scrollbar);
    int thickness = scrollbarThickness(scrollbar->controlSize());
    if (scrollbar->orientation() == HorizontalScrollbar)
        return IntRect(scrollbar->x() + bs.width(), scrollbar->y(), scrollbar->width(), thickness);
    return IntRect(scrollbar->x(), scrollbar->y() + bs.height(), thickness, scrollbar->height());
}

void ScrollbarThemeChromiumLinux::paintTrackPiece(GraphicsContext* gc, Scrollbar* scrollbar, const IntRect& rect, ScrollbarPart partType)
{
    SkCanvas* const canvas = gc->platformContext()->canvas();
    SkPaint paint;
    SkIRect skrect;

    skrect.set(rect.x(), rect.y(), rect.x() + rect.width(), rect.y() + rect.height());
    paint.setARGB(0xff, 0xe3, 0xdd, 0xd8);
    canvas->drawIRect(skrect, paint);

    paint.setARGB(0xff, 0xc5, 0xba, 0xb0);
    drawBox(canvas, rect, paint);
}

void ScrollbarThemeChromiumLinux::paintButton(GraphicsContext* gc, Scrollbar* scrollbar, const IntRect& rect, ScrollbarPart part)
{
    // We don't use buttons
}

void ScrollbarThemeChromiumLinux::paintThumb(GraphicsContext* gc, Scrollbar* scrollbar, const IntRect& rect)
{
    const bool hovered = scrollbar->hoveredPart() == ThumbPart;
    const int midx = rect.x() + rect.width() / 2;
    const int midy = rect.y() + rect.height() / 2;
    const bool vertical = scrollbar->orientation() == VerticalScrollbar;
    SkCanvas* const canvas = gc->platformContext()->canvas();

    SkPaint paint;
    if (hovered)
        paint.setARGB(0xff, 0xff, 0xff, 0xff);
    else
        paint.setARGB(0xff, 0xf4, 0xf2, 0xef);

    SkIRect skrect;
    if (vertical)
        skrect.set(rect.x(), rect.y(), midx + 1, rect.y() + rect.height());
    else
        skrect.set(rect.x(), rect.y(), rect.x() + rect.width(), midy + 1);

    canvas->drawIRect(skrect, paint);

    if (hovered)
        paint.setARGB(0xff, 0xf4, 0xf2, 0xef);
    else
        paint.setARGB(0xff, 0xea, 0xe5, 0xe0);

    if (vertical)
        skrect.set(midx + 1, rect.y(), rect.x() + rect.width(), rect.y() + rect.height());
    else
        skrect.set(rect.x(), midy + 1, rect.x() + rect.width(), rect.y() + rect.height());

    canvas->drawIRect(skrect, paint);

    paint.setARGB(0xff, 0x9d, 0x96, 0x8e);
    drawBox(canvas, rect, paint);

    if (rect.height() > 10 && rect.width() > 10) {
        paint.setARGB(0xff, 0x9d, 0x96, 0x8e);
        const int grippyHalfWidth = 2;
        const int interGrippyOffset = 3;
        if (vertical) {
            drawHorizLine(canvas, midx - grippyHalfWidth, midx + grippyHalfWidth, midy - interGrippyOffset, paint);
            drawHorizLine(canvas, midx - grippyHalfWidth, midx + grippyHalfWidth, midy,                     paint);
            drawHorizLine(canvas, midx - grippyHalfWidth, midx + grippyHalfWidth, midy + interGrippyOffset, paint);
        } else {
            drawVertLine(canvas, midx - interGrippyOffset, midy - grippyHalfWidth, midy + grippyHalfWidth, paint);
            drawVertLine(canvas, midx,                     midy - grippyHalfWidth, midy + grippyHalfWidth, paint);
            drawVertLine(canvas, midx + interGrippyOffset, midy - grippyHalfWidth, midy + grippyHalfWidth, paint);
        }
    }
}

bool ScrollbarThemeChromiumLinux::shouldCenterOnThumb(Scrollbar*, const PlatformMouseEvent& evt)
{
    return (evt.shiftKey() && evt.button() == LeftButton) || (evt.button() == MiddleButton);
}

IntSize ScrollbarThemeChromiumLinux::buttonSize(Scrollbar* scrollbar)
{
    // On Linux, we don't use buttons
    return IntSize(0, 0);
}

int ScrollbarThemeChromiumLinux::minimumThumbLength(Scrollbar* scrollbar)
{
    // This matches Firefox on Linux.
    return 2 * scrollbarThickness(scrollbar->controlSize());
}

} // namespace WebCore
