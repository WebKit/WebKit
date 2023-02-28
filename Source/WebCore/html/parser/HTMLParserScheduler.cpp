/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 * Copyright (C) 2013 Apple, Inc. All Rights Reserved.
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
#include "HTMLParserScheduler.h"

#include "Document.h"
#include "ElementInlines.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLDocumentParser.h"
#include "Page.h"
#include "ScriptController.h"
#include "ScriptElement.h"

namespace WebCore {

static Seconds parserTimeLimit(Page* page)
{
    // Always yield after exceeding this.
    constexpr auto defaultParserTimeLimit = 500_ms;

    // We're using the poorly named customHTMLTokenizerTimeDelay setting.
    if (page && page->hasCustomHTMLTokenizerTimeDelay())
        return Seconds(page->customHTMLTokenizerTimeDelay());
    return defaultParserTimeLimit;
}

ActiveParserSession::ActiveParserSession(Document* document)
    : m_document(document)
{
    if (!m_document)
        return;
    m_document->incrementActiveParserCount();
}

ActiveParserSession::~ActiveParserSession()
{
    if (!m_document)
        return;
    m_document->decrementActiveParserCount();
}

PumpSession::PumpSession(unsigned& nestingLevel, Document* document)
    : NestingLevelIncrementer(nestingLevel)
    , ActiveParserSession(document)
{
}

PumpSession::~PumpSession() = default;

HTMLParserScheduler::HTMLParserScheduler(HTMLDocumentParser& parser)
    : m_parser(parser)
    , m_parserTimeLimit(parserTimeLimit(m_parser.document()->page()))
    , m_continueNextChunkTimer(*this, &HTMLParserScheduler::continueNextChunkTimerFired)
    , m_isSuspendedWithActiveTimer(false)
#if ASSERT_ENABLED
    , m_suspended(false)
#endif
{
}

HTMLParserScheduler::~HTMLParserScheduler()
{
    m_continueNextChunkTimer.stop();
}

void HTMLParserScheduler::continueNextChunkTimerFired()
{
    ASSERT(!m_suspended);

    // FIXME: The timer class should handle timer priorities instead of this code.
    // If a layout is scheduled, wait again to let the layout timer run first.
    if (m_parser.document()->isLayoutPending()) {
        m_continueNextChunkTimer.startOneShot(0_s);
        return;
    }
    m_parser.resumeParsingAfterYield();
}

bool HTMLParserScheduler::shouldYieldBeforeExecutingScript(const ScriptElement* scriptElement, PumpSession& session)
{
    // If we've never painted before and a layout is pending, yield prior to running
    // scripts to give the page a chance to paint earlier.
    RefPtr<Document> document = m_parser.document();

    session.didSeeScript = true;

    if (!document->body())
        return false;

    if (!document->frame() || !document->frame()->script().canExecuteScripts(NotAboutToExecuteScript))
        return false;

    if (!document->haveStylesheetsLoaded())
        return false;

    if (UNLIKELY(m_documentHasActiveParserYieldTokens))
        return true;

    auto elapsedTime = MonotonicTime::now() - session.startTime;

    constexpr auto elapsedTimeLimit = 16_ms;
    // Require at least some new parsed content before yielding.
    constexpr auto tokenLimit = 256;
    // Don't yield on very short inline scripts. This is an imperfect way to try to guess the execution cost.
    constexpr auto inlineScriptLengthLimit = 1024;

    if (elapsedTime < elapsedTimeLimit)
        return false;
    if (session.processedTokens < tokenLimit)
        return false;

    if (scriptElement) {
        // Async and deferred scripts are not executed by the parser.
        if (scriptElement->hasAsyncAttribute() || scriptElement->hasDeferAttribute())
            return false;
        if (!scriptElement->hasSourceAttribute() && scriptElement->scriptContent().length() < inlineScriptLengthLimit)
            return false;
    }

    return true;
}

void HTMLParserScheduler::scheduleForResume()
{
    ASSERT(!m_suspended);
    m_continueNextChunkTimer.startOneShot(0_s);
}

void HTMLParserScheduler::suspend()
{
    ASSERT(!m_suspended);
    ASSERT(!m_isSuspendedWithActiveTimer);
#if ASSERT_ENABLED
    m_suspended = true;
#endif

    if (!m_continueNextChunkTimer.isActive())
        return;
    m_isSuspendedWithActiveTimer = true;
    m_continueNextChunkTimer.stop();
}

void HTMLParserScheduler::resume()
{
    ASSERT(m_suspended);
    ASSERT(!m_continueNextChunkTimer.isActive());
#if ASSERT_ENABLED
    m_suspended = false;
#endif

    if (!m_isSuspendedWithActiveTimer)
        return;
    m_isSuspendedWithActiveTimer = false;
    m_continueNextChunkTimer.startOneShot(0_s);
}

}
