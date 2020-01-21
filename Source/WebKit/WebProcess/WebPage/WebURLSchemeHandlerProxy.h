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

#include "WebURLSchemeTaskProxy.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>

namespace WebCore {
class ResourceError;
class ResourceLoader;
class ResourceResponse;
class ResourceRequest;
}

namespace WebKit {

class WebPage;
typedef uint64_t ResourceLoadIdentifier;

class WebURLSchemeHandlerProxy : public RefCounted<WebURLSchemeHandlerProxy> {
public:
    static Ref<WebURLSchemeHandlerProxy> create(WebPage& page, uint64_t identifier)
    {
        return adoptRef(*new WebURLSchemeHandlerProxy(page, identifier));
    }
    ~WebURLSchemeHandlerProxy();

    void startNewTask(WebCore::ResourceLoader&, WebFrame&);
    void stopAllTasks();

    void loadSynchronously(ResourceLoadIdentifier, WebFrame&, const WebCore::ResourceRequest&, WebCore::ResourceResponse&, WebCore::ResourceError&, Vector<char>&);

    uint64_t identifier() const { return m_identifier; }
    WebPage& page() { return m_webPage; }

    void taskDidPerformRedirection(uint64_t taskIdentifier, WebCore::ResourceResponse&&, WebCore::ResourceRequest&&);
    void taskDidReceiveResponse(uint64_t taskIdentifier, const WebCore::ResourceResponse&);
    void taskDidReceiveData(uint64_t taskIdentifier, size_t, const uint8_t* data);
    void taskDidComplete(uint64_t taskIdentifier, const WebCore::ResourceError&);
    void taskDidStopLoading(WebURLSchemeTaskProxy&);

private:
    WebURLSchemeHandlerProxy(WebPage&, uint64_t identifier);

    RefPtr<WebURLSchemeTaskProxy> removeTask(uint64_t identifier);

    WebPage& m_webPage;
    uint64_t m_identifier { 0 };

    HashMap<uint64_t, RefPtr<WebURLSchemeTaskProxy>> m_tasks;
}; // class WebURLSchemeHandlerProxy

} // namespace WebKit
