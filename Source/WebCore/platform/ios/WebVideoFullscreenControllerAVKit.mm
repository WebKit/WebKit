/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS)

#import "WebVideoFullscreenControllerAVKit.h"

#import "Logging.h"
#import "QuartzCoreSPI.h"
#import "SoftLinking.h"
#import "TimeRanges.h"
#import "WebVideoFullscreenChangeObserver.h"
#import "WebVideoFullscreenInterfaceAVKit.h"
#import "WebVideoFullscreenModelVideoElement.h"
#import <QuartzCore/CoreAnimation.h>
#import <WebCore/FrameView.h>
#import <WebCore/HTMLVideoElement.h>
#import <WebCore/RenderElement.h>
#import <WebCore/WebCoreThreadRun.h>

SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK_CLASS(UIKit, UIView)

using namespace WebCore;

#if !HAVE(AVKIT)

@implementation WebVideoFullscreenController
- (void)setVideoElement:(WebCore::HTMLVideoElement*)videoElement
{
    UNUSED_PARAM(videoElement);
}

- (WebCore::HTMLVideoElement*)videoElement
{
    return nullptr;
}

- (void)enterFullscreen:(UIView *)view mode:(WebCore::HTMLMediaElementEnums::VideoFullscreenMode)mode
{
    UNUSED_PARAM(view);
    UNUSED_PARAM(mode);
}

- (void)requestHideAndExitFullscreen
{
}

- (void)exitFullscreen
{
}
@end

#else

static IntRect elementRectInWindow(HTMLVideoElement* videoElement)
{
    if (!videoElement || !videoElement->renderer() || !videoElement->document().view())
        return IntRect();
    
    return videoElement->document().view()->convertToContainingWindow(videoElement->renderer()->absoluteBoundingBoxRect());
}

class WebVideoFullscreenControllerContext;

@interface WebVideoFullscreenController (delegate)
-(void)didFinishFullscreen:(WebVideoFullscreenControllerContext*)context;
@end

class WebVideoFullscreenControllerContext final
    : private WebVideoFullscreenInterface
    , private WebVideoFullscreenModel
    , private WebVideoFullscreenChangeObserver
    , public ThreadSafeRefCounted<WebVideoFullscreenControllerContext> {

public:
    static Ref<WebVideoFullscreenControllerContext> create()
    {
        return adoptRef(*new WebVideoFullscreenControllerContext);
    }
    
    void setController(WebVideoFullscreenController* controller) { m_controller = controller; }
    void setUpFullscreen(HTMLVideoElement&, UIView *, HTMLMediaElementEnums::VideoFullscreenMode);
    void exitFullscreen();
    void requestHideAndExitFullscreen();
    void invalidate();

private:
    WebVideoFullscreenControllerContext() { }

    // WebVideoFullscreenChangeObserver
    void didSetupFullscreen() override;
    void didEnterFullscreen() override { }
    void didExitFullscreen() override;
    void didCleanupFullscreen() override;
    void fullscreenMayReturnToInline() override;
    
    // WebVideoFullscreenInterface
    void resetMediaState() override;
    void setDuration(double) override;
    void setCurrentTime(double currentTime, double anchorTime) override;
    void setBufferedTime(double) override;
    void setRate(bool isPlaying, float playbackRate) override;
    void setVideoDimensions(bool hasVideo, float width, float height) override;
    void setSeekableRanges(const TimeRanges&) override;
    void setCanPlayFastReverse(bool) override;
    void setAudioMediaSelectionOptions(const Vector<String>& options, uint64_t selectedIndex) override;
    void setLegibleMediaSelectionOptions(const Vector<String>& options, uint64_t selectedIndex) override;
    void setExternalPlayback(bool enabled, ExternalPlaybackTargetType, String localizedDeviceName) override;
    void setWirelessVideoPlaybackDisabled(bool) override;

    // WebVideoFullscreenModel
    void play() override;
    void pause() override;
    void togglePlayState() override;
    void beginScrubbing() override;
    void endScrubbing() override;
    void seekToTime(double time) override;
    void fastSeek(double time) override;
    void beginScanningForward() override;
    void beginScanningBackward() override;
    void endScanning() override;
    void requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenMode) override;
    void setVideoLayerFrame(FloatRect) override;
    void setVideoLayerGravity(WebVideoFullscreenModel::VideoGravity) override;
    void selectAudioMediaOption(uint64_t index) override;
    void selectLegibleMediaOption(uint64_t index) override;
    void fullscreenModeChanged(HTMLMediaElementEnums::VideoFullscreenMode) override;
    bool isVisible() const override;

    RefPtr<WebVideoFullscreenInterfaceAVKit> m_interface;
    RefPtr<WebVideoFullscreenModelVideoElement> m_model;
    RefPtr<HTMLVideoElement> m_videoElement;
    RetainPtr<UIView> m_videoFullscreenView;
    RetainPtr<WebVideoFullscreenController> m_controller;
};

#pragma mark WebVideoFullscreenChangeObserver

void WebVideoFullscreenControllerContext::didSetupFullscreen()
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    RetainPtr<CALayer> videoFullscreenLayer = [m_videoFullscreenView layer];
    WebThreadRun([strongThis, this, videoFullscreenLayer] {
        [videoFullscreenLayer setBackgroundColor:cachedCGColor(WebCore::Color::transparent)];
        m_model->setVideoFullscreenLayer(videoFullscreenLayer.get());
        dispatch_async(dispatch_get_main_queue(), [strongThis, this] {
            m_interface->enterFullscreen();
        });
    });
}

void WebVideoFullscreenControllerContext::didExitFullscreen()
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    WebThreadRun([strongThis, this] {
        m_model->setVideoFullscreenLayer(nil);
        dispatch_async(dispatch_get_main_queue(), [strongThis, this] {
            m_interface->cleanupFullscreen();
        });
    });
}

void WebVideoFullscreenControllerContext::didCleanupFullscreen()
{
    ASSERT(isUIThread());
    m_interface->setWebVideoFullscreenModel(nullptr);
    m_interface->setWebVideoFullscreenChangeObserver(nullptr);
    m_interface = nullptr;
    m_videoFullscreenView = nil;
    
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    WebThreadRun([strongThis, this] {
        m_model->setVideoFullscreenLayer(nil);
        m_model->setWebVideoFullscreenInterface(nullptr);
        m_model->setVideoElement(nullptr);
        m_model = nullptr;
        m_videoElement = nullptr;
        
        [m_controller didFinishFullscreen:this];
    });
}

void WebVideoFullscreenControllerContext::fullscreenMayReturnToInline()
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    WebThreadRun([strongThis, this] {
        IntRect clientRect = elementRectInWindow(m_videoElement.get());
        dispatch_async(dispatch_get_main_queue(), [strongThis, this, clientRect] {
            m_interface->preparedToReturnToInline(true, clientRect);
        });
    });
}

#pragma mark WebVideoFullscreenInterface

void WebVideoFullscreenControllerContext::resetMediaState()
{
    ASSERT(WebThreadIsCurrent() || isMainThread());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    dispatch_async(dispatch_get_main_queue(), [strongThis, this] {
        if (m_interface)
            m_interface->resetMediaState();
    });
}

void WebVideoFullscreenControllerContext::setDuration(double duration)
{
    ASSERT(WebThreadIsCurrent());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    dispatch_async(dispatch_get_main_queue(), [strongThis, this, duration] {
        if (m_interface)
            m_interface->setDuration(duration);
    });
}

void WebVideoFullscreenControllerContext::setCurrentTime(double currentTime, double anchorTime)
{
    ASSERT(WebThreadIsCurrent());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    dispatch_async(dispatch_get_main_queue(), [strongThis, this, currentTime, anchorTime] {
        if (m_interface)
            m_interface->setCurrentTime(currentTime, anchorTime);
    });
}

void WebVideoFullscreenControllerContext::setBufferedTime(double bufferedTime)
{
    ASSERT(WebThreadIsCurrent());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    dispatch_async(dispatch_get_main_queue(), [strongThis, this, bufferedTime] {
        if (m_interface)
            m_interface->setBufferedTime(bufferedTime);
    });
}

void WebVideoFullscreenControllerContext::setRate(bool isPlaying, float playbackRate)
{
    ASSERT(WebThreadIsCurrent());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    dispatch_async(dispatch_get_main_queue(), [strongThis, this, isPlaying, playbackRate] {
        if (m_interface)
            m_interface->setRate(isPlaying, playbackRate);
    });
}

void WebVideoFullscreenControllerContext::setVideoDimensions(bool hasVideo, float width, float height)
{
    ASSERT(WebThreadIsCurrent());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    dispatch_async(dispatch_get_main_queue(), [strongThis, this, hasVideo, width, height] {
        if (m_interface)
            m_interface->setVideoDimensions(hasVideo, width, height);
    });
}

void WebVideoFullscreenControllerContext::setSeekableRanges(const TimeRanges& timeRanges)
{
    ASSERT(WebThreadIsCurrent());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    const PlatformTimeRanges& platformTimeRanges = timeRanges.ranges();
    dispatch_async(dispatch_get_main_queue(), [strongThis, this, platformTimeRanges] {
        if (m_interface)
            m_interface->setSeekableRanges(TimeRanges::create(platformTimeRanges));
    });
}

void WebVideoFullscreenControllerContext::setCanPlayFastReverse(bool canPlayFastReverse)
{
    ASSERT(WebThreadIsCurrent());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    dispatch_async(dispatch_get_main_queue(), [strongThis, this, canPlayFastReverse] {
        if (m_interface)
            m_interface->setCanPlayFastReverse(canPlayFastReverse);
    });
}

void WebVideoFullscreenControllerContext::setAudioMediaSelectionOptions(const Vector<String>& options, uint64_t selectedIndex)
{
    ASSERT(WebThreadIsCurrent());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);

    RetainPtr<NSMutableArray> optionsArray = adoptNS([[NSMutableArray alloc] initWithCapacity:options.size()]);
    for (auto& name : options)
        [optionsArray addObject:name];
    
    dispatch_async(dispatch_get_main_queue(), [strongThis, this, optionsArray, selectedIndex] {
        Vector<String> options;
        for (NSString *name : optionsArray.get())
            options.append(name);
        
        if (m_interface)
            m_interface->setAudioMediaSelectionOptions(options, selectedIndex);
    });
}

void WebVideoFullscreenControllerContext::setLegibleMediaSelectionOptions(const Vector<String>& options, uint64_t selectedIndex)
{
    ASSERT(WebThreadIsCurrent());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    
    RetainPtr<NSMutableArray> optionsArray = adoptNS([[NSMutableArray alloc] initWithCapacity:options.size()]);
    for (auto& name : options)
        [optionsArray addObject:name];
    
    dispatch_async(dispatch_get_main_queue(), [strongThis, this, optionsArray, selectedIndex] {
        Vector<String> options;
        for (NSString *name : optionsArray.get())
            options.append(name);
        
        if (m_interface)
            m_interface->setLegibleMediaSelectionOptions(options, selectedIndex);
    });
}

void WebVideoFullscreenControllerContext::setExternalPlayback(bool enabled, ExternalPlaybackTargetType type, String localizedDeviceName)
{
    ASSERT(WebThreadIsCurrent());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    StringCapture capturedLocalizedDeviceName(localizedDeviceName);
    dispatch_async(dispatch_get_main_queue(), [strongThis, this, enabled, type, capturedLocalizedDeviceName] {
        if (m_interface)
            m_interface->setExternalPlayback(enabled, type, capturedLocalizedDeviceName.string());
    });
}

void WebVideoFullscreenControllerContext::setWirelessVideoPlaybackDisabled(bool disabled)
{
    ASSERT(WebThreadIsCurrent());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    dispatch_async(dispatch_get_main_queue(), [strongThis, this, disabled] {
        if (m_interface)
            m_interface->setWirelessVideoPlaybackDisabled(disabled);
    });
}

#pragma mark WebVideoFullscreenModel

void WebVideoFullscreenControllerContext::play()
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    WebThreadRun([strongThis, this] {
        if (m_model)
            m_model->play();
    });
}

void WebVideoFullscreenControllerContext::pause()
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    WebThreadRun([strongThis, this] {
        if (m_model)
            m_model->pause();
    });
}

void WebVideoFullscreenControllerContext::togglePlayState()
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    WebThreadRun([strongThis, this] {
        if (m_model)
            m_model->togglePlayState();
    });
}

void WebVideoFullscreenControllerContext::beginScrubbing()
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    WebThreadRun([strongThis, this] {
        if (m_model)
            m_model->beginScrubbing();
    });
}

void WebVideoFullscreenControllerContext::endScrubbing()
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    WebThreadRun([strongThis, this] {
        if (m_model)
            m_model->endScrubbing();
    });
}

void WebVideoFullscreenControllerContext::seekToTime(double time)
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    WebThreadRun([strongThis, this, time] {
        if (m_model)
            m_model->seekToTime(time);
    });
}

void WebVideoFullscreenControllerContext::fastSeek(double time)
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    WebThreadRun([strongThis, this, time] {
        if (m_model)
            m_model->fastSeek(time);
    });
}

void WebVideoFullscreenControllerContext::beginScanningForward()
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    WebThreadRun([strongThis, this] {
        if (m_model)
            m_model->beginScanningForward();
    });
}

void WebVideoFullscreenControllerContext::beginScanningBackward()
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    WebThreadRun([strongThis, this] {
        if (m_model)
            m_model->beginScanningBackward();
    });
}

void WebVideoFullscreenControllerContext::endScanning()
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    WebThreadRun([strongThis, this] {
        if (m_model)
            m_model->endScanning();
    });
}

void WebVideoFullscreenControllerContext::requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    WebThreadRun([strongThis, this, mode] {
        if (m_model)
            m_model->requestFullscreenMode(mode);
    });
}

void WebVideoFullscreenControllerContext::setVideoLayerFrame(FloatRect frame)
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    RetainPtr<CALayer> videoFullscreenLayer = [m_videoFullscreenView layer];
    
    [videoFullscreenLayer setSublayerTransform:[videoFullscreenLayer transform]];

    dispatch_async(dispatch_get_main_queue(), ^{
        WebThreadRun([strongThis, this, frame, videoFullscreenLayer] {
            [CATransaction begin];
            [CATransaction setDisableActions:YES];
            [CATransaction setAnimationDuration:0];
            
            [videoFullscreenLayer setSublayerTransform:CATransform3DIdentity];
            
            if (m_model)
                m_model->setVideoLayerFrame(frame);
            [CATransaction commit];
        });
    });
}

void WebVideoFullscreenControllerContext::setVideoLayerGravity(WebVideoFullscreenModel::VideoGravity videoGravity)
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    WebThreadRun([strongThis, this, videoGravity] {
        if (m_model)
            m_model->setVideoLayerGravity(videoGravity);
    });
}

void WebVideoFullscreenControllerContext::selectAudioMediaOption(uint64_t index)
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    WebThreadRun([strongThis, this, index] {
        if (m_model)
            m_model->selectAudioMediaOption(index);
    });
}

void WebVideoFullscreenControllerContext::selectLegibleMediaOption(uint64_t index)
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    WebThreadRun([strongThis, this, index] {
        if (m_model)
            m_model->selectLegibleMediaOption(index);
    });
}

void WebVideoFullscreenControllerContext::fullscreenModeChanged(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    WebThreadRun([strongThis, this, mode] {
        if (m_model)
            m_model->fullscreenModeChanged(mode);
    });
}

bool WebVideoFullscreenControllerContext::isVisible() const
{
    ASSERT(isUIThread());
    return m_model ? m_model->isVisible() : false;
}

#pragma mark Other

void WebVideoFullscreenControllerContext::setUpFullscreen(HTMLVideoElement& videoElement, UIView *view, HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    ASSERT(isMainThread());
    RetainPtr<UIView> viewRef = view;
    m_videoElement = &videoElement;

    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    dispatch_async(dispatch_get_main_queue(), [strongThis, this, viewRef, mode] {
        ASSERT(isUIThread());

        m_interface = WebVideoFullscreenInterfaceAVKit::create();
        m_interface->setWebVideoFullscreenChangeObserver(this);
        m_interface->setWebVideoFullscreenModel(this);
        m_videoFullscreenView = adoptNS([[getUIViewClass() alloc] init]);
        
        RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
        WebThreadRun([strongThis, this, viewRef, mode] {
            m_model = WebVideoFullscreenModelVideoElement::create();
            m_model->setWebVideoFullscreenInterface(this);
            m_model->setVideoElement(m_videoElement.get());
            
            bool allowsPictureInPicture = m_videoElement->mediaSession().allowsPictureInPicture(*m_videoElement.get());

            IntRect videoElementClientRect = elementRectInWindow(m_videoElement.get());
            FloatRect videoLayerFrame = FloatRect(FloatPoint(), videoElementClientRect.size());
            m_model->setVideoLayerFrame(videoLayerFrame);
            
            dispatch_async(dispatch_get_main_queue(), [strongThis, this, videoElementClientRect, viewRef, mode, allowsPictureInPicture] {
                m_interface->setupFullscreen(*m_videoFullscreenView.get(), videoElementClientRect, viewRef.get(), mode, allowsPictureInPicture);
            });
        });
    });
}

void WebVideoFullscreenControllerContext::exitFullscreen()
{
    ASSERT(WebThreadIsCurrent() || isMainThread());
    IntRect clientRect = elementRectInWindow(m_videoElement.get());
    RefPtr<WebVideoFullscreenControllerContext> strongThis(this);
    dispatch_async(dispatch_get_main_queue(), [strongThis, this, clientRect] {
        ASSERT(isUIThread());
        m_interface->exitFullscreen(clientRect);
    });
}

void WebVideoFullscreenControllerContext::requestHideAndExitFullscreen()
{
    ASSERT(isUIThread());
    m_interface->requestHideAndExitFullscreen();
}

@implementation WebVideoFullscreenController {
    RefPtr<WebVideoFullscreenControllerContext> _context;
    RefPtr<HTMLVideoElement> _videoElement;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;
    
    return self;
}

- (void)setVideoElement:(HTMLVideoElement*)videoElement
{
    _videoElement = videoElement;
}

- (HTMLVideoElement*)videoElement
{
    return _videoElement.get();
}

- (void)enterFullscreen:(UIView *)view mode:(HTMLMediaElementEnums::VideoFullscreenMode)mode
{
    ASSERT(isMainThread());
    _context = WebVideoFullscreenControllerContext::create();
    _context->setController(self);
    _context->setUpFullscreen(*_videoElement.get(), view, mode);
}

- (void)exitFullscreen
{
    ASSERT(WebThreadIsCurrent() || isMainThread());
    _context->exitFullscreen();
}

- (void)requestHideAndExitFullscreen
{
    ASSERT(isUIThread());
    if (_context)
        _context->requestHideAndExitFullscreen();
}

- (void)didFinishFullscreen:(WebVideoFullscreenControllerContext*)context
{
    ASSERT(WebThreadIsCurrent());
    ASSERT_UNUSED(context, context == _context);
    [[self retain] autorelease]; // retain self before breaking a retain cycle.
    _context->setController(nil);
    _context = nullptr;
    _videoElement = nullptr;
}

@end

#endif // !HAVE(AVKIT)

#endif // PLATFORM(IOS)
