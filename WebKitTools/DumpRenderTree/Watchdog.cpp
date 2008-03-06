/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "Watchdog.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

static const double defaultWatchdogInterval = 30.0;

Watchdog::Watchdog()
    : m_shouldStop(false)
    , m_thread(0)
    , m_watchdogInterval(defaultWatchdogInterval)
{
}

Watchdog::~Watchdog()
{
    stop();
}

void Watchdog::start()
{
    MutexLocker locker(m_mutex);

    if (m_thread)
        return;
        
    m_shouldStop = false;
    m_thread = createThread(Watchdog::watchdogThreadStart, this);
}

void Watchdog::stop()
{
    MutexLocker locker(m_mutex);

    if (!m_thread)
        return;

    m_shouldStop = true;
    m_watchdogCondition.signal();
    m_stopCondition.wait(m_mutex);
    m_thread = 0;
}

void Watchdog::checkIn()
{
    MutexLocker locker(m_mutex);
    m_watchdogCondition.signal();
}

void Watchdog::setWatchdogInterval(double interval)
{
    MutexLocker locker(m_mutex);
    m_watchdogInterval = interval;
    if (m_thread)
        m_watchdogCondition.signal();
}

void Watchdog::handleHang()
{
    // No default implementation
    // Platforms that don't use some subclass of Watchdog with their own "handleHang()" will just get an _exit()'ed process by default
}

void* Watchdog::watchdogThreadStart(void* arg) 
{
    ASSERT(arg);
    return static_cast<Watchdog*>(arg)->watchdogThread();
}

void* Watchdog::watchdogThread()
{
    MutexLocker locker(m_mutex);
    
    do {      
        if (!m_watchdogCondition.timedWait(m_mutex, m_watchdogInterval)) {
            handleHang();
            _exit(1);
        }
    } while (!m_shouldStop);

    m_stopCondition.signal();
    return 0;
}
