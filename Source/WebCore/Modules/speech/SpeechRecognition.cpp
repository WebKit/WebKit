/*
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SpeechRecognition.h"

#include "ClientOrigin.h"
#include "Document.h"
#include "EventNames.h"
#include "FeaturePolicy.h"
#include "FrameDestructionObserverInlines.h"
#include "Page.h"
#include "SpeechRecognitionError.h"
#include "SpeechRecognitionErrorEvent.h"
#include "SpeechRecognitionEvent.h"
#include "SpeechRecognitionResultData.h"
#include "SpeechRecognitionResultList.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SpeechRecognition);

Ref<SpeechRecognition> SpeechRecognition::create(Document& document)
{
    auto recognition = adoptRef(*new SpeechRecognition(document));
    recognition->suspendIfNeeded();
    return recognition;
}

SpeechRecognition::SpeechRecognition(Document& document)
    : ActiveDOMObject(document)
{
    if (auto* page = document.page()) {
        m_connection = &page->speechRecognitionConnection();
        m_connection->registerClient(*this);
    }
}

void SpeechRecognition::suspend(ReasonForSuspension)
{
    abortRecognition();
}

ExceptionOr<void> SpeechRecognition::startRecognition()
{
    if (m_state != State::Inactive)
        return Exception { ExceptionCode::InvalidStateError, "Recognition is being started or already started"_s };

    if (!m_connection)
        return Exception { ExceptionCode::UnknownError, "Recognition does not have a valid connection"_s };

    Ref document = downcast<Document>(*scriptExecutionContext());
    RefPtr frame = document->frame();
    if (!frame)
        return Exception { ExceptionCode::UnknownError, "Recognition is not in a valid frame"_s };

    auto optionalFrameIdentifier = document->frameID();
    if (!isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type::Microphone, document.get(), LogFeaturePolicyFailure::No)) {
        didError({ SpeechRecognitionErrorType::NotAllowed, "Permission is denied"_s });
        return { };
    }

    auto frameIdentifier = optionalFrameIdentifier ? *optionalFrameIdentifier : FrameIdentifier { };
    m_connection->start(identifier(), m_lang, m_continuous, m_interimResults, m_maxAlternatives, ClientOrigin { document->topOrigin().data(), document->securityOrigin().data() }, frameIdentifier);
    m_state = State::Starting;
    return { };
}

void SpeechRecognition::stopRecognition()
{
    if (m_state == State::Inactive || m_state == State::Stopping || m_state == State::Aborting)
        return;

    m_connection->stop(identifier());
    m_state = State::Stopping;
}

void SpeechRecognition::abortRecognition()
{
    if (m_state == State::Inactive || m_state == State::Aborting)
        return;

    m_connection->abort(identifier());
    m_state = State::Aborting;
}

const char* SpeechRecognition::activeDOMObjectName() const
{
    return "SpeechRecognition";
}

void SpeechRecognition::stop()
{
    abortRecognition();

    if (!m_connection)
        return;
    m_connection->unregisterClient(*this);

    auto& document = downcast<Document>(*scriptExecutionContext());
    document.setActiveSpeechRecognition(nullptr);
}

void SpeechRecognition::didStart()
{
    if (m_state == State::Starting)
        m_state = State::Running;

    queueTaskToDispatchEvent(*this, TaskSource::Speech, Event::create(eventNames().startEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void SpeechRecognition::didStartCapturingAudio()
{
    auto& document = downcast<Document>(*scriptExecutionContext());
    document.setActiveSpeechRecognition(this);

    queueTaskToDispatchEvent(*this, TaskSource::Speech, Event::create(eventNames().audiostartEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void SpeechRecognition::didStartCapturingSound()
{
    queueTaskToDispatchEvent(*this, TaskSource::Speech, Event::create(eventNames().soundstartEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void SpeechRecognition::didStartCapturingSpeech()
{
    queueTaskToDispatchEvent(*this, TaskSource::Speech, Event::create(eventNames().speechstartEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void SpeechRecognition::didStopCapturingSpeech()
{
    queueTaskToDispatchEvent(*this, TaskSource::Speech, Event::create(eventNames().speechendEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void SpeechRecognition::didStopCapturingSound()
{
    queueTaskToDispatchEvent(*this, TaskSource::Speech, Event::create(eventNames().soundendEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void SpeechRecognition::didStopCapturingAudio()
{
    auto& document = downcast<Document>(*scriptExecutionContext());
    document.setActiveSpeechRecognition(nullptr);

    queueTaskToDispatchEvent(*this, TaskSource::Speech, Event::create(eventNames().audioendEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void SpeechRecognition::didFindNoMatch()
{
    queueTaskToDispatchEvent(*this, TaskSource::Speech, SpeechRecognitionEvent::create(eventNames().nomatchEvent, 0, nullptr));
}

void SpeechRecognition::didReceiveResult(Vector<SpeechRecognitionResultData>&& resultDatas)
{
    Vector<Ref<SpeechRecognitionResult>> allResults;
    allResults.reserveInitialCapacity(m_finalResults.size() + resultDatas.size());
    allResults.appendVector(m_finalResults);

    auto firstChangedIndex = allResults.size();
    for (auto resultData : resultDatas) {
        auto alternatives = WTF::map(resultData.alternatives, [](auto& alternativeData) {
            return SpeechRecognitionAlternative::create(WTFMove(alternativeData.transcript), alternativeData.confidence);
        });

        auto newResult = SpeechRecognitionResult::create(WTFMove(alternatives), resultData.isFinal);
        if (resultData.isFinal)
            m_finalResults.append(newResult);

        allResults.append(WTFMove(newResult));
    }

    auto resultList = SpeechRecognitionResultList::create(WTFMove(allResults));
    queueTaskToDispatchEvent(*this, TaskSource::Speech, SpeechRecognitionEvent::create(eventNames().resultEvent, firstChangedIndex, WTFMove(resultList)));
}

void SpeechRecognition::didError(const SpeechRecognitionError& error)
{
    m_finalResults.clear();
    m_state = State::Inactive;

    queueTaskToDispatchEvent(*this, TaskSource::Speech, SpeechRecognitionErrorEvent::create(eventNames().errorEvent, error.type, error.message));
}

void SpeechRecognition::didEnd()
{
    m_finalResults.clear();
    m_state = State::Inactive;

    queueTaskToDispatchEvent(*this, TaskSource::Speech, Event::create(eventNames().endEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

SpeechRecognition::~SpeechRecognition()
{
}

} // namespace WebCore
