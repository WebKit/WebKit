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

#ifndef SharedWorkerContext_h
#define SharedWorkerContext_h

#if ENABLE(SHARED_WORKERS)

#include "WorkerContext.h"

namespace WebCore {

    class SharedWorkerThread;

    class SharedWorkerContext : public WorkerContext {
    public:
        typedef WorkerContext Base;
        static PassRefPtr<SharedWorkerContext> create(const String& name, const KURL& url, const String& userAgent, SharedWorkerThread* thread)
        {
            return adoptRef(new SharedWorkerContext(name, url, userAgent, thread));
        }
        virtual ~SharedWorkerContext();

        virtual bool isSharedWorkerContext() const { return true; }

        // ScriptExecutionContext
        virtual void addMessage(MessageDestination, MessageSource, MessageType, MessageLevel, const String& message, unsigned lineNumber, const String& sourceURL);

        virtual void forwardException(const String& errorMessage, int lineNumber, const String& sourceURL);

        // EventTarget
        virtual SharedWorkerContext* toSharedWorkerContext() { return this; }

        // Setters/Getters for attributes in SharedWorkerContext.idl
        void setOnconnect(PassRefPtr<EventListener> eventListener) { m_onconnectListener = eventListener; }
        EventListener* onconnect() const { return m_onconnectListener.get(); }
        String name() const { return m_name; }

        void dispatchConnect(PassRefPtr<MessagePort>);

        SharedWorkerThread* thread();
    private:
        SharedWorkerContext(const String& name, const KURL&, const String&, SharedWorkerThread*);
        RefPtr<EventListener> m_onconnectListener;
        String m_name;
    };

} // namespace WebCore

#endif // ENABLE(SHARED_WORKERS)

#endif // SharedWorkerContext_h
