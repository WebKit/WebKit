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

#import <AVKit/AVKit.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/TZoneMallocInlines.h>

#import <pal/cf/CoreMediaSoftLink.h>

#define FOR_EACH_COMMON_READONLY_KEY_PATH(Macro) \
    Macro(timeRange, TimeRange, MediaTimeRange) \
    Macro(ready, Ready, bool) \
    Macro(buffering, Buffering, bool) \
    Macro(audioOptions, AudioOptions, Vector<MediaSelectionOption>) \
\

#define FOR_EACH_COMMON_READWRITE_KEY_PATH(Macro) \
    Macro(playing, Playing, bool) \
    Macro(playbackSpeed, PlaybackSpeed, float) \
    Macro(scanSpeed, ScanSpeed, float) \
    Macro(muted, Muted, bool) \
    Macro(volume, Volume, float) \
\

#define FOR_EACH_COMMON_KEY_PATH(Macro) \
    FOR_EACH_COMMON_READONLY_KEY_PATH(Macro) \
    FOR_EACH_COMMON_READWRITE_KEY_PATH(Macro) \
\

#define FOR_EACH_MEDIA_SOURCE_READONLY_KEY_PATH(Macro) \
    FOR_EACH_COMMON_READONLY_KEY_PATH(Macro) \
    Macro(playbackError, PlaybackError, std::optional<MediaPlaybackSourceError>) \
\

#define FOR_EACH_MEDIA_SOURCE_READWRITE_KEY_PATH(Macro) \
    FOR_EACH_COMMON_READWRITE_KEY_PATH(Macro) \
    Macro(currentPlaybackPosition, CurrentPlaybackPosition, MediaTime) \
\

#define FOR_EACH_MEDIA_SOURCE_KEY_PATH(Macro) \
    FOR_EACH_MEDIA_SOURCE_READONLY_KEY_PATH(Macro) \
    FOR_EACH_MEDIA_SOURCE_READWRITE_KEY_PATH(Macro) \
\

#define FOR_EACH_PLAYBACK_CONTROL_KEY_PATH(Macro) \
    FOR_EACH_COMMON_KEY_PATH(Macro) \
\

#define ADD_MEDIA_SOURCE_OBSERVER(KeyPath, SetterSuffix, Type) \
    [_mediaSource addObserver:self forKeyPath:@#KeyPath options:NSKeyValueObservingOptionInitial context:WebMediaSourceObserverContext]; \
\

#define REMOVE_MEDIA_SOURCE_OBSERVER(KeyPath, SetterSuffix, Type) \
    [_mediaSource removeObserver:self forKeyPath:@#KeyPath context:WebMediaSourceObserverContext]; \
\

#define ADD_PLAYBACK_CONTROL_OBSERVER(KeyPath, SetterSuffix, Type) \
    [_playbackControl addObserver:self forKeyPath:@#KeyPath options:NSKeyValueObservingOptionInitial context:WebPlaybackControlObserverContext]; \
\

#define REMOVE_PLAYBACK_CONTROL_OBSERVER(KeyPath, SetterSuffix, Type) \
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
        if (RetainPtr playbackControl = [m_mediaSourceObserver playbackControl]) \
            return convert(playbackControl.get().KeyPath); \
        return convert([m_mediaSourceObserver mediaSource].KeyPath); \
    } \
\

#define DEFINE_SETTER(KeyPath, SetterSuffix, Type) \
    void MediaDeviceRoute::set##SetterSuffix(Type KeyPath) \
    { \
        if (RetainPtr playbackControl = [m_mediaSourceObserver playbackControl]) \
            return [playbackControl set##SetterSuffix:convert(WTF::move(KeyPath))]; \
        [[m_mediaSourceObserver mediaSource] set##SetterSuffix:convert(WTF::move(KeyPath))]; \
    } \
\

NS_ASSUME_NONNULL_BEGIN

static void* WebMediaSourceObserverContext = &WebMediaSourceObserverContext;
static void* WebPlaybackControlObserverContext = &WebPlaybackControlObserverContext;

@interface WebMediaSourceObserver : NSObject
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithRoute:(WebCore::MediaDeviceRoute&)route NS_DESIGNATED_INITIALIZER;
@property (nonatomic, nullable, strong) AVMediaSource *mediaSource;
@property (nonatomic, nullable, strong) AVPlaybackControl *playbackControl;
@end

@implementation WebMediaSourceObserver {
    WeakPtr<WebCore::MediaDeviceRoute> _route;
    RetainPtr<AVMediaSource> _mediaSource;
    RetainPtr<AVPlaybackControl> _playbackControl;
}

- (instancetype)initWithRoute:(WebCore::MediaDeviceRoute&)route
{
    if (!(self = [super init]))
        return nil;

    _route = route;
    return self;
}

- (AVMediaSource * _Nullable)mediaSource
{
    return _mediaSource.get();
}

- (void)setMediaSource:(AVMediaSource * _Nullable)mediaSource
{
    if (mediaSource)
        self.playbackControl = nil;

    FOR_EACH_MEDIA_SOURCE_KEY_PATH(REMOVE_MEDIA_SOURCE_OBSERVER)

    _mediaSource = mediaSource;

    FOR_EACH_MEDIA_SOURCE_KEY_PATH(ADD_MEDIA_SOURCE_OBSERVER)
}

- (AVPlaybackControl * _Nullable)playbackControl
{
    return _playbackControl.get();
}

- (void)setPlaybackControl:(AVPlaybackControl * _Nullable)playbackControl
{
    if (playbackControl)
        self.mediaSource = nil;

    FOR_EACH_PLAYBACK_CONTROL_KEY_PATH(REMOVE_PLAYBACK_CONTROL_OBSERVER)
    REMOVE_PLAYBACK_CONTROL_OBSERVER(error, Error, std::optional<MediaPlaybackSourceError>)
    REMOVE_PLAYBACK_CONTROL_OBSERVER(playbackPosition, PlaybackPosition, AVPlaybackUserInterfacePlaybackPosition *)

    _playbackControl = playbackControl;

    FOR_EACH_PLAYBACK_CONTROL_KEY_PATH(ADD_PLAYBACK_CONTROL_OBSERVER)
    ADD_PLAYBACK_CONTROL_OBSERVER(error, Error, std::optional<MediaPlaybackSourceError>)
    ADD_PLAYBACK_CONTROL_OBSERVER(playbackPosition, PlaybackPosition, AVPlaybackUserInterfacePlaybackPosition *)
}

- (void)observeValueForKeyPath:(nullable NSString *)keyPath ofObject:(nullable id)object change:(nullable NSDictionary *)change context:(nullable void*)context
{
    if (context != WebMediaSourceObserverContext && context != WebPlaybackControlObserverContext) {
        [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
        return;
    }

    dispatch_async(mainDispatchQueueSingleton(), ^{
        if (context == WebMediaSourceObserverContext) {
            FOR_EACH_MEDIA_SOURCE_KEY_PATH(OBSERVE_VALUE)
            ASSERT_NOT_REACHED();
            return;
        }

        if ([keyPath isEqualToString:@"error"]) {
            if (RefPtr route = _route.get()) {
                if (RefPtr client = route->client())
                    client->playbackErrorDidChange(*route);
            }
            return;
        }
        if ([keyPath isEqualToString:@"playbackPosition"]) {
            if (RefPtr route = _route.get()) {
                if (RefPtr client = route->client())
                    client->currentPlaybackPositionDidChange(*route);
            }
            return;
        }

        FOR_EACH_PLAYBACK_CONTROL_KEY_PATH(OBSERVE_VALUE)
        ASSERT_NOT_REACHED();
    });
}

- (void)dealloc
{
    FOR_EACH_MEDIA_SOURCE_KEY_PATH(REMOVE_MEDIA_SOURCE_OBSERVER)
    FOR_EACH_PLAYBACK_CONTROL_KEY_PATH(REMOVE_PLAYBACK_CONTROL_OBSERVER)
    REMOVE_PLAYBACK_CONTROL_OBSERVER(error, Error, std::optional<MediaPlaybackSourceError>)
    REMOVE_PLAYBACK_CONTROL_OBSERVER(playbackPosition, PlaybackPosition, AVPlaybackUserInterfacePlaybackPosition *)
    [super dealloc];
}

@end

NS_ASSUME_NONNULL_END

#import <WebKitAdditions/MediaDeviceRouteAdditions.mm>

namespace WebCore {

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
            [option displayName],
            [option identifier],
            MediaSelectionOption::Type::Audio,
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
    , m_mediaSourceObserver { adoptNS([[WebMediaSourceObserver alloc] initWithRoute:*this]) }
{
}

String MediaDeviceRoute::deviceName() const
{
    return [m_platformRoute routeDisplayName];
}

WebMediaDevicePlatformRoute *MediaDeviceRoute::platformRoute() const
{
    return m_platformRoute.get();
}

std::optional<MediaPlaybackSourceError> MediaDeviceRoute::playbackError() const
{
    if (RetainPtr playbackControl = [m_mediaSourceObserver playbackControl])
        return convert(playbackControl.get().error);
    return convert([m_mediaSourceObserver mediaSource].playbackError);
}

MediaTime MediaDeviceRoute::currentPlaybackPosition() const
{
    if (RetainPtr playbackControl = [m_mediaSourceObserver playbackControl])
        return convert(playbackControl.get().playbackPosition.position);
    return convert([m_mediaSourceObserver mediaSource].currentPlaybackPosition);
}

void MediaDeviceRoute::setCurrentPlaybackPosition(MediaTime currentPlaybackPosition)
{
    if (RetainPtr playbackControl = [m_mediaSourceObserver playbackControl]) {
        // FIXME: We should introduce a proper seek-with-tolerance function on MediaDeviceRoute rather than assuming a zero tolerance here.
        [playbackControl seekToPosition:convert(WTF::move(currentPlaybackPosition)) tolerance:PAL::kCMTimeZero];
        return;
    }

    [m_mediaSourceObserver mediaSource].currentPlaybackPosition = convert(WTF::move(currentPlaybackPosition));
}

MediaDeviceRoute::~MediaDeviceRoute()
{
#if HAVE(AVROUTING_FRAMEWORK)
    [m_routeSession stop];
#endif
}

FOR_EACH_COMMON_KEY_PATH(DEFINE_GETTER)
FOR_EACH_COMMON_READWRITE_KEY_PATH(DEFINE_SETTER)

} // namespace WebCore

#undef FOR_EACH_COMMON_READONLY_KEY_PATH
#undef FOR_EACH_COMMON_READWRITE_KEY_PATH
#undef FOR_EACH_COMMON_KEY_PATH
#undef FOR_EACH_MEDIA_SOURCE_READONLY_KEY_PATH
#undef FOR_EACH_MEDIA_SOURCE_READWRITE_KEY_PATH
#undef FOR_EACH_MEDIA_SOURCE_KEY_PATH
#undef FOR_EACH_PLAYBACK_CONTROL_KEY_PATH
#undef ADD_MEDIA_SOURCE_OBSERVER
#undef REMOVE_MEDIA_SOURCE_OBSERVER
#undef ADD_PLAYBACK_CONTROL_OBSERVER
#undef REMOVE_PLAYBACK_CONTROL_OBSERVER
#undef OBSERVE_VALUE
#undef DEFINE_GETTER
#undef DEFINE_SETTER

#endif // ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)
