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

#ifndef WebWorkerBase_h
#define WebWorkerBase_h

#if ENABLE(WORKERS)

#include "ScriptExecutionContext.h"
#include "WebCommonWorkerClient.h"
#include "WorkerLoaderProxy.h"
#include "WorkerObjectProxy.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>


namespace WebKit {
class WebView;

// Base class for WebSharedWorkerImpl, WebWorkerClientImpl and (defunct) WebWorkerImpl
// containing common interface for shared workers and dedicated in-proc workers implementation.
//
// FIXME: Rename this class into WebWorkerBase, merge existing WebWorkerBase and WebSharedWorker.
class WebWorkerBase {
public:
    virtual WebCore::WorkerLoaderProxy* workerLoaderProxy() = 0;
    virtual WebCommonWorkerClient* commonClient() = 0;
    virtual WebView* view() const = 0;

    // Executes the given task on the main thread.
    static void dispatchTaskToMainThread(PassOwnPtr<WebCore::ScriptExecutionContext::Task>);
};

} // namespace WebKit

#endif // ENABLE(WORKERS)

#endif
