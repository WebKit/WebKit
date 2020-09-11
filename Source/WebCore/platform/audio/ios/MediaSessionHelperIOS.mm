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
#import "MediaSessionHelperIOS.h"

#if PLATFORM(IOS_FAMILY)

#import "Logging.h"
#import "MediaPlaybackTargetCocoa.h"
#import "RuntimeEnabledFeatures.h"
#import "WebCoreThreadRun.h"
#import <AVFoundation/AVAudioSession.h>
#import <AVFoundation/AVRouteDetector.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <pal/spi/ios/CelestialSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/MainThread.h>
#import <wtf/RetainPtr.h>
#import <wtf/UniqueRef.h>

#import <pal/cocoa/AVFoundationSoftLink.h>
#import <pal/ios/UIKitSoftLink.h>

WEBCORE_EXPORT NSString* WebUIApplicationWillResignActiveNotification = @"WebUIApplicationWillResignActiveNotification";
WEBCORE_EXPORT NSString* WebUIApplicationWillEnterForegroundNotification = @"WebUIApplicationWillEnterForegroundNotification";
WEBCORE_EXPORT NSString* WebUIApplicationDidBecomeActiveNotification = @"WebUIApplicationDidBecomeActiveNotification";
WEBCORE_EXPORT NSString* WebUIApplicationDidEnterBackgroundNotification = @"WebUIApplicationDidEnterBackgroundNotification";

#if HAVE(CELESTIAL)
SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(Celestial)
SOFT_LINK_CLASS_OPTIONAL(Celestial, AVSystemController)
SOFT_LINK_CONSTANT_MAY_FAIL(Celestial, AVSystemController_PIDToInheritApplicationStateFrom, NSString *)
SOFT_LINK_CONSTANT_MAY_FAIL(Celestial, AVSystemController_CarPlayIsConnectedAttribute, NSString *)
SOFT_LINK_CONSTANT_MAY_FAIL(Celestial, AVSystemController_CarPlayIsConnectedDidChangeNotification, NSString *)
SOFT_LINK_CONSTANT_MAY_FAIL(Celestial, AVSystemController_CarPlayIsConnectedNotificationParameter, NSString *)
SOFT_LINK_CONSTANT_MAY_FAIL(Celestial, AVSystemController_ServerConnectionDiedNotification, NSString *)
#endif

using namespace WebCore;

class MediaSessionHelperiOS;

@interface WebMediaSessionHelper : NSObject {
    MediaSessionHelperiOS* _callback;

#if !PLATFORM(WATCHOS)
    RetainPtr<AVRouteDetector> _routeDetector;
#endif
    bool _monitoringAirPlayRoutes;
    bool _startMonitoringAirPlayRoutesPending;
}

- (id)initWithCallback:(MediaSessionHelperiOS*)callback;

- (void)clearCallback;
- (void)applicationWillEnterForeground:(NSNotification *)notification;
- (void)applicationWillResignActive:(NSNotification *)notification;
- (void)applicationDidEnterBackground:(NSNotification *)notification;
- (BOOL)hasWirelessTargetsAvailable;

#if !PLATFORM(WATCHOS)
- (void)startMonitoringAirPlayRoutes;
- (void)stopMonitoringAirPlayRoutes;
#endif

@end

class MediaSessionHelperiOS final : public MediaSessionHelper {
public:
    MediaSessionHelperiOS();
    ~MediaSessionHelperiOS();

    void externalOutputDeviceAvailableDidChange();
    void applicationWillEnterForeground(MediaSessionHelperClient::SuspendedUnderLock);
    void applicationDidEnterBackground(MediaSessionHelperClient::SuspendedUnderLock);
    void applicationWillBecomeInactive();
    void applicationDidBecomeActive();
#if HAVE(CELESTIAL)
    void mediaServerConnectionDied();
    void updateCarPlayIsConnected(Optional<bool>&&);
#endif
#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST) && !PLATFORM(WATCHOS)
    void activeAudioRouteDidChange(bool);
    void activeVideoRouteDidChange();
#endif

private:
    using HasAvailableTargets = MediaSessionHelperClient::HasAvailableTargets;
    using PlayingToAutomotiveHeadUnit = MediaSessionHelperClient::PlayingToAutomotiveHeadUnit;
    using ShouldPause = MediaSessionHelperClient::ShouldPause;
    using SupportsAirPlayVideo = MediaSessionHelperClient::SupportsAirPlayVideo;
    using SuspendedUnderLock = MediaSessionHelperClient::SuspendedUnderLock;

    void setIsPlayingToAutomotiveHeadUnit(bool);

    void providePresentingApplicationPID(int) final;
    void startMonitoringWirelessRoutes() final;
    void stopMonitoringWirelessRoutes() final;

    RetainPtr<WebMediaSessionHelper> m_objcObserver;
#if HAVE(CELESTIAL)
    bool m_havePresentedApplicationPID { false };
#endif
};

static UniqueRef<MediaSessionHelper>& sharedHelperInstance()
{
    static NeverDestroyed<UniqueRef<MediaSessionHelper>> helper = makeUniqueRef<MediaSessionHelperiOS>();
    return helper;
}

MediaSessionHelper& MediaSessionHelper::sharedHelper()
{
    return sharedHelperInstance();
}

void MediaSessionHelper::resetSharedHelper()
{
    sharedHelperInstance() = makeUniqueRef<MediaSessionHelperiOS>();
}

void MediaSessionHelper::setSharedHelper(UniqueRef<MediaSessionHelper>&& helper)
{
    sharedHelperInstance() = WTFMove(helper);
}

void MediaSessionHelper::addClient(MediaSessionHelperClient& client)
{
    ASSERT(!m_clients.contains(client));
    m_clients.add(client);
}

void MediaSessionHelper::removeClient(MediaSessionHelperClient& client)
{
    ASSERT(m_clients.contains(client));
    m_clients.remove(client);
}

MediaSessionHelperiOS::MediaSessionHelperiOS()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    m_objcObserver = adoptNS([[WebMediaSessionHelper alloc] initWithCallback:this]);
    m_isExternalOutputDeviceAvailable = [m_objcObserver hasWirelessTargetsAvailable];
    END_BLOCK_OBJC_EXCEPTIONS

#if HAVE(CELESTIAL)
    updateCarPlayIsConnected(WTF::nullopt);
#endif
}

MediaSessionHelperiOS::~MediaSessionHelperiOS()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_objcObserver clearCallback];
    END_BLOCK_OBJC_EXCEPTIONS
}

void MediaSessionHelperiOS::providePresentingApplicationPID(int pid)
{
#if HAVE(CELESTIAL)
    if (m_havePresentedApplicationPID)
        return;
    m_havePresentedApplicationPID = true;

    if (RuntimeEnabledFeatures::sharedFeatures().disableMediaExperiencePIDInheritance())
        return;

    if (!canLoadAVSystemController_PIDToInheritApplicationStateFrom())
        return;

    NSError *error = nil;
    [[getAVSystemControllerClass() sharedAVSystemController] setAttribute:@(pid) forKey:getAVSystemController_PIDToInheritApplicationStateFrom() error:&error];
    if (error)
        WTFLogAlways("Failed to set up PID proxying: %s", error.localizedDescription.UTF8String);
#else
    UNUSED_PARAM(pid);
#endif
}

void MediaSessionHelperiOS::startMonitoringWirelessRoutes()
{
    if (m_monitoringWirelessRoutesCount++)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
#if !PLATFORM(WATCHOS)
    [m_objcObserver startMonitoringAirPlayRoutes];
#endif
    END_BLOCK_OBJC_EXCEPTIONS
}

void MediaSessionHelperiOS::stopMonitoringWirelessRoutes()
{
    if (!m_monitoringWirelessRoutesCount) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (--m_monitoringWirelessRoutesCount)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
#if !PLATFORM(WATCHOS)
    [m_objcObserver stopMonitoringAirPlayRoutes];
#endif
    END_BLOCK_OBJC_EXCEPTIONS
}

#if HAVE(CELESTIAL)
void MediaSessionHelperiOS::mediaServerConnectionDied()
{
    updateCarPlayIsConnected(WTF::nullopt);

    if (!m_havePresentedApplicationPID)
        return;

    m_havePresentedApplicationPID = false;
    for (auto& client : m_clients)
        client.mediaServerConnectionDied();
}

void MediaSessionHelperiOS::updateCarPlayIsConnected(Optional<bool>&& carPlayIsConnected)
{
    if (carPlayIsConnected) {
        setIsPlayingToAutomotiveHeadUnit(carPlayIsConnected.value());
        return;
    }

    if (!canLoadAVSystemController_CarPlayIsConnectedAttribute()) {
        setIsPlayingToAutomotiveHeadUnit(false);
        return;
    }

    setIsPlayingToAutomotiveHeadUnit([[[getAVSystemControllerClass() sharedAVSystemController] attributeForKey:getAVSystemController_CarPlayIsConnectedAttribute()] boolValue]);
}

void MediaSessionHelperiOS::setIsPlayingToAutomotiveHeadUnit(bool isPlaying)
{
    if (isPlaying == m_isPlayingToAutomotiveHeadUnit)
        return;

    m_isPlayingToAutomotiveHeadUnit = isPlaying;
    for (auto& client : m_clients)
        client.isPlayingToAutomotiveHeadUnitDidChange(m_isPlayingToAutomotiveHeadUnit ? PlayingToAutomotiveHeadUnit::Yes : PlayingToAutomotiveHeadUnit::No);
}
#endif // HAVE(CELESTIAL)

#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST) && !PLATFORM(WATCHOS)
void MediaSessionHelperiOS::activeAudioRouteDidChange(bool shouldPause)
{
    for (auto& client : m_clients)
        client.activeAudioRouteDidChange(shouldPause ? ShouldPause::Yes : ShouldPause::No);
}

void MediaSessionHelperiOS::activeVideoRouteDidChange()
{
    auto playbackTarget = MediaPlaybackTargetCocoa::create();
    m_activeVideoRouteSupportsAirPlayVideo = playbackTarget->supportsRemoteVideoPlayback();
    for (auto& client : m_clients)
        client.activeVideoRouteDidChange(m_activeVideoRouteSupportsAirPlayVideo ? SupportsAirPlayVideo::Yes : SupportsAirPlayVideo::No, playbackTarget.copyRef());
}
#endif

void MediaSessionHelperiOS::applicationDidBecomeActive()
{
    for (auto& client : m_clients)
        client.applicationDidBecomeActive();
}

void MediaSessionHelperiOS::applicationDidEnterBackground(MediaSessionHelperClient::SuspendedUnderLock underLock)
{
    for (auto& client : m_clients)
        client.applicationDidEnterBackground(underLock);
}

void MediaSessionHelperiOS::applicationWillBecomeInactive()
{
    for (auto& client : m_clients)
        client.applicationWillBecomeInactive();
}

void MediaSessionHelperiOS::applicationWillEnterForeground(MediaSessionHelperClient::SuspendedUnderLock underLock)
{
    for (auto& client : m_clients)
        client.applicationWillEnterForeground(underLock);
}

void MediaSessionHelperiOS::externalOutputDeviceAvailableDidChange()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    m_isExternalOutputDeviceAvailable = [m_objcObserver hasWirelessTargetsAvailable];
    END_BLOCK_OBJC_EXCEPTIONS

    for (auto& client : m_clients)
        client.externalOutputDeviceAvailableDidChange(m_isExternalOutputDeviceAvailable ? HasAvailableTargets::Yes : HasAvailableTargets::No);
}

@implementation WebMediaSessionHelper

- (id)initWithCallback:(MediaSessionHelperiOS*)callback
{
    LOG(Media, "-[WebMediaSessionHelper initWithCallback]");

    if (!(self = [super init]))
        return nil;

    _callback = callback;

    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center addObserver:self selector:@selector(applicationWillEnterForeground:) name:PAL::get_UIKit_UIApplicationWillEnterForegroundNotification() object:nil];
    [center addObserver:self selector:@selector(applicationWillEnterForeground:) name:WebUIApplicationWillEnterForegroundNotification object:nil];
    [center addObserver:self selector:@selector(applicationDidBecomeActive:) name:PAL::get_UIKit_UIApplicationDidBecomeActiveNotification() object:nil];
    [center addObserver:self selector:@selector(applicationDidBecomeActive:) name:WebUIApplicationDidBecomeActiveNotification object:nil];
    [center addObserver:self selector:@selector(applicationWillResignActive:) name:PAL::get_UIKit_UIApplicationWillResignActiveNotification() object:nil];
    [center addObserver:self selector:@selector(applicationWillResignActive:) name:WebUIApplicationWillResignActiveNotification object:nil];
    [center addObserver:self selector:@selector(applicationDidEnterBackground:) name:PAL::get_UIKit_UIApplicationDidEnterBackgroundNotification() object:nil];
    [center addObserver:self selector:@selector(applicationDidEnterBackground:) name:WebUIApplicationDidEnterBackgroundNotification object:nil];
#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST) && !PLATFORM(WATCHOS)
    [center addObserver:self selector:@selector(activeOutputDeviceDidChange:) name:PAL::get_AVFoundation_AVAudioSessionRouteChangeNotification() object:nil];
#endif

#if HAVE(CELESTIAL)
    if (canLoadAVSystemController_ServerConnectionDiedNotification())
        [center addObserver:self selector:@selector(mediaServerConnectionDied:) name:getAVSystemController_ServerConnectionDiedNotification() object:nil];
    if (canLoadAVSystemController_CarPlayIsConnectedDidChangeNotification())
        [center addObserver:self selector:@selector(carPlayIsConnectedDidChange:) name:getAVSystemController_CarPlayIsConnectedDidChangeNotification() object:nil];
#endif

    // Now playing won't work unless we turn on the delivery of remote control events.
    dispatch_async(dispatch_get_main_queue(), ^{
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        [[PAL::getUIApplicationClass() sharedApplication] beginReceivingRemoteControlEvents];
        END_BLOCK_OBJC_EXCEPTIONS
    });

    return self;
}

- (void)dealloc
{
    LOG(Media, "-[WebMediaSessionHelper dealloc]");

#if !PLATFORM(WATCHOS)
    if (!pthread_main_np()) {
        dispatch_async(dispatch_get_main_queue(), [routeDetector = WTFMove(_routeDetector)] () mutable {
            LOG(Media, "safelyTearDown - dipatched to UI thread.");
            BEGIN_BLOCK_OBJC_EXCEPTIONS
            routeDetector.get().routeDetectionEnabled = NO;
            routeDetector.clear();
            END_BLOCK_OBJC_EXCEPTIONS
        });
    } else
        _routeDetector.get().routeDetectionEnabled = NO;
#endif

    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [super dealloc];
}

- (void)clearCallback
{
    LOG(Media, "-[WebMediaSessionHelper clearCallback]");
    _callback = nil;
}

- (BOOL)hasWirelessTargetsAvailable
{
    LOG(Media, "-[WebMediaSessionHelper hasWirelessTargetsAvailable]");
#if !PLATFORM(WATCHOS)
    return _routeDetector.get().multipleRoutesDetected;
#else
    return NO;
#endif
}

#if !PLATFORM(WATCHOS)
- (void)startMonitoringAirPlayRoutes
{
    if (_monitoringAirPlayRoutes)
        return;

    _monitoringAirPlayRoutes = true;

    if (_startMonitoringAirPlayRoutesPending)
        return;

    if (_routeDetector) {
        _routeDetector.get().routeDetectionEnabled = YES;
        return;
    }

    _startMonitoringAirPlayRoutesPending = true;

    LOG(Media, "-[WebMediaSessionHelper startMonitoringAirPlayRoutes]");

    callOnWebThreadOrDispatchAsyncOnMainThread([protectedSelf = retainPtr(self)]() mutable {
        ASSERT(!protectedSelf->_routeDetector);

        if (protectedSelf->_callback) {
            BEGIN_BLOCK_OBJC_EXCEPTIONS
            protectedSelf->_routeDetector = adoptNS([PAL::allocAVRouteDetectorInstance() init]);
            protectedSelf->_routeDetector.get().routeDetectionEnabled = protectedSelf->_monitoringAirPlayRoutes;
            [[NSNotificationCenter defaultCenter] addObserver:protectedSelf.get() selector:@selector(wirelessRoutesAvailableDidChange:) name:AVRouteDetectorMultipleRoutesDetectedDidChangeNotification object:protectedSelf->_routeDetector.get()];

            protectedSelf->_callback->externalOutputDeviceAvailableDidChange();
            END_BLOCK_OBJC_EXCEPTIONS
        }

        protectedSelf->_startMonitoringAirPlayRoutesPending = false;
    });
}

- (void)stopMonitoringAirPlayRoutes
{
    if (!_monitoringAirPlayRoutes)
        return;

    LOG(Media, "-[WebMediaSessionHelper stopMonitoringAirPlayRoutes]");

    _monitoringAirPlayRoutes = false;
    _routeDetector.get().routeDetectionEnabled = NO;
}
#endif // !PLATFORM(WATCHOS)

- (void)applicationWillEnterForeground:(NSNotification *)notification
{
    using SuspendedUnderLock = MediaSessionHelperClient::SuspendedUnderLock;

    if (!_callback)
        return;

    LOG(Media, "-[WebMediaSessionHelper applicationWillEnterForeground]");

    auto isSuspendedUnderLock = [[[notification userInfo] objectForKey:@"isSuspendedUnderLock"] boolValue] ? SuspendedUnderLock::Yes : SuspendedUnderLock::No;
    callOnWebThreadOrDispatchAsyncOnMainThread([protectedSelf = retainPtr(self), isSuspendedUnderLock]() mutable {
        if (auto* callback = protectedSelf->_callback)
            callback->applicationWillEnterForeground(isSuspendedUnderLock);
    });
}

- (void)applicationDidBecomeActive:(NSNotification *)notification
{
    UNUSED_PARAM(notification);

    if (!_callback)
        return;

    LOG(Media, "-[WebMediaSessionHelper applicationDidBecomeActive]");

    callOnWebThreadOrDispatchAsyncOnMainThread([protectedSelf = retainPtr(self)]() mutable {
        if (auto* callback = protectedSelf->_callback)
            callback->applicationDidBecomeActive();
    });
}

- (void)applicationWillResignActive:(NSNotification *)notification
{
    UNUSED_PARAM(notification);

    if (!_callback)
        return;

    LOG(Media, "-[WebMediaSessionHelper applicationWillResignActive]");

    callOnWebThreadOrDispatchAsyncOnMainThread([protectedSelf = retainPtr(self)]() mutable {
        if (auto* callback = protectedSelf->_callback)
            callback->applicationWillBecomeInactive();
    });
}

- (void)wirelessRoutesAvailableDidChange:(NSNotification *)notification
{
    UNUSED_PARAM(notification);

    if (!_callback || !_monitoringAirPlayRoutes)
        return;

    LOG(Media, "-[WebMediaSessionHelper wirelessRoutesAvailableDidChange]");

    callOnWebThreadOrDispatchAsyncOnMainThread([protectedSelf = retainPtr(self)]() mutable {
        if (auto* callback = protectedSelf->_callback)
            callback->externalOutputDeviceAvailableDidChange();
    });
}

- (void)applicationDidEnterBackground:(NSNotification *)notification
{
    using SuspendedUnderLock = MediaSessionHelperClient::SuspendedUnderLock;

    if (!_callback)
        return;

    LOG(Media, "-[WebMediaSessionHelper applicationDidEnterBackground]");

    auto isSuspendedUnderLock = [[[notification userInfo] objectForKey:@"isSuspendedUnderLock"] boolValue] ? SuspendedUnderLock::Yes : SuspendedUnderLock::No;
    callOnWebThreadOrDispatchAsyncOnMainThread([protectedSelf = retainPtr(self), isSuspendedUnderLock]() mutable {
        if (auto* callback = protectedSelf->_callback)
            callback->applicationDidEnterBackground(isSuspendedUnderLock);
    });
}

#if HAVE(CELESTIAL)
- (void)mediaServerConnectionDied:(NSNotification *)notification
{
    if (!_callback)
        return;

    LOG(Media, "-[WebMediaSessionHelper mediaServerConnectionDied:]");
    UNUSED_PARAM(notification);
    callOnWebThreadOrDispatchAsyncOnMainThread([protectedSelf = retainPtr(self)]() mutable {
        if (auto* callback = protectedSelf->_callback)
            callback->mediaServerConnectionDied();
    });
}

- (void)carPlayIsConnectedDidChange:(NSNotification *)notification
{
    if (!_callback)
        return;

    Optional<bool> carPlayIsConnected;
    if (notification && canLoadAVSystemController_CarPlayIsConnectedNotificationParameter()) {
        NSNumber *nsCarPlayIsConnected = [[notification userInfo] valueForKey:getAVSystemController_CarPlayIsConnectedNotificationParameter()];
        if (nsCarPlayIsConnected)
            carPlayIsConnected = [nsCarPlayIsConnected boolValue];
    }

    callOnWebThreadOrDispatchAsyncOnMainThread([protectedSelf = retainPtr(self), carPlayIsConnected = WTFMove(carPlayIsConnected)]() mutable {
        if (auto* callback = protectedSelf->_callback)
            callback->updateCarPlayIsConnected(WTFMove(carPlayIsConnected));
    });
}
#endif // HAVE(CELESTIAL)

#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST) && !PLATFORM(WATCHOS)
- (void)activeOutputDeviceDidChange:(NSNotification *)notification
{
    if (!_callback)
        return;

    bool shouldPause = [[notification.userInfo objectForKey:PAL::get_AVFoundation_AVAudioSessionRouteChangeReasonKey()] unsignedIntegerValue] == AVAudioSessionRouteChangeReasonOldDeviceUnavailable;
    callOnWebThreadOrDispatchAsyncOnMainThread([protectedSelf = retainPtr(self), shouldPause]() mutable {
        if (auto* callback = protectedSelf->_callback) {
            callback->activeAudioRouteDidChange(shouldPause);
            callback->activeVideoRouteDidChange();
        }
    });

}
#endif

@end

#endif
