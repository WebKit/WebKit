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

#include "APIObject.h"

#include <WebCore/UserContentURLPattern.h>
#include <wtf/URL.h>

namespace API {

class UserContentURLPattern : public API::ObjectImpl<API::Object::Type::UserContentURLPattern> {
public:
    static Ref<UserContentURLPattern> create(const WTF::String& pattern)
    {
        return adoptRef(*new UserContentURLPattern(pattern));
    }

    const WTF::String& host() const { return m_pattern.host(); }
    const WTF::String& scheme() const { return m_pattern.scheme(); }
    bool isValid() const { return m_pattern.isValid(); };
    bool matchesURL(const WTF::String& url) const { return m_pattern.matches(WTF::URL({ }, url)); }
    bool matchesSubdomains() const { return m_pattern.matchSubdomains(); }

    const WTF::String& patternString() const { return m_patternString; }

private:
    explicit UserContentURLPattern(const WTF::String& pattern)
        : m_pattern(WebCore::UserContentURLPattern(pattern))
        , m_patternString(pattern)
    {
    }

    WebCore::UserContentURLPattern m_pattern;
    WTF::String m_patternString;
};

}
