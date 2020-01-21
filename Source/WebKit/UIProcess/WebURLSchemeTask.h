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

#include "WebProcessProxy.h"
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/CompletionHandler.h>
#include <wtf/InstanceCounted.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace API {
class FrameInfo;
}

namespace IPC {
class DataReference;
}

namespace WebCore {
class ResourceError;
class ResourceResponse;
class SharedBuffer;
}

namespace WebKit {

struct URLSchemeTaskParameters;
class WebURLSchemeHandler;
class WebPageProxy;

using SyncLoadCompletionHandler = CompletionHandler<void(const WebCore::ResourceResponse&, const WebCore::ResourceError&, const Vector<char>&)>;

class WebURLSchemeTask : public ThreadSafeRefCounted<WebURLSchemeTask>, public InstanceCounted<WebURLSchemeTask> {
    WTF_MAKE_NONCOPYABLE(WebURLSchemeTask);
public:
    static Ref<WebURLSchemeTask> create(WebURLSchemeHandler&, WebPageProxy&, WebProcessProxy&, WebCore::PageIdentifier, URLSchemeTaskParameters&&, SyncLoadCompletionHandler&&);

    ~WebURLSchemeTask();

    uint64_t identifier() const { ASSERT(RunLoop::isMain()); return m_identifier; }
    WebPageProxyIdentifier pageProxyID() const { ASSERT(RunLoop::isMain()); return m_pageProxyID; }
    WebCore::PageIdentifier webPageID() const { ASSERT(RunLoop::isMain()); return m_webPageID; }
    WebProcessProxy* process() { ASSERT(RunLoop::isMain()); return m_process.get(); }
    const WebCore::ResourceRequest& request() const { ASSERT(RunLoop::isMain()); return m_request; }
    API::FrameInfo& frameInfo() const { return m_frameInfo.get(); }

#if PLATFORM(COCOA)
    NSURLRequest *nsRequest() const;
#endif

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
    ExceptionType didReceiveData(Ref<WebCore::SharedBuffer>&&);
    ExceptionType didComplete(const WebCore::ResourceError&);

    void stop();
    void pageDestroyed();

private:
    WebURLSchemeTask(WebURLSchemeHandler&, WebPageProxy&, WebProcessProxy&, WebCore::PageIdentifier, URLSchemeTaskParameters&&, SyncLoadCompletionHandler&&);

    bool isSync() const { return !!m_syncCompletionHandler; }

    Ref<WebURLSchemeHandler> m_urlSchemeHandler;
    RefPtr<WebProcessProxy> m_process;
    uint64_t m_identifier;
    WebPageProxyIdentifier m_pageProxyID;
    WebCore::PageIdentifier m_webPageID;
    WebCore::ResourceRequest m_request;
    Ref<API::FrameInfo> m_frameInfo;
    mutable Lock m_requestLock;
    bool m_stopped { false };
    bool m_responseSent { false };
    bool m_dataSent { false };
    bool m_completed { false };
    
    SyncLoadCompletionHandler m_syncCompletionHandler;
    WebCore::ResourceResponse m_syncResponse;
    RefPtr<WebCore::SharedBuffer> m_syncData;
};

} // namespace WebKit
