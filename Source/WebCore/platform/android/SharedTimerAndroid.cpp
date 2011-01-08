/*
 * Copyright 2007, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SharedTimer.h"

#define LOG_TAG "Timers"

#include <TimerClient.h>
#include <JavaSharedClient.h>
#include <utils/Log.h>
#include <wtf/CurrentTime.h>

using namespace android;

namespace WebCore {

// Single timer, shared to implement all the timers managed by the Timer class.
// Not intended to be used directly; use the Timer class instead.
void setSharedTimerFiredFunction(void (*f)())
{
    if (JavaSharedClient::GetTimerClient())
        JavaSharedClient::GetTimerClient()->setSharedTimerCallback(f);
}

// The fire time is relative to the classic POSIX epoch of January 1, 1970,
// as the result of currentTime() is.
void setSharedTimerFireTime(double fireTime)
{
    long long timeInMs = static_cast<long long>((fireTime - WTF::currentTime()) * 1000);

    LOGV("setSharedTimerFireTime: in %ld millisec", timeInMs);
    if (JavaSharedClient::GetTimerClient())
        JavaSharedClient::GetTimerClient()->setSharedTimer(timeInMs);
}

void stopSharedTimer()
{
    if (JavaSharedClient::GetTimerClient())
        JavaSharedClient::GetTimerClient()->stopSharedTimer();
}

}  // namespace WebCore
