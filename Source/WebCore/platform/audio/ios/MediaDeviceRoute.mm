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

#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <wtf/TZoneMallocInlines.h>

#import <pal/cf/CoreMediaSoftLink.h>

#define FOR_EACH_READONLY_KEY_PATH(Macro) \
    Macro(timeRange, TimeRange, MediaTimeRange) \
    Macro(ready, Ready, bool) \
    Macro(buffering, Buffering, bool) \
    Macro(playbackError, PlaybackError, std::optional<MediaPlaybackSourceError>) \
    Macro(hasAudio, HasAudio, bool) \
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
    [_mediaSource addObserver:self forKeyPath:@#KeyPath options:NSKeyValueObservingOptionInitial context:WebMediaSourceObserverContext]; \
\

#define REMOVE_OBSERVER(KeyPath, SetterSuffix, Type) \
    [_mediaSource removeObserver:self forKeyPath:@#KeyPath context:WebMediaSourceObserverContext]; \
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
        return convert([m_mediaSourceObserver mediaSource].KeyPath); \
    } \
\

#define DEFINE_SETTER(KeyPath, SetterSuffix, Type) \
    void MediaDeviceRoute::set##SetterSuffix(Type KeyPath) \
    { \
        [[m_mediaSourceObserver mediaSource] set##SetterSuffix:convert(WTF::move(KeyPath))]; \
    } \
\

NS_ASSUME_NONNULL_BEGIN

@interface NSObject (Staging_169033633)
@property (nonatomic) CMTime currentPlaybackPosition;
@property (nonatomic) CMTime currentValue;
@end

static void* WebMediaSourceObserverContext = &WebMediaSourceObserverContext;

@interface WebMediaSourceObserver : NSObject
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithRoute:(WebCore::MediaDeviceRoute&)route NS_DESIGNATED_INITIALIZER;
@property (nonatomic, nullable, strong) AVMediaSource *mediaSource;
@end

@implementation WebMediaSourceObserver {
    WeakPtr<WebCore::MediaDeviceRoute> _route;
    RetainPtr<AVMediaSource> _mediaSource;
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
    FOR_EACH_KEY_PATH(REMOVE_OBSERVER)
    REMOVE_OBSERVER(currentPlaybackPosition, CurrentPlaybackPosition, CMTime)
    REMOVE_OBSERVER(currentValue, CurrentValue, CMTime)

    _mediaSource = mediaSource;

    FOR_EACH_KEY_PATH(ADD_OBSERVER)
    ADD_OBSERVER(currentPlaybackPosition, CurrentPlaybackPosition, CMTime)
    ADD_OBSERVER(currentValue, CurrentValue, CMTime)
}

- (void)observeValueForKeyPath:(nullable NSString *)keyPath ofObject:(nullable id)object change:(nullable NSDictionary *)change context:(nullable void*)context
{
    if (context != WebMediaSourceObserverContext) {
        [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
        return;
    }

    if ([keyPath isEqualToString:@"currentValue"] || [keyPath isEqualToString:@"currentPlaybackPosition"]) {
        if (RefPtr route = _route.get()) {
            if (RefPtr client = route->client())
                client->currentPlaybackPositionDidChange(*route);
        }
        return;
    }

    FOR_EACH_KEY_PATH(OBSERVE_VALUE)
    ASSERT_NOT_REACHED();
}

- (void)dealloc
{
    FOR_EACH_KEY_PATH(REMOVE_OBSERVER)
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

MediaTime MediaDeviceRoute::currentPlaybackPosition() const
{
    if ([[m_mediaSourceObserver mediaSource] respondsToSelector:@selector(currentPlaybackPosition)])
        return convert([[m_mediaSourceObserver mediaSource] currentPlaybackPosition]);

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return convert([m_mediaSourceObserver mediaSource].currentValue);
ALLOW_DEPRECATED_DECLARATIONS_END
}

void MediaDeviceRoute::setCurrentPlaybackPosition(MediaTime currentPlaybackPosition)
{
    if ([[m_mediaSourceObserver mediaSource] respondsToSelector:@selector(setCurrentPlaybackPosition:)]) {
        [[m_mediaSourceObserver mediaSource] setCurrentPlaybackPosition:convert(WTF::move(currentPlaybackPosition))];
        return;
    }

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [[m_mediaSourceObserver mediaSource] setCurrentValue:convert(WTF::move(currentPlaybackPosition))];
ALLOW_DEPRECATED_DECLARATIONS_END
}

MediaDeviceRoute::~MediaDeviceRoute()
{
#if HAVE(AVROUTING_FRAMEWORK)
    [m_routeSession stop];
#endif
}

FOR_EACH_KEY_PATH(DEFINE_GETTER)
FOR_EACH_READWRITE_KEY_PATH(DEFINE_SETTER)

} // namespace WebCore

#undef FOR_EACH_READONLY_KEY_PATH
#undef FOR_EACH_READWRITE_KEY_PATH
#undef FOR_EACH_KEY_PATH
#undef ADD_OBSERVER
#undef OBSERVE_VALUE
#undef DEFINE_GETTER
#undef DEFINE_SETTER

#endif // ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)
