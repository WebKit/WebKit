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
#import "VideoFrame.h"
#import "VideoFrameMetadata.h"
#import <AVFoundation/AVSampleBufferDisplayLayer.h>
#import <QuartzCore/CALayer.h>
#import <QuartzCore/CATransaction.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <wtf/MainThread.h>
#import <wtf/MonotonicTime.h>
#import <wtf/cf/TypeCastsCF.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

using namespace WebCore;

@interface WebAVSampleBufferStatusChangeListener : NSObject {
    RetainPtr<PlatformLayer> _displayLayer;
    ThreadSafeWeakPtr<LocalSampleBufferDisplayLayer> _parent;
}

- (id)initWithParent:(LocalSampleBufferDisplayLayer*)callback;
- (void)invalidate;
- (void)begin:(PlatformLayer*)displayLayer;
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

- (void)begin:(PlatformLayer*)displayLayer
{
    _displayLayer = displayLayer;
    [_displayLayer addObserver:self forKeyPath:@"status" options:NSKeyValueObservingOptionNew context:nil];
    [_displayLayer addObserver:self forKeyPath:@"error" options:NSKeyValueObservingOptionNew context:nil];
}

- (void)stop
{
    if (!_displayLayer)
        return;

    [_displayLayer removeObserver:self forKeyPath:@"status"];
    [_displayLayer removeObserver:self forKeyPath:@"error"];
    _displayLayer = nullptr;
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
            if (auto parent = protectedSelf->_parent.get())
                parent->layerStatusDidChange();
        });
        return;
    }

    if ([keyPath isEqualToString:@"error"]) {
        callOnMainThread([protectedSelf = RetainPtr<WebAVSampleBufferStatusChangeListener>(self)] {
            if (auto parent = protectedSelf->_parent.get())
                parent->layerErrorDidChange();
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

RefPtr<LocalSampleBufferDisplayLayer> LocalSampleBufferDisplayLayer::create(Client& client)
{
    RetainPtr<AVSampleBufferDisplayLayer> sampleBufferDisplayLayer;
    @try {
        sampleBufferDisplayLayer = adoptNS([PAL::allocAVSampleBufferDisplayLayerInstance() init]);
    } @catch(id exception) {
        RELEASE_LOG_ERROR(WebRTC, "LocalSampleBufferDisplayLayer::create failed to allocate display layer");
    }
    if (!sampleBufferDisplayLayer)
        return nullptr;

    return adoptRef(*new LocalSampleBufferDisplayLayer(WTFMove(sampleBufferDisplayLayer), client));
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

void LocalSampleBufferDisplayLayer::initialize(bool hideRootLayer, IntSize size, bool shouldMaintainAspectRatio, CompletionHandler<void(bool didSucceed)>&& callback)
{
    m_sampleBufferDisplayLayer.get().anchorPoint = { .5, .5 };
    m_sampleBufferDisplayLayer.get().videoGravity = shouldMaintainAspectRatio ? AVLayerVideoGravityResizeAspect : AVLayerVideoGravityResize;

    m_rootLayer = adoptNS([[CALayer alloc] init]);
    m_rootLayer.get().hidden = hideRootLayer;

    auto bounds = CGRectMake(0, 0, size.width(), size.height());
    m_rootLayer.get().bounds = bounds;
    m_rootLayer.get().position = { bounds.size.width / 2, bounds.size.height / 2 };
    m_sampleBufferDisplayLayer.get().bounds = bounds;
    m_sampleBufferDisplayLayer.get().position = { bounds.size.width / 2, bounds.size.height / 2 };

    [m_statusChangeListener begin:m_sampleBufferDisplayLayer.get()];

    [m_rootLayer addSublayer:m_sampleBufferDisplayLayer.get()];

    [m_sampleBufferDisplayLayer setName:@"LocalSampleBufferDisplayLayer AVSampleBufferDisplayLayer"];
    [m_rootLayer setName:@"LocalSampleBufferDisplayLayer AVSampleBufferDisplayLayer parent"];

    callback(true);
}

LocalSampleBufferDisplayLayer::~LocalSampleBufferDisplayLayer()
{
    ASSERT(isMainThread());

    [m_statusChangeListener stop];

    m_pendingVideoFrameQueue.clear();

    [m_sampleBufferDisplayLayer stopRequestingMediaData];
    [m_sampleBufferDisplayLayer flush];
    m_sampleBufferDisplayLayer = nullptr;

    m_rootLayer = nullptr;
}

void LocalSampleBufferDisplayLayer::setShouldMaintainAspectRatio(bool shouldMaintainAspectRatio)
{
    ensureOnMainThread([this, weakThis = ThreadSafeWeakPtr { *this }, shouldMaintainAspectRatio]() mutable {
        auto protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        runWithoutAnimations([&] {
            m_sampleBufferDisplayLayer.get().videoGravity = shouldMaintainAspectRatio ? AVLayerVideoGravityResizeAspect : AVLayerVideoGravityResize;
        });
    });
}

void LocalSampleBufferDisplayLayer::layerStatusDidChange()
{
    ASSERT(isMainThread());
    if (m_client && m_sampleBufferDisplayLayer.get().status == AVQueuedSampleBufferRenderingStatusFailed) {
        RELEASE_LOG_ERROR(WebRTC, "LocalSampleBufferDisplayLayer::layerStatusDidChange going to failed status (%{public}s) ", m_logIdentifier.utf8().data());
        if (!m_didFail) {
            m_didFail = true;
            m_client->sampleBufferDisplayLayerStatusDidFail();
        }
    }
}

void LocalSampleBufferDisplayLayer::layerErrorDidChange()
{
    ASSERT(isMainThread());
    RELEASE_LOG_ERROR(WebRTC, "LocalSampleBufferDisplayLayer::layerErrorDidChange (%{public}s) ", m_logIdentifier.utf8().data());
    if (!m_client || m_didFail)
        return;
    m_didFail = true;
    m_client->sampleBufferDisplayLayerStatusDidFail();
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
    return m_didFail || [m_sampleBufferDisplayLayer status] == AVQueuedSampleBufferRenderingStatusFailed;
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

void LocalSampleBufferDisplayLayer::updateBoundsAndPosition(CGRect bounds, std::optional<WTF::MachSendRight>&&)
{
    updateSampleLayerBoundsAndPosition(bounds);
}

void LocalSampleBufferDisplayLayer::updateSampleLayerBoundsAndPosition(std::optional<CGRect> bounds)
{
    ensureOnMainThread([this, weakThis = ThreadSafeWeakPtr { *this }, bounds, rotation = m_videoFrameRotation, affineTransform = m_affineTransform]() mutable {
        auto protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        auto layerBounds = bounds.value_or(m_rootLayer.get().bounds);
        CGPoint layerPosition { layerBounds.size.width / 2, layerBounds.size.height / 2 };
        if (rotation == VideoFrame::Rotation::Right || rotation == VideoFrame::Rotation::Left)
            std::swap(layerBounds.size.width, layerBounds.size.height);
        runWithoutAnimations([&] {
            if (bounds) {
                m_rootLayer.get().position = { bounds->size.width / 2, bounds->size.height / 2 };
                m_rootLayer.get().bounds = *bounds;
            }
            m_sampleBufferDisplayLayer.get().affineTransform = affineTransform;
            m_sampleBufferDisplayLayer.get().position = layerPosition;
            m_sampleBufferDisplayLayer.get().bounds = layerBounds;
        });
    });
}

void LocalSampleBufferDisplayLayer::flush()
{
    m_processingQueue->dispatch([this, weakThis = ThreadSafeWeakPtr { *this }] {
        auto protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        [m_sampleBufferDisplayLayer flush];
    });
}

void LocalSampleBufferDisplayLayer::flushAndRemoveImage()
{
    m_processingQueue->dispatch([this, weakThis = ThreadSafeWeakPtr { *this }] {
        auto protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        @try {
            [m_sampleBufferDisplayLayer flushAndRemoveImage];
        } @catch(id exception) {
            RELEASE_LOG_ERROR(WebRTC, "LocalSampleBufferDisplayLayer::flushAndRemoveImage failed");
            layerErrorDidChange();
        }
    });
}

static void setSampleBufferAsDisplayImmediately(CMSampleBufferRef sampleBuffer)
{
    CFArrayRef attachmentsArray = PAL::CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, true);
    if (!attachmentsArray)
        return;
    for (CFIndex i = 0; i < CFArrayGetCount(attachmentsArray); ++i) {
        CFMutableDictionaryRef attachments = checked_cf_cast<CFMutableDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray, i));
        CFDictionarySetValue(attachments, PAL::kCMSampleAttachmentKey_DisplayImmediately, kCFBooleanTrue);
    }
}

static inline CGAffineTransform transformationMatrixForVideoFrame(VideoFrame& videoFrame)
{
    auto size = videoFrame.presentationSize();
    size_t width = static_cast<size_t>(size.width());
    size_t height = static_cast<size_t>(size.height());
    if (!width || !height)
        return CGAffineTransformIdentity;

#if PLATFORM(MAC)
    int rotationAngle = -static_cast<int>(videoFrame.rotation());
#else
    int rotationAngle = static_cast<int>(videoFrame.rotation());
#endif
    auto videoTransform = CGAffineTransformMakeRotation(rotationAngle * M_PI / 180);
    if (videoFrame.isMirrored())
        videoTransform = CGAffineTransformScale(videoTransform, -1, 1);

    return videoTransform;
}

void LocalSampleBufferDisplayLayer::enqueueVideoFrame(VideoFrame& videoFrame)
{
    auto affineTransform = transformationMatrixForVideoFrame(videoFrame);
    if (!CGAffineTransformEqualToTransform(affineTransform, m_affineTransform)) {
        m_affineTransform = affineTransform;
        m_videoFrameRotation = videoFrame.rotation();
        updateSampleLayerBoundsAndPosition({ });
    }

    m_processingQueue->dispatch([this, weakThis = ThreadSafeWeakPtr { *this }, pixelBuffer = RetainPtr { videoFrame.pixelBuffer() }, presentationTime = videoFrame.presentationTime()]() mutable {
        auto protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        assertIsCurrent(workQueue());
        enqueueBufferInternal(pixelBuffer.get(), presentationTime);
    });
}

static const double rendererLatency = 0.02;

void LocalSampleBufferDisplayLayer::enqueueBufferInternal(CVPixelBufferRef pixelBuffer, MediaTime presentationTime)
{
    assertIsCurrent(workQueue());

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
    assertIsCurrent(workQueue());

    callOnMainThread([frameTime = frameTime.secondsSinceEpoch().value(), lastFrameTime = lastFrameTime.secondsSinceEpoch().value(), observedFrameRate = m_frameRateMonitor.observedFrameRate(), frameCount = m_frameRateMonitor.frameCount(), weakThis = ThreadSafeWeakPtr { *this }] {
        auto protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RELEASE_LOG(WebRTC, "LocalSampleBufferDisplayLayer::enqueueVideoFrame (%{public}s) at %f, previous frame was at %f, observed frame rate is %f, delay since last frame is %f ms, frame count is %lu", protectedThis->m_logIdentifier.utf8().data(), frameTime, lastFrameTime, observedFrameRate, (frameTime - lastFrameTime) * 1000, frameCount);
    });
}
#endif

void LocalSampleBufferDisplayLayer::removeOldVideoFramesFromPendingQueue()
{
    assertIsCurrent(workQueue());

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
    assertIsCurrent(workQueue());

    removeOldVideoFramesFromPendingQueue();
    m_pendingVideoFrameQueue.append(WTFMove(videoFrame));
}

void LocalSampleBufferDisplayLayer::clearVideoFrames()
{
    m_processingQueue->dispatch([this, weakThis = ThreadSafeWeakPtr { *this }] {
        auto protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        assertIsCurrent(workQueue());

        m_pendingVideoFrameQueue.clear();
    });
}

void LocalSampleBufferDisplayLayer::requestNotificationWhenReadyForVideoData()
{
    assertIsCurrent(workQueue());

    ThreadSafeWeakPtr weakThis { *this };
    [m_sampleBufferDisplayLayer requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^{
        auto protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        m_processingQueue->dispatch([this, weakThis] {
            auto protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            [m_sampleBufferDisplayLayer stopRequestingMediaData];

            while (!m_pendingVideoFrameQueue.isEmpty()) {
                if (![m_sampleBufferDisplayLayer isReadyForMoreMediaData]) {
                    requestNotificationWhenReadyForVideoData();
                    return;
                }
                auto videoFrame = m_pendingVideoFrameQueue.takeFirst();
                enqueueBufferInternal(videoFrame->pixelBuffer(), videoFrame->presentationTime());
            }
        });
    }];
}

}

#endif
