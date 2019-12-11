/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)

#include <CoreVideo/CVDisplayLink.h>
#include <WebCore/PlatformScreen.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>

namespace IPC {
class Connection;
}

namespace WebKit {
    
class DisplayLink {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit DisplayLink(WebCore::PlatformDisplayID);
    ~DisplayLink();
    
    void addObserver(IPC::Connection&, unsigned observerID);
    void removeObserver(IPC::Connection&, unsigned observerID);
    void removeObservers(IPC::Connection&);
    bool hasObservers() const;

    WebCore::PlatformDisplayID displayID() const { return m_displayID; }

private:
    static CVReturn displayLinkCallback(CVDisplayLinkRef, const CVTimeStamp*, const CVTimeStamp*, CVOptionFlags, CVOptionFlags*, void* data);
    
    CVDisplayLinkRef m_displayLink { nullptr };
    Lock m_observersLock;
    HashMap<RefPtr<IPC::Connection>, Vector<unsigned>> m_observers;
    WebCore::PlatformDisplayID m_displayID;
};

} // namespace WebKit

#endif

