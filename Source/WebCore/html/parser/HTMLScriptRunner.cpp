/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 * Copyright (C) 2010-2017 Apple Inc. All Rights Reserved.
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

#include "Element.h"
#include "Event.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameDestructionObserverInlines.h"
#include "HTMLInputStream.h"
#include "HTMLNames.h"
#include "HTMLScriptRunnerHost.h"
#include "IgnoreDestructiveWriteCountIncrementer.h"
#include "InlineClassicScript.h"
#include "MutationObserver.h"
#include "NestingLevelIncrementer.h"
#include "ScriptElement.h"
#include "ScriptSourceCode.h"

namespace WebCore {

using namespace HTMLNames;

HTMLScriptRunner::HTMLScriptRunner(Document& document, HTMLScriptRunnerHost& host)
    : m_document(document)
    , m_host(host)
    , m_scriptNestingLevel(0)
    , m_hasScriptsWaitingForStylesheets(false)
{
}

HTMLScriptRunner::~HTMLScriptRunner()
{
    // FIXME: Should we be passed a "done loading/parsing" callback sooner than destruction?
    if (m_parserBlockingScript) {
        if (m_parserBlockingScript->watchingForLoad())
            stopWatchingForLoad(*m_parserBlockingScript);
    }

    while (!m_scriptsToExecuteAfterParsing.isEmpty()) {
        auto pendingScript = m_scriptsToExecuteAfterParsing.takeFirst();
        if (pendingScript->watchingForLoad())
            stopWatchingForLoad(pendingScript);
    }
}

void HTMLScriptRunner::detach()
{
    m_document = nullptr;
}

static URL documentURLForScriptExecution(Document* document)
{
    if (!document || !document->frame())
        return URL();

    // Use the URL of the currently active document for this frame.
    return document->frame()->document()->url();
}

inline Ref<Event> createScriptLoadEvent()
{
    return Event::create(eventNames().loadEvent, Event::CanBubble::No, Event::IsCancelable::No);
}

bool HTMLScriptRunner::isPendingScriptReady(const PendingScript& script)
{
    if (!m_document)
        return false;
    m_hasScriptsWaitingForStylesheets = !m_document->haveStylesheetsLoaded();
    if (m_hasScriptsWaitingForStylesheets)
        return false;
    if (script.needsLoading() && !script.isLoaded())
        return false;
    return true;
}

void HTMLScriptRunner::executePendingScriptAndDispatchEvent(PendingScript& pendingScript)
{
    // Stop watching loads before executeScript to prevent recursion if the script reloads itself.
    if (pendingScript.watchingForLoad())
        stopWatchingForLoad(pendingScript);

    if (!isExecutingScript() && m_document)
        m_document->eventLoop().performMicrotaskCheckpoint();

    {
        NestingLevelIncrementer nestingLevelIncrementer(m_scriptNestingLevel);
        pendingScript.element().executePendingScript(pendingScript);
    }
    ASSERT(!isExecutingScript());
}

void HTMLScriptRunner::watchForLoad(PendingScript& pendingScript)
{
    ASSERT(!pendingScript.watchingForLoad());
    m_host.watchForLoad(pendingScript);
}

void HTMLScriptRunner::stopWatchingForLoad(PendingScript& pendingScript)
{
    ASSERT(pendingScript.watchingForLoad());
    m_host.stopWatchingForLoad(pendingScript);
}

// This function should match 10.2.5.11 "An end tag whose tag name is 'script'"
// Script handling lives outside the tree builder to keep the each class simple.
void HTMLScriptRunner::execute(Ref<ScriptElement>&& element, const TextPosition& scriptStartPosition)
{
    // FIXME: If scripting is disabled, always just return.

    bool hadPreloadScanner = m_host.hasPreloadScanner();

    // Try to execute the script given to us.
    runScript(element.get(), scriptStartPosition);

    if (hasParserBlockingScript()) {
        if (isExecutingScript())
            return; // Unwind to the outermost HTMLScriptRunner::execute before continuing parsing.
        // If preload scanner got created, it is missing the source after the current insertion point. Append it and scan.
        if (!hadPreloadScanner && m_host.hasPreloadScanner())
            m_host.appendCurrentInputStreamToPreloadScannerAndScan();
        executeParsingBlockingScripts();
    }
}

bool HTMLScriptRunner::hasParserBlockingScript() const
{
    return !!m_parserBlockingScript;
}

void HTMLScriptRunner::executeParsingBlockingScripts()
{
    while (hasParserBlockingScript() && isPendingScriptReady(*m_parserBlockingScript)) {
        ASSERT(m_document);
        ASSERT(!isExecutingScript());
        ASSERT(m_document->haveStylesheetsLoaded());
        InsertionPointRecord insertionPointRecord(m_host.inputStream());
        executePendingScriptAndDispatchEvent(m_parserBlockingScript.releaseNonNull().get());
    }
}

void HTMLScriptRunner::executeScriptsWaitingForLoad(PendingScript& pendingScript)
{
    ASSERT(!isExecutingScript());
    ASSERT(hasParserBlockingScript());
    ASSERT_UNUSED(pendingScript, m_parserBlockingScript.get() == &pendingScript);
    ASSERT(m_parserBlockingScript->isLoaded());
    executeParsingBlockingScripts();
}

void HTMLScriptRunner::executeScriptsWaitingForStylesheets()
{
    ASSERT(m_document);
    // Callers should check hasScriptsWaitingForStylesheets() before calling
    // to prevent parser or script re-entry during </style> parsing.
    ASSERT(hasScriptsWaitingForStylesheets());
    ASSERT(!isExecutingScript());
    ASSERT(m_document->haveStylesheetsLoaded());
    executeParsingBlockingScripts();
}

bool HTMLScriptRunner::executeScriptsWaitingForParsing()
{
    while (!m_scriptsToExecuteAfterParsing.isEmpty()) {
        ASSERT(!isExecutingScript());
        ASSERT(!hasParserBlockingScript());
        ASSERT(m_scriptsToExecuteAfterParsing.first()->needsLoading());
        if (!m_scriptsToExecuteAfterParsing.first()->isLoaded()) {
            watchForLoad(m_scriptsToExecuteAfterParsing.first());
            return false;
        }
        executePendingScriptAndDispatchEvent(m_scriptsToExecuteAfterParsing.takeFirst().get());
        // FIXME: What is this m_document check for?
        if (!m_document)
            return false;
    }
    return true;
}

static Ref<PendingScript> requestPendingScript(ScriptElement& scriptElement)
{
    ASSERT(scriptElement.willBeParserExecuted());
    ASSERT(scriptElement.loadableScript());
    return PendingScript::create(scriptElement, *scriptElement.loadableScript());
}

void HTMLScriptRunner::requestParsingBlockingScript(ScriptElement& scriptElement)
{
    ASSERT(!m_parserBlockingScript);
    m_parserBlockingScript = requestPendingScript(scriptElement);
    ASSERT(m_parserBlockingScript->needsLoading());

    // We only care about a load callback if LoadableScript is not already
    // in the cache. Callers will attempt to run the m_parserBlockingScript
    // if possible before returning control to the parser.
    if (!m_parserBlockingScript->isLoaded())
        watchForLoad(*m_parserBlockingScript);
}

void HTMLScriptRunner::requestDeferredScript(ScriptElement& scriptElement)
{
    auto pendingScript = requestPendingScript(scriptElement);
    ASSERT(pendingScript->needsLoading());
    m_scriptsToExecuteAfterParsing.append(WTFMove(pendingScript));
}

// This method is meant to match the HTML5 definition of "running a script"
// http://www.whatwg.org/specs/web-apps/current-work/multipage/scripting-1.html#running-a-script
void HTMLScriptRunner::runScript(ScriptElement& scriptElement, const TextPosition& scriptStartPosition)
{
    ASSERT(m_document);
    ASSERT(!hasParserBlockingScript());

    // FIXME: This may be too agressive as we always deliver mutations at
    // every script element, even if it's not ready to execute yet. There's
    // unfortunately no obvious way to tell if prepareScript is going to
    // execute the script before calling it.
    if (!isExecutingScript() && m_document)
        m_document->eventLoop().performMicrotaskCheckpoint();

    InsertionPointRecord insertionPointRecord(m_host.inputStream());
    NestingLevelIncrementer nestingLevelIncrementer(m_scriptNestingLevel);

    scriptElement.prepareScript(scriptStartPosition);

    if (!scriptElement.willBeParserExecuted())
        return;

    if (scriptElement.willExecuteWhenDocumentFinishedParsing())
        requestDeferredScript(scriptElement);
    else if (scriptElement.readyToBeParserExecuted()) {
        if (m_scriptNestingLevel == 1)
            m_parserBlockingScript = PendingScript::create(scriptElement, scriptStartPosition);
        else {
            if (scriptElement.scriptType() == ScriptType::Classic)
                scriptElement.executeClassicScript(ScriptSourceCode(scriptElement.element().textContent(), documentURLForScriptExecution(m_document.get()), scriptStartPosition, JSC::SourceProviderSourceType::Program, InlineClassicScript::create(scriptElement)));
            else
                scriptElement.registerImportMap(ScriptSourceCode(scriptElement.element().textContent(), documentURLForScriptExecution(m_document.get()), scriptStartPosition, JSC::SourceProviderSourceType::ImportMap));
        }
    } else
        requestParsingBlockingScript(scriptElement);
}

}
