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

#ifndef HTML5Tokenizer_h
#define HTML5Tokenizer_h

#include "CachedResourceClient.h"
#include "HTML5ScriptRunnerHost.h"
#include "HTML5Token.h"
#include "SegmentedString.h"
#include "Tokenizer.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class HTML5Lexer;
class HTML5ScriptRunner;
class HTML5TreeBuilder;
class HTMLDocument;
class ScriptSourceCode;

// FIXME: The whole Tokenizer class system should be renamed "Parser"
// or "ParserController" as the job of this class is to drive parsing process
// but it does not itself Tokenize.
class HTML5Tokenizer :  public Tokenizer, HTML5ScriptRunnerHost, CachedResourceClient {
public:
    HTML5Tokenizer(HTMLDocument*, bool reportErrors);
    virtual ~HTML5Tokenizer();

    // Tokenizer
    virtual void begin();
    virtual void write(const SegmentedString&, bool appendData);
    virtual void end();
    virtual void finish();
    virtual int executingScript() const;
    virtual bool isWaitingForScripts() const;
    virtual void executeScriptsWaitingForStylesheets();
    virtual int lineNumber() const;
    virtual int columnNumber() const;

    // HTML5ScriptRunnerHost
    virtual void watchForLoad(CachedResource*);
    virtual void stopWatchingForLoad(CachedResource*);
    virtual void executeScript(const ScriptSourceCode&);

    // CachedResourceClient
    virtual void notifyFinished(CachedResource*);

private:
    void pumpLexer();
    void resumeParsingAfterScriptExecution();

    void attemptToEnd();
    void endIfDelayed();
    bool inWrite() const { return m_writeNestingLevel > 0; }

    SegmentedString m_source;

    // We hold m_token here because it might be partially complete.
    HTML5Token m_token;

    HTMLDocument* m_document;
    OwnPtr<HTML5Lexer> m_lexer;
    OwnPtr<HTML5ScriptRunner> m_scriptRunner;
    OwnPtr<HTML5TreeBuilder> m_treeBuilder;
    bool m_endWasDelayed;
    int m_writeNestingLevel;
};

}

#endif
