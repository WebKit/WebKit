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
#include "DisplayUpdate.h"
#include "PlatformScreen.h"
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class DisplayAnimationClient;
class DisplayRefreshMonitorClient;
class DisplayRefreshMonitorFactory;

class DisplayRefreshMonitor : public ThreadSafeRefCounted<DisplayRefreshMonitor> {
    friend class DisplayRefreshMonitorManager;
public:
    static RefPtr<DisplayRefreshMonitor> create(DisplayRefreshMonitorFactory*, PlatformDisplayID);
    WEBCORE_EXPORT virtual ~DisplayRefreshMonitor();
    
    WEBCORE_EXPORT virtual void stop();

    // Return true if callback request was scheduled, false if it couldn't be
    // (e.g., hardware refresh is not available)
    WEBCORE_EXPORT virtual bool requestRefreshCallback();

    void windowScreenDidChange(PlatformDisplayID);
    
    bool hasClients() const { return m_clients.size(); }
    void addClient(DisplayRefreshMonitorClient&);
    bool removeClient(DisplayRefreshMonitorClient&);

    void clientPreferredFramesPerSecondChanged(DisplayRefreshMonitorClient&);
    std::optional<FramesPerSecond> maxClientPreferredFramesPerSecond() const { return m_maxClientPreferredFramesPerSecond; }

    virtual std::optional<FramesPerSecond> displayNominalFramesPerSecond() { return std::nullopt; }

    PlatformDisplayID displayID() const { return m_displayID; }

    static RefPtr<DisplayRefreshMonitor> createDefaultDisplayRefreshMonitor(PlatformDisplayID);
    WEBCORE_EXPORT virtual void displayLinkFired(const DisplayUpdate&);

protected:
    WEBCORE_EXPORT explicit DisplayRefreshMonitor(PlatformDisplayID);

    WEBCORE_EXPORT virtual void dispatchDisplayDidRefresh(const DisplayUpdate&);

    Lock& lock() { return m_lock; }
    void setMaxUnscheduledFireCount(unsigned count) { m_maxUnscheduledFireCount = count; }

    // Returns true if the start was successful.
    WEBCORE_EXPORT virtual bool startNotificationMechanism() = 0;
    WEBCORE_EXPORT virtual void stopNotificationMechanism() = 0;

    bool isScheduled() const { return m_scheduled; }
    void setIsScheduled(bool scheduled) { m_scheduled = scheduled; }

    bool isPreviousFrameDone() const { return m_previousFrameDone; }
    void setIsPreviousFrameDone(bool done) { m_previousFrameDone = done; }

    WEBCORE_EXPORT void displayDidRefresh(const DisplayUpdate&);

private:
    bool firedAndReachedMaxUnscheduledFireCount();

    virtual void adjustPreferredFramesPerSecond(FramesPerSecond) { }

    std::optional<FramesPerSecond> maximumClientPreferredFramesPerSecond() const;
    void computeMaxPreferredFramesPerSecond();

    HashSet<DisplayRefreshMonitorClient*> m_clients;
    HashSet<DisplayRefreshMonitorClient*>* m_clientsToBeNotified { nullptr };

    PlatformDisplayID m_displayID { 0 };
    std::optional<FramesPerSecond> m_maxClientPreferredFramesPerSecond;

    Lock m_lock;
    bool m_scheduled { false };
    bool m_previousFrameDone { true };
    
    unsigned m_unscheduledFireCount { 0 };
    unsigned m_maxUnscheduledFireCount { 0 };
};

}
