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
#include "HTML5ScriptRunner.h"

#include "Attribute.h"
#include "CachedScript.h"
#include "DocLoader.h"
#include "Element.h"
#include "Event.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "NotImplemented.h"
#include "ScriptElement.h"
#include "ScriptSourceCode.h"

namespace WebCore {

using namespace HTMLNames;

HTML5ScriptRunner::HTML5ScriptRunner(Document* document, CachedResourceClient* loadNotifier)
    : m_document(document)
    , m_loadNotifier(loadNotifier)
    , m_scriptNestingLevel(0)
{
}

HTML5ScriptRunner::~HTML5ScriptRunner()
{
}

Frame* HTML5ScriptRunner::frame() const
{
    return m_document->frame();
}

inline PassRefPtr<Event> createScriptLoadEvent()
{
    return Event::create(eventNames().loadEvent, false, false);
}

inline PassRefPtr<Event> createScriptErrorEvent()
{
    return Event::create(eventNames().errorEvent, true, false);
}

ScriptSourceCode HTML5ScriptRunner::sourceFromPendingScript(const PendingScript& script, bool& errorOccurred)
{
    if (script.cachedScript) {
        errorOccurred = script.cachedScript->errorOccurred();
        ASSERT(script.cachedScript->isLoaded());
        return ScriptSourceCode(script.cachedScript.get());
    }
    errorOccurred = false;
    // FIXME: Line numbers are wrong.
    KURL documentURL = frame() ? frame()->document()->url() : KURL();
    return ScriptSourceCode(script.element->textContent(), documentURL);
}

bool HTML5ScriptRunner::isPendingScriptReady(const PendingScript& script)
{
    // FIXME: We can't block for stylesheets yet, because that causes us to re-enter
    // the parser from executeScriptsWaitingForStylesheets when parsing style tags.
    // if (!m_document->haveStylesheetsLoaded())
    //     return false;
    if (script.cachedScript && !script.cachedScript->isLoaded())
        return false;
    return true;
}

void HTML5ScriptRunner::executePendingScript()
{
    ASSERT(!m_scriptNestingLevel);
    // FIXME: We can't block for stylesheets yet, because that causes us to re-enter
    // the parser from executeScriptsWaitingForStylesheets when parsing style tags.
    // ASSERT(m_document->haveStylesheetsLoaded());
    bool errorOccurred = false;
    ASSERT(isPendingScriptReady(m_parsingBlockingScript));
    ScriptSourceCode sourceCode = sourceFromPendingScript(m_parsingBlockingScript, errorOccurred);

    RefPtr<Element> scriptElement = m_parsingBlockingScript.element.release();
    CachedScript* cachedScript = m_parsingBlockingScript.cachedScript.get();
    m_parsingBlockingScript.cachedScript = 0;

    // removeClient before execution to prevent recursion if the script reloads itself.
    if (cachedScript)
        cachedScript->removeClient(m_loadNotifier);

    m_scriptNestingLevel++;
    if (errorOccurred)
        scriptElement->dispatchEvent(createScriptErrorEvent());
    else {
        if (toScriptElement(scriptElement.get())->shouldExecuteAsJavaScript())
            frame()->script()->executeScript(sourceCode);
        scriptElement->dispatchEvent(createScriptLoadEvent());
    }
    m_scriptNestingLevel--;
    ASSERT(!m_scriptNestingLevel);
}

// This function should match 10.2.5.11 "An end tag whose tag name is 'script'"
// Script handling lives outside the tree builder to keep the each class simple.
bool HTML5ScriptRunner::execute(PassRefPtr<Element> scriptElement)
{
    ASSERT(scriptElement);
    // FIXME: If scripting is disabled, always just return true;

    // Try to execute the script given to us.
    runScript(scriptElement.get());
    if (m_scriptNestingLevel)
        return false; // Don't continue parsing.
    if (!executeParsingBlockingScripts())
        return false;

    notImplemented(); // Restore insertion point?
    // FIXME: Handle re-entrant scripts and m_pendingParsingBlockinScript.
    return true;
}

bool HTML5ScriptRunner::executeParsingBlockingScripts()
{
    while (m_parsingBlockingScript.element) {
        // We only really need to check once.
        if (!isPendingScriptReady(m_parsingBlockingScript))
            return false;
        executePendingScript();
    }
    return true;
}

bool HTML5ScriptRunner::executeScriptsWaitingForLoad(CachedResource*)
{
    ASSERT(m_parsingBlockingScript.element);
    ASSERT(m_parsingBlockingScript.cachedScript->isLoaded());
    return executeParsingBlockingScripts();
}

void HTML5ScriptRunner::requestScript(Element* script)
{
    ASSERT(!m_parsingBlockingScript.element);
    AtomicString srcValue = script->getAttribute(srcAttr);
    // FIXME: We need to resolve the url relative to the element.
    m_parsingBlockingScript.element = script;
    if (!script->dispatchBeforeLoadEvent(srcValue)) // Part of HTML5?
        return;
    // This should correctly return 0 for empty or invalid srcValues.
    CachedScript* cachedScript = m_document->docLoader()->requestScript(srcValue, toScriptElement(script)->scriptCharset());
    if (!cachedScript) {
        notImplemented(); // Dispatch error event.
        return;
    }
    m_parsingBlockingScript.cachedScript = cachedScript;
    // NOTE: This may cause immediate execution of the script.
    cachedScript->addClient(m_loadNotifier);
}

void HTML5ScriptRunner::runScript(Element* script)
{
    ASSERT(!m_parsingBlockingScript.element);
    m_scriptNestingLevel++;
    // Check script type and language, current code uses ScriptElement::shouldExecuteAsJavaScript(), but that may not be HTML5 compliant.
    notImplemented(); // event for support

    if (script->hasAttribute(srcAttr)) {
        // FIXME: Handle defer and async
        requestScript(script);
    } else if (!m_document->haveStylesheetsLoaded()) {
        m_parsingBlockingScript.element = script;
    } else {
        KURL documentURL = frame() ? frame()->document()->url() : KURL();
        // FIXME: Need a line numbers implemenation.
        ScriptSourceCode sourceCode = ScriptSourceCode(script->textContent(), documentURL, 0);
        frame()->script()->executeScript(sourceCode);
    }
    m_scriptNestingLevel--;
}

}
