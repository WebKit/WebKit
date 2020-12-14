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
#include "TrackReader.h"

#if HAVE(MT_PLUGIN_FORMAT_READER)

#include "FormatReader.h"
#include "SampleCursor.h"
#include <WebCore/AudioTrackPrivate.h>
#include <WebCore/InbandTextTrackPrivate.h>
#include <WebCore/MediaDescription.h>
#include <WebCore/MediaSample.h>
#include <WebCore/MediaSampleAVFObjC.h>
#include <WebCore/SampleMap.h>
#include <WebCore/VideoTrackPrivate.h>
#include <pal/avfoundation/MediaTimeAVFoundation.h>

#include <pal/cocoa/MediaToolboxSoftLink.h>

WTF_DECLARE_CF_TYPE_TRAIT(MTPluginTrackReader);

namespace WebKit {

using namespace PAL;
using namespace WebCore;

class MediaSampleByteRange final : public MediaSampleAVFObjC {
public:
    static Ref<MediaSampleByteRange> create(MediaSample& sample, MTPluginByteSourceRef byteSource)
    {
        return adoptRef(*new MediaSampleByteRange(sample, byteSource));
    }

    MediaTime presentationTime() const final { return m_presentationTime; }
    MediaTime decodeTime() const final { return m_decodeTime; }
    MediaTime duration() const final { return m_duration; }
    size_t sizeInBytes() const final { return m_sizeInBytes; }
    FloatSize presentationSize() const final { return m_presentationSize; }
    SampleFlags flags() const final { return m_flags; }
    Optional<ByteRange> byteRange() const final { return m_byteRange; }

    PlatformSample platformSample() final;
    void offsetTimestampsBy(const MediaTime&) final;
    void setTimestamps(const MediaTime&, const MediaTime&) final;

private:
    MediaSampleByteRange(MediaSample&, MTPluginByteSourceRef);

    MediaTime m_presentationTime;
    MediaTime m_decodeTime;
    MediaTime m_duration;
    size_t m_sizeInBytes;
    FloatSize m_presentationSize;
    SampleFlags m_flags;
    Optional<ByteRange> m_byteRange;
    RetainPtr<MTPluginByteSourceRef> m_byteSource;
    RetainPtr<CMFormatDescriptionRef> m_formatDescription;
};

MediaSampleByteRange::MediaSampleByteRange(MediaSample& sample, MTPluginByteSourceRef byteSource)
    : MediaSampleAVFObjC(nullptr, sample.trackID())
    , m_presentationTime(sample.presentationTime())
    , m_decodeTime(sample.decodeTime())
    , m_duration(sample.duration())
    , m_sizeInBytes(sample.sizeInBytes())
    , m_presentationSize(sample.presentationSize())
    , m_flags(sample.flags())
    , m_byteRange(sample.byteRange())
    , m_byteSource(byteSource)
{
    auto platformSample = sample.platformSample();
    switch (platformSample.type) {
    case PlatformSample::CMSampleBufferType:
        m_formatDescription = CMSampleBufferGetFormatDescription(platformSample.sample.cmSampleBuffer);
        break;
    case PlatformSample::ByteRangeSampleType:
        m_formatDescription = platformSample.sample.byteRangeSample.second;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

PlatformSample MediaSampleByteRange::platformSample()
{
    return {
        PlatformSample::ByteRangeSampleType,
        { .byteRangeSample = std::make_pair(m_byteSource.get(), m_formatDescription.get()) },
    };
}

void MediaSampleByteRange::offsetTimestampsBy(const MediaTime& offset)
{
    setTimestamps(presentationTime() + offset, decodeTime() + offset);
}

void MediaSampleByteRange::setTimestamps(const MediaTime& presentationTime, const MediaTime& decodeTime)
{
    m_presentationTime = presentationTime;
    m_decodeTime = decodeTime;
}

CMBaseClassID TrackReader::wrapperClassID()
{
    return MTPluginTrackReaderGetClassID();
}

CoreMediaWrapped<TrackReader>* TrackReader::unwrap(CMBaseObjectRef object)
{
    return unwrap(checked_cf_cast<WrapperRef>(object));
}

RefPtr<TrackReader> TrackReader::create(Allocator&& allocator, const FormatReader& formatReader, const VideoTrackPrivate* track)
{
    if (!track)
        return nullptr;
    return adoptRef(new (allocator) TrackReader(WTFMove(allocator), formatReader, *track, kCMMediaType_Video));
}

RefPtr<TrackReader> TrackReader::create(Allocator&& allocator, const FormatReader& formatReader, const AudioTrackPrivate* track)
{
    if (!track)
        return nullptr;
    return adoptRef(new (allocator) TrackReader(WTFMove(allocator), formatReader, *track, kCMMediaType_Audio));
}

RefPtr<TrackReader> TrackReader::create(Allocator&& allocator, const FormatReader& formatReader, const InbandTextTrackPrivate* track)
{
    if (!track)
        return nullptr;
    return adoptRef(new (allocator) TrackReader(WTFMove(allocator), formatReader, *track, kCMMediaType_Text));
}

TrackReader::TrackReader(Allocator&& allocator, const FormatReader& formatReader, const TrackPrivateBase& track, CMMediaType mediaType)
    : CoreMediaWrapped(WTFMove(allocator))
    , m_trackID(track.id().string().toUInt64())
    , m_mediaType(mediaType)
    , m_duration(formatReader.duration())
{
    ASSERT(isMainThread());
    ASSERT(m_trackID > 0);
}

void TrackReader::addSample(Ref<MediaSample>&& sample, MTPluginByteSourceRef byteSource)
{
    ASSERT(isMainThread());
    auto locker = holdLock(m_sampleStorageLock);
    if (!m_sampleStorage)
        m_sampleStorage = makeUnique<SampleStorage>();

    ASSERT(!sample->isDivisable() && sample->byteRange());
    auto sampleToAdd = MediaSampleByteRange::create(sample.get(), byteSource);

    // FIXME: Even though WebM muxer guidelines say this must not happen, some video tracks have two
    // consecutive frames with the same presentation time. SampleMap will not store the second frame
    // in these cases, corrupting all subsequent non-key frames. Find a way to store frames with
    // duplicate presentation times.

    m_sampleStorage->sampleMap.addSample(sampleToAdd.get());
    m_sampleStorageCondition.notifyAll();
}

void TrackReader::waitForSample(Function<bool(SampleMap&, bool)>&& predicate) const
{
    ASSERT(!isMainThread());
    auto locker = holdLock(m_sampleStorageLock);
    if (!m_sampleStorage)
        m_sampleStorage = makeUnique<SampleStorage>();
    m_sampleStorageCondition.wait(m_sampleStorageLock, [predicate = WTFMove(predicate), this] {
        return predicate(m_sampleStorage->sampleMap, m_sampleStorage->hasAllSamples);
    });
}

void TrackReader::finishParsing()
{
    ASSERT(isMainThread());
    auto locker = holdLock(m_sampleStorageLock);
    if (!m_sampleStorage)
        m_sampleStorage = makeUnique<SampleStorage>();
    m_sampleStorage->hasAllSamples = true;
    m_sampleStorageCondition.notifyAll();
}

OSStatus TrackReader::copyProperty(CFStringRef key, CFAllocatorRef allocator, void* copiedValue)
{
    if (CFEqual(key, PAL::get_MediaToolbox_kMTPluginTrackReaderProperty_Enabled())) {
        *reinterpret_cast<CFBooleanRef*>(copiedValue) = retainPtr(m_isEnabled ? kCFBooleanTrue : kCFBooleanFalse).leakRef();
        return noErr;
    }

    auto locker = holdLock(m_sampleStorageLock);
    m_sampleStorageCondition.wait(m_sampleStorageLock, [&] {
        return !!m_sampleStorage;
    });

    auto& sampleMap = m_sampleStorage->sampleMap;
    if (sampleMap.empty())
        return kCMBaseObjectError_ValueNotAvailable;

    auto& lastSample = *sampleMap.decodeOrder().rbegin()->second;

    if (CFEqual(key, PAL::get_MediaToolbox_kMTPluginTrackReaderProperty_FormatDescriptionArray())) {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(lastSample.platformSample().type == PlatformSample::ByteRangeSampleType);
        const void* descriptions[1] = { lastSample.platformSample().sample.byteRangeSample.second };
        *reinterpret_cast<CFArrayRef*>(copiedValue) = adoptCF(CFArrayCreate(allocator, descriptions, 1, &kCFTypeArrayCallBacks)).leakRef();
        return noErr;
    }

    if (CFEqual(key, PAL::get_MediaToolbox_kMTPluginTrackReaderProperty_NominalFrameRate())) {
        float frameRate = 1 / lastSample.duration().toFloat();
        *reinterpret_cast<CFNumberRef*>(copiedValue) = adoptCF(CFNumberCreate(allocator, kCFNumberFloat32Type, &frameRate)).leakRef();
        return noErr;
    }

    return kCMBaseObjectError_ValueNotAvailable;
}

void TrackReader::finalize()
{
    auto locker = holdLock(m_sampleStorageLock);
    callOnMainThread([sampleStorage = std::exchange(m_sampleStorage, nullptr)] { });
    CoreMediaWrapped<TrackReader>::finalize();
}

OSStatus TrackReader::getTrackInfo(MTPersistentTrackID* trackID, CMMediaType* mediaType)
{
    *trackID = m_trackID;
    *mediaType = m_mediaType;
    return noErr;
}

OSStatus TrackReader::createCursorAtPresentationTimeStamp(CMTime time, MTPluginSampleCursorRef* sampleCursor)
{
    *sampleCursor = SampleCursor::createAtPresentationTime(allocator(), *this, PAL::toMediaTime(time)).leakRef()->wrapper();
    return noErr;
}

OSStatus TrackReader::createCursorAtFirstSampleInDecodeOrder(MTPluginSampleCursorRef* sampleCursor)
{
    *sampleCursor = SampleCursor::createAtDecodedSample(allocator(), *this, SampleCursor::DecodedSample::First).leakRef()->wrapper();
    return noErr;
}

OSStatus TrackReader::createCursorAtLastSampleInDecodeOrder(MTPluginSampleCursorRef* sampleCursor)
{
    *sampleCursor = SampleCursor::createAtDecodedSample(allocator(), *this, SampleCursor::DecodedSample::Last).leakRef()->wrapper();
    return noErr;
}

} // namespace WebKit

#endif // HAVE(MT_PLUGIN_FORMAT_READER)
