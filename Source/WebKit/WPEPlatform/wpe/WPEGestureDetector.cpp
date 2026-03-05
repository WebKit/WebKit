/*
 * Copyright (C) 2024 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEGestureDetector.h"
#include "WPEDisplay.h"
#include "WPESettings.h"

#include <cmath>

namespace WPE {

bool GestureDetector::handleEvent(WPEEvent* event)
{
    if (m_sequenceId && *m_sequenceId != wpe_event_touch_get_sequence_id(event))
        return false;

    switch (wpe_event_get_event_type(event)) {
    case WPE_EVENT_TOUCH_DOWN:
        reset();
        if (double x, y; wpe_event_get_position(event, &x, &y)) {
            m_gesture = WPE_GESTURE_TAP;
            m_position = { x, y };
            m_sequenceId = wpe_event_touch_get_sequence_id(event);
        }
        break;
    case WPE_EVENT_TOUCH_CANCEL:
        reset();
        break;
    case WPE_EVENT_TOUCH_MOVE:
        if (!m_sequenceId)
            return false;
        if (double x, y; wpe_event_get_position(event, &x, &y) && m_position) {
            auto* settings = wpe_display_get_settings(wpe_view_get_display(wpe_event_get_view(event)));
            auto dragActivationThresholdPx = wpe_settings_get_uint32(settings, WPE_SETTING_DRAG_THRESHOLD, nullptr);
            if (m_gesture != WPE_GESTURE_DRAG && std::hypot(x - m_position->x, y - m_position->y) > dragActivationThresholdPx) {
                m_gesture = WPE_GESTURE_DRAG;
                m_nextDeltaReferencePosition = m_position;
                m_dragBegin = true;
            } else if (m_gesture == WPE_GESTURE_DRAG)
                m_dragBegin = false;
            if (m_gesture == WPE_GESTURE_DRAG) {
                m_delta = { x - m_nextDeltaReferencePosition->x, y - m_nextDeltaReferencePosition->y };
                m_nextDeltaReferencePosition = { x, y };
            }
        } else
            reset();
        break;
    case WPE_EVENT_TOUCH_UP:
        if (!m_sequenceId)
            return false;
        if (double x, y; wpe_event_get_position(event, &x, &y) && m_position) {
            if (m_gesture == WPE_GESTURE_DRAG)
                m_delta = { x - m_nextDeltaReferencePosition->x, y - m_nextDeltaReferencePosition->y };
        } else
            reset();
        m_sequenceId = std::nullopt; // We can accept new sequence at this point.
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    return true;
}

void GestureDetector::reset()
{
    m_gesture = WPE_GESTURE_NONE;
    m_sequenceId = std::nullopt;
    m_position = std::nullopt;
    m_nextDeltaReferencePosition = std::nullopt;
    m_delta = std::nullopt;
    m_dragBegin = std::nullopt;
}

} // namespace WPE
