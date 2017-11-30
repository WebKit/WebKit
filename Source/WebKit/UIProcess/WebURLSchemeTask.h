/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <WebCore/ResourceRequest.h>
#include <wtf/InstanceCounted.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {
class ResourceError;
class ResourceResponse;
class SharedBuffer;
}

namespace WebKit {

class WebURLSchemeHandler;
class WebPageProxy;

class WebURLSchemeTask : public RefCounted<WebURLSchemeTask>, public InstanceCounted<WebURLSchemeTask> {
    WTF_MAKE_NONCOPYABLE(WebURLSchemeTask);
public:
    static Ref<WebURLSchemeTask> create(WebURLSchemeHandler&, WebPageProxy&, uint64_t identifier, WebCore::ResourceRequest&&);

    uint64_t identifier() const { return m_identifier; }
    uint64_t pageID() const { return m_pageIdentifier; }

    const WebCore::ResourceRequest& request() const { return m_request; }

    enum class ExceptionType {
        DataAlreadySent,
        CompleteAlreadyCalled,
        RedirectAfterResponse,
        TaskAlreadyStopped,
        NoResponseSent,
        None,
    };
    ExceptionType didPerformRedirection(WebCore::ResourceResponse&&, WebCore::ResourceRequest&&);
    ExceptionType didReceiveResponse(const WebCore::ResourceResponse&);
    ExceptionType didReceiveData(Ref<WebCore::SharedBuffer>);
    ExceptionType didComplete(const WebCore::ResourceError&);

    void stop();
    void pageDestroyed();

private:
    WebURLSchemeTask(WebURLSchemeHandler&, WebPageProxy&, uint64_t identifier, WebCore::ResourceRequest&&);

    Ref<WebURLSchemeHandler> m_urlSchemeHandler;
    WebPageProxy* m_page;
    uint64_t m_identifier;
    uint64_t m_pageIdentifier;
    WebCore::ResourceRequest m_request;
    bool m_stopped { false };
    bool m_responseSent { false };
    bool m_dataSent { false };
    bool m_completed { false };
};

} // namespace WebKit
