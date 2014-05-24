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

#ifndef DisplayRefreshMonitorManager_h
#define DisplayRefreshMonitorManager_h

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)

#include "DisplayRefreshMonitor.h"
#include "PlatformScreen.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class DisplayRefreshMonitorManager {
    friend class NeverDestroyed<DisplayRefreshMonitorManager>;
public:
    static DisplayRefreshMonitorManager& sharedManager();
    
    void registerClient(DisplayRefreshMonitorClient*);
    void unregisterClient(DisplayRefreshMonitorClient*);

    bool scheduleAnimation(DisplayRefreshMonitorClient*);
    void windowScreenDidChange(PlatformDisplayID, DisplayRefreshMonitorClient*);

private:
    friend class DisplayRefreshMonitor;
    void displayDidRefresh(DisplayRefreshMonitor*);
    
    DisplayRefreshMonitorManager() { }
    virtual ~DisplayRefreshMonitorManager();

    DisplayRefreshMonitor* ensureMonitorForClient(DisplayRefreshMonitorClient*);

    // We know nothing about the values of PlatformDisplayIDs, so use UnsignedWithZeroKeyHashTraits.
    // FIXME: Since we know nothing about these values, this is not sufficient.
    // Even with UnsignedWithZeroKeyHashTraits, there are still two special values used for empty and deleted hash table slots.
    typedef HashMap<uint64_t, RefPtr<DisplayRefreshMonitor>, WTF::IntHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> DisplayRefreshMonitorMap;
    DisplayRefreshMonitorMap m_monitors;
};

}

#endif // USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)

#endif
