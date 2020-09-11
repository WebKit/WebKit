/*
 * Copyright (C) 2010, 2014, 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "AnimationFrameRate.h"
#include "PlatformScreen.h"
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class DisplayAnimationClient;
class DisplayRefreshMonitorClient;

class DisplayRefreshMonitor : public ThreadSafeRefCounted<DisplayRefreshMonitor> {
public:
    static RefPtr<DisplayRefreshMonitor> create(DisplayRefreshMonitorClient&);
    WEBCORE_EXPORT virtual ~DisplayRefreshMonitor();

    virtual void displayLinkFired() { }

    virtual void setPreferredFramesPerSecond(FramesPerSecond) { }

    // Return true if callback request was scheduled, false if it couldn't be
    // (e.g., hardware refresh is not available)
    virtual bool requestRefreshCallback() = 0;

    virtual void stop() { }

    void windowScreenDidChange(PlatformDisplayID);
    
    bool hasClients() const { return m_clients.size(); }
    void addClient(DisplayRefreshMonitorClient&);
    bool removeClient(DisplayRefreshMonitorClient&);
    
    PlatformDisplayID displayID() const { return m_displayID; }

    bool shouldBeTerminated() const
    {
        const int maxInactiveFireCount = 1;
        return !m_scheduled && m_unscheduledFireCount > maxInactiveFireCount;
    }

    static RefPtr<DisplayRefreshMonitor> createDefaultDisplayRefreshMonitor(PlatformDisplayID);

protected:
    WEBCORE_EXPORT explicit DisplayRefreshMonitor(PlatformDisplayID);
    WEBCORE_EXPORT static void handleDisplayRefreshedNotificationOnMainThread(void* data);

    friend class DisplayRefreshMonitorManager;
    
    Lock& mutex() { return m_mutex; }

    bool isActive() const { return m_active; }
    void setIsActive(bool active) { m_active = active; }

    bool isScheduled() const { return m_scheduled; }
    void setIsScheduled(bool scheduled) { m_scheduled = scheduled; }

    bool isPreviousFrameDone() const { return m_previousFrameDone; }
    void setIsPreviousFrameDone(bool done) { m_previousFrameDone = done; }

    virtual bool hasRequestedRefreshCallback() const { return false; }

private:
    void displayDidRefresh();

    HashSet<DisplayRefreshMonitorClient*> m_clients;
    HashSet<DisplayRefreshMonitorClient*>* m_clientsToBeNotified { nullptr };
    Lock m_mutex;
    PlatformDisplayID m_displayID { 0 };
    int m_unscheduledFireCount { 0 }; // Number of times the display link has fired with no clients.
    bool m_active { true };
    bool m_scheduled { false };
    bool m_previousFrameDone { true };
};

}
