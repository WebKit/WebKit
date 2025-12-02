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

#if HAVE(AVROUTING_FRAMEWORK)

#include <WebKitAdditions/MediaDeviceRouteImplementationAdditions.h>
#include <wtf/TZoneMallocInlines.h>

#define FOR_EACH_READONLY_KEY_PATH(Macro) \
    Macro(minValue, MinValue, float) \
    Macro(maxValue, MinValue, float) \
    Macro(segments, Segments, Vector<MediaTimelineSegment>) \
    Macro(currentSegment, CurrentSegment, std::optional<MediaTimelineSegment>) \
    Macro(state, State, MediaPlaybackSourceState) \
    Macro(supportedModes, SupportedModes, OptionSet<MediaPlaybackSourceSupportedMode>) \
    Macro(playbackType, PlaybackType, OptionSet<MediaPlaybackSourcePlaybackType>) \
    Macro(playbackError, PlaybackError, std::optional<MediaPlaybackSourceError>) \
    Macro(options, Options, Vector<MediaSelectionOption>) \
    Macro(hasAudio, HasAudio, bool) \
\

#define FOR_EACH_READWRITE_KEY_PATH(Macro) \
    Macro(currentValue, CurrentValue, float) \
    Macro(isPlaying, IsPlaying, bool) \
    Macro(playbackSpeed, PlaybackSpeed, double) \
    Macro(scanSpeed, ScanSpeed, double) \
    Macro(currentAudioOption, CurrentAudioOption, std::optional<MediaSelectionOption>) \
    Macro(currentSubtitleOption, CurrentSubtitleOption, std::optional<MediaSelectionOption>) \
    Macro(muted, Muted, bool) \
    Macro(volume, Volume, double) \
\

#define FOR_EACH_KEY_PATH(Macro) \
    FOR_EACH_READONLY_KEY_PATH(Macro) \
    FOR_EACH_READWRITE_KEY_PATH(Macro) \
\

#define ADD_OBSERVER(KeyPath, SetterSuffix, Type) \
    [_platformRoute addObserver:self forKeyPath:@#KeyPath options:0 context:WebMediaDeviceRouteObserverContext]; \
\

#define REMOVE_OBSERVER(KeyPath, SetterSuffix, Type) \
    [_platformRoute removeObserver:self forKeyPath:@#KeyPath context:WebMediaDeviceRouteObserverContext]; \
\

#define OBSERVE_VALUE(KeyPath, SetterSuffix, Type) \
    if ([keyPath isEqualToString:@#KeyPath]) { \
        if (RefPtr route = _route.get()) { \
            if (RefPtr client = route->client()) \
                client->KeyPath##DidChange(*route); \
        } \
        return; \
    } \
\

#define DEFINE_GETTER(KeyPath, SetterSuffix, Type) \
    Type MediaDeviceRoute::KeyPath() const \
    { \
        return convert([[m_route platformRoute] KeyPath]); \
    } \
\

#define DEFINE_SETTER(KeyPath, SetterSuffix, Type) \
    void MediaDeviceRoute::set##SetterSuffix(Type KeyPath) \
    { \
        [[m_route platformRoute] set##SetterSuffix:convert(WTFMove(KeyPath))]; \
    } \
\

NS_ASSUME_NONNULL_BEGIN

static void* WebMediaDeviceRouteObserverContext = &WebMediaDeviceRouteObserverContext;

@interface WebMediaSelectionOption : NSObject <AVMediaSelectionOptionSource>
@end

@interface WebMediaSelectionOption ()
@property (nonatomic, copy) NSString *displayName;
@property (nonatomic, copy) NSString *identifier;
@property (nonatomic) AVMediaOptionType type;
@property (nonatomic, copy) NSString *extendedLanguageTag;
@end

@implementation WebMediaSelectionOption
@end

@interface WebMediaDeviceRoute : NSObject
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithRoute:(WebCore::MediaDeviceRoute&)route platformRoute:(WebMediaDevicePlatformRoute *)platformRoute NS_DESIGNATED_INITIALIZER;
@property (nonatomic, readonly, strong) WebMediaDevicePlatformRoute *platformRoute;
@end

@implementation WebMediaDeviceRoute {
    WeakPtr<WebCore::MediaDeviceRoute> _route;
    RetainPtr<WebMediaDevicePlatformRoute> _platformRoute;
}

- (instancetype)initWithRoute:(WebCore::MediaDeviceRoute&)route platformRoute:(WebMediaDevicePlatformRoute *)platformRoute
{
    if (!(self = [super init]))
        return nil;

    _route = route;
    _platformRoute = platformRoute;
    FOR_EACH_KEY_PATH(ADD_OBSERVER)
    return self;
}

- (WebMediaDevicePlatformRoute *)platformRoute
{
    return _platformRoute.get();
}

- (void)observeValueForKeyPath:(nullable NSString *)keyPath ofObject:(nullable id)object change:(nullable NSDictionary *)change context:(nullable void*)context
{
    if (context != WebMediaDeviceRouteObserverContext) {
        [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
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

namespace WebCore {

static double convert(double value)
{
    return value;
}

static bool convert(bool value)
{
    return value;
}

static MediaTimelineSegment::Type convert(AVMediaTimelineSegmentType segmentType)
{
    switch (segmentType) {
    case AVMediaTimelineSegmentTypePrimary:
        return MediaTimelineSegment::Type::Primary;
    case AVMediaTimelineSegmentTypeSecondary:
        return MediaTimelineSegment::Type::Secondary;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static MediaTimeRange convert(CMTimeRange timeRange)
{
    MediaTime start = PAL::toMediaTime(timeRange.start);
    return { WTFMove(start), start + PAL::toMediaTime(timeRange.duration) };
}

static std::optional<MediaTimelineSegment> convert(id<AVMediaTimelineSegment> _Nullable segment)
{
    if (!segment)
        return std::nullopt;

    return MediaTimelineSegment {
        convert(segment.type),
        segment.isMarked,
        segment.requiresLinearPlayback,
        convert(segment.timeRange),
        segment.identifier,
    };
}

static Vector<MediaTimelineSegment> convert(NSArray<AVMediaTimelineSegment> * _Nullable segments)
{
    Vector<MediaTimelineSegment> result;

    for (id<AVMediaTimelineSegment> segment in segments)
        result.append(*convert(segment));

    return result;
}

static MediaPlaybackSourceState convert(AVMediaPlaybackSourceState state)
{
    switch (state) {
    case AVMediaPlaybackSourceStateReady:
        return MediaPlaybackSourceState::Ready;
    case AVMediaPlaybackSourceStateLoading:
        return MediaPlaybackSourceState::Loading;
    case AVMediaPlaybackSourceStateSeeking:
        return MediaPlaybackSourceState::Seeking;
    case AVMediaPlaybackSourceStateScanning:
        return MediaPlaybackSourceState::Scanning;
    case AVMediaPlaybackSourceStateScrubbing:
        return MediaPlaybackSourceState::Scrubbing;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static OptionSet<MediaPlaybackSourceSupportedMode> convert(AVMediaPlaybackSourceSupportedMode supportedModes)
{
    OptionSet<MediaPlaybackSourceSupportedMode> result;

    if (supportedModes & AVMediaPlaybackSourceSupportedModeScanForward)
        result.add(MediaPlaybackSourceSupportedMode::ScanForward);
    if (supportedModes & AVMediaPlaybackSourceSupportedModeScanBackward)
        result.add(MediaPlaybackSourceSupportedMode::ScanBackward);
    if (supportedModes & AVMediaPlaybackSourceSupportedModeSeek)
        result.add(MediaPlaybackSourceSupportedMode::Seek);

    return result;
}

static OptionSet<MediaPlaybackSourcePlaybackType> convert(AVMediaPlaybackSourcePlaybackType playbackType)
{
    OptionSet<MediaPlaybackSourcePlaybackType> result;

    if (playbackType & AVMediaPlaybackSourcePlaybackTypeRegular)
        result.add(MediaPlaybackSourcePlaybackType::Regular);
    if (playbackType & AVMediaPlaybackSourcePlaybackTypeLive)
        result.add(MediaPlaybackSourcePlaybackType::Live);

    return result;
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

static MediaSelectionOption::Type convert(AVMediaOptionType type)
{
    switch (type) {
    case AVMediaOptionTypeAudio:
        return MediaSelectionOption::Type::Audio;
    case AVMediaOptionTypeLegible:
        return MediaSelectionOption::Type::Legible;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static AVMediaOptionType convert(MediaSelectionOption::Type type)
{
    switch (type) {
    case MediaSelectionOption::Type::Audio:
        return AVMediaOptionTypeAudio;
    case MediaSelectionOption::Type::Legible:
        return AVMediaOptionTypeLegible;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static std::optional<MediaSelectionOption> convert(id<AVMediaSelectionOptionSource> _Nullable option)
{
    if (!option)
        return std::nullopt;

    return MediaSelectionOption {
        option.displayName,
        option.identifier,
        convert(option.type),
        option.extendedLanguageTag,
    };
}

static id<AVMediaSelectionOptionSource> _Nullable convert(const std::optional<MediaSelectionOption>& option)
{
    if (!option)
        return nil;

    RetainPtr result = adoptNS([[WebMediaSelectionOption alloc] init]);
    [result setDisplayName:option->displayName.createNSString().get()];
    [result setIdentifier:option->identifier.createNSString().get()];
    [result setType:convert(option->type)];
    [result setExtendedLanguageTag:option->extendedLanguageTag.createNSString().get()];

    return result.autorelease();
}

static Vector<MediaSelectionOption> convert(NSArray<AVMediaSelectionOptionSource> * _Nullable options)
{
    Vector<MediaSelectionOption> result;

    for (id<AVMediaSelectionOptionSource> option in options)
        result.append(*convert(option));

    return result;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaDeviceRoute);

Ref<MediaDeviceRoute> MediaDeviceRoute::create(WebMediaDevicePlatformRoute *platformRoute)
{
    return adoptRef(*new MediaDeviceRoute(platformRoute));
}

MediaDeviceRoute::MediaDeviceRoute(WebMediaDevicePlatformRoute *platformRoute)
    : m_route { adoptNS([[WebMediaDeviceRoute alloc] initWithRoute:*this platformRoute:platformRoute]) }
{
}

MediaDeviceRoute::~MediaDeviceRoute() = default;

FOR_EACH_KEY_PATH(DEFINE_GETTER)
FOR_EACH_READWRITE_KEY_PATH(DEFINE_SETTER)

} // namespace WebCore

NS_ASSUME_NONNULL_END

#undef FOR_EACH_READONLY_KEY_PATH
#undef FOR_EACH_READWRITE_KEY_PATH
#undef FOR_EACH_KEY_PATH
#undef ADD_OBSERVER
#undef OBSERVE_VALUE
#undef DEFINE_GETTER
#undef DEFINE_SETTER

#endif // HAVE(AVROUTING_FRAMEWORK)
