/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#include "HTMLDocumentParser.h"

#include "CustomElementReactionQueue.h"
#include "DocumentFragment.h"
#include "DocumentLoader.h"
#include "EventLoop.h"
#include "Frame.h"
#include "HTMLDocument.h"
#include "HTMLParserScheduler.h"
#include "HTMLPreloadScanner.h"
#include "HTMLScriptRunner.h"
#include "HTMLTreeBuilder.h"
#include "HTMLUnknownElement.h"
#include "JSCustomElementInterface.h"
#include "LinkLoader.h"
#include "NavigationScheduler.h"
#include "ScriptElement.h"
#include "ThrowOnDynamicMarkupInsertionCountIncrementer.h"

#include <wtf/SystemTracing.h>

namespace WebCore {

using namespace HTMLNames;

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(HTMLDocumentParser);

static bool isMainDocumentLoadingFromHTTP(const Document& document)
{
    return !document.ownerElement() && document.url().protocolIsInHTTPFamily();
}

HTMLDocumentParser::HTMLDocumentParser(HTMLDocument& document)
    : ScriptableDocumentParser(document)
    , m_options(document)
    , m_tokenizer(m_options)
    , m_scriptRunner(makeUnique<HTMLScriptRunner>(document, static_cast<HTMLScriptRunnerHost&>(*this)))
    , m_treeBuilder(makeUnique<HTMLTreeBuilder>(*this, document, parserContentPolicy(), m_options))
    , m_parserScheduler(makeUnique<HTMLParserScheduler>(*this))
    , m_xssAuditorDelegate(document)
    , m_preloader(makeUnique<HTMLResourcePreloader>(document))
    , m_shouldEmitTracePoints(isMainDocumentLoadingFromHTTP(document))
{
}

Ref<HTMLDocumentParser> HTMLDocumentParser::create(HTMLDocument& document)
{
    return adoptRef(*new HTMLDocumentParser(document));
}

inline HTMLDocumentParser::HTMLDocumentParser(DocumentFragment& fragment, Element& contextElement, ParserContentPolicy rawPolicy)
    : ScriptableDocumentParser(fragment.document(), rawPolicy)
    , m_options(fragment.document())
    , m_tokenizer(m_options)
    , m_treeBuilder(makeUnique<HTMLTreeBuilder>(*this, fragment, contextElement, parserContentPolicy(), m_options))
    , m_xssAuditorDelegate(fragment.document())
    , m_shouldEmitTracePoints(false) // Avoid emitting trace points when parsing fragments like outerHTML.
{
    // https://html.spec.whatwg.org/multipage/syntax.html#parsing-html-fragments
    if (contextElement.isHTMLElement())
        m_tokenizer.updateStateFor(contextElement.tagQName().localName());
    m_xssAuditor.initForFragment();
}

inline Ref<HTMLDocumentParser> HTMLDocumentParser::create(DocumentFragment& fragment, Element& contextElement, ParserContentPolicy parserContentPolicy)
{
    return adoptRef(*new HTMLDocumentParser(fragment, contextElement, parserContentPolicy));
}

HTMLDocumentParser::~HTMLDocumentParser()
{
    ASSERT(!m_parserScheduler);
    ASSERT(!m_pumpSessionNestingLevel);
    ASSERT(!m_preloadScanner);
    ASSERT(!m_insertionPreloadScanner);
}

void HTMLDocumentParser::detach()
{
    ScriptableDocumentParser::detach();

    if (m_scriptRunner)
        m_scriptRunner->detach();
    // FIXME: It seems wrong that we would have a preload scanner here.
    // Yet during fast/dom/HTMLScriptElement/script-load-events.html we do.
    m_preloadScanner = nullptr;
    m_insertionPreloadScanner = nullptr;
    m_parserScheduler = nullptr; // Deleting the scheduler will clear any timers.
}

void HTMLDocumentParser::stopParsing()
{
    DocumentParser::stopParsing();
    m_parserScheduler = nullptr; // Deleting the scheduler will clear any timers.
}

// This kicks off "Once the user agent stops parsing" as described by:
// https://html.spec.whatwg.org/multipage/syntax.html#the-end
void HTMLDocumentParser::prepareToStopParsing()
{
    ASSERT(!hasInsertionPoint());

    // pumpTokenizer can cause this parser to be detached from the Document,
    // but we need to ensure it isn't deleted yet.
    Ref<HTMLDocumentParser> protectedThis(*this);

    // NOTE: This pump should only ever emit buffered character tokens,
    // so ForceSynchronous vs. AllowYield should be meaningless.
    pumpTokenizerIfPossible(ForceSynchronous);

    if (isStopped())
        return;

    DocumentParser::prepareToStopParsing();

    // We will not have a scriptRunner when parsing a DocumentFragment.
    if (m_scriptRunner)
        document()->setReadyState(Document::Interactive);

    // Setting the ready state above can fire mutation event and detach us
    // from underneath. In that case, just bail out.
    if (isDetached())
        return;

    attemptToRunDeferredScriptsAndEnd();
}

inline bool HTMLDocumentParser::inPumpSession() const
{
    return m_pumpSessionNestingLevel > 0;
}

inline bool HTMLDocumentParser::shouldDelayEnd() const
{
    return inPumpSession() || isWaitingForScripts() || isScheduledForResume() || isExecutingScript();
}

void HTMLDocumentParser::didBeginYieldingParser()
{
    m_parserScheduler->didBeginYieldingParser();
}

void HTMLDocumentParser::didEndYieldingParser()
{
    m_parserScheduler->didEndYieldingParser();
}

bool HTMLDocumentParser::isParsingFragment() const
{
    return m_treeBuilder->isParsingFragment();
}

bool HTMLDocumentParser::processingData() const
{
    return isScheduledForResume() || inPumpSession();
}

void HTMLDocumentParser::pumpTokenizerIfPossible(SynchronousMode mode)
{
    if (isStopped() || isWaitingForScripts())
        return;

    // Once a resume is scheduled, HTMLParserScheduler controls when we next pump.
    if (isScheduledForResume()) {
        ASSERT(mode == AllowYield);
        return;
    }

    pumpTokenizer(mode);
}

bool HTMLDocumentParser::isScheduledForResume() const
{
    return m_parserScheduler && m_parserScheduler->isScheduledForResume();
}

// Used by HTMLParserScheduler
void HTMLDocumentParser::resumeParsingAfterYield()
{
    // pumpTokenizer can cause this parser to be detached from the Document,
    // but we need to ensure it isn't deleted yet.
    Ref<HTMLDocumentParser> protectedThis(*this);

    // We should never be here unless we can pump immediately.
    // Call pumpTokenizer() directly so that ASSERTS will fire if we're wrong.
    pumpTokenizer(AllowYield);
    endIfDelayed();
}

void HTMLDocumentParser::runScriptsForPausedTreeBuilder()
{
    ASSERT(scriptingContentIsAllowed(parserContentPolicy()));

    if (std::unique_ptr<CustomElementConstructionData> constructionData = m_treeBuilder->takeCustomElementConstructionData()) {
        ASSERT(!m_treeBuilder->hasParserBlockingScriptWork());

        // https://html.spec.whatwg.org/#create-an-element-for-the-token
        {
            // Prevent document.open/write during reactions by allocating the incrementer before the reactions stack.
            ThrowOnDynamicMarkupInsertionCountIncrementer incrementer(*document());

            document()->eventLoop().performMicrotaskCheckpoint();

            CustomElementReactionStack reactionStack(document()->globalObject());
            auto& elementInterface = constructionData->elementInterface.get();
            auto newElement = elementInterface.constructElementWithFallback(*document(), constructionData->name);
            m_treeBuilder->didCreateCustomOrFallbackElement(WTFMove(newElement), *constructionData);
        }
        return;
    }

    TextPosition scriptStartPosition = TextPosition::belowRangePosition();
    if (auto scriptElement = m_treeBuilder->takeScriptToProcess(scriptStartPosition)) {
        ASSERT(!m_treeBuilder->hasParserBlockingScriptWork());
        // We will not have a scriptRunner when parsing a DocumentFragment.
        if (m_scriptRunner)
            m_scriptRunner->execute(scriptElement.releaseNonNull(), scriptStartPosition);
    }
}

Document* HTMLDocumentParser::contextForParsingSession()
{
    // The parsing session should interact with the document only when parsing
    // non-fragments. Otherwise, we might delay the load event mistakenly.
    if (isParsingFragment())
        return nullptr;
    return document();
}

bool HTMLDocumentParser::pumpTokenizerLoop(SynchronousMode mode, bool parsingFragment, PumpSession& session)
{
    do {
        if (UNLIKELY(isWaitingForScripts())) {
            if (mode == AllowYield && m_parserScheduler->shouldYieldBeforeExecutingScript(m_treeBuilder->scriptToProcess(), session))
                return true;
            
            runScriptsForPausedTreeBuilder();
            // If we're paused waiting for a script, we try to execute scripts before continuing.
            if (isWaitingForScripts() || isStopped())
                return false;
        }

        // FIXME: It's wrong for the HTMLDocumentParser to reach back to the Frame, but this approach is
        // how the parser has always handled stopping when the page assigns window.location. What should
        // happen instead is that assigning window.location causes the parser to stop parsing cleanly.
        // The problem is we're not prepared to do that at every point where we run JavaScript.
        if (UNLIKELY(!parsingFragment && document()->frame() && document()->frame()->navigationScheduler().locationChangePending()))
            return false;

        if (UNLIKELY(mode == AllowYield && m_parserScheduler->shouldYieldBeforeToken(session)))
            return true;

        if (!parsingFragment)
            m_sourceTracker.startToken(m_input.current(), m_tokenizer);

        auto token = m_tokenizer.nextToken(m_input.current());
        if (!token)
            return false;

        if (!parsingFragment) {
            m_sourceTracker.endToken(m_input.current(), m_tokenizer);

            // We do not XSS filter innerHTML, which means we (intentionally) fail
            // http/tests/security/xssAuditor/dom-write-innerHTML.html
            if (auto xssInfo = m_xssAuditor.filterToken(FilterTokenRequest(*token, m_sourceTracker, m_tokenizer.shouldAllowCDATA())))
                m_xssAuditorDelegate.didBlockScript(*xssInfo);
        }

        constructTreeFromHTMLToken(token);
    } while (!isStopped());

    return false;
}

void HTMLDocumentParser::pumpTokenizer(SynchronousMode mode)
{
    ASSERT(!isStopped());
    ASSERT(!isScheduledForResume());

    // This is an attempt to check that this object is both attached to the Document and protected by something.
    ASSERT(refCount() >= 2);

    PumpSession session(m_pumpSessionNestingLevel, contextForParsingSession());

    m_xssAuditor.init(document(), &m_xssAuditorDelegate);

    auto emitTracePoint = [this](TracePointCode code) {
        if (!m_shouldEmitTracePoints)
            return;

        auto position = textPosition();
        tracePoint(code, position.m_line.oneBasedInt(), position.m_column.oneBasedInt());
    };

    emitTracePoint(ParseHTMLStart);
    bool shouldResume = pumpTokenizerLoop(mode, isParsingFragment(), session);
    emitTracePoint(ParseHTMLEnd);

    // Ensure we haven't been totally deref'ed after pumping. Any caller of this
    // function should be holding a RefPtr to this to ensure we weren't deleted.
    ASSERT(refCount() >= 1);

    if (isStopped() || isParsingFragment())
        return;

    if (shouldResume)
        m_parserScheduler->scheduleForResume();

    if (isWaitingForScripts() && !isDetached()) {
        ASSERT(m_tokenizer.isInDataState());
        if (!m_preloadScanner) {
            m_preloadScanner = makeUnique<HTMLPreloadScanner>(m_options, document()->url(), document()->deviceScaleFactor());
            m_preloadScanner->appendToEnd(m_input.current());
        }
        m_preloadScanner->scan(*m_preloader, *document());
    }
    // The viewport definition is known here, so we can load link preloads with media attributes.
    if (document()->loader())
        LinkLoader::loadLinksFromHeader(document()->loader()->response().httpHeaderField(HTTPHeaderName::Link), document()->url(), *document(), LinkLoader::MediaAttributeCheck::MediaAttributeNotEmpty);
}

void HTMLDocumentParser::constructTreeFromHTMLToken(HTMLTokenizer::TokenPtr& rawToken)
{
    AtomHTMLToken token(*rawToken);

    // We clear the rawToken in case constructTree
    // synchronously re-enters the parser. We don't clear the token immedately
    // for Character tokens because the AtomHTMLToken avoids copying the
    // characters by keeping a pointer to the underlying buffer in the
    // HTMLToken. Fortunately, Character tokens can't cause us to re-enter
    // the parser.
    //
    // FIXME: Stop clearing the rawToken once we start running the parser off
    // the main thread or once we stop allowing synchronous JavaScript
    // execution from parseAttribute.
    if (rawToken->type() != HTMLToken::Character) {
        // Clearing the TokenPtr makes sure we don't clear the HTMLToken a second time
        // later when the TokenPtr is destroyed.
        rawToken.clear();
    }

    m_treeBuilder->constructTree(WTFMove(token));
}

bool HTMLDocumentParser::hasInsertionPoint()
{
    // FIXME: The wasCreatedByScript() branch here might not be fully correct.
    // Our model of the EOF character differs slightly from the one in the spec
    // because our treatment is uniform between network-sourced and script-sourced
    // input streams whereas the spec treats them differently.
    return m_input.hasInsertionPoint() || (wasCreatedByScript() && !m_input.haveSeenEndOfFile());
}

void HTMLDocumentParser::insert(SegmentedString&& source)
{
    if (isStopped())
        return;

    // pumpTokenizer can cause this parser to be detached from the Document,
    // but we need to ensure it isn't deleted yet.
    Ref<HTMLDocumentParser> protectedThis(*this);

    source.setExcludeLineNumbers();
    m_input.insertAtCurrentInsertionPoint(WTFMove(source));
    pumpTokenizerIfPossible(ForceSynchronous);

    if (isWaitingForScripts() && !isDetached()) {
        // Check the document.write() output with a separate preload scanner as
        // the main scanner can't deal with insertions.
        if (!m_insertionPreloadScanner)
            m_insertionPreloadScanner = makeUnique<HTMLPreloadScanner>(m_options, document()->url(), document()->deviceScaleFactor());
        m_insertionPreloadScanner->appendToEnd(source);
        m_insertionPreloadScanner->scan(*m_preloader, *document());
    }

    endIfDelayed();
}

void HTMLDocumentParser::append(RefPtr<StringImpl>&& inputSource)
{
    if (isStopped())
        return;

    // pumpTokenizer can cause this parser to be detached from the Document,
    // but we need to ensure it isn't deleted yet.
    Ref<HTMLDocumentParser> protectedThis(*this);

    String source { WTFMove(inputSource) };

    if (m_preloadScanner) {
        if (m_input.current().isEmpty() && !isWaitingForScripts()) {
            // We have parsed until the end of the current input and so are now moving ahead of the preload scanner.
            // Clear the scanner so we know to scan starting from the current input point if we block again.
            m_preloadScanner = nullptr;
        } else {
            m_preloadScanner->appendToEnd(source);
            if (isWaitingForScripts())
                m_preloadScanner->scan(*m_preloader, *document());
        }
    }

    m_input.appendToEnd(source);

    if (inPumpSession()) {
        // We've gotten data off the network in a nested write.
        // We don't want to consume any more of the input stream now.  Do
        // not worry.  We'll consume this data in a less-nested write().
        return;
    }

    pumpTokenizerIfPossible(AllowYield);

    endIfDelayed();
}

void HTMLDocumentParser::end()
{
    ASSERT(!isDetached());
    ASSERT(!isScheduledForResume());

    // Informs the rest of WebCore that parsing is really finished (and deletes this).
    m_treeBuilder->finished();
}

void HTMLDocumentParser::attemptToRunDeferredScriptsAndEnd()
{
    ASSERT(isStopping());
    ASSERT(!hasInsertionPoint());
    if (m_scriptRunner && !m_scriptRunner->executeScriptsWaitingForParsing())
        return;
    end();
}

void HTMLDocumentParser::attemptToEnd()
{
    // finish() indicates we will not receive any more data. If we are waiting on
    // an external script to load, we can't finish parsing quite yet.

    if (shouldDelayEnd()) {
        m_endWasDelayed = true;
        return;
    }
    prepareToStopParsing();
}

void HTMLDocumentParser::endIfDelayed()
{
    // If we've already been detached, don't bother ending.
    if (isDetached())
        return;

    if (!m_endWasDelayed || shouldDelayEnd())
        return;

    m_endWasDelayed = false;
    prepareToStopParsing();
}

void HTMLDocumentParser::finish()
{
    // FIXME: We should ASSERT(!m_parserStopped) here, since it does not
    // makes sense to call any methods on DocumentParser once it's been stopped.
    // However, FrameLoader::stop calls DocumentParser::finish unconditionally.

    // We're not going to get any more data off the network, so we tell the
    // input stream we've reached the end of file. finish() can be called more
    // than once, if the first time does not call end().
    if (!m_input.haveSeenEndOfFile())
        m_input.markEndOfFile();

    attemptToEnd();
}

bool HTMLDocumentParser::isExecutingScript() const
{
    return m_scriptRunner && m_scriptRunner->isExecutingScript();
}

TextPosition HTMLDocumentParser::textPosition() const
{
    auto& currentString = m_input.current();
    return TextPosition(currentString.currentLine(), currentString.currentColumn());
}

bool HTMLDocumentParser::shouldAssociateConsoleMessagesWithTextPosition() const
{
    return inPumpSession() && !isExecutingScript();
}

bool HTMLDocumentParser::isWaitingForScripts() const
{
    if (isParsingFragment()) {
        // HTMLTreeBuilder may have a parser blocking script element but we ignore them during fragment parsing.
        ASSERT(!m_scriptRunner || !m_scriptRunner->hasParserBlockingScript());
        return false;
    }
    // When the TreeBuilder encounters a </script> tag, it returns to the HTMLDocumentParser
    // where the script is transfered from the treebuilder to the script runner.
    // The script runner will hold the script until its loaded and run. During
    // any of this time, we want to count ourselves as "waiting for a script" and thus
    // run the preload scanner, as well as delay completion of parsing.
    bool treeBuilderHasBlockingScript = m_treeBuilder->hasParserBlockingScriptWork();
    bool scriptRunnerHasBlockingScript = m_scriptRunner && m_scriptRunner->hasParserBlockingScript();
    // Since the parser is paused while a script runner has a blocking script, it should
    // never be possible to end up with both objects holding a blocking script.
    ASSERT(!(treeBuilderHasBlockingScript && scriptRunnerHasBlockingScript));
    // If either object has a blocking script, the parser should be paused.
    return treeBuilderHasBlockingScript || scriptRunnerHasBlockingScript;
}

void HTMLDocumentParser::resumeParsingAfterScriptExecution()
{
    ASSERT(!isExecutingScript());
    ASSERT(!isWaitingForScripts());

    // pumpTokenizer can cause this parser to be detached from the Document,
    // but we need to ensure it isn't deleted yet.
    Ref<HTMLDocumentParser> protectedThis(*this);

    m_insertionPreloadScanner = nullptr;
    pumpTokenizerIfPossible(AllowYield);
    endIfDelayed();
}

void HTMLDocumentParser::watchForLoad(PendingScript& pendingScript)
{
    ASSERT(!pendingScript.isLoaded());
    // setClient would call notifyFinished if the load were complete.
    // Callers do not expect to be re-entered from this call, so they should
    // not an already-loaded PendingScript.
    pendingScript.setClient(*this);
}

void HTMLDocumentParser::stopWatchingForLoad(PendingScript& pendingScript)
{
    pendingScript.clearClient();
}

void HTMLDocumentParser::appendCurrentInputStreamToPreloadScannerAndScan()
{
    ASSERT(m_preloadScanner);
    m_preloadScanner->appendToEnd(m_input.current());
    m_preloadScanner->scan(*m_preloader, *document());
}

void HTMLDocumentParser::notifyFinished(PendingScript& pendingScript)
{
    // pumpTokenizer can cause this parser to be detached from the Document,
    // but we need to ensure it isn't deleted yet.
    Ref<HTMLDocumentParser> protectedThis(*this);

    // After Document parser is stopped or detached, the parser-inserted deferred script execution should be ignored.
    if (isStopped())
        return;

    ASSERT(m_scriptRunner);
    ASSERT(!isExecutingScript());
    if (isStopping()) {
        attemptToRunDeferredScriptsAndEnd();
        return;
    }

    m_scriptRunner->executeScriptsWaitingForLoad(pendingScript);
    if (!isWaitingForScripts())
        resumeParsingAfterScriptExecution();
}

bool HTMLDocumentParser::hasScriptsWaitingForStylesheets() const
{
    return m_scriptRunner && m_scriptRunner->hasScriptsWaitingForStylesheets();
}

void HTMLDocumentParser::executeScriptsWaitingForStylesheets()
{
    // Document only calls this when the Document owns the DocumentParser
    // so this will not be called in the DocumentFragment case.
    ASSERT(m_scriptRunner);
    // Ignore calls unless we have a script blocking the parser waiting on a
    // stylesheet load.  Otherwise we are currently parsing and this
    // is a re-entrant call from encountering a </ style> tag.
    if (!m_scriptRunner->hasScriptsWaitingForStylesheets())
        return;

    // pumpTokenizer can cause this parser to be detached from the Document,
    // but we need to ensure it isn't deleted yet.
    Ref<HTMLDocumentParser> protectedThis(*this);
    m_scriptRunner->executeScriptsWaitingForStylesheets();
    if (!isWaitingForScripts())
        resumeParsingAfterScriptExecution();
}

void HTMLDocumentParser::parseDocumentFragment(const String& source, DocumentFragment& fragment, Element& contextElement, ParserContentPolicy parserContentPolicy)
{
    auto parser = create(fragment, contextElement, parserContentPolicy);
    parser->insert(source); // Use insert() so that the parser will not yield.
    parser->finish();
    ASSERT(!parser->processingData());
    parser->detach();
}
    
void HTMLDocumentParser::suspendScheduledTasks()
{
    if (m_parserScheduler)
        m_parserScheduler->suspend();
}

void HTMLDocumentParser::resumeScheduledTasks()
{
    if (m_parserScheduler)
        m_parserScheduler->resume();
}

}
