/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "config.h"
#import "UserActivity.h"

namespace WebCore {

#if HAVE(NS_ACTIVITY)

static const double kHysteresisSeconds = 5.0;

UserActivity::UserActivity(const char* description)
    : m_active(false)
    , m_description([NSString stringWithUTF8String:description])
    , m_timer(RunLoop::main(), this, &UserActivity::hysteresisTimerFired)
{
    ASSERT(isValid());
}

bool UserActivity::isValid()
{
    // If count is non-zero then we should be holding an activity, and the hysteresis timer should not be running.
    // Else if count is zero then:
    //  (a) if we're holding an activity there should be an active timer to clear this,
    //  (b) if we're not holding an activity there should be no active timer.
    return m_active ? m_activity && !m_timer.isActive() : !!m_activity == m_timer.isActive();
}

void UserActivity::start()
{
    ASSERT(isValid());

    if (m_active)
        return;
    m_active = true;

    if (m_timer.isActive())
        m_timer.stop();
    if (!m_activity) {
        NSActivityOptions options = (NSActivityUserInitiatedAllowingIdleSystemSleep | NSActivityLatencyCritical) & ~(NSActivitySuddenTerminationDisabled | NSActivityAutomaticTerminationDisabled);
        m_activity = [[NSProcessInfo processInfo] beginActivityWithOptions:options reason:m_description.get()];
    }

    ASSERT(isValid());
}

void UserActivity::stop()
{
    ASSERT(isValid());

    if (!m_active)
        return;
    m_active = false;

    m_timer.startOneShot(kHysteresisSeconds);

    ASSERT(isValid());
}

void UserActivity::hysteresisTimerFired()
{
    ASSERT(isValid());

    [[NSProcessInfo processInfo] endActivity:m_activity.get()];
    m_activity.clear();
    m_timer.stop();

    ASSERT(isValid());
}

#endif

} // namespace WebCore
