/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
#include "MockSourceBufferPrivate.h"

#if ENABLE(MEDIA_SOURCE)

#include "MediaDescription.h"
#include "MediaPlayer.h"
#include "MediaSample.h"
#include "MockBox.h"
#include "MockMediaPlayerMediaSource.h"
#include "MockMediaSourcePrivate.h"
#include "MockTracks.h"
#include "SourceBufferPrivateClient.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/StringPrintStream.h>

namespace WebCore {

class MockMediaSample final : public MediaSample {
public:
    static Ref<MockMediaSample> create(const MockSampleBox& box) { return adoptRef(*new MockMediaSample(box)); }
    virtual ~MockMediaSample() = default;

private:
    MockMediaSample(const MockSampleBox& box)
        : m_box(box)
        , m_id(AtomString::number(box.trackID()))
    {
    }

    MediaTime presentationTime() const override { return m_box.presentationTimestamp(); }
    MediaTime decodeTime() const override { return m_box.decodeTimestamp(); }
    MediaTime duration() const override { return m_box.duration(); }
    AtomString trackID() const override { return m_id; }
    void setTrackID(const String& id) override { m_id = id; }
    size_t sizeInBytes() const override { return sizeof(m_box); }
    SampleFlags flags() const override;
    PlatformSample platformSample() override;
    FloatSize presentationSize() const override { return FloatSize(); }
    void dump(PrintStream&) const override;
    void offsetTimestampsBy(const MediaTime& offset) override { m_box.offsetTimestampsBy(offset); }
    void setTimestamps(const MediaTime& presentationTimestamp, const MediaTime& decodeTimestamp) override { m_box.setTimestamps(presentationTimestamp, decodeTimestamp); }
    bool isDivisable() const override { return false; }
    std::pair<RefPtr<MediaSample>, RefPtr<MediaSample>> divide(const MediaTime&) override { return {nullptr, nullptr}; }
    Ref<MediaSample> createNonDisplayingCopy() const override;

    unsigned generation() const { return m_box.generation(); }

    MockSampleBox m_box;
    AtomString m_id;
};

MediaSample::SampleFlags MockMediaSample::flags() const
{
    unsigned flags = None;
    if (m_box.isSync())
        flags |= IsSync;
    if (m_box.isNonDisplaying())
        flags |= IsNonDisplaying;
    return SampleFlags(flags);
}

PlatformSample MockMediaSample::platformSample()
{
    PlatformSample sample = { PlatformSample::MockSampleBoxType, { &m_box } };
    return sample;
}

void MockMediaSample::dump(PrintStream& out) const
{
    out.print("{PTS(", presentationTime(), "), DTS(", decodeTime(), "), duration(", duration(), "), flags(", (int)flags(), "), generation(", generation(), ")}");
}

Ref<MediaSample> MockMediaSample::createNonDisplayingCopy() const
{
    auto copy = MockMediaSample::create(m_box);
    copy->m_box.setFlag(MockSampleBox::IsNonDisplaying);
    return copy;
}

class MockMediaDescription final : public MediaDescription {
public:
    static Ref<MockMediaDescription> create(const MockTrackBox& box) { return adoptRef(*new MockMediaDescription(box)); }
    virtual ~MockMediaDescription() = default;

    AtomString codec() const override { return m_box.codec(); }
    bool isVideo() const override { return m_box.kind() == MockTrackBox::Video; }
    bool isAudio() const override { return m_box.kind() == MockTrackBox::Audio; }
    bool isText() const override { return m_box.kind() == MockTrackBox::Text; }

protected:
    MockMediaDescription(const MockTrackBox& box) : m_box(box) { }
    MockTrackBox m_box;
};

Ref<MockSourceBufferPrivate> MockSourceBufferPrivate::create(MockMediaSourcePrivate* parent)
{
    return adoptRef(*new MockSourceBufferPrivate(parent));
}

MockSourceBufferPrivate::MockSourceBufferPrivate(MockMediaSourcePrivate* parent)
    : m_mediaSource(parent)
    , m_client(0)
{
}

MockSourceBufferPrivate::~MockSourceBufferPrivate() = default;

void MockSourceBufferPrivate::setClient(SourceBufferPrivateClient* client)
{
    m_client = client;
}

void MockSourceBufferPrivate::append(Vector<unsigned char>&& data)
{
    m_inputBuffer.appendVector(data);
    SourceBufferPrivateClient::AppendResult result = SourceBufferPrivateClient::AppendSucceeded;

    while (m_inputBuffer.size() && result == SourceBufferPrivateClient::AppendSucceeded) {
        auto buffer = ArrayBuffer::create(m_inputBuffer.data(), m_inputBuffer.size());
        size_t boxLength = MockBox::peekLength(buffer.ptr());
        if (boxLength > buffer->byteLength())
            break;

        String type = MockBox::peekType(buffer.ptr());
        if (type == MockInitializationBox::type()) {
            MockInitializationBox initBox = MockInitializationBox(buffer.ptr());
            didReceiveInitializationSegment(initBox);
        } else if (type == MockSampleBox::type()) {
            MockSampleBox sampleBox = MockSampleBox(buffer.ptr());
            didReceiveSample(sampleBox);
        } else
            result = SourceBufferPrivateClient::ParsingFailed;

        m_inputBuffer.remove(0, boxLength);
    }

    if (m_client)
        m_client->sourceBufferPrivateAppendComplete(result);
}

void MockSourceBufferPrivate::didReceiveInitializationSegment(const MockInitializationBox& initBox)
{
    if (!m_client)
        return;

    SourceBufferPrivateClient::InitializationSegment segment;
    segment.duration = initBox.duration();

    for (auto it = initBox.tracks().begin(); it != initBox.tracks().end(); ++it) {
        const MockTrackBox& trackBox = *it;
        if (trackBox.kind() == MockTrackBox::Video) {
            SourceBufferPrivateClient::InitializationSegment::VideoTrackInformation info;
            info.track = MockVideoTrackPrivate::create(trackBox);
            info.description = MockMediaDescription::create(trackBox);
            segment.videoTracks.append(info);
        } else if (trackBox.kind() == MockTrackBox::Audio) {
            SourceBufferPrivateClient::InitializationSegment::AudioTrackInformation info;
            info.track = MockAudioTrackPrivate::create(trackBox);
            info.description = MockMediaDescription::create(trackBox);
            segment.audioTracks.append(info);
        } else if (trackBox.kind() == MockTrackBox::Text) {
            SourceBufferPrivateClient::InitializationSegment::TextTrackInformation info;
            info.track = MockTextTrackPrivate::create(trackBox);
            info.description = MockMediaDescription::create(trackBox);
            segment.textTracks.append(info);
        }
    }

    m_client->sourceBufferPrivateDidReceiveInitializationSegment(segment);
}


void MockSourceBufferPrivate::didReceiveSample(const MockSampleBox& sampleBox)
{
    if (!m_client)
        return;

    m_client->sourceBufferPrivateDidReceiveSample(MockMediaSample::create(sampleBox));
}

void MockSourceBufferPrivate::abort()
{
}

void MockSourceBufferPrivate::resetParserState()
{
}

void MockSourceBufferPrivate::removedFromMediaSource()
{
    if (m_mediaSource)
        m_mediaSource->removeSourceBuffer(this);
}

MediaPlayer::ReadyState MockSourceBufferPrivate::readyState() const
{
    return m_mediaSource ? m_mediaSource->player().readyState() : MediaPlayer::HaveNothing;
}

void MockSourceBufferPrivate::setReadyState(MediaPlayer::ReadyState readyState)
{
    if (m_mediaSource)
        m_mediaSource->player().setReadyState(readyState);
}

void MockSourceBufferPrivate::setActive(bool isActive)
{
    if (m_mediaSource)
        m_mediaSource->sourceBufferPrivateDidChangeActiveState(this, isActive);
}

Vector<String> MockSourceBufferPrivate::enqueuedSamplesForTrackID(const AtomString&)
{
    return m_enqueuedSamples;
}

MediaTime MockSourceBufferPrivate::minimumUpcomingPresentationTimeForTrackID(const AtomString&)
{
    return m_minimumUpcomingPresentationTime;
}

void MockSourceBufferPrivate::setMaximumQueueDepthForTrackID(const AtomString&, size_t maxQueueDepth)
{
    m_maxQueueDepth = maxQueueDepth;
}

bool MockSourceBufferPrivate::canSetMinimumUpcomingPresentationTime(const AtomString&) const
{
    return true;
}

void MockSourceBufferPrivate::setMinimumUpcomingPresentationTime(const AtomString&, const MediaTime& presentationTime)
{
    m_minimumUpcomingPresentationTime = presentationTime;
}

void MockSourceBufferPrivate::clearMinimumUpcomingPresentationTime(const AtomString&)
{
    m_minimumUpcomingPresentationTime = MediaTime::invalidTime();
}

bool MockSourceBufferPrivate::canSwitchToType(const ContentType& contentType)
{
    MediaEngineSupportParameters parameters;
    parameters.isMediaSource = true;
    parameters.type = contentType;
    return MockMediaPlayerMediaSource::supportsType(parameters) != MediaPlayer::IsNotSupported;
}

void MockSourceBufferPrivate::enqueueSample(Ref<MediaSample>&& sample, const AtomString&)
{
    if (!m_mediaSource)
        return;

    PlatformSample platformSample = sample->platformSample();
    if (platformSample.type != PlatformSample::MockSampleBoxType)
        return;

    auto* box = platformSample.sample.mockSampleBox;
    if (!box)
        return;

    m_mediaSource->incrementTotalVideoFrames();
    if (box->isCorrupted())
        m_mediaSource->incrementCorruptedFrames();
    if (box->isDropped())
        m_mediaSource->incrementDroppedFrames();
    if (box->isDelayed())
        m_mediaSource->incrementTotalFrameDelayBy(MediaTime(1, 1));

    m_enqueuedSamples.append(toString(sample.get()));
}

bool MockSourceBufferPrivate::hasVideo() const
{
    return m_client && m_client->sourceBufferPrivateHasVideo();
}

bool MockSourceBufferPrivate::hasAudio() const
{
    return m_client && m_client->sourceBufferPrivateHasAudio();
}

MediaTime MockSourceBufferPrivate::fastSeekTimeForMediaTime(const MediaTime& time, const MediaTime& negativeThreshold, const MediaTime& positiveThreshold)
{
    if (m_client)
        return m_client->sourceBufferPrivateFastSeekTimeForMediaTime(time, negativeThreshold, positiveThreshold);
    return time;
}

#if !RELEASE_LOG_DISABLED
const Logger& MockSourceBufferPrivate::sourceBufferLogger() const
{
    return m_mediaSource->mediaSourceLogger();
}

const void* MockSourceBufferPrivate::sourceBufferLogIdentifier()
{
    return m_mediaSource->mediaSourceLogIdentifier();
}
#endif

}

#endif

