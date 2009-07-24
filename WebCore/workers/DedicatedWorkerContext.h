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

#ifndef DedicatedWorkerContext_h
#define DedicatedWorkerContext_h

#include "WorkerContext.h"

namespace WebCore {

    class DedicatedWorkerContext : public WorkerContext {
    public:
        static PassRefPtr<DedicatedWorkerContext> create(const KURL& url, const String& userAgent, WorkerThread* thread)
        {
            return adoptRef(new DedicatedWorkerContext(url, userAgent, thread));
        }
        virtual ~DedicatedWorkerContext();

        virtual void reportException(const String& errorMessage, int lineNumber, const String& sourceURL);

        // EventTarget
        virtual DedicatedWorkerContext* toDedicatedWorkerContext() { return this; }
        void postMessage(const String&, ExceptionCode&);
        void postMessage(const String&, MessagePort*, ExceptionCode&);
        void setOnmessage(PassRefPtr<EventListener> eventListener) { m_onmessageListener = eventListener; }
        EventListener* onmessage() const { return m_onmessageListener.get(); }

        void dispatchMessage(const String&, PassRefPtr<MessagePort>);

    private:
        DedicatedWorkerContext(const KURL&, const String&, WorkerThread*);
        RefPtr<EventListener> m_onmessageListener;
    };

} // namespace WebCore

#endif // DedicatedWorkerContext_h
