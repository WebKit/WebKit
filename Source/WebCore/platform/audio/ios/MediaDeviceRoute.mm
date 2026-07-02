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

#import "MediaSelectionOption.h"
#import <AVKit/AVKit.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/TZoneMallocInlines.h>

#import <pal/cf/CoreMediaSoftLink.h>

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
@property (nonatomic, nullable, strong) AVPlaybackControl *playbackControl;
@end

@implementation WebPlaybackControlObserver {
    WeakPtr<WebCore::MediaDeviceRoute> _route;
    RetainPtr<AVPlaybackControl> _playbackControl;
}

- (instancetype)initWithRoute:(WebCore::MediaDeviceRoute&)route
{
    if (!(self = [super init]))
        return nil;

    _route = route;
    return self;
}

- (AVPlaybackControl * _Nullable)playbackControl
{
    return _playbackControl.get();
}

- (void)setPlaybackControl:(AVPlaybackControl * _Nullable)playbackControl
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

String MediaDeviceRoute::deviceName() const
{
    return [m_platformRoute routeDisplayName];
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
#undef REMOVE_OBSERVER
#undef OBSERVE_VALUE
#undef DEFINE_GETTER
#undef DEFINE_SETTER

#endif // ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)
