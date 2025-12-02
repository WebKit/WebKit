/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebAVPlayerLayerView.h"

#if PLATFORM(IOS_FAMILY) && HAVE(AVKIT)

#import "WebAVPlayerLayer.h"
#import <objc/message.h>
#import <objc/runtime.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>
#import <pal/ios/UIKitSoftLink.h>

#if HAVE(PICTUREINPICTUREPLAYERLAYERVIEW)
static NSString * const pictureInPicturePlayerLayerViewKey = @"_pictureInPicturePlayerLayerView";
#endif

SOFTLINK_AVKIT_FRAMEWORK()
SOFT_LINK_CLASS_OPTIONAL(AVKit, __AVPlayerLayerView)

namespace WebCore {

static Class WebAVPlayerLayerView_layerClass(id, SEL)
{
    return [WebAVPlayerLayer class];
}

static void WebAVPlayerLayerView_transferVideoViewTo(id aSelf, SEL, WebAVPlayerLayerView *targetPlayerLayerView)
{
    WebAVPlayerLayerView *playerLayerView = aSelf;
    RetainPtr videoView = [playerLayerView videoView];
    if (!videoView)
        return;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [videoView removeFromSuperview];
    [playerLayerView setVideoView:nil];
    [targetPlayerLayerView setVideoView:videoView.get()];
    [targetPlayerLayerView addSubview:videoView.get()];
    [CATransaction commit];
}

static AVPlayerController *WebAVPlayerLayerView_playerController(id aSelf, SEL)
{
    __AVPlayerLayerView *playerLayer = aSelf;
    WebAVPlayerLayer *webAVPlayerLayer = (WebAVPlayerLayer *)[playerLayer playerLayer];
    return [webAVPlayerLayer playerController];
}

static void WebAVPlayerLayerView_setPlayerController(id aSelf, SEL, AVPlayerController *playerController)
{
    __AVPlayerLayerView *playerLayerView = aSelf;
    WebAVPlayerLayer *webAVPlayerLayer = (WebAVPlayerLayer *)[playerLayerView playerLayer];
    [webAVPlayerLayer setPlayerController:playerController];
}

static AVPlayerLayer *WebAVPlayerLayerView_playerLayer(id aSelf, SEL)
{
    __AVPlayerLayerView *playerLayerView = aSelf;

    if ([get__AVPlayerLayerViewClassSingleton() instancesRespondToSelector:@selector(playerLayer)]) {
        objc_super superClass { playerLayerView, get__AVPlayerLayerViewClassSingleton() };
        auto superClassMethod = reinterpret_cast<AVPlayerLayer *(*)(objc_super *, SEL)>(objc_msgSendSuper);
        return superClassMethod(&superClass, @selector(playerLayer));
    }

    return (AVPlayerLayer *)[playerLayerView layer];
}

static UIView *WebAVPlayerLayerView_videoView(id aSelf, SEL)
{
    __AVPlayerLayerView *playerLayerView = aSelf;
    WebAVPlayerLayer *webAVPlayerLayer = (WebAVPlayerLayer *)[playerLayerView playerLayer];
    CALayer* videoLayer = [webAVPlayerLayer videoSublayer];
    if (!videoLayer || !videoLayer.delegate)
        return nil;
    ASSERT([[videoLayer delegate] isKindOfClass:PAL::getUIViewClassSingleton()]);
    return (UIView *)[videoLayer delegate];
}

static void WebAVPlayerLayerView_setVideoView(id aSelf, SEL, UIView *videoView)
{
    __AVPlayerLayerView *playerLayerView = aSelf;
    WebAVPlayerLayer *webAVPlayerLayer = (WebAVPlayerLayer *)[playerLayerView playerLayer];
    [webAVPlayerLayer setVideoSublayer:[videoView layer]];
}

#if HAVE(PICTUREINPICTUREPLAYERLAYERVIEW)
static void WebAVPlayerLayerView_startRoutingVideoToPictureInPicturePlayerLayerView(id aSelf, SEL)
{
    WebAVPlayerLayerView *playerLayerView = aSelf;
    auto *pipView = (WebAVPictureInPicturePlayerLayerView *)[playerLayerView pictureInPicturePlayerLayerView];

    auto *playerLayer = (WebAVPlayerLayer *)[playerLayerView playerLayer];
    auto *pipPlayerLayer = (WebAVPlayerLayer *)[pipView layer];
    [playerLayer setVideoGravity:AVLayerVideoGravityResizeAspectFill];
    [pipPlayerLayer setPresentationModel:playerLayer.presentationModel];
    [pipPlayerLayer setVideoSublayer:playerLayer.videoSublayer];
    [pipPlayerLayer setVideoDimensions:playerLayer.videoDimensions];
    [pipPlayerLayer setVideoGravity:playerLayer.videoGravity];
    [pipPlayerLayer setPlayerController:playerLayer.playerController];
    [pipView addSubview:playerLayerView.videoView];
    [pipPlayerLayer setCaptionsLayer:playerLayer.captionsLayer];
    [pipPlayerLayer layoutSublayers];
}

static void WebAVPlayerLayerView_stopRoutingVideoToPictureInPicturePlayerLayerView(id aSelf, SEL)
{
    WebAVPlayerLayerView *playerLayerView = aSelf;
    auto *pipView = (WebAVPictureInPicturePlayerLayerView *)[playerLayerView pictureInPicturePlayerLayerView];

    auto *playerLayer = (WebAVPlayerLayer *)[playerLayerView playerLayer];
    auto *pipPlayerLayer = (WebAVPlayerLayer *)[pipView layer];
    [playerLayerView addSubview:playerLayerView.videoView];
    [playerLayer setCaptionsLayer:pipPlayerLayer.captionsLayer];
    [playerLayer layoutSublayers];
}

static WebAVPictureInPicturePlayerLayerView *WebAVPlayerLayerView_pictureInPicturePlayerLayerView(id aSelf, SEL)
{
    WebAVPlayerLayerView *playerLayerView = aSelf;
    if (WebAVPictureInPicturePlayerLayerView *pipView = [playerLayerView valueForKey:pictureInPicturePlayerLayerViewKey])
        return pipView;

    auto pipView = adoptNS([allocWebAVPictureInPicturePlayerLayerViewInstance() initWithFrame:CGRectZero]);
    [playerLayerView setValue:pipView.get() forKey:pictureInPicturePlayerLayerViewKey];
    return pipView.unsafeGet();
}
#endif // HAVE(PICTUREINPICTUREPLAYERLAYERVIEW)

static void WebAVPlayerLayerView_dealloc(id aSelf, SEL)
{
    WebAVPlayerLayerView *playerLayerView = aSelf;

    RetainPtr<UIView> videoView = playerLayerView.videoView;
    [playerLayerView setVideoView:nil];
    [videoView removeFromSuperview];
    videoView = nil;

#if HAVE(PICTUREINPICTUREPLAYERLAYERVIEW)
    [playerLayerView setValue:nil forKey:pictureInPicturePlayerLayerViewKey];
#endif
    objc_super superClass { playerLayerView, get__AVPlayerLayerViewClassSingleton() };
    auto super_dealloc = reinterpret_cast<void(*)(objc_super*, SEL)>(objc_msgSendSuper);
    super_dealloc(&superClass, @selector(dealloc));
}

#pragma mark - Methods

WebAVPlayerLayerView *allocWebAVPlayerLayerViewInstance()
{
    static Class theClass = [] {
        ASSERT(get__AVPlayerLayerViewClassSingleton());
        auto theClass = objc_allocateClassPair(get__AVPlayerLayerViewClassSingleton(), "WebAVPlayerLayerView", 0);
        class_addMethod(theClass, @selector(dealloc), (IMP)WebAVPlayerLayerView_dealloc, "v@:");
        class_addMethod(theClass, @selector(transferVideoViewTo:), (IMP)WebAVPlayerLayerView_transferVideoViewTo, "v@:@");
        class_addMethod(theClass, @selector(setPlayerController:), (IMP)WebAVPlayerLayerView_setPlayerController, "v@:@");
        class_addMethod(theClass, @selector(playerController), (IMP)WebAVPlayerLayerView_playerController, "@@:");
        class_addMethod(theClass, @selector(setVideoView:), (IMP)WebAVPlayerLayerView_setVideoView, "v@:@");
        class_addMethod(theClass, @selector(videoView), (IMP)WebAVPlayerLayerView_videoView, "@@:");
        class_addMethod(theClass, @selector(playerLayer), (IMP)WebAVPlayerLayerView_playerLayer, "@@:");
#if HAVE(PICTUREINPICTUREPLAYERLAYERVIEW)
        class_addMethod(theClass, @selector(startRoutingVideoToPictureInPicturePlayerLayerView), (IMP)WebAVPlayerLayerView_startRoutingVideoToPictureInPicturePlayerLayerView, "v@:");
        class_addMethod(theClass, @selector(stopRoutingVideoToPictureInPicturePlayerLayerView), (IMP)WebAVPlayerLayerView_stopRoutingVideoToPictureInPicturePlayerLayerView, "v@:");
        class_addMethod(theClass, @selector(pictureInPicturePlayerLayerView), (IMP)WebAVPlayerLayerView_pictureInPicturePlayerLayerView, "@@:");
        
        class_addIvar(theClass, "_pictureInPicturePlayerLayerView", sizeof(WebAVPictureInPicturePlayerLayerView *), log2(sizeof(WebAVPictureInPicturePlayerLayerView *)), "@");
#endif

        objc_registerClassPair(theClass);
        Class metaClass = objc_getMetaClass("WebAVPlayerLayerView");
        class_addMethod(metaClass, @selector(layerClass), (IMP)WebAVPlayerLayerView_layerClass, "@@:");
        return theClass;
    }();
    return (WebAVPlayerLayerView *)[theClass alloc];
}

#if HAVE(PICTUREINPICTUREPLAYERLAYERVIEW)
static Class WebAVPictureInPicturePlayerLayerView_layerClass(id, SEL)
{
    return [WebAVPlayerLayer class];
}

WebAVPictureInPicturePlayerLayerView *allocWebAVPictureInPicturePlayerLayerViewInstance()
{
    static Class theClass = [] {
        auto theClass = objc_allocateClassPair(PAL::getUIViewClassSingleton(), "WebAVPictureInPicturePlayerLayerView", 0);
        objc_registerClassPair(theClass);
        Class metaClass = objc_getMetaClass("WebAVPictureInPicturePlayerLayerView");
        class_addMethod(metaClass, @selector(layerClass), (IMP)WebAVPictureInPicturePlayerLayerView_layerClass, "@@:");
        return theClass;
    }();

    return (WebAVPictureInPicturePlayerLayerView *)[theClass alloc];
}
#endif

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY) && HAVE(AVKIT)
