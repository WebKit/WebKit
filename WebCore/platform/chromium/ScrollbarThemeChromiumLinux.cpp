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

#include "gtkdrawing.h"
#include <gtk/gtk.h>
#include "skia/ext/GdkSkia.h"

namespace WebCore {

int ScrollbarThemeChromium::scrollbarThickness(ScrollbarControlSize controlSize)
{
    static int size = 0;
    if (!size) {
        MozGtkScrollbarMetrics metrics;
        moz_gtk_get_scrollbar_metrics(&metrics);
        size = metrics.slider_width;
    }
    return size;
}

bool ScrollbarThemeChromium::invalidateOnMouseEnterExit()
{
    notImplemented();
    return false;
}

// Given an uninitialised widget state object, set the members such that it's
// sane for drawing scrollbars
static void initMozState(GtkWidgetState* mozState)
{
    mozState->active = true;
    mozState->focused = false;
    mozState->inHover = false;
    mozState->disabled = false;
    mozState->isDefault = false;
    mozState->canDefault = false;
    mozState->depressed = false;
    mozState->curpos = 0;
    mozState->maxpos = 0;
}

// Paint a GTK widget
//   gc: context to draw onto
//   rect: the area of the widget
//   widget_type: the type of widget to draw
//   flags: widget dependent flags (e.g. direction of scrollbar arrows etc)
//
// See paintMozWiget in RenderThemeGtk.cpp for an explanation of the clipping.
static void paintScrollbarWidget(GraphicsContext* gc, const IntRect& rect,
                                 GtkThemeWidgetType widget_type, gint flags)
{
    PlatformContextSkia* pcs = gc->platformContext();

    GdkRectangle gdkRect = { rect.x(), rect.y(), rect.width(), rect.height() };

    const SkIRect clip_region = pcs->canvas()->getTotalClip().getBounds();
    TransformationMatrix ctm = gc->getCTM().inverse();
    IntPoint pos = ctm.mapPoint(
        IntPoint(SkScalarRound(clip_region.fLeft), SkScalarRound(clip_region.fTop)));
    GdkRectangle gdkClipRect;
    gdkClipRect.x = pos.x();
    gdkClipRect.y = pos.y();
    gdkClipRect.width = clip_region.width();
    gdkClipRect.height = clip_region.height();

    gdk_rectangle_intersect(&gdkRect, &gdkClipRect, &gdkClipRect);

    GtkWidgetState mozState;
    initMozState(&mozState);

    moz_gtk_widget_paint(widget_type, pcs->gdk_skia(), &gdkRect, &gdkClipRect,
                         &mozState, flags, GTK_TEXT_DIR_LTR);
}

void ScrollbarThemeChromium::paintTrackPiece(GraphicsContext* gc, Scrollbar* scrollbar,
                                             const IntRect& rect, ScrollbarPart partType)
{
    const bool horz = scrollbar->orientation() == HorizontalScrollbar;
    const GtkThemeWidgetType track_type =
        horz ? MOZ_GTK_SCROLLBAR_TRACK_HORIZONTAL : MOZ_GTK_SCROLLBAR_TRACK_VERTICAL;
    paintScrollbarWidget(gc, rect, track_type, 0);
}

void ScrollbarThemeChromium::paintButton(GraphicsContext* gc, Scrollbar* scrollbar,
                                    const IntRect& rect, ScrollbarPart part)
{
    // FIXME: It appears the either we're upsetting GTK by forcing WebKit sizes
    // on it, or the buttons expect the track to be drawn under them.  Either
    // way, we end up with unpainted pixels which are upsetting the pixel
    // tests. Thus we paint green under the buttons to, at least, make the
    // pixel output the same between debug and opt builds.
    SkPaint paint;
    paint.setARGB(255, 0, 255, 128);
    SkRect skrect;
    skrect.set(rect.x(), rect.y(), rect.x() + rect.width() - 1, rect.y() + rect.height() - 1);
    gc->platformContext()->canvas()->drawRect(skrect, paint);

    const bool horz = scrollbar->orientation() == HorizontalScrollbar;
    gint flags = horz ? 0 : MOZ_GTK_STEPPER_VERTICAL;
    flags |= ForwardButtonEndPart == part ? MOZ_GTK_STEPPER_DOWN : 0;
    paintScrollbarWidget(gc, rect, MOZ_GTK_SCROLLBAR_BUTTON, flags);
}

void ScrollbarThemeChromium::paintThumb(GraphicsContext* gc, Scrollbar* scrollbar, const IntRect& rect)
{
    const bool horz = scrollbar->orientation() == HorizontalScrollbar;
    const GtkThemeWidgetType thumb_type =
        horz ? MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL : MOZ_GTK_SCROLLBAR_THUMB_VERTICAL;
    paintScrollbarWidget(gc, rect, thumb_type, 0);
}

} // namespace WebCore
