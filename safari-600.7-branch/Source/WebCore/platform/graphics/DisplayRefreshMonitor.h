/*
 * Copyright (C) 2010, 2014 Apple Inc. All rights reserved.
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

#ifndef DisplayRefreshMonitor_h
#define DisplayRefreshMonitor_h

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)

#include "PlatformScreen.h"
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Threading.h>

namespace WebCore {

class DisplayAnimationClient;
class DisplayRefreshMonitorClient;

class DisplayRefreshMonitor : public RefCounted<DisplayRefreshMonitor> {
public:
    static RefPtr<DisplayRefreshMonitor> create(DisplayRefreshMonitorClient&);
    virtual ~DisplayRefreshMonitor();
    
    // Return true if callback request was scheduled, false if it couldn't be
    // (e.g., hardware refresh is not available)
    virtual bool requestRefreshCallback() = 0;
    void windowScreenDidChange(PlatformDisplayID);
    
    bool hasClients() const { return m_clients.size(); }
    void addClient(DisplayRefreshMonitorClient&);
    bool removeClient(DisplayRefreshMonitorClient&);
    
    PlatformDisplayID displayID() const { return m_displayID; }

    bool shouldBeTerminated() const
    {
        const int maxInactiveFireCount = 10;
        return !m_scheduled && m_unscheduledFireCount > maxInactiveFireCount;
    }

    bool isActive() const { return m_active; }
    void setIsActive(bool active) { m_active = active; }

    bool isScheduled() const { return m_scheduled; }
    void setIsScheduled(bool scheduled) { m_scheduled = scheduled; }

    bool isPreviousFrameDone() const { return m_previousFrameDone; }
    void setIsPreviousFrameDone(bool done) { m_previousFrameDone = done; }

    void setMonotonicAnimationStartTime(double startTime) { m_monotonicAnimationStartTime = startTime; }

    Mutex& mutex() { return m_mutex; }

    static RefPtr<DisplayRefreshMonitor> createDefaultDisplayRefreshMonitor(PlatformDisplayID);

protected:
    explicit DisplayRefreshMonitor(PlatformDisplayID);
    static void handleDisplayRefreshedNotificationOnMainThread(void* data);

private:
    void displayDidRefresh();

    double m_monotonicAnimationStartTime;
    bool m_active;
    bool m_scheduled;
    bool m_previousFrameDone;
    int m_unscheduledFireCount; // Number of times the display link has fired with no clients.
    PlatformDisplayID m_displayID;
    Mutex m_mutex;

    HashSet<DisplayRefreshMonitorClient*> m_clients;
    HashSet<DisplayRefreshMonitorClient*>* m_clientsToBeNotified;
};

}

#endif // USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)

#endif
