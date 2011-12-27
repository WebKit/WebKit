/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef TestWebWorker_h
#define TestWebWorker_h

#include "WebMessagePortChannel.h"
#include "WebSharedWorkerClient.h"
#include "WebWorker.h"
#include <wtf/RefCounted.h>

namespace WebKit {
class WebNotificationPresenter;
class WebString;
class WebURL;
}

class TestWebWorker : public WebKit::WebWorker,
                      public WebKit::WebSharedWorkerClient,
                      public WTF::RefCounted<TestWebWorker> {
public:
    TestWebWorker()
    {
        // This class expects refcounting semantics like those found in
        // Chromium's base::RefCounted, so it's OK to call ref() directly.
        relaxAdoptionRequirement();
        ref();
        // The initial counter value should be 2. One for a worker object,
        // another for a worker context object. We need to call ref() just once
        // because the default counter value of RefCounted is 1.
    }

    // WebWorker methods:
    virtual void startWorkerContext(const WebKit::WebURL&, const WebKit::WebString&, const WebKit::WebString&) { }
    virtual void terminateWorkerContext() { }
    virtual void postMessageToWorkerContext(const WebKit::WebString&, const WebKit::WebMessagePortChannelArray&) { }
    virtual void workerObjectDestroyed()
    {
        // Releases the reference held for worker object.
        deref();
    }
    virtual void clientDestroyed() { }

    // WebWorkerClient methods:
    virtual void postMessageToWorkerObject(const WebKit::WebString&, const WebKit::WebMessagePortChannelArray&) { }
    virtual void postExceptionToWorkerObject(const WebKit::WebString&, int, const WebKit::WebString&) { }
    virtual void postConsoleMessageToWorkerObject(int, int, int, int, const WebKit::WebString&, int, const WebKit::WebString&) { }
    virtual void confirmMessageFromWorkerObject(bool) { }
    virtual void reportPendingActivity(bool) { }
    virtual void workerContextClosed() { }
    virtual void workerContextDestroyed()
    {
        // Releases the reference held for worker context object.
        deref();
    }
    virtual WebKit::WebWorker* createWorker(WebKit::WebSharedWorkerClient*) { return 0; }
    virtual WebKit::WebNotificationPresenter* notificationPresenter() { return 0; }
    virtual WebKit::WebApplicationCacheHost* createApplicationCacheHost(WebKit::WebApplicationCacheHostClient*) { return 0; }
    virtual bool allowDatabase(WebKit::WebFrame*, const WebKit::WebString&, const WebKit::WebString&, unsigned long) { return true; }

private:
    ~TestWebWorker() { }
    friend class WTF::RefCounted<TestWebWorker>;
};

#endif // TestWebWorker_h
