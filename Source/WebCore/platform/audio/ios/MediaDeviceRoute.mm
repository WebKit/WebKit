/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "MediaDeviceRoute.h"

#if ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)

#import "Logging.h"
#import "MediaSelectionOption.h"
#import "WebMediaDevicePlatformRoute.h"
#import <AVKit/AVKit.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>
#import <wtf/RunLoop.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/TZoneMallocInlines.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/ios/AVSystemRoutingSoftLink.h>

#define FOR_EACH_READONLY_KEY_PATH(Macro) \
    Macro(timeRange, TimeRange, MediaTimeRange) \
    Macro(ready, Ready, bool) \
    Macro(buffering, Buffering, bool) \
    Macro(audioOptions, AudioOptions, Vector<MediaSelectionOption>) \
    Macro(error, Error, std::optional<MediaPlaybackSourceError>) \
    Macro(playbackPosition, PlaybackPosition, MediaTime) \
\

#define FOR_EACH_READWRITE_KEY_PATH(Macro) \
    Macro(playing, Playing, bool) \
    Macro(playbackSpeed, PlaybackSpeed, float) \
    Macro(scanSpeed, ScanSpeed, float) \
    Macro(muted, Muted, bool) \
    Macro(volume, Volume, float) \
\

#define FOR_EACH_KEY_PATH(Macro) \
    FOR_EACH_READONLY_KEY_PATH(Macro) \
    FOR_EACH_READWRITE_KEY_PATH(Macro) \
\

#define ADD_OBSERVER(KeyPath, SetterSuffix, Type) \
    [_playbackControl addObserver:self forKeyPath:@#KeyPath options:NSKeyValueObservingOptionInitial context:WebPlaybackControlObserverContext]; \
\

#define REMOVE_OBSERVER(KeyPath, SetterSuffix, Type) \
    [_playbackControl removeObserver:self forKeyPath:@#KeyPath context:WebPlaybackControlObserverContext]; \
\

#define NOTIFY_CLIENT(KeyPath, SetterSuffix, Type) \
    if (RefPtr route = _route.get()) { \
        if (RefPtr client = route->client()) \
            client->KeyPath##DidChange(*route); \
    } \
\

#define OBSERVE_VALUE(KeyPath, SetterSuffix, Type) \
    if ([keyPath isEqualToString:@#KeyPath]) { \
        NOTIFY_CLIENT(KeyPath, SetterSuffix, Type) \
        return; \
    } \
\

#define DEFINE_GETTER(KeyPath, SetterSuffix, Type) \
    Type MediaDeviceRoute::KeyPath() const \
    { \
        return convert([m_playbackControlObserver playbackControl].KeyPath); \
    } \
\

#define DEFINE_SETTER(KeyPath, SetterSuffix, Type) \
    void MediaDeviceRoute::set##SetterSuffix(Type KeyPath) \
    { \
        [[m_playbackControlObserver playbackControl] set##SetterSuffix:convert(WTF::move(KeyPath))]; \
    } \
\

NS_ASSUME_NONNULL_BEGIN

static void* WebPlaybackControlObserverContext = &WebPlaybackControlObserverContext;

@interface WebPlaybackControlObserver : NSObject
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithRoute:(WebCore::MediaDeviceRoute&)route NS_DESIGNATED_INITIALIZER;
@property (nonatomic, nullable, strong) NSObject<AVPlaybackUserInterfaceControllable> *playbackControl;
@end

@implementation WebPlaybackControlObserver {
    WeakPtr<WebCore::MediaDeviceRoute> _route;
    RetainPtr<NSObject<AVPlaybackUserInterfaceControllable>> _playbackControl;
}

- (instancetype)initWithRoute:(WebCore::MediaDeviceRoute&)route
{
    if (!(self = [super init]))
        return nil;

    _route = route;
    return self;
}

- (NSObject<AVPlaybackUserInterfaceControllable> * _Nullable)playbackControl
{
    return _playbackControl.get();
}

- (void)setPlaybackControl:(NSObject<AVPlaybackUserInterfaceControllable> * _Nullable)playbackControl
{
    FOR_EACH_KEY_PATH(REMOVE_OBSERVER)

    _playbackControl = playbackControl;

    FOR_EACH_KEY_PATH(ADD_OBSERVER)
}

- (void)observeValueForKeyPath:(nullable NSString *)keyPath ofObject:(nullable id)object change:(nullable NSDictionary *)change context:(nullable void*)context
{
    if (context != WebPlaybackControlObserverContext) {
        [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
        return;
    }

    dispatch_async(mainDispatchQueueSingleton(), ^{
        FOR_EACH_KEY_PATH(OBSERVE_VALUE)
        ASSERT_NOT_REACHED();
    });
}

- (void)dealloc
{
    FOR_EACH_KEY_PATH(REMOVE_OBSERVER)
    [super dealloc];
}

@end

NS_ASSUME_NONNULL_END

namespace WebCore {

void MediaDeviceRoute::loadURL(const URL& url, CompletionHandler<void(const MediaDeviceRouteLoadURLResult&)>&& completionHandler)
{
    RetainPtr platformURL = url.createNSURL();
    if (!platformURL)
        return completionHandler(makeUnexpected(MediaDeviceRouteLoadURLError::InvalidURL));

    disconnectFromSession();

#if HAVE(AVSYSTEMROUTING_FRAMEWORK)
    m_routeSession = adoptNS([PAL::allocAVSystemRouteSessionInstance() initWithURL:platformURL.get() mode:AVSystemRouteLaunchModePlayer]);
    [platformRoute() addSession:m_routeSession.get()];

    auto completionBlock = makeBlockPtr([weakThis = WeakPtr { *this }, completionHandler = WTF::move(completionHandler)](NSError * _Nullable error, AVSystemRouteMediaSession * _Nullable mediaSession) mutable {
        RunLoop::mainSingleton().dispatch([weakThis = WTF::move(weakThis), completionHandler = WTF::move(completionHandler), error = RetainPtr { error }, mediaSession = RetainPtr { mediaSession }]() mutable {
            if (!!error)
                return completionHandler(makeUnexpected(MediaDeviceRouteLoadURLError::PlatformError));

            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return completionHandler(makeUnexpected(MediaDeviceRouteLoadURLError::NoRoute));

            RetainPtr<id> playbackControl = [mediaSession playbackControl];
            if ([playbackControl conformsToProtocol:@protocol(AVPlaybackUserInterfaceControllable)])
                [protectedThis->m_playbackControlObserver setPlaybackControl:(NSObject<AVPlaybackUserInterfaceControllable> *)playbackControl.get()];
            else
                RELEASE_LOG_ERROR(Media, "MediaDeviceRoute::loadURL: playbackControl does not conform to AVPlaybackUserInterfaceControllable");
            completionHandler({ });
        });
    });

    [m_routeSession startWithCompletionHandler:completionBlock.get()];
#else
    auto completionBlock = makeBlockPtr([weakThis = WeakPtr { *this }, completionHandler = WTF::move(completionHandler)](NSError * _Nullable error, NSObject<AVPlaybackUserInterfaceControllable> * _Nullable playbackControl) mutable {
        if (!!error)
            return completionHandler(makeUnexpected(MediaDeviceRouteLoadURLError::PlatformError));

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return completionHandler(makeUnexpected(MediaDeviceRouteLoadURLError::NoRoute));

        [protectedThis->m_playbackControlObserver setPlaybackControl:playbackControl];
        completionHandler({ });
    });

    [platformRoute() startWithURL:platformURL.get() completionHandler:completionBlock.get()];
#endif
}

static float convert(float value)
{
    return value;
}

static bool convert(bool value)
{
    return value;
}

static CMTime convert(MediaTime time)
{
    return PAL::toCMTime(time);
}

static MediaTime convert(CMTime time)
{
    return PAL::toMediaTime(time);
}

static MediaTime convert(AVPlaybackUserInterfacePlaybackPosition *playbackPosition)
{
    return convert(playbackPosition.position);
}

static MediaTimeRange convert(CMTimeRange timeRange)
{
    MediaTime start = PAL::toMediaTime(timeRange.start);
    return { WTF::move(start), start + PAL::toMediaTime(timeRange.duration) };
}

static std::optional<MediaPlaybackSourceError> convert(NSError * _Nullable error)
{
    if (!error)
        return std::nullopt;

    return MediaPlaybackSourceError {
        error.code,
        error.domain,
        error.localizedDescription,
    };
}

static Vector<MediaSelectionOption> convert(NSArray * _Nullable options)
{
    return Vector<MediaSelectionOption>(options.count, [&](size_t i) {
        id option = options[i];
        return MediaSelectionOption {
            MediaSelectionOption::MediaType::Audio,
            [option displayName],
            MediaSelectionOption::LegibleType::Regular,
            [option extendedLanguageTag],
        };
    });
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaDeviceRoute);

Ref<MediaDeviceRoute> MediaDeviceRoute::create(WebMediaDevicePlatformRoute *platformRoute)
{
    return adoptRef(*new MediaDeviceRoute(platformRoute));
}

MediaDeviceRoute::MediaDeviceRoute(WebMediaDevicePlatformRoute *platformRoute)
    : m_identifier { WTF::UUID::createVersion4() }
    , m_platformRoute { platformRoute }
    , m_playbackControlObserver { adoptNS([[WebPlaybackControlObserver alloc] initWithRoute:*this]) }
{
}

void MediaDeviceRoute::disconnectFromSession()
{
    [m_playbackControlObserver setPlaybackControl:nil];

#if HAVE(AVSYSTEMROUTING_FRAMEWORK)
    if (RetainPtr routeSession = std::exchange(m_routeSession, nil)) {
        [routeSession stop];
        [platformRoute() removeSession:routeSession.get()];
    }
#else
    [platformRoute() stop];
#endif
}

String MediaDeviceRoute::deviceName() const
{
    return [m_platformRoute routeDisplayName];
}

String MediaDeviceRoute::routeName() const
{
    return [m_platformRoute protocolType].localizedDescription;
}

WebMediaDevicePlatformRoute *MediaDeviceRoute::platformRoute() const
{
    return m_platformRoute.get();
}

void MediaDeviceRoute::setPlaybackPosition(MediaTime playbackPosition)
{
    // FIXME: We should introduce a proper seek-with-tolerance function on MediaDeviceRoute rather than assuming a zero tolerance here.
    [[m_playbackControlObserver playbackControl] seekToPosition:convert(WTF::move(playbackPosition)) tolerance:PAL::kCMTimeZero];
}

MediaDeviceRoute::~MediaDeviceRoute()
{
    disconnectFromSession();
}

FOR_EACH_KEY_PATH(DEFINE_GETTER)
FOR_EACH_READWRITE_KEY_PATH(DEFINE_SETTER)

} // namespace WebCore

#undef FOR_EACH_READONLY_KEY_PATH
#undef FOR_EACH_READWRITE_KEY_PATH
#undef FOR_EACH_KEY_PATH
#undef ADD_OBSERVER
#undef REMOVE_OBSERVER
#undef OBSERVE_VALUE
#undef DEFINE_GETTER
#undef DEFINE_SETTER

#endif // ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)
