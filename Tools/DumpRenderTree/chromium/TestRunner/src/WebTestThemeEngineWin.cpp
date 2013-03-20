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
#include "WebTestThemeEngineWin.h"

#include "TestCommon.h"
#include "WebTestThemeControlWin.h"
#include "third_party/skia/include/core/SkRect.h"
#include <public/WebRect.h>

// Although all this code is generic, we include these headers
// to pull in the Windows #defines for the parts and states of
// the controls.
#include <vsstyle.h>
#include <windows.h>

using namespace WebKit;

namespace WebTestRunner {

namespace {

// We define this for clarity, although there really should be a DFCS_NORMAL in winuser.h.
const int dfcsNormal = 0x0000;

SkIRect webRectToSkIRect(const WebRect& webRect)
{
    SkIRect irect;
    irect.set(webRect.x, webRect.y, webRect.x + webRect.width - 1, webRect.y + webRect.height - 1);
    return irect;
}

void drawControl(WebCanvas* canvas, const WebRect& rect, WebTestThemeControlWin::Type ctype, WebTestThemeControlWin::State cstate)
{
    WebTestThemeControlWin control(canvas, webRectToSkIRect(rect), ctype, cstate);
    control.draw();
}

void drawTextField(WebCanvas* canvas, const WebRect& rect, WebTestThemeControlWin::Type ctype, WebTestThemeControlWin::State cstate, bool drawEdges, bool fillContentArea, WebColor color)
{
    WebTestThemeControlWin control(canvas, webRectToSkIRect(rect), ctype, cstate);
    control.drawTextField(drawEdges, fillContentArea, color);
}

void drawProgressBar(WebCanvas* canvas, WebTestThemeControlWin::Type ctype, WebTestThemeControlWin::State cstate, const WebRect& barRect, const WebRect& fillRect)
{
    WebTestThemeControlWin control(canvas, webRectToSkIRect(barRect), ctype, cstate);
    control.drawProgressBar(webRectToSkIRect(fillRect));
}

}

void WebTestThemeEngineWin::paintButton(WebCanvas* canvas, int part, int state, int classicState, const WebRect& rect)
{
    WebTestThemeControlWin::Type ctype = WebTestThemeControlWin::UnknownType;
    WebTestThemeControlWin::State cstate = WebTestThemeControlWin::UnknownState;

    if (part == BP_CHECKBOX) {
        switch (state) {
        case CBS_UNCHECKEDNORMAL:
            WEBKIT_ASSERT(classicState == dfcsNormal);
            ctype = WebTestThemeControlWin::UncheckedBoxType;
            cstate = WebTestThemeControlWin::NormalState;
            break;

        case CBS_UNCHECKEDHOT:
            WEBKIT_ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_HOT));
            ctype = WebTestThemeControlWin::UncheckedBoxType;
            cstate = WebTestThemeControlWin::HotState;
            break;

        case CBS_UNCHECKEDPRESSED:
            WEBKIT_ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_PUSHED));
            ctype = WebTestThemeControlWin::UncheckedBoxType;
            cstate = WebTestThemeControlWin::PressedState;
            break;

        case CBS_UNCHECKEDDISABLED:
            WEBKIT_ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_INACTIVE));
            ctype = WebTestThemeControlWin::UncheckedBoxType;
            cstate = WebTestThemeControlWin::DisabledState;
            break;

        case CBS_CHECKEDNORMAL:
            WEBKIT_ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_CHECKED));
            ctype = WebTestThemeControlWin::CheckedBoxType;
            cstate = WebTestThemeControlWin::NormalState;
            break;

        case CBS_CHECKEDHOT:
            WEBKIT_ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_CHECKED | DFCS_HOT));
            ctype = WebTestThemeControlWin::CheckedBoxType;
            cstate = WebTestThemeControlWin::HotState;
            break;

        case CBS_CHECKEDPRESSED:
            WEBKIT_ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_CHECKED | DFCS_PUSHED));
            ctype = WebTestThemeControlWin::CheckedBoxType;
            cstate = WebTestThemeControlWin::PressedState;
            break;

        case CBS_CHECKEDDISABLED:
            WEBKIT_ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_CHECKED | DFCS_INACTIVE));
            ctype = WebTestThemeControlWin::CheckedBoxType;
            cstate = WebTestThemeControlWin::DisabledState;
            break;

        case CBS_MIXEDNORMAL:
            // Classic theme can't represent mixed state checkbox. We assume
            // it's equivalent to unchecked.
            WEBKIT_ASSERT(classicState == DFCS_BUTTONCHECK);
            ctype = WebTestThemeControlWin::IndeterminateCheckboxType;
            cstate = WebTestThemeControlWin::NormalState;
            break;

        case CBS_MIXEDHOT:
            WEBKIT_ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_HOT));
            ctype = WebTestThemeControlWin::IndeterminateCheckboxType;
            cstate = WebTestThemeControlWin::HotState;
            break;

        case CBS_MIXEDPRESSED:
            WEBKIT_ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_PUSHED));
            ctype = WebTestThemeControlWin::IndeterminateCheckboxType;
            cstate = WebTestThemeControlWin::PressedState;
            break;

        case CBS_MIXEDDISABLED:
            WEBKIT_ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_INACTIVE));
            ctype = WebTestThemeControlWin::IndeterminateCheckboxType;
            cstate = WebTestThemeControlWin::DisabledState;
            break;

        default:
            WEBKIT_ASSERT_NOT_REACHED();
            break;
        }
    } else if (BP_RADIOBUTTON == part) {
        switch (state) {
        case RBS_UNCHECKEDNORMAL:
            WEBKIT_ASSERT(classicState == DFCS_BUTTONRADIO);
            ctype = WebTestThemeControlWin::UncheckedRadioType;
            cstate = WebTestThemeControlWin::NormalState;
            break;

        case RBS_UNCHECKEDHOT:
            WEBKIT_ASSERT(classicState == (DFCS_BUTTONRADIO | DFCS_HOT));
            ctype = WebTestThemeControlWin::UncheckedRadioType;
            cstate = WebTestThemeControlWin::HotState;
            break;

        case RBS_UNCHECKEDPRESSED:
            WEBKIT_ASSERT(classicState == (DFCS_BUTTONRADIO | DFCS_PUSHED));
            ctype = WebTestThemeControlWin::UncheckedRadioType;
            cstate = WebTestThemeControlWin::PressedState;
            break;

        case RBS_UNCHECKEDDISABLED:
            WEBKIT_ASSERT(classicState == (DFCS_BUTTONRADIO | DFCS_INACTIVE));
            ctype = WebTestThemeControlWin::UncheckedRadioType;
            cstate = WebTestThemeControlWin::DisabledState;
            break;

        case RBS_CHECKEDNORMAL:
            WEBKIT_ASSERT(classicState == (DFCS_BUTTONRADIO | DFCS_CHECKED));
            ctype = WebTestThemeControlWin::CheckedRadioType;
            cstate = WebTestThemeControlWin::NormalState;
            break;

        case RBS_CHECKEDHOT:
            WEBKIT_ASSERT(classicState == (DFCS_BUTTONRADIO | DFCS_CHECKED | DFCS_HOT));
            ctype = WebTestThemeControlWin::CheckedRadioType;
            cstate = WebTestThemeControlWin::HotState;
            break;

        case RBS_CHECKEDPRESSED:
            WEBKIT_ASSERT(classicState == (DFCS_BUTTONRADIO | DFCS_CHECKED | DFCS_PUSHED));
            ctype = WebTestThemeControlWin::CheckedRadioType;
            cstate = WebTestThemeControlWin::PressedState;
            break;

        case RBS_CHECKEDDISABLED:
            WEBKIT_ASSERT(classicState == (DFCS_BUTTONRADIO | DFCS_CHECKED | DFCS_INACTIVE));
            ctype = WebTestThemeControlWin::CheckedRadioType;
            cstate = WebTestThemeControlWin::DisabledState;
            break;

        default:
            WEBKIT_ASSERT_NOT_REACHED();
            break;
        }
    } else if (BP_PUSHBUTTON == part) {
        switch (state) {
        case PBS_NORMAL:
            WEBKIT_ASSERT(classicState == DFCS_BUTTONPUSH);
            ctype = WebTestThemeControlWin::PushButtonType;
            cstate = WebTestThemeControlWin::NormalState;
            break;

        case PBS_HOT:
            WEBKIT_ASSERT(classicState == (DFCS_BUTTONPUSH | DFCS_HOT));
            ctype = WebTestThemeControlWin::PushButtonType;
            cstate = WebTestThemeControlWin::HotState;
            break;

        case PBS_PRESSED:
            WEBKIT_ASSERT(classicState == (DFCS_BUTTONPUSH | DFCS_PUSHED));
            ctype = WebTestThemeControlWin::PushButtonType;
            cstate = WebTestThemeControlWin::PressedState;
            break;

        case PBS_DISABLED:
            WEBKIT_ASSERT(classicState == (DFCS_BUTTONPUSH | DFCS_INACTIVE));
            ctype = WebTestThemeControlWin::PushButtonType;
            cstate = WebTestThemeControlWin::DisabledState;
            break;

        case PBS_DEFAULTED:
            WEBKIT_ASSERT(classicState == DFCS_BUTTONPUSH);
            ctype = WebTestThemeControlWin::PushButtonType;
            cstate = WebTestThemeControlWin::FocusedState;
            break;

        default:
            WEBKIT_ASSERT_NOT_REACHED();
            break;
        }
    } else
        WEBKIT_ASSERT_NOT_REACHED();

    drawControl(canvas, rect, ctype, cstate);
}

void WebTestThemeEngineWin::paintMenuList(WebCanvas* canvas, int part, int state, int classicState, const WebRect& rect)
{
    WebTestThemeControlWin::Type ctype = WebTestThemeControlWin::UnknownType;
    WebTestThemeControlWin::State cstate = WebTestThemeControlWin::UnknownState;

    if (CP_DROPDOWNBUTTON == part) {
        ctype = WebTestThemeControlWin::DropDownButtonType;
        switch (state) {
        case CBXS_NORMAL:
            WEBKIT_ASSERT(classicState == DFCS_MENUARROW);
            cstate = WebTestThemeControlWin::NormalState;
            break;

        case CBXS_HOT:
            WEBKIT_ASSERT(classicState == (DFCS_MENUARROW | DFCS_HOT));
            cstate = WebTestThemeControlWin::HoverState;
            break;

        case CBXS_PRESSED:
            WEBKIT_ASSERT(classicState == (DFCS_MENUARROW | DFCS_PUSHED));
            cstate = WebTestThemeControlWin::PressedState;
            break;

        case CBXS_DISABLED:
            WEBKIT_ASSERT(classicState == (DFCS_MENUARROW | DFCS_INACTIVE));
            cstate = WebTestThemeControlWin::DisabledState;
            break;

        default:
            WEBKIT_ASSERT_NOT_REACHED();
            break;
        }
    } else
        WEBKIT_ASSERT_NOT_REACHED();

    drawControl(canvas, rect, ctype, cstate);
}

void WebTestThemeEngineWin::paintScrollbarArrow(WebCanvas* canvas, int state, int classicState, const WebRect& rect)
{
    WebTestThemeControlWin::Type ctype = WebTestThemeControlWin::UnknownType;
    WebTestThemeControlWin::State cstate = WebTestThemeControlWin::UnknownState;

    switch (state) {
    case ABS_UPNORMAL:
        WEBKIT_ASSERT(classicState == DFCS_SCROLLUP);
        ctype = WebTestThemeControlWin::UpArrowType;
        cstate = WebTestThemeControlWin::NormalState;
        break;

    case ABS_DOWNNORMAL:
        WEBKIT_ASSERT(classicState == DFCS_SCROLLDOWN);
        ctype = WebTestThemeControlWin::DownArrowType;
        cstate = WebTestThemeControlWin::NormalState;
        break;

    case ABS_LEFTNORMAL:
        WEBKIT_ASSERT(classicState == DFCS_SCROLLLEFT);
        ctype = WebTestThemeControlWin::LeftArrowType;
        cstate = WebTestThemeControlWin::NormalState;
        break;

    case ABS_RIGHTNORMAL:
        WEBKIT_ASSERT(classicState == DFCS_SCROLLRIGHT);
        ctype = WebTestThemeControlWin::RightArrowType;
        cstate = WebTestThemeControlWin::NormalState;
        break;

    case ABS_UPHOT:
        WEBKIT_ASSERT(classicState == (DFCS_SCROLLUP | DFCS_HOT));
        ctype = WebTestThemeControlWin::UpArrowType;
        cstate = WebTestThemeControlWin::HotState;
        break;

    case ABS_DOWNHOT:
        WEBKIT_ASSERT(classicState == (DFCS_SCROLLDOWN | DFCS_HOT));
        ctype = WebTestThemeControlWin::DownArrowType;
        cstate = WebTestThemeControlWin::HotState;
        break;

    case ABS_LEFTHOT:
        WEBKIT_ASSERT(classicState == (DFCS_SCROLLLEFT | DFCS_HOT));
        ctype = WebTestThemeControlWin::LeftArrowType;
        cstate = WebTestThemeControlWin::HotState;
        break;

    case ABS_RIGHTHOT:
        WEBKIT_ASSERT(classicState == (DFCS_SCROLLRIGHT | DFCS_HOT));
        ctype = WebTestThemeControlWin::RightArrowType;
        cstate = WebTestThemeControlWin::HotState;
        break;

    case ABS_UPHOVER:
        WEBKIT_ASSERT(classicState == DFCS_SCROLLUP);
        ctype = WebTestThemeControlWin::UpArrowType;
        cstate = WebTestThemeControlWin::HoverState;
        break;

    case ABS_DOWNHOVER:
        WEBKIT_ASSERT(classicState == DFCS_SCROLLDOWN);
        ctype = WebTestThemeControlWin::DownArrowType;
        cstate = WebTestThemeControlWin::HoverState;
        break;

    case ABS_LEFTHOVER:
        WEBKIT_ASSERT(classicState == DFCS_SCROLLLEFT);
        ctype = WebTestThemeControlWin::LeftArrowType;
        cstate = WebTestThemeControlWin::HoverState;
        break;

    case ABS_RIGHTHOVER:
        WEBKIT_ASSERT(classicState == DFCS_SCROLLRIGHT);
        ctype = WebTestThemeControlWin::RightArrowType;
        cstate = WebTestThemeControlWin::HoverState;
        break;

    case ABS_UPPRESSED:
        WEBKIT_ASSERT(classicState == (DFCS_SCROLLUP | DFCS_PUSHED | DFCS_FLAT));
        ctype = WebTestThemeControlWin::UpArrowType;
        cstate = WebTestThemeControlWin::PressedState;
        break;

    case ABS_DOWNPRESSED:
        WEBKIT_ASSERT(classicState == (DFCS_SCROLLDOWN | DFCS_PUSHED | DFCS_FLAT));
        ctype = WebTestThemeControlWin::DownArrowType;
        cstate = WebTestThemeControlWin::PressedState;
        break;

    case ABS_LEFTPRESSED:
        WEBKIT_ASSERT(classicState == (DFCS_SCROLLLEFT | DFCS_PUSHED | DFCS_FLAT));
        ctype = WebTestThemeControlWin::LeftArrowType;
        cstate = WebTestThemeControlWin::PressedState;
        break;

    case ABS_RIGHTPRESSED:
        WEBKIT_ASSERT(classicState == (DFCS_SCROLLRIGHT | DFCS_PUSHED | DFCS_FLAT));
        ctype = WebTestThemeControlWin::RightArrowType;
        cstate = WebTestThemeControlWin::PressedState;
        break;

    case ABS_UPDISABLED:
        WEBKIT_ASSERT(classicState == (DFCS_SCROLLUP | DFCS_INACTIVE));
        ctype = WebTestThemeControlWin::UpArrowType;
        cstate = WebTestThemeControlWin::DisabledState;
        break;

    case ABS_DOWNDISABLED:
        WEBKIT_ASSERT(classicState == (DFCS_SCROLLDOWN | DFCS_INACTIVE));
        ctype = WebTestThemeControlWin::DownArrowType;
        cstate = WebTestThemeControlWin::DisabledState;
        break;

    case ABS_LEFTDISABLED:
        WEBKIT_ASSERT(classicState == (DFCS_SCROLLLEFT | DFCS_INACTIVE));
        ctype = WebTestThemeControlWin::LeftArrowType;
        cstate = WebTestThemeControlWin::DisabledState;
        break;

    case ABS_RIGHTDISABLED:
        WEBKIT_ASSERT(classicState == (DFCS_SCROLLRIGHT | DFCS_INACTIVE));
        ctype = WebTestThemeControlWin::RightArrowType;
        cstate = WebTestThemeControlWin::DisabledState;
        break;

    default:
        WEBKIT_ASSERT_NOT_REACHED();
        break;
    }

    drawControl(canvas, rect, ctype, cstate);
}

void WebTestThemeEngineWin::paintScrollbarThumb(WebCanvas* canvas, int part, int state, int classicState, const WebRect& rect)
{
    WebTestThemeControlWin::Type ctype = WebTestThemeControlWin::UnknownType;
    WebTestThemeControlWin::State cstate = WebTestThemeControlWin::UnknownState;

    switch (part) {
    case SBP_THUMBBTNHORZ:
        ctype = WebTestThemeControlWin::HorizontalScrollThumbType;
        break;

    case SBP_THUMBBTNVERT:
        ctype = WebTestThemeControlWin::VerticalScrollThumbType;
        break;

    case SBP_GRIPPERHORZ:
        ctype = WebTestThemeControlWin::HorizontalScrollGripType;
        break;

    case SBP_GRIPPERVERT:
        ctype = WebTestThemeControlWin::VerticalScrollGripType;
        break;

    default:
        WEBKIT_ASSERT_NOT_REACHED();
        break;
    }

    switch (state) {
    case SCRBS_NORMAL:
        WEBKIT_ASSERT(classicState == dfcsNormal);
        cstate = WebTestThemeControlWin::NormalState;
        break;

    case SCRBS_HOT:
        WEBKIT_ASSERT(classicState == DFCS_HOT);
        cstate = WebTestThemeControlWin::HotState;
        break;

    case SCRBS_HOVER:
        WEBKIT_ASSERT(classicState == dfcsNormal);
        cstate = WebTestThemeControlWin::HoverState;
        break;

    case SCRBS_PRESSED:
        WEBKIT_ASSERT(classicState == dfcsNormal);
        cstate = WebTestThemeControlWin::PressedState;
        break;

    case SCRBS_DISABLED:
        WEBKIT_ASSERT_NOT_REACHED(); // This should never happen in practice.
        break;

    default:
        WEBKIT_ASSERT_NOT_REACHED();
        break;
    }

    drawControl(canvas, rect, ctype, cstate);
}

void WebTestThemeEngineWin::paintScrollbarTrack(WebCanvas* canvas, int part, int state, int classicState, const WebRect& rect, const WebRect& alignRect)
{
    WebTestThemeControlWin::Type ctype = WebTestThemeControlWin::UnknownType;
    WebTestThemeControlWin::State cstate = WebTestThemeControlWin::UnknownState;

    switch (part) {
    case SBP_UPPERTRACKHORZ:
        ctype = WebTestThemeControlWin::HorizontalScrollTrackBackType;
        break;

    case SBP_LOWERTRACKHORZ:
        ctype = WebTestThemeControlWin::HorizontalScrollTrackForwardType;
        break;

    case SBP_UPPERTRACKVERT:
        ctype = WebTestThemeControlWin::VerticalScrollTrackBackType;
        break;

    case SBP_LOWERTRACKVERT:
        ctype = WebTestThemeControlWin::VerticalScrollTrackForwardType;
        break;

    default:
        WEBKIT_ASSERT_NOT_REACHED();
        break;
    }

    switch (state) {
    case SCRBS_NORMAL:
        WEBKIT_ASSERT(classicState == dfcsNormal);
        cstate = WebTestThemeControlWin::NormalState;
        break;

    case SCRBS_HOT:
        WEBKIT_ASSERT_NOT_REACHED(); // This should never happen in practice.
        break;

    case SCRBS_HOVER:
        WEBKIT_ASSERT(classicState == dfcsNormal);
        cstate = WebTestThemeControlWin::HoverState;
        break;

    case SCRBS_PRESSED:
        WEBKIT_ASSERT_NOT_REACHED(); // This should never happen in practice.
        break;

    case SCRBS_DISABLED:
        WEBKIT_ASSERT(classicState == DFCS_INACTIVE);
        cstate = WebTestThemeControlWin::DisabledState;
        break;

    default:
        WEBKIT_ASSERT_NOT_REACHED();
        break;
    }

    drawControl(canvas, rect, ctype, cstate);
}

void WebTestThemeEngineWin::paintSpinButton(WebCanvas* canvas, int part, int state, int classicState, const WebRect& rect)
{
    WebTestThemeControlWin::Type ctype = WebTestThemeControlWin::UnknownType;
    WebTestThemeControlWin::State cstate = WebTestThemeControlWin::UnknownState;

    if (part == SPNP_UP) {
        ctype = WebTestThemeControlWin::UpArrowType;
        switch (state) {
        case UPS_NORMAL:
            WEBKIT_ASSERT(classicState == DFCS_SCROLLUP);
            cstate = WebTestThemeControlWin::NormalState;
            break;
        case UPS_DISABLED:
            WEBKIT_ASSERT(classicState == (DFCS_SCROLLUP | DFCS_INACTIVE));
            cstate = WebTestThemeControlWin::DisabledState;
            break;
        case UPS_PRESSED:
            WEBKIT_ASSERT(classicState == (DFCS_SCROLLUP | DFCS_PUSHED));
            cstate = WebTestThemeControlWin::PressedState;
            break;
        case UPS_HOT:
            WEBKIT_ASSERT(classicState == (DFCS_SCROLLUP | DFCS_HOT));
            cstate = WebTestThemeControlWin::HoverState;
            break;
        default:
            WEBKIT_ASSERT_NOT_REACHED();
        }
    } else if (part == SPNP_DOWN) {
        ctype = WebTestThemeControlWin::DownArrowType;
        switch (state) {
        case DNS_NORMAL:
            WEBKIT_ASSERT(classicState == DFCS_SCROLLDOWN);
            cstate = WebTestThemeControlWin::NormalState;
            break;
        case DNS_DISABLED:
            WEBKIT_ASSERT(classicState == (DFCS_SCROLLDOWN | DFCS_INACTIVE));
            cstate = WebTestThemeControlWin::DisabledState;
            break;
        case DNS_PRESSED:
            WEBKIT_ASSERT(classicState == (DFCS_SCROLLDOWN | DFCS_PUSHED));
            cstate = WebTestThemeControlWin::PressedState;
            break;
        case DNS_HOT:
            WEBKIT_ASSERT(classicState == (DFCS_SCROLLDOWN | DFCS_HOT));
            cstate = WebTestThemeControlWin::HoverState;
            break;
        default:
            WEBKIT_ASSERT_NOT_REACHED();
        }
    } else
        WEBKIT_ASSERT_NOT_REACHED();
    drawControl(canvas, rect, ctype, cstate);
}

void WebTestThemeEngineWin::paintTextField(WebCanvas* canvas, int part, int state, int classicState, const WebRect& rect, WebColor color, bool fillContentArea, bool drawEdges)
{
    WebTestThemeControlWin::Type ctype = WebTestThemeControlWin::UnknownType;
    WebTestThemeControlWin::State cstate = WebTestThemeControlWin::UnknownState;

    WEBKIT_ASSERT(EP_EDITTEXT == part);
    ctype = WebTestThemeControlWin::TextFieldType;

    switch (state) {
    case ETS_NORMAL:
        WEBKIT_ASSERT(classicState == dfcsNormal);
        cstate = WebTestThemeControlWin::NormalState;
        break;

    case ETS_HOT:
        WEBKIT_ASSERT(classicState == DFCS_HOT);
        cstate = WebTestThemeControlWin::HotState;
        break;

    case ETS_DISABLED:
        WEBKIT_ASSERT(classicState == DFCS_INACTIVE);
        cstate = WebTestThemeControlWin::DisabledState;
        break;

    case ETS_SELECTED:
        WEBKIT_ASSERT(classicState == DFCS_PUSHED);
        cstate = WebTestThemeControlWin::PressedState;
        break;

    case ETS_FOCUSED:
        WEBKIT_ASSERT(classicState == dfcsNormal);
        cstate = WebTestThemeControlWin::FocusedState;
        break;

    case ETS_READONLY:
        WEBKIT_ASSERT(classicState == dfcsNormal);
        cstate = WebTestThemeControlWin::ReadOnlyState;
        break;

    default:
        WEBKIT_ASSERT_NOT_REACHED();
        break;
    }

    drawTextField(canvas, rect, ctype, cstate, drawEdges, fillContentArea, color);
}

void WebTestThemeEngineWin::paintTrackbar(WebCanvas* canvas, int part, int state, int classicState, const WebRect& rect)
{
    WebTestThemeControlWin::Type ctype = WebTestThemeControlWin::UnknownType;
    WebTestThemeControlWin::State cstate = WebTestThemeControlWin::UnknownState;

    if (TKP_THUMBBOTTOM == part) {
        ctype = WebTestThemeControlWin::HorizontalSliderThumbType;
        switch (state) {
        case TUS_NORMAL:
            WEBKIT_ASSERT(classicState == dfcsNormal);
            cstate = WebTestThemeControlWin::NormalState;
            break;

        case TUS_HOT:
            WEBKIT_ASSERT(classicState == DFCS_HOT);
            cstate = WebTestThemeControlWin::HotState;
            break;

        case TUS_DISABLED:
            WEBKIT_ASSERT(classicState == DFCS_INACTIVE);
            cstate = WebTestThemeControlWin::DisabledState;
            break;

        case TUS_PRESSED:
            WEBKIT_ASSERT(classicState == DFCS_PUSHED);
            cstate = WebTestThemeControlWin::PressedState;
            break;

        default:
            WEBKIT_ASSERT_NOT_REACHED();
            break;
        }
    } else if (TKP_THUMBVERT == part) {
        ctype = WebTestThemeControlWin::VerticalSliderThumbType;
        switch (state) {
        case TUS_NORMAL:
            WEBKIT_ASSERT(classicState == dfcsNormal);
            cstate = WebTestThemeControlWin::NormalState;
            break;

        case TUS_HOT:
            WEBKIT_ASSERT(classicState == DFCS_HOT);
            cstate = WebTestThemeControlWin::HotState;
            break;

        case TUS_DISABLED:
            WEBKIT_ASSERT(classicState == DFCS_INACTIVE);
            cstate = WebTestThemeControlWin::DisabledState;
            break;

        case TUS_PRESSED:
            WEBKIT_ASSERT(classicState == DFCS_PUSHED);
            cstate = WebTestThemeControlWin::PressedState;
            break;

        default:
            WEBKIT_ASSERT_NOT_REACHED();
            break;
        }
    } else if (TKP_TRACK == part) {
        ctype = WebTestThemeControlWin::HorizontalSliderTrackType;
        WEBKIT_ASSERT(state == TRS_NORMAL);
        WEBKIT_ASSERT(classicState == dfcsNormal);
        cstate = WebTestThemeControlWin::NormalState;
    } else if (TKP_TRACKVERT == part) {
        ctype = WebTestThemeControlWin::VerticalSliderTrackType;
        WEBKIT_ASSERT(state == TRVS_NORMAL);
        WEBKIT_ASSERT(classicState == dfcsNormal);
        cstate = WebTestThemeControlWin::NormalState;
    } else
        WEBKIT_ASSERT_NOT_REACHED();

    drawControl(canvas, rect, ctype, cstate);
}


void WebTestThemeEngineWin::paintProgressBar(WebKit::WebCanvas* canvas, const WebKit::WebRect& barRect, const WebKit::WebRect& valueRect, bool determinate, double)
{
    WebTestThemeControlWin::Type ctype = WebTestThemeControlWin::ProgressBarType;
    WebTestThemeControlWin::State cstate = determinate ? WebTestThemeControlWin::NormalState : WebTestThemeControlWin::IndeterminateState;
    drawProgressBar(canvas, ctype, cstate, barRect, valueRect);
}


WebKit::WebSize WebTestThemeEngineWin::getSize(int part)
{
    return WebKit::WebSize();
}

}
