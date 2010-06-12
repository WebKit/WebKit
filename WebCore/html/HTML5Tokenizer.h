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
#include "DocumentParser.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class HTMLDocument;
class HTMLParser;
class HTML5Lexer;
class HTML5ScriptRunner;
class HTML5TreeBuilder;
class ScriptController;
class ScriptSourceCode;

class HTML5Tokenizer :  public DocumentParser, HTML5ScriptRunnerHost, CachedResourceClient {
public:
    HTML5Tokenizer(HTMLDocument*, bool reportErrors);
    virtual ~HTML5Tokenizer();

    // DocumentParser
    virtual void begin();
    virtual void write(const SegmentedString&, bool appendData);
    virtual void end();
    virtual void finish();
    virtual int executingScript() const;
    virtual bool isWaitingForScripts() const;
    virtual void executeScriptsWaitingForStylesheets();
    virtual int lineNumber() const;
    virtual int columnNumber() const;
    // FIXME: HTMLFormControlElement accesses the HTMLParser via this method.
    // Remove this when the HTMLParser is no longer used.
    virtual HTMLParser* htmlParser() const;

    // HTML5ScriptRunnerHost
    virtual void watchForLoad(CachedResource*);
    virtual void stopWatchingForLoad(CachedResource*);
    virtual bool shouldLoadExternalScriptFromSrc(const AtomicString&);
    virtual void executeScript(const ScriptSourceCode&);

    // CachedResourceClient
    virtual void notifyFinished(CachedResource*);

private:
    // The InputStream is made up of a sequence of SegmentedStrings:
    //
    // [--current--][--next--][--next--] ... [--next--]
    //            /\                         (also called m_last)
    //            L_ current insertion point
    //
    // The current segmented string is stored in InputStream.  Each of the
    // afterInsertionPoint buffers are stored in InsertionPointRecords on the
    // stack.
    //
    // We remove characters from the "current" string in the InputStream.
    // document.write() will add characters at the current insertion point,
    // which appends them to the "current" string.
    //
    // m_last is a pointer to the last of the afterInsertionPoint strings.
    // The network adds data at the end of the InputStream, which appends
    // them to the "last" string.
    class InputStream {
    public:
        InputStream()
            : m_last(&m_first)
        {
        }

        void appendToEnd(const SegmentedString& string)
        {
            m_last->append(string);
        }

        void insertAtCurrentInsertionPoint(const SegmentedString& string)
        {
            m_first.append(string);
        }

        void close() { m_last->close(); }

        SegmentedString& current() { return m_first; }

        void splitInto(SegmentedString& next)
        {
            next = m_first;
            m_first = SegmentedString();
            if (m_last == &m_first) {
                // We used to only have one SegmentedString in the InputStream
                // but now we have two.  That means m_first is no longer also
                // the m_last string, |next| is now the last one.
                m_last = &next;
            }
        }

        void mergeFrom(SegmentedString& next)
        {
            m_first.append(next);
            if (m_last == &next) {
                // The string |next| used to be the last SegmentedString in
                // the InputStream.  Now that it's been merged into m_first,
                // that makes m_first the last one.
                m_last = &m_first;
            }
            if (next.isClosed()) {
                // We also need to merge the "closed" state from next to
                // m_first.  Arguably, this work could be done in append().
                m_first.close();
            }
        }

    private:
        SegmentedString m_first;
        SegmentedString* m_last;
    };

    class InsertionPointRecord {
    public:
        InsertionPointRecord(InputStream& inputStream)
            : m_inputStream(&inputStream)
        {
            m_inputStream->splitInto(m_next);
        }

        ~InsertionPointRecord()
        {
            m_inputStream->mergeFrom(m_next);
        }

    private:
        InputStream* m_inputStream;
        SegmentedString m_next;
    };

    void willPumpLexer();
    void didPumpLexer();

    void pumpLexer();
    void pumpLexerIfPossible();
    void resumeParsingAfterScriptExecution();

    void attemptToEnd();
    void endIfDelayed();
    bool inWrite() const { return m_writeNestingLevel > 0; }

    ScriptController* script() const;

    InputStream m_input;

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
