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

#ifndef SharedScriptContext_h
#define SharedScriptContext_h

#if ENABLE(SHARED_SCRIPT)

#include "EventTarget.h"
#include "SecurityOrigin.h"
#include "SharedScriptController.h"
#include "Timer.h"
#include "WorkerContext.h"
#include <wtf/HashSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Document;
class KURL;
class String;

class SharedScriptContext : public RefCounted<SharedScriptContext>, public ScriptExecutionContext, public EventTarget {
public:
    static PassRefPtr<SharedScriptContext> create(const String& name, const KURL& url, PassRefPtr<SecurityOrigin> origin)
    {
        return adoptRef(new SharedScriptContext(name, url, origin));
    }

    // ScriptExecutionContext
    virtual bool isSharedScriptContext() const { return true; }
    virtual String userAgent(const KURL&) const { return m_userAgent; }

    virtual void reportException(const String& errorMessage, int lineNumber, const String& sourceURL);
    virtual void addMessage(MessageDestination, MessageSource, MessageType, MessageLevel, const String& message, unsigned lineNumber, const String& sourceURL);
    virtual void resourceRetrievedByXMLHttpRequest(unsigned long identifier, const ScriptString& sourceString);
    virtual void scriptImported(unsigned long, const String&);
    virtual void postTask(PassOwnPtr<Task>); // Executes the task on context's thread asynchronously.

    // JS wrapper and EventTarget support.
    virtual ScriptExecutionContext* scriptExecutionContext() const;
    virtual SharedScriptContext* toSharedScriptContext() { return this; }
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);

    // SharedScriptGlobalScope
    SharedScriptContext* self() { return this; }

    // Lifetime - keep the list of documents that are connected, deref when the last disconnects.
    void addToDocumentsList(Document*);
    void removeFromDocumentList(Document*);

    SharedScriptController* script() { return m_script.get(); }
    void clearScript();

    bool loaded() { return m_loaded; }
    void load(const String& userAgent, const String& initialScript);

    bool matches(const String& name, const SecurityOrigin&, const KURL&) const;
    String name() const { return m_name; }

    using RefCounted<SharedScriptContext>::ref;
    using RefCounted<SharedScriptContext>::deref;

protected:
    SharedScriptContext(const String& name, const KURL&, PassRefPtr<SecurityOrigin>);

private:
    // EventTarget
    virtual EventTargetData* eventTargetData();
    virtual EventTargetData* ensureEventTargetData();
    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }
    
    // ScriptExecutionContext
    virtual const KURL& virtualURL() const;
    virtual KURL virtualCompleteURL(const String&) const;
    virtual void refScriptExecutionContext() { ref(); }
    virtual void derefScriptExecutionContext() { deref(); }
            
    void destructionTimerFired(Timer<SharedScriptContext>*);
            
    String m_name;
    String m_userAgent;
    KURL m_url;
    OwnPtr<SharedScriptController> m_script;
    HashSet<Document*> m_documentList;
    Timer<SharedScriptContext> m_destructionTimer;
    EventTargetData m_eventTargetData;

    bool m_loaded;
};

} // namespace WebCore

#endif // ENABLE(SHARED_SCRIPT)

#endif // SharedScriptContext_h

