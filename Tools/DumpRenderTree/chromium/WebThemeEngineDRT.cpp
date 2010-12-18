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

#include "config.h"
#include "WebThemeEngineDRT.h"

#include "WebRect.h"
#include "WebThemeControlDRT.h"
#include "third_party/skia/include/core/SkRect.h"

// Although all this code is generic, we include these headers
// to pull in the Windows #defines for the parts and states of
// the controls.
#include <vsstyle.h>
#include <windows.h>

#include <wtf/Assertions.h>

using namespace WebKit;

// We define this for clarity, although there really should be a DFCS_NORMAL in winuser.h.
static const int dfcsNormal = 0x0000;

static SkIRect webRectToSkIRect(const WebRect& webRect)
{
    SkIRect irect;
    irect.set(webRect.x, webRect.y, webRect.x + webRect.width, webRect.y + webRect.height);
    return irect;
}

static void drawControl(WebCanvas* canvas,
                        const WebRect& rect,
                        WebThemeControlDRT::Type ctype,
                        WebThemeControlDRT::State cstate)
{
    WebThemeControlDRT control(canvas, webRectToSkIRect(rect), ctype, cstate);
    control.draw();
}

static void drawTextField(WebCanvas* canvas,
                          const WebRect& rect,
                          WebThemeControlDRT::Type ctype,
                          WebThemeControlDRT::State cstate,
                          bool drawEdges,
                          bool fillContentArea,
                          WebColor color)
{
    WebThemeControlDRT control(canvas, webRectToSkIRect(rect), ctype, cstate);
    control.drawTextField(drawEdges, fillContentArea, color);
}

static void drawProgressBar(WebCanvas* canvas,
                            WebThemeControlDRT::Type ctype,
                            WebThemeControlDRT::State cstate,
                            const WebRect& barRect,
                            const WebRect& fillRect)
{
    WebThemeControlDRT control(canvas, webRectToSkIRect(barRect), ctype, cstate);
    control.drawProgressBar(webRectToSkIRect(fillRect));
}

// WebThemeEngineDRT

void WebThemeEngineDRT::paintButton(WebCanvas* canvas,
                                    int part,
                                    int state,
                                    int classicState,
                                    const WebRect& rect)
{
    WebThemeControlDRT::Type ctype = WebThemeControlDRT::UnknownType;
    WebThemeControlDRT::State cstate = WebThemeControlDRT::UnknownState;

    if (part == BP_CHECKBOX) {
        switch (state) {
        case CBS_UNCHECKEDNORMAL:
            ASSERT(classicState == dfcsNormal);
            ctype = WebThemeControlDRT::UncheckedBoxType;
            cstate = WebThemeControlDRT::NormalState;
            break;

        case CBS_UNCHECKEDHOT:
            ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_HOT));
            ctype = WebThemeControlDRT::UncheckedBoxType;
            cstate = WebThemeControlDRT::HotState;
            break;

        case CBS_UNCHECKEDPRESSED:
            ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_PUSHED));
            ctype = WebThemeControlDRT::UncheckedBoxType;
            cstate = WebThemeControlDRT::PressedState;
            break;

        case CBS_UNCHECKEDDISABLED:
            ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_INACTIVE));
            ctype = WebThemeControlDRT::UncheckedBoxType;
            cstate = WebThemeControlDRT::DisabledState;
            break;

        case CBS_CHECKEDNORMAL:
            ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_CHECKED));
            ctype = WebThemeControlDRT::CheckedBoxType;
            cstate = WebThemeControlDRT::NormalState;
            break;

        case CBS_CHECKEDHOT:
            ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_CHECKED | DFCS_HOT));
            ctype = WebThemeControlDRT::CheckedBoxType;
            cstate = WebThemeControlDRT::HotState;
            break;

        case CBS_CHECKEDPRESSED:
            ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_CHECKED | DFCS_PUSHED));
            ctype = WebThemeControlDRT::CheckedBoxType;
            cstate = WebThemeControlDRT::PressedState;
            break;

        case CBS_CHECKEDDISABLED:
            ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_CHECKED | DFCS_INACTIVE));
            ctype = WebThemeControlDRT::CheckedBoxType;
            cstate = WebThemeControlDRT::DisabledState;
            break;

        case CBS_MIXEDNORMAL:
            // Classic theme can't represent mixed state checkbox. We assume
            // it's equivalent to unchecked.
            ASSERT(classicState == DFCS_BUTTONCHECK);
            ctype = WebThemeControlDRT::IndeterminateCheckboxType;
            cstate = WebThemeControlDRT::NormalState;
            break;

        case CBS_MIXEDHOT:
            ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_HOT));
            ctype = WebThemeControlDRT::IndeterminateCheckboxType;
            cstate = WebThemeControlDRT::HotState;
            break;

        case CBS_MIXEDPRESSED:
            ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_PUSHED));
            ctype = WebThemeControlDRT::IndeterminateCheckboxType;
            cstate = WebThemeControlDRT::PressedState;
            break;

        case CBS_MIXEDDISABLED:
            ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_INACTIVE));
            ctype = WebThemeControlDRT::IndeterminateCheckboxType;
            cstate = WebThemeControlDRT::DisabledState;
            break;

        default:
            ASSERT_NOT_REACHED();
            break;
        }
    } else if (BP_RADIOBUTTON == part) {
        switch (state) {
        case RBS_UNCHECKEDNORMAL:
            ASSERT(classicState == DFCS_BUTTONRADIO);
            ctype = WebThemeControlDRT::UncheckedRadioType;
            cstate = WebThemeControlDRT::NormalState;
            break;

        case RBS_UNCHECKEDHOT:
            ASSERT(classicState == (DFCS_BUTTONRADIO | DFCS_HOT));
            ctype = WebThemeControlDRT::UncheckedRadioType;
            cstate = WebThemeControlDRT::HotState;
            break;

        case RBS_UNCHECKEDPRESSED:
            ASSERT(classicState == (DFCS_BUTTONRADIO | DFCS_PUSHED));
            ctype = WebThemeControlDRT::UncheckedRadioType;
            cstate = WebThemeControlDRT::PressedState;
            break;

        case RBS_UNCHECKEDDISABLED:
            ASSERT(classicState == (DFCS_BUTTONRADIO | DFCS_INACTIVE));
            ctype = WebThemeControlDRT::UncheckedRadioType;
            cstate = WebThemeControlDRT::DisabledState;
            break;

        case RBS_CHECKEDNORMAL:
            ASSERT(classicState == (DFCS_BUTTONRADIO | DFCS_CHECKED));
            ctype = WebThemeControlDRT::CheckedRadioType;
            cstate = WebThemeControlDRT::NormalState;
            break;

        case RBS_CHECKEDHOT:
            ASSERT(classicState == (DFCS_BUTTONRADIO | DFCS_CHECKED | DFCS_HOT));
            ctype = WebThemeControlDRT::CheckedRadioType;
            cstate = WebThemeControlDRT::HotState;
            break;

        case RBS_CHECKEDPRESSED:
            ASSERT(classicState == (DFCS_BUTTONRADIO | DFCS_CHECKED | DFCS_PUSHED));
            ctype = WebThemeControlDRT::CheckedRadioType;
            cstate = WebThemeControlDRT::PressedState;
            break;

        case RBS_CHECKEDDISABLED:
            ASSERT(classicState == (DFCS_BUTTONRADIO | DFCS_CHECKED | DFCS_INACTIVE));
            ctype = WebThemeControlDRT::CheckedRadioType;
            cstate = WebThemeControlDRT::DisabledState;
            break;

        default:
            ASSERT_NOT_REACHED();
            break;
        }
    } else if (BP_PUSHBUTTON == part) {
        switch (state) {
        case PBS_NORMAL:
            ASSERT(classicState == DFCS_BUTTONPUSH);
            ctype = WebThemeControlDRT::PushButtonType;
            cstate = WebThemeControlDRT::NormalState;
            break;

        case PBS_HOT:
            ASSERT(classicState == (DFCS_BUTTONPUSH | DFCS_HOT));
            ctype = WebThemeControlDRT::PushButtonType;
            cstate = WebThemeControlDRT::HotState;
            break;

        case PBS_PRESSED:
            ASSERT(classicState == (DFCS_BUTTONPUSH | DFCS_PUSHED));
            ctype = WebThemeControlDRT::PushButtonType;
            cstate = WebThemeControlDRT::PressedState;
            break;

        case PBS_DISABLED:
            ASSERT(classicState == (DFCS_BUTTONPUSH | DFCS_INACTIVE));
            ctype = WebThemeControlDRT::PushButtonType;
            cstate = WebThemeControlDRT::DisabledState;
            break;

        case PBS_DEFAULTED:
            ASSERT(classicState == DFCS_BUTTONPUSH);
            ctype = WebThemeControlDRT::PushButtonType;
            cstate = WebThemeControlDRT::FocusedState;
            break;

        default:
            ASSERT_NOT_REACHED();
            break;
        }
    } else {
        ASSERT_NOT_REACHED();
    }

    drawControl(canvas, rect, ctype, cstate);
}


void WebThemeEngineDRT::paintMenuList(WebCanvas* canvas,
                                      int part,
                                      int state,
                                      int classicState,
                                      const WebRect& rect)
{
    WebThemeControlDRT::Type ctype = WebThemeControlDRT::UnknownType;
    WebThemeControlDRT::State cstate = WebThemeControlDRT::UnknownState;

    if (CP_DROPDOWNBUTTON == part) {
        ctype = WebThemeControlDRT::DropDownButtonType;
        switch (state) {
        case CBXS_NORMAL:
            ASSERT(classicState == DFCS_MENUARROW);
            cstate = WebThemeControlDRT::NormalState;
            break;

        case CBXS_HOT:
            ASSERT(classicState == (DFCS_MENUARROW | DFCS_HOT));
            cstate = WebThemeControlDRT::HoverState;
            break;

        case CBXS_PRESSED:
            ASSERT(classicState == (DFCS_MENUARROW | DFCS_PUSHED));
            cstate = WebThemeControlDRT::PressedState;
            break;

        case CBXS_DISABLED:
            ASSERT(classicState == (DFCS_MENUARROW | DFCS_INACTIVE));
            cstate = WebThemeControlDRT::DisabledState;
            break;

        default:
            CRASH();
            break;
        }
    } else {
        CRASH();
    }

    drawControl(canvas, rect, ctype, cstate);
}

void WebThemeEngineDRT::paintScrollbarArrow(WebCanvas* canvas,
                                            int state,
                                            int classicState,
                                            const WebRect& rect)
{
    WebThemeControlDRT::Type ctype = WebThemeControlDRT::UnknownType;
    WebThemeControlDRT::State cstate = WebThemeControlDRT::UnknownState;

    switch (state) {
    case ABS_UPNORMAL:
        ASSERT(classicState == DFCS_SCROLLUP);
        ctype = WebThemeControlDRT::UpArrowType;
        cstate = WebThemeControlDRT::NormalState;
        break;

    case ABS_DOWNNORMAL:
        ASSERT(classicState == DFCS_SCROLLDOWN);
        ctype = WebThemeControlDRT::DownArrowType;
        cstate = WebThemeControlDRT::NormalState;
        break;

    case ABS_LEFTNORMAL:
        ASSERT(classicState == DFCS_SCROLLLEFT);
        ctype = WebThemeControlDRT::LeftArrowType;
        cstate = WebThemeControlDRT::NormalState;
        break;

    case ABS_RIGHTNORMAL:
        ASSERT(classicState == DFCS_SCROLLRIGHT);
        ctype = WebThemeControlDRT::RightArrowType;
        cstate = WebThemeControlDRT::NormalState;
        break;

    case ABS_UPHOT:
        ASSERT(classicState == (DFCS_SCROLLUP | DFCS_HOT));
        ctype = WebThemeControlDRT::UpArrowType;
        cstate = WebThemeControlDRT::HotState;
        break;

    case ABS_DOWNHOT:
        ASSERT(classicState == (DFCS_SCROLLDOWN | DFCS_HOT));
        ctype = WebThemeControlDRT::DownArrowType;
        cstate = WebThemeControlDRT::HotState;
        break;

    case ABS_LEFTHOT:
        ASSERT(classicState == (DFCS_SCROLLLEFT | DFCS_HOT));
        ctype = WebThemeControlDRT::LeftArrowType;
        cstate = WebThemeControlDRT::HotState;
        break;

    case ABS_RIGHTHOT:
        ASSERT(classicState == (DFCS_SCROLLRIGHT | DFCS_HOT));
        ctype = WebThemeControlDRT::RightArrowType;
        cstate = WebThemeControlDRT::HotState;
        break;

    case ABS_UPHOVER:
        ASSERT(classicState == DFCS_SCROLLUP);
        ctype = WebThemeControlDRT::UpArrowType;
        cstate = WebThemeControlDRT::HoverState;
        break;

    case ABS_DOWNHOVER:
        ASSERT(classicState == DFCS_SCROLLDOWN);
        ctype = WebThemeControlDRT::DownArrowType;
        cstate = WebThemeControlDRT::HoverState;
        break;

    case ABS_LEFTHOVER:
        ASSERT(classicState == DFCS_SCROLLLEFT);
        ctype = WebThemeControlDRT::LeftArrowType;
        cstate = WebThemeControlDRT::HoverState;
        break;

    case ABS_RIGHTHOVER:
        ASSERT(classicState == DFCS_SCROLLRIGHT);
        ctype = WebThemeControlDRT::RightArrowType;
        cstate = WebThemeControlDRT::HoverState;
        break;

    case ABS_UPPRESSED:
        ASSERT(classicState == (DFCS_SCROLLUP | DFCS_PUSHED | DFCS_FLAT));
        ctype = WebThemeControlDRT::UpArrowType;
        cstate = WebThemeControlDRT::PressedState;
        break;

    case ABS_DOWNPRESSED:
        ASSERT(classicState == (DFCS_SCROLLDOWN | DFCS_PUSHED | DFCS_FLAT));
        ctype = WebThemeControlDRT::DownArrowType;
        cstate = WebThemeControlDRT::PressedState;
        break;

    case ABS_LEFTPRESSED:
        ASSERT(classicState == (DFCS_SCROLLLEFT | DFCS_PUSHED | DFCS_FLAT));
        ctype = WebThemeControlDRT::LeftArrowType;
        cstate = WebThemeControlDRT::PressedState;
        break;

    case ABS_RIGHTPRESSED:
        ASSERT(classicState == (DFCS_SCROLLRIGHT | DFCS_PUSHED | DFCS_FLAT));
        ctype = WebThemeControlDRT::RightArrowType;
        cstate = WebThemeControlDRT::PressedState;
        break;

    case ABS_UPDISABLED:
        ASSERT(classicState == (DFCS_SCROLLUP | DFCS_INACTIVE));
        ctype = WebThemeControlDRT::UpArrowType;
        cstate = WebThemeControlDRT::DisabledState;
        break;

    case ABS_DOWNDISABLED:
        ASSERT(classicState == (DFCS_SCROLLDOWN | DFCS_INACTIVE));
        ctype = WebThemeControlDRT::DownArrowType;
        cstate = WebThemeControlDRT::DisabledState;
        break;

    case ABS_LEFTDISABLED:
        ASSERT(classicState == (DFCS_SCROLLLEFT | DFCS_INACTIVE));
        ctype = WebThemeControlDRT::LeftArrowType;
        cstate = WebThemeControlDRT::DisabledState;
        break;

    case ABS_RIGHTDISABLED:
        ASSERT(classicState == (DFCS_SCROLLRIGHT | DFCS_INACTIVE));
        ctype = WebThemeControlDRT::RightArrowType;
        cstate = WebThemeControlDRT::DisabledState;
        break;

    default:
        ASSERT_NOT_REACHED();
        break;
    }

    drawControl(canvas, rect, ctype, cstate);
}

void WebThemeEngineDRT::paintScrollbarThumb(WebCanvas* canvas,
                                            int part,
                                            int state,
                                            int classicState,
                                            const WebRect& rect)
{
    WebThemeControlDRT::Type ctype = WebThemeControlDRT::UnknownType;
    WebThemeControlDRT::State cstate = WebThemeControlDRT::UnknownState;

    switch (part) {
    case SBP_THUMBBTNHORZ:
        ctype = WebThemeControlDRT::HorizontalScrollThumbType;
        break;

    case SBP_THUMBBTNVERT:
        ctype = WebThemeControlDRT::VerticalScrollThumbType;
        break;

    case SBP_GRIPPERHORZ:
        ctype = WebThemeControlDRT::HorizontalScrollGripType;
        break;

    case SBP_GRIPPERVERT:
        ctype = WebThemeControlDRT::VerticalScrollGripType;
        break;

    default:
        ASSERT_NOT_REACHED();
        break;
    }

    switch (state) {
    case SCRBS_NORMAL:
        ASSERT(classicState == dfcsNormal);
        cstate = WebThemeControlDRT::NormalState;
        break;

    case SCRBS_HOT:
        ASSERT(classicState == DFCS_HOT);
        cstate = WebThemeControlDRT::HotState;
        break;

    case SCRBS_HOVER:
        ASSERT(classicState == dfcsNormal);
        cstate = WebThemeControlDRT::HoverState;
        break;

    case SCRBS_PRESSED:
        ASSERT(classicState == dfcsNormal);
        cstate = WebThemeControlDRT::PressedState;
        break;

    case SCRBS_DISABLED:
        ASSERT_NOT_REACHED(); // This should never happen in practice.
        break;

    default:
        ASSERT_NOT_REACHED();
        break;
    }

    drawControl(canvas, rect, ctype, cstate);
}

void WebThemeEngineDRT::paintScrollbarTrack(WebCanvas* canvas,
                                            int part,
                                            int state,
                                            int classicState,
                                            const WebRect& rect,
                                            const WebRect& alignRect)
{
    WebThemeControlDRT::Type ctype = WebThemeControlDRT::UnknownType;
    WebThemeControlDRT::State cstate = WebThemeControlDRT::UnknownState;

    switch (part) {
    case SBP_UPPERTRACKHORZ:
        ctype = WebThemeControlDRT::HorizontalScrollTrackBackType;
        break;

    case SBP_LOWERTRACKHORZ:
        ctype = WebThemeControlDRT::HorizontalScrollTrackForwardType;
        break;

    case SBP_UPPERTRACKVERT:
        ctype = WebThemeControlDRT::VerticalScrollTrackBackType;
        break;

    case SBP_LOWERTRACKVERT:
        ctype = WebThemeControlDRT::VerticalScrollTrackForwardType;
        break;

    default:
        ASSERT_NOT_REACHED();
        break;
    }

    switch (state) {
    case SCRBS_NORMAL:
        ASSERT(classicState == dfcsNormal);
        cstate = WebThemeControlDRT::NormalState;
        break;

    case SCRBS_HOT:
        ASSERT_NOT_REACHED(); // This should never happen in practice.
        break;

    case SCRBS_HOVER:
        ASSERT(classicState == dfcsNormal);
        cstate = WebThemeControlDRT::HoverState;
        break;

    case SCRBS_PRESSED:
        ASSERT_NOT_REACHED(); // This should never happen in practice.
        break;

    case SCRBS_DISABLED:
        ASSERT(classicState == DFCS_INACTIVE);
        cstate = WebThemeControlDRT::DisabledState;
        break;

    default:
        CRASH();
        break;
    }

    drawControl(canvas, rect, ctype, cstate);
}

void WebThemeEngineDRT::paintSpinButton(WebCanvas* canvas,
                                        int part,
                                        int state,
                                        int classicState,
                                        const WebRect& rect)
{
    WebThemeControlDRT::Type ctype = WebThemeControlDRT::UnknownType;
    WebThemeControlDRT::State cstate = WebThemeControlDRT::UnknownState;

    if (part == SPNP_UP) {
        ctype = WebThemeControlDRT::UpArrowType;
        switch (state) {
        case UPS_NORMAL:
            ASSERT(classicState == DFCS_SCROLLUP);
            cstate = WebThemeControlDRT::NormalState;
            break;
        case UPS_DISABLED:
            ASSERT(classicState == (DFCS_SCROLLUP | DFCS_INACTIVE));
            cstate = WebThemeControlDRT::DisabledState;
            break;
        case UPS_PRESSED:
            ASSERT(classicState == (DFCS_SCROLLUP | DFCS_PUSHED));
            cstate = WebThemeControlDRT::PressedState;
            break;
        case UPS_HOT:
            ASSERT(classicState == (DFCS_SCROLLUP | DFCS_HOT));
            cstate = WebThemeControlDRT::HoverState;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    } else if (part == SPNP_DOWN) {
        ctype = WebThemeControlDRT::DownArrowType;
        switch (state) {
        case DNS_NORMAL:
            ASSERT(classicState == DFCS_SCROLLDOWN);
            cstate = WebThemeControlDRT::NormalState;
            break;
        case DNS_DISABLED:
            ASSERT(classicState == (DFCS_SCROLLDOWN | DFCS_INACTIVE));
            cstate = WebThemeControlDRT::DisabledState;
            break;
        case DNS_PRESSED:
            ASSERT(classicState == (DFCS_SCROLLDOWN | DFCS_PUSHED));
            cstate = WebThemeControlDRT::PressedState;
            break;
        case DNS_HOT:
            ASSERT(classicState == (DFCS_SCROLLDOWN | DFCS_HOT));
            cstate = WebThemeControlDRT::HoverState;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    } else
        ASSERT_NOT_REACHED();
    drawControl(canvas, rect, ctype, cstate);
}

void WebThemeEngineDRT::paintTextField(WebCanvas* canvas,
                                       int part,
                                       int state,
                                       int classicState,
                                       const WebRect& rect,
                                       WebColor color,
                                       bool fillContentArea,
                                       bool drawEdges)
{
    WebThemeControlDRT::Type ctype = WebThemeControlDRT::UnknownType;
    WebThemeControlDRT::State cstate = WebThemeControlDRT::UnknownState;

    ASSERT(EP_EDITTEXT == part);
    ctype = WebThemeControlDRT::TextFieldType;

    switch (state) {
    case ETS_NORMAL:
        ASSERT(classicState == dfcsNormal);
        cstate = WebThemeControlDRT::NormalState;
        break;

    case ETS_HOT:
        ASSERT(classicState == DFCS_HOT);
        cstate = WebThemeControlDRT::HotState;
        break;

    case ETS_DISABLED:
        ASSERT(classicState == DFCS_INACTIVE);
        cstate = WebThemeControlDRT::DisabledState;
        break;

    case ETS_SELECTED:
        ASSERT(classicState == DFCS_PUSHED);
        cstate = WebThemeControlDRT::PressedState;
        break;

    case ETS_FOCUSED:
        ASSERT(classicState == dfcsNormal);
        cstate = WebThemeControlDRT::FocusedState;
        break;

    case ETS_READONLY:
        ASSERT(classicState == dfcsNormal);
        cstate = WebThemeControlDRT::ReadOnlyState;
        break;

    default:
        ASSERT_NOT_REACHED();
        break;
    }

    drawTextField(canvas, rect, ctype, cstate, drawEdges, fillContentArea, color);
}

void WebThemeEngineDRT::paintTrackbar(WebCanvas* canvas,
                                      int part,
                                      int state,
                                      int classicState,
                                      const WebRect& rect)
{
    WebThemeControlDRT::Type ctype = WebThemeControlDRT::UnknownType;
    WebThemeControlDRT::State cstate = WebThemeControlDRT::UnknownState;

    if (TKP_THUMBBOTTOM == part) {
        ctype = WebThemeControlDRT::HorizontalSliderThumbType;
        switch (state) {
        case TUS_NORMAL:
            ASSERT(classicState == dfcsNormal);
            cstate = WebThemeControlDRT::NormalState;
            break;

        case TUS_HOT:
            ASSERT(classicState == DFCS_HOT);
            cstate = WebThemeControlDRT::HotState;
            break;

        case TUS_DISABLED:
            ASSERT(classicState == DFCS_INACTIVE);
            cstate = WebThemeControlDRT::DisabledState;
            break;

        case TUS_PRESSED:
            ASSERT(classicState == DFCS_PUSHED);
            cstate = WebThemeControlDRT::PressedState;
            break;

        default:
            ASSERT_NOT_REACHED();
            break;
        }
    } else if (TKP_TRACK == part) {
        ctype = WebThemeControlDRT::HorizontalSliderTrackType;
        ASSERT(part == TUS_NORMAL);
        ASSERT(classicState == dfcsNormal);
        cstate = WebThemeControlDRT::NormalState;
    } else {
        ASSERT_NOT_REACHED();
    }

    drawControl(canvas, rect, ctype, cstate);
}


void WebThemeEngineDRT::paintProgressBar(WebKit::WebCanvas* canvas,
                                         const WebKit::WebRect& barRect,
                                         const WebKit::WebRect& valueRect,
                                         bool determinate,
                                         double)
{
    WebThemeControlDRT::Type ctype = WebThemeControlDRT::ProgressBarType;
    WebThemeControlDRT::State cstate = determinate ? WebThemeControlDRT::NormalState 
                                                   : WebThemeControlDRT::IndeterminateState;
    drawProgressBar(canvas, ctype, cstate, barRect, valueRect);
}

