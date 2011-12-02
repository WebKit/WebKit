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

class SpellChecker::SpellCheckRequest : public RefCounted<SpellChecker::SpellCheckRequest> {
public:
    SpellCheckRequest(int sequence, PassRefPtr<Range> range, const String& text, TextCheckingTypeMask mask)
        : m_sequence(sequence)
        , m_range(range)
        , m_text(text)
        , m_mask(mask)
        , m_rootEditableElement(m_range->startContainer()->rootEditableElement())
    {
    }

    int sequence() const { return m_sequence; }
    Range* range() const { return m_range.get(); }
    const String& text() const { return m_text; }
    TextCheckingTypeMask mask() const { return m_mask; }
    Element* rootEditableElement() const { return m_rootEditableElement; }

private:
    int m_sequence;
    RefPtr<Range> m_range;
    String m_text;
    TextCheckingTypeMask m_mask;
    Element* m_rootEditableElement;
};

SpellChecker::SpellChecker(Frame* frame)
    : m_frame(frame)
    , m_lastRequestedSequence(0)
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

PassRefPtr<SpellChecker::SpellCheckRequest> SpellChecker::createRequest(TextCheckingTypeMask mask, PassRefPtr<Range> range)
{
    ASSERT(canCheckAsynchronously(range.get()));

    String text = range->text();
    if (!text.length())
        return PassRefPtr<SpellCheckRequest>();

    return adoptRef(new SpellCheckRequest(++m_lastRequestedSequence, range, text, mask));
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
    return range && range->firstNode() && range->firstNode()->renderer();
}

void SpellChecker::requestCheckingFor(TextCheckingTypeMask mask, PassRefPtr<Range> range)
{
    if (!canCheckAsynchronously(range.get()))
        return;

    RefPtr<SpellCheckRequest> request(createRequest(mask, range));
    if (!request)
        return;

    if (m_timerToProcessQueuedRequest.isActive() || m_processingRequest) {
        enqueueRequest(request.release());
        return;
    }

    invokeRequest(request.release());
}

void SpellChecker::invokeRequest(PassRefPtr<SpellCheckRequest> request)
{
    ASSERT(!m_processingRequest);

    client()->requestCheckingOfString(this, request->sequence(), request->mask(), request->text());
    m_processingRequest = request;
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

static bool forwardIterator(PositionIterator& iterator, int distance)
{
    int remaining = distance;
    while (!iterator.atEnd()) {
        if (iterator.node()->isCharacterDataNode()) {
            int length = lastOffsetForEditing(iterator.node());
            int last = length - iterator.offsetInLeafNode();
            if (remaining < last) {
                iterator.setOffsetInLeafNode(iterator.offsetInLeafNode() + remaining);
                return true;
            }

            remaining -= last;
            iterator.setOffsetInLeafNode(iterator.offsetInLeafNode() + last);
        }

        iterator.increment();
    }

    return false;    
}

static DocumentMarker::MarkerType toMarkerType(TextCheckingType type)
{
    if (type == TextCheckingTypeSpelling)
        return DocumentMarker::Spelling;
    ASSERT(type == TextCheckingTypeGrammar);
    return DocumentMarker::Grammar;
}

// Currenntly ignoring TextCheckingResult::details but should be handled. See Bug 56368.
void SpellChecker::didCheck(int sequence, const Vector<TextCheckingResult>& results)
{
    ASSERT(m_processingRequest);

    ASSERT(m_processingRequest->sequence() == sequence);
    if (m_processingRequest->sequence() != sequence) {
        m_requestQueue.clear();
        return;
    }

    int startOffset = 0;
    PositionIterator start = m_processingRequest->range()->startPosition();
    for (size_t i = 0; i < results.size(); ++i) {
        if (results[i].type != TextCheckingTypeSpelling && results[i].type != TextCheckingTypeGrammar)
            continue;

        // To avoid moving the position backward, we assume the given results are sorted with
        // startOffset as the ones returned by [NSSpellChecker requestCheckingOfString:].
        ASSERT(startOffset <= results[i].location);
        if (!forwardIterator(start, results[i].location - startOffset))
            break;
        PositionIterator end = start;
        if (!forwardIterator(end, results[i].length))
            break;

        // Users or JavaScript applications may change text while a spell-checker checks its
        // spellings in the background. To avoid adding markers to the words modified by users or
        // JavaScript applications, retrieve the words in the specified region and compare them with
        // the original ones.
        RefPtr<Range> range = Range::create(m_processingRequest->range()->ownerDocument(), start, end);
        // FIXME: Use textContent() compatible string conversion.
        String destination = range->text();
        String source = m_processingRequest->text().substring(results[i].location, results[i].length);
        if (destination == source)
            m_processingRequest->range()->ownerDocument()->markers()->addMarker(range.get(), toMarkerType(results[i].type));

        startOffset = results[i].location;
    }

    m_processingRequest.clear();
    if (!m_requestQueue.isEmpty())
        m_timerToProcessQueuedRequest.startOneShot(0);
}


} // namespace WebCore
