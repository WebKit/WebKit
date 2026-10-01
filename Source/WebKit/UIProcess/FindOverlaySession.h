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

#pragma once

#include <WebCore/FrameIdentifier.h>
#include <WebCore/IntRect.h>
#include <wtf/HashMap.h>
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class WebPageProxy;

enum class FindOptions : uint16_t;

struct FindOverlayFrameResult {
    uint32_t matchCount { 0 };
    bool didWrap { false };
};

// One session per find action in the UI process. The session is the single
// authority for whether the find overlay should be visible for the page, fed
// by the aggregated per-process results. Staleness is expressed by identity:
// a newer find action replaces the page's session, so results settling into a
// superseded session have no effect.
class FindOverlaySession : public RefCounted<FindOverlaySession> {
public:
    static Ref<FindOverlaySession> create(const String&, OptionSet<FindOptions>);

    void didSettle(HashMap<WebCore::FrameIdentifier, FindOverlayFrameResult>&&, uint32_t totalMatchCount);
    void deliverResult(WebPageProxy&, std::optional<WebCore::FrameIdentifier>, const Vector<WebCore::IntRect>& matchRects, uint32_t matchCount, int32_t matchIndex, bool didWrap);

    bool overlayShouldBeVisible() const;

private:
    FindOverlaySession(const String&, OptionSet<FindOptions>);

    String m_string;
    OptionSet<FindOptions> m_options;
    HashMap<WebCore::FrameIdentifier, FindOverlayFrameResult> m_frameResults;
    uint32_t m_totalMatchCount { 0 };
    bool m_settled { false };
};

} // namespace WebKit
