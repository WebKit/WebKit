/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ScriptRunner.h"

#include "Element.h"
#include "PendingScript.h"
#include "ScriptElement.h"

namespace WebCore {

ScriptRunner::ScriptRunner(Document& document)
    : m_document(document)
    , m_timer(*this, &ScriptRunner::timerFired)
{
}

ScriptRunner::~ScriptRunner()
{
    for (auto& pendingScript : m_scriptsToExecuteSoon) {
        UNUSED_PARAM(pendingScript);
        m_document.decrementLoadEventDelayCount();
    }
    for (auto& pendingScript : m_scriptsToExecuteInOrder) {
        if (pendingScript->watchingForLoad())
            pendingScript->clearClient();
        m_document.decrementLoadEventDelayCount();
    }
    for (auto& pendingScript : m_pendingAsyncScripts) {
        if (pendingScript->watchingForLoad())
            pendingScript->clearClient();
        m_document.decrementLoadEventDelayCount();
    }
}

void ScriptRunner::queueScriptForExecution(ScriptElement& scriptElement, LoadableScript& loadableScript, ExecutionType executionType)
{
    ASSERT(scriptElement.element().isConnected());

    m_document.incrementLoadEventDelayCount();

    auto pendingScript = PendingScript::create(scriptElement, loadableScript);
    switch (executionType) {
    case ASYNC_EXECUTION:
        m_pendingAsyncScripts.add(pendingScript.copyRef());
        break;
    case IN_ORDER_EXECUTION:
        m_scriptsToExecuteInOrder.append(pendingScript.copyRef());
        break;
    }
    pendingScript->setClient(*this);
}

void ScriptRunner::suspend()
{
    m_timer.stop();
}

void ScriptRunner::resume()
{
    if (hasPendingScripts() && !m_document.hasActiveParserYieldToken())
        m_timer.startOneShot(0_s);
}

void ScriptRunner::documentFinishedParsing()
{
    if (!m_scriptsToExecuteSoon.isEmpty() && !m_timer.isActive())
        resume();
}

void ScriptRunner::notifyFinished(PendingScript& pendingScript)
{
    if (pendingScript.element().willExecuteInOrder())
        ASSERT(!m_scriptsToExecuteInOrder.isEmpty());
    else {
        ASSERT(m_pendingAsyncScripts.contains(pendingScript));
        m_scriptsToExecuteSoon.append(m_pendingAsyncScripts.take(pendingScript)->ptr());
    }
    pendingScript.clearClient();

    if (!m_document.hasActiveParserYieldToken())
        m_timer.startOneShot(0_s);
}

void ScriptRunner::timerFired()
{
    Ref<Document> protect(m_document);

    Vector<RefPtr<PendingScript>> scripts;

    if (!m_document.shouldDeferAsynchronousScriptsUntilParsingFinishes())
        scripts.swap(m_scriptsToExecuteSoon);

    size_t numInOrderScriptsToExecute = 0;
    for (; numInOrderScriptsToExecute < m_scriptsToExecuteInOrder.size() && m_scriptsToExecuteInOrder[numInOrderScriptsToExecute]->isLoaded(); ++numInOrderScriptsToExecute)
        scripts.append(m_scriptsToExecuteInOrder[numInOrderScriptsToExecute].ptr());
    if (numInOrderScriptsToExecute)
        m_scriptsToExecuteInOrder.remove(0, numInOrderScriptsToExecute);

    for (auto& currentScript : scripts) {
        auto script = WTFMove(currentScript);
        ASSERT(script);
        // Paper over https://bugs.webkit.org/show_bug.cgi?id=144050
        if (!script)
            continue;
        ASSERT(script->needsLoading());
        script->element().executePendingScript(*script);
        m_document.decrementLoadEventDelayCount();
    }
}

}
