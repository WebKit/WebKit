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

#include "DocumentMarkerController.h"
#include "DocumentPage.h"
#include "Editing.h"
#include "Editor.h"
#include "EditorClient.h"
#include "LocalFrame.h"
#include "Page.h"
#include "PositionIterator.h"
#include "Range.h"
#include "RenderObject.h"
#include "Settings.h"
#include "TextCheckerClient.h"
#include "TextIterator.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/SetForScope.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SpellChecker);

SpellCheckRequest::SpellCheckRequest(const SimpleRange& checkingRange, const SimpleRange& automaticReplacementRange, const SimpleRange& paragraphRange, const String& text, OptionSet<TextCheckingType> options, TextCheckingProcessType type)
    : m_checkingRange(checkingRange)
    , m_automaticReplacementRange(automaticReplacementRange)
    , m_paragraphRange(paragraphRange)
    , m_rootEditableElement(m_checkingRange.start.container->rootEditableElement())
    , m_requestData(std::nullopt, text, options, type)
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
    m_checker->didCheckSucceed(m_requestData.identifier().value(), results, m_existingResults, m_checkingRange);
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

void SpellCheckRequest::setExistingResults(const Vector<TextCheckingResult>& existingResults)
{
    m_existingResults = existingResults;
}

void SpellCheckRequest::requesterDestroyed()
{
    m_checker = nullptr;
}

SpellChecker::SpellChecker(Editor& editor)
    : m_editor(editor)
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

void SpellChecker::ref() const
{
    m_editor->ref();
}

void SpellChecker::deref() const
{
    m_editor->deref();
}

TextCheckerClient* SpellChecker::client() const
{
    RefPtr page = document().page();
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
    return document().settings().asynchronousSpellCheckingEnabled();
}

bool SpellChecker::canCheckAsynchronously(const SimpleRange& range) const
{
    return client() && isCheckable(range) && isAsynchronousEnabled();
}

bool SpellChecker::isCheckable(const SimpleRange& range) const
{
    bool foundRenderer = false;
    for (Ref node : intersectingNodes(range)) {
        if (node->renderer()) {
            foundRenderer = true;
            break;
        }
    }
    if (!foundRenderer)
        return false;
    RefPtr element = dynamicDowncast<Element>(range.start.container.get());
    return !element || element->isSpellCheckingEnabled();
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
        enqueueRequest(WTF::move(request));
        return;
    }

    invokeRequest(WTF::move(request));
}

void SpellChecker::requestExtendedCheckingFor(Ref<SpellCheckRequest>&& request, const Vector<TextCheckingResult>& results)
{
    if (m_inRecheck)
        return;

    auto identifier = TextCheckingRequestIdentifier::generate();
    request->setCheckerAndIdentifier(this, identifier);
    request->setExistingResults(results);

    client()->requestExtendedCheckingOfString(WTF::move(request), protect(document())->selection().selection());
}

void SpellChecker::invokeRequest(Ref<SpellCheckRequest>&& request)
{
    ASSERT(!m_processingRequest);
    if (!client())
        return;
    m_processingRequest = WTF::move(request);
    client()->requestCheckingOfString(*m_processingRequest, protect(document())->selection().selection());
}

void SpellChecker::enqueueRequest(Ref<SpellCheckRequest>&& request)
{
    for (auto& queue : m_requestQueue) {
        if (request->rootEditableElement() != queue->rootEditableElement())
            continue;

        queue = WTF::move(request);
        return;
    }

    m_requestQueue.append(WTF::move(request));
}

static bool containsGrammarResult(TextCheckingResult result, const Vector<TextCheckingResult>& existingResults)
{
    bool foundIt = false;
    for (TextCheckingResult existingResult : existingResults) {
        if (!existingResult.type.containsOnly({ TextCheckingType::Grammar }) || result.range.location != existingResult.range.location || result.range.length != existingResult.range.length || result.details.size() != existingResult.details.size())
            continue;
        bool detailsMatch = true;
        for (auto [detail, existingResultDetail] : zippedRange(result.details, existingResult.details)) {
            if (!detailsMatch)
                break;
            detailsMatch = std::tie(detail.range.location, detail.range.length, detail.guesses) == std::tie(existingResultDetail.range.location, existingResultDetail.range.length, existingResultDetail.guesses);
        }
        if (detailsMatch)
            foundIt = true;
    }
    return foundIt;
}

static bool containsAdditionalGrammarResults(const Vector<TextCheckingResult>& results, const Vector<TextCheckingResult>& existingResults)
{
    for (const auto& result : results) {
        if (result.type.containsOnly({ TextCheckingType::Grammar }) && !containsGrammarResult(result, existingResults))
            return true;
    }
    return false;
}

void SpellChecker::didCheck(TextCheckingRequestIdentifier identifier, const Vector<TextCheckingResult>& results, const Vector<TextCheckingResult>& existingResults, const std::optional<SimpleRange>& range)
{
    if (!m_processingRequest || m_processingRequest->data().identifier() != identifier) {
        // This is the extended checking case
        if (!range || !containsAdditionalGrammarResults(results, existingResults))
            return;
        VisibleSelection selection = VisibleSelection(*range);
        SetForScope isRecheckingForScope(m_inRecheck, true);
        protect(document())->editor().markMisspellingsAndBadGrammar(selection);
        return;
    }

    protect(document())->editor().markAndReplaceFor(*m_processingRequest, results);

    if (!m_lastProcessedIdentifier || *m_lastProcessedIdentifier < identifier)
        m_lastProcessedIdentifier = identifier;

    m_processingRequest = nullptr;
    if (!m_requestQueue.isEmpty())
        m_timerToProcessQueuedRequest.startOneShot(0_s);
}

Document& SpellChecker::document() const
{
    return m_editor->document();
}

void SpellChecker::didCheckSucceed(TextCheckingRequestIdentifier identifier, const Vector<TextCheckingResult>& results, const Vector<TextCheckingResult>& existingResults, const std::optional<SimpleRange>& range)
{
    if (m_processingRequest) {
        TextCheckingRequestData requestData = m_processingRequest->data();
        if (requestData.identifier() == identifier) {
            OptionSet<DocumentMarkerType> markerTypes;
            if (requestData.checkingTypes().contains(TextCheckingType::Spelling))
                markerTypes.add(DocumentMarkerType::Spelling);
            if (requestData.checkingTypes().contains(TextCheckingType::Grammar))
                markerTypes.add(DocumentMarkerType::Grammar);
            if (!markerTypes.isEmpty())
                removeMarkers(m_processingRequest->checkingRange(), markerTypes);
        }
    }
    didCheck(identifier, results, existingResults, range);
}

void SpellChecker::didCheckCancel(TextCheckingRequestIdentifier identifier)
{
    didCheck(identifier, Vector<TextCheckingResult>(), Vector<TextCheckingResult>(), std::nullopt);
}

} // namespace WebCore
