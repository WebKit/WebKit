
/*
 * Copyright (C) 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"

#if ENABLE(VIDEO) && PLATFORM(IOS)

#import "MediaPlayerPrivateIOS.h"

#import "GraphicsLayerCA.h"
#import "InbandTextTrackPrivateAVFIOS.h"
#import "Logging.h"
#import "PlatformTextTrack.h"
#import "TextTrackRepresentation.h"
#import "TimeRanges.h"
#import "URL.h"
#import "WAKAppKitStubs.h"
#import "WebCoreThreadRun.h"
#import <CoreGraphics/CoreGraphics.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/StdLibExtras.h>

using namespace WebCore;
using namespace std;

@interface WebCoreMediaPlayerNotificationHelper : NSObject
{
    MediaPlayerPrivateIOS* _client;
    BOOL _deferredPropertiesScheduled;
}
- (id)initWithClient:(MediaPlayerPrivateIOS *)client;
- (void)disconnect;
- (void)cancelPendingRequests;
- (void)delayNotification:(int)notification;
- (void)deliverNotification:(NSNumber *)notification;
- (void)schedulePrepareToPlayWithOptionalDelay:(NSNumber *)shouldDelay;
- (void)scheduleDeferredPropertiesWithOptionalDelay:(NSNumber *)shouldDelay;

- (void)pluginElementInBandAlternateTextTracksDidChange:(NSArray *)tracks;
- (void)pluginElementDidSelectTextTrack:(NSString *)trackId;
- (void)pluginElementDidOutputAttributedStrings:(NSArray *)strings nativeSampleBuffers:(NSArray *)buffers forTime:(NSTimeInterval)time;
@end

// Private selectors implemented by the media player proxy object returned by the plug-in.
@interface NSObject (WebMediaPlayerProxy_Extras)
- (void)_attributeChanged:(NSString *)name value:(NSString *)value;
- (BOOL)_readyForPlayback;
@end

namespace WebCore {

const int kRequiredHelperInterfaceVersion = 1;

static NSString * const DeferredPropertyControlsKey = @"controls";
static NSString * const DeferredPropertyPosterKey = @"poster";
static NSString * const DeferredPropertySrcKey = @"src";
static NSString * const DeferredPlayKey = @"play";
static NSString * const DeferredPropertySelectedTrackKey = @"selectedtrack";
static NSString * const DeferredPropertyOutOfBandTracksKey = @"outofbandtracks";
#if ENABLE(IOS_AIRPLAY)
static NSString * const DeferredWirelessVideoPlaybackDisabled = @"wirelessVideoPlaybackDisabled";
#endif

static NSString * const TextTrackDisplayNameKey = @"displayname";
static NSString * const TextTrackLocaleIdentifierKey = @"localeidentifier";
static NSString * const TextTrackSubtitlesKey = @"subtitles";
static NSString * const TextTrackCaptionsKey = @"captions";
static NSString * const TextTrackTypeKey = @"type";
static NSString * const TextTrackIDKey = @"id";

const int TextTrackOffMenuItemID = -1;
const int TextTrackAutoMenuItemID = -2;

MediaPlayerPrivateIOS::PlatformTextTrackMenuInterfaceIOS::PlatformTextTrackMenuInterfaceIOS(MediaPlayerPrivateIOS* owner)
    : m_client(nullptr)
    , m_owner(owner)
{
}
        
MediaPlayerPrivateIOS::PlatformTextTrackMenuInterfaceIOS::~PlatformTextTrackMenuInterfaceIOS()
{
    m_owner = nullptr;
    m_client = nullptr;
}

PassOwnPtr<MediaPlayerPrivateInterface> MediaPlayerPrivateIOS::create(MediaPlayer* player)
{ 
    return adoptPtr(new MediaPlayerPrivateIOS(player));
}

void MediaPlayerPrivateIOS::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (isAvailable())
        registrar(create, getSupportedTypes, supportsType, 0, 0, 0, 0);
}

MediaPlayerPrivateIOS::MediaPlayerPrivateIOS(MediaPlayer* player)
    : m_mediaPlayer(player)
    , m_objcHelper(adoptNS([[WebCoreMediaPlayerNotificationHelper alloc] initWithClient:this]))
    , m_networkState(MediaPlayer::Empty)
    , m_readyState(MediaPlayer::HaveNothing)
#if ENABLE(VIDEO_TRACK)
    , m_currentTrack(nullptr)
#endif
    , m_delayCallbacks(0)
    , m_changingVolume(0)
    , m_bytesLoadedAtLastDidLoadingProgress(0)
    , m_requestedRate(0)
    , m_bufferingState(Empty)
    , m_visible(false)
    , m_usingNetwork(false)
    , m_inFullScreen(false)
    , m_shouldPrepareToPlay(false)
    , m_preparingToPlay(false)
    , m_pauseRequested(false)
{
}

MediaPlayerPrivateIOS::~MediaPlayerPrivateIOS()
{
    clearTextTracks();
    [m_objcHelper.get() disconnect];
    if ([m_mediaPlayerHelper.get() respondsToSelector:@selector(_setDelegate:)])
        [m_mediaPlayerHelper.get() _setDelegate:nil];
    [m_mediaPlayerHelper.get() _disconnect];
}

void MediaPlayerPrivateIOS::setMediaPlayerProxy(WebMediaPlayerProxy *proxy)
{
    if (m_mediaPlayerHelper)
        [m_mediaPlayerHelper.get() _disconnect];

    m_mediaPlayerHelper = nullptr;

    if (proxy) {
        // don't try to use a helper if the interface version is too low
        if ([proxy _interfaceVersion] <  kRequiredHelperInterfaceVersion) {
            NSLog(@"Media player helper interface version too low, exiting.");
            return;
        }
        m_mediaPlayerHelper = adoptNS(static_cast<NSObject *>([proxy retain]));
        if ([m_mediaPlayerHelper.get() respondsToSelector:@selector(_setDelegate:)])
            [m_mediaPlayerHelper.get() _setDelegate:m_objcHelper.get()];

        acceleratedRenderingStateChanged();
    }

    processPendingRequests();
}

void MediaPlayerPrivateIOS::load(const String& url)
{
    NSURL *nsURL = URL(ParsedURLString, url);

    if (!m_mediaPlayerHelper) {
        addDeferredRequest(DeferredPropertySrcKey, [nsURL absoluteString]);
        return;
    }

    [m_deferredProperties.get() removeObjectForKey:DeferredPropertySrcKey];
    setDelayCallbacks(true);
    {
        if (m_networkState != MediaPlayer::Loading) {
            m_networkState = MediaPlayer::Loading;
            m_mediaPlayer->networkStateChanged();
        }
        if (m_readyState != MediaPlayer::HaveNothing) {
            m_readyState = MediaPlayer::HaveNothing;
            m_mediaPlayer->readyStateChanged();
        }

        m_pauseRequested = false;
        m_preparingToPlay = false;

        [m_mediaPlayerHelper.get() _cancelLoad];
        [m_mediaPlayerHelper.get() _load:nsURL];
    }
    setDelayCallbacks(false);
}

void MediaPlayerPrivateIOS::cancelLoad()
{
    [m_objcHelper.get() cancelPendingRequests];
    [m_deferredProperties.get() removeAllObjects];

    if (!m_mediaPlayerHelper)
        return;
    [m_mediaPlayerHelper.get() _cancelLoad];
}

void MediaPlayerPrivateIOS::addDeferredRequest(NSString *name, id value)
{
    if (!m_deferredProperties)
        m_deferredProperties = adoptNS([[NSMutableDictionary alloc] init]);
    [m_deferredProperties.get() setObject:value ? value : @"" forKey:name];
}

void MediaPlayerPrivateIOS::processDeferredRequests()
{
    if (![m_deferredProperties.get() count])
        return;

    setDelayCallbacks(true);
    {
        RetainPtr<NSDictionary> localProperties = adoptNS([[NSDictionary alloc] initWithDictionary:m_deferredProperties.get()]);
        [m_deferredProperties.get() removeAllObjects];

        // Always set the src first.
        id value = [localProperties.get() objectForKey:DeferredPropertySrcKey];
        if (value)
            load(value);

        for (NSString *name in localProperties.get()) {
            value = [localProperties.get() objectForKey:name];

            LOG(Media, "MediaPlayerPrivateIOS::processDeferredRequests(%p) - processing %s", this, [name UTF8String]);

            if ([name isEqualToString:DeferredPropertyPosterKey])
                setPoster(value);
            else if ([name isEqualToString:DeferredPropertyControlsKey])
                setControls([value boolValue]);
            else if ([name isEqualToString:DeferredPlayKey])
                play();
            else if ([name isEqualToString:DeferredPropertyOutOfBandTracksKey])
                setOutOfBandTextTracks(value);
            else if ([name isEqualToString:DeferredPropertySelectedTrackKey])
                setSelectedTextTrack(value);
#if ENABLE(IOS_AIRPLAY)
            else if ([name isEqualToString:DeferredWirelessVideoPlaybackDisabled])
                setWirelessVideoPlaybackDisabled([value boolValue]);
#endif
            else if (![name isEqualToString:DeferredPropertySrcKey])
                attributeChanged(name, value);
        }
    }
    setDelayCallbacks(false);
}

void MediaPlayerPrivateIOS::setPoster(const String& url)
{
    setDelayCallbacks(true);
    {
        NSURL *nsURL = URL(ParsedURLString, url);
        if (m_mediaPlayerHelper) {
            [m_deferredProperties.get() removeObjectForKey:DeferredPropertyPosterKey];
            [m_mediaPlayerHelper.get() _setPoster:nsURL];
        }
        else
            addDeferredRequest(DeferredPropertyPosterKey, url);
    }
    setDelayCallbacks(false);
}

void MediaPlayerPrivateIOS::setControls(bool controls)
{
    setDelayCallbacks(true);
    {
        if (m_mediaPlayerHelper) {
            [m_deferredProperties.get() removeObjectForKey:DeferredPropertyControlsKey];
            [m_mediaPlayerHelper.get() _setControls:controls];
        }
        else
            addDeferredRequest(DeferredPropertyControlsKey, [NSNumber numberWithBool:controls]);
    }
    setDelayCallbacks(false);
}

void MediaPlayerPrivateIOS::processPendingRequests()
{
    if (m_shouldPrepareToPlay && !m_preparingToPlay)
        [m_objcHelper.get() schedulePrepareToPlayWithOptionalDelay:[NSNumber numberWithBool:YES]];
    if ([m_deferredProperties.get() count])
        [m_objcHelper.get() scheduleDeferredPropertiesWithOptionalDelay:[NSNumber numberWithBool:YES]];
    m_shouldPrepareToPlay = false;
}

void MediaPlayerPrivateIOS::prepareToPlay()
{
    if (!m_mediaPlayerHelper) {
        // Not hooked up to the plug-in yet, remember this request.
        m_shouldPrepareToPlay = true;
        return;
    }

    if (m_preparingToPlay || m_readyState >= MediaPlayer::HaveFutureData)
        return;

    m_shouldPrepareToPlay = false;
    m_preparingToPlay = true;

    setDelayCallbacks(true);
        [m_mediaPlayerHelper.get() _prepareForPlayback];
    setDelayCallbacks(false);
}


void MediaPlayerPrivateIOS::play()
{
    if (!m_mediaPlayerHelper) {
        addDeferredRequest(DeferredPlayKey, nil);
        return;
    }

    m_pauseRequested = false;
    setDelayCallbacks(true);
        [m_mediaPlayerHelper.get() _play];
    setDelayCallbacks(false);
}

void MediaPlayerPrivateIOS::pause()
{
    if (!m_mediaPlayerHelper)
        return;

    m_pauseRequested = true;
    setDelayCallbacks(true);
        [m_mediaPlayerHelper.get() _pause];
    setDelayCallbacks(false);
}

float MediaPlayerPrivateIOS::duration() const
{
    if (!m_mediaPlayerHelper)
        return 0;
    float duration = static_cast<float>([m_mediaPlayerHelper.get() _duration]);
    return duration == -1 ? numeric_limits<float>::infinity() : duration;
}

float MediaPlayerPrivateIOS::currentTime() const
{
    if (!m_mediaPlayerHelper)
        return 0;
    return static_cast<float>([m_mediaPlayerHelper.get() _currentTime]);
}

void MediaPlayerPrivateIOS::seek(float time)
{
    if (!m_mediaPlayerHelper)
        return;
    float duration = this->duration();
    [m_mediaPlayerHelper.get() _setCurrentTime:(time > duration ? duration : time)];
}

void MediaPlayerPrivateIOS::setEndTime(float time)
{
    if (!m_mediaPlayerHelper)
        return;
    [m_mediaPlayerHelper.get() _setEndTime:time];
}

bool MediaPlayerPrivateIOS::paused() const
{
    if (!m_mediaPlayerHelper)
        return true;
    return !rate();
}

bool MediaPlayerPrivateIOS::seeking() const
{
    if (!m_mediaPlayerHelper)
        return false;
    return [m_mediaPlayerHelper.get() _seeking];
}

IntSize MediaPlayerPrivateIOS::naturalSize() const
{
    if (!m_mediaPlayerHelper)
        return IntSize();

    NSSize size = [m_mediaPlayerHelper.get() _naturalSize];
    return IntSize(size);
}

bool MediaPlayerPrivateIOS::hasVideo() const
{
    if (!m_mediaPlayerHelper)
        return false;
    return [m_mediaPlayerHelper.get() _hasVideo];
}

bool MediaPlayerPrivateIOS::hasAudio() const
{
    if (!m_mediaPlayerHelper)
        return false;
    return [m_mediaPlayerHelper.get() _hasAudio];
}

float MediaPlayerPrivateIOS::volume() const
{
    if (!m_mediaPlayerHelper)
        return false;
    return [m_mediaPlayerHelper.get() _volume];
}

void MediaPlayerPrivateIOS::setVolume(float inVolume)
{
    if (!m_mediaPlayerHelper || m_changingVolume)
        return;
    [m_mediaPlayerHelper.get() _setVolume:inVolume];
}

float MediaPlayerPrivateIOS::rate() const
{
    if (!m_mediaPlayerHelper)
        return false;
    return [m_mediaPlayerHelper.get() _rate];
}

void MediaPlayerPrivateIOS::setRate(float inRate)
{
    if (!m_mediaPlayerHelper)
        return;
    m_requestedRate = inRate;
    [m_mediaPlayerHelper.get() _setRate:inRate];
}

void MediaPlayerPrivateIOS::setMuted(bool inMute)
{
    if (!m_mediaPlayerHelper)
        return;
    [m_mediaPlayerHelper.get() _setMuted:inMute];
}

int MediaPlayerPrivateIOS::dataRate() const
{
    if (!m_mediaPlayerHelper)
        return 0;
    return [m_mediaPlayerHelper.get() _dataRate];
}

MediaPlayer::NetworkState MediaPlayerPrivateIOS::networkState() const 
{ 
    return m_networkState;
}

MediaPlayer::ReadyState MediaPlayerPrivateIOS::readyState() const 
{
    return m_readyState;
}

float MediaPlayerPrivateIOS::maxTimeBuffered() const
{
    if (!m_mediaPlayerHelper)
        return 0;
    return [m_mediaPlayerHelper.get() _maxTimeBuffered];
}

bool MediaPlayerPrivateIOS::didLoadingProgress() const
{
    if (!m_mediaPlayerHelper)
        return false;
    unsigned currentBytesLoaded = [m_mediaPlayerHelper.get() _bytesLoaded];
    bool didLoadingProgress = currentBytesLoaded != m_bytesLoadedAtLastDidLoadingProgress;
    m_bytesLoadedAtLastDidLoadingProgress = currentBytesLoaded;
    return didLoadingProgress;
}

bool MediaPlayerPrivateIOS::totalBytesKnown() const
{
    return totalBytes() > 0;
}

unsigned MediaPlayerPrivateIOS::totalBytes() const
{
    if (!m_mediaPlayerHelper)
        return 0;
    return [m_mediaPlayerHelper.get() _totalBytes];
}

float MediaPlayerPrivateIOS::maxTimeSeekable() const
{
    if (!m_mediaPlayerHelper)
        return 0.0f;
    return [m_mediaPlayerHelper.get() _maxTimeSeekable];
}

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateIOS::buffered() const
{
    auto timeRanges = PlatformTimeRanges::create();

    if (!m_mediaPlayerHelper)
        return timeRanges;

    NSArray *ranges = [m_mediaPlayerHelper.get() _bufferedTimeRanges];
    if (!ranges)
        return timeRanges;

    float timeRange[2];
    int count = [ranges count];
    for (int i = 0; i < count; ++i) {
        NSData *range = [ranges objectAtIndex:i];
        [range getBytes:timeRange length:(NSUInteger)sizeof(timeRange)];

        timeRanges->add(timeRange[0], timeRange[1]);
    }

    return timeRanges;
}

void MediaPlayerPrivateIOS::setSize(const IntSize&)
{
    // FIXME: Do we need to do anything here?
}

void MediaPlayerPrivateIOS::setVisible(bool visible)
{
    // FIXME: Do we need to do anything more here?
    if (m_visible != visible)
        m_visible = visible;
}

void MediaPlayerPrivateIOS::paint(GraphicsContext*, const IntRect&)
{
    // Nothing to do here.
}

static HashSet<String> mimeTypeCache()
{
    static NeverDestroyed<HashSet<String>> typeCache;
    static bool typeListInitialized = false;

    if (typeListInitialized)
        return typeCache;

    typeListInitialized = true;

    // FIXME: Should get these from the helper, but we don't have an instance when this is called.
    static const char* typeList[] = {
        "application/x-mpegurl",
        "application/vnd.apple.mpegurl",
        "audio/3gpp",
        "audio/3gpp2",
        "audio/aac",
        "audio/aiff",
        "audio/amr",
        "audio/basic",
        "audio/mp3",
        "audio/mp4",
        "audio/mpeg",
        "audio/mpegurl",
        "audio/mpeg3",
        "audio/scpls",
        "audio/wav",
        "audio/x-aac",
        "audio/x-aiff",
        "audio/x-caf",
        "audio/x-m4a",
        "audio/x-m4b",
        "audio/x-m4p",
        "audio/x-m4r",
        "audio/x-mp3",
        "audio/x-mpeg",
        "audio/x-mpeg3",
        "audio/x-mpegurl",
        "audio/x-scpls",
        "audio/x-wav",
        "video/3gpp",
        "video/3gpp2",
        "video/mp4",
        "video/quicktime",
        "video/x-m4v",

        // FIXME: This is added to restore old behavior for:
        // <rdar://problem/9792964> Embedded videos don't play on spiegel.de (.html file redirects)
        // However, a better fix is tracked by:
        // <http://webkit.org/b/64811> <video> ".html" src with a 301 redirect fails to load
        "text/html",
    };
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(typeList); ++i)
        typeCache.get().add(typeList[i]);

    return typeCache;
}

MediaPlayer::SupportsType MediaPlayerPrivateIOS::supportsType(const MediaEngineSupportParameters& parameters)
{
    // Only return "IsSupported" if there is no codecs parameter for now as there is no way to ask CoreMedia
    // if it supports an extended MIME type.
    if (!parameters.type.isEmpty() && mimeTypeCache().contains(parameters.type))
        return parameters.codecs.isEmpty() ? MediaPlayer::MayBeSupported : MediaPlayer::IsSupported;

    return MediaPlayer::IsNotSupported;
}

void MediaPlayerPrivateIOS::getSupportedTypes(HashSet<String>& types)
{
    types = mimeTypeCache();
}

bool MediaPlayerPrivateIOS::isAvailable()
{
    return true;
}

#if ENABLE(IOS_AIRPLAY)
bool MediaPlayerPrivateIOS::isCurrentPlaybackTargetWireless() const
{
    if (!m_mediaPlayerHelper)
        return false;
    return [m_mediaPlayerHelper.get() _isCurrentPlaybackTargetWireless];
}

void MediaPlayerPrivateIOS::showPlaybackTargetPicker()
{
    setDelayCallbacks(true);
        [m_mediaPlayerHelper.get() _showPlaybackTargetPicker];
    setDelayCallbacks(false);
}

bool MediaPlayerPrivateIOS::hasWirelessPlaybackTargets() const
{
    if (!m_mediaPlayerHelper)
        return false;
    return [m_mediaPlayerHelper.get() _hasWirelessPlaybackTargets];
}

bool MediaPlayerPrivateIOS::wirelessVideoPlaybackDisabled() const
{
    if (!m_mediaPlayerHelper)
        return false;
    return [m_mediaPlayerHelper.get() _wirelessVideoPlaybackDisabled];
}

void MediaPlayerPrivateIOS::setWirelessVideoPlaybackDisabled(bool disabled)
{
    setDelayCallbacks(true);
    {
        if (m_mediaPlayerHelper) {
            [m_deferredProperties.get() removeObjectForKey:DeferredWirelessVideoPlaybackDisabled];
            [m_mediaPlayerHelper.get() _setWirelessVideoPlaybackDisabled:disabled];
        } else
            addDeferredRequest(DeferredWirelessVideoPlaybackDisabled, [NSNumber numberWithBool:disabled]);
    }
    setDelayCallbacks(false);
}

void MediaPlayerPrivateIOS::setHasPlaybackTargetAvailabilityListeners(bool hasListeners)
{
    [m_mediaPlayerHelper.get() _setHasPlaybackTargetAvailabilityListeners:hasListeners];
}
#endif

bool MediaPlayerPrivateIOS::supportsAcceleratedRendering() const
{
    return true;
}

void MediaPlayerPrivateIOS::enterFullscreen()
{
    // The const cast is a workaround due to OpenSource changing the signature to const.
    MediaPlayerPrivateIOS* mutableThis = const_cast<MediaPlayerPrivateIOS*>(this);
    mutableThis->setDelayCallbacks(true);
    [m_mediaPlayerHelper.get() _enterFullScreen];
    mutableThis->setDelayCallbacks(false);
}

void MediaPlayerPrivateIOS::exitFullscreen()
{
    setDelayCallbacks(true);
    [m_mediaPlayerHelper.get() _exitFullScreen];
    setDelayCallbacks(false);
}

void MediaPlayerPrivateIOS::attributeChanged(const String& name, const String& value)
{
    if (!m_mediaPlayerHelper) {
        addDeferredRequest(name, value);
        return;
    }

    if (![m_mediaPlayerHelper.get() respondsToSelector:@selector(_attributeChanged:value:)])
        return;

    setDelayCallbacks(true);
    [m_mediaPlayerHelper.get() _attributeChanged:name value:value];
    setDelayCallbacks(false);
}

bool MediaPlayerPrivateIOS::readyForPlayback() const
{
    if ([m_mediaPlayerHelper.get() respondsToSelector:@selector(_readyForPlayback)])
        return [m_mediaPlayerHelper.get() _readyForPlayback];
    return MediaPlayerPrivateInterface::readyForPlayback();
}

bool MediaPlayerPrivateIOS::hasClosedCaptions() const
{
    return [m_mediaPlayerHelper.get() _hasClosedCaptions];
}    

void MediaPlayerPrivateIOS::setClosedCaptionsVisible(bool flag)
{
    [m_mediaPlayerHelper.get() _setClosedCaptionsVisible:flag];
}

void MediaPlayerPrivateIOS::deliverNotification(MediaPlayerProxyNotificationType notification) 
{
    LOG(Media, "MediaPlayerPrivateIOS::deliverNotification(%p) - notification = %i, entering with networkState = %i, readyState = %i", 
        this, static_cast<int>(notification), static_cast<int>(m_networkState), static_cast<int>(m_readyState));

    if (!m_mediaPlayer)
        return;

    if (callbacksDelayed()) {
        [m_objcHelper.get() delayNotification:notification];
        return;
    }

    MediaPlayer::NetworkState oldNetworkState = m_networkState;
    MediaPlayer::ReadyState oldReadyState = m_readyState;

    switch (notification) {
    case MediaPlayerNotificationStartUsingNetwork:
        m_usingNetwork = true;
        if (m_networkState < MediaPlayer::Loading)
            m_networkState = MediaPlayer::Loading;
        break;
    case MediaPlayerNotificationStopUsingNetwork:
        m_usingNetwork = false;
        if (m_readyState < MediaPlayer::HaveCurrentData && m_networkState < MediaPlayer::FormatError)
            m_networkState = MediaPlayer::Idle;
        break;

    case MediaPlayerNotificationGainFocus:
        if (m_networkState < MediaPlayer::Loading)
            m_networkState = MediaPlayer::Loading;
        break;

    case MediaPlayerNotificationLoseFocus:
        if (m_networkState < MediaPlayer::FormatError)
            m_networkState = MediaPlayer::Idle;
        if (m_readyState > MediaPlayer::HaveMetadata)
            m_readyState = MediaPlayer::HaveMetadata;
        break;

    case MediaPlayerNotificationEnteredFullscreen:
        m_inFullScreen = true;
        break;

    case MediaPlayerNotificationExitedFullscreen: {
        m_inFullScreen = false;
#if ENABLE(VIDEO_TRACK)
        setTextTrackRepresentation(0);
#endif
        break;
    }

    case MediaPlayerNotificationReadyForInspection:
        // The buffering state sometimes reaches "likely to keep up" or "full" before the player item is
        // ready for inspection. In that case advance directly to HaveEnoughData.
        if (m_bufferingState > UnlikeleyToKeepUp)
            m_readyState = MediaPlayer::HaveEnoughData;
        else
            m_readyState = MediaPlayer::HaveCurrentData;
        break;

    case MediaPlayerNotificationMediaValidated:
        if (m_networkState < MediaPlayer::Idle && !m_preparingToPlay)
            m_networkState = MediaPlayer::Idle;
        processPendingRequests();
        break;

    case MediaPlayerNotificationMediaFailedToValidate:
        m_networkState = MediaPlayer::FormatError;
        m_readyState = MediaPlayer::HaveNothing; 
        m_usingNetwork = false;
        m_preparingToPlay = false;
        break;

    case MediaPlayerNotificationPlaybackFailed: {
        // FIXME: We can't currently tell the difference between NetworkError and DecodeError.
        m_networkState = MediaPlayer::DecodeError;

        // Don't reset readyState, what we have loaded already may be playable.
        m_usingNetwork = false;
        break;
    }

    case MediaPlayerNotificationReadyForPlayback:
        if (m_networkState < MediaPlayer::Loading)
            m_networkState = MediaPlayer::Loading;
        m_readyState = MediaPlayer::HaveFutureData;
        break;

    case MediaPlayerNotificationDidPlayToTheEnd:
        m_mediaPlayer->timeChanged();
        break;

    case MediaPlayerNotificationStreamLikelyToKeepUp:
        if (m_networkState < MediaPlayer::Loading)
            m_networkState = MediaPlayer::Loading;
        if (m_readyState > MediaPlayer::HaveMetadata && m_readyState < MediaPlayer::HaveEnoughData)
            m_readyState = MediaPlayer::HaveEnoughData;
        m_bufferingState = LikeleyToKeepUp;
        break;

    case MediaPlayerNotificationStreamBufferFull:
        // The media buffers are full so no more data can be loaded. This isn't really the the same thing as 
        // "HaveFutureData", but nothing more will be loaded until playback consumes some of the buffered data
        // so bump the readyState to allow playback.
        if (m_readyState > MediaPlayer::HaveMetadata && m_readyState < MediaPlayer::HaveEnoughData)
            m_readyState = MediaPlayer::HaveFutureData;
        m_networkState = MediaPlayer::Idle;
        m_bufferingState = Full;
        break;

    case MediaPlayerNotificationStreamRanDry:
        m_networkState = MediaPlayer::Loading;
        if (m_readyState > MediaPlayer::HaveMetadata)
            m_readyState = MediaPlayer::HaveCurrentData;
        m_bufferingState = Empty;
        break;

    case MediaPlayerNotificationStreamUnlikelyToKeepUp:
        m_networkState = MediaPlayer::Loading;
        if (m_readyState > MediaPlayer::HaveMetadata && m_readyState < MediaPlayer::HaveEnoughData)
            m_readyState = MediaPlayer::HaveFutureData;
        m_bufferingState = UnlikeleyToKeepUp;
        break;

    case MediaPlayerNotificationFileLoaded:
        m_networkState = MediaPlayer::Loaded;
        m_readyState = MediaPlayer::HaveEnoughData;
        break;

    case MediaPlayerNotificationTimeJumped:
        // Sometimes get a spurious "time jumped" when the movie is being set up or immediately after pausing, 
        // don't fall for it.
        if (m_readyState >= MediaPlayer::HaveCurrentData)
            m_mediaPlayer->timeChanged();
        break;

    case MediaPlayerNotificationMutedDidChange:
    case MediaPlayerNotificationVolumeDidChange:
        ++m_changingVolume;
        m_mediaPlayer->volumeChanged(volume());
        --m_changingVolume;
        break;

    case MediaPlayerNotificationSizeDidChange:
        break;

    case MediaPlayerNotificationPlayPauseButtonPressed:
    case MediaPlayerRequestBeginPlayback:
    case MediaPlayerRequestPausePlayback:
        break;

    case MediaPlayerNotificationRateDidChange:
        m_preparingToPlay = false;
        if (m_pauseRequested && rate())
            m_pauseRequested = false;
        break;

#if ENABLE(IOS_AIRPLAY)
    case MediaPlayerNotificationCurrentPlaybackTargetIsWirelessChanged:
        m_mediaPlayer->currentPlaybackTargetIsWirelessChanged();
        break;

    case MediaPlayerNotificationPlaybackTargetAvailabilityChanged:
        m_mediaPlayer->playbackTargetAvailabilityChanged();
        break;
#endif
    }

    if (m_networkState != oldNetworkState)
        m_mediaPlayer->networkStateChanged();
    if (m_readyState != oldReadyState)
        m_mediaPlayer->readyStateChanged();

    LOG(Media, "MediaPlayerPrivateIOS::deliverNotification(%p) - exiting with networkState = %i, readyState = %i", 
        this, static_cast<int>(m_networkState), static_cast<int>(m_readyState));
}

#if ENABLE(VIDEO_TRACK)
bool MediaPlayerPrivateIOS::requiresTextTrackRepresentation() const
{
    return m_inFullScreen;
}

void MediaPlayerPrivateIOS::setTextTrackRepresentation(TextTrackRepresentation* representation)
{
    [m_mediaPlayerHelper.get() _setTextTrackRepresentation:representation ? representation->platformLayer() : nullptr];
}

void MediaPlayerPrivateIOS::inbandTextTracksChanged(NSArray *tracks)
{
    Vector<RefPtr<InbandTextTrackPrivateAVFIOS> > removedTextTracks = m_textTracks;

    for (NSDictionary *trackInfo in tracks) {
        bool newTrack = true;
        NSNumber *trackIDValue  = [trackInfo objectForKey:TextTrackIDKey];

        if (![trackIDValue isKindOfClass:[NSNumber class]]) {
            NSLog(@"Unexpected track ID, ignoring.");
            continue;
        }

        int trackID = [trackIDValue intValue];
        for (unsigned i = removedTextTracks.size(); i > 0; --i) {
            RefPtr<InbandTextTrackPrivateAVFIOS> track = static_cast<InbandTextTrackPrivateAVFIOS*>(removedTextTracks[i - 1].get());

            if (track->internalID() == trackID) {
                removedTextTracks.remove(i - 1);
                newTrack = false;
                break;
            }
        }
        if (!newTrack)
            continue;

        NSString *name = [trackInfo objectForKey:TextTrackDisplayNameKey];
        NSString *language = [trackInfo objectForKey:TextTrackLocaleIdentifierKey];
        NSString *type = [trackInfo objectForKey:TextTrackTypeKey];

        LOG(Media, "MediaPlayerPrivateIOS::inbandTextTracksChanged(%p) - adding track id %i, name = %s, language = %s, type = %s", this, trackID, [name UTF8String], [language UTF8String], [type UTF8String]);

        m_textTracks.append(InbandTextTrackPrivateAVFIOS::create(this, trackID, name, language, type));
    }

    if (removedTextTracks.size()) {
        for (unsigned i = m_textTracks.size(); i > 0; --i) {
            RefPtr<InbandTextTrackPrivateAVFIOS> track = static_cast<InbandTextTrackPrivateAVFIOS*>(m_textTracks[i - 1].get());

            if (!removedTextTracks.contains(track))
                continue;

            if (m_currentTrack == track.get())
                m_currentTrack = nullptr;
            m_mediaPlayer->removeTextTrack(track);
            m_textTracks.remove(i - 1);
        }
    }

    for (unsigned i = 0; i < m_textTracks.size(); ++i) {
        RefPtr<InbandTextTrackPrivateAVFIOS> track = static_cast<InbandTextTrackPrivateAVFIOS*>(m_textTracks[i].get());

        track->setTextTrackIndex(i);
        if (track->hasBeenReported())
            continue;

        track->setHasBeenReported(true);
        m_mediaPlayer->addTextTrack(track.get());
    }
    LOG(Media, "MediaPlayerPrivateIOS::inbandTextTracksChanged(%p) - found %i in-band tracks", this, m_textTracks.size());
}

void MediaPlayerPrivateIOS::setSelectedTextTrack(NSNumber *trackID)
{
    if (![trackID isKindOfClass:[NSNumber class]]) {
        NSLog(@"Unexpected track ID, ignoring.");
        return;
    }

    if (!m_mediaPlayerHelper) {
        addDeferredRequest(DeferredPropertySelectedTrackKey, trackID);
        return;
    }

    if (![m_mediaPlayerHelper.get() respondsToSelector:@selector(_setSelectedTextTrack:)])
        return;

    setDelayCallbacks(true);
    {
        LOG(Media, "MediaPlayerPrivateIOS::setCurrentTrack(%p) - selecting track id %i", this, [trackID intValue]);
        [m_deferredProperties.get() removeObjectForKey:DeferredPropertySelectedTrackKey];
        [m_mediaPlayerHelper.get() _setSelectedTextTrack:trackID];
    }
    setDelayCallbacks(false);
}

void MediaPlayerPrivateIOS::trackModeChanged()
{
    RefPtr<InbandTextTrackPrivateAVF> trackToEnable;

    // AVFoundation can only emit cues for one track at a time, so enable the first track that is showing, or the first that
    // is hidden if none are showing. Otherwise disable all tracks.
    for (unsigned i = 0; i < m_textTracks.size(); ++i) {
        RefPtr<InbandTextTrackPrivateAVFIOS> track = m_textTracks[i];
        if (track->mode() == InbandTextTrackPrivate::Showing) {
            trackToEnable = track;
            break;
        }
        if (!trackToEnable && track->mode() == InbandTextTrackPrivate::Hidden)
            trackToEnable = track;
    }

    InbandTextTrackPrivateAVFIOS* trackPrivate = static_cast<InbandTextTrackPrivateAVFIOS*>(trackToEnable.get());
    if (m_currentTrack == trackPrivate)
        return;

    m_currentTrack = trackPrivate;
    setSelectedTextTrack(trackPrivate ? [NSNumber numberWithInt:trackPrivate->internalID()] : nil);
}

void MediaPlayerPrivateIOS::clearTextTracks()
{
    m_currentTrack = nullptr;
    for (unsigned i = 0; i < m_textTracks.size(); ++i) {
        RefPtr<InbandTextTrackPrivateAVFIOS> track = m_textTracks[i];
        m_mediaPlayer->removeTextTrack(track);
        track->disconnect();
    }
    m_textTracks.clear();
}

void MediaPlayerPrivateIOS::processInbandTextTrackCue(NSArray *attributedStrings, double time)
{
    if (!m_currentTrack)
        return;

    m_currentTrack->processCue(reinterpret_cast<CFArrayRef>(attributedStrings), time);
}

void MediaPlayerPrivateIOS::setOutOfBandTextTracks(NSArray *tracksArray)
{
    if (!m_mediaPlayerHelper) {
        addDeferredRequest(DeferredPropertyOutOfBandTracksKey, tracksArray);
        return;
    }

    if (![m_mediaPlayerHelper.get() respondsToSelector:@selector(_setOutOfBandTextTracks:)])
        return;

    setDelayCallbacks(true);
    {
        [m_deferredProperties.get() removeObjectForKey:DeferredPropertyOutOfBandTracksKey];
        [m_mediaPlayerHelper.get() _setOutOfBandTextTracks:tracksArray];
    }
    setDelayCallbacks(false);
}

static int platformTrackID(PlatformTextTrack* platformTrack)
{
    if (platformTrack->type() == PlatformTextTrack::InBand) {
        InbandTextTrackPrivateAVFIOS* trackPrivate = static_cast<InbandTextTrackPrivateAVFIOS*>(platformTrack->client()->privateTrack());
        return trackPrivate ? trackPrivate->internalID() : 0;
    }

    return platformTrack->uniqueId();
}

void MediaPlayerPrivateIOS::outOfBandTextTracksChanged()
{
    if (!m_menuInterface->client())
        return;

    RetainPtr<NSMutableArray> tracksArray = adoptNS([[NSMutableArray alloc] init]);
    Vector<RefPtr<PlatformTextTrack> > textTracks = m_menuInterface->client()->platformTextTracks();

    for (unsigned i = 0; i < textTracks.size(); ++i) {
        RefPtr<PlatformTextTrack> platformTrack = textTracks[i];

        if (platformTrack->type() == PlatformTextTrack::InBand)
            continue;
        if (platformTrack->kind() != PlatformTextTrack::Subtitle && platformTrack->kind() != PlatformTextTrack::Caption)
            continue;

        RetainPtr<NSMutableDictionary> track = adoptNS([[NSMutableDictionary alloc] init]);

        [track.get() setObject:platformTrack->label() forKey:TextTrackDisplayNameKey];

        if (!platformTrack->language().isEmpty())
            [track.get() setObject:platformTrack->language() forKey:TextTrackLocaleIdentifierKey];

        NSString *type = platformTrack->kind() == PlatformTextTrack::Subtitle ? TextTrackSubtitlesKey : TextTrackCaptionsKey;
        [track.get() setObject:type forKey:TextTrackTypeKey];

        [track.get() setObject:[NSNumber numberWithInt:platformTrackID(platformTrack.get())] forKey:TextTrackIDKey];

        [tracksArray addObject:track.get()];
    }

    if (![tracksArray.get() count])
        return;

    setOutOfBandTextTracks(tracksArray.get());
}

void MediaPlayerPrivateIOS::textTrackWasSelectedByMediaElement(PassRefPtr<PlatformTextTrack> platformTrack)
{
    setSelectedTextTrack([NSNumber numberWithInt:platformTrackID(platformTrack.get())]);
}

void MediaPlayerPrivateIOS::textTrackWasSelectedByPlugin(NSDictionary *trackInfo)
{
    if (!m_menuInterface || !m_menuInterface->client())
        return;

    if (!trackInfo) {
        m_menuInterface->client()->setSelectedTextTrack(0);
        return;
    }

    RefPtr<PlatformTextTrack> trackToSelect;
    NSNumber *trackIDToSelect  = [trackInfo objectForKey:TextTrackIDKey];
    if (![trackIDToSelect isKindOfClass:[NSNumber class]]) {
        NSLog(@"Unexpected track ID, ignoring.");
        return;
    }

    int trackID = [trackIDToSelect intValue];
    if (trackID == TextTrackOffMenuItemID)
        trackToSelect = PlatformTextTrack::captionMenuOffItem();
    else if (trackID == TextTrackAutoMenuItemID)
        trackToSelect = PlatformTextTrack::captionMenuAutomaticItem();
    else {
        Vector<RefPtr<PlatformTextTrack> > textTracks = m_menuInterface->client()->platformTextTracks();
        for (unsigned i = 0; i < textTracks.size(); ++i) {
            RefPtr<PlatformTextTrack> platformTrack = textTracks[i];

            if (trackID == platformTrackID(platformTrack.get())) {
                trackToSelect = platformTrack;
                break;
            }
        }
    }

    if (!trackToSelect) {
        NSLog(@"Text track ID %i is unknown, ignoring.", trackID);
        return;
    }

    m_menuInterface->client()->setSelectedTextTrack(trackToSelect);
}

PassRefPtr<PlatformTextTrackMenuInterface> MediaPlayerPrivateIOS::textTrackMenu()
{
    if (!m_menuInterface)
        m_menuInterface = PlatformTextTrackMenuInterfaceIOS::create(this);
    return m_menuInterface;
}
#endif // ENABLE(VIDEO_TRACK)

} // namespace WebCore

@implementation WebCoreMediaPlayerNotificationHelper

- (id)initWithClient:(MediaPlayerPrivateIOS *)client
{
    _client = client;
    return [super init];
}

- (void)disconnect
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    _client = 0;
}

- (void)cancelPendingRequests
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
}

- (void)delayNotification:(int)notification
{
    if (_client)
        [self performSelector:@selector(deliverNotification:) withObject:[NSNumber numberWithInt:notification] afterDelay:0];
}

- (void)deliverNotification:(NSNumber *)notification
{
    if (!_client)
        return;
    if (_client->callbacksDelayed())
        [self delayNotification:[notification intValue]];
    else
        _client->deliverNotification((MediaPlayerProxyNotificationType)[notification intValue]);
}

- (void)schedulePrepareToPlayWithOptionalDelay:(NSNumber *)shouldDelay
{
    if (!_client)
        return;
    if ([shouldDelay boolValue])
        [self performSelector:@selector(schedulePrepareToPlayWithOptionalDelay:) withObject:[NSNumber numberWithBool:NO] afterDelay:0];
    else
        _client->prepareToPlay();
}

- (void)scheduleDeferredPropertiesWithOptionalDelay:(NSNumber *)shouldDelay
{
    if (!_client)
        return;

    if ([shouldDelay boolValue]) {
        if (!_deferredPropertiesScheduled) {
            [self performSelector:@selector(scheduleDeferredPropertiesWithOptionalDelay:) withObject:[NSNumber numberWithBool:NO] afterDelay:0];
            _deferredPropertiesScheduled = YES;
        }
        return;
    }

    _deferredPropertiesScheduled = NO;
    _client->processDeferredRequests();
}

- (void)pluginElementInBandAlternateTextTracksDidChange:(NSArray *)tracks
{
    if (!_client)
        return;

    WebThreadRun(^{
        if (!_client)
            return;
        _client->inbandTextTracksChanged(tracks);
    });
}

- (void)pluginElementDidSelectTextTrack:(NSDictionary *)trackInfo
{
    if (!_client)
        return;

    WebThreadRun(^{
        if (!_client)
            return;
        _client->textTrackWasSelectedByPlugin(trackInfo);
    });
}

- (void)pluginElementDidOutputAttributedStrings:(NSArray *)strings nativeSampleBuffers:(NSArray *)buffers forTime:(NSTimeInterval)time
{
    UNUSED_PARAM(buffers);

    if (!_client)
        return;

    WebThreadRun(^{
        if (!_client)
            return;
        _client->processInbandTextTrackCue(strings, time);
    });
}

@end

#endif
