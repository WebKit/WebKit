/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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
#import "PlatformMediaSessionManager.h"
#import "WebCoreThreadRun.h"
#import <AVFoundation/AVAudioSession.h>
#import <AVFoundation/AVRouteDetector.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <pal/spi/ios/CelestialSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/MainThread.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/UniqueRef.h>

#import <pal/cocoa/AVFoundationSoftLink.h>
#import <pal/ios/UIKitSoftLink.h>

WEBCORE_EXPORT NSString *WebUIApplicationWillResignActiveNotification = @"WebUIApplicationWillResignActiveNotification";
WEBCORE_EXPORT NSString *WebUIApplicationWillEnterForegroundNotification = @"WebUIApplicationWillEnterForegroundNotification";
WEBCORE_EXPORT NSString *WebUIApplicationDidBecomeActiveNotification = @"WebUIApplicationDidBecomeActiveNotification";
WEBCORE_EXPORT NSString *WebUIApplicationDidEnterBackgroundNotification = @"WebUIApplicationDidEnterBackgroundNotification";

#if HAVE(CELESTIAL)
SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(Celestial)
SOFT_LINK_CLASS_OPTIONAL(Celestial, AVSystemController)
SOFT_LINK_CONSTANT_MAY_FAIL(Celestial, AVSystemController_PIDToInheritApplicationStateFrom, NSString *)
SOFT_LINK_CONSTANT_MAY_FAIL(Celestial, AVSystemController_ServerConnectionDiedNotification, NSString *)
#endif

using namespace WebCore;

class MediaSessionHelperIOS;

@interface WebMediaSessionHelper : NSObject {
    ThreadSafeWeakPtr<MediaSessionHelperIOS> _callback;

#if !PLATFORM(WATCHOS)
    RetainPtr<AVRouteDetector> _routeDetector;
#endif
    bool _monitoringAirPlayRoutes;
    bool _startMonitoringAirPlayRoutesPending;
}

- (id)initWithCallback:(MediaSessionHelperIOS&)callback;

- (void)applicationWillEnterForeground:(NSNotification *)notification;
- (void)applicationWillResignActive:(NSNotification *)notification;
- (void)applicationDidEnterBackground:(NSNotification *)notification;
- (BOOL)hasWirelessTargetsAvailable;

#if !PLATFORM(WATCHOS)
- (void)startMonitoringAirPlayRoutes;
- (void)stopMonitoringAirPlayRoutes;
#endif

@end

class MediaSessionHelperIOS final : public MediaSessionHelper {
public:
    MediaSessionHelperIOS();

    void externalOutputDeviceAvailableDidChange();
    void updateCarPlayIsConnected();
#if HAVE(CELESTIAL)
    void mediaServerConnectionDied();
#endif
#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST) && !PLATFORM(WATCHOS)
    void activeAudioRouteDidChange(bool);
    void activeVideoRouteDidChange();
#endif

private:
    void setIsPlayingToAutomotiveHeadUnit(bool);

    void providePresentingApplicationPID(int, ShouldOverride) final;
    void startMonitoringWirelessRoutesInternal() final;
    void stopMonitoringWirelessRoutesInternal() final;

    RetainPtr<WebMediaSessionHelper> m_objcObserver;
#if HAVE(CELESTIAL)
    std::optional<int> m_presentedApplicationPID;
#endif
};

static RefPtr<MediaSessionHelper>& sharedHelperInstance()
{
    static NeverDestroyed<RefPtr<MediaSessionHelper>> helper;
    return helper;
}

MediaSessionHelper& MediaSessionHelper::sharedHelper()
{
    auto& helper = sharedHelperInstance();
    if (!helper)
        resetSharedHelper();

    ASSERT(helper);
    return *helper;
}

void MediaSessionHelper::resetSharedHelper()
{
    sharedHelperInstance() = adoptRef(*new MediaSessionHelperIOS());
}

void MediaSessionHelper::setSharedHelper(Ref<MediaSessionHelper>&& helper)
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

void MediaSessionHelper::activeAudioRouteDidChange(ShouldPause shouldPause)
{
    for (auto& client : m_clients)
        client.activeAudioRouteDidChange(shouldPause);
}

void MediaSessionHelper::applicationWillEnterForeground(SuspendedUnderLock suspendedUnderLock)
{
    for (auto& client : m_clients)
        client.applicationWillEnterForeground(suspendedUnderLock);
}

void MediaSessionHelper::applicationDidEnterBackground(SuspendedUnderLock suspendedUnderLock)
{
    for (auto& client : m_clients)
        client.applicationDidEnterBackground(suspendedUnderLock);
}

void MediaSessionHelper::applicationWillBecomeInactive()
{
    for (auto& client : m_clients)
        client.applicationWillBecomeInactive();
}

void MediaSessionHelper::applicationDidBecomeActive()
{
    for (auto& client : m_clients)
        client.applicationDidBecomeActive();
}

void MediaSessionHelper::externalOutputDeviceAvailableDidChange(HasAvailableTargets hasAvailableTargets)
{
    m_isExternalOutputDeviceAvailable = hasAvailableTargets == HasAvailableTargets::Yes;
    for (auto& client : m_clients)
        client.externalOutputDeviceAvailableDidChange(hasAvailableTargets);
}

void MediaSessionHelper::isPlayingToAutomotiveHeadUnitDidChange(PlayingToAutomotiveHeadUnit playingToAutomotiveHeadUnit)
{
    bool newValue = playingToAutomotiveHeadUnit == PlayingToAutomotiveHeadUnit::Yes;
    if (newValue == m_isPlayingToAutomotiveHeadUnit)
        return;

    m_isPlayingToAutomotiveHeadUnit = newValue;
    for (auto& client : m_clients)
        client.isPlayingToAutomotiveHeadUnitDidChange(playingToAutomotiveHeadUnit);
}

void MediaSessionHelper::activeVideoRouteDidChange(SupportsAirPlayVideo supportsAirPlayVideo, Ref<MediaPlaybackTarget>&& playbackTarget)
{
    m_playbackTarget = WTFMove(playbackTarget);
    m_activeVideoRouteSupportsAirPlayVideo = supportsAirPlayVideo == SupportsAirPlayVideo::Yes;
    for (auto& client : m_clients)
        client.activeVideoRouteDidChange(supportsAirPlayVideo, *m_playbackTarget);
}

void MediaSessionHelper::activeAudioRouteSupportsSpatialPlaybackDidChange(SupportsSpatialAudioPlayback supportsSpatialPlayback)
{
    if (m_activeAudioRouteSupportsSpatialPlayback == supportsSpatialPlayback)
        return;

    m_activeAudioRouteSupportsSpatialPlayback = supportsSpatialPlayback;
    for (auto& client : m_clients)
        client.activeAudioRouteSupportsSpatialPlaybackDidChange(supportsSpatialPlayback);
}

void MediaSessionHelper::startMonitoringWirelessRoutes()
{
    if (m_monitoringWirelessRoutesCount++)
        return;
    startMonitoringWirelessRoutesInternal();
}

void MediaSessionHelper::stopMonitoringWirelessRoutes()
{
    if (!m_monitoringWirelessRoutesCount) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (--m_monitoringWirelessRoutesCount)
        return;
    stopMonitoringWirelessRoutesInternal();
}

void MediaSessionHelper::updateActiveAudioRouteSupportsSpatialPlayback()
{
#if HAVE(AVAUDIOSESSION)
    AVAudioSession* audioSession = [PAL::getAVAudioSessionClass() sharedInstance];
    for (AVAudioSessionPortDescription* output in audioSession.currentRoute.outputs) {
        if (output.spatialAudioEnabled) {
            setActiveAudioRouteSupportsSpatialPlayback(true);
            return;
        }
    }
#endif // HAVE(AVAUDIOSESSION)

    setActiveAudioRouteSupportsSpatialPlayback(false);
}

void MediaSessionHelper::setActiveAudioRouteSupportsSpatialPlayback(bool supports)
{
    activeAudioRouteSupportsSpatialPlaybackDidChange(supports ? SupportsSpatialAudioPlayback::Yes : SupportsSpatialAudioPlayback::No);
}

MediaSessionHelperIOS::MediaSessionHelperIOS()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    m_objcObserver = adoptNS([[WebMediaSessionHelper alloc] initWithCallback:*this]);
    setIsExternalOutputDeviceAvailable([m_objcObserver hasWirelessTargetsAvailable]);
    END_BLOCK_OBJC_EXCEPTIONS

    updateCarPlayIsConnected();
}

void MediaSessionHelperIOS::providePresentingApplicationPID(int pid, ShouldOverride shouldOverride)
{
#if ENABLE(EXTENSION_CAPABILITIES)
    if (PlatformMediaSessionManager::mediaCapabilityGrantsEnabled())
        return;
#endif

#if HAVE(CELESTIAL)
    if (m_presentedApplicationPID && (*m_presentedApplicationPID == pid || shouldOverride == ShouldOverride::No))
        return;

    m_presentedApplicationPID = pid;

    if (!canLoadAVSystemController_PIDToInheritApplicationStateFrom())
        return;

    NSError *error = nil;
    [[getAVSystemControllerClass() sharedAVSystemController] setAttribute:@(pid) forKey:getAVSystemController_PIDToInheritApplicationStateFrom() error:&error];
    if (error)
        WTFLogAlways("Failed to set up PID proxying: %s", error.localizedDescription.UTF8String);
#else
    UNUSED_PARAM(pid);
    UNUSED_PARAM(shouldOverride);
#endif
}

void MediaSessionHelperIOS::startMonitoringWirelessRoutesInternal()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
#if !PLATFORM(WATCHOS)
    [m_objcObserver startMonitoringAirPlayRoutes];
#endif
    END_BLOCK_OBJC_EXCEPTIONS
}

void MediaSessionHelperIOS::stopMonitoringWirelessRoutesInternal()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
#if !PLATFORM(WATCHOS)
    [m_objcObserver stopMonitoringAirPlayRoutes];
#endif
    END_BLOCK_OBJC_EXCEPTIONS
}

#if HAVE(CELESTIAL)
void MediaSessionHelperIOS::mediaServerConnectionDied()
{
    if (m_presentedApplicationPID) {
        auto presentedApplicationPID = std::exchange(m_presentedApplicationPID, { });
        callOnMainRunLoop([presentedApplicationPID] {
            sharedHelper().providePresentingApplicationPID(*presentedApplicationPID);
        });
    }
}
#endif // HAVE(CELESTIAL)

void MediaSessionHelperIOS::updateCarPlayIsConnected()
{
#if HAVE(AVAUDIOSESSION)
    AVAudioSession *audioSession = [PAL::getAVAudioSessionClass() sharedInstance];
    for (AVAudioSessionPortDescription *output in audioSession.currentRoute.outputs) {
        if ([output.portType isEqualToString:AVAudioSessionPortCarAudio]) {
            setIsPlayingToAutomotiveHeadUnit(true);
            return;
        }
    }
#endif // HAVE(AVAUDIOSESSION)

    setIsPlayingToAutomotiveHeadUnit(false);
}

void MediaSessionHelperIOS::setIsPlayingToAutomotiveHeadUnit(bool isPlaying)
{
    isPlayingToAutomotiveHeadUnitDidChange(isPlaying ? PlayingToAutomotiveHeadUnit::Yes : PlayingToAutomotiveHeadUnit::No);
}

#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST) && !PLATFORM(WATCHOS)
void MediaSessionHelperIOS::activeAudioRouteDidChange(bool shouldPause)
{
    MediaSessionHelper::activeAudioRouteDidChange(shouldPause ? ShouldPause::Yes : ShouldPause::No);
}

void MediaSessionHelperIOS::activeVideoRouteDidChange()
{
    auto target = MediaPlaybackTargetCocoa::create();
    auto supportsRemoteVideoPlayback = target->supportsRemoteVideoPlayback() ? SupportsAirPlayVideo::Yes : SupportsAirPlayVideo::No;
    MediaSessionHelper::activeVideoRouteDidChange(supportsRemoteVideoPlayback, WTFMove(target));
}
#endif

void MediaSessionHelperIOS::externalOutputDeviceAvailableDidChange()
{
    HasAvailableTargets hasAvailableTargets;
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    hasAvailableTargets = [m_objcObserver hasWirelessTargetsAvailable] ? HasAvailableTargets::Yes : HasAvailableTargets::No;
    END_BLOCK_OBJC_EXCEPTIONS

    MediaSessionHelper::externalOutputDeviceAvailableDidChange(hasAvailableTargets);
}

@implementation WebMediaSessionHelper

- (id)initWithCallback:(MediaSessionHelperIOS&)callback
{
    LOG(Media, "-[WebMediaSessionHelper initWithCallback]");

    if (!(self = [super init]))
        return nil;

    _callback = callback;

    NSNotificationCenter *center = NSNotificationCenter.defaultCenter;
    [center addObserver:self selector:@selector(applicationWillEnterForeground:) name:PAL::get_UIKit_UIApplicationWillEnterForegroundNotification() object:nil];
    [center addObserver:self selector:@selector(applicationWillEnterForeground:) name:WebUIApplicationWillEnterForegroundNotification object:nil];
    [center addObserver:self selector:@selector(applicationDidBecomeActive:) name:PAL::get_UIKit_UIApplicationDidBecomeActiveNotification() object:nil];
    [center addObserver:self selector:@selector(applicationDidBecomeActive:) name:WebUIApplicationDidBecomeActiveNotification object:nil];
    [center addObserver:self selector:@selector(applicationWillResignActive:) name:PAL::get_UIKit_UIApplicationWillResignActiveNotification() object:nil];
    [center addObserver:self selector:@selector(applicationWillResignActive:) name:WebUIApplicationWillResignActiveNotification object:nil];
    [center addObserver:self selector:@selector(applicationDidEnterBackground:) name:PAL::get_UIKit_UIApplicationDidEnterBackgroundNotification() object:nil];
    [center addObserver:self selector:@selector(applicationDidEnterBackground:) name:WebUIApplicationDidEnterBackgroundNotification object:nil];
    [center addObserver:self selector:@selector(activeOutputDeviceDidChange:) name:PAL::get_AVFoundation_AVAudioSessionRouteChangeNotification() object:nil];
    [center addObserver:self selector:@selector(spatialPlaybackCapabilitiesChanged:) name:PAL::get_AVFoundation_AVAudioSessionSpatialPlaybackCapabilitiesChangedNotification() object:nil];

#if HAVE(CELESTIAL)
    if (canLoadAVSystemController_ServerConnectionDiedNotification())
        [center addObserver:self selector:@selector(mediaServerConnectionDied:) name:getAVSystemController_ServerConnectionDiedNotification() object:nil];
#endif

    // Now playing won't work unless we turn on the delivery of remote control events.
    RunLoop::main().dispatch([] {
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
        RunLoop::main().dispatch([routeDetector = std::exchange(_routeDetector, nil)]() {
            LOG(Media, "safelyTearDown - dipatched to UI thread.");
            BEGIN_BLOCK_OBJC_EXCEPTIONS
            [routeDetector setRouteDetectionEnabled:NO];
            END_BLOCK_OBJC_EXCEPTIONS
        });
    } else
        [_routeDetector setRouteDetectionEnabled:NO];
#endif

    [NSNotificationCenter.defaultCenter removeObserver:self];
    [super dealloc];
}

- (BOOL)hasWirelessTargetsAvailable
{
    LOG(Media, "-[WebMediaSessionHelper hasWirelessTargetsAvailable]");
#if !PLATFORM(WATCHOS)
    return [_routeDetector multipleRoutesDetected];
#else
    return NO;
#endif
}

#if !PLATFORM(WATCHOS)
- (void)startMonitoringAirPlayRoutes
{
    ASSERT(isMainThread());

    if (_monitoringAirPlayRoutes)
        return;

    _monitoringAirPlayRoutes = true;

    if (_startMonitoringAirPlayRoutesPending)
        return;

    if (_routeDetector) {
        [_routeDetector setRouteDetectionEnabled:YES];
        return;
    }

    _startMonitoringAirPlayRoutesPending = true;

    LOG(Media, "-[WebMediaSessionHelper startMonitoringAirPlayRoutes]");

    callOnWebThreadOrDispatchAsyncOnMainThread([self, protectedSelf = retainPtr(self)]() {
        ASSERT(!_routeDetector);

        if (auto callback = _callback.get()) {
            BEGIN_BLOCK_OBJC_EXCEPTIONS
            _routeDetector = adoptNS([PAL::allocAVRouteDetectorInstance() init]);
            [_routeDetector setRouteDetectionEnabled:_monitoringAirPlayRoutes];
            [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(wirelessRoutesAvailableDidChange:) name:PAL::AVRouteDetectorMultipleRoutesDetectedDidChangeNotification object:_routeDetector.get()];

            callback->externalOutputDeviceAvailableDidChange();
            END_BLOCK_OBJC_EXCEPTIONS
        }

        _startMonitoringAirPlayRoutesPending = false;
    });
}

- (void)stopMonitoringAirPlayRoutes
{
    ASSERT(isMainThread());

    if (!_monitoringAirPlayRoutes)
        return;

    LOG(Media, "-[WebMediaSessionHelper stopMonitoringAirPlayRoutes]");

    _monitoringAirPlayRoutes = false;
    [_routeDetector setRouteDetectionEnabled:NO];
}
#endif // !PLATFORM(WATCHOS)

- (void)applicationWillEnterForeground:(NSNotification *)notification
{
    using SuspendedUnderLock = MediaSessionHelperClient::SuspendedUnderLock;

    LOG(Media, "-[WebMediaSessionHelper applicationWillEnterForeground]");

    auto isSuspendedUnderLock = [[[notification userInfo] objectForKey:@"isSuspendedUnderLock"] boolValue] ? SuspendedUnderLock::Yes : SuspendedUnderLock::No;
    callOnWebThreadOrDispatchAsyncOnMainThread([self, protectedSelf = retainPtr(self), isSuspendedUnderLock]() {
        if (auto callback = _callback.get())
            callback->applicationWillEnterForeground(isSuspendedUnderLock);
    });
}

- (void)applicationDidBecomeActive:(NSNotification *)notification
{
    UNUSED_PARAM(notification);

    LOG(Media, "-[WebMediaSessionHelper applicationDidBecomeActive]");

    callOnWebThreadOrDispatchAsyncOnMainThread([self, protectedSelf = retainPtr(self)]() {
        if (auto callback = _callback.get())
            callback->applicationDidBecomeActive();
    });
}

- (void)applicationWillResignActive:(NSNotification *)notification
{
    UNUSED_PARAM(notification);

    LOG(Media, "-[WebMediaSessionHelper applicationWillResignActive]");

    callOnWebThreadOrDispatchAsyncOnMainThread([self, protectedSelf = retainPtr(self)]() {
        if (auto callback = _callback.get())
            callback->applicationWillBecomeInactive();
    });
}

- (void)wirelessRoutesAvailableDidChange:(NSNotification *)notification
{
    UNUSED_PARAM(notification);

    LOG(Media, "-[WebMediaSessionHelper wirelessRoutesAvailableDidChange]");

    callOnWebThreadOrDispatchAsyncOnMainThread([self, protectedSelf = retainPtr(self)]() {
        auto callback = _callback.get();
        if (callback && _monitoringAirPlayRoutes)
            callback->externalOutputDeviceAvailableDidChange();
    });
}

- (void)applicationDidEnterBackground:(NSNotification *)notification
{
    using SuspendedUnderLock = MediaSessionHelperClient::SuspendedUnderLock;

    LOG(Media, "-[WebMediaSessionHelper applicationDidEnterBackground]");

    auto isSuspendedUnderLock = [[[notification userInfo] objectForKey:@"isSuspendedUnderLock"] boolValue] ? SuspendedUnderLock::Yes : SuspendedUnderLock::No;
    callOnWebThreadOrDispatchAsyncOnMainThread([self, protectedSelf = retainPtr(self), isSuspendedUnderLock]() {
        if (auto callback = _callback.get())
            callback->applicationDidEnterBackground(isSuspendedUnderLock);
    });
}

#if HAVE(CELESTIAL)
- (void)mediaServerConnectionDied:(NSNotification *)notification
{
    LOG(Media, "-[WebMediaSessionHelper mediaServerConnectionDied:]");
    UNUSED_PARAM(notification);
    callOnWebThreadOrDispatchAsyncOnMainThread([self, protectedSelf = retainPtr(self)]() {
        if (auto callback = _callback.get())
            callback->mediaServerConnectionDied();
    });
}
#endif // HAVE(CELESTIAL)

- (void)activeOutputDeviceDidChange:(NSNotification *)notification
{
    LOG(Media, "-[WebMediaSessionHelper activeOutputDeviceDidChange:]");

    bool shouldPause = [[notification.userInfo objectForKey:AVAudioSessionRouteChangeReasonKey] unsignedIntegerValue] == AVAudioSessionRouteChangeReasonOldDeviceUnavailable;
    callOnWebThreadOrDispatchAsyncOnMainThread([self, protectedSelf = retainPtr(self), shouldPause]() {
        if (auto callback = _callback.get()) {
            callback->updateCarPlayIsConnected();
#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST) && !PLATFORM(WATCHOS)
            callback->activeAudioRouteDidChange(shouldPause);
            callback->activeVideoRouteDidChange();
#else
            UNUSED_PARAM(shouldPause);
#endif
        }
    });
}

- (void)spatialPlaybackCapabilitiesChanged:(NSNotification *)notification
{
    LOG(Media, "-[WebMediaSessionHelper spatialPlaybackCapabilitiesChanged:]");
    callOnWebThreadOrDispatchAsyncOnMainThread([self, protectedSelf = retainPtr(self)]() {
        if (auto callback = _callback.get())
            callback->updateActiveAudioRouteSupportsSpatialPlayback();
    });
}
@end

#endif // PLATFORM(IOS_FAMILY)
