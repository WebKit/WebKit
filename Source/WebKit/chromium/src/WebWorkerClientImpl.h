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

#include "WebFileSystem.h"
#include "WebWorkerClient.h"
#include "WorkerContextProxy.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {
class ScriptExecutionContext;
}

namespace WebKit {
class WebWorker;

// The purpose of this class is to provide a WorkerContextProxy
// implementation that we can give to WebKit.  Internally, it converts the
// data types to Chrome compatible ones so that renderer code can use it over
// IPC.
class WebWorkerClientImpl : public WebCore::WorkerContextProxy
                          , public WebWorkerClient {
public:
    WebWorkerClientImpl(WebCore::Worker*);

    // WebCore::WorkerContextProxy Factory.
    static WebCore::WorkerContextProxy* createWorkerContextProxy(WebCore::Worker*);
    void setWebWorker(WebWorker*);

    // WebCore::WorkerContextProxy methods:
    // These are called on the thread that created the worker.  In the renderer
    // process, this will be the main WebKit thread.  In the worker process, this
    // will be the thread of the executing worker (not the main WebKit thread).
    virtual void startWorkerContext(const WebCore::KURL&,
                                    const WTF::String&,
                                    const WTF::String&);
    virtual void terminateWorkerContext();
    virtual void postMessageToWorkerContext(
        PassRefPtr<WebCore::SerializedScriptValue> message,
        PassOwnPtr<WebCore::MessagePortChannelArray> channels);
    virtual bool hasPendingActivity() const;
    virtual void workerObjectDestroyed();

    // WebWorkerClient methods:
    // These are called on the main WebKit thread.
    virtual void postMessageToWorkerObject(const WebString&, const WebMessagePortChannelArray&);
    virtual void postExceptionToWorkerObject(const WebString&, int, const WebString&);

    // FIXME: the below is for compatibility only and should be     
    // removed once Chromium is updated to remove message 
    // destination parameter <http://webkit.org/b/37155>.
    virtual void postConsoleMessageToWorkerObject(int, int, int, int, const WebString&, int, const WebString&);
    virtual void postConsoleMessageToWorkerObject(int, int, int, const WebString&, int, const WebString&);
    virtual void confirmMessageFromWorkerObject(bool);
    virtual void reportPendingActivity(bool);
    virtual void workerContextClosed();
    virtual void workerContextDestroyed();
    virtual WebWorker* createWorker(WebWorkerClient*) { return 0; }
    virtual WebNotificationPresenter* notificationPresenter()
    {
        // FIXME: Notifications not yet supported in workers.
        return 0;
    }
    virtual WebApplicationCacheHost* createApplicationCacheHost(WebApplicationCacheHostClient*) { return 0; }
    virtual bool allowDatabase(WebFrame*, const WebString& name, const WebString& displayName, unsigned long estimatedSize)
    {
        ASSERT_NOT_REACHED();
        return true;
    }
    virtual void openFileSystem(WebFrame*, WebFileSystem::Type, long long size, WebFileSystemCallbacks*)
    {
        ASSERT_NOT_REACHED();
    }

private:
    virtual ~WebWorkerClientImpl();

    // Methods used to support WebWorkerClientImpl being constructed on worker
    // threads.
    // These tasks are dispatched on the WebKit thread.
    static void startWorkerContextTask(WebCore::ScriptExecutionContext* context,
                                       WebWorkerClientImpl* thisPtr,
                                       const WTF::String& scriptURL,
                                       const WTF::String& userAgent,
                                       const WTF::String& sourceCode);
    static void terminateWorkerContextTask(WebCore::ScriptExecutionContext* context,
                                           WebWorkerClientImpl* thisPtr);
    static void postMessageToWorkerContextTask(WebCore::ScriptExecutionContext* context,
                                               WebWorkerClientImpl* thisPtr,
                                               const WTF::String& message,
                                               PassOwnPtr<WebCore::MessagePortChannelArray> channels);
    static void workerObjectDestroyedTask(WebCore::ScriptExecutionContext* context,
                                          WebWorkerClientImpl* thisPtr);

    // These tasks are dispatched on the thread that created the worker (i.e.
    // main WebKit thread in renderer process, and the worker thread in the
    // worker process).
    static void postMessageToWorkerObjectTask(WebCore::ScriptExecutionContext* context,
                                              WebWorkerClientImpl* thisPtr,
                                              const WTF::String& message,
                                              PassOwnPtr<WebCore::MessagePortChannelArray> channels);
    static void postExceptionToWorkerObjectTask(WebCore::ScriptExecutionContext* context,
                                                WebWorkerClientImpl* thisPtr,
                                                const WTF::String& message,
                                                int lineNumber,
                                                const WTF::String& sourceURL);
    static void postConsoleMessageToWorkerObjectTask(WebCore::ScriptExecutionContext* context,
                                                     WebWorkerClientImpl* thisPtr,
                                                     int sourceId,
                                                     int messageType,
                                                     int messageLevel,
                                                     const WTF::String& message,
                                                     int lineNumber,
                                                     const WTF::String& sourceURL);
    static void confirmMessageFromWorkerObjectTask(WebCore::ScriptExecutionContext* context,
                                                   WebWorkerClientImpl* thisPtr);
    static void reportPendingActivityTask(WebCore::ScriptExecutionContext* context,
                                          WebWorkerClientImpl* thisPtr,
                                          bool hasPendingActivity);

    // Guard against context from being destroyed before a worker exits.
    RefPtr<WebCore::ScriptExecutionContext> m_scriptExecutionContext;

    WebCore::Worker* m_worker;
    WebWorker* m_webWorker;
    bool m_askedToTerminate;
    unsigned m_unconfirmedMessageCount;
    bool m_workerContextHadPendingActivity;
    ThreadIdentifier m_workerThreadId;
};

} // namespace WebKit;

#endif // ENABLE(WORKERS)

#endif
