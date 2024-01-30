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

#pragma once

#include <WebCore/FrameIdentifier.h>
#include <wtf/CompletionHandler.h>

namespace WebKit {

class WebFrameProxy;
class WebPageProxy;

enum class FindOptions : uint16_t;

class FindStringCallbackAggregator : public RefCounted<FindStringCallbackAggregator> {
public:
    static Ref<FindStringCallbackAggregator> create(WebPageProxy&, const String&, OptionSet<FindOptions>, unsigned maxMatchCount, CompletionHandler<void(bool)>&&);
    void foundString(std::optional<WebCore::FrameIdentifier>, bool didWrap);
    ~FindStringCallbackAggregator();

private:
    FindStringCallbackAggregator(WebPageProxy&, const String&, OptionSet<FindOptions>, unsigned maxMatchCount, CompletionHandler<void(bool)>&&);

    RefPtr<WebFrameProxy> incrementFrame(WebFrameProxy&);
    bool shouldTargetFrame(WebFrameProxy&, WebFrameProxy& focusedFrame, bool didWrap);

    WeakPtr<WebPageProxy> m_page;
    String m_string;
    OptionSet<FindOptions> m_options;
    unsigned m_maxMatchCount;
    CompletionHandler<void(bool)> m_completionHandler;
    HashMap<WebCore::FrameIdentifier, bool> m_matches;
};

} // namespace WebKit
