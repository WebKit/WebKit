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
#include "HTMLDocumentParser.h"

#include "DocumentFragment.h"
#include "Element.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "HTMLParserScheduler.h"
#include "HTMLTokenizer.h"
#include "HTMLPreloadScanner.h"
#include "HTMLScriptRunner.h"
#include "HTMLTreeBuilder.h"
#include "HTMLDocument.h"
#include "NestingLevelIncrementer.h"
#include "Settings.h"
#include "XSSAuditor.h"
#include <wtf/CurrentTime.h>

#if ENABLE(INSPECTOR)
#include "InspectorTimelineAgent.h"
#endif

namespace WebCore {

using namespace HTMLNames;

namespace {

// This is a direct transcription of step 4 from:
// http://www.whatwg.org/specs/web-apps/current-work/multipage/the-end.html#fragment-case
HTMLTokenizer::State tokenizerStateForContextElement(Element* contextElement, bool reportErrors)
{
    if (!contextElement)
        return HTMLTokenizer::DataState;

    const QualifiedName& contextTag = contextElement->tagQName();

    if (contextTag.matches(titleTag) || contextTag.matches(textareaTag))
        return HTMLTokenizer::RCDATAState;
    if (contextTag.matches(styleTag)
        || contextTag.matches(xmpTag)
        || contextTag.matches(iframeTag)
        || (contextTag.matches(noembedTag) && HTMLTreeBuilder::pluginsEnabled(contextElement->document()->frame()))
        || (contextTag.matches(noscriptTag) && HTMLTreeBuilder::scriptEnabled(contextElement->document()->frame()))
        || contextTag.matches(noframesTag))
        return reportErrors ? HTMLTokenizer::RAWTEXTState : HTMLTokenizer::PLAINTEXTState;
    if (contextTag.matches(scriptTag))
        return reportErrors ? HTMLTokenizer::ScriptDataState : HTMLTokenizer::PLAINTEXTState;
    if (contextTag.matches(plaintextTag))
        return HTMLTokenizer::PLAINTEXTState;
    return HTMLTokenizer::DataState;
}

} // namespace

HTMLDocumentParser::HTMLDocumentParser(HTMLDocument* document, bool reportErrors)
    : ScriptableDocumentParser(document)
    , m_tokenizer(HTMLTokenizer::create(usePreHTML5ParserQuirks(document)))
    , m_scriptRunner(HTMLScriptRunner::create(document, this))
    , m_treeBuilder(HTMLTreeBuilder::create(m_tokenizer.get(), document, reportErrors))
    , m_parserScheduler(HTMLParserScheduler::create(this))
    , m_endWasDelayed(false)
    , m_writeNestingLevel(0)
{
}

// FIXME: Member variables should be grouped into self-initializing structs to
// minimize code duplication between these constructors.
HTMLDocumentParser::HTMLDocumentParser(DocumentFragment* fragment, Element* contextElement, FragmentScriptingPermission scriptingPermission)
    : ScriptableDocumentParser(fragment->document())
    , m_tokenizer(HTMLTokenizer::create(usePreHTML5ParserQuirks(fragment->document())))
    , m_treeBuilder(HTMLTreeBuilder::create(m_tokenizer.get(), fragment, contextElement, scriptingPermission))
    , m_endWasDelayed(false)
    , m_writeNestingLevel(0)
{
    bool reportErrors = false; // For now document fragment parsing never reports errors.
    m_tokenizer->setState(tokenizerStateForContextElement(contextElement, reportErrors));
}

HTMLDocumentParser::~HTMLDocumentParser()
{
    ASSERT(!m_parserScheduler);
    ASSERT(!m_writeNestingLevel);
    ASSERT(!m_preloadScanner);
}

void HTMLDocumentParser::detach()
{
    DocumentParser::detach();
    if (m_scriptRunner)
        m_scriptRunner->detach();
    m_treeBuilder->detach();
    // FIXME: It seems wrong that we would have a preload scanner here.
    // Yet during fast/dom/HTMLScriptElement/script-load-events.html we do.
    m_preloadScanner.clear();
    m_parserScheduler.clear(); // Deleting the scheduler will clear any timers.
}

void HTMLDocumentParser::stopParsing()
{
    DocumentParser::stopParsing();
    m_parserScheduler.clear(); // Deleting the scheduler will clear any timers.
}

// This kicks off "Once the user agent stops parsing" as described by:
// http://www.whatwg.org/specs/web-apps/current-work/multipage/the-end.html#the-end
void HTMLDocumentParser::prepareToStopParsing()
{
    ASSERT(!hasInsertionPoint());

    // pumpTokenizer can cause this parser to be detached from the Document,
    // but we need to ensure it isn't deleted yet.
    RefPtr<HTMLDocumentParser> protect(this);

    // NOTE: This pump should only ever emit buffered character tokens,
    // so ForceSynchronous vs. AllowYield should be meaningless.
    pumpTokenizerIfPossible(ForceSynchronous);
    
    if (isStopped())
        return;

    DocumentParser::prepareToStopParsing();

    // We will not have a scriptRunner when parsing a DocumentFragment.
    if (m_scriptRunner)
        document()->setReadyState(Document::Interactive);

    attemptToRunDeferredScriptsAndEnd();
}

bool HTMLDocumentParser::processingData() const
{
    return isScheduledForResume() || inWrite();
}

void HTMLDocumentParser::pumpTokenizerIfPossible(SynchronousMode mode)
{
    if (isStopped() || m_treeBuilder->isPaused())
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
    RefPtr<HTMLDocumentParser> protect(this);

    // We should never be here unless we can pump immediately.  Call pumpTokenizer()
    // directly so that ASSERTS will fire if we're wrong.
    pumpTokenizer(AllowYield);
    endIfDelayed();
}

bool HTMLDocumentParser::runScriptsForPausedTreeBuilder()
{
    ASSERT(m_treeBuilder->isPaused());

    int scriptStartLine = 0;
    RefPtr<Element> scriptElement = m_treeBuilder->takeScriptToProcess(scriptStartLine);
    // We will not have a scriptRunner when parsing a DocumentFragment.
    if (!m_scriptRunner)
        return true;
    return m_scriptRunner->execute(scriptElement.release(), scriptStartLine);
}

void HTMLDocumentParser::pumpTokenizer(SynchronousMode mode)
{
    ASSERT(!isStopped());
    ASSERT(!m_treeBuilder->isPaused());
    ASSERT(!isScheduledForResume());
    // ASSERT that this object is both attached to the Document and protected.
    ASSERT(refCount() >= 2);

    // We tell the InspectorTimelineAgent about every pump, even if we
    // end up pumping nothing.  It can filter out empty pumps itself.
    willPumpLexer();

    HTMLParserScheduler::PumpSession session;
    // FIXME: This loop body has is now too long and needs cleanup.
    while (mode == ForceSynchronous || m_parserScheduler->shouldContinueParsing(session)) {
        if (!m_tokenizer->nextToken(m_input.current(), m_token))
            break;

        m_treeBuilder->constructTreeFromToken(m_token);
        m_token.clear();

        // JavaScript may have stopped or detached the parser.
        if (isStopped())
            return;

        // The parser will pause itself when waiting on a script to load or run.
        if (!m_treeBuilder->isPaused())
            continue;

        // If we're paused waiting for a script, we try to execute scripts before continuing.
        bool shouldContinueParsing = runScriptsForPausedTreeBuilder();
        m_treeBuilder->setPaused(!shouldContinueParsing);

        // JavaScript may have stopped or detached the parser.
        if (isStopped())
            return;

        if (!shouldContinueParsing)
            break;
    }

    // Ensure we haven't been totally deref'ed after pumping. Any caller of this
    // function should be holding a RefPtr to this to ensure we weren't deleted.
    ASSERT(refCount() >= 1);

    if (isWaitingForScripts()) {
        ASSERT(m_tokenizer->state() == HTMLTokenizer::DataState);
        if (!m_preloadScanner) {
            m_preloadScanner.set(new HTMLPreloadScanner(document()));
            m_preloadScanner->appendToEnd(m_input.current());
        }
        m_preloadScanner->scan();
    }

    didPumpLexer();
}

void HTMLDocumentParser::willPumpLexer()
{
#if ENABLE(INSPECTOR)
    // FIXME: m_input.current().length() is only accurate if we
    // end up parsing the whole buffer in this pump.  We should pass how
    // much we parsed as part of didWriteHTML instead of willWriteHTML.
    if (InspectorTimelineAgent* timelineAgent = document()->inspectorTimelineAgent())
        timelineAgent->willWriteHTML(m_input.current().length(), m_tokenizer->lineNumber());
#endif
}

void HTMLDocumentParser::didPumpLexer()
{
#if ENABLE(INSPECTOR)
    if (InspectorTimelineAgent* timelineAgent = document()->inspectorTimelineAgent())
        timelineAgent->didWriteHTML(m_tokenizer->lineNumber());
#endif
}

bool HTMLDocumentParser::hasInsertionPoint()
{
    return m_input.hasInsertionPoint();
}

void HTMLDocumentParser::insert(const SegmentedString& source)
{
    if (isStopped())
        return;

    // pumpTokenizer can cause this parser to be detached from the Document,
    // but we need to ensure it isn't deleted yet.
    RefPtr<HTMLDocumentParser> protect(this);

    {
        NestingLevelIncrementer nestingLevelIncrementer(m_writeNestingLevel);

        SegmentedString excludedLineNumberSource(source);
        excludedLineNumberSource.setExcludeLineNumbers();
        m_input.insertAtCurrentInsertionPoint(excludedLineNumberSource);
        pumpTokenizerIfPossible(ForceSynchronous);
    }

    endIfDelayed();
}

void HTMLDocumentParser::append(const SegmentedString& source)
{
    if (isStopped())
        return;

    // pumpTokenizer can cause this parser to be detached from the Document,
    // but we need to ensure it isn't deleted yet.
    RefPtr<HTMLDocumentParser> protect(this);

    {
        NestingLevelIncrementer nestingLevelIncrementer(m_writeNestingLevel);

        m_input.appendToEnd(source);
        if (m_preloadScanner)
            m_preloadScanner->appendToEnd(source);

        if (m_writeNestingLevel > 1) {
            // We've gotten data off the network in a nested write.
            // We don't want to consume any more of the input stream now.  Do
            // not worry.  We'll consume this data in a less-nested write().
            return;
        }

        pumpTokenizerIfPossible(AllowYield);
    }

    endIfDelayed();
}

void HTMLDocumentParser::end()
{
    ASSERT(!isDetached());
    ASSERT(!isScheduledForResume());

    // Informs the the rest of WebCore that parsing is really finished (and deletes this).
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
    // However, FrameLoader::stop calls Document::finishParsing unconditionally
    // which in turn calls m_parser->finish().

    // We're not going to get any more data off the network, so we tell the
    // input stream we've reached the end of file.  finish() can be called more
    // than once, if the first time does not call end().
    if (!m_input.haveSeenEndOfFile())
        m_input.markEndOfFile();
    attemptToEnd();
}

bool HTMLDocumentParser::finishWasCalled()
{
    return m_input.haveSeenEndOfFile();
}

// This function is virtual and just for the DocumentParser interface.
bool HTMLDocumentParser::isExecutingScript() const
{
    return inScriptExecution();
}

// This function is non-virtual and used throughout the implementation.
bool HTMLDocumentParser::inScriptExecution() const
{
    if (!m_scriptRunner)
        return false;
    return m_scriptRunner->isExecutingScript();
}

int HTMLDocumentParser::lineNumber() const
{
    return m_tokenizer->lineNumber();
}

int HTMLDocumentParser::columnNumber() const
{
    return m_tokenizer->columnNumber();
}

bool HTMLDocumentParser::isWaitingForScripts() const
{
    return m_treeBuilder->isPaused();
}

void HTMLDocumentParser::resumeParsingAfterScriptExecution()
{
    ASSERT(!inScriptExecution());
    ASSERT(!m_treeBuilder->isPaused());

    m_preloadScanner.clear();
    pumpTokenizerIfPossible(AllowYield);
    endIfDelayed();
}

void HTMLDocumentParser::watchForLoad(CachedResource* cachedScript)
{
    ASSERT(!cachedScript->isLoaded());
    // addClient would call notifyFinished if the load were complete.
    // Callers do not expect to be re-entered from this call, so they should
    // not an already-loaded CachedResource.
    cachedScript->addClient(this);
}

void HTMLDocumentParser::stopWatchingForLoad(CachedResource* cachedScript)
{
    cachedScript->removeClient(this);
}

bool HTMLDocumentParser::shouldLoadExternalScriptFromSrc(const AtomicString& srcValue)
{
    if (!xssAuditor())
        return true;
    return xssAuditor()->canLoadExternalScriptFromSrc(srcValue);
}

void HTMLDocumentParser::notifyFinished(CachedResource* cachedResource)
{
    // pumpTokenizer can cause this parser to be detached from the Document,
    // but we need to ensure it isn't deleted yet.
    RefPtr<HTMLDocumentParser> protect(this);

    ASSERT(m_scriptRunner);
    ASSERT(!inScriptExecution());
    if (isStopping()) {
        attemptToRunDeferredScriptsAndEnd();
        return;
    }

    ASSERT(m_treeBuilder->isPaused());
    // Note: We only ever wait on one script at a time, so we always know this
    // is the one we were waiting on and can un-pause the tree builder.
    m_treeBuilder->setPaused(false);
    bool shouldContinueParsing = m_scriptRunner->executeScriptsWaitingForLoad(cachedResource);
    m_treeBuilder->setPaused(!shouldContinueParsing);
    if (shouldContinueParsing)
        resumeParsingAfterScriptExecution();
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
    RefPtr<HTMLDocumentParser> protect(this);

    ASSERT(!m_scriptRunner->isExecutingScript());
    ASSERT(m_treeBuilder->isPaused());
    // Note: We only ever wait on one script at a time, so we always know this
    // is the one we were waiting on and can un-pause the tree builder.
    m_treeBuilder->setPaused(false);
    bool shouldContinueParsing = m_scriptRunner->executeScriptsWaitingForStylesheets();
    m_treeBuilder->setPaused(!shouldContinueParsing);
    if (shouldContinueParsing)
        resumeParsingAfterScriptExecution();
}

ScriptController* HTMLDocumentParser::script() const
{
    return document()->frame() ? document()->frame()->script() : 0;
}

void HTMLDocumentParser::parseDocumentFragment(const String& source, DocumentFragment* fragment, Element* contextElement, FragmentScriptingPermission scriptingPermission)
{
    RefPtr<HTMLDocumentParser> parser = HTMLDocumentParser::create(fragment, contextElement, scriptingPermission);
    parser->insert(source); // Use insert() so that the parser will not yield.
    parser->finish();
    ASSERT(!parser->processingData()); // Make sure we're done. <rdar://problem/3963151>
    parser->detach(); // Allows ~DocumentParser to assert it was detached before destruction.
}
    
bool HTMLDocumentParser::usePreHTML5ParserQuirks(Document* document)
{
    ASSERT(document);
    return document->settings() && document->settings()->usePreHTML5ParserQuirks();
}

}
