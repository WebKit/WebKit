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

#ifndef WebCommonWorkerClient_h
#define WebCommonWorkerClient_h

#include "WebCommon.h"
#include "WebFileSystem.h"

namespace WebKit {

class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
class WebFrame;
class WebNotificationPresenter;
class WebString;
class WebWorker;
class WebWorkerClient;

// Provides an interface back to the in-page script object for a worker.
// This interface contains common APIs used by both shared and dedicated
// workers.
// All functions are expected to be called back on the thread that created
// the Worker object, unless noted.
class WebCommonWorkerClient {
public:
    virtual void postExceptionToWorkerObject(
        const WebString& errorString, int lineNumber,
        const WebString& sourceURL) = 0;

    // FIXME: the below is for compatibility only and should be   
    // removed once Chromium is updated to remove message
    // destination parameter <http://webkit.org/b/37155>.
    virtual void postConsoleMessageToWorkerObject(int, int sourceIdentifier, int messageType, int messageLevel,
                                                  const WebString& message, int lineNumber, const WebString& sourceURL) = 0;

    virtual void postConsoleMessageToWorkerObject(int sourceIdentifier, int messageType, int messageLevel,
                                                  const WebString& message, int lineNumber, const WebString& sourceURL)
    {
        postConsoleMessageToWorkerObject(0, sourceIdentifier, messageType, messageLevel,
                                         message, lineNumber, sourceURL);
    }

    virtual void workerContextClosed() = 0;
    virtual void workerContextDestroyed() = 0;

    // Returns the notification presenter for this worker context.  Pointer
    // is owned by the object implementing WebCommonWorkerClient.
    virtual WebNotificationPresenter* notificationPresenter() = 0;

    // This can be called on any thread to create a nested WebWorker.
    // WebSharedWorkers are not instantiated via this API - instead
    // they are created via the WebSharedWorkerRepository.
    virtual WebWorker* createWorker(WebWorkerClient* client) = 0;

    // Called on the main webkit thread in the worker process during initialization.
    virtual WebApplicationCacheHost* createApplicationCacheHost(WebApplicationCacheHostClient*) = 0;

    // Called on the main webkit thread before opening a web database.
    virtual bool allowDatabase(WebFrame*, const WebString& name, const WebString& displayName, unsigned long estimatedSize) = 0;

    // Called on the main webkit thread before opening a file system.
    virtual bool allowFileSystem()
    {
        return true;
    }

    // Called on the main webkit thread before opening a file system.
    virtual void openFileSystem(WebFileSystem::Type, long long size, bool create, WebFileSystemCallbacks*)
    {
        WEBKIT_ASSERT_NOT_REACHED();
    }

protected:
    ~WebCommonWorkerClient() { }
};

} // namespace WebKit

#endif
