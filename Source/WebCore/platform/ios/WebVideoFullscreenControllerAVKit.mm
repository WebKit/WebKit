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
#import "WebVideoFullscreenModelMediaElement.h"
#import <QuartzCore/CoreAnimation.h>
#import <WebCore/WebCoreThreadRun.h>

using namespace WebCore;

#if __IPHONE_OS_VERSION_MIN_REQUIRED < 80000

@implementation WebVideoFullscreenController
- (void)setMediaElement:(WebCore::HTMLMediaElement*)mediaElement
{
    UNUSED_PARAM(mediaElement);
}

- (WebCore::HTMLMediaElement*)mediaElement
{
    return nullptr;
}

- (void)enterFullscreen:(UIView *)view
{
    UNUSED_PARAM(view);
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
@end

class WebVideoFullscreenControllerChangeObserver : public WebVideoFullscreenChangeObserver {
    WebVideoFullscreenController* _target;
public:
    void setTarget(WebVideoFullscreenController* target) { _target = target; }
    virtual void didSetupFullscreen() override { [_target didSetupFullscreen]; }
    virtual void didEnterFullscreen() override { [_target didEnterFullscreen]; }
    virtual void didExitFullscreen() override { [_target didExitFullscreen]; }
    virtual void didCleanupFullscreen() override { [_target didCleanupFullscreen]; }
};

@implementation WebVideoFullscreenController
{
    RefPtr<HTMLMediaElement> _mediaElement;
    RefPtr<WebVideoFullscreenInterfaceAVKit> _interface;
    RefPtr<WebVideoFullscreenModelMediaElement> _model;
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
    _mediaElement = nullptr;
    [super dealloc];
}

- (void)setMediaElement:(HTMLMediaElement*)mediaElement
{
    _mediaElement = mediaElement;
}

- (HTMLMediaElement*)mediaElement
{
    return _mediaElement.get();
}

- (void)enterFullscreen:(UIView *)view
{
    [self retain]; // Balanced by -release in didExitFullscreen:
    
    _interface = adoptRef(new WebVideoFullscreenInterfaceAVKit);
    _interface->setWebVideoFullscreenChangeObserver(&_changeObserver);
    _model = adoptRef(new WebVideoFullscreenModelMediaElement);
    _model->setWebVideoFullscreenInterface(_interface.get());
    _interface->setWebVideoFullscreenModel(_model.get());
    _model->setMediaElement(_mediaElement.get());
    _videoFullscreenLayer = [CALayer layer];
    _interface->setupFullscreen(*_videoFullscreenLayer.get(), _mediaElement->clientRect(), view);
}

- (void)exitFullscreen
{
    _interface->exitFullscreen(_mediaElement->screenRect());
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
        _model->setMediaElement(nullptr);
        _interface->setWebVideoFullscreenChangeObserver(nullptr);
        _model = nullptr;
        _interface = nullptr;
        
        [self release]; // Balance the -retain we did in enterFullscreen:
    });
}

@end

#endif // __IPHONE_OS_VERSION_MIN_REQUIRED < 80000

#endif // PLATFORM(IOS)
