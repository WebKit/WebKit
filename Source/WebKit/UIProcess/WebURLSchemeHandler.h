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

#include "WebURLSchemeTask.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace IPC {
class DataReference;
}

namespace WebCore {
class ResourceRequest;
}

namespace WebKit {

struct URLSchemeTaskParameters;
class WebPageProxy;
class WebProcessProxy;

using SyncLoadCompletionHandler = CompletionHandler<void(const WebCore::ResourceResponse&, const WebCore::ResourceError&, const Vector<char>&)>;

class WebURLSchemeHandler : public RefCounted<WebURLSchemeHandler> {
    WTF_MAKE_NONCOPYABLE(WebURLSchemeHandler);
public:
    virtual ~WebURLSchemeHandler();

    uint64_t identifier() const { return m_identifier; }

    void startTask(WebPageProxy&, WebProcessProxy&, WebCore::PageIdentifier, URLSchemeTaskParameters&&, SyncLoadCompletionHandler&&);
    void stopTask(WebPageProxy&, uint64_t taskIdentifier);
    void stopAllTasksForPage(WebPageProxy&, WebProcessProxy*);
    void taskCompleted(WebURLSchemeTask&);

protected:
    WebURLSchemeHandler();

private:
    virtual void platformStartTask(WebPageProxy&, WebURLSchemeTask&) = 0;
    virtual void platformStopTask(WebPageProxy&, WebURLSchemeTask&) = 0;
    virtual void platformTaskCompleted(WebURLSchemeTask&) = 0;

    void removeTaskFromPageMap(WebPageProxyIdentifier, uint64_t taskID);
    WebProcessProxy* processForTaskIdentifier(uint64_t) const;

    uint64_t m_identifier;

    HashMap<uint64_t, Ref<WebURLSchemeTask>> m_tasks;
    HashMap<WebPageProxyIdentifier, HashSet<uint64_t>> m_tasksByPageIdentifier;
    
    SyncLoadCompletionHandler m_syncLoadCompletionHandler;

}; // class WebURLSchemeHandler

} // namespace WebKit
