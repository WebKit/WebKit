/*
 * Copyright (C) 2009-2025 Apple Inc. All rights reserved.
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

#include <WebCore/UserContentTypes.h>
#include <WebCore/UserScriptTypes.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>

namespace WebCore {

class UserScript {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(UserScript, WEBCORE_EXPORT);
public:
    ~UserScript() = default;
    UserScript(const UserScript&) = default;
    UserScript(UserScript&&) = default;
    UserScript& operator=(const UserScript&) = default;
    UserScript& operator=(UserScript&&) = default;

    WEBCORE_EXPORT UserScript(String&& source, URL&& = { }, Vector<String>&& allowlist = { }, Vector<String>&& blocklist = { }, UserScriptInjectionTime = UserScriptInjectionTime::DocumentStart, UserContentInjectedFrames = UserContentInjectedFrames::InjectInAllFrames, UserContentMatchParentFrame = UserContentMatchParentFrame::Never);

    const String& source() const { return m_source; }
    const URL& url() const { return m_url; }
    const Vector<String>& allowlist() const { return m_allowlist; }
    const Vector<String>& blocklist() const { return m_blocklist; }
    UserScriptInjectionTime injectionTime() const { return m_injectionTime; }
    UserContentInjectedFrames injectedFrames() const { return m_injectedFrames; }
    UserContentMatchParentFrame matchParentFrame() const { return m_matchParentFrame; }
    String debugDescription() const;

private:
    String m_source;
    URL m_url;
    Vector<String> m_allowlist;
    Vector<String> m_blocklist;
    UserScriptInjectionTime m_injectionTime { UserScriptInjectionTime::DocumentStart };
    UserContentInjectedFrames m_injectedFrames { UserContentInjectedFrames::InjectInAllFrames };
    UserContentMatchParentFrame m_matchParentFrame { UserContentMatchParentFrame::Never };
};

} // namespace WebCore
