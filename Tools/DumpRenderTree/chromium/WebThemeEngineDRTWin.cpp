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
#include "WebThemeEngineDRTWin.h"

#include "WebThemeControlDRTWin.h"
#include "third_party/skia/include/core/SkRect.h"
#include <public/WebRect.h>

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
    irect.set(webRect.x, webRect.y, webRect.x + webRect.width - 1, webRect.y + webRect.height - 1);
    return irect;
}

static void drawControl(WebCanvas* canvas,
                        const WebRect& rect,
                        WebThemeControlDRTWin::Type ctype,
                        WebThemeControlDRTWin::State cstate)
{
    WebThemeControlDRTWin control(canvas, webRectToSkIRect(rect), ctype, cstate);
    control.draw();
}

static void drawTextField(WebCanvas* canvas,
                          const WebRect& rect,
                          WebThemeControlDRTWin::Type ctype,
                          WebThemeControlDRTWin::State cstate,
                          bool drawEdges,
                          bool fillContentArea,
                          WebColor color)
{
    WebThemeControlDRTWin control(canvas, webRectToSkIRect(rect), ctype, cstate);
    control.drawTextField(drawEdges, fillContentArea, color);
}

static void drawProgressBar(WebCanvas* canvas,
                            WebThemeControlDRTWin::Type ctype,
                            WebThemeControlDRTWin::State cstate,
                            const WebRect& barRect,
                            const WebRect& fillRect)
{
    WebThemeControlDRTWin control(canvas, webRectToSkIRect(barRect), ctype, cstate);
    control.drawProgressBar(webRectToSkIRect(fillRect));
}

// WebThemeEngineDRTWin

void WebThemeEngineDRTWin::paintButton(WebCanvas* canvas,
                                       int part,
                                       int state,
                                       int classicState,
                                       const WebRect& rect)
{
    WebThemeControlDRTWin::Type ctype = WebThemeControlDRTWin::UnknownType;
    WebThemeControlDRTWin::State cstate = WebThemeControlDRTWin::UnknownState;

    if (part == BP_CHECKBOX) {
        switch (state) {
        case CBS_UNCHECKEDNORMAL:
            ASSERT(classicState == dfcsNormal);
            ctype = WebThemeControlDRTWin::UncheckedBoxType;
            cstate = WebThemeControlDRTWin::NormalState;
            break;

        case CBS_UNCHECKEDHOT:
            ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_HOT));
            ctype = WebThemeControlDRTWin::UncheckedBoxType;
            cstate = WebThemeControlDRTWin::HotState;
            break;

        case CBS_UNCHECKEDPRESSED:
            ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_PUSHED));
            ctype = WebThemeControlDRTWin::UncheckedBoxType;
            cstate = WebThemeControlDRTWin::PressedState;
            break;

        case CBS_UNCHECKEDDISABLED:
            ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_INACTIVE));
            ctype = WebThemeControlDRTWin::UncheckedBoxType;
            cstate = WebThemeControlDRTWin::DisabledState;
            break;

        case CBS_CHECKEDNORMAL:
            ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_CHECKED));
            ctype = WebThemeControlDRTWin::CheckedBoxType;
            cstate = WebThemeControlDRTWin::NormalState;
            break;

        case CBS_CHECKEDHOT:
            ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_CHECKED | DFCS_HOT));
            ctype = WebThemeControlDRTWin::CheckedBoxType;
            cstate = WebThemeControlDRTWin::HotState;
            break;

        case CBS_CHECKEDPRESSED:
            ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_CHECKED | DFCS_PUSHED));
            ctype = WebThemeControlDRTWin::CheckedBoxType;
            cstate = WebThemeControlDRTWin::PressedState;
            break;

        case CBS_CHECKEDDISABLED:
            ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_CHECKED | DFCS_INACTIVE));
            ctype = WebThemeControlDRTWin::CheckedBoxType;
            cstate = WebThemeControlDRTWin::DisabledState;
            break;

        case CBS_MIXEDNORMAL:
            // Classic theme can't represent mixed state checkbox. We assume
            // it's equivalent to unchecked.
            ASSERT(classicState == DFCS_BUTTONCHECK);
            ctype = WebThemeControlDRTWin::IndeterminateCheckboxType;
            cstate = WebThemeControlDRTWin::NormalState;
            break;

        case CBS_MIXEDHOT:
            ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_HOT));
            ctype = WebThemeControlDRTWin::IndeterminateCheckboxType;
            cstate = WebThemeControlDRTWin::HotState;
            break;

        case CBS_MIXEDPRESSED:
            ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_PUSHED));
            ctype = WebThemeControlDRTWin::IndeterminateCheckboxType;
            cstate = WebThemeControlDRTWin::PressedState;
            break;

        case CBS_MIXEDDISABLED:
            ASSERT(classicState == (DFCS_BUTTONCHECK | DFCS_INACTIVE));
            ctype = WebThemeControlDRTWin::IndeterminateCheckboxType;
            cstate = WebThemeControlDRTWin::DisabledState;
            break;

        default:
            ASSERT_NOT_REACHED();
            break;
        }
    } else if (BP_RADIOBUTTON == part) {
        switch (state) {
        case RBS_UNCHECKEDNORMAL:
            ASSERT(classicState == DFCS_BUTTONRADIO);
            ctype = WebThemeControlDRTWin::UncheckedRadioType;
            cstate = WebThemeControlDRTWin::NormalState;
            break;

        case RBS_UNCHECKEDHOT:
            ASSERT(classicState == (DFCS_BUTTONRADIO | DFCS_HOT));
            ctype = WebThemeControlDRTWin::UncheckedRadioType;
            cstate = WebThemeControlDRTWin::HotState;
            break;

        case RBS_UNCHECKEDPRESSED:
            ASSERT(classicState == (DFCS_BUTTONRADIO | DFCS_PUSHED));
            ctype = WebThemeControlDRTWin::UncheckedRadioType;
            cstate = WebThemeControlDRTWin::PressedState;
            break;

        case RBS_UNCHECKEDDISABLED:
            ASSERT(classicState == (DFCS_BUTTONRADIO | DFCS_INACTIVE));
            ctype = WebThemeControlDRTWin::UncheckedRadioType;
            cstate = WebThemeControlDRTWin::DisabledState;
            break;

        case RBS_CHECKEDNORMAL:
            ASSERT(classicState == (DFCS_BUTTONRADIO | DFCS_CHECKED));
            ctype = WebThemeControlDRTWin::CheckedRadioType;
            cstate = WebThemeControlDRTWin::NormalState;
            break;

        case RBS_CHECKEDHOT:
            ASSERT(classicState == (DFCS_BUTTONRADIO | DFCS_CHECKED | DFCS_HOT));
            ctype = WebThemeControlDRTWin::CheckedRadioType;
            cstate = WebThemeControlDRTWin::HotState;
            break;

        case RBS_CHECKEDPRESSED:
            ASSERT(classicState == (DFCS_BUTTONRADIO | DFCS_CHECKED | DFCS_PUSHED));
            ctype = WebThemeControlDRTWin::CheckedRadioType;
            cstate = WebThemeControlDRTWin::PressedState;
            break;

        case RBS_CHECKEDDISABLED:
            ASSERT(classicState == (DFCS_BUTTONRADIO | DFCS_CHECKED | DFCS_INACTIVE));
            ctype = WebThemeControlDRTWin::CheckedRadioType;
            cstate = WebThemeControlDRTWin::DisabledState;
            break;

        default:
            ASSERT_NOT_REACHED();
            break;
        }
    } else if (BP_PUSHBUTTON == part) {
        switch (state) {
        case PBS_NORMAL:
            ASSERT(classicState == DFCS_BUTTONPUSH);
            ctype = WebThemeControlDRTWin::PushButtonType;
            cstate = WebThemeControlDRTWin::NormalState;
            break;

        case PBS_HOT:
            ASSERT(classicState == (DFCS_BUTTONPUSH | DFCS_HOT));
            ctype = WebThemeControlDRTWin::PushButtonType;
            cstate = WebThemeControlDRTWin::HotState;
            break;

        case PBS_PRESSED:
            ASSERT(classicState == (DFCS_BUTTONPUSH | DFCS_PUSHED));
            ctype = WebThemeControlDRTWin::PushButtonType;
            cstate = WebThemeControlDRTWin::PressedState;
            break;

        case PBS_DISABLED:
            ASSERT(classicState == (DFCS_BUTTONPUSH | DFCS_INACTIVE));
            ctype = WebThemeControlDRTWin::PushButtonType;
            cstate = WebThemeControlDRTWin::DisabledState;
            break;

        case PBS_DEFAULTED:
            ASSERT(classicState == DFCS_BUTTONPUSH);
            ctype = WebThemeControlDRTWin::PushButtonType;
            cstate = WebThemeControlDRTWin::FocusedState;
            break;

        default:
            ASSERT_NOT_REACHED();
            break;
        }
    } else
        ASSERT_NOT_REACHED();

    drawControl(canvas, rect, ctype, cstate);
}


void WebThemeEngineDRTWin::paintMenuList(WebCanvas* canvas,
                                         int part,
                                         int state,
                                         int classicState,
                                         const WebRect& rect)
{
    WebThemeControlDRTWin::Type ctype = WebThemeControlDRTWin::UnknownType;
    WebThemeControlDRTWin::State cstate = WebThemeControlDRTWin::UnknownState;

    if (CP_DROPDOWNBUTTON == part) {
        ctype = WebThemeControlDRTWin::DropDownButtonType;
        switch (state) {
        case CBXS_NORMAL:
            ASSERT(classicState == DFCS_MENUARROW);
            cstate = WebThemeControlDRTWin::NormalState;
            break;

        case CBXS_HOT:
            ASSERT(classicState == (DFCS_MENUARROW | DFCS_HOT));
            cstate = WebThemeControlDRTWin::HoverState;
            break;

        case CBXS_PRESSED:
            ASSERT(classicState == (DFCS_MENUARROW | DFCS_PUSHED));
            cstate = WebThemeControlDRTWin::PressedState;
            break;

        case CBXS_DISABLED:
            ASSERT(classicState == (DFCS_MENUARROW | DFCS_INACTIVE));
            cstate = WebThemeControlDRTWin::DisabledState;
            break;

        default:
            CRASH();
            break;
        }
    } else
        CRASH();

    drawControl(canvas, rect, ctype, cstate);
}

void WebThemeEngineDRTWin::paintScrollbarArrow(WebCanvas* canvas,
                                               int state,
                                               int classicState,
                                               const WebRect& rect)
{
    WebThemeControlDRTWin::Type ctype = WebThemeControlDRTWin::UnknownType;
    WebThemeControlDRTWin::State cstate = WebThemeControlDRTWin::UnknownState;

    switch (state) {
    case ABS_UPNORMAL:
        ASSERT(classicState == DFCS_SCROLLUP);
        ctype = WebThemeControlDRTWin::UpArrowType;
        cstate = WebThemeControlDRTWin::NormalState;
        break;

    case ABS_DOWNNORMAL:
        ASSERT(classicState == DFCS_SCROLLDOWN);
        ctype = WebThemeControlDRTWin::DownArrowType;
        cstate = WebThemeControlDRTWin::NormalState;
        break;

    case ABS_LEFTNORMAL:
        ASSERT(classicState == DFCS_SCROLLLEFT);
        ctype = WebThemeControlDRTWin::LeftArrowType;
        cstate = WebThemeControlDRTWin::NormalState;
        break;

    case ABS_RIGHTNORMAL:
        ASSERT(classicState == DFCS_SCROLLRIGHT);
        ctype = WebThemeControlDRTWin::RightArrowType;
        cstate = WebThemeControlDRTWin::NormalState;
        break;

    case ABS_UPHOT:
        ASSERT(classicState == (DFCS_SCROLLUP | DFCS_HOT));
        ctype = WebThemeControlDRTWin::UpArrowType;
        cstate = WebThemeControlDRTWin::HotState;
        break;

    case ABS_DOWNHOT:
        ASSERT(classicState == (DFCS_SCROLLDOWN | DFCS_HOT));
        ctype = WebThemeControlDRTWin::DownArrowType;
        cstate = WebThemeControlDRTWin::HotState;
        break;

    case ABS_LEFTHOT:
        ASSERT(classicState == (DFCS_SCROLLLEFT | DFCS_HOT));
        ctype = WebThemeControlDRTWin::LeftArrowType;
        cstate = WebThemeControlDRTWin::HotState;
        break;

    case ABS_RIGHTHOT:
        ASSERT(classicState == (DFCS_SCROLLRIGHT | DFCS_HOT));
        ctype = WebThemeControlDRTWin::RightArrowType;
        cstate = WebThemeControlDRTWin::HotState;
        break;

    case ABS_UPHOVER:
        ASSERT(classicState == DFCS_SCROLLUP);
        ctype = WebThemeControlDRTWin::UpArrowType;
        cstate = WebThemeControlDRTWin::HoverState;
        break;

    case ABS_DOWNHOVER:
        ASSERT(classicState == DFCS_SCROLLDOWN);
        ctype = WebThemeControlDRTWin::DownArrowType;
        cstate = WebThemeControlDRTWin::HoverState;
        break;

    case ABS_LEFTHOVER:
        ASSERT(classicState == DFCS_SCROLLLEFT);
        ctype = WebThemeControlDRTWin::LeftArrowType;
        cstate = WebThemeControlDRTWin::HoverState;
        break;

    case ABS_RIGHTHOVER:
        ASSERT(classicState == DFCS_SCROLLRIGHT);
        ctype = WebThemeControlDRTWin::RightArrowType;
        cstate = WebThemeControlDRTWin::HoverState;
        break;

    case ABS_UPPRESSED:
        ASSERT(classicState == (DFCS_SCROLLUP | DFCS_PUSHED | DFCS_FLAT));
        ctype = WebThemeControlDRTWin::UpArrowType;
        cstate = WebThemeControlDRTWin::PressedState;
        break;

    case ABS_DOWNPRESSED:
        ASSERT(classicState == (DFCS_SCROLLDOWN | DFCS_PUSHED | DFCS_FLAT));
        ctype = WebThemeControlDRTWin::DownArrowType;
        cstate = WebThemeControlDRTWin::PressedState;
        break;

    case ABS_LEFTPRESSED:
        ASSERT(classicState == (DFCS_SCROLLLEFT | DFCS_PUSHED | DFCS_FLAT));
        ctype = WebThemeControlDRTWin::LeftArrowType;
        cstate = WebThemeControlDRTWin::PressedState;
        break;

    case ABS_RIGHTPRESSED:
        ASSERT(classicState == (DFCS_SCROLLRIGHT | DFCS_PUSHED | DFCS_FLAT));
        ctype = WebThemeControlDRTWin::RightArrowType;
        cstate = WebThemeControlDRTWin::PressedState;
        break;

    case ABS_UPDISABLED:
        ASSERT(classicState == (DFCS_SCROLLUP | DFCS_INACTIVE));
        ctype = WebThemeControlDRTWin::UpArrowType;
        cstate = WebThemeControlDRTWin::DisabledState;
        break;

    case ABS_DOWNDISABLED:
        ASSERT(classicState == (DFCS_SCROLLDOWN | DFCS_INACTIVE));
        ctype = WebThemeControlDRTWin::DownArrowType;
        cstate = WebThemeControlDRTWin::DisabledState;
        break;

    case ABS_LEFTDISABLED:
        ASSERT(classicState == (DFCS_SCROLLLEFT | DFCS_INACTIVE));
        ctype = WebThemeControlDRTWin::LeftArrowType;
        cstate = WebThemeControlDRTWin::DisabledState;
        break;

    case ABS_RIGHTDISABLED:
        ASSERT(classicState == (DFCS_SCROLLRIGHT | DFCS_INACTIVE));
        ctype = WebThemeControlDRTWin::RightArrowType;
        cstate = WebThemeControlDRTWin::DisabledState;
        break;

    default:
        ASSERT_NOT_REACHED();
        break;
    }

    drawControl(canvas, rect, ctype, cstate);
}

void WebThemeEngineDRTWin::paintScrollbarThumb(WebCanvas* canvas,
                                               int part,
                                               int state,
                                               int classicState,
                                               const WebRect& rect)
{
    WebThemeControlDRTWin::Type ctype = WebThemeControlDRTWin::UnknownType;
    WebThemeControlDRTWin::State cstate = WebThemeControlDRTWin::UnknownState;

    switch (part) {
    case SBP_THUMBBTNHORZ:
        ctype = WebThemeControlDRTWin::HorizontalScrollThumbType;
        break;

    case SBP_THUMBBTNVERT:
        ctype = WebThemeControlDRTWin::VerticalScrollThumbType;
        break;

    case SBP_GRIPPERHORZ:
        ctype = WebThemeControlDRTWin::HorizontalScrollGripType;
        break;

    case SBP_GRIPPERVERT:
        ctype = WebThemeControlDRTWin::VerticalScrollGripType;
        break;

    default:
        ASSERT_NOT_REACHED();
        break;
    }

    switch (state) {
    case SCRBS_NORMAL:
        ASSERT(classicState == dfcsNormal);
        cstate = WebThemeControlDRTWin::NormalState;
        break;

    case SCRBS_HOT:
        ASSERT(classicState == DFCS_HOT);
        cstate = WebThemeControlDRTWin::HotState;
        break;

    case SCRBS_HOVER:
        ASSERT(classicState == dfcsNormal);
        cstate = WebThemeControlDRTWin::HoverState;
        break;

    case SCRBS_PRESSED:
        ASSERT(classicState == dfcsNormal);
        cstate = WebThemeControlDRTWin::PressedState;
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

void WebThemeEngineDRTWin::paintScrollbarTrack(WebCanvas* canvas,
                                               int part,
                                               int state,
                                               int classicState,
                                               const WebRect& rect,
                                               const WebRect& alignRect)
{
    WebThemeControlDRTWin::Type ctype = WebThemeControlDRTWin::UnknownType;
    WebThemeControlDRTWin::State cstate = WebThemeControlDRTWin::UnknownState;

    switch (part) {
    case SBP_UPPERTRACKHORZ:
        ctype = WebThemeControlDRTWin::HorizontalScrollTrackBackType;
        break;

    case SBP_LOWERTRACKHORZ:
        ctype = WebThemeControlDRTWin::HorizontalScrollTrackForwardType;
        break;

    case SBP_UPPERTRACKVERT:
        ctype = WebThemeControlDRTWin::VerticalScrollTrackBackType;
        break;

    case SBP_LOWERTRACKVERT:
        ctype = WebThemeControlDRTWin::VerticalScrollTrackForwardType;
        break;

    default:
        ASSERT_NOT_REACHED();
        break;
    }

    switch (state) {
    case SCRBS_NORMAL:
        ASSERT(classicState == dfcsNormal);
        cstate = WebThemeControlDRTWin::NormalState;
        break;

    case SCRBS_HOT:
        ASSERT_NOT_REACHED(); // This should never happen in practice.
        break;

    case SCRBS_HOVER:
        ASSERT(classicState == dfcsNormal);
        cstate = WebThemeControlDRTWin::HoverState;
        break;

    case SCRBS_PRESSED:
        ASSERT_NOT_REACHED(); // This should never happen in practice.
        break;

    case SCRBS_DISABLED:
        ASSERT(classicState == DFCS_INACTIVE);
        cstate = WebThemeControlDRTWin::DisabledState;
        break;

    default:
        CRASH();
        break;
    }

    drawControl(canvas, rect, ctype, cstate);
}

void WebThemeEngineDRTWin::paintSpinButton(WebCanvas* canvas,
                                           int part,
                                           int state,
                                           int classicState,
                                           const WebRect& rect)
{
    WebThemeControlDRTWin::Type ctype = WebThemeControlDRTWin::UnknownType;
    WebThemeControlDRTWin::State cstate = WebThemeControlDRTWin::UnknownState;

    if (part == SPNP_UP) {
        ctype = WebThemeControlDRTWin::UpArrowType;
        switch (state) {
        case UPS_NORMAL:
            ASSERT(classicState == DFCS_SCROLLUP);
            cstate = WebThemeControlDRTWin::NormalState;
            break;
        case UPS_DISABLED:
            ASSERT(classicState == (DFCS_SCROLLUP | DFCS_INACTIVE));
            cstate = WebThemeControlDRTWin::DisabledState;
            break;
        case UPS_PRESSED:
            ASSERT(classicState == (DFCS_SCROLLUP | DFCS_PUSHED));
            cstate = WebThemeControlDRTWin::PressedState;
            break;
        case UPS_HOT:
            ASSERT(classicState == (DFCS_SCROLLUP | DFCS_HOT));
            cstate = WebThemeControlDRTWin::HoverState;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    } else if (part == SPNP_DOWN) {
        ctype = WebThemeControlDRTWin::DownArrowType;
        switch (state) {
        case DNS_NORMAL:
            ASSERT(classicState == DFCS_SCROLLDOWN);
            cstate = WebThemeControlDRTWin::NormalState;
            break;
        case DNS_DISABLED:
            ASSERT(classicState == (DFCS_SCROLLDOWN | DFCS_INACTIVE));
            cstate = WebThemeControlDRTWin::DisabledState;
            break;
        case DNS_PRESSED:
            ASSERT(classicState == (DFCS_SCROLLDOWN | DFCS_PUSHED));
            cstate = WebThemeControlDRTWin::PressedState;
            break;
        case DNS_HOT:
            ASSERT(classicState == (DFCS_SCROLLDOWN | DFCS_HOT));
            cstate = WebThemeControlDRTWin::HoverState;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    } else
        ASSERT_NOT_REACHED();
    drawControl(canvas, rect, ctype, cstate);
}

void WebThemeEngineDRTWin::paintTextField(WebCanvas* canvas,
                                          int part,
                                          int state,
                                          int classicState,
                                          const WebRect& rect,
                                          WebColor color,
                                          bool fillContentArea,
                                          bool drawEdges)
{
    WebThemeControlDRTWin::Type ctype = WebThemeControlDRTWin::UnknownType;
    WebThemeControlDRTWin::State cstate = WebThemeControlDRTWin::UnknownState;

    ASSERT(EP_EDITTEXT == part);
    ctype = WebThemeControlDRTWin::TextFieldType;

    switch (state) {
    case ETS_NORMAL:
        ASSERT(classicState == dfcsNormal);
        cstate = WebThemeControlDRTWin::NormalState;
        break;

    case ETS_HOT:
        ASSERT(classicState == DFCS_HOT);
        cstate = WebThemeControlDRTWin::HotState;
        break;

    case ETS_DISABLED:
        ASSERT(classicState == DFCS_INACTIVE);
        cstate = WebThemeControlDRTWin::DisabledState;
        break;

    case ETS_SELECTED:
        ASSERT(classicState == DFCS_PUSHED);
        cstate = WebThemeControlDRTWin::PressedState;
        break;

    case ETS_FOCUSED:
        ASSERT(classicState == dfcsNormal);
        cstate = WebThemeControlDRTWin::FocusedState;
        break;

    case ETS_READONLY:
        ASSERT(classicState == dfcsNormal);
        cstate = WebThemeControlDRTWin::ReadOnlyState;
        break;

    default:
        ASSERT_NOT_REACHED();
        break;
    }

    drawTextField(canvas, rect, ctype, cstate, drawEdges, fillContentArea, color);
}

void WebThemeEngineDRTWin::paintTrackbar(WebCanvas* canvas,
                                         int part,
                                         int state,
                                         int classicState,
                                         const WebRect& rect)
{
    WebThemeControlDRTWin::Type ctype = WebThemeControlDRTWin::UnknownType;
    WebThemeControlDRTWin::State cstate = WebThemeControlDRTWin::UnknownState;

    if (TKP_THUMBBOTTOM == part) {
        ctype = WebThemeControlDRTWin::HorizontalSliderThumbType;
        switch (state) {
        case TUS_NORMAL:
            ASSERT(classicState == dfcsNormal);
            cstate = WebThemeControlDRTWin::NormalState;
            break;

        case TUS_HOT:
            ASSERT(classicState == DFCS_HOT);
            cstate = WebThemeControlDRTWin::HotState;
            break;

        case TUS_DISABLED:
            ASSERT(classicState == DFCS_INACTIVE);
            cstate = WebThemeControlDRTWin::DisabledState;
            break;

        case TUS_PRESSED:
            ASSERT(classicState == DFCS_PUSHED);
            cstate = WebThemeControlDRTWin::PressedState;
            break;

        default:
            ASSERT_NOT_REACHED();
            break;
        }
    } else if (TKP_THUMBVERT == part) {
        ctype = WebThemeControlDRTWin::VerticalSliderThumbType;
        switch (state) {
        case TUS_NORMAL:
            ASSERT(classicState == dfcsNormal);
            cstate = WebThemeControlDRTWin::NormalState;
            break;

        case TUS_HOT:
            ASSERT(classicState == DFCS_HOT);
            cstate = WebThemeControlDRTWin::HotState;
            break;

        case TUS_DISABLED:
            ASSERT(classicState == DFCS_INACTIVE);
            cstate = WebThemeControlDRTWin::DisabledState;
            break;

        case TUS_PRESSED:
            ASSERT(classicState == DFCS_PUSHED);
            cstate = WebThemeControlDRTWin::PressedState;
            break;

        default:
            ASSERT_NOT_REACHED();
            break;
        }
    } else if (TKP_TRACK == part) {
        ctype = WebThemeControlDRTWin::HorizontalSliderTrackType;
        ASSERT(state == TRS_NORMAL);
        ASSERT(classicState == dfcsNormal);
        cstate = WebThemeControlDRTWin::NormalState;
    } else if (TKP_TRACKVERT == part) {
        ctype = WebThemeControlDRTWin::VerticalSliderTrackType;
        ASSERT(state == TRVS_NORMAL);
        ASSERT(classicState == dfcsNormal);
        cstate = WebThemeControlDRTWin::NormalState;
    } else
        ASSERT_NOT_REACHED();

    drawControl(canvas, rect, ctype, cstate);
}


void WebThemeEngineDRTWin::paintProgressBar(WebKit::WebCanvas* canvas,
                                            const WebKit::WebRect& barRect,
                                            const WebKit::WebRect& valueRect,
                                            bool determinate,
                                            double)
{
    WebThemeControlDRTWin::Type ctype = WebThemeControlDRTWin::ProgressBarType;
    WebThemeControlDRTWin::State cstate = determinate ? WebThemeControlDRTWin::NormalState
                                                      : WebThemeControlDRTWin::IndeterminateState;
    drawProgressBar(canvas, ctype, cstate, barRect, valueRect);
}


WebKit::WebSize WebThemeEngineDRTWin::getSize(int part)
{
    return WebKit::WebSize();
}

