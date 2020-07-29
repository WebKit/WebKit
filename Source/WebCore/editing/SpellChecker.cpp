/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "SpellChecker.h"

#include "Document.h"
#include "DocumentMarkerController.h"
#include "Editing.h"
#include "Editor.h"
#include "EditorClient.h"
#include "Frame.h"
#include "Page.h"
#include "PositionIterator.h"
#include "RenderObject.h"
#include "Settings.h"
#include "TextCheckerClient.h"
#include "TextIterator.h"

namespace WebCore {

SpellCheckRequest::SpellCheckRequest(const SimpleRange& checkingRange, const SimpleRange& automaticReplacementRange, const SimpleRange& paragraphRange, const String& text, OptionSet<TextCheckingType> options, TextCheckingProcessType type)
    : m_checkingRange(checkingRange)
    , m_automaticReplacementRange(automaticReplacementRange)
    , m_paragraphRange(paragraphRange)
    , m_rootEditableElement(m_checkingRange.start.container->rootEditableElement())
    , m_requestData(WTF::nullopt, text, options, type)
{
}

SpellCheckRequest::~SpellCheckRequest() = default;

RefPtr<SpellCheckRequest> SpellCheckRequest::create(OptionSet<TextCheckingType> options, TextCheckingProcessType type, const SimpleRange& checkingRange, const SimpleRange& automaticReplacementRange, const SimpleRange& paragraphRange)
{
    String text = plainText(checkingRange);
    if (text.isEmpty())
        return nullptr;
    return adoptRef(*new SpellCheckRequest(checkingRange, automaticReplacementRange, paragraphRange, text, options, type));
}

const TextCheckingRequestData& SpellCheckRequest::data() const
{
    return m_requestData;
}

void SpellCheckRequest::didSucceed(const Vector<TextCheckingResult>& results)
{
    if (!m_checker)
        return;

    Ref<SpellCheckRequest> protectedThis(*this);
    m_checker->didCheckSucceed(m_requestData.identifier().value(), results);
    m_checker = nullptr;
}

void SpellCheckRequest::didCancel()
{
    if (!m_checker)
        return;

    Ref<SpellCheckRequest> protectedThis(*this);
    m_checker->didCheckCancel(m_requestData.identifier().value());
    m_checker = nullptr;
}

void SpellCheckRequest::setCheckerAndIdentifier(SpellChecker* requester, TextCheckingRequestIdentifier identifier)
{
    ASSERT(!m_checker);
    ASSERT(!m_requestData.identifier());
    m_checker = requester;
    m_requestData.m_identifier = identifier;
}

void SpellCheckRequest::requesterDestroyed()
{
    m_checker = nullptr;
}

SpellChecker::SpellChecker(Document& document)
    : m_document(document)
    , m_timerToProcessQueuedRequest(*this, &SpellChecker::timerFiredToProcessQueuedRequest)
{
}

SpellChecker::~SpellChecker()
{
    if (m_processingRequest)
        m_processingRequest->requesterDestroyed();
    for (auto& queue : m_requestQueue)
        queue->requesterDestroyed();
}

TextCheckerClient* SpellChecker::client() const
{
    Page* page = m_document.page();
    if (!page)
        return nullptr;
    return page->editorClient().textChecker();
}

void SpellChecker::timerFiredToProcessQueuedRequest()
{
    ASSERT(!m_requestQueue.isEmpty());
    if (m_requestQueue.isEmpty())
        return;

    invokeRequest(m_requestQueue.takeFirst());
}

bool SpellChecker::isAsynchronousEnabled() const
{
    return m_document.settings().asynchronousSpellCheckingEnabled();
}

bool SpellChecker::canCheckAsynchronously(const SimpleRange& range) const
{
    return client() && isCheckable(range) && isAsynchronousEnabled();
}

bool SpellChecker::isCheckable(const SimpleRange& range) const
{
    auto firstNode = createLiveRange(range)->firstNode();
    if (!firstNode || !firstNode->renderer())
        return false;
    auto& node = range.start.container.get();
    return !is<Element>(node) || downcast<Element>(node).isSpellCheckingEnabled();
}

void SpellChecker::requestCheckingFor(Ref<SpellCheckRequest>&& request)
{
    if (!canCheckAsynchronously(request->paragraphRange()))
        return;

    ASSERT(!request->data().identifier());
    auto identifier = TextCheckingRequestIdentifier::generate();

    m_lastRequestIdentifier = identifier;
    request->setCheckerAndIdentifier(this, identifier);

    if (m_timerToProcessQueuedRequest.isActive() || m_processingRequest) {
        enqueueRequest(WTFMove(request));
        return;
    }

    invokeRequest(WTFMove(request));
}

void SpellChecker::invokeRequest(Ref<SpellCheckRequest>&& request)
{
    ASSERT(!m_processingRequest);
    if (!client())
        return;
    m_processingRequest = WTFMove(request);
    client()->requestCheckingOfString(*m_processingRequest, m_document.selection().selection());
}

void SpellChecker::enqueueRequest(Ref<SpellCheckRequest>&& request)
{
    for (auto& queue : m_requestQueue) {
        if (request->rootEditableElement() != queue->rootEditableElement())
            continue;

        queue = WTFMove(request);
        return;
    }

    m_requestQueue.append(WTFMove(request));
}

void SpellChecker::didCheck(TextCheckingRequestIdentifier identifier, const Vector<TextCheckingResult>& results)
{
    ASSERT(m_processingRequest);
    ASSERT(m_processingRequest->data().identifier() == identifier);
    if (m_processingRequest->data().identifier() != identifier) {
        m_requestQueue.clear();
        return;
    }

    m_document.editor().markAndReplaceFor(*m_processingRequest, results);

    if (m_lastProcessedIdentifier.toUInt64() < identifier.toUInt64())
        m_lastProcessedIdentifier = identifier;

    m_processingRequest = nullptr;
    if (!m_requestQueue.isEmpty())
        m_timerToProcessQueuedRequest.startOneShot(0_s);
}

void SpellChecker::didCheckSucceed(TextCheckingRequestIdentifier identifier, const Vector<TextCheckingResult>& results)
{
    TextCheckingRequestData requestData = m_processingRequest->data();
    if (requestData.identifier() == identifier) {
        OptionSet<DocumentMarker::MarkerType> markerTypes;
        if (requestData.checkingTypes().contains(TextCheckingType::Spelling))
            markerTypes.add(DocumentMarker::Spelling);
        if (requestData.checkingTypes().contains(TextCheckingType::Grammar))
            markerTypes.add(DocumentMarker::Grammar);
        if (!markerTypes.isEmpty())
            removeMarkers(m_processingRequest->checkingRange(), markerTypes);
    }
    didCheck(identifier, results);
}

void SpellChecker::didCheckCancel(TextCheckingRequestIdentifier identifier)
{
    didCheck(identifier, Vector<TextCheckingResult>());
}

} // namespace WebCore
