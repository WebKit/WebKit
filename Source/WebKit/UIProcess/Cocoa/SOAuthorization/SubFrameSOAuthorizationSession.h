/*
 * Copyright (C) 2019-2022 Apple Inc. All rights reserved.
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

#if HAVE(APP_SSO)

#include <variant>
#include "FrameLoadState.h"
#include "NavigationSOAuthorizationSession.h"
#include <WebCore/FrameIdentifier.h>
#include <wtf/Deque.h>

namespace WebKit {

class SubFrameSOAuthorizationSession final : public NavigationSOAuthorizationSession, public FrameLoadState::Observer {
public:
    using Callback = CompletionHandler<void(bool)>;

    static Ref<SOAuthorizationSession> create(RetainPtr<WKSOAuthorizationDelegate>, Ref<API::NavigationAction>&&, WebPageProxy&, Callback&&, WebCore::FrameIdentifier);

    ~SubFrameSOAuthorizationSession();

private:
    using Supplement = std::variant<Vector<uint8_t>, String>;

    SubFrameSOAuthorizationSession(RetainPtr<WKSOAuthorizationDelegate>, Ref<API::NavigationAction>&&, WebPageProxy&, Callback&&, WebCore::FrameIdentifier);

    // SOAuthorizationSession
    void fallBackToWebPathInternal() final;
    void abortInternal() final;
    void completeInternal(const WebCore::ResourceResponse&, NSData *) final;

    // NavigationSOAuthorizationSession
    void beforeStart() final;

    // FrameLoadState::Observer
    void didFinishLoad() final;

    void appendRequestToLoad(URL&&, Supplement&&);
    void loadRequestToFrame();

    WebCore::FrameIdentifier m_frameID;
    Deque<std::pair<URL, Supplement>> m_requestsToLoad;
};

} // namespace WebKit

#endif
