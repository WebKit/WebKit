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
#import <WebCore/VideoPresentationModel.h>
#import <objc/message.h>
#import <objc/runtime.h>
#import <wtf/LoggerHelper.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>
#import <pal/cocoa/AVKitSoftLink.h>
#import <pal/ios/UIKitSoftLink.h>

#if HAVE(PICTUREINPICTUREPLAYERLAYERVIEW)
static NSString * const pictureInPicturePlayerLayerViewKey = @"_pictureInPicturePlayerLayerView";
#endif

#if !RELEASE_LOG_DISABLED
@interface WebAVPlayerLayerView (Logging)
@property (nonatomic) uint64_t logIdentifier;
@property (nonatomic) const Logger* loggerPtr;
@property (readonly, nonatomic) WTFLogChannel* logChannel;
@end
#endif

// __PRETTY_FUNC__ does the wrong thing for C-style ObjC method implementations:
#define C_OBJC_LOGIDENTIFIER WTF::Logger::LogSiteIdentifier(__func__, self.logIdentifier)

namespace WebCore {

static Class WebAVPlayerLayerView_layerClass(id, SEL)
{
    return [WebAVPlayerLayer class];
}

static WebAVPlayerLayer *WebAVPlayerLayerView_webPlayerLayer(WebAVPlayerLayerView *self, SEL)
{
    return (WebAVPlayerLayer *)[self playerLayer];
}

static void WebAVPlayerLayerView_transferVideoViewTo(WebAVPlayerLayerView *self, SEL, WebAVPlayerLayerView *targetPlayerLayerView)
{
    RetainPtr videoView = [self videoView];
    if (!videoView) {
        OBJC_ALWAYS_LOG(C_OBJC_LOGIDENTIFIER, "No videoView; bailing");
        return;
    }

    OBJC_ALWAYS_LOG(C_OBJC_LOGIDENTIFIER, "Transferring videoView to (", targetPlayerLayerView.logIdentifier, ")");
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [videoView removeFromSuperview];
    [self setVideoView:nil];
    [targetPlayerLayerView setVideoView:videoView.get()];
    [targetPlayerLayerView addSubview:videoView.get()];
    [CATransaction commit];
}

static AVPlayerController *WebAVPlayerLayerView_playerController(WebAVPlayerLayerView *self, SEL)
{
    return self.webPlayerLayer.playerController;
}

static void WebAVPlayerLayerView_setPlayerController(WebAVPlayerLayerView *self, SEL, AVPlayerController *playerController)
{
    [self.webPlayerLayer setPlayerController:playerController];
}

static AVPlayerLayer *WebAVPlayerLayerView_playerLayer(WebAVPlayerLayerView *self, SEL sel)
{
    if ([PAL::get__AVPlayerLayerViewClassSingleton() instancesRespondToSelector:@selector(playerLayer)]) {
        objc_super superClass { self, PAL::get__AVPlayerLayerViewClassSingleton() };
        auto superClassMethod = reinterpret_cast<AVPlayerLayer *(*)(objc_super *, SEL)>(objc_msgSendSuper);
        return superClassMethod(&superClass, sel);
    }

    return (AVPlayerLayer *)[self layer];
}

static UIView *WebAVPlayerLayerView_videoView(WebAVPlayerLayerView *self, SEL)
{
    WebAVPlayerLayer *webAVPlayerLayer = (WebAVPlayerLayer *)[self playerLayer];
    CALayer* videoLayer = [webAVPlayerLayer videoSublayer];
    if (!videoLayer || !videoLayer.delegate)
        return nil;
    ASSERT([[videoLayer delegate] isKindOfClass:PAL::getUIViewClassSingleton()]);
    return (UIView *)[videoLayer delegate];
}

static void WebAVPlayerLayerView_setVideoView(WebAVPlayerLayerView *self, SEL, UIView *videoView)
{
    OBJC_ALWAYS_LOG(C_OBJC_LOGIDENTIFIER, "view: ", (bool)videoView);
    WebAVPlayerLayer *webAVPlayerLayer = (WebAVPlayerLayer *)[self playerLayer];
    [webAVPlayerLayer setVideoSublayer:[videoView layer]];
}

#if HAVE(PICTUREINPICTUREPLAYERLAYERVIEW)
static void WebAVPlayerLayerView_startRoutingVideoToPictureInPicturePlayerLayerView(WebAVPlayerLayerView *self, SEL)
{
    OBJC_ALWAYS_LOG(C_OBJC_LOGIDENTIFIER);
    auto *pipView = (WebAVPictureInPicturePlayerLayerView *)[self pictureInPicturePlayerLayerView];

    auto *playerLayer = (WebAVPlayerLayer *)[self playerLayer];
    auto *pipPlayerLayer = (WebAVPlayerLayer *)[pipView layer];
    [playerLayer setVideoGravity:AVLayerVideoGravityResizeAspectFill];
    [pipPlayerLayer setPresentationModel:playerLayer.presentationModel];
    [pipPlayerLayer setVideoSublayer:playerLayer.videoSublayer];
    [pipPlayerLayer setVideoDimensions:playerLayer.videoDimensions];
    [pipPlayerLayer setVideoGravity:playerLayer.videoGravity];
    [pipPlayerLayer setPlayerController:playerLayer.playerController];
    [pipView addSubview:self.videoView];
    [pipPlayerLayer setCaptionsLayer:playerLayer.captionsLayer];
    [pipPlayerLayer layoutSublayers];
}

static void WebAVPlayerLayerView_stopRoutingVideoToPictureInPicturePlayerLayerView(WebAVPlayerLayerView *self, SEL)
{
    OBJC_ALWAYS_LOG(C_OBJC_LOGIDENTIFIER);
    auto *pipView = (WebAVPictureInPicturePlayerLayerView *)[self pictureInPicturePlayerLayerView];

    auto *playerLayer = (WebAVPlayerLayer *)[self playerLayer];
    auto *pipPlayerLayer = (WebAVPlayerLayer *)[pipView layer];
    [self addSubview:self.videoView];
    [playerLayer setCaptionsLayer:pipPlayerLayer.captionsLayer];
    [playerLayer layoutSublayers];
}

static WebAVPictureInPicturePlayerLayerView *WebAVPlayerLayerView_pictureInPicturePlayerLayerView(WebAVPlayerLayerView *self, SEL)
{
    if (WebAVPictureInPicturePlayerLayerView *pipView = [self valueForKey:pictureInPicturePlayerLayerViewKey])
        return pipView;

    RetainPtr pipView = adoptNS([allocWebAVPictureInPicturePlayerLayerViewInstance() initWithFrame:CGRectZero]);
    [self setValue:pipView.get() forKey:pictureInPicturePlayerLayerViewKey];
    return pipView.autorelease();
}
#endif // HAVE(PICTUREINPICTUREPLAYERLAYERVIEW)

static void WebAVPlayerLayerView_dealloc(WebAVPlayerLayerView *self, SEL)
{
    RetainPtr<UIView> videoView = self.videoView;
    [self setVideoView:nil];
    [videoView removeFromSuperview];
    videoView = nil;

#if HAVE(PICTUREINPICTUREPLAYERLAYERVIEW)
    [self setValue:nil forKey:pictureInPicturePlayerLayerViewKey];
#endif
    objc_super superClass { self, PAL::get__AVPlayerLayerViewClassSingleton() };
    auto super_dealloc = reinterpret_cast<void(*)(objc_super*, SEL)>(objc_msgSendSuper);
    super_dealloc(&superClass, @selector(dealloc));
}

#if !RELEASE_LOG_DISABLED
static uint64_t WebAVPlayerLayerView_logIdentifier(WebAVPlayerLayerView *self, SEL)
{
    return self.webPlayerLayer.logIdentifier;
}

static const Logger* WebAVPlayerLayerView_loggerPtr(WebAVPlayerLayerView *self, SEL)
{
    return self.webPlayerLayer.loggerPtr;
}

static WTFLogChannel* WebAVPlayerLayerView_logChannel(WebAVPlayerLayerView *self, SEL)
{
    return self.webPlayerLayer.logChannel;
}
#endif

#pragma mark - Methods

WebAVPlayerLayerView *allocWebAVPlayerLayerViewInstance()
{
    static Class theClass = [] {
        ASSERT(PAL::get__AVPlayerLayerViewClassSingleton());
        auto theClass = objc_allocateClassPair(PAL::get__AVPlayerLayerViewClassSingleton(), "WebAVPlayerLayerView", 0);
        class_addMethod(theClass, @selector(dealloc), (IMP)WebAVPlayerLayerView_dealloc, "v@:");
        class_addMethod(theClass, @selector(transferVideoViewTo:), (IMP)WebAVPlayerLayerView_transferVideoViewTo, "v@:@");
        class_addMethod(theClass, @selector(webPlayerLayer), (IMP)WebAVPlayerLayerView_webPlayerLayer, "@@:");
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
#if !RELEASE_LOG_DISABLED
        class_addMethod(theClass, @selector(logIdentifier), (IMP)WebAVPlayerLayerView_logIdentifier, "Q@:");
        class_addMethod(theClass, @selector(loggerPtr), (IMP)WebAVPlayerLayerView_loggerPtr, "^v@:");
        class_addMethod(theClass, @selector(logChannel), (IMP)WebAVPlayerLayerView_logChannel, "^v@:");
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

static void WebAVPictureInPicturePlayerLayerView_dealloc(WebAVPictureInPicturePlayerLayerView *self, SEL)
{
    WebAVPlayerLayer *pipPlayerLayer = dynamic_objc_cast<WebAVPlayerLayer>([self layer]);

    // Clear layer references before [super dealloc] to avoid crashes
    // during view hierarchy teardown.
    [pipPlayerLayer setVideoSublayer:nil];
    [pipPlayerLayer setCaptionsLayer:nil];

    objc_super superClass { self, PAL::getUIViewClassSingleton() };
    auto super_dealloc = reinterpret_cast<void(*)(objc_super*, SEL)>(objc_msgSendSuper);
    super_dealloc(&superClass, @selector(dealloc));
}

WebAVPictureInPicturePlayerLayerView *allocWebAVPictureInPicturePlayerLayerViewInstance()
{
    static Class theClass = [] {
        auto theClass = objc_allocateClassPair(PAL::getUIViewClassSingleton(), "WebAVPictureInPicturePlayerLayerView", 0);
        class_addMethod(theClass, @selector(dealloc), (IMP)WebAVPictureInPicturePlayerLayerView_dealloc, "v@:");
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
