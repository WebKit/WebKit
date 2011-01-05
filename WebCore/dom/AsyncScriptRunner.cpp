/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
#include "AsyncScriptRunner.h"

#include "CachedScript.h"
#include "Document.h"
#include "Element.h"
#include "PendingScript.h"
#include "ScriptElement.h"

namespace WebCore {

AsyncScriptRunner::AsyncScriptRunner(Document* document)
    : m_document(document)
    , m_timer(this, &AsyncScriptRunner::timerFired)
{
    ASSERT(document);
}

AsyncScriptRunner::~AsyncScriptRunner()
{
    for (size_t i = 0; i < m_scriptsToExecuteSoon.size(); ++i)
        m_document->decrementLoadEventDelayCount();
}

void AsyncScriptRunner::executeScriptSoon(ScriptElement* scriptElement, CachedResourceHandle<CachedScript> cachedScript)
{
    ASSERT_ARG(scriptElement, scriptElement);

    Element* element = scriptElement->element();
    ASSERT(element);
    ASSERT(element->inDocument());

    m_document->incrementLoadEventDelayCount();
    m_scriptsToExecuteSoon.append(PendingScript(element, cachedScript.get()));
    if (!m_timer.isActive())
        m_timer.startOneShot(0);
}

void AsyncScriptRunner::suspend()
{
    m_timer.stop();
}

void AsyncScriptRunner::resume()
{
    if (hasPendingScripts())
        m_timer.startOneShot(0);
}

void AsyncScriptRunner::timerFired(Timer<AsyncScriptRunner>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_timer);

    RefPtr<Document> protect(m_document);
    
    Vector<PendingScript> scripts;
    scripts.swap(m_scriptsToExecuteSoon);
    size_t size = scripts.size();
    for (size_t i = 0; i < size; ++i) {
        CachedScript* cachedScript = scripts[i].cachedScript();
        RefPtr<Element> element = scripts[i].releaseElementAndClear();
        toScriptElement(element.get())->execute(cachedScript);
        m_document->decrementLoadEventDelayCount();
    }
}

}
