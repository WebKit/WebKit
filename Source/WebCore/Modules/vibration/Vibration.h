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

#ifndef Vibration_h
#define Vibration_h

#if ENABLE(VIBRATION)

#include "Page.h"
#include "Timer.h"

namespace WebCore {

class VibrationClient;

class Vibration : public Supplement<Page> {
public:
    typedef Vector<unsigned> VibrationPattern;

    explicit Vibration(VibrationClient*);
    ~Vibration();

    bool vibrate(const VibrationPattern&);
    // FIXME: When a visibilitychange event is dispatched while vibrating, the vibration should be canceled.
    void cancelVibration();

    void timerFired(Timer*);

    static const char* supplementName();
    static Vibration* from(Page* page) { return static_cast<Vibration*>(Supplement<Page>::from(page, supplementName())); }

    bool isVibrating() { return m_state != State::Idle; }

private:
    enum class State { Idle, Vibrating, Waiting };

    VibrationClient* m_vibrationClient;
    Timer m_timer;
    State m_state;
    VibrationPattern m_pattern;
};

} // namespace WebCore

#endif // ENABLE(VIBRATION)

#endif // Vibration_h

