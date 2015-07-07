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

#ifndef HTMLDocumentParser_h
#define HTMLDocumentParser_h

#include "CachedResourceClient.h"
#include "FragmentScriptingPermission.h"
#include "HTMLInputStream.h"
#include "HTMLParserOptions.h"
#include "HTMLPreloadScanner.h"
#include "HTMLScriptRunnerHost.h"
#include "HTMLSourceTracker.h"
#include "HTMLToken.h"
#include "HTMLTokenizer.h"
#include "ScriptableDocumentParser.h"
#include "SegmentedString.h"
#include "XSSAuditor.h"
#include "XSSAuditorDelegate.h"
#include <wtf/Deque.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/TextPosition.h>

namespace WebCore {

class BackgroundHTMLParser;
class CompactHTMLToken;
class Document;
class DocumentFragment;
class HTMLDocument;
class HTMLParserScheduler;
class HTMLScriptRunner;
class HTMLTreeBuilder;
class HTMLResourcePreloader;
class ScriptController;
class ScriptSourceCode;

class PumpSession;

class HTMLDocumentParser :  public ScriptableDocumentParser, HTMLScriptRunnerHost, CachedResourceClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassRefPtr<HTMLDocumentParser> create(HTMLDocument& document)
    {
        return adoptRef(new HTMLDocumentParser(document));
    }
    virtual ~HTMLDocumentParser();

    // Exposed for HTMLParserScheduler
    void resumeParsingAfterYield();

    static void parseDocumentFragment(const String&, DocumentFragment&, Element* contextElement, ParserContentPolicy = AllowScriptingContent);

    HTMLTokenizer* tokenizer() const { return m_tokenizer.get(); }

    virtual TextPosition textPosition() const override;

    virtual void suspendScheduledTasks() override;
    virtual void resumeScheduledTasks() override;

protected:
    virtual void insert(const SegmentedString&) override;
    virtual void append(PassRefPtr<StringImpl>) override;
    virtual void finish() override;

    explicit HTMLDocumentParser(HTMLDocument&);
    HTMLDocumentParser(DocumentFragment&, Element* contextElement, ParserContentPolicy);

    HTMLTreeBuilder* treeBuilder() const { return m_treeBuilder.get(); }

    void forcePlaintextForTextDocument();

private:
    static PassRefPtr<HTMLDocumentParser> create(DocumentFragment& fragment, Element* contextElement, ParserContentPolicy parserContentPolicy)
    {
        return adoptRef(new HTMLDocumentParser(fragment, contextElement, parserContentPolicy));
    }

    // DocumentParser
    virtual void detach() override;
    virtual bool hasInsertionPoint() override;
    virtual bool processingData() const override;
    virtual void prepareToStopParsing() override;
    virtual void stopParsing() override;
    virtual bool isWaitingForScripts() const override;
    virtual bool isExecutingScript() const override;
    virtual void executeScriptsWaitingForStylesheets() override;

    // HTMLScriptRunnerHost
    virtual void watchForLoad(CachedResource*) override;
    virtual void stopWatchingForLoad(CachedResource*) override;
    virtual HTMLInputStream& inputStream() override { return m_input; }
    virtual bool hasPreloadScanner() const override { return m_preloadScanner.get(); }
    virtual void appendCurrentInputStreamToPreloadScannerAndScan() override;

    // CachedResourceClient
    virtual void notifyFinished(CachedResource*) override;

    Document* contextForParsingSession();

    enum SynchronousMode {
        AllowYield,
        ForceSynchronous,
    };
    bool canTakeNextToken(SynchronousMode, PumpSession&);
    void pumpTokenizer(SynchronousMode);
    void pumpTokenizerIfPossible(SynchronousMode);
    void constructTreeFromHTMLToken(HTMLToken&);

    void runScriptsForPausedTreeBuilder();
    void resumeParsingAfterScriptExecution();

    void attemptToEnd();
    void endIfDelayed();
    void attemptToRunDeferredScriptsAndEnd();
    void end();

    bool isParsingFragment() const;
    bool isScheduledForResume() const;
    bool inPumpSession() const { return m_pumpSessionNestingLevel > 0; }
    bool shouldDelayEnd() const { return inPumpSession() || isWaitingForScripts() || isScheduledForResume() || isExecutingScript(); }

    HTMLToken& token() { return *m_token.get(); }

    HTMLParserOptions m_options;
    HTMLInputStream m_input;

    std::unique_ptr<HTMLToken> m_token;
    std::unique_ptr<HTMLTokenizer> m_tokenizer;
    std::unique_ptr<HTMLScriptRunner> m_scriptRunner;
    std::unique_ptr<HTMLTreeBuilder> m_treeBuilder;
    std::unique_ptr<HTMLPreloadScanner> m_preloadScanner;
    std::unique_ptr<HTMLPreloadScanner> m_insertionPreloadScanner;
    std::unique_ptr<HTMLParserScheduler> m_parserScheduler;
    HTMLSourceTracker m_sourceTracker;
    TextPosition m_textPosition;
    XSSAuditor m_xssAuditor;
    XSSAuditorDelegate m_xssAuditorDelegate;

    std::unique_ptr<HTMLResourcePreloader> m_preloader;

    bool m_endWasDelayed;
    bool m_haveBackgroundParser;
    unsigned m_pumpSessionNestingLevel;
};

}

#endif
