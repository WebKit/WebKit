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
#include "HTML5DocumentParser.h"

#include "DocumentFragment.h"
#include "Element.h"
#include "Frame.h"
#include "FrameView.h" // Only for isLayoutTimerActive
#include "HTML5Lexer.h"
#include "HTML5PreloadScanner.h"
#include "HTML5ScriptRunner.h"
#include "HTML5TreeBuilder.h"
#include "HTMLDocument.h"
#include "Page.h"
#include "XSSAuditor.h"
#include <wtf/CurrentTime.h>

#if ENABLE(INSPECTOR)
#include "InspectorTimelineAgent.h"
#endif

// defaultParserChunkSize is used to define how many tokens the parser will
// process before checking against parserTimeLimit and possibly yielding.
// This is a performance optimization to prevent checking after every token.
static const int defaultParserChunkSize = 4096;

// defaultParserTimeLimit is the seconds the parser will run in one write() call
// before yielding.  Inline <script> execution can cause it to excede the limit.
// FIXME: We would like this value to be 0.2.
static const double defaultParserTimeLimit = 0.500;

namespace WebCore {

namespace {

class NestingLevelIncrementer : public Noncopyable {
public:
    explicit NestingLevelIncrementer(int& counter)
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

static double parserTimeLimit(Page* page)
{
    // We're using the poorly named customHTMLTokenizerTimeDelay setting.
    if (page && page->hasCustomHTMLTokenizerTimeDelay())
        return page->customHTMLTokenizerTimeDelay();
    return defaultParserTimeLimit;
}

static int parserChunkSize(Page* page)
{
    // FIXME: We may need to divide the value from customHTMLTokenizerChunkSize
    // by some constant to translate from the "character" based behavior of the
    // old HTMLDocumentParser to the token-based behavior of this parser.
    if (page && page->hasCustomHTMLTokenizerChunkSize())
        return page->customHTMLTokenizerChunkSize();
    return defaultParserChunkSize;
}

HTML5DocumentParser::HTML5DocumentParser(HTMLDocument* document, bool reportErrors)
    : m_document(document)
    , m_lexer(new HTML5Lexer)
    , m_scriptRunner(new HTML5ScriptRunner(document, this))
    , m_treeConstructor(new HTML5TreeBuilder(m_lexer.get(), document, reportErrors))
    , m_endWasDelayed(false)
    , m_writeNestingLevel(0)
    , m_parserTimeLimit(parserTimeLimit(document->page()))
    , m_parserChunkSize(parserChunkSize(document->page()))
    , m_continueNextChunkTimer(this, &HTML5DocumentParser::continueNextChunkTimerFired)
{
    begin();
}

// FIXME: Member variables should be grouped into self-initializing structs to
// minimize code duplication between these constructors.
HTML5DocumentParser::HTML5DocumentParser(DocumentFragment* fragment, FragmentScriptingPermission scriptingPermission)
    : m_document(fragment->document())
    , m_lexer(new HTML5Lexer)
    , m_treeConstructor(new HTML5TreeBuilder(m_lexer.get(), fragment, scriptingPermission))
    , m_endWasDelayed(false)
    , m_writeNestingLevel(0)
    // FIXME: Parser yielding should be a separate class.
    , m_parserTimeLimit(0)
    , m_parserChunkSize(0)
    , m_continueNextChunkTimer(this, &HTML5DocumentParser::continueNextChunkTimerFired)
{
    begin();
}

HTML5DocumentParser::~HTML5DocumentParser()
{
    // FIXME: We'd like to ASSERT that normal operation of this class clears
    // out any delayed actions, but we can't because we're unceremoniously
    // deleted.  If there were a required call to some sort of cancel function,
    // then we could ASSERT some invariants here.
}

void HTML5DocumentParser::begin()
{
    // FIXME: Should we reset the lexer?
}

void HTML5DocumentParser::stopParsing()
{
    DocumentParser::stopParsing();
    m_continueNextChunkTimer.stop();
}

bool HTML5DocumentParser::processingData() const
{
    return m_continueNextChunkTimer.isActive() || inWrite();
}

void HTML5DocumentParser::pumpLexerIfPossible(SynchronousMode mode)
{
    if (m_parserStopped || m_treeConstructor->isPaused())
        return;

    // Once a timer is set, it has control of when the parser continues.
    if (m_continueNextChunkTimer.isActive()) {
        ASSERT(mode == AllowYield);
        return;
    }

    pumpLexer(mode);
}

struct HTML5DocumentParser::PumpSession {
    PumpSession()
        : processedTokens(0)
        , startTime(currentTime())
    {
    }

    int processedTokens;
    double startTime;
};

inline bool HTML5DocumentParser::shouldContinueParsing(PumpSession& session)
{
    if (m_parserStopped)
        return false;

    if (session.processedTokens > m_parserChunkSize) {
        session.processedTokens = 0;
        double elapsedTime = currentTime() - session.startTime;
        if (elapsedTime > m_parserTimeLimit) {
            // Schedule an immediate timer and yield from the parser.
            m_continueNextChunkTimer.startOneShot(0);
            return false;
        }
    }

    ++session.processedTokens;
    return true;
}

bool HTML5DocumentParser::runScriptsForPausedTreeConstructor()
{
    ASSERT(m_treeConstructor->isPaused());

    int scriptStartLine = 0;
    RefPtr<Element> scriptElement = m_treeConstructor->takeScriptToProcess(scriptStartLine);
    // We will not have a scriptRunner when parsing a DocumentFragment.
    if (!m_scriptRunner)
        return true;
    return m_scriptRunner->execute(scriptElement.release(), scriptStartLine);
}

void HTML5DocumentParser::pumpLexer(SynchronousMode mode)
{
    ASSERT(!m_parserStopped);
    ASSERT(!m_treeConstructor->isPaused());
    ASSERT(!m_continueNextChunkTimer.isActive());

    // We tell the InspectorTimelineAgent about every pump, even if we
    // end up pumping nothing.  It can filter out empty pumps itself.
    willPumpLexer();

    PumpSession session;
    // FIXME: This loop body has is now too long and needs cleanup.
    while (mode == ForceSynchronous || shouldContinueParsing(session)) {
        if (!m_lexer->nextToken(m_input.current(), m_token))
            break;

        m_treeConstructor->constructTreeFromToken(m_token);
        m_token.clear();

        // The parser will pause itself when waiting on a script to load or run.
        if (!m_treeConstructor->isPaused())
            continue;

        // If we're paused waiting for a script, we try to execute scripts before continuing.
        bool shouldContinueParsing = runScriptsForPausedTreeConstructor();
        m_treeConstructor->setPaused(!shouldContinueParsing);
        if (!shouldContinueParsing)
            break;
    }

    if (isWaitingForScripts()) {
        ASSERT(m_lexer->state() == HTML5Lexer::DataState);
        if (!m_preloadScanner) {
            m_preloadScanner.set(new HTML5PreloadScanner(m_document));
            m_preloadScanner->appendToEnd(m_input.current());
        }
        m_preloadScanner->scan();
    }

    didPumpLexer();
}

// FIXME: This belongs on Document.
static bool isLayoutTimerActive(Document* doc)
{
    ASSERT(doc);
    return doc->view() && doc->view()->layoutPending() && !doc->minimumLayoutDelay();
}

void HTML5DocumentParser::continueNextChunkTimerFired(Timer<HTML5DocumentParser>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_continueNextChunkTimer);
    // FIXME: The timer class should handle timer priorities instead of this code.
    // If a layout is scheduled, wait again to let the layout timer run first.
    if (isLayoutTimerActive(m_document)) {
        m_continueNextChunkTimer.startOneShot(0);
        return;
    }

    // We should never be here unless we can pump immediately.  Call pumpLexer()
    // directly so that ASSERTS will fire if we're wrong.
    pumpLexer(AllowYield);
}

void HTML5DocumentParser::willPumpLexer()
{
#if ENABLE(INSPECTOR)
    // FIXME: m_input.current().length() is only accurate if we
    // end up parsing the whole buffer in this pump.  We should pass how
    // much we parsed as part of didWriteHTML instead of willWriteHTML.
    if (InspectorTimelineAgent* timelineAgent = m_document->inspectorTimelineAgent())
        timelineAgent->willWriteHTML(m_input.current().length(), m_lexer->lineNumber());
#endif
}

void HTML5DocumentParser::didPumpLexer()
{
#if ENABLE(INSPECTOR)
    if (InspectorTimelineAgent* timelineAgent = m_document->inspectorTimelineAgent())
        timelineAgent->didWriteHTML(m_lexer->lineNumber());
#endif
}

void HTML5DocumentParser::write(const SegmentedString& source, bool isFromNetwork)
{
    if (m_parserStopped)
        return;

    NestingLevelIncrementer nestingLevelIncrementer(m_writeNestingLevel);

    if (isFromNetwork) {
        m_input.appendToEnd(source);
        if (m_preloadScanner)
            m_preloadScanner->appendToEnd(source);

        if (m_writeNestingLevel > 1) {
            // We've gotten data off the network in a nested call to write().
            // We don't want to consume any more of the input stream now.  Do
            // not worry.  We'll consume this data in a less-nested write().
            return;
        }
    } else
        m_input.insertAtCurrentInsertionPoint(source);

    pumpLexerIfPossible(isFromNetwork ? AllowYield : ForceSynchronous);
    endIfDelayed();
}

void HTML5DocumentParser::end()
{
    ASSERT(!m_continueNextChunkTimer.isActive());
    // NOTE: This pump should only ever emit buffered character tokens,
    // so ForceSynchronous vs. AllowYield should be meaningless.
    pumpLexerIfPossible(ForceSynchronous);

    // Informs the the rest of WebCore that parsing is really finished (and deletes this).
    m_treeConstructor->finished();
}

void HTML5DocumentParser::attemptToEnd()
{
    // finish() indicates we will not receive any more data. If we are waiting on
    // an external script to load, we can't finish parsing quite yet.

    if (inWrite() || isWaitingForScripts() || executingScript() || m_continueNextChunkTimer.isActive()) {
        m_endWasDelayed = true;
        return;
    }
    end();
}

void HTML5DocumentParser::endIfDelayed()
{
    // We don't check inWrite() here since inWrite() will be true if this was
    // called from write().
    if (!m_endWasDelayed || isWaitingForScripts() || executingScript() || m_continueNextChunkTimer.isActive())
        return;

    m_endWasDelayed = false;
    end();
}

void HTML5DocumentParser::finish()
{
    // We're not going to get any more data off the network, so we close the
    // input stream to indicate EOF.
    m_input.close();
    attemptToEnd();
}

bool HTML5DocumentParser::finishWasCalled()
{
    return m_input.isClosed();
}

// This function is virtual and just for the DocumentParser interface.
int HTML5DocumentParser::executingScript() const
{
    return inScriptExecution();
}

// This function is non-virtual and used throughout the implementation.
bool HTML5DocumentParser::inScriptExecution() const
{
    if (!m_scriptRunner)
        return false;
    return m_scriptRunner->inScriptExecution();
}

int HTML5DocumentParser::lineNumber() const
{
    return m_lexer->lineNumber();
}

int HTML5DocumentParser::columnNumber() const
{
    return m_lexer->columnNumber();
}

LegacyHTMLTreeConstructor* HTML5DocumentParser::htmlTreeConstructor() const
{
    return m_treeConstructor->legacyTreeConstructor();
}

bool HTML5DocumentParser::isWaitingForScripts() const
{
    return m_treeConstructor->isPaused();
}

void HTML5DocumentParser::resumeParsingAfterScriptExecution()
{
    ASSERT(!inScriptExecution());
    ASSERT(!m_treeConstructor->isPaused());

    pumpLexerIfPossible(AllowYield);

    // The document already finished parsing we were just waiting on scripts when finished() was called.
    endIfDelayed();
}

void HTML5DocumentParser::watchForLoad(CachedResource* cachedScript)
{
    ASSERT(!cachedScript->isLoaded());
    // addClient would call notifyFinished if the load were complete.
    // Callers do not expect to be re-entered from this call, so they should
    // not an already-loaded CachedResource.
    cachedScript->addClient(this);
}

void HTML5DocumentParser::stopWatchingForLoad(CachedResource* cachedScript)
{
    cachedScript->removeClient(this);
}

bool HTML5DocumentParser::shouldLoadExternalScriptFromSrc(const AtomicString& srcValue)
{
    if (!m_XSSAuditor)
        return true;
    return m_XSSAuditor->canLoadExternalScriptFromSrc(srcValue);
}

void HTML5DocumentParser::notifyFinished(CachedResource* cachedResource)
{
    ASSERT(m_scriptRunner);
    ASSERT(!inScriptExecution());
    ASSERT(m_treeConstructor->isPaused());
    // Note: We only ever wait on one script at a time, so we always know this
    // is the one we were waiting on and can un-pause the tree builder.
    m_treeConstructor->setPaused(false);
    bool shouldContinueParsing = m_scriptRunner->executeScriptsWaitingForLoad(cachedResource);
    m_treeConstructor->setPaused(!shouldContinueParsing);
    if (shouldContinueParsing)
        resumeParsingAfterScriptExecution();
}

void HTML5DocumentParser::executeScriptsWaitingForStylesheets()
{
    // Document only calls this when the Document owns the DocumentParser
    // so this will not be called in the DocumentFragment case.
    ASSERT(m_scriptRunner);
    // Ignore calls unless we have a script blocking the parser waiting on a
    // stylesheet load.  Otherwise we are currently parsing and this
    // is a re-entrant call from encountering a </ style> tag.
    if (!m_scriptRunner->hasScriptsWaitingForStylesheets())
        return;
    ASSERT(!m_scriptRunner->inScriptExecution());
    ASSERT(m_treeConstructor->isPaused());
    // Note: We only ever wait on one script at a time, so we always know this
    // is the one we were waiting on and can un-pause the tree builder.
    m_treeConstructor->setPaused(false);
    bool shouldContinueParsing = m_scriptRunner->executeScriptsWaitingForStylesheets();
    m_treeConstructor->setPaused(!shouldContinueParsing);
    if (shouldContinueParsing)
        resumeParsingAfterScriptExecution();
}

ScriptController* HTML5DocumentParser::script() const
{
    return m_document->frame() ? m_document->frame()->script() : 0;
}

}
