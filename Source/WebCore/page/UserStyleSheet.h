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

#include "PageIdentifier.h"
#include "UserContentTypes.h"
#include "UserStyleSheetTypes.h"
#include <wtf/URL.h>
#include <wtf/Vector.h>

namespace WebCore {

class UserStyleSheet {
    WTF_MAKE_FAST_ALLOCATED;
public:
    UserStyleSheet()
        : m_injectedFrames(UserContentInjectedFrames::InjectInAllFrames)
        , m_level(UserStyleUserLevel)
    {
    }

    UserStyleSheet(const String& source, const URL& url, Vector<String>&& allowlist, Vector<String>&& blocklist, UserContentInjectedFrames injectedFrames, UserStyleLevel level, std::optional<PageIdentifier> pageID = std::nullopt)
        : m_source(source)
        , m_url(url)
        , m_allowlist(WTFMove(allowlist))
        , m_blocklist(WTFMove(blocklist))
        , m_injectedFrames(injectedFrames)
        , m_level(level)
        , m_pageID(pageID)
    {
    }

    const String& source() const { return m_source; }
    const URL& url() const { return m_url; }
    const Vector<String>& allowlist() const { return m_allowlist; }
    const Vector<String>& blocklist() const { return m_blocklist; }
    UserContentInjectedFrames injectedFrames() const { return m_injectedFrames; }
    UserStyleLevel level() const { return m_level; }
    std::optional<PageIdentifier> pageID() const { return m_pageID; }

private:
    String m_source;
    URL m_url;
    Vector<String> m_allowlist;
    Vector<String> m_blocklist;
    UserContentInjectedFrames m_injectedFrames;
    UserStyleLevel m_level;
    std::optional<PageIdentifier> m_pageID;
};

} // namespace WebCore
