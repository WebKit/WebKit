/*
 * Copyright (C) 2014-2022 Apple Inc. All rights reserved.
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
#import "WebAVPlayerLayer.h"

#if HAVE(AVKIT)

#import "CaptionUserPreferencesMediaAF.h"
#import "GeometryUtilities.h"
#import "LayoutRect.h"
#import "Logging.h"
#import "VideoPresentationModel.h"
#import "WebAVPlayerController.h"
#import <wtf/LoggerHelper.h>
#import <wtf/RunLoop.h>
#import <wtf/WeakObjCPtr.h>

#import <pal/spi/cocoa/AVKitSPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

using namespace WebCore;

SOFTLINK_AVKIT_FRAMEWORK()
SOFT_LINK_CLASS_OPTIONAL(AVKit, __AVPlayerLayerView)

#if !RELEASE_LOG_DISABLED
@interface WebAVPlayerLayer (Logging)
@property (readonly, nonatomic) uint64_t logIdentifier;
@property (readonly, nonatomic) const Logger* loggerPtr;
@property (readonly, nonatomic) WTFLogChannel* logChannel;
@end
#endif

namespace WebCore {
class WebAVPlayerLayerPresentationModelClient final : public VideoPresentationModelClient, public CanMakeCheckedPtr<WebAVPlayerLayerPresentationModelClient> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(WebAVPlayerLayerPresentationModelClient);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WebAVPlayerLayerPresentationModelClient);
public:
    WebAVPlayerLayerPresentationModelClient(WebAVPlayerLayer* playerLayer)
        : m_playerLayer(playerLayer)
    {
    }
private:
    // CheckedPtr interface
    uint32_t checkedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
    uint32_t checkedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
    void incrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
    void decrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }
    void setDidBeginCheckedPtrDeletion() final { CanMakeCheckedPtr::setDidBeginCheckedPtrDeletion(); }

    void videoDimensionsChanged(const FloatSize& videoDimensions)
    {
        [m_playerLayer.get() setVideoDimensions:videoDimensions];
    }

    WeakObjCPtr<WebAVPlayerLayer> m_playerLayer;
};
}

@implementation WebAVPlayerLayer {
    ThreadSafeWeakPtr<VideoPresentationModel> _presentationModel;
    RetainPtr<WebAVPlayerController> _playerController;
    RetainPtr<CALayer> _videoSublayer;
    RetainPtr<CALayer> _captionsLayer;
    FloatRect _targetVideoFrame;
    CGSize _videoDimensions;
    RetainPtr<NSString> _videoGravity;
    RetainPtr<NSString> _previousVideoGravity;
    std::unique_ptr<WebAVPlayerLayerPresentationModelClient> _presentationModelClient;
    NSEdgeInsets _legibleContentInsets;
    BOOL _showingCaptionPreview;
#if !RELEASE_LOG_DISABLED
    uint64_t _logIdentifier;
#endif
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        self.masksToBounds = YES;
        self.allowsHitTesting = NO;
        _videoGravity = AVLayerVideoGravityResizeAspect;
        _previousVideoGravity = AVLayerVideoGravityResizeAspect;
        self.name = @"WebAVPlayerLayer";
        _presentationModelClient = WTF::makeUnique<WebAVPlayerLayerPresentationModelClient>(self);
        _showingCaptionPreview = NO;
    }
    return self;
}

- (void)dealloc
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(resolveBounds) object:nil];
    [_pixelBufferAttributes release];
    if (auto model = _presentationModel.get())
        model->removeClient(*_presentationModelClient);
    [super dealloc];
}

- (VideoPresentationModel*)presentationModel
{
    return _presentationModel.get().unsafeGet();
}

- (void)setPresentationModel:(VideoPresentationModel*)presentationModel
{
    auto model = _presentationModel.get();
    if (model == presentationModel)
        return;

    if (model)
        model->removeClient(*_presentationModelClient);

    _presentationModel = presentationModel;
#if !RELEASE_LOG_DISABLED
    _logIdentifier = presentationModel ? presentationModel->nextChildIdentifier() : 0;
#endif

    if (presentationModel)
        presentationModel->addClient(*_presentationModelClient);

    self.videoDimensions = presentationModel ? presentationModel->videoDimensions() : CGSizeZero;
}

- (AVPlayerController *)playerController
{
    return (AVPlayerController *)_playerController.get();
}

- (void)setPlayerController:(AVPlayerController *)playerController
{
    ASSERT(!playerController || [playerController isKindOfClass:webAVPlayerControllerClassSingleton()]);
    _playerController = (WebAVPlayerController *)playerController;
}

- (void)setVideoSublayer:(CALayer *)videoSublayer
{
    _videoSublayer = videoSublayer;
}

- (CALayer*)videoSublayer
{
    return _videoSublayer.get();
}

- (void)setCaptionsLayer:(CALayer *)captionsLayer
{
    if (_captionsLayer)
        [_captionsLayer removeFromSuperlayer];

    _captionsLayer = captionsLayer;

    if (_captionsLayer) {
        [self addSublayer:_captionsLayer.get()];
        [self setNeedsLayout];
    }
}

- (CALayer*)captionsLayer
{
    return _captionsLayer.get();
}

- (CGSize)videoDimensions
{
    return _videoDimensions;
}

- (void)setVideoDimensions:(CGSize)videoDimensions
{
    if (CGSizeEqualToSize(_videoDimensions, videoDimensions))
        return;

    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, FloatSize { videoDimensions });
    _videoDimensions = videoDimensions;
    [self setNeedsLayout];
}

- (FloatRect)calculateTargetVideoFrame
{
    FloatRect targetVideoFrame;
    float videoAspectRatio = self.videoDimensions.width / self.videoDimensions.height;

    if ([AVLayerVideoGravityResize isEqualToString:self.videoGravity])
        targetVideoFrame = self.bounds;
    else if ([AVLayerVideoGravityResizeAspect isEqualToString:self.videoGravity])
        targetVideoFrame = largestRectWithAspectRatioInsideRect(videoAspectRatio, self.bounds);
    else if ([AVLayerVideoGravityResizeAspectFill isEqualToString:self.videoGravity])
        targetVideoFrame = smallestRectWithAspectRatioAroundRect(videoAspectRatio, self.bounds);
    else
        ASSERT_NOT_REACHED();

    return snappedIntRect(LayoutRect(targetVideoFrame));
}

// Sometimes the `frame` returned by CA will differ from the value assigned
// by a extremely small amount, but a larger amount than can be accounted for
// by `areEssentiallyEqual`. Because setting self.videoSublayer.frame will
// cause layoutSublayers to be called again, this small difference can cause
// a loop of endless -layoutSublayers/-resolveBounds iterations. Define a
// constant value to use for a custom `areEssentiallyEqual` where we will
// bail out of -resolveBounds if the _targetVideoFrame is essentially equal
// to the current video frame.
static bool areFramesEssentiallyEqualWithTolerance(const FloatRect& a, const FloatRect& b)
{
    static constexpr double frameValueDeltaTolerance { 0.01 };
    FloatRect delta { FloatPoint { a.location() - b.location() }, a.size() - b.size() };
    return abs(delta.x()) < frameValueDeltaTolerance
        && abs(delta.y()) < frameValueDeltaTolerance
        && abs(delta.width()) < frameValueDeltaTolerance
        && abs(delta.height()) < frameValueDeltaTolerance;
};

- (void)layoutSublayers
{
    if ([_videoSublayer superlayer] != self) {
        OBJC_INFO_LOG(OBJC_LOGIDENTIFIER, "videoSublayer is has another superlayer, bailing");
        return;
    }

    if (self.videoDimensions.height <= 0 || self.videoDimensions.width <= 0) {
        OBJC_INFO_LOG(OBJC_LOGIDENTIFIER, "videoDimensions are empty, bailing");
        return;
    }

    FloatRect sourceVideoFrame = self.videoSublayer.bounds;
    _targetVideoFrame = [self calculateTargetVideoFrame];

    if (_captionsLayer) {
        // Captions should be placed atop video content, but if the video content overflows
        // the WebAVPlayerLayer bounds, restrict the caption area to only what is visible.
        FloatRect captionsFrame = _targetVideoFrame;
        captionsFrame.intersect(self.bounds);
        [_captionsLayer setFrame:captionsFrame];
        if (auto model = _presentationModel.get())
            model->setTextTrackRepresentationBounds(enclosingIntRect(captionsFrame));
    }

    float videoAspectRatio = self.videoDimensions.width / self.videoDimensions.height;

    if ([AVLayerVideoGravityResizeAspect isEqualToString:_previousVideoGravity.get()])
        sourceVideoFrame = largestRectWithAspectRatioInsideRect(videoAspectRatio, sourceVideoFrame);
    else if ([AVLayerVideoGravityResizeAspectFill isEqualToString:_previousVideoGravity.get()])
        sourceVideoFrame = smallestRectWithAspectRatioAroundRect(videoAspectRatio, sourceVideoFrame);
    else
        ASSERT([AVLayerVideoGravityResize isEqualToString:_previousVideoGravity.get()]);

    if (sourceVideoFrame.isEmpty()) {
        // The initial resize will have an empty videoLayerFrame, which makes
        // the subsequent calculations incorrect. When this happens, just do
        // the synchronous resize step instead.
        OBJC_INFO_LOG(OBJC_LOGIDENTIFIER, "sourceVideoFrame is empty; calling -resolveBounds");
        [self resolveBounds];
        return;
    }

    if (sourceVideoFrame == _targetVideoFrame && CGAffineTransformIsIdentity([_videoSublayer affineTransform])) {
        OBJC_DEBUG_LOG(OBJC_LOGIDENTIFIER, "targetVideoFrame (", _targetVideoFrame, ") is equal to sourceVideoFrame, and affineTransform is identity, bailing");
        return;
    }

    // CALayer -frame will take the -position, -bounds, and -affineTransform into account,
    // so bail out if the current -frame is essentially equal to the targetVideoFrame.
    if (areFramesEssentiallyEqualWithTolerance(self.videoSublayer.frame, _targetVideoFrame)) {
        OBJC_DEBUG_LOG(OBJC_LOGIDENTIFIER, "targetVideoFrame (", _targetVideoFrame, ") is essentially equal to videoSublayer.frame, bailing");
        return;
    }

    CGAffineTransform transform = CGAffineTransformMakeScale(_targetVideoFrame.width() / sourceVideoFrame.width(), _targetVideoFrame.height() / sourceVideoFrame.height());

    [CATransaction begin];
    [_videoSublayer setAnchorPoint:CGPointMake(0.5, 0.5)];
    [_videoSublayer setAffineTransform:transform];
    [_videoSublayer setPosition:CGPointMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds))];
    [CATransaction commit];

    OBJC_DEBUG_LOG(OBJC_LOGIDENTIFIER, "self.bounds: ", FloatRect(self.bounds), ", targetVideoFrame: ", _targetVideoFrame, ", transform: [", transform.a, ", ", transform.d, "]");

    NSTimeInterval animationDuration = [CATransaction animationDuration];
    RunLoop::mainSingleton().dispatch([self, strongSelf = retainPtr(self), animationDuration] {
        [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(resolveBounds) object:nil];
        [self performSelector:@selector(resolveBounds) withObject:nil afterDelay:animationDuration + 0.1];
    });
}

- (void)resolveBounds
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(resolveBounds) object:nil];

    if ([_videoSublayer superlayer] != self) {
        OBJC_ERROR_LOG(OBJC_LOGIDENTIFIER, "videoSublayer is has another superlayer, bailing");
        return;
    }

    if (areFramesEssentiallyEqualWithTolerance(self.videoSublayer.frame, _targetVideoFrame) && CGAffineTransformIsIdentity([_videoSublayer affineTransform])) {
        OBJC_INFO_LOG(OBJC_LOGIDENTIFIER, "targetVideoFrame (", _targetVideoFrame, ") is equal to videoSublayer.bounds, and affineTransform is identity, bailing");
        return;
    }

    [CATransaction begin];
    [CATransaction setAnimationDuration:0];
    [CATransaction setDisableActions:YES];

    OBJC_DEBUG_LOG(OBJC_LOGIDENTIFIER, _targetVideoFrame);

    if (auto model = _presentationModel.get()) {
        FloatRect targetVideoBounds { { }, _targetVideoFrame.size() };
        model->setVideoLayerFrame(targetVideoBounds);
    }

    _previousVideoGravity = _videoGravity;

    [_videoSublayer setAnchorPoint:CGPointMake(0.5, 0.5)];
    [_videoSublayer setAffineTransform:CGAffineTransformIdentity];
    [_videoSublayer setFrame:_targetVideoFrame];

    [CATransaction commit];
}

- (void)setVideoGravity:(NSString *)videoGravity
{
#if PLATFORM(MACCATALYST)
    // FIXME<rdar://46011230>: remove this #if once this radar lands.
    if (!videoGravity)
        videoGravity = AVLayerVideoGravityResizeAspect;
#endif

    if ([_videoGravity isEqualToString:videoGravity])
        return;

    _previousVideoGravity = _videoGravity;
    _videoGravity = videoGravity;

    MediaPlayerEnums::VideoGravity gravity = MediaPlayerEnums::VideoGravity::ResizeAspect;
    if ([videoGravity isEqualToString:AVLayerVideoGravityResize])
        gravity = MediaPlayerEnums::VideoGravity::Resize;
    else if ([videoGravity isEqualToString:AVLayerVideoGravityResizeAspect])
        gravity = MediaPlayerEnums::VideoGravity::ResizeAspect;
    else if ([videoGravity isEqualToString:AVLayerVideoGravityResizeAspectFill])
        gravity = MediaPlayerEnums::VideoGravity::ResizeAspectFill;
    else
        ASSERT_NOT_REACHED();

    OBJC_INFO_LOG(OBJC_LOGIDENTIFIER, videoGravity);

    if (auto model = _presentationModel.get())
        model->setVideoLayerGravity(gravity);

    [self setNeedsLayout];
}

- (NSString *)videoGravity
{
    return _videoGravity.get();
}

- (CGRect)videoRect
{
    if (self.videoDimensions.width <= 0 || self.videoDimensions.height <= 0)
        return self.bounds;

    float videoAspectRatio = self.videoDimensions.width / self.videoDimensions.height;

    if ([AVLayerVideoGravityResizeAspect isEqualToString:self.videoGravity])
        return largestRectWithAspectRatioInsideRect(videoAspectRatio, self.bounds);
    if ([AVLayerVideoGravityResizeAspectFill isEqualToString:self.videoGravity])
        return smallestRectWithAspectRatioAroundRect(videoAspectRatio, self.bounds);

    return self.bounds;
}

+ (NSSet *)keyPathsForValuesAffectingVideoRect
{
    return [NSSet setWithObjects:@"videoDimensions", @"videoGravity", nil];
}

- (NSEdgeInsets)legibleContentInsets
{
    return _legibleContentInsets;
}

- (void)setLegibleContentInsets:(NSEdgeInsets)legibleContentInsets
{
    _legibleContentInsets = legibleContentInsets;
}

- (BOOL)showingCaptionPreview
{
    return _showingCaptionPreview;
}

#pragma mark - Caption Preview Support

- (void)setCaptionPreviewProfileID:(NSString * _Nonnull)profileID position:(CGPoint)position text:(nullable NSString *)text
{
    CaptionUserPreferencesMediaAF::setActiveProfileID(profileID);

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
    if (RefPtr model = _presentationModel.get())
        model->requestShowCaptionDisplaySettingsPreview();
#endif

    _showingCaptionPreview = YES;
}

- (void)stopShowingCaptionPreview
{
    OBJC_INFO_LOG(OBJC_LOGIDENTIFIER);

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
    if (RefPtr model = _presentationModel.get())
        model->requestHideCaptionDisplaySettingsPreview();
#endif

    _showingCaptionPreview = NO;
}

#if PLATFORM(APPLETV)
- (BOOL)avkit_isVisible
{
    return !CGRectIsEmpty(self.bounds);
}

- (CGRect)avkit_videoRectInWindow
{
    return self.videoRect;
}
#endif

@end

#if !RELEASE_LOG_DISABLED
@implementation WebAVPlayerLayer (Logging)
- (uint64_t)logIdentifier
{
    return _logIdentifier;
}

- (const Logger*)loggerPtr
{
    if (auto model = _presentationModel.get())
        return model->loggerPtr();
    return nullptr;
}

- (WTFLogChannel*)logChannel
{
    return &LogFullscreen;
}
@end
#endif

#endif // HAVE(AVKIT)
