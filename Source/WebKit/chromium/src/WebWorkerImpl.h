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

#ifndef WebWorkerImpl_h
#define WebWorkerImpl_h

#include "WebWorker.h"

#if ENABLE(WORKERS)

#include "ScriptExecutionContext.h"

#include "WebWorkerBase.h"

namespace WebKit {

// This class is used by the worker process code to talk to the WebCore::Worker
// implementation.  It can't use it directly since it uses WebKit types, so this
// class converts the data types.  When the WebCore::Worker object wants to call
// WebCore::WorkerObjectProxy, this class will conver to Chrome data types first
// and then call the supplied WebWorkerClient.
class WebWorkerImpl : public WebWorkerBase, public WebWorker {
public:
    explicit WebWorkerImpl(WebWorkerClient* client);

    // WebWorker methods:
    virtual void startWorkerContext(const WebURL&, const WebString&, const WebString&);
    virtual void terminateWorkerContext();
    virtual void postMessageToWorkerContext(const WebString&, const WebMessagePortChannelArray&);
    virtual void workerObjectDestroyed();
    virtual void clientDestroyed();

    // WebWorkerBase methods:
    virtual WebWorkerClient* client() { return m_client; }
    virtual WebCommonWorkerClient* commonClient();

    // NewWebWorkerBase methods:
    virtual NewWebCommonWorkerClient* newCommonClient();

private:
    virtual ~WebWorkerImpl();

    // Tasks that are run on the worker thread.
    static void postMessageToWorkerContextTask(
        WebCore::ScriptExecutionContext* context,
        WebWorkerImpl* thisPtr,
        const WTF::String& message,
        PassOwnPtr<WebCore::MessagePortChannelArray> channels);

    WebWorkerClient* m_client;
};

} // namespace WebKit

#endif // ENABLE(WORKERS)

#endif
