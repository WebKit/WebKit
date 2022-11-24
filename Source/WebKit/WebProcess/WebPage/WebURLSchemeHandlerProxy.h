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

#include "WebURLSchemeHandlerIdentifier.h"
#include "WebURLSchemeTaskProxy.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>

namespace WebCore {
class ResourceError;
class ResourceLoader;
class ResourceResponse;
class ResourceRequest;
class SharedBuffer;
}

namespace WebKit {

class WebPage;

class WebURLSchemeHandlerProxy : public RefCounted<WebURLSchemeHandlerProxy> {
public:
    static Ref<WebURLSchemeHandlerProxy> create(WebPage& page, WebURLSchemeHandlerIdentifier identifier)
    {
        return adoptRef(*new WebURLSchemeHandlerProxy(page, identifier));
    }
    ~WebURLSchemeHandlerProxy();

    void startNewTask(WebCore::ResourceLoader&, WebFrame&);
    void stopAllTasks();

    void loadSynchronously(WebCore::ResourceLoaderIdentifier, WebFrame&, const WebCore::ResourceRequest&, WebCore::ResourceResponse&, WebCore::ResourceError&, Vector<uint8_t>&);

    WebURLSchemeHandlerIdentifier identifier() const { return m_identifier; }
    WebPage& page() { return m_webPage; }

    void taskDidPerformRedirection(WebCore::ResourceLoaderIdentifier, WebCore::ResourceResponse&&, WebCore::ResourceRequest&&, CompletionHandler<void(WebCore::ResourceRequest&&)>&&);
    void taskDidReceiveResponse(WebCore::ResourceLoaderIdentifier, const WebCore::ResourceResponse&);
    void taskDidReceiveData(WebCore::ResourceLoaderIdentifier, Ref<WebCore::SharedBuffer>&&);
    void taskDidComplete(WebCore::ResourceLoaderIdentifier, const WebCore::ResourceError&);
    void taskDidStopLoading(WebURLSchemeTaskProxy&);

private:
    WebURLSchemeHandlerProxy(WebPage&, WebURLSchemeHandlerIdentifier);

    RefPtr<WebURLSchemeTaskProxy> removeTask(WebCore::ResourceLoaderIdentifier);

    WebPage& m_webPage;
    WebURLSchemeHandlerIdentifier m_identifier;

    HashMap<WebCore::ResourceLoaderIdentifier, RefPtr<WebURLSchemeTaskProxy>> m_tasks;
}; // class WebURLSchemeHandlerProxy

} // namespace WebKit
