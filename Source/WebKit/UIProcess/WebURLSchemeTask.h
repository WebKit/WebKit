/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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

#include "APIObject.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/PageIdentifier.h>
#include <WebCore/ResourceLoaderIdentifier.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/CompletionHandler.h>
#include <wtf/InstanceCounted.h>
#include <wtf/Lock.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>

namespace API {
class FrameInfo;
}

namespace WebCore {
class ResourceError;
}

namespace WebKit {

struct URLSchemeTaskParameters;
class WebURLSchemeHandler;
class WebPageProxy;
class WebProcessProxy;

using SyncLoadCompletionHandler = CompletionHandler<void(const WebCore::ResourceResponse&, const WebCore::ResourceError&, Vector<uint8_t>&&)>;

class WebURLSchemeTask : public API::ObjectImpl<API::Object::Type::URLSchemeTask>, public InstanceCounted<WebURLSchemeTask> {
    WTF_MAKE_NONCOPYABLE(WebURLSchemeTask);
public:
    static Ref<WebURLSchemeTask> create(WebURLSchemeHandler&, WebPageProxy&, WebProcessProxy&, WebCore::PageIdentifier, URLSchemeTaskParameters&&, SyncLoadCompletionHandler&&);

    ~WebURLSchemeTask();

    WebCore::ResourceLoaderIdentifier resourceLoaderID() const { ASSERT(RunLoop::isMain()); return m_resourceLoaderID; }
    std::optional<WebPageProxyIdentifier> pageProxyID() const { ASSERT(RunLoop::isMain()); return m_pageProxyID; }
    std::optional<WebCore::PageIdentifier> webPageID() const { ASSERT(RunLoop::isMain()); return m_webPageID.asOptional(); }
    WebProcessProxy& process() { ASSERT(RunLoop::isMain()); return m_process.get(); }
    WebCore::ResourceRequest request() const;
    API::FrameInfo& frameInfo() const { return m_frameInfo.get(); }

#if PLATFORM(COCOA)
    NSURLRequest *nsRequest() const;
    RetainPtr<NSURLRequest> protectedNSRequest() const;
#endif

    enum class ExceptionType {
        DataAlreadySent,
        CompleteAlreadyCalled,
        RedirectAfterResponse,
        TaskAlreadyStopped,
        NoResponseSent,
        WaitingForRedirectCompletionHandler,
        None,
    };
    ExceptionType willPerformRedirection(WebCore::ResourceResponse&&, WebCore::ResourceRequest&&,  Function<void(WebCore::ResourceRequest&&)>&&);
    ExceptionType didPerformRedirection(WebCore::ResourceResponse&&, WebCore::ResourceRequest&&);
    ExceptionType didReceiveResponse(WebCore::ResourceResponse&&);
    ExceptionType didReceiveData(Ref<WebCore::SharedBuffer>&&);
    ExceptionType didComplete(const WebCore::ResourceError&);

    void stop();

    void suppressTaskStoppedExceptions() { m_shouldSuppressTaskStoppedExceptions = true; }

    bool waitingForRedirectCompletionHandlerCallback() const { return m_waitingForRedirectCompletionHandlerCallback; }

private:
    WebURLSchemeTask(WebURLSchemeHandler&, WebPageProxy&, WebProcessProxy&, WebCore::PageIdentifier, URLSchemeTaskParameters&&, SyncLoadCompletionHandler&&);

    bool isSync() const { return !!m_syncCompletionHandler; }

    const Ref<WebURLSchemeHandler> m_urlSchemeHandler;
    const Ref<WebProcessProxy> m_process;
    WebCore::ResourceLoaderIdentifier m_resourceLoaderID;
    Markable<WebPageProxyIdentifier> m_pageProxyID;
    Markable<WebCore::PageIdentifier> m_webPageID;
    WebCore::ResourceRequest m_request WTF_GUARDED_BY_LOCK(m_requestLock);
    const Ref<API::FrameInfo> m_frameInfo;
    mutable Lock m_requestLock;
    bool m_stopped { false };
    bool m_responseSent { false };
    bool m_dataSent { false };
    bool m_completed { false };
    bool m_shouldSuppressTaskStoppedExceptions { false };

    SyncLoadCompletionHandler m_syncCompletionHandler;
    WebCore::ResourceResponse m_syncResponse;
    WebCore::SharedBufferBuilder m_syncData;

    bool m_waitingForRedirectCompletionHandlerCallback { false };
};

} // namespace WebKit
