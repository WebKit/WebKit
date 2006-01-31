/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef TIMER_H
#define TIMER_H

namespace WebCore {

    // Fire times are in the same units as currentTime() in SystemTime.h.
    // In seconds, using the classic POSIX epoch of January 1, 1970.

    class TimerBase {
    public:
        typedef void (*TimerBaseFiredFunction)(TimerBase*);

        TimerBase(TimerBaseFiredFunction);
        ~TimerBase() { stop(); }

        void start(double nextFireTime, double repeatInterval);

        void startRepeating(double repeatInterval);
        void startOneShot(double interval);

        void stop();
        bool isActive() const;

        double nextFireTime() const;
        double repeatInterval() const;

        void fire();

    private:
        TimerBase(const TimerBase&);
        TimerBase& operator=(const TimerBase&);

        TimerBaseFiredFunction m_function;

    #if __APPLE__
        CFRunLoopTimerRef m_runLoopTimer;
    #endif
    };

    template <typename TimerFiredClass> class Timer : public TimerBase {
    public:
        typedef void (TimerFiredClass::*TimerFiredFunction)(Timer*);

        Timer(TimerFiredClass* o, TimerFiredFunction f)
            : TimerBase(fired), m_object(o), m_memberFunction(f)
        {
        }

        static void fired(TimerBase* b)
        {
            Timer* t = static_cast<Timer*>(b);
            (t->m_object->*t->m_memberFunction)(t);
        }

    private:
        TimerFiredClass* m_object;
        TimerFiredFunction m_memberFunction;
    };

}

#endif
