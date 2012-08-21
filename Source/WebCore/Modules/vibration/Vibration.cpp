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

Vibration::Vibration(VibrationClient* client)
    : m_vibrationClient(client)
    , m_timerStart(this, &Vibration::timerStartFired)
    , m_timerStop(this, &Vibration::timerStopFired)
    , m_isVibrating(false)
{
}

Vibration::~Vibration()
{
    m_vibrationClient->vibrationDestroyed();
}

PassOwnPtr<Vibration> Vibration::create(VibrationClient* client)
{
    return adoptPtr(new Vibration(client));
}

void Vibration::vibrate(const unsigned long& time)
{
    if (!time) {
        cancelVibration();
        return;
    }
    m_pattern.append(time);
    m_timerStart.startOneShot(0);
}

void Vibration::vibrate(const VibrationPattern& pattern)
{
    int length = pattern.size();

    // Cancel the pre-existing instance of vibration patterns, if the pattern is 0 or an empty list.
    if (!length || (length == 1 && !pattern[0])) {
        cancelVibration();
        return;
    }

    if (m_isVibrating)
        cancelVibration();

    if (m_timerStart.isActive())
        m_timerStart.stop();

    m_pattern = pattern;
    m_timerStart.startOneShot(0);
}

void Vibration::cancelVibration()
{
    m_pattern.clear();
    if (m_isVibrating) {
        m_vibrationClient->cancelVibration();
        m_isVibrating = false;
        m_timerStop.stop();
    }
}

void Vibration::suspendVibration()
{
    if (!m_isVibrating)
        return;

    m_pattern.insert(0, m_timerStop.nextFireInterval());
    m_timerStop.stop();
    cancelVibration();
}

void Vibration::resumeVibration()
{
    m_timerStart.startOneShot(0);
}

void Vibration::timerStartFired(Timer<Vibration>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_timerStart);

    m_timerStart.stop();

    if (m_pattern.size()) {
        m_isVibrating = true;
        m_vibrationClient->vibrate(m_pattern[0]);
        m_timerStop.startOneShot(m_pattern[0] / 1000.0);
        m_pattern.remove(0);
    }
}

void Vibration::timerStopFired(Timer<Vibration>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_timerStop);

    m_timerStop.stop();
    m_isVibrating = false;

    if (m_pattern.size()) {
        m_timerStart.startOneShot(m_pattern[0] / 1000.0);
        m_pattern.remove(0);
    }
}

const AtomicString& Vibration::supplementName()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("Vibration"));
    return name;
}

bool Vibration::isActive(Page* page)
{
    return static_cast<bool>(Vibration::from(page));
}

void provideVibrationTo(Page* page, VibrationClient* client)
{
    Vibration::provideTo(page, Vibration::supplementName(), Vibration::create(client));
}

} // namespace WebCore

#endif // ENABLE(VIBRATION)

