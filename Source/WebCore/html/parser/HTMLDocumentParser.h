/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 * Copyright (C) 2015-2021 Apple Inc. All Rights Reserved.
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

#pragma once

#include "HTMLInputStream.h"
#include "HTMLScriptRunnerHost.h"
#include "HTMLTokenizer.h"
#include "PendingScriptClient.h"
#include "ScriptableDocumentParser.h"

namespace WebCore {

class DocumentFragment;
class Element;
class HTMLDocument;
class HTMLParserScheduler;
class HTMLPreloadScanner;
class HTMLScriptRunner;
class HTMLTreeBuilder;
class HTMLResourcePreloader;
class PumpSession;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(HTMLDocumentParser);
class HTMLDocumentParser : public ScriptableDocumentParser, private HTMLScriptRunnerHost, private PendingScriptClient {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(HTMLDocumentParser);
public:
    static Ref<HTMLDocumentParser> create(HTMLDocument&, OptionSet<ParserContentPolicy> = DefaultParserContentPolicy);
    virtual ~HTMLDocumentParser();

    static void parseDocumentFragment(const String&, DocumentFragment&, Element& contextElement, OptionSet<ParserContentPolicy> = { ParserContentPolicy::AllowScriptingContent, ParserContentPolicy::AllowPluginContent });

    // For HTMLParserScheduler.
    void resumeParsingAfterYield();

    // For HTMLTreeBuilder.
    HTMLTokenizer& tokenizer();
    TextPosition textPosition() const final;

protected:
    explicit HTMLDocumentParser(HTMLDocument&, OptionSet<ParserContentPolicy> = DefaultParserContentPolicy);

    void insert(SegmentedString&&) final;
    void append(RefPtr<StringImpl>&&) override;
    void appendSynchronously(RefPtr<StringImpl>&&) override;
    void finish() override;

    HTMLTreeBuilder& treeBuilder();

private:
    HTMLDocumentParser(DocumentFragment&, Element& contextElement, OptionSet<ParserContentPolicy>);
    static Ref<HTMLDocumentParser> create(DocumentFragment&, Element& contextElement, OptionSet<ParserContentPolicy>);

    // DocumentParser
    void detach() final;
    bool hasInsertionPoint() final;
    bool processingData() const final;
    void prepareToStopParsing() final;
    void stopParsing() final;
    bool isWaitingForScripts() const;
    bool isExecutingScript() const final;
    bool hasScriptsWaitingForStylesheets() const final;
    void executeScriptsWaitingForStylesheets() final;
    void suspendScheduledTasks() final;
    void resumeScheduledTasks() final;

    bool shouldAssociateConsoleMessagesWithTextPosition() const final;

    // HTMLScriptRunnerHost
    void watchForLoad(PendingScript&) final;
    void stopWatchingForLoad(PendingScript&) final;
    HTMLInputStream& inputStream() final;
    bool hasPreloadScanner() const final;
    void appendCurrentInputStreamToPreloadScannerAndScan() final;

    // PendingScriptClient
    void notifyFinished(PendingScript&) final;

    Document* contextForParsingSession();

    enum SynchronousMode { AllowYield, ForceSynchronous };
    void append(RefPtr<StringImpl>&&, SynchronousMode);

    void pumpTokenizer(SynchronousMode);
    bool pumpTokenizerLoop(SynchronousMode, bool parsingFragment, PumpSession&);
    void pumpTokenizerIfPossible(SynchronousMode);
    void constructTreeFromHTMLToken(HTMLTokenizer::TokenPtr&);

    void runScriptsForPausedTreeBuilder();
    void resumeParsingAfterScriptExecution();

    void attemptToEnd();
    void endIfDelayed();
    void attemptToRunDeferredScriptsAndEnd();
    void end();

    bool isParsingFragment() const;
    bool isScheduledForResume() const;
    bool inPumpSession() const;
    bool shouldDelayEnd() const;

    void didBeginYieldingParser() final;
    void didEndYieldingParser() final;

    HTMLParserOptions m_options;
    HTMLInputStream m_input;

    HTMLTokenizer m_tokenizer;
    std::unique_ptr<HTMLScriptRunner> m_scriptRunner;
    std::unique_ptr<HTMLTreeBuilder> m_treeBuilder;
    std::unique_ptr<HTMLPreloadScanner> m_preloadScanner;
    std::unique_ptr<HTMLPreloadScanner> m_insertionPreloadScanner;
    std::unique_ptr<HTMLParserScheduler> m_parserScheduler;
    TextPosition m_textPosition;

    std::unique_ptr<HTMLResourcePreloader> m_preloader;

    bool m_endWasDelayed { false };
    unsigned m_pumpSessionNestingLevel { 0 };
    bool m_shouldEmitTracePoints { false };
};

inline HTMLTokenizer& HTMLDocumentParser::tokenizer()
{
    return m_tokenizer;
}

inline HTMLInputStream& HTMLDocumentParser::inputStream()
{
    return m_input;
}

inline bool HTMLDocumentParser::hasPreloadScanner() const
{
    return m_preloadScanner.get();
}

inline HTMLTreeBuilder& HTMLDocumentParser::treeBuilder()
{
    ASSERT(m_treeBuilder);
    return *m_treeBuilder;
}

} // namespace WebCore
