/*
 * Copyright (C) 2011-2013 University of Washington. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorReplayAgent.h"

#if ENABLE(INSPECTOR) && ENABLE(WEB_REPLAY)

#include "DocumentLoader.h"
#include "Event.h"
#include "EventLoopInput.h"
#include "Frame.h"
#include "FunctorInputCursor.h"
#include "InspectorController.h"
#include "InspectorPageAgent.h"
#include "InspectorWebTypeBuilders.h"
#include "InstrumentingAgents.h"
#include "Logging.h"
#include "Page.h"
#include "ReplayController.h"
#include "ReplaySession.h"
#include "ReplaySessionSegment.h"
#include "SerializationMethods.h"
#include "WebReplayInputs.h" // For EncodingTraits<InputQueue>.
#include <inspector/InspectorValues.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

using namespace Inspector;

namespace WebCore {

static PassRefPtr<TypeBuilder::Replay::ReplayPosition> buildInspectorObjectForPosition(const ReplayPosition& position)
{
    RefPtr<TypeBuilder::Replay::ReplayPosition> positionObject = TypeBuilder::Replay::ReplayPosition::create()
        .setSegmentOffset(position.segmentOffset)
        .setInputOffset(position.inputOffset);

    return positionObject.release();
}

static PassRefPtr<TypeBuilder::Replay::ReplayInput> buildInspectorObjectForInput(const NondeterministicInputBase& input, size_t offset)
{
    EncodedValue encodedInput = EncodingTraits<NondeterministicInputBase>::encodeValue(input);
    RefPtr<TypeBuilder::Replay::ReplayInput> inputObject = TypeBuilder::Replay::ReplayInput::create()
        .setType(input.type())
        .setOffset(offset)
        .setData(encodedInput.asObject());

    if (input.queue() == InputQueue::EventLoopInput)
        inputObject->setTimestamp(static_cast<const EventLoopInputBase&>(input).timestamp());

    return inputObject.release();
}

static PassRefPtr<TypeBuilder::Replay::ReplaySession> buildInspectorObjectForSession(PassRefPtr<ReplaySession> prpSession)
{
    RefPtr<ReplaySession> session = prpSession;
    RefPtr<TypeBuilder::Array<SegmentIdentifier>> segments = TypeBuilder::Array<SegmentIdentifier>::create();

    for (auto it = session->begin(); it != session->end(); ++it)
        segments->addItem((*it)->identifier());

    RefPtr<TypeBuilder::Replay::ReplaySession> sessionObject = TypeBuilder::Replay::ReplaySession::create()
        .setId(session->identifier())
        .setTimestamp(session->timestamp())
        .setSegments(segments.release());

    return sessionObject.release();
}

class SerializeInputToJSONFunctor {
public:
    typedef PassRefPtr<TypeBuilder::Array<TypeBuilder::Replay::ReplayInput>> ReturnType;

    SerializeInputToJSONFunctor()
        : m_inputs(TypeBuilder::Array<TypeBuilder::Replay::ReplayInput>::create()) { }
    ~SerializeInputToJSONFunctor() { }

    void operator()(size_t index, const NondeterministicInputBase* input)
    {
        LOG(WebReplay, "%-25s Writing %5zu: %s\n", "[SerializeInput]", index, input->type().string().ascii().data());

        if (RefPtr<TypeBuilder::Replay::ReplayInput> serializedInput = buildInspectorObjectForInput(*input, index))
            m_inputs->addItem(serializedInput.release());
    }

    ReturnType returnValue() { return m_inputs.release(); }
private:
    RefPtr<TypeBuilder::Array<TypeBuilder::Replay::ReplayInput>> m_inputs;
};

static PassRefPtr<TypeBuilder::Replay::SessionSegment> buildInspectorObjectForSegment(PassRefPtr<ReplaySessionSegment> prpSegment)
{
    RefPtr<ReplaySessionSegment> segment = prpSegment;
    RefPtr<TypeBuilder::Array<TypeBuilder::Replay::ReplayInputQueue>> queuesObject = TypeBuilder::Array<TypeBuilder::Replay::ReplayInputQueue>::create();

    for (size_t i = 0; i < static_cast<size_t>(InputQueue::Count); i++) {
        SerializeInputToJSONFunctor collector;
        InputQueue queue = static_cast<InputQueue>(i);
        RefPtr<TypeBuilder::Array<TypeBuilder::Replay::ReplayInput>> queueInputs = segment->createFunctorCursor()->forEachInputInQueue(queue, collector);

        RefPtr<TypeBuilder::Replay::ReplayInputQueue> queueObject = TypeBuilder::Replay::ReplayInputQueue::create()
            .setType(EncodingTraits<InputQueue>::encodeValue(queue).convertTo<String>())
            .setInputs(queueInputs);

        queuesObject->addItem(queueObject.release());
    }

    RefPtr<TypeBuilder::Replay::SessionSegment> segmentObject = TypeBuilder::Replay::SessionSegment::create()
        .setId(segment->identifier())
        .setTimestamp(segment->timestamp())
        .setQueues(queuesObject.release());

    return segmentObject.release();
}

InspectorReplayAgent::InspectorReplayAgent(InstrumentingAgents* instrumentingAgents, InspectorPageAgent* pageAgent)
    : InspectorAgentBase(ASCIILiteral("Replay"), instrumentingAgents)
    , m_page(*pageAgent->page())
{
}

InspectorReplayAgent::~InspectorReplayAgent()
{
    ASSERT(!m_sessionsMap.size());
    ASSERT(!m_segmentsMap.size());
}

SessionState InspectorReplayAgent::sessionState() const
{
    return m_page.replayController().sessionState();
}

void InspectorReplayAgent::didCreateFrontendAndBackend(InspectorFrontendChannel* frontendChannel, InspectorBackendDispatcher* backendDispatcher)
{
    m_frontendDispatcher = std::make_unique<InspectorReplayFrontendDispatcher>(frontendChannel);
    m_backendDispatcher = InspectorReplayBackendDispatcher::create(backendDispatcher, this);

    m_instrumentingAgents->setInspectorReplayAgent(this);
    ASSERT(sessionState() == SessionState::Inactive);

    // Keep track of the (default) session currently loaded by ReplayController,
    // and any segments within the session.
    RefPtr<ReplaySession> session = m_page.replayController().loadedSession();
    m_sessionsMap.add(session->identifier(), session);

    for (auto it = session->begin(); it != session->end(); ++it)
        m_segmentsMap.add((*it)->identifier(), *it);
}

void InspectorReplayAgent::willDestroyFrontendAndBackend(InspectorDisconnectReason)
{
    m_frontendDispatcher = nullptr;
    m_backendDispatcher.clear();

    m_instrumentingAgents->setInspectorReplayAgent(nullptr);

    // Drop references to all sessions and segments.
    m_sessionsMap.clear();
    m_segmentsMap.clear();
}

void InspectorReplayAgent::frameNavigated(DocumentLoader* loader)
{
    if (sessionState() != SessionState::Inactive)
        m_page.replayController().frameNavigated(loader);
}

void InspectorReplayAgent::frameDetached(Frame* frame)
{
    if (sessionState() != SessionState::Inactive)
        m_page.replayController().frameDetached(frame);
}

void InspectorReplayAgent::sessionCreated(PassRefPtr<ReplaySession> prpSession)
{
    RefPtr<ReplaySession> session = prpSession;

    auto result = m_sessionsMap.add(session->identifier(), session);
    // Can't have two sessions with same identifier.
    ASSERT_UNUSED(result, result.isNewEntry);

    m_frontendDispatcher->sessionCreated(session->identifier());
}

void InspectorReplayAgent::sessionModified(PassRefPtr<ReplaySession> session)
{
    m_frontendDispatcher->sessionModified(session->identifier());
}

void InspectorReplayAgent::sessionLoaded(PassRefPtr<ReplaySession> prpSession)
{
    RefPtr<ReplaySession> session = prpSession;

    // In case we didn't know about the loaded session, add here.
    m_sessionsMap.add(session->identifier(), session);

    m_frontendDispatcher->sessionLoaded(session->identifier());
}

void InspectorReplayAgent::segmentCreated(PassRefPtr<ReplaySessionSegment> prpSegment)
{
    RefPtr<ReplaySessionSegment> segment = prpSegment;

    auto result = m_segmentsMap.add(segment->identifier(), segment);
    // Can't have two segments with the same identifier.
    ASSERT_UNUSED(result, result.isNewEntry);

    m_frontendDispatcher->segmentCreated(segment->identifier());
}

void InspectorReplayAgent::segmentCompleted(PassRefPtr<ReplaySessionSegment> segment)
{
    m_frontendDispatcher->segmentCompleted(segment->identifier());
}

void InspectorReplayAgent::segmentLoaded(PassRefPtr<ReplaySessionSegment> prpSegment)
{
    RefPtr<ReplaySessionSegment> segment = prpSegment;

    // In case we didn't know about the loaded segment, add here.
    m_segmentsMap.add(segment->identifier(), segment);

    m_frontendDispatcher->segmentLoaded(segment->identifier());
}

void InspectorReplayAgent::segmentUnloaded()
{
    m_frontendDispatcher->segmentUnloaded();
}

void InspectorReplayAgent::captureStarted()
{
    LOG(WebReplay, "-----CAPTURE START-----");

    m_frontendDispatcher->captureStarted();
}

void InspectorReplayAgent::captureStopped()
{
    LOG(WebReplay, "-----CAPTURE STOP-----");

    m_frontendDispatcher->captureStopped();
}

void InspectorReplayAgent::playbackStarted()
{
    LOG(WebReplay, "-----REPLAY START-----");

    m_frontendDispatcher->playbackStarted();
}

void InspectorReplayAgent::playbackPaused(const ReplayPosition& position)
{
    LOG(WebReplay, "-----REPLAY PAUSED-----");

    m_frontendDispatcher->playbackPaused(buildInspectorObjectForPosition(position));
}

void InspectorReplayAgent::playbackHitPosition(const ReplayPosition& position)
{
    m_frontendDispatcher->playbackHitPosition(buildInspectorObjectForPosition(position), monotonicallyIncreasingTime());
}

void InspectorReplayAgent::startCapturing(ErrorString* errorString)
{
    if (sessionState() != SessionState::Inactive) {
        *errorString = ASCIILiteral("Can't start capturing if the session is already capturing or replaying.");
        return;
    }

    m_page.replayController().startCapturing();
}

void InspectorReplayAgent::stopCapturing(ErrorString* errorString)
{
    if (sessionState() != SessionState::Capturing) {
        *errorString = ASCIILiteral("Can't stop capturing if capture is not in progress.");
        return;
    }

    m_page.replayController().stopCapturing();
}

void InspectorReplayAgent::replayToPosition(ErrorString* errorString, const RefPtr<InspectorObject>& positionObject, bool fastReplay)
{
    ReplayPosition position;
    if (!positionObject->getNumber(ASCIILiteral("segmentOffset"), &position.segmentOffset)) {
        *errorString = ASCIILiteral("Couldn't decode ReplayPosition segment offset provided to ReplayAgent.replayToPosition.");
        return;
    }

    if (!positionObject->getNumber(ASCIILiteral("inputOffset"), &position.inputOffset)) {
        *errorString = ASCIILiteral("Couldn't decode ReplayPosition input offset provided to ReplayAgent.replayToPosition.");
        return;
    }

    if (sessionState() != SessionState::Inactive) {
        *errorString = ASCIILiteral("Can't start replay while capture or playback is in progress.");
        return;
    }

    m_page.replayController().replayToPosition(position, (fastReplay) ? DispatchSpeed::FastForward : DispatchSpeed::RealTime);
}

void InspectorReplayAgent::replayToCompletion(ErrorString* errorString, bool fastReplay)
{
    if (sessionState() != SessionState::Inactive) {
        *errorString = ASCIILiteral("Can't start replay while capture or playback is in progress.");
        return;
    }

    m_page.replayController().replayToCompletion((fastReplay) ? DispatchSpeed::FastForward : DispatchSpeed::RealTime);
}

void InspectorReplayAgent::pausePlayback(ErrorString* errorString)
{
    if (sessionState() != SessionState::Replaying) {
        *errorString = ASCIILiteral("Can't pause playback if playback is not in progress.");
        return;
    }

    m_page.replayController().pausePlayback();
}

void InspectorReplayAgent::cancelPlayback(ErrorString* errorString)
{
    if (sessionState() == SessionState::Capturing) {
        *errorString = ASCIILiteral("Can't cancel playback if capture is in progress.");
        return;
    }

    m_page.replayController().cancelPlayback();
}

void InspectorReplayAgent::switchSession(ErrorString* errorString, SessionIdentifier identifier)
{
    ASSERT(identifier > 0);

    if (sessionState() != SessionState::Inactive) {
        *errorString = ASCIILiteral("Can't switch sessions unless the session is neither capturing or replaying.");
        return;
    }

    RefPtr<ReplaySession> session = findSession(errorString, identifier);
    if (!session)
        return;

    m_page.replayController().switchSession(session);
}

void InspectorReplayAgent::insertSessionSegment(ErrorString* errorString, SessionIdentifier sessionIdentifier, SegmentIdentifier segmentIdentifier, int segmentIndex)
{
    ASSERT(sessionIdentifier > 0);
    ASSERT(segmentIdentifier > 0);
    ASSERT(segmentIndex >= 0);

    RefPtr<ReplaySession> session = findSession(errorString, sessionIdentifier);
    RefPtr<ReplaySessionSegment> segment = findSegment(errorString, segmentIdentifier);

    if (!session || !segment)
        return;

    if (static_cast<size_t>(segmentIndex) > session->size()) {
        *errorString = ASCIILiteral("Invalid segment index.");
        return;
    }

    if (session == m_page.replayController().loadedSession() && sessionState() != SessionState::Inactive) {
        *errorString = ASCIILiteral("Can't modify a loaded session unless the session is inactive.");
        return;
    }

    session->insertSegment(segmentIndex, segment);
    sessionModified(session);
}

void InspectorReplayAgent::removeSessionSegment(ErrorString* errorString, SessionIdentifier identifier, int segmentIndex)
{
    ASSERT(identifier > 0);
    ASSERT(segmentIndex >= 0);

    RefPtr<ReplaySession> session = findSession(errorString, identifier);

    if (!session)
        return;

    if (static_cast<size_t>(segmentIndex) >= session->size()) {
        *errorString = ASCIILiteral("Invalid segment index.");
        return;
    }

    if (session == m_page.replayController().loadedSession() && sessionState() != SessionState::Inactive) {
        *errorString = ASCIILiteral("Can't modify a loaded session unless the session is inactive.");
        return;
    }

    session->removeSegment(segmentIndex);
    sessionModified(session);
}

PassRefPtr<ReplaySession> InspectorReplayAgent::findSession(ErrorString* errorString, SessionIdentifier identifier)
{
    ASSERT(identifier > 0);

    auto it = m_sessionsMap.find(identifier);
    if (it == m_sessionsMap.end()) {
        *errorString = ASCIILiteral("Couldn't find session with specified identifier");
        return nullptr;
    }

    return it->value;
}

PassRefPtr<ReplaySessionSegment> InspectorReplayAgent::findSegment(ErrorString* errorString, SegmentIdentifier identifier)
{
    ASSERT(identifier > 0);

    auto it = m_segmentsMap.find(identifier);
    if (it == m_segmentsMap.end()) {
        *errorString = ASCIILiteral("Couldn't find segment with specified identifier");
        return nullptr;
    }

    return it->value;
}

void InspectorReplayAgent::getAvailableSessions(ErrorString*, RefPtr<Inspector::TypeBuilder::Array<SessionIdentifier>>& sessionsList)
{
    sessionsList = TypeBuilder::Array<SessionIdentifier>::create();
    for (auto& pair : m_sessionsMap)
        sessionsList->addItem(pair.key);
}

void InspectorReplayAgent::getSerializedSession(ErrorString* errorString, SessionIdentifier identifier, RefPtr<Inspector::TypeBuilder::Replay::ReplaySession>& serializedObject)
{
    RefPtr<ReplaySession> session = findSession(errorString, identifier);
    if (!session) {
        *errorString = ASCIILiteral("Couldn't find the specified session.");
        return;
    }

    serializedObject = buildInspectorObjectForSession(session);
}

void InspectorReplayAgent::getSerializedSegment(ErrorString* errorString, SegmentIdentifier identifier, RefPtr<Inspector::TypeBuilder::Replay::SessionSegment>& serializedObject)
{
    RefPtr<ReplaySessionSegment> segment = findSegment(errorString, identifier);
    if (!segment) {
        *errorString = ASCIILiteral("Couldn't find the specified segment.");
        return;
    }

    serializedObject = buildInspectorObjectForSegment(segment);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR) && ENABLE(WEB_REPLAY)
