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
#include "MediaSampleCursor.h"

#if ENABLE(WEBM_FORMAT_READER)

#include "MediaTrackReader.h"
#include <WebCore/MediaSample.h>
#include <WebCore/SampleMap.h>
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <wtf/CompletionHandler.h>
#include <wtf/MediaTime.h>
#include <wtf/Variant.h>

#include <pal/cocoa/MediaToolboxSoftLink.h>

WTF_DECLARE_CF_TYPE_TRAIT(MTPluginSampleCursor);

namespace WebKit {

using namespace PAL;
using namespace WebCore;

static Optional<MediaSampleCursor::DecodeOrderIterator> makeIterator(const MediaSampleCursor::DecodedSample& decodedSample, SampleMap& samples, bool hasAllSamples)
{
    auto& decodeOrder = samples.decodeOrder();
    switch (decodedSample) {
    case MediaSampleCursor::DecodedSample::First:
        if (samples.size())
            return decodeOrder.begin();
        break;
    case MediaSampleCursor::DecodedSample::Last:
        if (samples.size() && hasAllSamples)
            return std::prev(decodeOrder.end());
        break;
    }
    return WTF::nullopt;
}

static Optional<MediaSampleCursor::DecodeOrderIterator> makeIterator(const MediaSampleCursor::PresentationOrderIterator& iterator, SampleMap& samples, bool)
{
    MediaSample& sample = *iterator->second.get();
    return samples.decodeOrder().findSampleWithDecodeKey(std::make_pair(sample.decodeTime(), sample.presentationTime()));
}

static Optional<MediaSampleCursor::PresentationOrderIterator> makeIterator(const MediaTime& presentationTime, SampleMap& samples, bool hasAllSamples)
{
    auto& presentationOrder = samples.presentationOrder();
    auto iterator = presentationOrder.findSampleContainingOrAfterPresentationTime(presentationTime);
    if (iterator != presentationOrder.end())
        return iterator;
    if (samples.size() && hasAllSamples)
        return std::prev(iterator);
    return WTF::nullopt;
}

static Optional<MediaSampleCursor::PresentationOrderIterator> makeIterator(const MediaSampleCursor::DecodeOrderIterator& iterator, SampleMap& samples, bool)
{
    MediaSample& sample = *iterator->second.get();
    return samples.presentationOrder().findSampleWithPresentationTime(sample.presentationTime());
}

static MediaTime makeTime(const DecodeOrderSampleMap::iterator& it)
{
    return it->second->decodeTime();
}

static MediaTime makeTime(const PresentationOrderSampleMap::iterator& it)
{
    return it->second->presentationTime();
}

template<typename OrderedMap> static OrderedMap& orderedSamples(SampleMap&);

template<>
DecodeOrderSampleMap& orderedSamples(SampleMap& samples)
{
    return samples.decodeOrder();
}

template<>
PresentationOrderSampleMap& orderedSamples(SampleMap& samples)
{
    return samples.presentationOrder();
}

static DecodeOrderSampleMap::iterator upperBound(DecodeOrderSampleMap& map, const MediaTime& time)
{
    return map.findSampleAfterDecodeKey(std::make_pair(time, MediaTime::positiveInfiniteTime()));
}

static PresentationOrderSampleMap::iterator upperBound(PresentationOrderSampleMap& map, const MediaTime& time)
{
    return map.findSampleStartingAfterPresentationTime(time);
}

template<typename OrderedMap, typename Step> static Step stepIterator(Step, typename OrderedMap::iterator&, OrderedMap&);

template<typename OrderedMap>
int64_t stepIterator(int64_t stepsRemaining, typename OrderedMap::iterator& iterator, OrderedMap& samples)
{
    ASSERT(iterator != samples.end());
    if (stepsRemaining < 0)
        for (; stepsRemaining && iterator != samples.begin(); ++stepsRemaining, --iterator) { }
    else
        for (auto next = std::next(iterator); stepsRemaining && next != samples.end(); --stepsRemaining, iterator = next++) { }
    return stepsRemaining;
}

template<typename OrderedMap>
MediaTime stepIterator(MediaTime deltaRemaining, typename OrderedMap::iterator& iterator, OrderedMap& samples)
{
    ASSERT(iterator != samples.end());
    auto requestedTime = makeTime(iterator) + deltaRemaining;
    iterator = upperBound(samples, requestedTime);
    if (iterator != samples.begin())
        --iterator;
    deltaRemaining = requestedTime - makeTime(iterator);
    if (deltaRemaining > MediaTime::zeroTime() && deltaRemaining < iterator->second->duration())
        return MediaTime::zeroTime();
    return deltaRemaining;
}

CMBaseClassID MediaSampleCursor::wrapperClassID()
{
    return MTPluginSampleCursorGetClassID();
}

CoreMediaWrapped<MediaSampleCursor>* MediaSampleCursor::unwrap(CMBaseObjectRef object)
{
    return unwrap(checked_cf_cast<WrapperRef>(object));
}

RefPtr<MediaSampleCursor> MediaSampleCursor::createAtPresentationTime(Allocator&& allocator, MediaTrackReader& trackReader, MediaTime time)
{
    return adoptRef(new (allocator) MediaSampleCursor(WTFMove(allocator), trackReader, WTFMove(time)));
}

RefPtr<MediaSampleCursor> MediaSampleCursor::createAtDecodedSample(Allocator&& allocator, MediaTrackReader& trackReader, DecodedSample decodedSample)
{
    return adoptRef(new (allocator) MediaSampleCursor(WTFMove(allocator), trackReader, WTFMove(decodedSample)));
}

RefPtr<MediaSampleCursor> MediaSampleCursor::copy(Allocator&& allocator, const MediaSampleCursor& cursor)
{
    auto locker = holdLock(cursor.m_locatorLock);
    return adoptRef(new (allocator) MediaSampleCursor(WTFMove(allocator), cursor));
}

MediaSampleCursor::MediaSampleCursor(Allocator&& allocator, MediaTrackReader& trackReader, Locator locator)
    : CoreMediaWrapped(WTFMove(allocator))
    , m_trackReader(trackReader)
    , m_locator(WTFMove(locator))
{
}

MediaSampleCursor::MediaSampleCursor(Allocator&& allocator, const MediaSampleCursor& cursor)
    : CoreMediaWrapped(WTFMove(allocator))
    , m_trackReader(cursor.m_trackReader.copyRef())
    , m_locator(cursor.m_locator)
{
}

template<typename Iterator>
Optional<Iterator> MediaSampleCursor::locateIterator(SampleMap& samples, bool hasAllSamples) const
{
    ASSERT(m_locatorLock.isLocked());
    return WTF::switchOn(m_locator,
        [&](const auto& locator) -> Optional<Iterator> {
            auto iterator = makeIterator(locator, samples, hasAllSamples);
            if (!iterator)
                return WTF::nullopt;
            m_locator = WTFMove(*iterator);
            return locateIterator<Iterator>(samples, hasAllSamples);
        },
        [&](const Iterator& iterator) {
            return iterator;
        }
    );
}

MediaSample* MediaSampleCursor::locateMediaSample() const
{
    ASSERT(m_locatorLock.isLocked());
    return WTF::switchOn(m_locator,
        [&](const auto& locator) {
            m_trackReader->waitForSample([&](SampleMap& samples, bool hasAllSamples) {
                auto iterator = makeIterator(locator, samples, hasAllSamples);
                if (!iterator)
                    return hasAllSamples;
                m_locator = WTFMove(*iterator);
                return true;
            });
            return locateMediaSample();
        },
        [&](const MediaSampleCursor::DecodeOrderIterator& iterator) {
            return iterator->second.get();
        },
        [&](const MediaSampleCursor::PresentationOrderIterator& iterator) {
            return iterator->second.get();
        }
    );
}

template<typename OrderedMap, typename Step>
Step MediaSampleCursor::stepInOrderedMap(Step stepsRemaining)
{
    auto locker = holdLock(m_locatorLock);
    m_trackReader->waitForSample([&](SampleMap& samples, bool hasAllSamples) {
        if (auto iterator = locateIterator<typename OrderedMap::iterator>(samples, hasAllSamples)) {
            stepsRemaining = stepIterator(stepsRemaining, *iterator, orderedSamples<OrderedMap>(samples));
            m_locator = WTFMove(*iterator);
        }
        return hasAllSamples || !stepsRemaining;
    });
    return stepsRemaining;
}

OSStatus MediaSampleCursor::getMediaSample(CompletionHandler<void(MediaSample&)>&& completionHandler) const
{
    auto locker = holdLock(m_locatorLock);
    auto sample = locateMediaSample();
    if (!sample)
        return kMTPluginSampleCursorError_NoSamples;
    completionHandler(*sample);
    return noErr;
}

OSStatus MediaSampleCursor::copyProperty(CFStringRef, CFAllocatorRef, void*)
{
    return kCMBaseObjectError_ValueNotAvailable;
}

OSStatus MediaSampleCursor::copy(MTPluginSampleCursorRef* sampleCursor) const
{
    *sampleCursor = MediaSampleCursor::copy(allocator(), *this).leakRef()->wrapper();
    return noErr;
}

OSStatus MediaSampleCursor::stepInDecodeOrderAndReportStepsTaken(int64_t stepsToTake, int64_t* stepsTaken)
{
    auto stepsRemaining = stepInOrderedMap<DecodeOrderSampleMap>(stepsToTake);
    *stepsTaken = stepsToTake - stepsRemaining;
    return noErr;
}

OSStatus MediaSampleCursor::stepInPresentationOrderAndReportStepsTaken(int64_t stepsToTake, int64_t* stepsTaken)
{
    auto stepsRemaining = stepInOrderedMap<PresentationOrderSampleMap>(stepsToTake);
    *stepsTaken = stepsToTake - stepsRemaining;
    return noErr;
}

OSStatus MediaSampleCursor::stepByDecodeTime(CMTime time, Boolean* wasPinned)
{
    auto timeRemaining = stepInOrderedMap<DecodeOrderSampleMap>(PAL::toMediaTime(time));
    *wasPinned = !!timeRemaining;
    return noErr;
}

OSStatus MediaSampleCursor::stepByPresentationTime(CMTime time, Boolean* wasPinned)
{
    auto timeRemaining = stepInOrderedMap<PresentationOrderSampleMap>(PAL::toMediaTime(time));
    *wasPinned = !!timeRemaining;
    return noErr;
}

CFComparisonResult MediaSampleCursor::compareInDecodeOrder(MTPluginSampleCursorRef otherCursorRef) const
{
    if (wrapper() == otherCursorRef)
        return kCFCompareEqualTo;

    auto decodeTime = MediaTime::invalidTime();
    getMediaSample([&](MediaSample& sample) {
        decodeTime = sample.decodeTime();
    });

    auto otherDecodeTime = MediaTime::invalidTime();
    if (auto otherCursor = unwrap(otherCursorRef)) {
        otherCursor->getMediaSample([&](MediaSample& sample) {
            otherDecodeTime = sample.decodeTime();
        });
    }

    switch (decodeTime.compare(otherDecodeTime)) {
    case MediaTime::LessThan:
        return kCFCompareLessThan;
    case MediaTime::EqualTo:
        return kCFCompareEqualTo;
    case MediaTime::GreaterThan:
        return kCFCompareGreaterThan;
    }
}

OSStatus MediaSampleCursor::getSampleTiming(CMSampleTimingInfo* sampleTiming) const
{
    return getMediaSample([&](MediaSample& sample) {
        *sampleTiming = {
            // FIXME: Duration should be the difference in presentation time between this sample and the next in presentation order (or track duration if this is the last sample).
            .duration = PAL::toCMTime(sample.duration()),
            .presentationTimeStamp = PAL::toCMTime(sample.presentationTime()),
            .decodeTimeStamp = PAL::toCMTime(sample.decodeTime()),
        };
    });
}

OSStatus MediaSampleCursor::getSyncInfo(MTPluginSampleCursorSyncInfo* syncInfo) const
{
    return getMediaSample([&](MediaSample& sample) {
        *syncInfo = {
            .fullSync = sample.isSync()
        };
    });
}

OSStatus MediaSampleCursor::copyFormatDescription(CMFormatDescriptionRef* formatDescriptionOut) const
{
    return getMediaSample([&](MediaSample& sample) {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(sample.platformSample().type == PlatformSample::ByteRangeSampleType);
        *formatDescriptionOut = retainPtr(sample.platformSample().sample.byteRangeSample.second).leakRef();
    });
}

OSStatus MediaSampleCursor::copySampleLocation(MTPluginSampleCursorStorageRange* storageRange, MTPluginByteSourceRef* byteSource) const
{
    return getMediaSample([&](MediaSample& sample) {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(sample.platformSample().type == PlatformSample::ByteRangeSampleType);
        auto byteRange = *sample.byteRange();
        *storageRange = {
            .offset = CheckedInt64(byteRange.byteOffset).unsafeGet(),
            .length = CheckedInt64(byteRange.byteLength).unsafeGet(),
        };
        *byteSource = retainPtr(sample.platformSample().sample.byteRangeSample.first).leakRef();
    });
}

OSStatus MediaSampleCursor::getPlayableHorizon(CMTime*) const
{
    return kCMBaseObjectError_ValueNotAvailable;
}

} // namespace WebKit

#endif // ENABLE(WEBM_FORMAT_READER)
