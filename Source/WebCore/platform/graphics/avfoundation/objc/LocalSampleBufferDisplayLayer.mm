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

#import "config.h"
#import "LocalSampleBufferDisplayLayer.h"

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#import "Color.h"
#import "IntSize.h"
#import "Logging.h"
#import "MediaUtilities.h"

#import <AVFoundation/AVSampleBufferDisplayLayer.h>
#import <QuartzCore/CALayer.h>
#import <QuartzCore/CATransaction.h>

#import <wtf/MainThread.h>
#import <wtf/MonotonicTime.h>
#import <wtf/cf/TypeCastsCF.h>

#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

using namespace WebCore;

@interface WebAVSampleBufferStatusChangeListener : NSObject {
    WeakPtr<LocalSampleBufferDisplayLayer> _parent;
}

- (id)initWithParent:(LocalSampleBufferDisplayLayer*)callback;
- (void)invalidate;
- (void)begin;
- (void)stop;
@end

@implementation WebAVSampleBufferStatusChangeListener

- (id)initWithParent:(LocalSampleBufferDisplayLayer*)parent
{
    if (!(self = [super init]))
        return nil;

    _parent = parent;

    return self;
}

- (void)dealloc
{
    [self invalidate];
    [super dealloc];
}

- (void)invalidate
{
    [self stop];
    _parent = nullptr;
}

- (void)begin
{
    ASSERT(_parent);
    ASSERT(_parent->displayLayer());
    ASSERT(_parent->rootLayer());

    [_parent->displayLayer() addObserver:self forKeyPath:@"status" options:NSKeyValueObservingOptionNew context:nil];
    [_parent->displayLayer() addObserver:self forKeyPath:@"error" options:NSKeyValueObservingOptionNew context:nil];
}

- (void)stop
{
    if (!_parent)
        return;

    if (_parent->displayLayer()) {
        [_parent->displayLayer() removeObserver:self forKeyPath:@"status"];
        [_parent->displayLayer() removeObserver:self forKeyPath:@"error"];
    }
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(keyPath);
    UNUSED_PARAM(change);

    if (![object isKindOfClass:PAL::getAVSampleBufferDisplayLayerClass()])
        return;

    if ([keyPath isEqualToString:@"status"]) {
        callOnMainThread([protectedSelf = RetainPtr<WebAVSampleBufferStatusChangeListener>(self)] {
            if (protectedSelf->_parent)
                protectedSelf->_parent->layerStatusDidChange();
        });
        return;
    }

    if ([keyPath isEqualToString:@"error"]) {
        callOnMainThread([protectedSelf = RetainPtr<WebAVSampleBufferStatusChangeListener>(self)] {
            if (protectedSelf->_parent)
                protectedSelf->_parent->layerErrorDidChange();
        });
        return;
    }
}
@end

namespace WebCore {

static void runWithoutAnimations(const WTF::Function<void()>& function)
{
    [CATransaction begin];
    [CATransaction setAnimationDuration:0];
    [CATransaction setDisableActions:YES];
    function();
    [CATransaction commit];
}

std::unique_ptr<LocalSampleBufferDisplayLayer> LocalSampleBufferDisplayLayer::create(Client& client)
{
    auto sampleBufferDisplayLayer = adoptNS([PAL::allocAVSampleBufferDisplayLayerInstance() init]);
    if (!sampleBufferDisplayLayer)
        return nullptr;

    return makeUnique<LocalSampleBufferDisplayLayer>(WTFMove(sampleBufferDisplayLayer), client);
}

LocalSampleBufferDisplayLayer::LocalSampleBufferDisplayLayer(RetainPtr<AVSampleBufferDisplayLayer>&& sampleBufferDisplayLayer, Client& client)
    : SampleBufferDisplayLayer(client)
    , m_statusChangeListener(adoptNS([[WebAVSampleBufferStatusChangeListener alloc] initWithParent:this]))
    , m_sampleBufferDisplayLayer(WTFMove(sampleBufferDisplayLayer))
    , m_processingQueue(WorkQueue::create("LocalSampleBufferDisplayLayer queue"))
#if !RELEASE_LOG_DISABLED
    , m_frameRateMonitor([this](auto info) { onIrregularFrameRateNotification(info.frameTime, info.lastFrameTime); })
#endif
{
    ASSERT(isMainThread());
}

void LocalSampleBufferDisplayLayer::initialize(bool hideRootLayer, IntSize size, CompletionHandler<void(bool didSucceed)>&& callback)
{
    m_sampleBufferDisplayLayer.get().anchorPoint = { .5, .5 };
    m_sampleBufferDisplayLayer.get().needsDisplayOnBoundsChange = YES;
    m_sampleBufferDisplayLayer.get().videoGravity = AVLayerVideoGravityResizeAspectFill;

    m_rootLayer = adoptNS([[CALayer alloc] init]);
    m_rootLayer.get().hidden = hideRootLayer;

    m_rootLayer.get().needsDisplayOnBoundsChange = YES;

    m_rootLayer.get().bounds = CGRectMake(0, 0, size.width(), size.height());

    [m_statusChangeListener begin];

    [m_rootLayer addSublayer:m_sampleBufferDisplayLayer.get()];

#ifndef NDEBUG
    [m_sampleBufferDisplayLayer setName:@"LocalSampleBufferDisplayLayer AVSampleBufferDisplayLayer"];
    [m_rootLayer setName:@"LocalSampleBufferDisplayLayer AVSampleBufferDisplayLayer parent"];
#endif
    callback(true);
}

LocalSampleBufferDisplayLayer::~LocalSampleBufferDisplayLayer()
{
    ASSERT(isMainThread());

    m_processingQueue->dispatchSync([] { });

    m_processingQueue = nullptr;

    [m_statusChangeListener stop];

    m_pendingVideoFrameQueue.clear();

    [m_sampleBufferDisplayLayer stopRequestingMediaData];
    [m_sampleBufferDisplayLayer flush];
    m_sampleBufferDisplayLayer = nullptr;

    m_rootLayer = nullptr;
}

void LocalSampleBufferDisplayLayer::layerStatusDidChange()
{
    ASSERT(isMainThread());
    if (m_client && m_sampleBufferDisplayLayer.get().status == AVQueuedSampleBufferRenderingStatusFailed)
        m_client->sampleBufferDisplayLayerStatusDidFail();
}

void LocalSampleBufferDisplayLayer::layerErrorDidChange()
{
    ASSERT(isMainThread());
    // FIXME: Log error.
}

PlatformLayer* LocalSampleBufferDisplayLayer::displayLayer()
{
    return m_sampleBufferDisplayLayer.get();
}

PlatformLayer* LocalSampleBufferDisplayLayer::rootLayer()
{
    return m_rootLayer.get();
}

bool LocalSampleBufferDisplayLayer::didFail() const
{
    return [m_sampleBufferDisplayLayer status] == AVQueuedSampleBufferRenderingStatusFailed;
}

void LocalSampleBufferDisplayLayer::updateDisplayMode(bool hideDisplayLayer, bool hideRootLayer)
{
    if (m_rootLayer.get().hidden == hideRootLayer && m_sampleBufferDisplayLayer.get().hidden == hideDisplayLayer)
        return;

    runWithoutAnimations([&] {
        if (hideDisplayLayer && !hideRootLayer)
            m_rootLayer.get().backgroundColor = cachedCGColor(Color::black).get();
        else
            m_rootLayer.get().backgroundColor = nil;
        m_sampleBufferDisplayLayer.get().hidden = hideDisplayLayer;
        m_rootLayer.get().hidden = hideRootLayer;
    });
}

CGRect LocalSampleBufferDisplayLayer::bounds() const
{
    return m_rootLayer.get().bounds;
}

void LocalSampleBufferDisplayLayer::updateRootLayerAffineTransform(CGAffineTransform transform)
{
    runWithoutAnimations([&] {
        m_rootLayer.get().affineTransform = transform;
    });
}

void LocalSampleBufferDisplayLayer::updateAffineTransform(CGAffineTransform transform)
{
    runWithoutAnimations([&] {
        m_sampleBufferDisplayLayer.get().affineTransform = transform;
    });
}

void LocalSampleBufferDisplayLayer::updateBoundsAndPosition(CGRect bounds, VideoFrame::Rotation rotation)
{
    updateRootLayerBoundsAndPosition(bounds, rotation, ShouldUpdateRootLayer::No);
}

void LocalSampleBufferDisplayLayer::setRootLayerBoundsAndPositions(CGRect bounds, VideoFrame::Rotation rotation)
{
    CGPoint position = { bounds.size.width / 2, bounds.size.height / 2};
    if (rotation == VideoFrame::Rotation::Right || rotation == VideoFrame::Rotation::Left)
        std::swap(bounds.size.width, bounds.size.height);

    m_rootLayer.get().position = position;
    m_rootLayer.get().bounds = bounds;
}

void LocalSampleBufferDisplayLayer::updateRootLayerBoundsAndPosition(CGRect bounds, VideoFrame::Rotation rotation, ShouldUpdateRootLayer shouldUpdateRootLayer)
{
    runWithoutAnimations([&] {
        if (shouldUpdateRootLayer == ShouldUpdateRootLayer::Yes)
            setRootLayerBoundsAndPositions(bounds, rotation);

        if (rotation == VideoFrame::Rotation::Right || rotation == VideoFrame::Rotation::Left)
            std::swap(bounds.size.width, bounds.size.height);

        CGPoint position = { bounds.size.width / 2, bounds.size.height / 2};

        m_sampleBufferDisplayLayer.get().position = position;
        m_sampleBufferDisplayLayer.get().bounds = bounds;
    });
}

void LocalSampleBufferDisplayLayer::flush()
{
    m_processingQueue->dispatch([this] {
        [m_sampleBufferDisplayLayer flush];
    });
}

void LocalSampleBufferDisplayLayer::flushAndRemoveImage()
{
    m_processingQueue->dispatch([this] {
        [m_sampleBufferDisplayLayer flushAndRemoveImage];
    });
}

static const double rendererLatency = 0.02;

void LocalSampleBufferDisplayLayer::enqueueVideoFrame(VideoFrame& videoFrame)
{
    if (m_paused) {
#if !RELEASE_LOG_DISABLED
        m_frameRateMonitor.update();
#endif
        return;
    }

    m_processingQueue->dispatch([this, videoFrame = Ref { videoFrame }]() mutable {
        if (![m_sampleBufferDisplayLayer isReadyForMoreMediaData]) {
            RELEASE_LOG(WebRTC, "LocalSampleBufferDisplayLayer::enqueueSample (%{public}s) not ready for more media data", m_logIdentifier.utf8().data());
            addVideoFrameToPendingQueue(WTFMove(videoFrame));
            requestNotificationWhenReadyForVideoData();
            return;
        }
        enqueueBuffer(videoFrame->pixelBuffer(), videoFrame->presentationTime());
    });
}

static void setSampleBufferAsDisplayImmediately(CMSampleBufferRef sampleBuffer)
{
    CFArrayRef attachmentsArray = PAL::CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, true);
    for (CFIndex i = 0; i < CFArrayGetCount(attachmentsArray); ++i) {
        CFMutableDictionaryRef attachments = checked_cf_cast<CFMutableDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray, i));
        CFDictionarySetValue(attachments, PAL::kCMSampleAttachmentKey_DisplayImmediately, kCFBooleanTrue);
    }
}

void LocalSampleBufferDisplayLayer::enqueueBuffer(CVPixelBufferRef pixelBuffer, MediaTime presentationTime)
{
    ASSERT(!isMainThread());

    auto sampleBuffer = createVideoSampleBuffer(pixelBuffer, PAL::toCMTime(presentationTime));
    auto now = MediaTime::createWithDouble(MonotonicTime::now().secondsSinceEpoch().value() + rendererLatency);
    if (m_renderPolicy == RenderPolicy::Immediately || now >= presentationTime)
        setSampleBufferAsDisplayImmediately(sampleBuffer.get());

    [m_sampleBufferDisplayLayer enqueueSampleBuffer:sampleBuffer.get()];

#if !RELEASE_LOG_DISABLED
    constexpr size_t frameCountPerLog = 1800; // log every minute at 30 fps
    if (!(m_frameRateMonitor.frameCount() % frameCountPerLog)) {
        if (auto* metrics = [m_sampleBufferDisplayLayer videoPerformanceMetrics])
            RELEASE_LOG(WebRTC, "LocalSampleBufferDisplayLayer (%{public}s) metrics, total=%lu, dropped=%lu, corrupted=%lu, display-composited=%lu, non-display-composited=%lu (pending=%lu)", m_logIdentifier.utf8().data(), metrics.totalNumberOfVideoFrames, metrics.numberOfDroppedVideoFrames, metrics.numberOfCorruptedVideoFrames, metrics.numberOfDisplayCompositedVideoFrames, metrics.numberOfNonDisplayCompositedVideoFrames, m_pendingVideoFrameQueue.size());
    }
    m_frameRateMonitor.update();
#endif
}

#if !RELEASE_LOG_DISABLED
void LocalSampleBufferDisplayLayer::onIrregularFrameRateNotification(MonotonicTime frameTime, MonotonicTime lastFrameTime)
{
    callOnMainThread([frameTime = frameTime.secondsSinceEpoch().value(), lastFrameTime = lastFrameTime.secondsSinceEpoch().value(), observedFrameRate = m_frameRateMonitor.observedFrameRate(), frameCount = m_frameRateMonitor.frameCount(), weakThis = WeakPtr { *this }] {
        if (!weakThis)
            return;
        RELEASE_LOG(WebRTC, "LocalSampleBufferDisplayLayer::enqueueVideoFrame (%{public}s) at %f, previous frame was at %f, observed frame rate is %f, delay since last frame is %f ms, frame count is %lu", weakThis->m_logIdentifier.utf8().data(), frameTime, lastFrameTime, observedFrameRate, (frameTime - lastFrameTime) * 1000, frameCount);
    });
}
#endif

void LocalSampleBufferDisplayLayer::removeOldVideoFramesFromPendingQueue()
{
    ASSERT(!isMainThread());

    if (m_pendingVideoFrameQueue.isEmpty())
        return;

    if (m_renderPolicy == RenderPolicy::Immediately) {
        m_pendingVideoFrameQueue.clear();
        return;
    }

    auto now = MediaTime::createWithDouble(MonotonicTime::now().secondsSinceEpoch().value());
    while (!m_pendingVideoFrameQueue.isEmpty()) {
        auto presentationTime = m_pendingVideoFrameQueue.first()->presentationTime();
        if (presentationTime.isValid() && presentationTime > now)
            break;
        m_pendingVideoFrameQueue.removeFirst();
    }
}

void LocalSampleBufferDisplayLayer::addVideoFrameToPendingQueue(Ref<VideoFrame>&& videoFrame)
{
    ASSERT(!isMainThread());

    removeOldVideoFramesFromPendingQueue();
    m_pendingVideoFrameQueue.append(WTFMove(videoFrame));
}

void LocalSampleBufferDisplayLayer::clearVideoFrames()
{
    m_processingQueue->dispatch([this] {
        m_pendingVideoFrameQueue.clear();
    });
}

void LocalSampleBufferDisplayLayer::requestNotificationWhenReadyForVideoData()
{
    WeakPtr weakThis { *this };
    [m_sampleBufferDisplayLayer requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^{
        if (!weakThis)
            return;

        m_processingQueue->dispatch([this] {
            [m_sampleBufferDisplayLayer stopRequestingMediaData];

            while (!m_pendingVideoFrameQueue.isEmpty()) {
                if (![m_sampleBufferDisplayLayer isReadyForMoreMediaData]) {
                    requestNotificationWhenReadyForVideoData();
                    return;
                }
                auto videoFrame = m_pendingVideoFrameQueue.takeFirst();
                enqueueBuffer(videoFrame->pixelBuffer(), videoFrame->presentationTime());
            }
        });
    }];
}

}

#endif
