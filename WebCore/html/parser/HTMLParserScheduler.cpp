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
#include "HTMLParserScheduler.h"

#include "FrameView.h" // Only for isLayoutTimerActive
#include "HTMLDocumentParser.h"
#include "Document.h"

// defaultParserChunkSize is used to define how many tokens the parser will
// process before checking against parserTimeLimit and possibly yielding.
// This is a performance optimization to prevent checking after every token.
static const int defaultParserChunkSize = 4096;

// defaultParserTimeLimit is the seconds the parser will run in one write() call
// before yielding.  Inline <script> execution can cause it to excede the limit.
// FIXME: We would like this value to be 0.2.
static const double defaultParserTimeLimit = 0.500;

namespace WebCore {

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
    // old LegacyHTMLDocumentParser to the token-based behavior of this parser.
    if (page && page->hasCustomHTMLTokenizerChunkSize())
        return page->customHTMLTokenizerChunkSize();
    return defaultParserChunkSize;
}

HTMLParserScheduler::HTMLParserScheduler(HTMLDocumentParser* parser)
    : m_parser(parser)
    , m_parserTimeLimit(parserTimeLimit(m_parser->document()->page()))
    , m_parserChunkSize(parserChunkSize(m_parser->document()->page()))
    , m_continueNextChunkTimer(this, &HTMLParserScheduler::continueNextChunkTimerFired)
{
}

HTMLParserScheduler::~HTMLParserScheduler()
{
    m_continueNextChunkTimer.stop();
}

// FIXME: This belongs on Document.
static bool isLayoutTimerActive(Document* doc)
{
    ASSERT(doc);
    return doc->view() && doc->view()->layoutPending() && !doc->minimumLayoutDelay();
}

void HTMLParserScheduler::continueNextChunkTimerFired(Timer<HTMLParserScheduler>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_continueNextChunkTimer);
    // FIXME: The timer class should handle timer priorities instead of this code.
    // If a layout is scheduled, wait again to let the layout timer run first.
    if (isLayoutTimerActive(m_parser->document())) {
        m_continueNextChunkTimer.startOneShot(0);
        return;
    }
    m_parser->resumeParsingAfterYield();
}

}
