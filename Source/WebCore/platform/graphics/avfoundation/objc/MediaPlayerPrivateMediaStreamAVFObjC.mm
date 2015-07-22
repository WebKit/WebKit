/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "MediaPlayerPrivateMediaStreamAVFObjC.h"

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#import "AVVideoCaptureSource.h"
#import "FileSystem.h"
#import "Logging.h"
#import "MediaStreamPrivate.h"
#import "MediaStreamPrivateAVFObjC.h"
#import "MediaTimeAVFoundation.h"
#import "PlatformClockCM.h"
#import "WebCoreSystemInterface.h"
#import <AVFoundation/AVAsset.h>
#import <AVFoundation/AVCaptureVideoPreviewLayer.h>
#import <AVFoundation/AVTime.h>
#import <QuartzCore/CALayer.h>
#import <objc_runtime.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>

#pragma mark - Soft Linking

#import "CoreMediaSoftLink.h"

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)

SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVAsset)
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVCaptureVideoPreviewLayer)
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVURLAsset)
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVStreamSession);

#pragma mark -
#pragma mark AVStreamSession

@interface AVStreamSession : NSObject
- (instancetype)initWithStorageDirectoryAtURL:(NSURL *)storageDirectory;
@end

namespace WebCore {

#pragma mark -
#pragma mark MediaPlayerPrivateMediaStreamAVFObjC

MediaPlayerPrivateMediaStreamAVFObjC::MediaPlayerPrivateMediaStreamAVFObjC(MediaPlayer* player)
    : m_player(player)
    , m_weakPtrFactory(this)
    , m_networkState(MediaPlayer::Idle)
    , m_readyState(MediaPlayer::HaveNothing)
    , m_rate(1)
    , m_playing(0)
    , m_seeking(false)
    , m_seekCompleted(true)
    , m_loadingProgressed(false)
{

}

MediaPlayerPrivateMediaStreamAVFObjC::~MediaPlayerPrivateMediaStreamAVFObjC()
{
}

#pragma mark -
#pragma mark MediaPlayer Factory Methods

void MediaPlayerPrivateMediaStreamAVFObjC::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (isAvailable())
        registrar([](MediaPlayer* player) { return std::make_unique<MediaPlayerPrivateMediaStreamAVFObjC>(player); }, getSupportedTypes,
            supportsType, 0, 0, 0, 0);
}

bool MediaPlayerPrivateMediaStreamAVFObjC::isAvailable()
{
    return AVFoundationLibrary()
        && isCoreMediaFrameworkAvailable();
}

static const HashSet<String>& mimeTypeCache()
{
    static NeverDestroyed<HashSet<String>> cache;
    static bool typeListInitialized = false;

    if (typeListInitialized)
        return cache;
    typeListInitialized = true;

    NSArray *types = [getAVURLAssetClass() audiovisualMIMETypes];
    for (NSString *mimeType in types)
        cache.get().add(mimeType);

    return cache;
}

void MediaPlayerPrivateMediaStreamAVFObjC::getSupportedTypes(HashSet<String>& types)
{
    types = mimeTypeCache();
}

MediaPlayer::SupportsType MediaPlayerPrivateMediaStreamAVFObjC::supportsType(const MediaEngineSupportParameters& parameters)
{
    // This engine does not support non-media-stream sources.
    if (parameters.isMediaStream)
        return MediaPlayer::IsSupported;
    return MediaPlayer::IsNotSupported;
}

#pragma mark -
#pragma mark MediaPlayerPrivateInterface Overrides

void MediaPlayerPrivateMediaStreamAVFObjC::load(MediaStreamPrivate& client)
{
    m_MediaStreamPrivate = MediaStreamPrivateAVFObjC::create(*this, *client.client());
    for (auto track : client.tracks()) {
        m_MediaStreamPrivate->addTrack(WTF::move(track), MediaStreamPrivate::NotifyClientOption::DontNotify);
        m_MediaStreamPrivate->client()->didAddTrackToPrivate(*track);
        if (!track->ended()) {
            track->source()->startProducingData();
            track->setEnabled(true);
        }
    }
    m_previewLayer = nullptr;
    for (auto track : m_MediaStreamPrivate->tracks()) {
        if (track->type() == RealtimeMediaSource::Type::Video)
            m_previewLayer = static_cast<AVVideoCaptureSource*>(track->source())->previewLayer();
    }
    m_player->client().mediaPlayerRenderingModeChanged(m_player);
}

void MediaPlayerPrivateMediaStreamAVFObjC::cancelLoad()
{
}

void MediaPlayerPrivateMediaStreamAVFObjC::prepareToPlay()
{
}

PlatformMedia MediaPlayerPrivateMediaStreamAVFObjC::platformMedia() const
{
    PlatformMedia pm;
    pm.type = PlatformMedia::AVFoundationAssetType;
    pm.media.avfAsset = m_asset.get();
    return pm;
}

PlatformLayer* MediaPlayerPrivateMediaStreamAVFObjC::platformLayer() const
{
    if (!m_previewLayer)
        return nullptr;
    return static_cast<PlatformLayer*>(m_previewLayer);
}

void MediaPlayerPrivateMediaStreamAVFObjC::play()
{
    auto weakThis = createWeakPtr();
    callOnMainThread([weakThis] {
        if (!weakThis)
            return;
        weakThis.get()->playInternal();
    });
}

void MediaPlayerPrivateMediaStreamAVFObjC::playInternal()
{
    m_playing = true;
    if (shouldBePlaying())
        [m_synchronizer setRate:m_rate];

    for (auto track : m_MediaStreamPrivate->tracks())
        track->source()->startProducingData();
}

void MediaPlayerPrivateMediaStreamAVFObjC::pause()
{
    auto weakThis = createWeakPtr();
    callOnMainThread([weakThis] {
        if (!weakThis)
            return;
        weakThis.get()->pauseInternal();
    });
}

void MediaPlayerPrivateMediaStreamAVFObjC::pauseInternal()
{
    m_playing = false;

    for (auto track : m_MediaStreamPrivate->tracks())
        track->source()->stopProducingData();
}

bool MediaPlayerPrivateMediaStreamAVFObjC::paused() const
{
    // Shouldn't be able to pause streams
    return false;
}

void MediaPlayerPrivateMediaStreamAVFObjC::setVolume(float)
{
}

bool MediaPlayerPrivateMediaStreamAVFObjC::supportsScanning() const
{
    return true;
}

void MediaPlayerPrivateMediaStreamAVFObjC::setMuted(bool muted)
{
    for (auto track : m_MediaStreamPrivate->tracks()) {
        if (track->type() == RealtimeMediaSource::Type::Audio)
            track->source()->setMuted(muted);
    }
}

FloatSize MediaPlayerPrivateMediaStreamAVFObjC::naturalSize() const
{
    FloatSize floatSize(0, 0);
    for (auto track : m_MediaStreamPrivate->tracks()) {
        if (track->type() == RealtimeMediaSource::Type::Video) {
            AVVideoCaptureSource* source = (AVVideoCaptureSource*)track->source();
            if (!source->stopped() && track->enabled()) {
                if (source->width() > floatSize.width())
                    floatSize.setWidth(source->width());
                if (source->height() > floatSize.height())
                    floatSize.setHeight(source->height());
            }
        }
    }
    return floatSize;
}

bool MediaPlayerPrivateMediaStreamAVFObjC::hasVideo() const
{
    for (auto track : m_MediaStreamPrivate->tracks()) {
        if (track->type() == RealtimeMediaSource::Type::Video)
            return true;
    }
    return false;
}

bool MediaPlayerPrivateMediaStreamAVFObjC::hasAudio() const
{
    for (auto track : m_MediaStreamPrivate->tracks()) {
        if (track->type() == RealtimeMediaSource::Type::Audio)
            return true;
    }
    return false;
}

void MediaPlayerPrivateMediaStreamAVFObjC::setVisible(bool)
{
    // No-op.
}

MediaTime MediaPlayerPrivateMediaStreamAVFObjC::durationMediaTime() const
{
    return MediaTime::positiveInfiniteTime();
}

MediaTime MediaPlayerPrivateMediaStreamAVFObjC::currentMediaTime() const
{
    // FIXME(147125): Must implement this later
    return MediaTime::zeroTime();
}

bool MediaPlayerPrivateMediaStreamAVFObjC::seeking() const
{
    // MediaStream is unseekable
    return false;
}

void MediaPlayerPrivateMediaStreamAVFObjC::setRateDouble(double)
{
    // MediaStream is unseekable; therefore, cannot set rate
}

MediaPlayer::NetworkState MediaPlayerPrivateMediaStreamAVFObjC::networkState() const
{
    return m_networkState;
}

MediaPlayer::ReadyState MediaPlayerPrivateMediaStreamAVFObjC::readyState() const
{
    return m_readyState;
}

void MediaPlayerPrivateMediaStreamAVFObjC::sizeChanged()
{
    m_player->sizeChanged();
}

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateMediaStreamAVFObjC::seekable() const
{
    return std::make_unique<PlatformTimeRanges>(minMediaTimeSeekable(), maxMediaTimeSeekable());
}

MediaTime MediaPlayerPrivateMediaStreamAVFObjC::maxMediaTimeSeekable() const
{
    return durationMediaTime();
}

MediaTime MediaPlayerPrivateMediaStreamAVFObjC::minMediaTimeSeekable() const
{
    return startTime();
}

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateMediaStreamAVFObjC::buffered() const
{
    return std::make_unique<PlatformTimeRanges>();
}

bool MediaPlayerPrivateMediaStreamAVFObjC::didLoadingProgress() const
{
    bool loadingProgressed = m_loadingProgressed;
    m_loadingProgressed = false;
    return loadingProgressed;
}

void MediaPlayerPrivateMediaStreamAVFObjC::setSize(const IntSize&)
{
    // No-op.
}

void MediaPlayerPrivateMediaStreamAVFObjC::paint(GraphicsContext*, const FloatRect&)
{
    // FIXME(125157): Implement painting.
}

void MediaPlayerPrivateMediaStreamAVFObjC::paintCurrentFrameInContext(GraphicsContext*, const FloatRect&)
{
    // FIXME(125157): Implement painting.
}

bool MediaPlayerPrivateMediaStreamAVFObjC::supportsAcceleratedRendering() const
{
    return true;
}

MediaPlayer::MovieLoadType MediaPlayerPrivateMediaStreamAVFObjC::movieLoadType() const
{
    return MediaPlayer::LiveStream;
}

void MediaPlayerPrivateMediaStreamAVFObjC::prepareForRendering()
{
    // No-op.
}

String MediaPlayerPrivateMediaStreamAVFObjC::engineDescription() const
{
    static NeverDestroyed<String> description(ASCIILiteral("AVFoundation MediaStream Engine"));
    return description;
}

String MediaPlayerPrivateMediaStreamAVFObjC::languageOfPrimaryAudioTrack() const
{
    // FIXME(125158): implement languageOfPrimaryAudioTrack()
    return emptyString();
}

size_t MediaPlayerPrivateMediaStreamAVFObjC::extraMemoryCost() const
{
    return 0;
}

bool MediaPlayerPrivateMediaStreamAVFObjC::shouldBePlaying() const
{
    return m_playing && m_readyState >= MediaPlayer::HaveFutureData;
}

void MediaPlayerPrivateMediaStreamAVFObjC::setReadyState(MediaPlayer::ReadyState readyState)
{
    if (m_readyState == readyState)
        return;

    m_readyState = readyState;

    m_player->readyStateChanged();
}

}

#endif
