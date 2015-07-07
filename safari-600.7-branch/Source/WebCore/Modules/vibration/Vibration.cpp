/*
 *  Copyright (C) 2012 Samsung Electronics
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "Vibration.h"

#if ENABLE(VIBRATION)

#include "VibrationClient.h"

namespace WebCore {

// Maximum number of entries in a vibration pattern.
const unsigned MaxVibrationPatternLength = 99;
// Maximum duration of a vibration in 10 seconds.
const unsigned MaxVibrationDuration = 10000;

Vibration::Vibration(VibrationClient* client)
    : m_vibrationClient(client)
    , m_timer(this, &Vibration::timerFired)
    , m_state(State::Idle)
{
}

Vibration::~Vibration()
{
    m_vibrationClient->vibrationDestroyed();
}

bool Vibration::vibrate(const VibrationPattern& pattern)
{
    VibrationPattern& sanitized = const_cast<VibrationPattern&>(pattern);
    size_t length = sanitized.size();

    // If the pattern is too long then truncate it.
    if (length > MaxVibrationPatternLength) {
        sanitized.shrink(MaxVibrationPatternLength);
        length = MaxVibrationPatternLength;
    }

    // If the length of pattern is even and is not zero, then remove the last entry in pattern.
    if (length && !(length % 2)) {
        sanitized.removeLast();
        length--;
    }

    // If any pattern entry is too long then truncate it.
    for (auto& duration : sanitized)
        duration = std::min(duration, MaxVibrationDuration);

    // Pre-exisiting instance need to be canceled when vibrate() is called.
    if (isVibrating())
        cancelVibration();

    // If pattern is an empty list, then return true and terminate these steps.
    if (!length || (length == 1 && !sanitized[0])) {
        cancelVibration();
        return true;
    }

    m_pattern = sanitized;

    m_timer.startOneShot(0);
    m_state = State::Waiting;
    return true;
}

void Vibration::cancelVibration()
{
    m_pattern.clear();
    if (isVibrating()) {
        m_timer.stop();
        m_state = State::Idle;
        m_vibrationClient->cancelVibration();
    }
}

void Vibration::timerFired(Timer* timer)
{
    ASSERT_UNUSED(timer, timer == &m_timer);

    m_timer.stop();

    if (m_pattern.isEmpty()) {
        m_state = State::Idle;
        return;
    }

    switch (m_state) {
    case State::Vibrating:
        m_state = State::Waiting;
        break;
    case State::Waiting:
    case State::Idle:
        m_state = State::Vibrating;
        m_vibrationClient->vibrate(m_pattern[0]);
        break;
    }
    m_timer.startOneShot(m_pattern[0] / 1000.0);
    m_pattern.remove(0);
}

const char* Vibration::supplementName()
{
    return "Vibration";
}

void provideVibrationTo(Page* page, VibrationClient* client)
{
    Vibration::provideTo(page, Vibration::supplementName(), std::make_unique<Vibration>(client));
}

} // namespace WebCore

#endif // ENABLE(VIBRATION)

