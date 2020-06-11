/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#include <wtf/URL.h>
#include "UserContentTypes.h"
#include "UserScriptTypes.h"
#include <wtf/Vector.h>

namespace WebCore {

class UserScript {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ~UserScript() = default;
    UserScript(const UserScript&) = default;
    UserScript(UserScript&&) = default;
    UserScript& operator=(const UserScript&) = default;
    UserScript& operator=(UserScript&&) = default;

    UserScript(String&& source, URL&& url, Vector<String>&& allowlist, Vector<String>&& blocklist, UserScriptInjectionTime injectionTime, UserContentInjectedFrames injectedFrames, WaitForNotificationBeforeInjecting waitForNotification)
        : m_source(WTFMove(source))
        , m_url(WTFMove(url))
        , m_allowlist(WTFMove(allowlist))
        , m_blocklist(WTFMove(blocklist))
        , m_injectionTime(injectionTime)
        , m_injectedFrames(injectedFrames)
        , m_waitForNotificationBeforeInjecting(waitForNotification)
    {
    }

    const String& source() const { return m_source; }
    const URL& url() const { return m_url; }
    const Vector<String>& allowlist() const { return m_allowlist; }
    const Vector<String>& blocklist() const { return m_blocklist; }
    UserScriptInjectionTime injectionTime() const { return m_injectionTime; }
    UserContentInjectedFrames injectedFrames() const { return m_injectedFrames; }
    WaitForNotificationBeforeInjecting waitForNotificationBeforeInjecting() const { return m_waitForNotificationBeforeInjecting; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<UserScript> decode(Decoder&);

private:
    String m_source;
    URL m_url;
    Vector<String> m_allowlist;
    Vector<String> m_blocklist;
    UserScriptInjectionTime m_injectionTime { UserScriptInjectionTime::DocumentStart };
    UserContentInjectedFrames m_injectedFrames { UserContentInjectedFrames::InjectInAllFrames };
    WaitForNotificationBeforeInjecting m_waitForNotificationBeforeInjecting { WaitForNotificationBeforeInjecting::No };
};

template<class Encoder>
void UserScript::encode(Encoder& encoder) const
{
    encoder << m_source;
    encoder << m_url;
    encoder << m_allowlist;
    encoder << m_blocklist;
    encoder << m_injectionTime;
    encoder << m_injectedFrames;
    encoder << m_waitForNotificationBeforeInjecting;
}

template<class Decoder>
Optional<UserScript> UserScript::decode(Decoder& decoder)
{
    Optional<String> source;
    decoder >> source;
    if (!source)
        return WTF::nullopt;
    
    Optional<URL> url;
    decoder >> url;
    if (!url)
        return WTF::nullopt;
    
    Optional<Vector<String>> allowlist;
    decoder >> allowlist;
    if (!allowlist)
        return WTF::nullopt;
    
    Optional<Vector<String>> blocklist;
    decoder >> blocklist;
    if (!blocklist)
        return WTF::nullopt;
    
    Optional<UserScriptInjectionTime> injectionTime;
    decoder >> injectionTime;
    if (!injectionTime)
        return WTF::nullopt;
    
    Optional<UserContentInjectedFrames> injectedFrames;
    decoder >> injectedFrames;
    if (!injectedFrames)
        return WTF::nullopt;
    
    Optional<WaitForNotificationBeforeInjecting> waitForNotification;
    decoder >> waitForNotification;
    if (!waitForNotification)
        return WTF::nullopt;
    
    return {{
        WTFMove(*source),
        WTFMove(*url),
        WTFMove(*allowlist),
        WTFMove(*blocklist),
        WTFMove(*injectionTime),
        WTFMove(*injectedFrames),
        WTFMove(*waitForNotification)
    }};
}

} // namespace WebCore
