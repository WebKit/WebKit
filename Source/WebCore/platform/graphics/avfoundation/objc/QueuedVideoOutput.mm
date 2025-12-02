/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "QueuedVideoOutput.h"

#if ENABLE(VIDEO)

#include <AVFoundation/AVPlayerItemOutput.h>
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <pal/spi/cocoa/AVFoundationSPI.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/TZoneMallocInlines.h>

#include <pal/cf/CoreMediaSoftLink.h>
#include <pal/cocoa/AVFoundationSoftLink.h>

SPECIALIZE_TYPE_TRAITS_BEGIN(AVPlayerItemVideoOutput)
static bool isType(const AVPlayerItemOutput& output) { return [&output isKindOfClass:PAL::getAVPlayerItemVideoOutputClassSingleton()]; }
SPECIALIZE_TYPE_TRAITS_END()

@interface WebQueuedVideoOutputDelegate : NSObject<AVPlayerItemOutputPullDelegate> {
    WeakPtr<WebCore::QueuedVideoOutput> _parent;
}
- (id)initWithParent:(WebCore::QueuedVideoOutput*)parent;
- (void)outputMediaDataWillChange:(AVPlayerItemOutput *)output;
- (void)outputSequenceWasFlushed:(AVPlayerItemOutput *)output;
- (void)observeValueForKeyPath:keyPath ofObject:(id)object change:(NSDictionary *)change context:(void*)context;
@end

@implementation WebQueuedVideoOutputDelegate
- (id)initWithParent:(WebCore::QueuedVideoOutput*)parent
{
    self = [super init];
    if (!self)
        return nil;

    _parent = parent;
    return self;
}

- (void)outputMediaDataWillChange:(AVPlayerItemOutput *)output
{
    auto* videoOutput = downcast<AVPlayerItemVideoOutput>(output);

    Vector<WebCore::QueuedVideoOutput::VideoFrameEntry> videoFrameEntries;
    do {
        CMTime earliestTime = [videoOutput earliestAvailablePixelBufferItemTime];
        if (CMTIME_IS_INVALID(earliestTime))
            break;

        auto pixelBuffer = adoptCF([videoOutput copyPixelBufferForItemTime:earliestTime itemTimeForDisplay:nil]);
        if (!pixelBuffer)
            break;

        videoFrameEntries.append({ WTFMove(pixelBuffer), PAL::toMediaTime(earliestTime) });
    } while (true);

    if (videoFrameEntries.isEmpty())
        return;

    callOnMainRunLoop([videoFrameEntries = WTFMove(videoFrameEntries), parent = _parent] () mutable {
        if (parent)
            parent->addVideoFrameEntries(WTFMove(videoFrameEntries));
    });
}

- (void)outputSequenceWasFlushed:(AVPlayerItemOutput *)output
{
    auto* videoOutput = downcast<AVPlayerItemVideoOutput>(output);
    [videoOutput requestNotificationOfMediaDataChangeAsSoonAsPossible];

    callOnMainRunLoop([parent = _parent] {
        if (parent)
            parent->purgeVideoFrameEntries();
    });
}

- (void)observeValueForKeyPath:keyPath ofObject:(id)object change:(NSDictionary *)change context:(void*)context
{
    if (![keyPath isEqualToString:@"rate"])
        return;

    RetainPtr rateValue = checked_objc_cast<NSNumber>([change valueForKey:NSKeyValueChangeNewKey]);
    auto rate = rateValue.get().floatValue;

    ensureOnMainRunLoop([parent = _parent, rate] {
        if (parent)
            parent->rateChanged(rate);
    });
}
@end

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(QueuedVideoOutput);

static dispatch_queue_t globalOutputDelegateQueue()
{
    static NeverDestroyed<OSObjectPtr<dispatch_queue_t>> globalQueue = adoptOSObject(dispatch_queue_create("WebQueuedVideoOutputDelegate queue", DISPATCH_QUEUE_SERIAL));
    return globalQueue.get().get();
}

RefPtr<QueuedVideoOutput> QueuedVideoOutput::create(AVPlayerItem* item, AVPlayer* player)
{
    auto queuedVideoOutput = adoptRef(new QueuedVideoOutput(item, player));
    return queuedVideoOutput->valid() ? queuedVideoOutput : nullptr;
}

QueuedVideoOutput::QueuedVideoOutput(AVPlayerItem* item, AVPlayer* player)
    : m_playerItem(item)
    , m_player(player)
    , m_delegate(adoptNS([[WebQueuedVideoOutputDelegate alloc] initWithParent:this]))
{
    m_videoOutput = adoptNS([PAL::allocAVPlayerItemVideoOutputInstance() initWithPixelBufferAttributes:nil]);
    if (!m_videoOutput) {
        // When bailing out early, also release these following objects
        // to avoid doing unnecessary work in ::invalidate(). Failure to
        // do so will result in exceptions being thrown from -removeObserver:.
        m_player = nullptr;
        m_playerItem = nullptr;
        m_delegate = nullptr;
        return;
    }

    [m_videoOutput setDelegate:m_delegate.get() queue:globalOutputDelegateQueue()];
    [m_videoOutput requestNotificationOfMediaDataChangeAsSoonAsPossible];

    [m_playerItem addOutput:m_videoOutput.get()];

    [m_player addObserver:m_delegate.get() forKeyPath:@"rate" options:(NSKeyValueObservingOptionNew | NSKeyValueObservingOptionInitial) context:nil];

    m_videoTimebaseObserver = [m_player addPeriodicTimeObserverForInterval:PAL::CMTimeMake(1, 60) queue:globalOutputDelegateQueue() usingBlock:[weakThis = WeakPtr { *this }, protectedDelegate = m_delegate, protectedOutput = m_videoOutput](CMTime currentTime) mutable {

        // Periodically check for new available pixel buffers.
        [protectedDelegate outputMediaDataWillChange:protectedOutput.get()];

        // And purge the back buffer of past frames.
        callOnMainRunLoop([weakThis = weakThis, time = PAL::toMediaTime(currentTime)] {
            if (weakThis)
                weakThis->purgeImagesBeforeTime(time);
        });
    }];
}

QueuedVideoOutput::~QueuedVideoOutput()
{
    invalidate();
}

bool QueuedVideoOutput::valid()
{
    return m_videoTimebaseObserver
        && m_videoOutput
        && m_player
        && m_playerItem;
}

void QueuedVideoOutput::invalidate()
{
    if (m_videoTimebaseObserver) {
        [m_player removeTimeObserver:m_videoTimebaseObserver.get()];
        m_videoTimebaseObserver = nil;
    }

    cancelNextImageTimeObserver();

    if (m_videoOutput) {
        [m_playerItem removeOutput:m_videoOutput.get()];
        [m_videoOutput setDelegate:nil queue:nil];
        m_videoOutput = nil;
    }

    if (m_player) {
        [m_player removeObserver:m_delegate.get() forKeyPath:@"rate"];
        m_player = nil;
    }

    m_playerItem = nil;
}

template <typename MaybeConstMap, typename MaybeConstIter = decltype(MaybeConstMap().begin())>
MaybeConstIter findImageForTime(MaybeConstMap& map, const MediaTime& time)
{
    // Outputs of AVPlayerItemVideoOutput have a display time, but not a duration; they are valid
    // until the arrival of a later frame. Find this frame efficiently using set::upper_bound to find
    // the next displayable frame, and walk backwards by one to find a frame for the target time.
    if (map.empty())
        return map.end();

    auto iter = map.upper_bound(time);
    if (iter == map.begin())
        return map.end();

    return --iter;
}

bool QueuedVideoOutput::hasImageForTime(const MediaTime& time) const
{
    return findImageForTime(m_videoFrames, time) != m_videoFrames.end();
}

auto QueuedVideoOutput::takeVideoFrameEntryForTime(const MediaTime& time) -> VideoFrameEntry
{
    auto iter = findImageForTime(m_videoFrames, time);
    if (iter == m_videoFrames.end())
        return { nullptr, MediaTime::invalidTime() };

    VideoFrameEntry entry = { WTFMove(iter->second), iter->first };

    // Purge all frames before `time`, so that repeated calls with the same time don't return
    // successively earlier images.
    m_videoFrames.erase(m_videoFrames.begin(), ++iter);

    return entry;
}

void QueuedVideoOutput::addCurrentImageChangedObserver(const CurrentImageChangedObserver& observer)
{
    m_currentImageChangedObservers.add(observer);
    configureNextImageTimeObserver();

    [m_videoOutput requestNotificationOfMediaDataChangeAsSoonAsPossible];
}

void QueuedVideoOutput::configureNextImageTimeObserver()
{
    if (m_nextImageTimebaseObserver)
        return;

    auto currentTime = PAL::toMediaTime([m_player currentTime]);
    auto iter = findImageForTime(m_videoFrames, currentTime);
    if (iter == m_videoFrames.end() || ++iter == m_videoFrames.end())
        return;

    auto nextImageTime = iter->first;

    m_nextImageTimebaseObserver = [m_player addBoundaryTimeObserverForTimes:@[[NSValue valueWithCMTime:PAL::toCMTime(nextImageTime)]] queue:globalOutputDelegateQueue() usingBlock:[weakThis = WeakPtr { *this }, protectedDelegate = m_delegate, protectedOutput = m_videoOutput] () mutable {
        callOnMainRunLoop([weakThis = WTFMove(weakThis)] {
            if (weakThis)
                weakThis->nextImageTimeReached();
        });
    }];
}

void QueuedVideoOutput::cancelNextImageTimeObserver()
{
    if (!m_nextImageTimebaseObserver)
        return;

    [m_player removeTimeObserver:m_nextImageTimebaseObserver.get()];
    m_nextImageTimebaseObserver = nil;
}

void QueuedVideoOutput::nextImageTimeReached()
{
    cancelNextImageTimeObserver();
    auto observers = std::exchange(m_currentImageChangedObservers, { });
    observers.forEach([](auto& observer) { observer(); });
}

void QueuedVideoOutput::addVideoFrameEntries(Vector<VideoFrameEntry>&& videoFrameEntries)
{
    bool needsImageForCurrentTimeChanged = false;
    bool hasCurrentImageChangedObservers = m_currentImageChangedObservers.computeSize();
    MediaTime currentTime = hasCurrentImageChangedObservers ? PAL::toMediaTime([m_player currentTime]) : MediaTime::invalidTime();

    for (auto& entry : videoFrameEntries) {
        if (hasCurrentImageChangedObservers && entry.displayTime <= currentTime)
            needsImageForCurrentTimeChanged = true;
        m_videoFrames.emplace(entry.displayTime, WTFMove(entry.pixelBuffer));
    }

    if (needsImageForCurrentTimeChanged)
        nextImageTimeReached();
    else if (hasCurrentImageChangedObservers)
        configureNextImageTimeObserver();

    if (m_paused)
        [m_videoOutput requestNotificationOfMediaDataChangeAsSoonAsPossible];
}

void QueuedVideoOutput::purgeVideoFrameEntries()
{
    cancelNextImageTimeObserver();
    if (!m_paused)
        m_videoFrames.clear();
    else if (m_videoFrames.size() >= 1)
        m_videoFrames.erase(m_videoFrames.begin(), --m_videoFrames.end());
}

void QueuedVideoOutput::purgeImagesBeforeTime(const MediaTime& time)
{
    m_videoFrames.erase(m_videoFrames.begin(), findImageForTime(m_videoFrames, time));
}

void QueuedVideoOutput::rateChanged(float rate)
{
    m_paused = !rate;

    if (m_paused)
        [m_videoOutput requestNotificationOfMediaDataChangeAsSoonAsPossible];
}

}

#endif

