/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 */

#ifndef DedicatedWorker_h
#define DedicatedWorker_h

#if ENABLE(WORKERS)

#include "ActiveDOMObject.h"
#include "CachedResourceClient.h"
#include "CachedResourceHandle.h"
#include "EventListener.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

    class CachedResource;
    class CachedScript;
    class Document;
    class ScriptExecutionContext;
    class MessagePort;
    class String;

    typedef int ExceptionCode;

    class DedicatedWorker : public RefCounted<DedicatedWorker>, public ActiveDOMObject, private CachedResourceClient {
    public:
        static PassRefPtr<DedicatedWorker> create(const String& url, Document* document, ExceptionCode& ec) { return adoptRef(new DedicatedWorker(url, document, ec)); }
        ~DedicatedWorker();

        Document* document() const;

        PassRefPtr<MessagePort> startConversation(ScriptExecutionContext*, const String& message);
        void close();
        void postMessage(const String& message, MessagePort* port = 0);

        void setOnMessageListener(PassRefPtr<EventListener> eventListener) { m_onMessageListener = eventListener; }
        EventListener* onMessageListener() const { return m_onMessageListener.get(); }

        void setOnCloseListener(PassRefPtr<EventListener> eventListener) { m_onCloseListener = eventListener; }
        EventListener* onCloseListener() const { return m_onCloseListener.get(); }

        void setOnErrorListener(PassRefPtr<EventListener> eventListener) { m_onErrorListener = eventListener; }
        EventListener* onErrorListener() const { return m_onErrorListener.get(); }

    private:
        friend class WorkerThreadScriptLoadTimer;

        DedicatedWorker(const String&, Document*, ExceptionCode&);

        void startLoad();
        virtual void notifyFinished(CachedResource*);

        void dispatchErrorEvent();

        KURL m_scriptURL;
        CachedResourceHandle<CachedScript> m_cachedScript;

        RefPtr<EventListener> m_onMessageListener;
        RefPtr<EventListener> m_onCloseListener;
        RefPtr<EventListener> m_onErrorListener;
    };

} // namespace WebCore

#endif // ENABLE(WORKERS)

#endif // DedicatedWorker_h
