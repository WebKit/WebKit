/*
 * Copyright (C) 2017 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollGestureController.h"

#include <WebCore/Scrollbar.h>

namespace WebKit {

bool ScrollGestureController::handleEvent(const struct wpe_input_touch_event_raw* touchPoint)
{
    switch (touchPoint->type) {
    case wpe_input_touch_event_type_down:
        m_start.time = touchPoint->time;
        m_start.x = touchPoint->x;
        m_start.y = touchPoint->y;
        m_offset.x = touchPoint->x;
        m_offset.y = touchPoint->y;
        return false;
    case wpe_input_touch_event_type_motion:
        if (!m_handling) {
            int32_t deltaX = touchPoint->x - m_start.x;
            int32_t deltaY = touchPoint->y - m_start.y;
            uint32_t deltaTime = touchPoint->time - m_start.time;

            int pixelsPerLineStep = WebCore::Scrollbar::pixelsPerLineStep();
            m_handling = std::abs(deltaX) >= pixelsPerLineStep
                || std::abs(deltaY) >= pixelsPerLineStep
                || deltaTime >= 200;
        }
        if (m_handling) {
#if WPE_CHECK_VERSION(1, 5, 0)
            m_axisEvent.base = {
                static_cast<enum wpe_input_axis_event_type>(wpe_input_axis_event_type_mask_2d | wpe_input_axis_event_type_motion_smooth),
                touchPoint->time, m_start.x, m_start.y,
                0, 0, 0,
            };
            m_axisEvent.x_axis = -(m_offset.x - touchPoint->x);
            m_axisEvent.y_axis = -(m_offset.y - touchPoint->y);
#else
            m_axisEvent = {
                wpe_input_axis_event_type_motion,
                touchPoint->time, m_start.x, m_start.y,
                2, (touchPoint->y - m_offset.y), 0
            };
#endif
            m_offset.x = touchPoint->x;
            m_offset.y = touchPoint->y;
            m_phase = WebWheelEvent::Phase::PhaseChanged;
            return true;
        }
        return false;
    case wpe_input_touch_event_type_up:
        if (m_handling) {
            m_handling = false;
#if WPE_CHECK_VERSION(1, 5, 0)
            m_axisEvent.base = {
                m_axisEvent.base.type,
                touchPoint->time, m_start.x, m_start.y,
                0, 0, 0
            };
            m_axisEvent.x_axis = m_axisEvent.y_axis = 0;
#else
            m_axisEvent = {
                m_axisEvent.type,
                touchPoint->time, m_start.x, m_start.y,
                0, 0, 0
            };
#endif
            m_offset.x = m_offset.y = 0;
            m_phase = WebWheelEvent::Phase::PhaseEnded;
            return true;
        }
        return false;
    default:
        return false;
    }
}

} // namespace WebKit
