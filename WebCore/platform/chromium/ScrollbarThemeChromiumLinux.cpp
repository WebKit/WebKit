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
#include "RenderTheme.h"
#include "RenderThemeChromiumLinux.h"
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

static SkScalar clamp(SkScalar value, SkScalar min, SkScalar max)
{
    return std::min(std::max(value, min), max);
}

static SkColor saturateAndBrighten(SkScalar* hsv,
                                   SkScalar saturateAmount,
                                   SkScalar brightenAmount)
{
    SkScalar color[3];
    color[0] = hsv[0];
    color[1] = clamp(hsv[1] + saturateAmount, 0.0, 1.0);
    color[2] = clamp(hsv[2] + brightenAmount, 0.0, 1.0);
    return SkHSVToColor(color);
}

static SkColor outlineColor(SkScalar* hsv1, SkScalar* hsv2)
{
    // GTK Theme engines have way too much control over the layout of
    // the scrollbar. We might be able to more closely approximate its
    // look-and-feel, if we sent whole images instead of just colors
    // from the browser to the renderer. But even then, some themes
    // would just break.
    //
    // So, instead, we don't even try to 100% replicate the look of
    // the native scrollbar. We render our own version, but we make
    // sure to pick colors that blend in nicely with the system GTK
    // theme. In most cases, we can just sample a couple of pixels
    // from the system scrollbar and use those colors to draw our
    // scrollbar.
    //
    // This works fine for the track color and the overall thumb
    // color. But it fails spectacularly for the outline color used
    // around the thumb piece.  Not all themes have a clearly defined
    // outline. For some of them it is partially transparent, and for
    // others the thickness is very unpredictable.
    //
    // So, instead of trying to approximate the system theme, we
    // instead try to compute a reasonable looking choice based on the
    // known color of the track and the thumb piece. This is difficult
    // when trying to deal both with high- and low-contrast themes,
    // and both with positive and inverted themes.
    //
    // The following code has been tested to look OK with all of the
    // default GTK themes.
    SkScalar minDiff = clamp((hsv1[1] + hsv2[1]) * 1.2, 0.2, 0.5);
    SkScalar diff = clamp(fabs(hsv1[2] - hsv2[2]) / 2, minDiff, 0.5);

    if (hsv1[2] + hsv2[2] > 1.0)
        diff = -diff;

    return saturateAndBrighten(hsv2, -0.2, diff);
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
    SkScalar track_hsv[3];
    SkColorToHSV(RenderThemeChromiumLinux::trackColor(), track_hsv);
    paint.setColor(saturateAndBrighten(track_hsv, 0, 0));
    canvas->drawIRect(skrect, paint);

    SkScalar thumb_hsv[3];
    SkColorToHSV(RenderThemeChromiumLinux::thumbInactiveColor(),
                 thumb_hsv);

    paint.setColor(outlineColor(track_hsv, thumb_hsv));
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

    SkScalar thumb[3];
    SkColorToHSV(hovered
                 ? RenderThemeChromiumLinux::thumbActiveColor()
                 : RenderThemeChromiumLinux::thumbInactiveColor(),
                 thumb);

    SkPaint paint;
    paint.setColor(saturateAndBrighten(thumb, 0, 0.02));

    SkIRect skrect;
    if (vertical)
        skrect.set(rect.x(), rect.y(), midx + 1, rect.y() + rect.height());
    else
        skrect.set(rect.x(), rect.y(), rect.x() + rect.width(), midy + 1);

    canvas->drawIRect(skrect, paint);

    paint.setColor(saturateAndBrighten(thumb, 0, -0.02));

    if (vertical)
        skrect.set(midx + 1, rect.y(), rect.x() + rect.width(), rect.y() + rect.height());
    else
        skrect.set(rect.x(), midy + 1, rect.x() + rect.width(), rect.y() + rect.height());

    canvas->drawIRect(skrect, paint);

    SkScalar track[3];
    SkColorToHSV(RenderThemeChromiumLinux::trackColor(), track);
    paint.setColor(outlineColor(track, thumb));
    drawBox(canvas, rect, paint);

    if (rect.height() > 10 && rect.width() > 10) {
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
