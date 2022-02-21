/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#include "ResourceResponse.h"
#include "SharedBuffer.h"
#include <wtf/RefPtr.h>
#include <wtf/URL.h>

namespace WebCore {

    class SubstituteData {
    public:
        enum class SessionHistoryVisibility : bool {
            Visible,
            Hidden,
        };

        SubstituteData()
        {
        }

        SubstituteData(RefPtr<FragmentedSharedBuffer>&& content, const URL& failingURL, const ResourceResponse& response, SessionHistoryVisibility shouldRevealToSessionHistory)
            : m_content(WTFMove(content))
            , m_failingURL(failingURL)
            , m_response(response)
            , m_shouldRevealToSessionHistory(shouldRevealToSessionHistory)
        {
        }

        bool isValid() const { return m_content != nullptr; }
        bool shouldRevealToSessionHistory() const { return m_shouldRevealToSessionHistory == SessionHistoryVisibility::Visible; }

        const FragmentedSharedBuffer* content() const { return m_content.get(); }
        const String& mimeType() const { return m_response.mimeType(); }
        const String& textEncoding() const { return m_response.textEncodingName(); }
        const URL& failingURL() const { return m_failingURL; }
        const ResourceResponse& response() const { return m_response; }

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static std::optional<SubstituteData> decode(Decoder&);

    private:
        RefPtr<FragmentedSharedBuffer> m_content;
        URL m_failingURL;
        ResourceResponse m_response;
        SessionHistoryVisibility m_shouldRevealToSessionHistory { SessionHistoryVisibility::Hidden };
    };

template<class Encoder>
void SubstituteData::encode(Encoder& encoder) const
{
    encoder << m_content << m_failingURL << m_response << m_shouldRevealToSessionHistory;
}

template<class Decoder>
std::optional<SubstituteData> SubstituteData::decode(Decoder& decoder)
{
    std::optional<RefPtr<FragmentedSharedBuffer>> content;
    decoder >> content;
    if (!content)
        return std::nullopt;

    std::optional<URL> failingURL;
    decoder >> failingURL;
    if (!failingURL)
        return std::nullopt;

    std::optional<ResourceResponse> response;
    decoder >> response;
    if (!response)
        return std::nullopt;

    std::optional<SessionHistoryVisibility> shouldRevealToSessionHistory;
    decoder >> shouldRevealToSessionHistory;
    if (!shouldRevealToSessionHistory)
        return std::nullopt;

    return { { WTFMove(*content), *failingURL, *response, *shouldRevealToSessionHistory } };
}

} // namespace WebCore
