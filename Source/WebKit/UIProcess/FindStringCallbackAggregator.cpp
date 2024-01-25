/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "FindStringCallbackAggregator.h"

#include "WebFrameProxy.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"

namespace WebKit {

using namespace WebCore;

Ref<FindStringCallbackAggregator> FindStringCallbackAggregator::create(WebPageProxy& page, const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount, CompletionHandler<void(bool)>&& completionHandler)
{
    return adoptRef(*new FindStringCallbackAggregator(page, string, options, maxMatchCount, WTFMove(completionHandler)));
}

void FindStringCallbackAggregator::foundString(std::optional<FrameIdentifier> frameID, bool didWrap)
{
    if (!frameID)
        return;

    m_matches.set(*frameID, didWrap);
}

static inline bool shouldTargetFrame(const WebFrameProxy& frame, const WebFrameProxy& focusedFrame, bool didWrap)
{
    if (!didWrap)
        return true;

    if (frame.process() != focusedFrame.process())
        return true;

    RefPtr nextFrameInProcess = focusedFrame.traverseNext(CanWrap::Yes).frame;
    while (nextFrameInProcess && nextFrameInProcess != &focusedFrame && nextFrameInProcess->process() == focusedFrame.process()) {
        if (nextFrameInProcess == &frame)
            return true;
        nextFrameInProcess = nextFrameInProcess->traverseNext(CanWrap::Yes).frame;
    }
    return false;
}

FindStringCallbackAggregator::~FindStringCallbackAggregator()
{
    RefPtr protectedPage = m_page.get();
    if (!protectedPage) {
        m_completionHandler(false);
        return;
    }

    RefPtr focusedFrame = protectedPage->focusedOrMainFrame();
    if (!focusedFrame) {
        m_completionHandler(false);
        return;
    }

    RefPtr targetFrame = focusedFrame.get();
    do {
        auto it = m_matches.find(targetFrame->frameID());
        if (it != m_matches.end()) {
            if (shouldTargetFrame(*targetFrame, *focusedFrame, it->value))
                break;
        }
        // FIXME(267905): This should have support for backwards traversal.
        // FIXME(267904): This probably shouldn't always wrap if FindOptions::WrapAround is not in m_options.
        targetFrame = targetFrame->traverseNext(CanWrap::Yes).frame;
    } while (targetFrame && targetFrame != focusedFrame);

    if (!targetFrame) {
        m_completionHandler(false);
        return;
    }

    targetFrame->process().sendWithAsyncReply(Messages::WebPage::FindString(m_string, m_options, m_maxMatchCount), [completionHandler = WTFMove(m_completionHandler)](std::optional<FrameIdentifier> frameID, bool) mutable {
        completionHandler(frameID.has_value());
    }, protectedPage->webPageID());

    if (focusedFrame && focusedFrame->process() != targetFrame->process())
        focusedFrame->process().send(Messages::WebPage::ClearSelection(), protectedPage->webPageID());
}

FindStringCallbackAggregator::FindStringCallbackAggregator(WebPageProxy& page, const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount, CompletionHandler<void(bool)>&& completionHandler)
    : m_page(page)
    , m_string(string)
    , m_options(options)
    , m_maxMatchCount(maxMatchCount)
    , m_completionHandler(WTFMove(completionHandler))
{
}

} // namespace WebKit
