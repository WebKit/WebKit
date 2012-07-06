/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "SpellChecker.h"

#include "Document.h"
#include "DocumentMarkerController.h"
#include "EditorClient.h"
#include "Frame.h"
#include "HTMLInputElement.h"
#include "HTMLTextAreaElement.h"
#include "Node.h"
#include "Page.h"
#include "PositionIterator.h"
#include "Range.h"
#include "RenderObject.h"
#include "Settings.h"
#include "TextCheckerClient.h"
#include "TextCheckingHelper.h"
#include "TextIterator.h"
#include "htmlediting.h"

namespace WebCore {

static const int unrequestedSequence = -1;

SpellCheckRequest::SpellCheckRequest(int sequence, PassRefPtr<Range> checkingRange, PassRefPtr<Range> paragraphRange, const String& text, TextCheckingTypeMask mask, TextCheckingProcessType processType)
    : m_sequence(sequence)
    , m_text(text)
    , m_mask(mask)
    , m_processType(processType)
    , m_checkingRange(checkingRange)
    , m_paragraphRange(paragraphRange)
    , m_rootEditableElement(m_checkingRange->startContainer()->rootEditableElement())
{
}

SpellCheckRequest::~SpellCheckRequest()
{
}

// static
PassRefPtr<SpellCheckRequest> SpellCheckRequest::create(TextCheckingTypeMask textCheckingOptions, TextCheckingProcessType processType, PassRefPtr<Range> checkingRange, PassRefPtr<Range> paragraphRange)
{
    ASSERT(checkingRange);
    ASSERT(paragraphRange);

    String text = checkingRange->text();
    if (!text.length())
        return PassRefPtr<SpellCheckRequest>();

    return adoptRef(new SpellCheckRequest(unrequestedSequence, checkingRange, paragraphRange, text, textCheckingOptions, processType));
}



SpellChecker::SpellChecker(Frame* frame)
    : m_frame(frame)
    , m_lastRequestSequence(0)
    , m_lastProcessedSequence(0)
    , m_timerToProcessQueuedRequest(this, &SpellChecker::timerFiredToProcessQueuedRequest)
{
}

SpellChecker::~SpellChecker()
{
}

TextCheckerClient* SpellChecker::client() const
{
    Page* page = m_frame->page();
    if (!page)
        return 0;
    return page->editorClient()->textChecker();
}

void SpellChecker::timerFiredToProcessQueuedRequest(Timer<SpellChecker>*)
{
    ASSERT(!m_requestQueue.isEmpty());
    if (m_requestQueue.isEmpty())
        return;

    invokeRequest(m_requestQueue.takeFirst());
}

bool SpellChecker::isAsynchronousEnabled() const
{
    return m_frame->settings() && m_frame->settings()->asynchronousSpellCheckingEnabled();
}

bool SpellChecker::canCheckAsynchronously(Range* range) const
{
    return client() && isCheckable(range) && isAsynchronousEnabled();
}

bool SpellChecker::isCheckable(Range* range) const
{
    if (!range || !range->firstNode() || !range->firstNode()->renderer())
        return false;
    const Node* node = range->startContainer();
    if (node && node->isElementNode() && !toElement(node)->isSpellCheckingEnabled())
        return false;
    return true;
}

void SpellChecker::requestCheckingFor(PassRefPtr<SpellCheckRequest> request)
{
    if (!request || !canCheckAsynchronously(request->paragraphRange().get()))
        return;

    ASSERT(request->sequence() == unrequestedSequence);
    int sequence = ++m_lastRequestSequence;
    if (sequence == unrequestedSequence)
        sequence = ++m_lastRequestSequence;

    request->setSequence(sequence);

    if (m_timerToProcessQueuedRequest.isActive() || m_processingRequest) {
        enqueueRequest(request);
        return;
    }

    invokeRequest(request);
}

void SpellChecker::invokeRequest(PassRefPtr<SpellCheckRequest> request)
{
    ASSERT(!m_processingRequest);

    m_processingRequest = request;
    client()->requestCheckingOfString(this, m_processingRequest->textCheckingRequest());
}

void SpellChecker::enqueueRequest(PassRefPtr<SpellCheckRequest> request)
{
    ASSERT(request);

    for (RequestQueue::iterator it = m_requestQueue.begin(); it != m_requestQueue.end(); ++it) {
        if (request->rootEditableElement() != (*it)->rootEditableElement())
            continue;

        *it = request;
        return;
    }

    m_requestQueue.append(request);
}

void SpellChecker::didCheck(int sequence, const Vector<TextCheckingResult>& results)
{
    ASSERT(m_processingRequest);
    ASSERT(m_processingRequest->sequence() == sequence);
    if (m_processingRequest->sequence() != sequence) {
        m_requestQueue.clear();
        return;
    }

    m_frame->editor()->markAndReplaceFor(m_processingRequest, results);

    if (m_lastProcessedSequence < sequence)
        m_lastProcessedSequence = sequence;

    m_processingRequest.clear();
    if (!m_requestQueue.isEmpty())
        m_timerToProcessQueuedRequest.startOneShot(0);
}

void SpellChecker::didCheckSucceeded(int sequence, const Vector<TextCheckingResult>& results)
{
    if (m_processingRequest->sequence() == sequence) {
        unsigned markers = 0;
        if (m_processingRequest->mask() & TextCheckingTypeSpelling)
            markers |= DocumentMarker::Spelling;
        if (m_processingRequest->mask() & TextCheckingTypeGrammar)
            markers |= DocumentMarker::Grammar;
        if (markers)
            m_frame->document()->markers()->removeMarkers(m_processingRequest->checkingRange().get(), markers);
    }
    didCheck(sequence, results);
}

void SpellChecker::didCheckCanceled(int sequence)
{
    Vector<TextCheckingResult> results;
    didCheck(sequence, results);
}

} // namespace WebCore
