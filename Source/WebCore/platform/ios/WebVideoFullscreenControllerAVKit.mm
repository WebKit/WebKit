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
#import "WebVideoFullscreenInterfaceAVKit.h"
#import "WebVideoFullscreenModelVideoElement.h"
#import <QuartzCore/CoreAnimation.h>
#import <WebCore/WebCoreThreadRun.h>

using namespace WebCore;

#if __IPHONE_OS_VERSION_MIN_REQUIRED <= 80200

@implementation WebVideoFullscreenController
- (void)setVideoElement:(WebCore::HTMLVideoElement*)videoElement
{
    UNUSED_PARAM(videoElement);
}

- (WebCore::HTMLVideoElement*)videoElement
{
    return nullptr;
}

- (void)enterFullscreen:(UIView *)view mode:(WebCore::HTMLMediaElement::VideoFullscreenMode)mode
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

@interface WebVideoFullscreenController (FullscreenObservation)
- (void)didSetupFullscreen;
- (void)didEnterFullscreen;
- (void)didExitFullscreen;
- (void)didCleanupFullscreen;
- (void)fullscreenMayReturnToInline;
@end

class WebVideoFullscreenControllerChangeObserver : public WebVideoFullscreenChangeObserver {
    WebVideoFullscreenController* _target;
public:
    void setTarget(WebVideoFullscreenController* target) { _target = target; }
    virtual void didSetupFullscreen() override { [_target didSetupFullscreen]; }
    virtual void didEnterFullscreen() override { [_target didEnterFullscreen]; }
    virtual void didExitFullscreen() override { [_target didExitFullscreen]; }
    virtual void didCleanupFullscreen() override { [_target didCleanupFullscreen]; }
    virtual void fullscreenMayReturnToInline() override { [_target fullscreenMayReturnToInline]; }
};

@implementation WebVideoFullscreenController
{
    RefPtr<HTMLVideoElement> _videoElement;
    RefPtr<WebVideoFullscreenInterfaceAVKit> _interface;
    RefPtr<WebVideoFullscreenModelVideoElement> _model;
    WebVideoFullscreenControllerChangeObserver _changeObserver;
    RetainPtr<PlatformLayer> _videoFullscreenLayer;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;
    
    _changeObserver.setTarget(self);

    return self;
}

- (void)dealloc
{
    _videoElement = nullptr;
    [super dealloc];
}

- (void)setVideoElement:(HTMLVideoElement*)videoElement
{
    _videoElement = videoElement;
}

- (HTMLVideoElement*)videoElement
{
    return _videoElement.get();
}

- (void)enterFullscreen:(UIView *)view mode:(HTMLMediaElement::VideoFullscreenMode)mode
{
    [self retain]; // Balanced by -release in didExitFullscreen:
    
    _interface = adoptRef(new WebVideoFullscreenInterfaceAVKit);
    _interface->setWebVideoFullscreenChangeObserver(&_changeObserver);
    _model = adoptRef(new WebVideoFullscreenModelVideoElement);
    _model->setWebVideoFullscreenInterface(_interface.get());
    _interface->setWebVideoFullscreenModel(_model.get());
    _model->setVideoElement(_videoElement.get());
    _videoFullscreenLayer = [CALayer layer];
    _interface->setupFullscreen(*_videoFullscreenLayer.get(), _videoElement->clientRect(), view, mode, _videoElement->mediaSession().allowsAlternateFullscreen(*_videoElement.get()));
}

- (void)exitFullscreen
{
    _interface->exitFullscreen(_videoElement->screenRect());
}

- (void)requestHideAndExitFullscreen
{
    if (_interface)
        _interface->requestHideAndExitFullscreen();
}

- (void)didSetupFullscreen
{
    WebThreadRun(^{
        _model->setVideoFullscreenLayer(_videoFullscreenLayer.get());
        _interface->enterFullscreen();
    });
}

- (void)didEnterFullscreen
{
}

- (void)didExitFullscreen
{
    WebThreadRun(^{
        _model->setVideoFullscreenLayer(nil);
        _interface->cleanupFullscreen();
    });
}

- (void)didCleanupFullscreen
{
    WebThreadRun(^{
        _model->setVideoFullscreenLayer(nil);
        _interface->setWebVideoFullscreenModel(nullptr);
        _model->setWebVideoFullscreenInterface(nullptr);
        _model->setVideoElement(nullptr);
        _interface->setWebVideoFullscreenChangeObserver(nullptr);
        _model = nullptr;
        _interface = nullptr;
        
        [self release]; // Balance the -retain we did in enterFullscreen:
    });
}

- (void)fullscreenMayReturnToInline
{
    _interface->preparedToReturnToInline(true, _videoElement->clientRect());
}

@end

#endif // __IPHONE_OS_VERSION_MIN_REQUIRED < 80000

#endif // PLATFORM(IOS)
