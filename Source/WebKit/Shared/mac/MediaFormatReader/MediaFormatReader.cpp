/*
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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
#include "MediaFormatReader.h"

#if ENABLE(WEBM_FORMAT_READER)

#include "Logging.h"
#include "MediaTrackReader.h"
#include <WebCore/AudioTrackPrivate.h>
#include <WebCore/ContentType.h>
#include <WebCore/Document.h>
#include <WebCore/InbandTextTrackPrivate.h>
#include <WebCore/MediaSample.h>
#include <WebCore/MediaSampleAVFObjC.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/SourceBufferParserWebM.h>
#include <WebCore/VideoTrackPrivate.h>
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/LoggerHelper.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/WorkQueue.h>

#include <pal/cf/CoreMediaSoftLink.h>
#include <pal/cocoa/MediaToolboxSoftLink.h>

WTF_DECLARE_CF_TYPE_TRAIT(MTPluginFormatReader);

namespace WebKit {

using namespace WebCore;

class AbortAction {
public:
    AbortAction(Condition& condition)
        : m_condition(condition)
    {
        if (noErr != PAL::FigThreadRegisterAbortAction(action, this, &m_token))
            m_token = nullptr;
    }

    ~AbortAction()
    {
        if (m_token)
            PAL::FigThreadUnregisterAbortAction(m_token);
    }

    bool aborted() const { return m_aborted; }

private:
    static void action(void* refcon)
    {
        auto thisPtr = static_cast<AbortAction*>(refcon);
        thisPtr->m_aborted = true;
        thisPtr->m_condition.notifyAll();
    }

    Condition& m_condition;
    FigThreadAbortActionToken m_token { nullptr };
    bool m_aborted { false };
};

CMBaseClassID MediaFormatReader::wrapperClassID()
{
    return MTPluginFormatReaderGetClassID();
}

CoreMediaWrapped<MediaFormatReader>* MediaFormatReader::unwrap(CMBaseObjectRef object)
{
    return unwrap(checked_cf_cast<WrapperRef>(object));
}

RefPtr<MediaFormatReader> MediaFormatReader::create(Allocator&& allocator)
{
    return adoptRef(new (allocator) MediaFormatReader(WTFMove(allocator)));
}

MediaFormatReader::MediaFormatReader(Allocator&& allocator)
    : CoreMediaWrapped(WTFMove(allocator))
    , m_duration(MediaTime::invalidTime())
{
}

void MediaFormatReader::startOnMainThread(MTPluginByteSourceRef byteSource)
{
    ensureOnMainRunLoop([this, protectedThis = Ref { *this }, byteSource = retainPtr(byteSource)]() mutable {
        parseByteSource(WTFMove(byteSource));
    });
}

static ConcurrentWorkQueue& readerQueue()
{
    static NeverDestroyed<Ref<ConcurrentWorkQueue>> queue = ConcurrentWorkQueue::create("WebKit::MediaFormatReader Queue");
    return queue.get();
}

void MediaFormatReader::parseByteSource(RetainPtr<MTPluginByteSourceRef>&& byteSource)
{
    ASSERT(isMainRunLoop());

    if (!m_logger) {
        m_logger = &Document::sharedLogger();
        m_logIdentifier = nextLogIdentifier();
    }

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    Locker locker { m_parseTracksLock };
    m_byteSource = WTFMove(byteSource);
    m_parseTracksStatus = std::nullopt;
    m_duration = MediaTime::invalidTime();
    m_trackReaders.clear();

    // FIXME: why do we need a storage queue different to reader's queue?
    readerQueue().dispatch([this, protectedThis = Ref { *this }, byteSource = m_byteSource]() mutable {
        WebMParser parser(*this);
        parser.setLogger(*m_logger, m_logIdentifier);
        parser.createByteRangeSamples();
        auto result = parser.parse(WTFMove(byteSource));
        MediaTrackReader::storageQueue().dispatch([this, protectedThis = Ref { *this }, result] () mutable {
            if (!result.hasException() && result.returnValue())
                didParseTracks({ }, result.returnValue());
            finishParsing();
        });
    });
}

void MediaFormatReader::parsedInitializationData(SourceBufferParser::InitializationSegment&& initializationSegment)
{
    MediaTrackReader::storageQueue().dispatch([this, protectedThis = Ref { *this }, initializationSegment = WTFMove(initializationSegment)] () mutable {
        didParseTracks(WTFMove(initializationSegment), noErr);
    });
}

void MediaFormatReader::parsedMediaData(MediaSamplesBlock&& mediaSamples)
{
    MediaTrackReader::storageQueue().dispatch([this, protectedThis = Ref { *this }, mediaSamples = WTFMove(mediaSamples)] () mutable {
        didProvideMediaData(WTFMove(mediaSamples));
    });
}

void MediaFormatReader::didParseTracks(SourceBufferPrivateClient::InitializationSegment&& segment, uint64_t errorCode)
{
    ASSERT(!isMainRunLoop());

    Locker locker { m_parseTracksLock };
    ASSERT(!m_parseTracksStatus || (m_init && errorCode));
    ASSERT(m_duration.isInvalid() || (m_init && errorCode));
    ASSERT(m_trackReaders.isEmpty() || (m_init && errorCode));

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (errorCode)
        ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, errorCode);

    m_parseTracksStatus = errorCode ? static_cast<OSStatus>(kMTPluginFormatReaderError_ParsingFailure) : noErr;
    m_duration = WTFMove(segment.duration);

    for (auto& videoTrack : segment.videoTracks) {
        auto track = videoTrack.track.get();
        auto trackReader = MediaTrackReader::create(allocator(), *this, kCMMediaType_Video, *track->trackUID(), track->defaultEnabled());
        if (!trackReader)
            continue;

        m_trackReaders.append(trackReader.releaseNonNull());
    }

    for (auto& audioTrack : segment.audioTracks) {
        auto track = audioTrack.track.get();
        auto trackReader = MediaTrackReader::create(allocator(), *this, kCMMediaType_Audio, *track->trackUID(), track->defaultEnabled());
        if (!trackReader)
            continue;

        m_trackReaders.append(trackReader.releaseNonNull());
    }

    for (auto& textTrack : segment.textTracks) {
        auto track = textTrack.track.get();
        if (auto trackReader = MediaTrackReader::create(allocator(), *this, kCMMediaType_Text, *track->trackUID(), track->defaultEnabled()))
            m_trackReaders.append(trackReader.releaseNonNull());
    }

    m_parseTracksCondition.notifyAll();
    m_init = true;
}

void MediaFormatReader::didProvideMediaData(MediaSamplesBlock&& mediaSamples)
{
    ASSERT(!isMainRunLoop());

    Locker locker { m_parseTracksLock };
    auto trackIndex = m_trackReaders.findIf([&](auto& track) {
        return track->trackID() == mediaSamples.info()->trackID;
    });

    if (trackIndex != notFound)
        m_trackReaders[trackIndex]->addSample(WTFMove(mediaSamples), m_byteSource.get());
}

void MediaFormatReader::finishParsing()
{
    ASSERT(!isMainRunLoop());
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    Locker locker { m_parseTracksLock };
    ASSERT(m_parseTracksStatus.has_value());

    for (auto& trackReader : m_trackReaders)
        trackReader->finishParsing();

    if (m_duration.isIndefinite()) {
        MediaTime greatestPresentationTime { MediaTime::invalidTime() };
        for (auto& trackReader : m_trackReaders) {
            if (greatestPresentationTime.isInvalid() || trackReader->greatestPresentationTime() > greatestPresentationTime)
                greatestPresentationTime = trackReader->greatestPresentationTime();
        }
        if (greatestPresentationTime.isValid())
            m_duration = greatestPresentationTime;
    }
}

OSStatus MediaFormatReader::copyProperty(CFStringRef key, CFAllocatorRef allocator, void* valueCopy)
{
    if (!CFEqual(key, PAL::get_MediaToolbox_kMTPluginFormatReaderProperty_Duration())) {
        ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "asked for unsupported property ", String(key));
        return kCMBaseObjectError_ValueNotAvailable;
    }

    AbortAction action { m_parseTracksCondition };

    Locker locker { m_parseTracksLock };
    m_parseTracksCondition.wait(m_parseTracksLock, [&] {
        assertIsHeld(m_parseTracksLock);
        return m_parseTracksStatus.has_value() || action.aborted();
    });

    if (m_duration.isIndefinite())
        return kCMBaseObjectError_ValueNotAvailable;

    if (auto leakedDuration = adoptCF(PAL::CMTimeCopyAsDictionary(PAL::toCMTime(m_duration), allocator)).leakRef()) {
        *reinterpret_cast<CFDictionaryRef*>(valueCopy) = leakedDuration;
        return noErr;
    }

    ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "Could not retrieve value for property ", String(key));
    return kCMBaseObjectError_ValueNotAvailable;
}

OSStatus MediaFormatReader::copyTrackArray(CFArrayRef* trackArrayCopy)
{
    AbortAction action { m_parseTracksCondition };

    Locker locker { m_parseTracksLock };
    m_parseTracksCondition.wait(m_parseTracksLock, [&] {
        assertIsHeld(m_parseTracksLock);
        return m_parseTracksStatus.has_value() || action.aborted();
    });

    if (*m_parseTracksStatus != noErr)
        return *m_parseTracksStatus;

    auto mutableArray = adoptCF(CFArrayCreateMutable(allocator(), Checked<CFIndex>(m_trackReaders.size()), &kCFTypeArrayCallBacks));
    for (auto& trackReader : m_trackReaders)
        CFArrayAppendValue(mutableArray.get(), trackReader->wrapper());

    *trackArrayCopy = adoptCF(CFArrayCreateCopy(allocator(), mutableArray.get())).leakRef();
    return noErr;
}

const void* MediaFormatReader::nextTrackReaderLogIdentifier(uint64_t trackID) const
{
    return LoggerHelper::childLogIdentifier(m_logIdentifier, trackID);
}

const void* MediaFormatReader::nextLogIdentifier()
{
    static uint64_t logIdentifier = cryptographicallyRandomNumber<uint32_t>();
    return reinterpret_cast<const void*>(++logIdentifier);
}

WTFLogChannel& MediaFormatReader::logChannel()
{
    return JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, Media);
}

} // namespace WebKit

#endif // ENABLE(WEBM_FORMAT_READER)
