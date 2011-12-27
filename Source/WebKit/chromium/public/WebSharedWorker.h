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

#ifndef WebSharedWorker_h
#define WebSharedWorker_h

#include "platform/WebCommon.h"

namespace WebCore {
class ScriptExecutionContext;
}

namespace WebKit {

class WebString;
class WebMessagePortChannel;
class WebSharedWorkerClient;
class WebURL;

// This is the interface to a SharedWorker thread.
// Since SharedWorkers communicate entirely through MessagePorts this interface only contains APIs for starting up a SharedWorker.
class WebSharedWorker {
public:
    // Invoked from the worker thread to instantiate a WebSharedWorker that interacts with the WebKit worker components.
    WEBKIT_EXPORT static WebSharedWorker* create(WebSharedWorkerClient*);

    virtual ~WebSharedWorker() {};

    // Returns false if the thread hasn't been started yet (script loading has not taken place).
    // FIXME(atwilson): Remove this when we move the initial script loading into the worker process.
    virtual bool isStarted() = 0;

    virtual void startWorkerContext(const WebURL& scriptURL,
                                    const WebString& name,
                                    const WebString& userAgent,
                                    const WebString& sourceCode,
                                    long long scriptResourceAppCacheID) = 0;

    class ConnectListener {
    public:
        // Invoked once the connect event has been sent so the caller can free this object.
        virtual void connected() = 0;
    };

    // Sends a connect event to the SharedWorker context. The listener is invoked when this async operation completes.
    virtual void connect(WebMessagePortChannel*, ConnectListener*) = 0;

    // Invoked to shutdown the worker when there are no more associated documents.
    virtual void terminateWorkerContext() = 0;

    // Notification when the WebCommonWorkerClient is destroyed.
    virtual void clientDestroyed() = 0;

    virtual void pauseWorkerContextOnStart() { }
    virtual void resumeWorkerContext() { }
    virtual void attachDevTools() { }
    virtual void reattachDevTools(const WebString& savedState) { }
    virtual void detachDevTools() { }
    virtual void dispatchDevToolsMessage(const WebString&) { }
};

} // namespace WebKit

#endif
