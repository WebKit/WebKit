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
#include "FragmentScriptingPermission.h"
#include "HTML5ScriptRunnerHost.h"
#include "HTML5Token.h"
#include "HTMLInputStream.h"
#include "SegmentedString.h"
#include "DocumentParser.h"
#include "Timer.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class Document;
class DocumentFragment;
class HTMLDocument;
class HTMLParserScheduler;
class HTML5Lexer;
class HTML5ScriptRunner;
class HTML5TreeBuilder;
class HTML5PreloadScanner;
class LegacyHTMLTreeConstructor;
class ScriptController;
class ScriptSourceCode;

class HTML5DocumentParser :  public DocumentParser, HTML5ScriptRunnerHost, CachedResourceClient {
public:
    // FIXME: These constructors should be made private and replaced by create() methods.
    HTML5DocumentParser(HTMLDocument*, bool reportErrors);
    HTML5DocumentParser(DocumentFragment*, FragmentScriptingPermission);
    virtual ~HTML5DocumentParser();

    // DocumentParser
    virtual void begin();
    virtual void write(const SegmentedString&, bool isFromNetwork);
    virtual void end();
    virtual void finish();
    virtual bool finishWasCalled();
    virtual int executingScript() const;
    virtual bool processingData() const;
    virtual void stopParsing();
    virtual bool isWaitingForScripts() const;
    virtual void executeScriptsWaitingForStylesheets();
    virtual int lineNumber() const;
    virtual int columnNumber() const;
    // FIXME: HTMLFormControlElement accesses the LegacyHTMLTreeConstructor via this method.
    // Remove this when the LegacyHTMLTreeConstructor is no longer used.
    virtual LegacyHTMLTreeConstructor* htmlTreeConstructor() const;

    // HTML5ScriptRunnerHost
    virtual void watchForLoad(CachedResource*);
    virtual void stopWatchingForLoad(CachedResource*);
    virtual bool shouldLoadExternalScriptFromSrc(const AtomicString&);
    virtual HTMLInputStream& inputStream() { return m_input; }

    // CachedResourceClient
    virtual void notifyFinished(CachedResource*);

    // Exposed for HTMLParserScheduler
    Document* document() const { return m_document; }
    void resumeParsingAfterYield();

private:
    void willPumpLexer();
    void didPumpLexer();

    enum SynchronousMode {
        AllowYield,
        ForceSynchronous,
    };
    void pumpLexer(SynchronousMode);
    void pumpLexerIfPossible(SynchronousMode);

    bool runScriptsForPausedTreeConstructor();
    void resumeParsingAfterScriptExecution();

    void attemptToEnd();
    void endIfDelayed();

    bool isScheduledForResume() const;
    bool inScriptExecution() const;
    bool inWrite() const { return m_writeNestingLevel > 0; }

    ScriptController* script() const;

    HTMLInputStream m_input;

    // We hold m_token here because it might be partially complete.
    HTML5Token m_token;

    // We must support parsing into a Document* and not just HTMLDocument*
    // to support DocumentFragment (which has a Document*).
    Document* m_document;
    OwnPtr<HTML5Lexer> m_lexer;
    OwnPtr<HTML5ScriptRunner> m_scriptRunner;
    OwnPtr<HTML5TreeBuilder> m_treeConstructor;
    OwnPtr<HTML5PreloadScanner> m_preloadScanner;
    OwnPtr<HTMLParserScheduler> m_parserScheduler;

    bool m_endWasDelayed;
    int m_writeNestingLevel;
};

}

#endif
