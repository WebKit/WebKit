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
    UserScript() = default;
    ~UserScript() = default;

    UserScript(String&& source, URL&& url, Vector<String>&& whitelist, Vector<String>&& blacklist, UserScriptInjectionTime injectionTime, UserContentInjectedFrames injectedFrames)
        : m_source(WTFMove(source))
        , m_url(WTFMove(url))
        , m_whitelist(WTFMove(whitelist))
        , m_blacklist(WTFMove(blacklist))
        , m_injectionTime(injectionTime)
        , m_injectedFrames(injectedFrames)
    {
    }

    const String& source() const { return m_source; }
    const URL& url() const { return m_url; }
    const Vector<String>& whitelist() const { return m_whitelist; }
    const Vector<String>& blacklist() const { return m_blacklist; }
    UserScriptInjectionTime injectionTime() const { return m_injectionTime; }
    UserContentInjectedFrames injectedFrames() const { return m_injectedFrames; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, UserScript&);

private:
    String m_source;
    URL m_url;
    Vector<String> m_whitelist;
    Vector<String> m_blacklist;
    UserScriptInjectionTime m_injectionTime { InjectAtDocumentStart };
    UserContentInjectedFrames m_injectedFrames { InjectInAllFrames };
};

template<class Encoder>
void UserScript::encode(Encoder& encoder) const
{
    encoder << m_source;
    encoder << m_url;
    encoder << m_whitelist;
    encoder << m_blacklist;
    encoder.encodeEnum(m_injectionTime);
    encoder.encodeEnum(m_injectedFrames);
}

template<class Decoder>
bool UserScript::decode(Decoder& decoder, UserScript& userScript)
{
    String source;
    if (!decoder.decode(source))
        return false;
    
    URL url;
    if (!decoder.decode(url))
        return false;
    
    Vector<String> whitelist;
    if (!decoder.decode(whitelist))
        return false;
    
    Vector<String> blacklist;
    if (!decoder.decode(blacklist))
        return false;
    
    UserScriptInjectionTime injectionTime;
    if (!decoder.decodeEnum(injectionTime))
        return false;
    
    UserContentInjectedFrames injectedFrames;
    if (!decoder.decodeEnum(injectedFrames))
        return false;
    
    userScript = { WTFMove(source), WTFMove(url), WTFMove(whitelist), WTFMove(blacklist), injectionTime, injectedFrames };
    return true;
}

} // namespace WebCore
