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

namespace IPC {
class DataReference;
}

namespace WebCore {
class ResourceError;
class ResourceResponse;
class SharedBuffer;
}

namespace WebKit {

class WebURLSchemeHandler;
class WebPageProxy;

using SyncLoadCompletionHandler = CompletionHandler<void(const WebCore::ResourceResponse&, const WebCore::ResourceError&, const Vector<char>&)>;

class WebURLSchemeTask : public ThreadSafeRefCounted<WebURLSchemeTask>, public InstanceCounted<WebURLSchemeTask> {
    WTF_MAKE_NONCOPYABLE(WebURLSchemeTask);
public:
    static Ref<WebURLSchemeTask> create(WebURLSchemeHandler&, WebPageProxy&, WebProcessProxy&, uint64_t identifier, WebCore::ResourceRequest&&, SyncLoadCompletionHandler&&);

    ~WebURLSchemeTask();

    uint64_t identifier() const { ASSERT(RunLoop::isMain()); return m_identifier; }
    WebCore::PageIdentifier pageID() const { ASSERT(RunLoop::isMain()); return m_pageIdentifier; }

    WebProcessProxy* process() { ASSERT(RunLoop::isMain()); return m_process.get(); }

#if PLATFORM(COCOA)
    NSURLRequest *nsRequest() const;
#endif
#if PLATFORM(WIN)
    const WebCore::ResourceRequest& request() const { ASSERT(RunLoop::isMain()); return m_request; }
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
    WebURLSchemeTask(WebURLSchemeHandler&, WebPageProxy&, WebProcessProxy&, uint64_t identifier, WebCore::ResourceRequest&&, SyncLoadCompletionHandler&&);

    bool isSync() const { return !!m_syncCompletionHandler; }

    Ref<WebURLSchemeHandler> m_urlSchemeHandler;
    WebPageProxy* m_page;
    RefPtr<WebProcessProxy> m_process;
    uint64_t m_identifier;
    WebCore::PageIdentifier m_pageIdentifier;
    WebCore::ResourceRequest m_request;
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
