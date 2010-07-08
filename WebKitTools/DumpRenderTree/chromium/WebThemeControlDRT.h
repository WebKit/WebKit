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

// WebThemeControlDRT implements the generic rendering of controls
// needed by WebThemeEngineDRT. See the comments in that class
// header file for why this class is needed and used.
//
// This class implements a generic set of widgets using Skia. The widgets
// are optimized for testability, not a pleasing appearance.
//

#ifndef WebThemeControlDRT_h
#define WebThemeControlDRT_h

#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include <wtf/Noncopyable.h>

// Skia forward declarations
struct SkIRect;

class WebThemeControlDRT : public Noncopyable {
public:
    // This list of states mostly mirrors the list in WebCore/platform/ThemeTypes.h
    // but is maintained separately since that isn't public and also to minimize
    // dependencies.
    // Note that the WebKit ThemeTypes seem to imply that a control can be
    // in multiple states simultaneously but WebThemeEngine only allows for
    // a single state at a time.
    //
    // Some definitions for the various states:
    //   Disabled - indicates that a control can't be modified or selected
    //              (corresponds to HTML 'disabled' attribute)
    //   ReadOnly - indicates that a control can't be modified but can be
    //              selected
    //   Normal   - the normal state of control on the page when it isn't
    //              focused or otherwise active
    //   Hot      - when the mouse is hovering over a part of the control,
    //              all the other parts are considered "hot"
    //   Hover    - when the mouse is directly over a control (the CSS
    //               :hover pseudo-class)
    //   Focused  - when the control has the keyboard focus
    //   Pressed  - when the control is being triggered (by a mousedown or
    //              a key event).
    //   Indeterminate - when set to indeterminate (only for progress bar)
    enum State {
        UnknownState = 0,
        DisabledState,
        ReadOnlyState,
        NormalState,
        HotState,
        HoverState,
        FocusedState,
        PressedState,
        IndeterminateState
    };

    // This list of types mostly mirrors the list in
    // WebCore/platform/ThemeTypes.h but is maintained
    // separately since that isn't public and also to minimize dependencies.
    //
    // Note that what the user might think of as a single control can be
    // made up of multiple parts. For example, a single scroll bar contains
    // six clickable parts - two arrows, the "thumb" indicating the current
    // position on the bar, the other two parts of the bar (before and after
    // the thumb) and the "gripper" on the thumb itself.
    //
    enum Type {
        UnknownType = 0,
        TextFieldType,
        PushButtonType,
        UncheckedBoxType,
        CheckedBoxType,
        IndeterminateCheckboxType,
        UncheckedRadioType,
        CheckedRadioType,
        HorizontalScrollTrackBackType,
        HorizontalScrollTrackForwardType,
        HorizontalScrollThumbType,
        HorizontalScrollGripType,
        VerticalScrollTrackBackType,
        VerticalScrollTrackForwardType,
        VerticalScrollThumbType,
        VerticalScrollGripType,
        LeftArrowType,
        RightArrowType,
        UpArrowType,
        DownArrowType,
        HorizontalSliderTrackType,
        HorizontalSliderThumbType,
        DropDownButtonType,
        ProgressBarType
    };

    // canvas is the canvas to draw onto, and rect gives the size of the
    // control. ctype and cstate specify the type and state of the control.
    WebThemeControlDRT(skia::PlatformCanvas* canvas,
                       const SkIRect& rect,
                       Type ctype,
                       State cstate);
    ~WebThemeControlDRT();

    // Draws the control.
    void draw();

    // Use this for TextField controls instead, because the logic
    // for drawing them is dependent on what WebKit tells us to do.
    // If drawEdges is true, draw an edge around the control. If
    // fillContentArea is true, fill the content area with the given color.
    void drawTextField(bool drawEdges, bool fillContentArea, SkColor color);

    // Use this for drawing ProgressBar controls instead, since we
    // need to know the rect to fill inside the bar.
    void drawProgressBar(const SkIRect& fillRect);

private:
    // Draws a box of size specified by irect, filled with the given color.
    // The box will have a border drawn in the default edge color.
    void box(const SkIRect& irect, SkColor color);


    // Draws a triangle of size specified by the three pairs of coordinates,
    // filled with the given color. The box will have an edge drawn in the
    // default edge color.
    void triangle(int x0, int y0, int x1, int y1, int x2, int y2, SkColor color);

    // Draws a rectangle the size of the control with rounded corners, filled
    // with the specified color (and with a border in the default edge color).
    void roundRect(SkColor color);

    // Draws an oval the size of the control, filled with the specified color
    // and with a border in the default edge color.
    void oval(SkColor color);

    // Draws a circle centered in the control with the specified radius,
    // filled with the specified color, and with a border draw in the
    // default edge color.
    void circle(SkScalar radius, SkColor color);

    // Draws a box the size of the control, filled with the outerColor and
    // with a border in the default edge color, and then draws another box
    // indented on all four sides by the specified amounts, filled with the
    // inner color and with a border in the default edge color.
    void nestedBoxes(int indentLeft,
                     int indentTop,
                     int indentRight,
                     int indentBottom,
                     SkColor outerColor,
                     SkColor innerColor);

    // Draws a line between the two points in the given color.
    void line(int x0, int y0, int x1, int y1, SkColor color);

    // Draws a distinctive mark on the control for each state, so that the
    // state of the control can be determined without needing to know which
    // color is which.
    void markState();

    skia::PlatformCanvas* m_canvas;
    const SkIRect m_irect;
    const Type m_type;
    const State m_state;
    const SkColor m_edgeColor;
    const SkColor m_bgColor;
    const SkColor m_fgColor;

    // The following are convenience accessors for m_irect.
    const int m_left;
    const int m_right;
    const int m_top;
    const int m_bottom;
    const int m_width;
    const int m_height;
};

#endif // WebThemeControlDRT_h
