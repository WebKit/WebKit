/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include <WebCore/URL.h>

namespace WebKit {

class FrameLoadState {
public:
    ~FrameLoadState();

    enum class State {
        Provisional,
        Committed,
        Finished
    };

    void didStartProvisionalLoad(const WebCore::URL&);
    void didReceiveServerRedirectForProvisionalLoad(const WebCore::URL&);
    void didFailProvisionalLoad();

    void didCommitLoad();
    void didFinishLoad();
    void didFailLoad();

    void didSameDocumentNotification(const WebCore::URL&);

    State state() const { return m_state; }
    const WebCore::URL& url() const { return m_url; }
    void setURL(const WebCore::URL& url) { m_url = url; }
    const WebCore::URL& provisionalURL() const { return m_provisionalURL; }

    void setUnreachableURL(const WebCore::URL&);
    const WebCore::URL& unreachableURL() const { return m_unreachableURL; }

private:
    State m_state { State::Finished };
    WebCore::URL m_url;
    WebCore::URL m_provisionalURL;
    WebCore::URL m_unreachableURL;
    WebCore::URL m_lastUnreachableURL;
};

} // namespace WebKit
