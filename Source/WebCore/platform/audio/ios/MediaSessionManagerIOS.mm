/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "MediaSessionManagerIOS.h"

#if PLATFORM(IOS)

#import "Logging.h"
#import "MediaPlayer.h"
#import "MediaSession.h"
#import "SoftLinking.h"
#import "WebCoreSystemInterface.h"
#import "WebCoreThreadRun.h"
#import <AVFoundation/AVAudioSession.h>
#import <MediaPlayer/MPAVRoutingController.h>
#import <MediaPlayer/MPMediaItem.h>
#import <MediaPlayer/MPNowPlayingInfoCenter.h>
#import <MediaPlayer/MPVolumeView.h>
#import <UIKit/UIApplication.h>
#import <objc/runtime.h>
#import <wtf/RetainPtr.h>

SOFT_LINK_FRAMEWORK(AVFoundation)
SOFT_LINK_CLASS(AVFoundation, AVAudioSession)
SOFT_LINK_POINTER(AVFoundation, AVAudioSessionInterruptionNotification, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVAudioSessionInterruptionTypeKey, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVAudioSessionInterruptionOptionKey, NSString *)

#define AVAudioSession getAVAudioSessionClass()
#define AVAudioSessionInterruptionNotification getAVAudioSessionInterruptionNotification()
#define AVAudioSessionInterruptionTypeKey getAVAudioSessionInterruptionTypeKey()
#define AVAudioSessionInterruptionOptionKey getAVAudioSessionInterruptionOptionKey()

SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK_CLASS(UIKit, UIApplication)
SOFT_LINK_POINTER(UIKit, UIApplicationWillResignActiveNotification, NSString *)
SOFT_LINK_POINTER(UIKit, UIApplicationWillEnterForegroundNotification, NSString *)
SOFT_LINK_POINTER(UIKit, UIApplicationDidBecomeActiveNotification, NSString *)

#define UIApplication getUIApplicationClass()
#define UIApplicationWillResignActiveNotification getUIApplicationWillResignActiveNotification()
#define UIApplicationWillEnterForegroundNotification getUIApplicationWillEnterForegroundNotification()
#define UIApplicationDidBecomeActiveNotification getUIApplicationDidBecomeActiveNotification()

SOFT_LINK_FRAMEWORK(MediaPlayer)
SOFT_LINK_CLASS(MediaPlayer, MPAVRoutingController)
SOFT_LINK_CLASS(MediaPlayer, MPNowPlayingInfoCenter)
SOFT_LINK_CLASS(MediaPlayer, MPVolumeView)
SOFT_LINK_POINTER(MediaPlayer, MPMediaItemPropertyTitle, NSString *)
SOFT_LINK_POINTER(MediaPlayer, MPMediaItemPropertyPlaybackDuration, NSString *)
SOFT_LINK_POINTER(MediaPlayer, MPNowPlayingInfoPropertyElapsedPlaybackTime, NSString *)
SOFT_LINK_POINTER(MediaPlayer, MPNowPlayingInfoPropertyPlaybackRate, NSString *)
SOFT_LINK_POINTER(MediaPlayer, MPVolumeViewWirelessRoutesAvailableDidChangeNotification, NSString *)


#define MPMediaItemPropertyTitle getMPMediaItemPropertyTitle()
#define MPMediaItemPropertyPlaybackDuration getMPMediaItemPropertyPlaybackDuration()
#define MPNowPlayingInfoPropertyElapsedPlaybackTime getMPNowPlayingInfoPropertyElapsedPlaybackTime()
#define MPNowPlayingInfoPropertyPlaybackRate getMPNowPlayingInfoPropertyPlaybackRate()
#define MPVolumeViewWirelessRoutesAvailableDidChangeNotification getMPVolumeViewWirelessRoutesAvailableDidChangeNotification()

NSString* WebUIApplicationWillResignActiveNotification = @"WebUIApplicationWillResignActiveNotification";
NSString* WebUIApplicationWillEnterForegroundNotification = @"WebUIApplicationWillEnterForegroundNotification";
NSString* WebUIApplicationDidBecomeActiveNotification = @"WebUIApplicationDidBecomeActiveNotification";

using namespace WebCore;

@interface WebMediaSessionHelper : NSObject {
    MediaSessionManageriOS* _callback;
    RetainPtr<MPVolumeView> _volumeView;
    RetainPtr<MPAVRoutingController> _airPlayPresenceRoutingController;
}

- (id)initWithCallback:(MediaSessionManageriOS*)callback;
- (void)clearCallback;
- (void)interruption:(NSNotification *)notification;
- (void)applicationWillEnterForeground:(NSNotification *)notification;
- (void)applicationWillResignActive:(NSNotification *)notification;
- (BOOL)hasWirelessTargetsAvailable;
- (void)startMonitoringAirPlayRoutes;
- (void)stopMonitoringAirPlayRoutes;
@end

namespace WebCore {

MediaSessionManager& MediaSessionManager::sharedManager()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(MediaSessionManageriOS, manager, ());
    return manager;
}

MediaSessionManageriOS::MediaSessionManageriOS()
    :MediaSessionManager()
    , m_objcObserver(adoptNS([[WebMediaSessionHelper alloc] initWithCallback:this]))
{
    resetRestrictions();
}

MediaSessionManageriOS::~MediaSessionManageriOS()
{
    [m_objcObserver clearCallback];
}

void MediaSessionManageriOS::resetRestrictions()
{
    MediaSessionManager::resetRestrictions();

    static wkDeviceClass deviceClass = iosDeviceClass();
    if (deviceClass == wkDeviceClassiPhone || deviceClass == wkDeviceClassiPod)
        addRestriction(MediaSession::Video, InlineVideoPlaybackRestricted);

    addRestriction(MediaSession::Video, ConcurrentPlaybackNotPermitted);
    addRestriction(MediaSession::Video, BackgroundPlaybackNotPermitted);

    removeRestriction(MediaSession::Audio, ConcurrentPlaybackNotPermitted);
    removeRestriction(MediaSession::Audio, BackgroundPlaybackNotPermitted);

    removeRestriction(MediaSession::WebAudio, ConcurrentPlaybackNotPermitted);
    removeRestriction(MediaSession::WebAudio, BackgroundPlaybackNotPermitted);

    removeRestriction(MediaSession::Audio, MetadataPreloadingNotPermitted);
    removeRestriction(MediaSession::Video, MetadataPreloadingNotPermitted);

    addRestriction(MediaSession::Audio, AutoPreloadingNotPermitted);
    addRestriction(MediaSession::Video, AutoPreloadingNotPermitted);
}

#if ENABLE(IOS_AIRPLAY)

bool MediaSessionManageriOS::hasWirelessTargetsAvailable()
{
    return [m_objcObserver hasWirelessTargetsAvailable];
}

void MediaSessionManageriOS::startMonitoringAirPlayRoutes()
{
    [m_objcObserver startMonitoringAirPlayRoutes];
}

void MediaSessionManageriOS::stopMonitoringAirPlayRoutes()
{
    [m_objcObserver stopMonitoringAirPlayRoutes];
}
#endif
    
void MediaSessionManageriOS::sessionWillBeginPlayback(MediaSession& session)
{
    MediaSessionManager::sessionWillBeginPlayback(session);
    updateNowPlayingInfo();
}
    
void MediaSessionManageriOS::sessionWillEndPlayback(MediaSession& session)
{
    MediaSessionManager::sessionWillEndPlayback(session);
    updateNowPlayingInfo();
}
    
void MediaSessionManageriOS::updateNowPlayingInfo()
{
    MPNowPlayingInfoCenter *nowPlaying = (MPNowPlayingInfoCenter *)[getMPNowPlayingInfoCenterClass() defaultCenter];
    const MediaSession* currentSession = this->currentSession();
    
    if (!currentSession) {
        [nowPlaying setNowPlayingInfo:nil];
        return;
    }
    
    RetainPtr<NSMutableDictionary> info = adoptNS([[NSMutableDictionary alloc] init]);
    
    String title = currentSession->title();
    if (!title.isEmpty())
        [info setValue:static_cast<NSString *>(title) forKey:MPMediaItemPropertyTitle];
    
    double duration = currentSession->duration();
    if (std::isfinite(duration) && duration != MediaPlayer::invalidTime())
        [info setValue:@(duration) forKey:MPMediaItemPropertyPlaybackDuration];
    
    double currentTime = currentSession->currentTime();
    if (std::isfinite(currentTime) && currentTime != MediaPlayer::invalidTime())
        [info setValue:@(currentTime) forKey:MPNowPlayingInfoPropertyElapsedPlaybackTime];
    
    [info setValue:(currentSession->state() == MediaSession::Playing ? @YES : @NO) forKey:MPNowPlayingInfoPropertyPlaybackRate];
    [nowPlaying setNowPlayingInfo:info.get()];
}

} // namespace WebCore

@implementation WebMediaSessionHelper

- (id)initWithCallback:(MediaSessionManageriOS*)callback
{
    if (!(self = [super init]))
        return nil;
    
    _callback = callback;
    _volumeView = adoptNS([[getMPVolumeViewClass() alloc] init]);

    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center addObserver:self selector:@selector(interruption:) name:AVAudioSessionInterruptionNotification object:[AVAudioSession sharedInstance]];

    [center addObserver:self selector:@selector(applicationWillEnterForeground:) name:UIApplicationWillEnterForegroundNotification object:nil];
    [center addObserver:self selector:@selector(applicationWillEnterForeground:) name:WebUIApplicationWillEnterForegroundNotification object:nil];
    [center addObserver:self selector:@selector(applicationDidBecomeActive:) name:UIApplicationDidBecomeActiveNotification object:nil];
    [center addObserver:self selector:@selector(applicationDidBecomeActive:) name:WebUIApplicationDidBecomeActiveNotification object:nil];
    [center addObserver:self selector:@selector(applicationWillResignActive:) name:UIApplicationWillResignActiveNotification object:nil];
    [center addObserver:self selector:@selector(applicationWillResignActive:) name:WebUIApplicationWillResignActiveNotification object:nil];
    [center addObserver:self selector:@selector(wirelessRoutesAvailableDidChange:) name:MPVolumeViewWirelessRoutesAvailableDidChangeNotification object:_volumeView.get()];

    // Now playing won't work unless we turn on the delivery of remote control events.
    [[UIApplication sharedApplication] beginReceivingRemoteControlEvents];
    
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [super dealloc];
}

- (void)clearCallback
{
    _callback = nil;
}

- (BOOL)hasWirelessTargetsAvailable
{
    return [_volumeView areWirelessRoutesAvailable];
}

- (void)startMonitoringAirPlayRoutes
{
    if (_airPlayPresenceRoutingController)
        return;

    _airPlayPresenceRoutingController = adoptNS([[getMPAVRoutingControllerClass() alloc] initWithName:@"WebCore - HTML media element checking for AirPlay route presence"]);
    [_airPlayPresenceRoutingController setDiscoveryMode:MPRouteDiscoveryModePresence];
}

- (void)stopMonitoringAirPlayRoutes
{
    if (!_airPlayPresenceRoutingController)
        return;

    [_airPlayPresenceRoutingController setDiscoveryMode:MPRouteDiscoveryModeDisabled];
    _airPlayPresenceRoutingController = nil;
}

- (void)interruption:(NSNotification *)notification
{
    if (!_callback)
        return;

    NSUInteger type = [[[notification userInfo] objectForKey:AVAudioSessionInterruptionTypeKey] unsignedIntegerValue];
    MediaSession::EndInterruptionFlags flags = MediaSession::NoFlags;

    if (type == AVAudioSessionInterruptionTypeEnded && [[[notification userInfo] objectForKey:AVAudioSessionInterruptionOptionKey] unsignedIntegerValue] == AVAudioSessionInterruptionOptionShouldResume)
        flags = MediaSession::MayResumePlaying;

    WebThreadRun(^{
        if (!_callback)
            return;

        if (type == AVAudioSessionInterruptionTypeBegan)
            _callback->beginInterruption(MediaSession::SystemInterruption);
        else
            _callback->endInterruption(flags);

    });
}

- (void)applicationWillEnterForeground:(NSNotification *)notification
{
    UNUSED_PARAM(notification);

    if (!_callback)
        return;

    WebThreadRun(^{
        if (!_callback)
            return;

        _callback->applicationWillEnterForeground();
    });
}

- (void)applicationDidBecomeActive:(NSNotification *)notification
{
    UNUSED_PARAM(notification);

    if (!_callback)
        return;

    WebThreadRun(^{
        if (!_callback)
            return;

        _callback->applicationWillEnterForeground();
    });
}

- (void)applicationWillResignActive:(NSNotification *)notification
{
    UNUSED_PARAM(notification);

    if (!_callback)
        return;
    
    WebThreadRun(^{
        if (!_callback)
            return;
        
        _callback->applicationWillEnterBackground();
    });
}

- (void)wirelessRoutesAvailableDidChange:(NSNotification *)notification
{
    UNUSED_PARAM(notification);

    if (!_callback)
        return;

    WebThreadRun(^{
        if (!_callback)
            return;

        _callback->wirelessRoutesAvailableChanged();
    });
}
@end

#endif // PLATFORM(IOS)
