/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "FindOverlaySession.h"

#include "APIFindClient.h"
#include "WebFindOptions.h"
#include "WebPageProxy.h"

namespace WebKit {

Ref<FindOverlaySession> FindOverlaySession::create(const String& string, OptionSet<FindOptions> options)
{
    return adoptRef(*new FindOverlaySession(string, options));
}

FindOverlaySession::FindOverlaySession(const String& string, OptionSet<FindOptions> options)
    : m_string(string)
    , m_options(options)
{
}

void FindOverlaySession::didSettle(HashMap<WebCore::FrameIdentifier, FindOverlayFrameResult>&& frameResults, uint32_t totalMatchCount)
{
    // The image analysis pass re-runs the full find, so a settlement can repeat
    // matches an earlier settlement already reported; take the larger count.
    for (auto& [frameID, result] : frameResults) {
        auto& accumulated = m_frameResults.add(frameID, FindOverlayFrameResult { }).iterator->value;
        accumulated.matchCount = std::max(accumulated.matchCount, result.matchCount);
        accumulated.didWrap |= result.didWrap;
    }
    m_totalMatchCount = std::max(m_totalMatchCount, totalMatchCount);
    m_settled = true;
}

void FindOverlaySession::deliverResult(WebPageProxy& page, std::optional<WebCore::FrameIdentifier> frameID, const Vector<WebCore::IntRect>& matchRects, uint32_t matchCount, int32_t matchIndex, bool didWrap)
{
    if (!frameID)
        page.findClient().didFailToFindString(&page, m_string);
    else
        page.findClient().didFindString(&page, m_string, matchRects, matchCount, matchIndex, didWrap);
}

bool FindOverlaySession::overlayShouldBeVisible() const
{
    return m_settled && m_totalMatchCount && m_options.contains(FindOptions::ShowOverlay);
}

} // namespace WebKit
