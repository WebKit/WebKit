/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "PlatformThemeChromiumLinux.h"

namespace WebCore {

unsigned PlatformThemeChromiumLinux::s_thumbInactiveColor = 0xeaeaea;
unsigned PlatformThemeChromiumLinux::s_thumbActiveColor = 0xf4f4f4;
unsigned PlatformThemeChromiumLinux::s_trackColor = 0xd3d3d3;

void PlatformThemeChromiumLinux::setScrollbarColors(
    SkColor inactiveColor, SkColor activeColor, SkColor trackColor)
{
    s_thumbInactiveColor = inactiveColor;
    s_thumbActiveColor = activeColor;
    s_trackColor = trackColor;
}

static SkScalar clamp(SkScalar value, SkScalar min, SkScalar max)
{
    return std::min(std::max(value, min), max);
}

SkColor PlatformThemeChromiumLinux::saturateAndBrighten(const SkScalar hsv[3], SkScalar saturateAmount, SkScalar brightenAmount)
{
    SkScalar color[3];
    color[0] = hsv[0];
    color[1] = clamp(hsv[1] + saturateAmount, 0.0, 1.0);
    color[2] = clamp(hsv[2] + brightenAmount, 0.0, 1.0);
    return SkHSVToColor(color);
}

SkColor PlatformThemeChromiumLinux::outlineColor(const SkScalar hsv1[3], const SkScalar hsv2[3])
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
    // around the thumb piece. Not all themes have a clearly defined
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
    SkScalar minDiff = clamp((hsv1[1] + hsv2[1]) * 1.2, 0.28, 0.5);
    SkScalar diff = clamp(fabs(hsv1[2] - hsv2[2]) / 2, minDiff, 0.5);

    if (hsv1[2] + hsv2[2] > 1.0)
        diff = -diff;

    return saturateAndBrighten(hsv2, -0.2, diff);
}

void PlatformThemeChromiumLinux::paintArrowButton(GraphicsContext* gc, const IntRect& rect, ArrowDirection direction, ControlStates states)
{
    PlatformContextSkia* platformContext = gc->platformContext();
    int widthMiddle, lengthMiddle;
    SkPaint paint;
    if (direction == North || direction == South) {
        widthMiddle = rect.width() / 2 + 1;
        lengthMiddle = rect.height() / 2 + 1;
    } else {
        lengthMiddle = rect.width() / 2 + 1;
        widthMiddle = rect.height() / 2 + 1;
    }

    // Calculate button color.
    SkScalar trackHSV[3];
    SkColorToHSV(trackColor(), trackHSV);
    SkColor buttonColor = saturateAndBrighten(trackHSV, 0, 0.2);
    SkColor backgroundColor = buttonColor;
    if (states & PressedState) {
        SkScalar buttonHSV[3];
        SkColorToHSV(buttonColor, buttonHSV);
        buttonColor = saturateAndBrighten(buttonHSV, 0, -0.1);
    } else if (states & HoverState) {
        SkScalar buttonHSV[3];
        SkColorToHSV(buttonColor, buttonHSV);
        buttonColor = saturateAndBrighten(buttonHSV, 0, 0.05);
    }

    SkIRect skrect;
    skrect.set(rect.x(), rect.y(), rect.x() + rect.width(), rect.y() + rect.height());
    // Paint the background (the area visible behind the rounded corners).
    paint.setColor(backgroundColor);
    platformContext->drawIRect(skrect, paint);

    // Paint the button's outline and fill the middle
    SkPath outline;
    switch (direction) {
    case North:
        outline.moveTo(rect.x() + 0.5, rect.y() + rect.height() + 0.5);
        outline.rLineTo(0, -(rect.height() - 2));
        outline.rLineTo(2, -2);
        outline.rLineTo(rect.width() - 5, 0);
        outline.rLineTo(2, 2);
        outline.rLineTo(0, rect.height() - 2);
        break;
    case South:
        outline.moveTo(rect.x() + 0.5, rect.y() - 0.5);
        outline.rLineTo(0, rect.height() - 2);
        outline.rLineTo(2, 2);
        outline.rLineTo(rect.width() - 5, 0);
        outline.rLineTo(2, -2);
        outline.rLineTo(0, -(rect.height() - 2));
        break;
    case East:
        outline.moveTo(rect.x() - 0.5, rect.y() + 0.5);
        outline.rLineTo(rect.width() - 2, 0);
        outline.rLineTo(2, 2);
        outline.rLineTo(0, rect.height() - 5);
        outline.rLineTo(-2, 2);
        outline.rLineTo(-(rect.width() - 2), 0);
        break;
    case West:
        outline.moveTo(rect.x() + rect.width() + 0.5, rect.y() + 0.5);
        outline.rLineTo(-(rect.width() - 2), 0);
        outline.rLineTo(-2, 2);
        outline.rLineTo(0, rect.height() - 5);
        outline.rLineTo(2, 2);
        outline.rLineTo(rect.width() - 2, 0);
        break;
    }
    outline.close();

    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(buttonColor);
    platformContext->drawPath(outline, paint);

    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);
    SkScalar thumbHSV[3];
    SkColorToHSV(thumbInactiveColor(), thumbHSV);
    paint.setColor(outlineColor(trackHSV, thumbHSV));
    platformContext->drawPath(outline, paint);

    // If the button is disabled or read-only, the arrow is drawn with the outline color.
    if (states & EnabledState && !(states & ReadOnlyState))
        paint.setColor(SK_ColorBLACK);

    paint.setAntiAlias(false);
    paint.setStyle(SkPaint::kFill_Style);

    SkPath path;
    // The constants in this block of code are hand-tailored to produce good
    // looking arrows without anti-aliasing.
    switch (direction) {
    case North:
        path.moveTo(rect.x() + widthMiddle - 4, rect.y() + lengthMiddle + 2);
        path.rLineTo(7, 0);
        path.rLineTo(-4, -4);
        break;
    case South:
        path.moveTo(rect.x() + widthMiddle - 4, rect.y() + lengthMiddle - 3);
        path.rLineTo(7, 0);
        path.rLineTo(-4, 4);
        break;
    case East:
        path.moveTo(rect.x() + lengthMiddle - 3, rect.y() + widthMiddle - 4);
        path.rLineTo(0, 7);
        path.rLineTo(4, -4);
        break;
    case West:
        path.moveTo(rect.x() + lengthMiddle + 1, rect.y() + widthMiddle - 5);
        path.rLineTo(0, 9);
        path.rLineTo(-4, -4);
        break;
    }
    path.close();

    platformContext->drawPath(path, paint);
}

} // namespace WebCore

