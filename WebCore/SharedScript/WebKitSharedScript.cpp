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

#include "WebKitSharedScript.h"

#include "Event.h"
#include "EventException.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "SharedScriptContext.h"
#include "WebKitSharedScriptRepository.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

WebKitSharedScript::WebKitSharedScript(const String& url, const String& name, ScriptExecutionContext* context, ExceptionCode& ec)
    : ActiveDOMObject(context, this)
{
    if (url.isEmpty()) {
        ec = SYNTAX_ERR;
        return;
    }

    // FIXME: This should use the dynamic global scope (bug #27887).
    KURL scriptURL = context->completeURL(url);
    if (!scriptURL.isValid()) {
        ec = SYNTAX_ERR;
        return;
    }

    if (!context->securityOrigin()->canAccess(SecurityOrigin::create(scriptURL).get())) {
        ec = SECURITY_ERR;
        return;
    }

    WebKitSharedScriptRepository::connect(this, scriptURL, name, ec);
}

WebKitSharedScript::~WebKitSharedScript()
{
}

void WebKitSharedScript::setContext(PassRefPtr<SharedScriptContext> context)
{ 
    m_innerContext = context;
}

class LoadEventTask : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<LoadEventTask> create(PassRefPtr<WebKitSharedScript> sharedScript, PassRefPtr<SharedScriptContext> innerContext)
    {
        return new LoadEventTask(sharedScript, innerContext);
    }

    virtual void performTask(ScriptExecutionContext* context)
    {
        ASSERT_UNUSED(context, context->isDocument());
        m_sharedScript->setContext(m_innerContext);
        m_sharedScript->dispatchEvent(Event::create(eventNames().loadEvent, false, false));

        m_sharedScript->unsetPendingActivity(m_sharedScript.get());
    }

private:
    LoadEventTask(PassRefPtr<WebKitSharedScript> sharedScript, PassRefPtr<SharedScriptContext> innerContext)
        : m_sharedScript(sharedScript)
        , m_innerContext(innerContext)
    {
        // Keep event listeners alive.
        m_sharedScript->setPendingActivity(m_sharedScript.get());
    }

    RefPtr<WebKitSharedScript> m_sharedScript;
    RefPtr<SharedScriptContext> m_innerContext;
};

void WebKitSharedScript::scheduleLoadEvent(PassRefPtr<SharedScriptContext> innerContext)
{
    ASSERT(!m_innerContext);
    scriptExecutionContext()->postTask(LoadEventTask::create(this, innerContext));
}

} // namespace WebCore

#endif // ENABLE(SHARED_SCRIPT)

