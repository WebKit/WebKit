/*
 * Copyright (C) 2013-2014 Apple Inc. All rights reserved.
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
#include <map>
#include <runtime/ArrayBuffer.h>
#include <wtf/PrintStream.h>

namespace WebCore {

class MockMediaSample final : public MediaSample {
public:
    static RefPtr<MockMediaSample> create(const MockSampleBox& box) { return adoptRef(new MockMediaSample(box)); }
    virtual ~MockMediaSample() { }

private:
    MockMediaSample(const MockSampleBox& box)
        : m_box(box)
        , m_id(String::format("%d", box.trackID()))
    {
    }

    virtual MediaTime presentationTime() const override { return m_box.presentationTimestamp(); }
    virtual MediaTime decodeTime() const override { return m_box.decodeTimestamp(); }
    virtual MediaTime duration() const override { return m_box.duration(); }
    virtual AtomicString trackID() const override { return m_id; }
    virtual size_t sizeInBytes() const override { return sizeof(m_box); }
    virtual SampleFlags flags() const override;
    virtual PlatformSample platformSample() override;
    virtual FloatSize presentationSize() const override { return FloatSize(); }
    virtual void dump(PrintStream&) const override;

    unsigned generation() const { return m_box.generation(); }

    MockSampleBox m_box;
    AtomicString m_id;
};

MediaSample::SampleFlags MockMediaSample::flags() const
{
    unsigned flags = None;
    if (m_box.flags() & MockSampleBox::IsSync)
        flags |= IsSync;
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

class MockMediaDescription final : public MediaDescription {
public:
    static RefPtr<MockMediaDescription> create(const MockTrackBox& box) { return adoptRef(new MockMediaDescription(box)); }
    virtual ~MockMediaDescription() { }

    virtual AtomicString codec() const override { return m_box.codec(); }
    virtual bool isVideo() const override { return m_box.kind() == MockTrackBox::Video; }
    virtual bool isAudio() const override { return m_box.kind() == MockTrackBox::Audio; }
    virtual bool isText() const override { return m_box.kind() == MockTrackBox::Text; }

protected:
    MockMediaDescription(const MockTrackBox& box) : m_box(box) { }
    MockTrackBox m_box;
};

RefPtr<MockSourceBufferPrivate> MockSourceBufferPrivate::create(MockMediaSourcePrivate* parent)
{
    return adoptRef(new MockSourceBufferPrivate(parent));
}

MockSourceBufferPrivate::MockSourceBufferPrivate(MockMediaSourcePrivate* parent)
    : m_mediaSource(parent)
    , m_client(0)
{
}

MockSourceBufferPrivate::~MockSourceBufferPrivate()
{
}

void MockSourceBufferPrivate::setClient(SourceBufferPrivateClient* client)
{
    m_client = client;
}

void MockSourceBufferPrivate::append(const unsigned char* data, unsigned length)
{
    m_inputBuffer.append(data, length);
    SourceBufferPrivateClient::AppendResult result = SourceBufferPrivateClient::AppendSucceeded;

    while (m_inputBuffer.size() && result == SourceBufferPrivateClient::AppendSucceeded) {
        RefPtr<ArrayBuffer> buffer = ArrayBuffer::create(m_inputBuffer.data(), m_inputBuffer.size());
        size_t boxLength = MockBox::peekLength(buffer.get());
        if (boxLength > buffer->byteLength())
            break;

        String type = MockBox::peekType(buffer.get());
        if (type == MockInitializationBox::type()) {
            MockInitializationBox initBox = MockInitializationBox(buffer.get());
            didReceiveInitializationSegment(initBox);
        } else if (type == MockSampleBox::type()) {
            MockSampleBox sampleBox = MockSampleBox(buffer.get());
            didReceiveSample(sampleBox);
        } else
            result = SourceBufferPrivateClient::ParsingFailed;

        m_inputBuffer.remove(0, boxLength);
    }

    if (m_client)
        m_client->sourceBufferPrivateAppendComplete(this, result);
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

    m_client->sourceBufferPrivateDidReceiveInitializationSegment(this, segment);
}


void MockSourceBufferPrivate::didReceiveSample(const MockSampleBox& sampleBox)
{
    if (!m_client)
        return;

    m_client->sourceBufferPrivateDidReceiveSample(this, MockMediaSample::create(sampleBox));
}

void MockSourceBufferPrivate::abort()
{
}

void MockSourceBufferPrivate::removedFromMediaSource()
{
    if (m_mediaSource)
        m_mediaSource->removeSourceBuffer(this);
}

MediaPlayer::ReadyState MockSourceBufferPrivate::readyState() const
{
    return m_mediaSource ? m_mediaSource->player()->readyState() : MediaPlayer::HaveNothing;
}

void MockSourceBufferPrivate::setReadyState(MediaPlayer::ReadyState readyState)
{
    if (m_mediaSource)
        m_mediaSource->player()->setReadyState(readyState);
}

void MockSourceBufferPrivate::setActive(bool isActive)
{
    if (m_mediaSource)
        m_mediaSource->sourceBufferPrivateDidChangeActiveState(this, isActive);
}

void MockSourceBufferPrivate::enqueueSample(PassRefPtr<MediaSample> sample, AtomicString)
{
    if (!m_mediaSource || !sample)
        return;

    PlatformSample platformSample = sample->platformSample();
    if (platformSample.type != PlatformSample::MockSampleBoxType)
        return;

    MockSampleBox* box = platformSample.sample.mockSampleBox;
    if (!box)
        return;

    m_mediaSource->incrementTotalVideoFrames();
    if (box->isCorrupted())
        m_mediaSource->incrementCorruptedFrames();
    if (box->isDropped())
        m_mediaSource->incrementDroppedFrames();
    if (box->isDelayed())
        m_mediaSource->incrementTotalFrameDelayBy(1);
}

bool MockSourceBufferPrivate::hasVideo() const
{
    if (!m_client)
        return false;

    return m_client->sourceBufferPrivateHasVideo(this);
}

bool MockSourceBufferPrivate::hasAudio() const
{
    if (!m_client)
        return false;

    return m_client->sourceBufferPrivateHasAudio(this);
}


MediaTime MockSourceBufferPrivate::fastSeekTimeForMediaTime(const MediaTime& time, const MediaTime& negativeThreshold, const MediaTime& positiveThreshold)
{
    if (m_client)
        return m_client->sourceBufferPrivateFastSeekTimeForMediaTime(this, time, negativeThreshold, positiveThreshold);
    return time;
}

void MockSourceBufferPrivate::seekToTime(const MediaTime& time)
{
    if (m_client)
        m_client->sourceBufferPrivateSeekToTime(this, time);
}

}

#endif

