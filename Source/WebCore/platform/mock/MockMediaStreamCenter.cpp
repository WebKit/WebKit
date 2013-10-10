/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Computer, Inc.  All rights reserved.
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
#include "MockMediaStreamCenter.h"

#if ENABLE(MEDIA_STREAM)

#include "MediaStream.h"
#include "MediaStreamCreationClient.h"
#include "MediaStreamDescriptor.h"
#include "MediaStreamSource.h"
#include "MediaStreamSourceCapabilities.h"
#include "MediaStreamTrack.h"
#include "MediaStreamTrackSourcesRequestClient.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

class MockSource : public MediaStreamSource {
public:
    MockSource(const AtomicString& id, const AtomicString& name, MediaStreamSource::Type type)
        : MediaStreamSource(id, type, name)
    {
    }

    virtual ~MockSource() { }

    virtual RefPtr<MediaStreamSourceCapabilities> capabilities() const { return m_capabilities; }
    virtual const MediaStreamSourceStates& states() { return m_currentStates; }

    RefPtr<MediaStreamSourceCapabilities> m_capabilities;
    MediaStreamSourceStates m_currentStates;
};
    
typedef HashMap<String, RefPtr<MockSource>> MockSourceMap;

static MockSourceMap& mockSourceMap()
{
    DEFINE_STATIC_LOCAL(MockSourceMap, mockSourceMap, ());
    return mockSourceMap;
}

static const AtomicString& mockAudioSourceID()
{
    static NeverDestroyed<AtomicString> id("239c24b1-2b15-11e3-8224-0800200c9a66", AtomicString::ConstructFromLiteral);
    return id;
}

static const AtomicString& mockVideoSourceID()
{
    static NeverDestroyed<AtomicString> id("239c24b0-2b15-11e3-8224-0800200c9a66", AtomicString::ConstructFromLiteral);
    return id;
}

static void initializeMockSources()
{
    RefPtr<MockSource> mockSource1 = adoptRef(new MockSource(mockVideoSourceID(), "Mock video device", MediaStreamSource::Video));
    mockSource1->m_capabilities = MediaStreamSourceCapabilities::create();
    mockSource1->m_capabilities->setSourceId(mockSource1->id());
    mockSource1->m_capabilities->addSourceType(MediaStreamSourceStates::Camera);
    mockSource1->m_capabilities->addSourceType(MediaStreamSourceStates::Microphone);
    mockSource1->m_capabilities->addFacingMode(MediaStreamSourceStates::User);
    mockSource1->m_capabilities->addFacingMode(MediaStreamSourceStates::Environment);
    mockSource1->m_capabilities->setWidthRange(MediaStreamSourceCapabilityRange(320UL, 1920UL, true));
    mockSource1->m_capabilities->setHeightRange(MediaStreamSourceCapabilityRange(240UL, 1080UL, true));
    mockSource1->m_capabilities->setFrameRateRange(MediaStreamSourceCapabilityRange(15.0f, 60.0f, true));
    mockSource1->m_capabilities->setAspectRatioRange(MediaStreamSourceCapabilityRange(4 / 3.0f, 16 / 9.0f, true));
    mockSource1->m_capabilities->setVolumeRange(MediaStreamSourceCapabilityRange(10UL, 90UL, true));

    mockSource1->m_currentStates.sourceType = MediaStreamSourceStates::Camera;
    mockSource1->m_currentStates.sourceId = mockSource1->id();
    mockSource1->m_currentStates.facingMode = MediaStreamSourceStates::User;
    mockSource1->m_currentStates.width = 1920;
    mockSource1->m_currentStates.height = 1080;
    mockSource1->m_currentStates.frameRate = 30;
    mockSource1->m_currentStates.aspectRatio = 16 / 9.0f;
    mockSource1->m_currentStates.volume = 70;
    String mockSource1id = mockSource1->id();
    mockSourceMap().add(mockSource1id, mockSource1.release());

    RefPtr<MockSource> mockSource2 = adoptRef(new MockSource(mockAudioSourceID(), "Mock audio device", MediaStreamSource::Audio));
    mockSource2->m_capabilities = MediaStreamSourceCapabilities::create();
    mockSource2->m_capabilities->setSourceId(mockSource2->id());
    mockSource2->m_capabilities->setVolumeRange(MediaStreamSourceCapabilityRange(0UL, 100UL, true));

    mockSource2->m_currentStates.sourceType = MediaStreamSourceStates::Microphone;
    mockSource2->m_currentStates.sourceId = mockSource2->id();
    mockSource2->m_currentStates.volume = 50;
    String mockSource2id = mockSource2->id();
    mockSourceMap().add(mockSource2id, mockSource2.release());
}

void MockMediaStreamCenter::registerMockMediaStreamCenter()
{
    DEFINE_STATIC_LOCAL(MockMediaStreamCenter, center, ());
    static bool registered = false;
    if (!registered) {
        registered = true;
        MediaStreamCenter::setSharedStreamCenter(&center);
        initializeMockSources();
    }
}

static bool isSupportedMockConstraint(const String& constraintName)
{
    return notFound != constraintName.find("_and_supported_");
}

static bool isValidMockConstraint(const String& constraintName)
{
    if (isSupportedMockConstraint(constraintName))
        return true;
    return notFound != constraintName.find("_but_unsupported_");
}

static String verifyConstraints(MediaConstraints* constraints)
{
    Vector<MediaConstraint> mandatoryConstraints;
    constraints->getMandatoryConstraints(mandatoryConstraints);
    if (mandatoryConstraints.size()) {
        for (size_t i = 0; i < mandatoryConstraints.size(); ++i) {
            const MediaConstraint& constraint = mandatoryConstraints[i];
            if (!isSupportedMockConstraint(constraint.m_name) || constraint.m_value != "1")
                return constraint.m_name;
        }
    }
    
    Vector<MediaConstraint> optionalConstraints;
    constraints->getOptionalConstraints(optionalConstraints);
    if (optionalConstraints.size()) {
        for (size_t i = 0; i < optionalConstraints.size(); ++i) {
            const MediaConstraint& constraint = optionalConstraints[i];
            if (!isValidMockConstraint(constraint.m_name) || constraint.m_value != "0")
                return constraint.m_name;
        }
    }
    
    return emptyString();
}

void MockMediaStreamCenter::validateRequestConstraints(PassRefPtr<MediaStreamCreationClient> prpQueryClient, PassRefPtr<MediaConstraints> audioConstraints, PassRefPtr<MediaConstraints> videoConstraints)
{
    RefPtr<MediaStreamCreationClient> client = prpQueryClient;
    
    ASSERT(client);
    
    if (audioConstraints) {
        String invalidQuery = verifyConstraints(audioConstraints.get());
        if (!invalidQuery.isEmpty()) {
            client->constraintsInvalid(invalidQuery);
            return;
        }
    }
    
    if (videoConstraints) {
        String invalidQuery = verifyConstraints(videoConstraints.get());
        if (!invalidQuery.isEmpty()) {
            client->constraintsInvalid(invalidQuery);
            return;
        }
    }

    client->constraintsValidated();
}

void MockMediaStreamCenter::createMediaStream(PassRefPtr<MediaStreamCreationClient> prpQueryClient, PassRefPtr<MediaConstraints> audioConstraints, PassRefPtr<MediaConstraints> videoConstraints)
{
    RefPtr<MediaStreamCreationClient> client = prpQueryClient;

    ASSERT(client);
    
    Vector<RefPtr<MediaStreamSource>> audioSources;
    Vector<RefPtr<MediaStreamSource>> videoSources;
    MockSourceMap& map = mockSourceMap();

    if (audioConstraints) {
        String invalidQuery = verifyConstraints(audioConstraints.get());
        if (!invalidQuery.isEmpty()) {
            client->failedToCreateStreamWithConstraintsError(invalidQuery);
            return;
        }

        if (audioConstraints) {
            MockSourceMap::iterator it = map.find(mockAudioSourceID());
            ASSERT(it != map.end());

            RefPtr<MediaStreamSource> audioSource = it->value;
            audioSource->reset();
            audioSource->setReadyState(MediaStreamSource::Live);
            audioSources.append(audioSource.release());
        }
    }

    if (videoConstraints) {
        String invalidQuery = verifyConstraints(videoConstraints.get());
        if (!invalidQuery.isEmpty()) {
            client->failedToCreateStreamWithConstraintsError(invalidQuery);
            return;
        }

        if (videoConstraints) {
            MockSourceMap::iterator it = map.find(mockVideoSourceID());
            ASSERT(it != map.end());

            RefPtr<MediaStreamSource> videoSource = it->value;
            videoSource->reset();
            videoSource->setReadyState(MediaStreamSource::Live);
            videoSources.append(videoSource.release());
        }
    }
    
    client->didCreateStream(MediaStreamDescriptor::create(audioSources, videoSources, MediaStreamDescriptor::IsNotEnded));
}

bool MockMediaStreamCenter::getMediaStreamTrackSources(PassRefPtr<MediaStreamTrackSourcesRequestClient> prpClient)
{
    RefPtr<MediaStreamTrackSourcesRequestClient> requestClient = prpClient;
    Vector<RefPtr<TrackSourceInfo>> sources;

    MockSourceMap& map = mockSourceMap();
    MockSourceMap::iterator end = map.end();
    for (MockSourceMap::iterator it = map.begin(); it != end; ++it) {
        MockSource* source = it->value.get();

        sources.append(TrackSourceInfo::create(source->id(), source->type() == MediaStreamSource::Video ? TrackSourceInfo::Video : TrackSourceInfo::Audio, source->name()));
    }

    requestClient->didCompleteRequest(sources);
    return true;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
