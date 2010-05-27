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
#include "HTML5Lexer.h"
#include "HTML5ScriptRunner.h"
#include "HTML5Token.h"
#include "HTML5TreeBuilder.h"
#include "HTMLDocument.h"
#include "Node.h"
#include "NotImplemented.h"

namespace WebCore {

HTML5Tokenizer::HTML5Tokenizer(HTMLDocument* document, bool reportErrors)
    : Tokenizer()
    , m_lexer(new HTML5Lexer)
    , m_scriptRunner(new HTML5ScriptRunner(document, this))
    , m_treeBuilder(new HTML5TreeBuilder(m_lexer.get(), document, reportErrors))
{
    begin();
}

HTML5Tokenizer::~HTML5Tokenizer()
{
}

void HTML5Tokenizer::begin()
{
}

void HTML5Tokenizer::pumpLexer()
{
    ASSERT(!m_treeBuilder->isPaused());
    while (m_lexer->nextToken(m_source, m_token)) {
        m_treeBuilder->constructTreeFromToken(m_token);
        m_token.clear();

        if (!m_treeBuilder->isPaused())
            continue;

        // The parser will pause itself when waiting on a script to load or run.
        // ScriptRunner executes scripts at the right times and handles reentrancy.
        bool shouldContinueParsing = m_scriptRunner->execute(m_treeBuilder->takeScriptToProcess());
        if (!shouldContinueParsing) {
            // ASSERT(m_source.isEmpty() || m_treeBuilder->isPaused());
            // FIXME: the script runner should either make this call or return a special
            // value to indicate we should pause.
            m_treeBuilder->setPaused(true);
            return;
        }
    }
}

void HTML5Tokenizer::write(const SegmentedString& source, bool)
{
    // FIXME: This does not yet correctly handle reentrant writes.
    m_source.append(source);
    if (!m_treeBuilder->isPaused())
        pumpLexer();
}

void HTML5Tokenizer::end()
{
    if (!m_treeBuilder->isPaused())
        pumpLexer();
    m_treeBuilder->finished();
}

void HTML5Tokenizer::finish()
{
    // finish() indicates we will not receive any more data. If we are waiting on
    // an external script to load, we can't finish parsing quite yet.
    m_source.close();
    if (!m_treeBuilder->isPaused())
        end();
}

bool HTML5Tokenizer::isWaitingForScripts() const
{
    return m_treeBuilder->isPaused();
}

void HTML5Tokenizer::resumeParsingAfterScriptExecution()
{
    ASSERT(!m_treeBuilder->isPaused());
    // FIXME: This is the wrong write in the case of document.write re-entry.
    pumpLexer();
    if (m_source.isEmpty() && m_source.isClosed())
        end(); // The document already finished parsing we were just waiting on scripts when finished() was called.
}

void HTML5Tokenizer::notifyFinished(CachedResource* cachedResource)
{
    bool shouldContinueParsing = m_scriptRunner->executeScriptsWaitingForLoad(cachedResource);
    if (shouldContinueParsing) {
        m_treeBuilder->setPaused(false);
        resumeParsingAfterScriptExecution();
    }
}

void HTML5Tokenizer::executeScriptsWaitingForStylesheets()
{
    // FIXME: We can't block for stylesheets yet, because that causes us to re-enter
    // the parser from executeScriptsWaitingForStylesheets when parsing style tags.
}

}
