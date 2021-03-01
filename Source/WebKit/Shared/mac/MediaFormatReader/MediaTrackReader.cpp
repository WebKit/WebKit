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
#include "MediaTrackReader.h"

#if ENABLE(WEBM_FORMAT_READER)

#include "MediaFormatReader.h"
#include "MediaSampleByteRange.h"
#include "MediaSampleCursor.h"
#include <WebCore/AudioTrackPrivate.h>
#include <WebCore/InbandTextTrackPrivate.h>
#include <WebCore/Logging.h>
#include <WebCore/MediaDescription.h>
#include <WebCore/SampleMap.h>
#include <WebCore/VideoTrackPrivate.h>
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <wtf/LoggerHelper.h>

#include <pal/cocoa/MediaToolboxSoftLink.h>

WTF_DECLARE_CF_TYPE_TRAIT(MTPluginTrackReader);

namespace WebKit {

using namespace PAL;
using namespace WebCore;

CMBaseClassID MediaTrackReader::wrapperClassID()
{
    return MTPluginTrackReaderGetClassID();
}

CoreMediaWrapped<MediaTrackReader>* MediaTrackReader::unwrap(CMBaseObjectRef object)
{
    return unwrap(checked_cf_cast<WrapperRef>(object));
}

RefPtr<MediaTrackReader> MediaTrackReader::create(Allocator&& allocator, const MediaFormatReader& formatReader, CMMediaType mediaType, uint64_t trackID, Optional<bool> enabled)
{
    return adoptRef(new (allocator) MediaTrackReader(WTFMove(allocator), formatReader, mediaType, trackID, enabled));
}

WorkQueue& MediaTrackReader::storageQueue()
{
    static auto& queue = WorkQueue::create("WebKit::MediaTrackReader Storage Queue", WorkQueue::Type::Serial).leakRef();
    return queue;
}

MediaTrackReader::MediaTrackReader(Allocator&& allocator, const MediaFormatReader& formatReader, CMMediaType mediaType, uint64_t trackID, Optional<bool> enabled)
    : CoreMediaWrapped(WTFMove(allocator))
    , m_trackID(trackID)
    , m_mediaType(mediaType)
    , m_duration(formatReader.duration())
    , m_logger(formatReader.logger())
    , m_logIdentifier(formatReader.nextTrackReaderLogIdentifier(trackID))
{
    ASSERT(!isMainThread());

    ALWAYS_LOG(LOGIDENTIFIER, mediaTypeString(), " ", trackID);
    if (enabled)
        m_isEnabled = enabled.value() ? Enabled::True : Enabled::False;
}

void MediaTrackReader::addSample(Ref<MediaSample>&& sample, MTPluginByteSourceRef byteSource)
{
    ASSERT(!isMainThread());
    auto locker = holdLock(m_sampleStorageLock);
    if (!m_sampleStorage)
        m_sampleStorage = makeUnique<SampleStorage>();

    ASSERT(!sample->isDivisable() && sample->byteRange());
    auto sampleToAdd = MediaSampleByteRange::create(sample.get(), byteSource, m_trackID);

    // FIXME: Even though WebM muxer guidelines say this must not happen, some video tracks have two
    // consecutive frames with the same presentation time. SampleMap will not store the second frame
    // in these cases, corrupting all subsequent non-key frames. Find a way to store frames with
    // duplicate presentation times.

    m_sampleStorage->sampleMap.addSample(sampleToAdd.get());
    m_sampleStorageCondition.notifyAll();
}

void MediaTrackReader::waitForSample(Function<bool(SampleMap&, bool)>&& predicate) const
{
    auto locker = holdLock(m_sampleStorageLock);
    if (!m_sampleStorage)
        m_sampleStorage = makeUnique<SampleStorage>();
    m_sampleStorageCondition.wait(m_sampleStorageLock, [predicate = WTFMove(predicate), this] {
        return predicate(m_sampleStorage->sampleMap, m_sampleStorage->hasAllSamples);
    });
}

void MediaTrackReader::finishParsing()
{
    ASSERT(!isMainThread());

    ALWAYS_LOG(LOGIDENTIFIER);
    auto locker = holdLock(m_sampleStorageLock);
    if (!m_sampleStorage)
        m_sampleStorage = makeUnique<SampleStorage>();
    m_sampleStorage->hasAllSamples = true;
    if (m_isEnabled == Enabled::Unknown) {
        m_isEnabled = m_sampleStorage->sampleMap.empty() ? Enabled::False : Enabled::True;
        if (m_isEnabled == Enabled::False)
            ERROR_LOG(LOGIDENTIFIER, "ignoring empty ", mediaTypeString(), " track");
    }
    m_sampleStorageCondition.notifyAll();
}

const char* MediaTrackReader::mediaTypeString() const
{
    switch (m_mediaType) {
    case kCMMediaType_Video:
        return "video";
    case kCMMediaType_Audio:
        return "audio";
    case kCMMediaType_Text:
        return "text";
    }
    ASSERT_NOT_REACHED();
    return "unknown";
}

OSStatus MediaTrackReader::copyProperty(CFStringRef key, CFAllocatorRef allocator, void* copiedValue)
{
    // Don't block waiting for media if the we know the enabled state.
    if (CFEqual(key, PAL::get_MediaToolbox_kMTPluginTrackReaderProperty_Enabled()) && m_isEnabled != Enabled::Unknown) {
        *reinterpret_cast<CFBooleanRef*>(copiedValue) = retainPtr(m_isEnabled == Enabled::True ? kCFBooleanTrue : kCFBooleanFalse).leakRef();
        return noErr;
    }

    auto locker = holdLock(m_sampleStorageLock);
    m_sampleStorageCondition.wait(m_sampleStorageLock, [&] {
        return !!m_sampleStorage;
    });

    auto& sampleMap = m_sampleStorage->sampleMap;

    if (CFEqual(key, PAL::get_MediaToolbox_kMTPluginTrackReaderProperty_Enabled())) {
        if (m_isEnabled == Enabled::Unknown) {
            m_isEnabled = sampleMap.empty() ? Enabled::False : Enabled::True;
            if (m_isEnabled == Enabled::False)
                ERROR_LOG(LOGIDENTIFIER, "ignoring empty ", mediaTypeString(), " track");
        }

        *reinterpret_cast<CFBooleanRef*>(copiedValue) = retainPtr(m_isEnabled == Enabled::True ? kCFBooleanTrue : kCFBooleanFalse).leakRef();
        return noErr;
    }

    if (sampleMap.empty()) {
        ERROR_LOG(LOGIDENTIFIER, "sample table empty when asked for ", String(key));
        return kCMBaseObjectError_ValueNotAvailable;
    }

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

    ERROR_LOG(LOGIDENTIFIER, "asked for unsupported property ", String(key));
    return kCMBaseObjectError_ValueNotAvailable;
}

void MediaTrackReader::finalize()
{
    auto locker = holdLock(m_sampleStorageLock);
    storageQueue().dispatch([sampleStorage = std::exchange(m_sampleStorage, nullptr)]() mutable {
        sampleStorage = nullptr;
    });
    CoreMediaWrapped<MediaTrackReader>::finalize();
}

OSStatus MediaTrackReader::getTrackInfo(MTPersistentTrackID* trackID, CMMediaType* mediaType)
{
    *trackID = m_trackID;
    *mediaType = m_mediaType;
    return noErr;
}

OSStatus MediaTrackReader::createCursorAtPresentationTimeStamp(CMTime time, MTPluginSampleCursorRef* sampleCursor)
{
    *sampleCursor = MediaSampleCursor::createAtPresentationTime(allocator(), *this, PAL::toMediaTime(time)).leakRef()->wrapper();
    return noErr;
}

OSStatus MediaTrackReader::createCursorAtFirstSampleInDecodeOrder(MTPluginSampleCursorRef* sampleCursor)
{
    *sampleCursor = MediaSampleCursor::createAtPresentationTime(allocator(), *this, MediaTime::negativeInfiniteTime()).leakRef()->wrapper();
    return noErr;
}

OSStatus MediaTrackReader::createCursorAtLastSampleInDecodeOrder(MTPluginSampleCursorRef* sampleCursor)
{
    *sampleCursor = MediaSampleCursor::createAtPresentationTime(allocator(), *this, MediaTime::positiveInfiniteTime()).leakRef()->wrapper();
    return noErr;
}

WTFLogChannel& MediaTrackReader::logChannel() const
{
    return WebCore::LogMedia;
}

const void* MediaTrackReader::nextSampleCursorLogIdentifier(uint64_t cursorID) const
{
    uint64_t trackID = reinterpret_cast<uint64_t>(m_logIdentifier) & 0xffffull;
    uint64_t trackAndCursorID = trackID << 8 | (cursorID & 0xffull);
    return LoggerHelper::childLogIdentifier(m_logIdentifier, trackAndCursorID);
}

} // namespace WebKit

#endif // ENABLE(WEBM_FORMAT_READER)
