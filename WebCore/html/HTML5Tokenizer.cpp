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
    ASSERT(!m_endWasDelayed);
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
    ASSERT(!m_parserStopped);
    ASSERT(!m_treeBuilder->isPaused());
    while (!m_parserStopped && m_lexer->nextToken(m_source, m_token)) {
        m_treeBuilder->constructTreeFromToken(m_token);
        m_token.clear();

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
}

void HTML5Tokenizer::write(const SegmentedString& source, bool)
{
    if (m_parserStopped)
        return;

    NestingLevelIncrementer nestingLevelIncrementer(m_writeNestingLevel);

    // HTML5Tokenizer::executeScript is responsible for handling saving m_source before re-entry.
    m_source.append(source);
    pumpLexerIfPossible();
    endIfDelayed();
}

void HTML5Tokenizer::end()
{
    m_source.close();
    pumpLexerIfPossible();
    // Informs the the rest of WebCore that parsing is really finished.
    m_treeBuilder->finished();
}

void HTML5Tokenizer::attemptToEnd()
{
    // finish() indicates we will not receive any more data. If we are waiting on
    // an external script to load, we can't finish parsing quite yet.

    if (inWrite() || isWaitingForScripts()) {
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
    // We can't call m_source.close() yet as we may have a <script> execution
    // pending which will call document.write().  No more data off the network though.
    // end() calls Document::finishedParsing() once we're actually done parsing.
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

void HTML5Tokenizer::executeScript(const ScriptSourceCode& sourceCode)
{
    ASSERT(m_scriptRunner->inScriptExecution());
    if (!m_document->frame())
        return;

    SegmentedString oldInsertionPoint = m_source;
    m_source = SegmentedString();
    m_document->frame()->script()->executeScript(sourceCode);
    // Append oldInsertionPoint onto the new (likely empty) m_source instead of
    // oldInsertionPoint.prepent(m_source) as that would ASSERT if
    // m_source.escaped() (it had characters pushed back onto it).
    // If m_source was closed, then the tokenizer was stopped, and we discard
    // any pending data as though an EOF character was inserted into the stream.
    if (!m_source.isClosed())
        m_source.append(oldInsertionPoint);
}

void HTML5Tokenizer::notifyFinished(CachedResource* cachedResource)
{
    ASSERT(!m_scriptRunner->inScriptExecution());
    ASSERT(m_treeBuilder->isPaused());
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
    bool shouldContinueParsing = m_scriptRunner->executeScriptsWaitingForStylesheets();
    m_treeBuilder->setPaused(!shouldContinueParsing);
    if (shouldContinueParsing)
        resumeParsingAfterScriptExecution();
}

}
