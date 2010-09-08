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
#include "HTMLScriptRunner.h"

#include "Attribute.h"
#include "CachedScript.h"
#include "CachedResourceLoader.h"
#include "Element.h"
#include "Event.h"
#include "Frame.h"
#include "HTMLScriptRunnerHost.h"
#include "HTMLInputStream.h"
#include "HTMLNames.h"
#include "NotImplemented.h"
#include "ScriptElement.h"
#include "ScriptSourceCode.h"

namespace WebCore {

using namespace HTMLNames;

// FIXME: Factor out to avoid duplication with HTMLDocumentParser.
class NestingLevelIncrementer : public Noncopyable {
public:
    explicit NestingLevelIncrementer(unsigned& nestingLevel)
        : m_nestingLevel(&nestingLevel)
    {
        ++(*m_nestingLevel);
    }

    ~NestingLevelIncrementer()
    {
        --(*m_nestingLevel);
    }

private:
    unsigned* m_nestingLevel;
};

HTMLScriptRunner::HTMLScriptRunner(Document* document, HTMLScriptRunnerHost* host)
    : m_document(document)
    , m_host(host)
    , m_scriptNestingLevel(0)
    , m_hasScriptsWaitingForStylesheets(false)
{
    ASSERT(m_host);
}

HTMLScriptRunner::~HTMLScriptRunner()
{
    // FIXME: Should we be passed a "done loading/parsing" callback sooner than destruction?
    if (m_parsingBlockingScript.cachedScript() && m_parsingBlockingScript.watchingForLoad())
        stopWatchingForLoad(m_parsingBlockingScript);

    while (!m_scriptsToExecuteAfterParsing.isEmpty()) {
        PendingScript pendingScript = m_scriptsToExecuteAfterParsing.takeFirst();
        if (pendingScript.cachedScript() && pendingScript.watchingForLoad())
            stopWatchingForLoad(pendingScript);
    }
}

void HTMLScriptRunner::detach()
{
    m_document = 0;
}

static KURL documentURLForScriptExecution(Document* document)
{
    if (!document || !document->frame())
        return KURL();

    // Use the URL of the currently active document for this frame.
    return document->frame()->document()->url();
}

inline PassRefPtr<Event> createScriptLoadEvent()
{
    return Event::create(eventNames().loadEvent, false, false);
}

inline PassRefPtr<Event> createScriptErrorEvent()
{
    return Event::create(eventNames().errorEvent, true, false);
}

ScriptSourceCode HTMLScriptRunner::sourceFromPendingScript(const PendingScript& script, bool& errorOccurred) const
{
    if (script.cachedScript()) {
        errorOccurred = script.cachedScript()->errorOccurred();
        ASSERT(script.cachedScript()->isLoaded());
        return ScriptSourceCode(script.cachedScript());
    }
    errorOccurred = false;
    return ScriptSourceCode(script.element()->textContent(), documentURLForScriptExecution(m_document), script.startingLineNumber());
}

bool HTMLScriptRunner::isPendingScriptReady(const PendingScript& script)
{
    m_hasScriptsWaitingForStylesheets = !m_document->haveStylesheetsLoaded();
    if (m_hasScriptsWaitingForStylesheets)
        return false;
    if (script.cachedScript() && !script.cachedScript()->isLoaded())
        return false;
    return true;
}

void HTMLScriptRunner::executeParsingBlockingScript()
{
    ASSERT(m_document);
    ASSERT(!m_scriptNestingLevel);
    ASSERT(m_document->haveStylesheetsLoaded());
    ASSERT(isPendingScriptReady(m_parsingBlockingScript));

    InsertionPointRecord insertionPointRecord(m_host->inputStream());
    executePendingScriptAndDispatchEvent(m_parsingBlockingScript);
}

void HTMLScriptRunner::executePendingScriptAndDispatchEvent(PendingScript& pendingScript)
{
    bool errorOccurred = false;
    ScriptSourceCode sourceCode = sourceFromPendingScript(pendingScript, errorOccurred);

    // Stop watching loads before executeScript to prevent recursion if the script reloads itself.
    if (pendingScript.cachedScript() && pendingScript.watchingForLoad())
        stopWatchingForLoad(pendingScript);

    // Clear the pending script before possible rentrancy from executeScript()
    RefPtr<Element> scriptElement = pendingScript.releaseElementAndClear();
    {
        NestingLevelIncrementer nestingLevelIncrementer(m_scriptNestingLevel);
        if (errorOccurred)
            scriptElement->dispatchEvent(createScriptErrorEvent());
        else {
            executeScript(scriptElement.get(), sourceCode);
            scriptElement->dispatchEvent(createScriptLoadEvent());
        }
    }
    ASSERT(!m_scriptNestingLevel);
}

void HTMLScriptRunner::executeScript(Element* element, const ScriptSourceCode& sourceCode) const
{
    ASSERT(m_document);
    ScriptElement* scriptElement = toScriptElement(element);
    ASSERT(scriptElement);
    if (!scriptElement->shouldExecuteAsJavaScript())
        return;
    ASSERT(isExecutingScript());
    if (!m_document->frame())
        return;
    m_document->frame()->script()->executeScript(sourceCode);
}

void HTMLScriptRunner::watchForLoad(PendingScript& pendingScript)
{
    ASSERT(!pendingScript.watchingForLoad());
    m_host->watchForLoad(pendingScript.cachedScript());
    pendingScript.setWatchingForLoad(true);
}

void HTMLScriptRunner::stopWatchingForLoad(PendingScript& pendingScript)
{
    ASSERT(pendingScript.watchingForLoad());
    m_host->stopWatchingForLoad(pendingScript.cachedScript());
    pendingScript.setWatchingForLoad(false);
}

// This function should match 10.2.5.11 "An end tag whose tag name is 'script'"
// Script handling lives outside the tree builder to keep the each class simple.
bool HTMLScriptRunner::execute(PassRefPtr<Element> scriptElement, int startLine)
{
    ASSERT(scriptElement);
    // FIXME: If scripting is disabled, always just return true;

    // Try to execute the script given to us.
    runScript(scriptElement.get(), startLine);

    if (haveParsingBlockingScript()) {
        if (m_scriptNestingLevel)
            return false; // Block the parser.  Unwind to the outermost HTMLScriptRunner::execute before continuing parsing.
        if (!executeParsingBlockingScripts())
            return false; // We still have a parsing blocking script, block the parser.
    }
    return true; // Scripts executed as expected, continue parsing.
}

bool HTMLScriptRunner::haveParsingBlockingScript() const
{
    return !!m_parsingBlockingScript.element();
}

bool HTMLScriptRunner::executeParsingBlockingScripts()
{
    while (haveParsingBlockingScript()) {
        // We only really need to check once.
        if (!isPendingScriptReady(m_parsingBlockingScript))
            return false;
        executeParsingBlockingScript();
    }
    return true;
}

bool HTMLScriptRunner::executeScriptsWaitingForLoad(CachedResource* cachedScript)
{
    ASSERT(!m_scriptNestingLevel);
    ASSERT(haveParsingBlockingScript());
    ASSERT_UNUSED(cachedScript, m_parsingBlockingScript.cachedScript() == cachedScript);
    ASSERT(m_parsingBlockingScript.cachedScript()->isLoaded());
    return executeParsingBlockingScripts();
}

bool HTMLScriptRunner::executeScriptsWaitingForStylesheets()
{
    ASSERT(m_document);
    // Callers should check hasScriptsWaitingForStylesheets() before calling
    // to prevent parser or script re-entry during </style> parsing.
    ASSERT(hasScriptsWaitingForStylesheets());
    ASSERT(!m_scriptNestingLevel);
    ASSERT(m_document->haveStylesheetsLoaded());
    return executeParsingBlockingScripts();
}

bool HTMLScriptRunner::executeScriptsWaitingForParsing()
{
    while (!m_scriptsToExecuteAfterParsing.isEmpty()) {
        ASSERT(!m_scriptNestingLevel);
        ASSERT(!haveParsingBlockingScript());
        ASSERT(m_scriptsToExecuteAfterParsing.first().cachedScript());
        if (!m_scriptsToExecuteAfterParsing.first().cachedScript()->isLoaded()) {
            watchForLoad(m_scriptsToExecuteAfterParsing.first());
            return false;
        }
        PendingScript first = m_scriptsToExecuteAfterParsing.takeFirst();
        executePendingScriptAndDispatchEvent(first);
        if (!m_document)
            return false;
    }
    return true;
}

void HTMLScriptRunner::requestParsingBlockingScript(Element* element)
{
    if (!requestPendingScript(m_parsingBlockingScript, element))
        return;

    ASSERT(m_parsingBlockingScript.cachedScript());

    // We only care about a load callback if cachedScript is not already
    // in the cache.  Callers will attempt to run the m_parsingBlockingScript
    // if possible before returning control to the parser.
    if (!m_parsingBlockingScript.cachedScript()->isLoaded())
        watchForLoad(m_parsingBlockingScript);
}

void HTMLScriptRunner::requestDeferredScript(Element* element)
{
    PendingScript pendingScript;
    if (!requestPendingScript(pendingScript, element))
        return;

    ASSERT(pendingScript.cachedScript());
    m_scriptsToExecuteAfterParsing.append(pendingScript);
}

bool HTMLScriptRunner::requestPendingScript(PendingScript& pendingScript, Element* script) const
{
    ASSERT(!pendingScript.element());
    const AtomicString& srcValue = script->getAttribute(srcAttr);
    // Allow the host to disllow script loads (using the XSSAuditor, etc.)
    if (!m_host->shouldLoadExternalScriptFromSrc(srcValue))
        return false;
    // FIXME: We need to resolve the url relative to the element.
    if (!script->dispatchBeforeLoadEvent(srcValue))
        return false;
    pendingScript.adoptElement(script);
    // This should correctly return 0 for empty or invalid srcValues.
    CachedScript* cachedScript = m_document->cachedResourceLoader()->requestScript(srcValue, toScriptElement(script)->scriptCharset());
    if (!cachedScript) {
        notImplemented(); // Dispatch error event.
        return false;
    }
    pendingScript.setCachedScript(cachedScript);
    return true;
}

// This method is meant to match the HTML5 definition of "running a script"
// http://www.whatwg.org/specs/web-apps/current-work/multipage/scripting-1.html#running-a-script
void HTMLScriptRunner::runScript(Element* script, int startingLineNumber)
{
    ASSERT(m_document);
    ASSERT(!haveParsingBlockingScript());
    {
        InsertionPointRecord insertionPointRecord(m_host->inputStream());
        NestingLevelIncrementer nestingLevelIncrementer(m_scriptNestingLevel);

        // Check script type and language, current code uses ScriptElement::shouldExecuteAsJavaScript(), but that may not be HTML5 compliant.
        notImplemented(); // event for support

        if (script->hasAttribute(srcAttr)) {
            // FIXME: Handle async.
            if (script->hasAttribute(deferAttr))
                requestDeferredScript(script);
            else
                requestParsingBlockingScript(script);
        } else {
            // FIXME: We do not block inline <script> tags on stylesheets to match the
            // old parser for now.  When we do, the ASSERT below should be added.
            // See https://bugs.webkit.org/show_bug.cgi?id=40047
            // ASSERT(document()->haveStylesheetsLoaded());
            ASSERT(isExecutingScript());
            ScriptSourceCode sourceCode(script->textContent(), documentURLForScriptExecution(m_document), startingLineNumber);
            executeScript(script, sourceCode);
        }
    }
}

}
