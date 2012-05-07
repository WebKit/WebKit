/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
#include <wtf/Threading.h>
#include <wtf/Vector.h>
#if PLATFORM(BLACKBERRY)
#include <BlackBerryPlatformAnimation.h>
#endif

#if PLATFORM(MAC)
typedef struct __CVDisplayLink *CVDisplayLinkRef;
#endif

namespace WebCore {

class DisplayRefreshMonitor;
class DisplayRefreshMonitorManager;

//
// Abstract virtual client which receives refresh fired messages on the main thread
//
class DisplayRefreshMonitorClient {
    friend class DisplayRefreshMonitor;
    friend class DisplayRefreshMonitorManager;
    
public:
    DisplayRefreshMonitorClient();
    virtual ~DisplayRefreshMonitorClient();
    
    virtual void displayRefreshFired(double timestamp) = 0;

private:
    void fireDisplayRefreshIfNeeded(double timestamp);
    
    void setDisplayID(PlatformDisplayID displayID)
    {
        m_displayID = displayID;
        m_displayIDIsSet = true;
    }
    
    bool m_scheduled;
    bool m_displayIDIsSet;
    PlatformDisplayID m_displayID;
};

#if PLATFORM(BLACKBERRY)
class DisplayAnimationClient : public BlackBerry::Platform::AnimationFrameRateClient {
public:
    DisplayAnimationClient(DisplayRefreshMonitor *);
    ~DisplayAnimationClient() { }
private:
    virtual void animationFrameChanged();
    DisplayRefreshMonitor *m_monitor;
};
#endif

//
// Monitor for display refresh messages for a given screen
//

class DisplayRefreshMonitor {
public:
    DisplayRefreshMonitor(PlatformDisplayID);
    ~DisplayRefreshMonitor();
    
    // Return true if callback request was scheduled, false if it couldn't be
    // (e.g., hardware refresh is not available)
    bool requestRefreshCallback();
    void windowScreenDidChange(PlatformDisplayID);
    
    bool hasClients() const { return m_clients.size(); }
    void addClient(DisplayRefreshMonitorClient* client) { m_clients.append(client); }
    bool removeClient(DisplayRefreshMonitorClient* client)
    {
        size_t i = m_clients.find(client);
        if (i == notFound)
            return false;
        m_clients.remove(i);
        return true;
    }
    
    PlatformDisplayID displayID() const { return m_displayID; }

private:
    void notifyClients();
    static void refreshDisplayOnMainThread(void* data);

    double m_timestamp;
    bool m_active;
    bool m_scheduled;
    bool m_previousFrameDone;
    PlatformDisplayID m_displayID;
    DisplayRefreshMonitorManager* m_manager;
    Mutex m_mutex;
    Vector<DisplayRefreshMonitorClient*> m_clients;
#if PLATFORM(BLACKBERRY)
public:
    void displayLinkFired();
private:
    DisplayAnimationClient *m_animationClient;
    void startAnimationClient();
    void stopAnimationClient();
#endif
#if PLATFORM(MAC)
public:
    void displayLinkFired(double nowSeconds, double outputTimeSeconds);
private:
    CVDisplayLinkRef m_displayLink;
#endif
};

//
// Singleton manager for all the DisplayRefreshMonitors. This is the interface to the 
// outside world. It distributes requests to the appropriate monitor. When the display
// refresh event fires, the passed DisplayRefreshMonitorClient is called directly on
// the main thread.
//
class DisplayRefreshMonitorManager {
public:
    static DisplayRefreshMonitorManager* sharedManager();
    
    void registerClient(DisplayRefreshMonitorClient*);
    void unregisterClient(DisplayRefreshMonitorClient*);

    bool scheduleAnimation(DisplayRefreshMonitorClient*);
    void windowScreenDidChange(PlatformDisplayID, DisplayRefreshMonitorClient*);

private:
    DisplayRefreshMonitorManager() { }

    size_t findMonitor(PlatformDisplayID) const;
    
    Vector<DisplayRefreshMonitor*> m_monitors;
};

}

#endif // USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)

#endif
