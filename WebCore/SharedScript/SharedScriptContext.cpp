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

#include "config.h"

#if ENABLE(SHARED_SCRIPT)

#include "SharedScriptContext.h"
#include "Event.h"
#include "EventException.h"
#include "NotImplemented.h"
#include "ScriptSourceCode.h"
#include "ScriptValue.h"
#include "SecurityOrigin.h"
#include "SharedScriptController.h"
#include "WebKitSharedScriptRepository.h"

namespace WebCore {

SharedScriptContext::SharedScriptContext(const String& name, const KURL& url, PassRefPtr<SecurityOrigin> origin)
    : m_name(name)
    , m_url(url)
    , m_script(new SharedScriptController(this))
    , m_destructionTimer(this, &SharedScriptContext::destructionTimerFired)
    , m_loaded(false)
{
    setSecurityOrigin(origin);
    ref(); // Matching deref is in destructionTimerFired callback.
}

void SharedScriptContext::clearScript()
{
    m_script.clear(); 
}

const KURL& SharedScriptContext::virtualURL() const
{
    return m_url;
}

KURL SharedScriptContext::virtualCompleteURL(const String& url) const
{
    // Always return a null URL when passed a null string.
    // FIXME: Should we change the KURL constructor to have this behavior?
    if (url.isNull())
        return KURL();
    // Always use UTF-8 in Workers.
    return KURL(m_url, url);
}

void SharedScriptContext::reportException(const String& errorMessage, int lineNumber, const String& sourceURL)
{
    bool errorHandled = false;
    if (onerror())
        errorHandled = onerror()->reportError(this, errorMessage, sourceURL, lineNumber);
}

void SharedScriptContext::addMessage(MessageDestination destination, MessageSource source, MessageType type, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceURL)
{
    // FIXME: figure out console/inspector story for SharedScript. Maybe similar to SharedWorkers.
    UNUSED_PARAM(destination);
    UNUSED_PARAM(source);
    UNUSED_PARAM(type);
    UNUSED_PARAM(level);
    UNUSED_PARAM(message);
    UNUSED_PARAM(lineNumber);
    UNUSED_PARAM(sourceURL);
    notImplemented();
}

void SharedScriptContext::resourceRetrievedByXMLHttpRequest(unsigned long, const ScriptString&)
{
    // FIXME: figure out console/inspector story for SharedScript. Maybe similar to SharedWorkers.
    notImplemented();
}

void SharedScriptContext::scriptImported(unsigned long, const String&)
{
    // FIXME: figure out console/inspector story for SharedScript. Maybe similar to SharedWorkers.
    notImplemented();
}

bool SharedScriptContext::matches(const String& name, const SecurityOrigin& origin, const KURL& urlToMatch) const
{
    // If the origins don't match, or the names don't match, then this is not the context we are looking for.
    if (!origin.equal(securityOrigin()))
        return false;

    // If the names are both empty, compares the URLs instead.
    if (name.isEmpty() && m_name.isEmpty())
        return urlToMatch == m_url;

    return name == m_name;
}

void SharedScriptContext::addToDocumentsList(Document* document)
{
    m_documentList.add(document);

    if (m_destructionTimer.isActive())
        m_destructionTimer.stop();
}

void SharedScriptContext::destructionTimerFired(Timer<SharedScriptContext>*)
{
    if (!m_documentList.size()) {
        WebKitSharedScriptRepository::removeSharedScriptContext(this);
        stopActiveDOMObjects();
        clearScript();
        deref();
    }
}

void SharedScriptContext::removeFromDocumentList(Document* document)
{
    HashSet<Document*>::iterator it = m_documentList.find(document);
    if (it == m_documentList.end())
        return;

    m_documentList.remove(it);

    // The use of a timer makes destruction of the context happen later to
    // avoid deallocating it right now while it is notified.
    // The context can gain a new owner by the time the timer is fired.
    if (!m_documentList.size())
        m_destructionTimer.startOneShot(0);
}

void SharedScriptContext::load(const String& userAgent, const String& initialScript)
{
    m_userAgent = userAgent;
    script()->evaluate(ScriptSourceCode(initialScript, m_url));
    m_loaded = true;
}

void SharedScriptContext::postTask(PassOwnPtr<Task> task)
{
    // FIXME: Need to implement ScriptExecutionContext::postTaskToMainThread to share between Document and SharedScriptContext.
    UNUSED_PARAM(task);
    notImplemented();
}

EventTargetData* SharedScriptContext::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* SharedScriptContext::ensureEventTargetData()
{
    return &m_eventTargetData;
}

ScriptExecutionContext* SharedScriptContext::scriptExecutionContext() const
{
    return const_cast<SharedScriptContext*>(this);
}

} // namespace WebCore

#endif // ENABLE(SHARED_SCRIPT)

