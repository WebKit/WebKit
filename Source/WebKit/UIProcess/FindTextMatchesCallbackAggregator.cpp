/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "FindTextMatchesCallbackAggregator.h"

#include "WebFoundTextRange.h"
#include "WebFrameProxy.h"
#include "WebPageProxy.h"
#include <wtf/StdLibExtras.h>

namespace WebKit {

Ref<FindTextMatchCallbackAggregator> FindTextMatchCallbackAggregator::create(WebPageProxy& page, CompletionHandler<void(Vector<WebFoundTextRange>&&)>&& completionHandler)
{
    return adoptRef(*new FindTextMatchCallbackAggregator(page, WTF::move(completionHandler)));
}

void FindTextMatchCallbackAggregator::foundMatches(HashMap<WebCore::FrameIdentifier, Vector<WebFoundTextRange>>&& matches)
{
    for (auto& [frameId, match] : matches)
        m_frameMatches.set(frameId, match);
}

FindTextMatchCallbackAggregator::~FindTextMatchCallbackAggregator()
{
    Vector<WebFoundTextRange> ranges;
    uint64_t frameOrder = 0;

    RefPtr protectedPage = m_page.get();
    if (!protectedPage) {
        m_completionHandler(WTF::move(ranges));
        return;
    }

    for (RefPtr frame = protectedPage->mainFrame(); frame; frame = frame->traverseNext().frame) {
        const auto frameID = frame->frameID();
        if (auto it = m_frameMatches.find(frameID); it != m_frameMatches.end()) {
            for (auto& match : it->value) {
                match.order = frameOrder;
                ranges.append(match);
            }
        }
        frameOrder++;
    }

    m_completionHandler(WTF::move(ranges));
}

FindTextMatchCallbackAggregator::FindTextMatchCallbackAggregator(WebPageProxy& page, CompletionHandler<void(Vector<WebFoundTextRange>&&)>&& completionHandler)
    : m_page(page)
    , m_completionHandler(WTF::move(completionHandler))
{
}

} // namespace WebKit
