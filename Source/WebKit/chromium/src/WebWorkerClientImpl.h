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

#ifndef WebWorkerClientImpl_h
#define WebWorkerClientImpl_h

#if ENABLE(WORKERS)

#include "ScriptExecutionContext.h"
#include "WorkerContextProxy.h"
#include "WorkerLoaderProxy.h"
#include "WorkerMessagingProxy.h"
#include "WorkerObjectProxy.h"

#include "WebWorkerBase.h"
#include <public/WebFileSystem.h>
#include <public/WebFileSystemType.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>


namespace WebKit {
class WebWorker;
class WebFrameImpl;

// This class provides chromium implementation for WorkerContextProxy, WorkerObjectProxy amd WorkerLoaderProxy
// for in-proc dedicated workers. It also acts as a bridge for workers to chromium implementation of file systems,
// databases and other related functionality.
//
// In essence, this class decorates WorkerMessagingProxy.
//
// It is imperative that this class inherit from WorkerMessagingProxy rather than delegate to an instance of
// WorkerMessagingProxy, because that class tracks and reports its activity to outside callers, and manages
// its own lifetime, via calls to workerObjectDestroyed, workerContextDestroyed, workerContextClosed, etc. It
// is basically impossible to correctly manage the lifetime of this class separately from WorkerMessagingProxy.
class WebWorkerClientImpl : public WebCore::WorkerMessagingProxy
                          , public WebWorkerBase
                          , public WebCommonWorkerClient {
public:
    // WebCore::WorkerContextProxy Factory.
    static WebCore::WorkerContextProxy* createWorkerContextProxy(WebCore::Worker*);

    // WebCore::WorkerContextProxy methods:
    // These are called on the thread that created the worker.  In the renderer
    // process, this will be the main WebKit thread.
    virtual void terminateWorkerContext() OVERRIDE;

    // WebCore::WorkerLoaderProxy methods
    virtual WebWorkerBase* toWebWorkerBase() OVERRIDE;

    // WebWorkerBase methods:
    virtual WebCore::WorkerLoaderProxy* workerLoaderProxy() OVERRIDE { return this; }
    virtual WebCommonWorkerClient* commonClient() OVERRIDE { return this; }
    virtual WebView* view() const OVERRIDE;

    // WebCommonWorkerClient methods:
    virtual bool allowDatabase(WebFrame*, const WebString& name, const WebString& displayName, unsigned long estimatedSize) OVERRIDE;
    virtual bool allowFileSystem();
    virtual void openFileSystem(WebFileSystemType, long long size, bool create,
        WebFileSystemCallbacks*) OVERRIDE;
    virtual bool allowIndexedDB(const WebString& name) OVERRIDE;

private:
    WebWorkerClientImpl(WebCore::Worker*, WebFrameImpl*);
    virtual ~WebWorkerClientImpl();

    WebFrameImpl* m_webFrame;
};

} // namespace WebKit;

#endif // ENABLE(WORKERS)

#endif
