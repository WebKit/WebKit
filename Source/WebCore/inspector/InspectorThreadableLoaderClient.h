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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ThreadableLoaderClient.h"
#include <JavaScriptCore/InspectorBackendDispatchers.h> // for LoadResourceCallback.
#include <wtf/Forward.h>
#include <wtf/ThreadSafeRefCounted.h>

// FIXME: remove dependency on legacy callbacks in InspectorThreadableLoaderClient.
using LoadResourceCallback = Inspector::NetworkBackendDispatcherHandler::LoadResourceCallback;

namespace WebCore {
class TextResourceDecoder;
class ThreadableLoader;
}

namespace Inspector {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(InspectorThreadableLoaderClient);

class InspectorThreadableLoaderClient final : public ThreadSafeRefCounted<InspectorThreadableLoaderClient, WTF::DestructionThread::Main>, public WebCore::ThreadableLoaderClient {
    WTF_MAKE_NONCOPYABLE(InspectorThreadableLoaderClient);
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(InspectorThreadableLoaderClient, InspectorThreadableLoaderClient);
public:
    static Ref<InspectorThreadableLoaderClient> create(RefPtr<LoadResourceCallback>&& callback)
    {
        return adoptRef(*new InspectorThreadableLoaderClient(WTF::move(callback)));
    }

    ~InspectorThreadableLoaderClient() final = default;

    // WebCore::ThreadableLoaderClient.
    void ref() const final { ThreadSafeRefCounted::ref(); }
    void deref() const final { ThreadSafeRefCounted::deref(); }

    void didReceiveResponse(WebCore::ScriptExecutionContextIdentifier, std::optional<WebCore::ResourceLoaderIdentifier>, const WebCore::ResourceResponse&) override;
    void didReceiveData(const WebCore::SharedBuffer&) override;
    void didFinishLoading(WebCore::ScriptExecutionContextIdentifier, std::optional<WebCore::ResourceLoaderIdentifier>, const WebCore::NetworkLoadMetrics&) override;
    void didFail(std::optional<WebCore::ScriptExecutionContextIdentifier>, const WebCore::ResourceError&) override;
    void setLoader(RefPtr<WebCore::ThreadableLoader>&&);

private:
    explicit InspectorThreadableLoaderClient(RefPtr<LoadResourceCallback>&& callback)
        : m_callback(WTF::move(callback))
    {
        // FIXME: This is error-prone, we should avoid explicit calls to ref() / deref().
        ref(); // dispose() is in charge of calling deref();
    }

    void dispose();

    const RefPtr<LoadResourceCallback> m_callback;
    RefPtr<WebCore::ThreadableLoader> m_loader;
    RefPtr<WebCore::TextResourceDecoder> m_decoder;
    String m_mimeType;
    StringBuilder m_responseText;
    int m_statusCode;
    bool m_hasCalledDeref { false };
};

} // namespace Inspector
