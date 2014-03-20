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

@interface WebVideoFullscreenController (FullscreenObservation)
- (void)didEnterFullscreen;
- (void)didExitFullscreen;
@end

class WebVideoFullscreenControllerChangeObserver : public WebVideoFullscreenChangeObserver {
    WebVideoFullscreenController* _target;
public:
    void setTarget(WebVideoFullscreenController* target) { _target = target; }
    virtual void didEnterFullscreen() override { [_target didEnterFullscreen]; }
    virtual void didExitFullscreen() override { [_target didExitFullscreen]; }
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

- (void)enterFullscreen:(UIScreen *)screen
{
    [self retain]; // Balanced by -release in didExitFullscreen:
    
    UNUSED_PARAM(screen);
    _videoFullscreenLayer = [CALayer layer];
    _interface = adoptRef(new WebVideoFullscreenInterfaceAVKit);
    _interface->setWebVideoFullscreenChangeObserver(&_changeObserver);
    _model = adoptRef(new WebVideoFullscreenModelMediaElement);
    _model->setWebVideoFullscreenInterface(_interface.get());
    _interface->setWebVideoFullscreenModel(_model.get());
    _model->setMediaElement(_mediaElement.get());
    _model->setVideoFullscreenLayer(_videoFullscreenLayer.get());
    _interface->enterFullscreen(*_videoFullscreenLayer.get());
}

- (void)exitFullscreen
{
    _interface->exitFullscreen();
}

- (void)didEnterFullscreen
{
}

- (void)didExitFullscreen
{
    WebThreadRun(^{
        _interface->setWebVideoFullscreenModel(nullptr);
        _model->setWebVideoFullscreenInterface(nullptr);
        _model->setVideoFullscreenLayer(nullptr);
        _model->setMediaElement(nullptr);
        _interface->setWebVideoFullscreenChangeObserver(nullptr);
        _model = nullptr;
        _interface = nullptr;
        
        [self release]; // Balance the -retain we did in enterFullscreen:
    });
}

@end

#endif
