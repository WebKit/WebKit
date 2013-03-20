/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

// This file implements a simple generic version of the WebThemeEngine,
// which is used to draw all the native controls on a web page. We use this
// file when running in layout test mode in order to remove any
// platform-specific rendering differences due to themes, colors, etc.
//

#include "config.h"
#include "WebTestThemeControlWin.h"

#include "TestCommon.h"
#include "skia/ext/skia_utils_win.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRect.h"

#include <algorithm>

using namespace WebKit;
using namespace std;

namespace WebTestRunner {

namespace {

const SkColor edgeColor     = SK_ColorBLACK;
const SkColor readOnlyColor = SkColorSetRGB(0xe9, 0xc2, 0xa6);
const SkColor fgColor       = SK_ColorBLACK;
const SkColor bgColors[]    = {
    SK_ColorBLACK, // Unknown
    SkColorSetRGB(0xc9, 0xc9, 0xc9), // Disabled
    SkColorSetRGB(0xf3, 0xe0, 0xd0), // Readonly
    SkColorSetRGB(0x89, 0xc4, 0xff), // Normal
    SkColorSetRGB(0x43, 0xf9, 0xff), // Hot
    SkColorSetRGB(0x20, 0xf6, 0xcc), // Focused
    SkColorSetRGB(0x00, 0xf3, 0xac), // Hover
    SkColorSetRGB(0xa9, 0xff, 0x12), // Pressed
    SkColorSetRGB(0xcc, 0xcc, 0xcc) // Indeterminate
};

SkIRect validate(const SkIRect& rect, WebTestThemeControlWin::Type ctype)
{
    switch (ctype) {
    case WebTestThemeControlWin::UncheckedBoxType:
    case WebTestThemeControlWin::CheckedBoxType:
    case WebTestThemeControlWin::UncheckedRadioType:
    case WebTestThemeControlWin::CheckedRadioType: {
        SkIRect retval = rect;

        // The maximum width and height is 13.
        // Center the square in the passed rectangle.
        const int maxControlSize = 13;
        int controlSize = std::min(rect.width(), rect.height());
        controlSize = std::min(controlSize, maxControlSize);

        retval.fLeft   = rect.fLeft + (rect.width() / 2) - (controlSize / 2);
        retval.fRight  = retval.fLeft + controlSize - 1;
        retval.fTop    = rect.fTop + (rect.height() / 2) - (controlSize / 2);
        retval.fBottom = retval.fTop + controlSize - 1;

        return retval;
    }

    default:
        return rect;
    }
}

}

WebTestThemeControlWin::WebTestThemeControlWin(SkCanvas* canvas, const SkIRect& irect, Type ctype, State cstate)
    : m_canvas(canvas)
    , m_irect(validate(irect, ctype))
    , m_type(ctype)
    , m_state(cstate)
    , m_left(m_irect.fLeft)
    , m_right(m_irect.fRight)
    , m_top(m_irect.fTop)
    , m_bottom(m_irect.fBottom)
    , m_height(m_irect.height())
    , m_width(m_irect.width())
    , m_edgeColor(edgeColor)
    , m_bgColor(bgColors[cstate])
    , m_fgColor(fgColor)
{
}

WebTestThemeControlWin::~WebTestThemeControlWin()
{
}

void WebTestThemeControlWin::box(const SkIRect& rect, SkColor fillColor)
{
    SkPaint paint;

    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(fillColor);
    m_canvas->drawIRect(rect, paint);

    paint.setColor(m_edgeColor);
    paint.setStyle(SkPaint::kStroke_Style);
    m_canvas->drawIRect(rect, paint);
}

void WebTestThemeControlWin::line(int x0, int y0, int x1, int y1, SkColor color)
{
    SkPaint paint;
    paint.setColor(color);
    m_canvas->drawLine(SkIntToScalar(x0), SkIntToScalar(y0), SkIntToScalar(x1), SkIntToScalar(y1), paint);
}

void WebTestThemeControlWin::triangle(int x0, int y0, int x1, int y1, int x2, int y2, SkColor color)
{
    SkPath path;
    SkPaint paint;

    paint.setColor(color);
    paint.setStyle(SkPaint::kFill_Style);
    path.incReserve(4);
    path.moveTo(SkIntToScalar(x0), SkIntToScalar(y0));
    path.lineTo(SkIntToScalar(x1), SkIntToScalar(y1));
    path.lineTo(SkIntToScalar(x2), SkIntToScalar(y2));
    path.close();
    m_canvas->drawPath(path, paint);

    paint.setColor(m_edgeColor);
    paint.setStyle(SkPaint::kStroke_Style);
    m_canvas->drawPath(path, paint);
}

void WebTestThemeControlWin::roundRect(SkColor color)
{
    SkRect rect;
    SkScalar radius = SkIntToScalar(5);
    SkPaint paint;

    rect.set(m_irect);
    paint.setColor(color);
    paint.setStyle(SkPaint::kFill_Style);
    m_canvas->drawRoundRect(rect, radius, radius, paint);

    paint.setColor(m_edgeColor);
    paint.setStyle(SkPaint::kStroke_Style);
    m_canvas->drawRoundRect(rect, radius, radius, paint);
}

void WebTestThemeControlWin::oval(SkColor color)
{
    SkRect rect;
    SkPaint paint;

    rect.set(m_irect);
    paint.setColor(color);
    paint.setStyle(SkPaint::kFill_Style);
    m_canvas->drawOval(rect, paint);

    paint.setColor(m_edgeColor);
    paint.setStyle(SkPaint::kStroke_Style);
    m_canvas->drawOval(rect, paint);
}

void WebTestThemeControlWin::circle(SkScalar radius, SkColor color)
{
    SkScalar cy = SkIntToScalar(m_top  + m_height / 2);
    SkScalar cx = SkIntToScalar(m_left + m_width / 2);
    SkPaint paint;

    paint.setColor(color);
    paint.setStyle(SkPaint::kFill_Style);
    m_canvas->drawCircle(cx, cy, radius, paint);

    paint.setColor(m_edgeColor);
    paint.setStyle(SkPaint::kStroke_Style);
    m_canvas->drawCircle(cx, cy, radius, paint);
}

void WebTestThemeControlWin::nestedBoxes(int indentLeft, int indentTop, int indentRight, int indentBottom, SkColor outerColor, SkColor innerColor)
{
    SkIRect lirect;
    box(m_irect, outerColor);
    lirect.set(m_irect.fLeft + indentLeft, m_irect.fTop + indentTop, m_irect.fRight - indentRight, m_irect.fBottom - indentBottom);
    box(lirect, innerColor);
}

void WebTestThemeControlWin::markState()
{
    // The horizontal lines in a read only control are spaced by this amount.
    const int readOnlyLineOffset = 5;

    // The length of a triangle side for the corner marks.
    const int triangleSize = 5;

    switch (m_state) {
    case UnknownState:
    case DisabledState:
    case NormalState:
    case IndeterminateState:
        // Don't visually mark these states (color is enough).
        break;
    case ReadOnlyState:
        // Drawing lines across the control.
        for (int i = m_top + readOnlyLineOffset; i < m_bottom; i += readOnlyLineOffset)
            line(m_left + 1, i, m_right - 1, i, readOnlyColor);
        break;

    case HotState:
        // Draw a triangle in the upper left corner of the control.
        triangle(m_left, m_top, m_left + triangleSize, m_top, m_left, m_top + triangleSize, m_edgeColor);
        break;

    case HoverState:
        // Draw a triangle in the upper right corner of the control.
        triangle(m_right, m_top, m_right, m_top + triangleSize, m_right - triangleSize, m_top, m_edgeColor);
        break;

    case FocusedState:
        // Draw a triangle in the bottom right corner of the control.
        triangle(m_right, m_bottom, m_right - triangleSize, m_bottom, m_right, m_bottom - triangleSize, m_edgeColor);
        break;

    case PressedState:
        // Draw a triangle in the bottom left corner of the control.
        triangle(m_left, m_bottom, m_left, m_bottom - triangleSize, m_left + triangleSize, m_bottom, m_edgeColor);
        break;

    default:
        WEBKIT_ASSERT_NOT_REACHED();
        break;
    }
}

void WebTestThemeControlWin::draw()
{
    int halfWidth = m_width / 2;
    int halfHeight = m_height / 2;
    int quarterWidth = m_width / 4;
    int quarterHeight = m_height / 4;

    // Indent amounts for the check in a checkbox or radio button.
    const int checkIndent = 3;

    // Indent amounts for short and long sides of the scrollbar notches.
    const int notchLongOffset = 1;
    const int notchShortOffset = 4;
    const int noOffset = 0;

    // Indent amounts for the short and long sides of a scroll thumb box.
    const int thumbLongIndent = 0;
    const int thumbShortIndent = 2;

    // Indents for the crosshatch on a scroll grip.
    const int gripLongIndent = 3;
    const int gripShortIndent = 5;

    // Indents for the the slider track.
    const int sliderIndent = 2;

    switch (m_type) {
    case UnknownType:
        WEBKIT_ASSERT_NOT_REACHED();
        break;

    case TextFieldType:
        // We render this by hand outside of this function.
        WEBKIT_ASSERT_NOT_REACHED();
        break;

    case PushButtonType:
        // push buttons render as a rounded rectangle
        roundRect(m_bgColor);
        break;

    case UncheckedBoxType:
        // Unchecked boxes are simply plain boxes.
        box(m_irect, m_bgColor);
        break;

    case CheckedBoxType:
        nestedBoxes(checkIndent, checkIndent, checkIndent, checkIndent, m_bgColor, m_fgColor);
        break;

    case IndeterminateCheckboxType:
        // Indeterminate checkbox is a box containing '-'.
        nestedBoxes(checkIndent, halfHeight, checkIndent, halfHeight, m_bgColor, m_fgColor);
        break;

    case UncheckedRadioType:
        circle(SkIntToScalar(halfHeight), m_bgColor);
        break;

    case CheckedRadioType:
        circle(SkIntToScalar(halfHeight), m_bgColor);
        circle(SkIntToScalar(halfHeight - checkIndent), m_fgColor);
        break;

    case HorizontalScrollTrackBackType: {
        // Draw a box with a notch at the left.
        int longOffset = halfHeight - notchLongOffset;
        int shortOffset = m_width - notchShortOffset;
        nestedBoxes(noOffset, longOffset, shortOffset, longOffset, m_bgColor, m_edgeColor);
        break;
    }

    case HorizontalScrollTrackForwardType: {
        // Draw a box with a notch at the right.
        int longOffset  = halfHeight - notchLongOffset;
        int shortOffset = m_width - notchShortOffset;
        nestedBoxes(shortOffset, longOffset, noOffset, longOffset, m_bgColor, m_fgColor);
        break;
    }

    case VerticalScrollTrackBackType: {
        // Draw a box with a notch at the top.
        int longOffset  = halfWidth - notchLongOffset;
        int shortOffset = m_height - notchShortOffset;
        nestedBoxes(longOffset, noOffset, longOffset, shortOffset, m_bgColor, m_fgColor);
        break;
    }

    case VerticalScrollTrackForwardType: {
        // Draw a box with a notch at the bottom.
        int longOffset  = halfWidth - notchLongOffset;
        int shortOffset = m_height - notchShortOffset;
        nestedBoxes(longOffset, shortOffset, longOffset, noOffset, m_bgColor, m_fgColor);
        break;
    }

    case HorizontalScrollThumbType:
        // Draw a narrower box on top of the outside box.
        nestedBoxes(thumbLongIndent, thumbShortIndent, thumbLongIndent, thumbShortIndent, m_bgColor, m_bgColor);
        break;

    case VerticalScrollThumbType:
        // Draw a shorter box on top of the outside box.
        nestedBoxes(thumbShortIndent, thumbLongIndent, thumbShortIndent, thumbLongIndent, m_bgColor, m_bgColor);
        break;

    case HorizontalSliderThumbType:
    case VerticalSliderThumbType:
        // Slider thumbs are ovals.
        oval(m_bgColor);
        break;

    case HorizontalScrollGripType: {
        // Draw a horizontal crosshatch for the grip.
        int longOffset = halfWidth - gripLongIndent;
        line(m_left + gripLongIndent, m_top + halfHeight, m_right - gripLongIndent, m_top + halfHeight, m_fgColor);
        line(m_left + longOffset, m_top + gripShortIndent, m_left + longOffset, m_bottom - gripShortIndent, m_fgColor);
        line(m_right - longOffset, m_top + gripShortIndent, m_right - longOffset, m_bottom - gripShortIndent, m_fgColor);
        break;
    }

    case VerticalScrollGripType: {
        // Draw a vertical crosshatch for the grip.
        int longOffset = halfHeight - gripLongIndent;
        line(m_left + halfWidth, m_top + gripLongIndent, m_left + halfWidth, m_bottom - gripLongIndent, m_fgColor);
        line(m_left + gripShortIndent, m_top + longOffset, m_right - gripShortIndent, m_top + longOffset, m_fgColor);
        line(m_left + gripShortIndent, m_bottom - longOffset, m_right - gripShortIndent, m_bottom - longOffset, m_fgColor);
        break;
    }

    case LeftArrowType:
        // Draw a left arrow inside a box.
        box(m_irect, m_bgColor);
        triangle(m_right - quarterWidth, m_top + quarterHeight, m_right - quarterWidth, m_bottom - quarterHeight, m_left + quarterWidth, m_top + halfHeight, m_fgColor);
        break;

    case RightArrowType:
        // Draw a left arrow inside a box.
        box(m_irect, m_bgColor);
        triangle(m_left + quarterWidth, m_top + quarterHeight, m_right - quarterWidth, m_top + halfHeight, m_left + quarterWidth, m_bottom - quarterHeight, m_fgColor);
        break;

    case UpArrowType:
        // Draw an up arrow inside a box.
        box(m_irect, m_bgColor);
        triangle(m_left + quarterWidth, m_bottom - quarterHeight, m_left + halfWidth, m_top + quarterHeight, m_right - quarterWidth, m_bottom - quarterHeight, m_fgColor);
        break;

    case DownArrowType:
        // Draw a down arrow inside a box.
        box(m_irect, m_bgColor);
        triangle(m_left + quarterWidth, m_top + quarterHeight, m_right - quarterWidth, m_top + quarterHeight, m_left + halfWidth, m_bottom - quarterHeight, m_fgColor);
        break;

    case HorizontalSliderTrackType: {
        // Draw a narrow rect for the track plus box hatches on the ends.
        SkIRect lirect;
        lirect = m_irect;
        lirect.inset(noOffset, halfHeight - sliderIndent);
        box(lirect, m_bgColor);
        line(m_left,  m_top, m_left,  m_bottom, m_edgeColor);
        line(m_right, m_top, m_right, m_bottom, m_edgeColor);
        break;
    }

    case VerticalSliderTrackType: {
        // Draw a narrow rect for the track plus box hatches on the ends.
        SkIRect lirect;
        lirect = m_irect;
        lirect.inset(halfWidth - sliderIndent, noOffset);
        box(lirect, m_bgColor);
        line(m_left, m_top, m_right, m_top, m_edgeColor);
        line(m_left, m_bottom, m_right, m_bottom, m_edgeColor);
        break;
    }

    case DropDownButtonType:
        // Draw a box with a big down arrow on top.
        box(m_irect, m_bgColor);
        triangle(m_left + quarterWidth, m_top, m_right - quarterWidth, m_top, m_left + halfWidth, m_bottom, m_fgColor);
        break;

    default:
        WEBKIT_ASSERT_NOT_REACHED();
        break;
    }

    markState();
}

// Because rendering a text field is dependent on input
// parameters the other controls don't have, we render it directly
// rather than trying to overcomplicate draw() further.
void WebTestThemeControlWin::drawTextField(bool drawEdges, bool fillContentArea, SkColor color)
{
    SkPaint paint;

    if (fillContentArea) {
        paint.setColor(color);
        paint.setStyle(SkPaint::kFill_Style);
        m_canvas->drawIRect(m_irect, paint);
    }
    if (drawEdges) {
        paint.setColor(m_edgeColor);
        paint.setStyle(SkPaint::kStroke_Style);
        m_canvas->drawIRect(m_irect, paint);
    }

    markState();
}

void WebTestThemeControlWin::drawProgressBar(const SkIRect& fillRect)
{
    SkPaint paint;

    paint.setColor(m_bgColor);
    paint.setStyle(SkPaint::kFill_Style);
    m_canvas->drawIRect(m_irect, paint);

    // Emulate clipping
    SkIRect tofill;
    tofill.intersect(m_irect, fillRect);
    paint.setColor(m_fgColor);
    paint.setStyle(SkPaint::kFill_Style);
    m_canvas->drawIRect(tofill, paint);

    markState();
}

}
