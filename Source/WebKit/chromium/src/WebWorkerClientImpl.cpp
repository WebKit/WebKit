/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebWorkerClientImpl.h"

#if ENABLE(WORKERS)

#include "CrossThreadTask.h"
#include "DedicatedWorkerThread.h"
#include "Document.h"
#include "ErrorEvent.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "GroupSettings.h"
#include "InspectorInstrumentation.h"
#include "MessageEvent.h"
#include "MessagePort.h"
#include "MessagePortChannel.h"
#include "Page.h"
#include "PageGroup.h"
#include "ScriptCallStack.h"
#include "ScriptExecutionContext.h"
#include "Worker.h"
#include "WorkerContext.h"
#include "WorkerScriptController.h"
#include "WorkerMessagingProxy.h"
#include <wtf/Threading.h>

#include "FrameLoaderClientImpl.h"
#include "PlatformMessagePortChannel.h"
#include "WebFileSystemCallbacks.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebMessagePortChannel.h"
#include "WebPermissionClient.h"
#include "WebViewImpl.h"
#include <public/WebString.h>
#include <public/WebURL.h>

using namespace WebCore;

namespace WebKit {

// Chromium-specific decorator of WorkerMessagingProxy.

// static
WorkerContextProxy* WebWorkerClientImpl::createWorkerContextProxy(Worker* worker)
{
    if (worker->scriptExecutionContext()->isDocument()) {
        Document* document = static_cast<Document*>(worker->scriptExecutionContext());
        WebFrameImpl* webFrame = WebFrameImpl::fromFrame(document->frame());
        WebWorkerClientImpl* proxy = new WebWorkerClientImpl(worker, webFrame);
        return proxy;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

void WebWorkerClientImpl::terminateWorkerContext()
{
    m_webFrame = 0;
    WebCore::WorkerMessagingProxy::terminateWorkerContext();
}

WebWorkerBase* WebWorkerClientImpl::toWebWorkerBase()
{
    return this;
}

WebView* WebWorkerClientImpl::view() const
{
    if (askedToTerminate())
        return 0;
    return m_webFrame->view();
}

bool WebWorkerClientImpl::allowDatabase(WebFrame*, const WebString& name, const WebString& displayName, unsigned long estimatedSize)
{
    if (askedToTerminate())
        return false;
    WebKit::WebViewImpl* webView = m_webFrame->viewImpl();
    if (!webView)
        return false;
    return !webView->permissionClient() || webView->permissionClient()->allowDatabase(m_webFrame, name, displayName, estimatedSize);
}

bool WebWorkerClientImpl::allowFileSystem()
{
    if (askedToTerminate())
        return false;
    WebKit::WebViewImpl* webView = m_webFrame->viewImpl();
    if (!webView)
        return false;
    return !webView->permissionClient() || webView->permissionClient()->allowFileSystem(m_webFrame);
}

void WebWorkerClientImpl::openFileSystem(WebFileSystem::Type type, long long size, bool create,
                                         WebFileSystemCallbacks* callbacks)
{
    if (askedToTerminate()) {
        callbacks->didFail(WebFileErrorAbort);
        return;
    }
    m_webFrame->client()->openFileSystem(m_webFrame, type, size, create, callbacks);
}

bool WebWorkerClientImpl::allowIndexedDB(const WebString& name)
{
    if (askedToTerminate())
        return false;
    WebKit::WebViewImpl* webView = m_webFrame->viewImpl();
    if (!webView)
        return false;
    return !webView->permissionClient() || webView->permissionClient()->allowIndexedDB(m_webFrame, name, WebSecurityOrigin());
}

WebWorkerClientImpl::WebWorkerClientImpl(Worker* worker, WebFrameImpl* webFrame)
    : WebCore::WorkerMessagingProxy(worker)
    , m_webFrame(webFrame)
{
}

WebWorkerClientImpl::~WebWorkerClientImpl()
{
}

} // namespace WebKit

#endif
