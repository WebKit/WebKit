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
#include "HTML5Tokenizer.h"

#include "Element.h"
#include "Frame.h"
#include "HTML5Lexer.h"
#include "HTML5ScriptRunner.h"
#include "HTML5TreeBuilder.h"
#include "HTMLDocument.h"
#include "Node.h"
#include "NotImplemented.h"
#include "XSSAuditor.h"

#if ENABLE(INSPECTOR)
#include "InspectorTimelineAgent.h"
#endif

namespace WebCore {

namespace {

class NestingLevelIncrementer : public Noncopyable {
public:
    NestingLevelIncrementer(int& counter)
        : m_counter(&counter)
    {
        ++(*m_counter);
    }
    
    ~NestingLevelIncrementer()
    {
        --(*m_counter);
    }

private:
    int* m_counter;
};

} // namespace

HTML5Tokenizer::HTML5Tokenizer(HTMLDocument* document, bool reportErrors)
    : Tokenizer()
    , m_document(document)
    , m_lexer(new HTML5Lexer)
    , m_scriptRunner(new HTML5ScriptRunner(document, this))
    , m_treeBuilder(new HTML5TreeBuilder(m_lexer.get(), document, reportErrors))
    , m_endWasDelayed(false)
    , m_writeNestingLevel(0)
{
    begin();
}

HTML5Tokenizer::~HTML5Tokenizer()
{
    // FIXME: We'd like to ASSERT that normal operation of this class clears
    // out any delayed actions, but we can't because we're unceremoniously
    // deleted.  If there were a required call to some sort of cancel function,
    // then we could ASSERT some invariants here.
}

void HTML5Tokenizer::begin()
{
    // FIXME: Should we reset the lexer?
}

void HTML5Tokenizer::pumpLexerIfPossible()
{
    if (m_parserStopped || m_treeBuilder->isPaused())
        return;
    pumpLexer();
}

void HTML5Tokenizer::pumpLexer()
{
    // We tell the InspectorTimelineAgent about every pump, even if we
    // end up pumping nothing.  It can filter out empty pumps itself.
    willPumpLexer();

    ASSERT(!m_parserStopped);
    ASSERT(!m_treeBuilder->isPaused());
    while (!m_parserStopped && m_lexer->nextToken(m_input.current(), m_token)) {
        if (ScriptController* scriptController = script())
            scriptController->setEventHandlerLineNumber(lineNumber() + 1);

        m_treeBuilder->constructTreeFromToken(m_token);
        m_token.clear();

        if (ScriptController* scriptController = script())
            scriptController->setEventHandlerLineNumber(0);

        if (!m_treeBuilder->isPaused())
            continue;

        // The parser will pause itself when waiting on a script to load or run.
        // ScriptRunner executes scripts at the right times and handles reentrancy.
        int scriptStartLine = 0;
        RefPtr<Element> scriptElement = m_treeBuilder->takeScriptToProcess(scriptStartLine);
        bool shouldContinueParsing = m_scriptRunner->execute(scriptElement.release(), scriptStartLine);
        m_treeBuilder->setPaused(!shouldContinueParsing);
        if (!shouldContinueParsing)
            return;
    }

    didPumpLexer();
}

void HTML5Tokenizer::willPumpLexer()
{
#if ENABLE(INSPECTOR)
    // FIXME: m_input.current().length() is only accurate if we end up pumping
    // the end up parsing the whole buffer in this pump.  We should pass how
    // much we parsed as part of didWriteHTML instead of willWriteHTML.
    if (InspectorTimelineAgent* timelineAgent = m_document->inspectorTimelineAgent())
        timelineAgent->willWriteHTML(m_input.current().length(), m_lexer->lineNumber());
#endif
}

void HTML5Tokenizer::didPumpLexer()
{
#if ENABLE(INSPECTOR)
    if (InspectorTimelineAgent* timelineAgent = m_document->inspectorTimelineAgent())
        timelineAgent->didWriteHTML(m_lexer->lineNumber());
#endif
}

void HTML5Tokenizer::write(const SegmentedString& source, bool appendData)
{
    if (m_parserStopped)
        return;

    NestingLevelIncrementer nestingLevelIncrementer(m_writeNestingLevel);

    if (appendData) {
        m_input.appendToEnd(source);
        if (m_writeNestingLevel > 1) {
            // We've gotten data off the network in a nested call to write().
            // We don't want to consume any more of the input stream now.  Do
            // not worry.  We'll consume this data in a less-nested write().
            return;
        }
    } else
        m_input.insertAtCurrentInsertionPoint(source);

    pumpLexerIfPossible();
    endIfDelayed();
}

void HTML5Tokenizer::end()
{
    pumpLexerIfPossible();
    // Informs the the rest of WebCore that parsing is really finished.
    m_treeBuilder->finished();
}

void HTML5Tokenizer::attemptToEnd()
{
    // finish() indicates we will not receive any more data. If we are waiting on
    // an external script to load, we can't finish parsing quite yet.

    if (inWrite() || isWaitingForScripts() || executingScript()) {
        m_endWasDelayed = true;
        return;
    }
    end();
}

void HTML5Tokenizer::endIfDelayed()
{
    if (!m_endWasDelayed || isWaitingForScripts() || executingScript())
        return;

    m_endWasDelayed = false;
    end();
}

void HTML5Tokenizer::finish()
{
    // We're not going to get any more data off the network, so we close the
    // input stream to indicate EOF.
    m_input.close();
    attemptToEnd();
}

int HTML5Tokenizer::executingScript() const
{
    return m_scriptRunner->inScriptExecution();
}

int HTML5Tokenizer::lineNumber() const
{
    return m_lexer->lineNumber();
}

int HTML5Tokenizer::columnNumber() const
{
    return m_lexer->columnNumber();
}

HTMLParser* HTML5Tokenizer::htmlParser() const
{
    return m_treeBuilder->htmlParser();
}

bool HTML5Tokenizer::isWaitingForScripts() const
{
    return m_treeBuilder->isPaused();
}

void HTML5Tokenizer::resumeParsingAfterScriptExecution()
{
    ASSERT(!m_scriptRunner->inScriptExecution());
    ASSERT(!m_treeBuilder->isPaused());
    pumpLexerIfPossible();

    // The document already finished parsing we were just waiting on scripts when finished() was called.
    endIfDelayed();
}

void HTML5Tokenizer::watchForLoad(CachedResource* cachedScript)
{
    ASSERT(!cachedScript->isLoaded());
    // addClient would call notifyFinished if the load were complete.
    // Callers do not expect to be re-entered from this call, so they should
    // not an already-loaded CachedResource.
    cachedScript->addClient(this);
}

void HTML5Tokenizer::stopWatchingForLoad(CachedResource* cachedScript)
{
    cachedScript->removeClient(this);
}

bool HTML5Tokenizer::shouldLoadExternalScriptFromSrc(const AtomicString& srcValue)
{
    if (!m_XSSAuditor)
        return true;
    return m_XSSAuditor->canLoadExternalScriptFromSrc(srcValue);
}

void HTML5Tokenizer::executeScript(const ScriptSourceCode& sourceCode)
{
    ASSERT(m_scriptRunner->inScriptExecution());
    if (!m_document->frame())
        return;
    InsertionPointRecord savedInsertionPoint(m_input);
    m_document->frame()->script()->executeScript(sourceCode);
}

void HTML5Tokenizer::notifyFinished(CachedResource* cachedResource)
{
    ASSERT(!m_scriptRunner->inScriptExecution());
    ASSERT(m_treeBuilder->isPaused());
    // Note: We only ever wait on one script at a time, so we always know this
    // is the one we were waiting on and can un-pause the tree builder.
    m_treeBuilder->setPaused(false);
    bool shouldContinueParsing = m_scriptRunner->executeScriptsWaitingForLoad(cachedResource);
    m_treeBuilder->setPaused(!shouldContinueParsing);
    if (shouldContinueParsing)
        resumeParsingAfterScriptExecution();
}

void HTML5Tokenizer::executeScriptsWaitingForStylesheets()
{
    // Ignore calls unless we have a script blocking the parser waiting on a
    // stylesheet load.  Otherwise we are currently parsing and this
    // is a re-entrant call from encountering a </ style> tag.
    if (!m_scriptRunner->hasScriptsWaitingForStylesheets())
        return;
    ASSERT(!m_scriptRunner->inScriptExecution());
    ASSERT(m_treeBuilder->isPaused());
    // Note: We only ever wait on one script at a time, so we always know this
    // is the one we were waiting on and can un-pause the tree builder.
    m_treeBuilder->setPaused(false);
    bool shouldContinueParsing = m_scriptRunner->executeScriptsWaitingForStylesheets();
    m_treeBuilder->setPaused(!shouldContinueParsing);
    if (shouldContinueParsing)
        resumeParsingAfterScriptExecution();
}

ScriptController* HTML5Tokenizer::script() const
{
    return m_document->frame() ? m_document->frame()->script() : 0;
}

}
