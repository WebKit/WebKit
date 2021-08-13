/*
 * Copyright (C) 2011-2015 Apple Inc. All rights reserved.
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

#include "config.h"

#if PLATFORM(WIN) && ENABLE(VIDEO) 

#if USE(AVFOUNDATION)

#include "MediaPlayerPrivateAVFoundationCF.h"

#include "ApplicationCacheResource.h"
#include "CDMSessionAVFoundationCF.h"
#include "COMPtr.h"
#include "FloatConversion.h"
#include "GraphicsContext.h"
#if HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
#include "InbandTextTrackPrivateAVCF.h"
#else
#include "InbandTextTrackPrivateLegacyAVCF.h"
#endif
#include "Logging.h"
#include "PlatformCALayerClient.h"
#include "PlatformCALayerWin.h"
#include "TimeRanges.h"
#include "WebCoreAVCFResourceLoader.h"
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <wtf/URL.h>

#include <AVFoundationCF/AVCFPlayerItem.h>
#if HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
#include <AVFoundationCF/AVCFPlayerItemLegibleOutput.h>
#endif
#include <AVFoundationCF/AVCFPlayerLayer.h>
#if HAVE(AVFOUNDATION_LOADER_DELEGATE)
#include <AVFoundationCF/AVCFAssetResourceLoader.h>
#endif
#include <AVFoundationCF/AVFoundationCF.h>
#include <d3d9.h>
#include <delayimp.h>
#include <dispatch/dispatch.h>
#if HAVE(AVFOUNDATION_LOADER_DELEGATE) && ENABLE(LEGACY_ENCRYPTED_MEDIA)
#include <JavaScriptCore/DataView.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/TypedArrayInlines.h>
#include <JavaScriptCore/Uint16Array.h>
#endif
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StringPrintStream.h>
#include <wtf/Threading.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringView.h>

// Soft-linking headers must be included last since they #define functions, constants, etc.
#include "AVFoundationCFSoftLinking.h"
#include <pal/cf/CoreMediaSoftLink.h>

// We don't bother softlinking against libdispatch since it's already been loaded by AAS.
#ifdef DEBUG_ALL
#pragma comment(lib, "libdispatch_debug.lib")
#else
#pragma comment(lib, "libdispatch.lib")
#endif

enum {
    AVAssetReferenceRestrictionForbidRemoteReferenceToLocal = (1UL << 0),
    AVAssetReferenceRestrictionForbidLocalReferenceToRemote = (1UL << 1)
};


namespace WebCore {
using namespace std;
using namespace PAL;

class LayerClient;

class AVFWrapper {
    WTF_MAKE_FAST_ALLOCATED;
public:
    AVFWrapper(MediaPlayerPrivateAVFoundationCF*);
    ~AVFWrapper();

    void scheduleDisconnectAndDelete();

    void createAVCFVideoLayer();
    void destroyVideoLayer();
    PlatformLayer* platformLayer();

    CACFLayerRef caVideoLayer() { return m_caVideoLayer.get(); }
    PlatformLayer* videoLayerWrapper() { return m_videoLayerWrapper ? m_videoLayerWrapper->platformLayer() : 0; };
    void setVideoLayerNeedsCommit();
    void setVideoLayerHidden(bool);

    void createImageGenerator();
    void destroyImageGenerator();
    RetainPtr<CGImageRef> createImageForTimeInRect(const MediaTime&, const FloatRect&);

    void createAssetForURL(const URL&, bool inheritURI);
    void setAsset(AVCFURLAssetRef);
    
    void createPlayer(IDirect3DDevice9*);
    void createPlayerItem();
    
    void checkPlayability();
    void beginLoadingMetadata();
    
    void seekToTime(const MediaTime&, const MediaTime&, const MediaTime&);
    void updateVideoLayerGravity();

    void setCurrentTextTrack(InbandTextTrackPrivateAVF*);
    InbandTextTrackPrivateAVF* currentTextTrack() const { return m_currentTextTrack; }

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP) && HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
    static void legibleOutputCallback(void* context, AVCFPlayerItemLegibleOutputRef, CFArrayRef attributedString, CFArrayRef nativeSampleBuffers, CMTime itemTime);
    static void processCue(void* context);
#endif
#if HAVE(AVFOUNDATION_LOADER_DELEGATE)
    static Boolean resourceLoaderShouldWaitForLoadingOfRequestedResource(AVCFAssetResourceLoaderRef, AVCFAssetResourceLoadingRequestRef, void* context);
#endif
    static void loadMetadataCompletionCallback(AVCFAssetRef, void*);
    static void loadPlayableCompletionCallback(AVCFAssetRef, void*);
    static void periodicTimeObserverCallback(AVCFPlayerRef, CMTime, void*);
    static void seekCompletedCallback(AVCFPlayerItemRef, Boolean, void*);
    static void notificationCallback(CFNotificationCenterRef, void*, CFStringRef, const void*, CFDictionaryRef);
    static void processNotification(void* context);

    inline AVCFPlayerLayerRef videoLayer() const { return (AVCFPlayerLayerRef)m_avCFVideoLayer.get(); }
    inline AVCFPlayerRef avPlayer() const { return (AVCFPlayerRef)m_avPlayer.get(); }
    inline AVCFURLAssetRef avAsset() const { return (AVCFURLAssetRef)m_avAsset.get(); }
    inline AVCFPlayerItemRef avPlayerItem() const { return (AVCFPlayerItemRef)m_avPlayerItem.get(); }
    inline AVCFPlayerObserverRef timeObserver() const { return (AVCFPlayerObserverRef)m_timeObserver.get(); }
    inline AVCFAssetImageGeneratorRef imageGenerator() const { return m_imageGenerator.get(); }
#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP) && HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
    inline AVCFPlayerItemLegibleOutputRef legibleOutput() const { return m_legibleOutput.get(); }
    AVCFMediaSelectionGroupRef safeMediaSelectionGroupForLegibleMedia() const;
#endif
    dispatch_queue_t dispatchQueue() const { return m_notificationQueue; }

#if HAVE(AVFOUNDATION_LOADER_DELEGATE) && ENABLE(LEGACY_ENCRYPTED_MEDIA)
    RetainPtr<AVCFAssetResourceLoadingRequestRef> takeRequestForKeyURI(const String&);
    void setRequestForKey(const String& keyURI, AVCFAssetResourceLoadingRequestRef avRequest);
#endif

private:
    inline void* callbackContext() const { return reinterpret_cast<void*>(m_objectID); }

    static Lock mapLock;
    static HashMap<uintptr_t, AVFWrapper*>& map() WTF_REQUIRES_LOCK(mapLock);
    static AVFWrapper* avfWrapperForCallbackContext(void*) WTF_REQUIRES_LOCK(mapLock);
    void addToMap();
    void removeFromMap() const;
#if HAVE(AVFOUNDATION_LOADER_DELEGATE)
    bool shouldWaitForLoadingOfResource(AVCFAssetResourceLoadingRequestRef avRequest);
    static void processShouldWaitForLoadingOfResource(void* context);
#endif

    static void disconnectAndDeleteAVFWrapper(void*);

    static uintptr_t s_nextAVFWrapperObjectID;
    uintptr_t m_objectID;

    MediaPlayerPrivateAVFoundationCF* m_owner;

    RetainPtr<AVCFPlayerRef> m_avPlayer;
    RetainPtr<AVCFURLAssetRef> m_avAsset;
    RetainPtr<AVCFPlayerItemRef> m_avPlayerItem;
    RetainPtr<AVCFPlayerLayerRef> m_avCFVideoLayer;
    RetainPtr<AVCFPlayerObserverRef> m_timeObserver;
    RetainPtr<AVCFAssetImageGeneratorRef> m_imageGenerator;
#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP) && HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
    RetainPtr<AVCFPlayerItemLegibleOutputRef> m_legibleOutput;
    RetainPtr<AVCFMediaSelectionGroupRef> m_selectionGroup;
#endif

    dispatch_queue_t m_notificationQueue;

    mutable RetainPtr<CACFLayerRef> m_caVideoLayer;
    RefPtr<PlatformCALayer> m_videoLayerWrapper;

    std::unique_ptr<LayerClient> m_layerClient;
    COMPtr<IDirect3DDevice9Ex> m_d3dDevice;

    InbandTextTrackPrivateAVF* m_currentTextTrack;

#if HAVE(AVFOUNDATION_LOADER_DELEGATE)
    HashMap<String, Vector<RetainPtr<AVCFAssetResourceLoadingRequestRef>>> m_keyURIToRequestMap;
    AVCFAssetResourceLoaderCallbacks m_resourceLoaderCallbacks;
#endif
};

uintptr_t AVFWrapper::s_nextAVFWrapperObjectID;

class LayerClient : public PlatformCALayerClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    LayerClient(AVFWrapper* parent) : m_parent(parent) { }
    virtual ~LayerClient() { m_parent = 0; }

private:
    virtual void platformCALayerLayoutSublayersOfLayer(PlatformCALayer*);
    virtual bool platformCALayerRespondsToLayoutChanges() const { return true; }

    virtual void platformCALayerAnimationStarted(MonotonicTime beginTime) { }
    virtual GraphicsLayer::CompositingCoordinatesOrientation platformCALayerContentsOrientation() const { return GraphicsLayer::CompositingCoordinatesOrientation::TopDown; }
    virtual void platformCALayerPaintContents(PlatformCALayer*, GraphicsContext&, const FloatRect&, GraphicsLayerPaintBehavior) { }
    virtual bool platformCALayerShowDebugBorders() const { return false; }
    virtual bool platformCALayerShowRepaintCounter(PlatformCALayer*) const { return false; }
    virtual int platformCALayerIncrementRepaintCount(PlatformCALayer*) { return 0; }

    virtual bool platformCALayerContentsOpaque() const { return false; }
    virtual bool platformCALayerDrawsContent() const { return false; }
    virtual float platformCALayerDeviceScaleFactor() const { return 1; }

    AVFWrapper* m_parent;
};

#if !LOG_DISABLED
static const char* boolString(bool val)
{
    return val ? "true" : "false";
}
#endif

static RetainPtr<CFArrayRef> createMetadataKeyNames()
{
    static const CFStringRef keyNames[] = {
        AVCFAssetPropertyDuration,
        AVCFAssetPropertyNaturalSize,
        AVCFAssetPropertyPreferredTransform,
        AVCFAssetPropertyPreferredRate,
        AVCFAssetPropertyPlayable,
        AVCFAssetPropertyTracks,
#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
        AVCFAssetPropertyAvailableMediaCharacteristicsWithMediaSelectionOptions,
#endif
    };
    
    return adoptCF(CFArrayCreate(0, (const void**)keyNames, sizeof(keyNames) / sizeof(keyNames[0]), &kCFTypeArrayCallBacks));
}

static CFArrayRef metadataKeyNames()
{
    static NeverDestroyed<RetainPtr<CFArrayRef>> keys = createMetadataKeyNames();
    return keys.get().get();
}

// FIXME: It would be better if AVCFTimedMetadataGroup.h exported this key.
static CFStringRef CMTimeRangeStartKey()
{
    return CFSTR("start");
}

// FIXME: It would be better if AVCFTimedMetadataGroup.h exported this key.
static CFStringRef CMTimeRangeDurationKey()
{
    return CFSTR("duration");
}

// FIXME: It would be better if AVCF exported this notification name.
static CFStringRef CACFContextNeedsFlushNotification()
{
    return CFSTR("kCACFContextNeedsFlushNotification");
}

// Define AVCF object accessors as inline functions here instead of in MediaPlayerPrivateAVFoundationCF so we don't have
// to include the AVCF headers in MediaPlayerPrivateAVFoundationCF.h
inline AVCFPlayerLayerRef videoLayer(AVFWrapper* wrapper)
{ 
    return wrapper ? wrapper->videoLayer() : 0; 
}

inline AVCFPlayerRef avPlayer(AVFWrapper* wrapper)
{ 
    return wrapper ? wrapper->avPlayer() : 0; 
}

inline AVCFURLAssetRef avAsset(AVFWrapper* wrapper)
{ 
    return wrapper ? wrapper->avAsset() : 0; 
}

inline AVCFPlayerItemRef avPlayerItem(AVFWrapper* wrapper)
{ 
    return wrapper ? wrapper->avPlayerItem() : 0; 
}

inline AVCFAssetImageGeneratorRef imageGenerator(AVFWrapper* wrapper)
{ 
    return wrapper ? wrapper->imageGenerator() : 0; 
}

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP) && HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
inline AVCFPlayerItemLegibleOutputRef avLegibleOutput(AVFWrapper* wrapper)
{
    return wrapper ? wrapper->legibleOutput() : 0;
}

inline AVCFMediaSelectionGroupRef safeMediaSelectionGroupForLegibleMedia(AVFWrapper* wrapper)
{
    return wrapper ? wrapper->safeMediaSelectionGroupForLegibleMedia() : 0;
}
#endif

#if HAVE(AVFOUNDATION_LOADER_DELEGATE)
static dispatch_queue_t globalQueue = nullptr;

static void initGlobalLoaderDelegateQueue(void* ctx)
{
    globalQueue = dispatch_queue_create("WebCoreAVFLoaderDelegate queue", DISPATCH_QUEUE_SERIAL);
}

static dispatch_queue_t globalLoaderDelegateQueue()
{
    static dispatch_once_t onceToken;

    dispatch_once_f(&onceToken, nullptr, initGlobalLoaderDelegateQueue);

    return globalQueue;
}
#endif

class MediaPlayerFactoryAVFoundationCF final : public MediaPlayerFactory {
private:
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::AVFoundationCF; };

    std::unique_ptr<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer* player) const final
    {
        return makeUnique<MediaPlayerPrivateAVFoundationCF>(player);
    }

    void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types) const final
    {
        return MediaPlayerPrivateAVFoundationCF::getSupportedTypes(types);
    }

    MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters) const final
    {
        return MediaPlayerPrivateAVFoundationCF::supportsType(parameters);
    }

    bool supportsKeySystem(const String& keySystem, const String& mimeType) const final
    {
        return MediaPlayerPrivateAVFoundationCF::supportsKeySystem(keySystem, mimeType);
    }
};

void MediaPlayerPrivateAVFoundationCF::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (!isAvailable())
        return;

    registrar(makeUnique<MediaPlayerFactoryAVFoundationCF>());
}

MediaPlayerPrivateAVFoundationCF::MediaPlayerPrivateAVFoundationCF(MediaPlayer* player)
    : MediaPlayerPrivateAVFoundation(player)
    , m_avfWrapper(0)
    , m_videoFrameHasDrawn(false)
{
    LOG(Media, "MediaPlayerPrivateAVFoundationCF::MediaPlayerPrivateAVFoundationCF(%p)", this);
}

MediaPlayerPrivateAVFoundationCF::~MediaPlayerPrivateAVFoundationCF()
{
    LOG(Media, "MediaPlayerPrivateAVFoundationCF::~MediaPlayerPrivateAVFoundationCF(%p)", this);
#if HAVE(AVFOUNDATION_LOADER_DELEGATE)
    for (auto& pair : m_resourceLoaderMap)
        pair.value->invalidate();
#endif
    cancelLoad();
}

void MediaPlayerPrivateAVFoundationCF::cancelLoad()
{
    LOG(Media, "MediaPlayerPrivateAVFoundationCF::cancelLoad(%p)", this);

    // Do nothing when our cancellation of pending loading calls its completion handler
    setDelayCallbacks(true);
    setIgnoreLoadStateChanges(true);

    tearDownVideoRendering();

    clearTextTracks();

    if (m_avfWrapper) {
        // The AVCF objects have to be destroyed on the same dispatch queue used for notifications, so schedule a call to 
        // disconnectAndDeleteAVFWrapper on that queue. 
        m_avfWrapper->scheduleDisconnectAndDelete();
        m_avfWrapper = 0;
    }

    setIgnoreLoadStateChanges(false);
    setDelayCallbacks(false);
}

void MediaPlayerPrivateAVFoundationCF::updateVideoLayerGravity()
{
    ASSERT(supportsAcceleratedRendering());

    if (m_avfWrapper)
        m_avfWrapper->updateVideoLayerGravity();
}

bool MediaPlayerPrivateAVFoundationCF::hasLayerRenderer() const
{
    return videoLayer(m_avfWrapper);
}

bool MediaPlayerPrivateAVFoundationCF::hasContextRenderer() const
{
    return imageGenerator(m_avfWrapper);
}

void MediaPlayerPrivateAVFoundationCF::createContextVideoRenderer()
{
    LOG(Media, "MediaPlayerPrivateAVFoundationCF::createContextVideoRenderer(%p)", this);
    ASSERT(isMainThread());

    if (imageGenerator(m_avfWrapper))
        return;

    if (!m_avfWrapper)
        return;

    m_avfWrapper->createImageGenerator();
    setNeedsRenderingModeChanged();
}

void MediaPlayerPrivateAVFoundationCF::destroyContextVideoRenderer()
{
    ASSERT(isMainThread());
    if (!m_avfWrapper)
        return;

    m_avfWrapper->destroyImageGenerator();
    setNeedsRenderingModeChanged();
}

void MediaPlayerPrivateAVFoundationCF::createVideoLayer()
{
    ASSERT(isMainThread());
    ASSERT(supportsAcceleratedRendering());

    if (!m_avfWrapper)
        return;

    m_avfWrapper->createAVCFVideoLayer();
    setNeedsRenderingModeChanged();
}

void MediaPlayerPrivateAVFoundationCF::destroyVideoLayer()
{
    ASSERT(isMainThread());
    LOG(Media, "MediaPlayerPrivateAVFoundationCF::destroyVideoLayer(%p) - destroying %p", this, videoLayer(m_avfWrapper));
    if (!m_avfWrapper)
        return;

    m_avfWrapper->destroyVideoLayer();
    setNeedsRenderingModeChanged();
}

bool MediaPlayerPrivateAVFoundationCF::hasAvailableVideoFrame() const
{
    return (m_videoFrameHasDrawn || (videoLayer(m_avfWrapper) && AVCFPlayerLayerIsReadyForDisplay(videoLayer(m_avfWrapper))));
}

void MediaPlayerPrivateAVFoundationCF::setCurrentTextTrack(InbandTextTrackPrivateAVF* track)
{
    if (m_avfWrapper)
        m_avfWrapper->setCurrentTextTrack(track);
}

InbandTextTrackPrivateAVF* MediaPlayerPrivateAVFoundationCF::currentTextTrack() const
{
    if (m_avfWrapper)
        return m_avfWrapper->currentTextTrack();

    return 0;
}

void MediaPlayerPrivateAVFoundationCF::createAVAssetForURL(const URL& url)
{
    ASSERT(!m_avfWrapper);

    setDelayCallbacks(true);

    bool inheritURI = player()->doesHaveAttribute("x-itunes-inherit-uri-query-component");

    m_avfWrapper = new AVFWrapper(this);
    m_avfWrapper->createAssetForURL(url, inheritURI);
    setDelayCallbacks(false);
    m_avfWrapper->checkPlayability();
}

void MediaPlayerPrivateAVFoundationCF::createAVPlayer()
{
    ASSERT(isMainThread());
    ASSERT(m_avfWrapper);
    
    setDelayCallbacks(true);
    m_avfWrapper->createPlayer(reinterpret_cast<IDirect3DDevice9*>(player()->graphicsDeviceAdapter()));
    setDelayCallbacks(false);
}

void MediaPlayerPrivateAVFoundationCF::createAVPlayerItem()
{
    ASSERT(isMainThread());
    ASSERT(m_avfWrapper);
    
    setDelayCallbacks(true);
    m_avfWrapper->createPlayerItem();

    setDelayCallbacks(false);
}

void MediaPlayerPrivateAVFoundationCF::beginLoadingMetadata()
{
    ASSERT(m_avfWrapper);
    m_avfWrapper->beginLoadingMetadata();
}

MediaPlayerPrivateAVFoundation::ItemStatus MediaPlayerPrivateAVFoundationCF::playerItemStatus() const
{
    if (!avPlayerItem(m_avfWrapper))
        return MediaPlayerPrivateAVFoundation::MediaPlayerAVPlayerItemStatusDoesNotExist;

    AVCFPlayerItemStatus status = AVCFPlayerItemGetStatus(avPlayerItem(m_avfWrapper), 0);
    if (status == AVCFPlayerItemStatusUnknown)
        return MediaPlayerPrivateAVFoundation::MediaPlayerAVPlayerItemStatusUnknown;
    if (status == AVCFPlayerItemStatusFailed)
        return MediaPlayerPrivateAVFoundation::MediaPlayerAVPlayerItemStatusFailed;
    if (AVCFPlayerItemIsPlaybackLikelyToKeepUp(avPlayerItem(m_avfWrapper)))
        return MediaPlayerPrivateAVFoundation::MediaPlayerAVPlayerItemStatusPlaybackLikelyToKeepUp;
    if (AVCFPlayerItemIsPlaybackBufferFull(avPlayerItem(m_avfWrapper)))
        return MediaPlayerPrivateAVFoundation::MediaPlayerAVPlayerItemStatusPlaybackBufferFull;
    if (AVCFPlayerItemIsPlaybackBufferEmpty(avPlayerItem(m_avfWrapper)))
        return MediaPlayerPrivateAVFoundation::MediaPlayerAVPlayerItemStatusPlaybackBufferEmpty;
    return MediaPlayerPrivateAVFoundation::MediaPlayerAVPlayerItemStatusReadyToPlay;
}

PlatformLayer* MediaPlayerPrivateAVFoundationCF::platformLayer() const
{
    ASSERT(isMainThread());
    if (!m_avfWrapper)
        return 0;

    return m_avfWrapper->platformLayer();
}

void MediaPlayerPrivateAVFoundationCF::platformSetVisible(bool isVisible)
{
    ASSERT(isMainThread());
    if (!m_avfWrapper)
        return;
    
    // FIXME: We use a CATransaction here on the Mac, we need to figure out why this was done there and
    // whether we're affected by the same issue.
    setDelayCallbacks(true);
    m_avfWrapper->setVideoLayerHidden(!isVisible);    
    if (!isVisible)
        tearDownVideoRendering();
    setDelayCallbacks(false);
}

void MediaPlayerPrivateAVFoundationCF::platformPlay()
{
    LOG(Media, "MediaPlayerPrivateAVFoundationCF::play(%p)", this);
    if (!metaDataAvailable() || !avPlayer(m_avfWrapper))
        return;

    setDelayCallbacks(true);
    AVCFPlayerSetRate(avPlayer(m_avfWrapper), player()->requestedRate());
    setDelayCallbacks(false);
}

void MediaPlayerPrivateAVFoundationCF::platformPause()
{
    LOG(Media, "MediaPlayerPrivateAVFoundationCF::pause(%p)", this);
    if (!metaDataAvailable() || !avPlayer(m_avfWrapper))
        return;

    setDelayCallbacks(true);
    AVCFPlayerSetRate(avPlayer(m_avfWrapper), 0);
    setDelayCallbacks(false);
}

MediaTime MediaPlayerPrivateAVFoundationCF::platformDuration() const
{
    if (!metaDataAvailable() || !avAsset(m_avfWrapper))
        return MediaTime::zeroTime();

    CMTime cmDuration;

    // Check the AVItem if we have one and it has loaded duration, some assets never report duration.
    if (avPlayerItem(m_avfWrapper) && playerItemStatus() >= MediaPlayerAVPlayerItemStatusReadyToPlay)
        cmDuration = AVCFPlayerItemGetDuration(avPlayerItem(m_avfWrapper));
    else
        cmDuration = AVCFAssetGetDuration(avAsset(m_avfWrapper));

    if (CMTIME_IS_NUMERIC(cmDuration))
        return PAL::toMediaTime(cmDuration);

    if (CMTIME_IS_INDEFINITE(cmDuration))
        return MediaTime::positiveInfiniteTime();

    LOG(Media, "MediaPlayerPrivateAVFoundationCF::platformDuration(%p) - invalid duration, returning %s", this, toString(MediaTime::invalidTime()).utf8().data());
    return MediaTime::invalidTime();
}

MediaTime MediaPlayerPrivateAVFoundationCF::currentMediaTime() const
{
    if (!metaDataAvailable() || !avPlayerItem(m_avfWrapper))
        return MediaTime::zeroTime();

    CMTime itemTime = AVCFPlayerItemGetCurrentTime(avPlayerItem(m_avfWrapper));
    if (CMTIME_IS_NUMERIC(itemTime))
        return max(PAL::toMediaTime(itemTime), MediaTime::zeroTime());

    return MediaTime::zeroTime();
}

void MediaPlayerPrivateAVFoundationCF::seekToTime(const MediaTime& time, const MediaTime& negativeTolerance, const MediaTime& positiveTolerance)
{
    if (!m_avfWrapper)
        return;
    
    // seekToTime generates several event callbacks, update afterwards.
    setDelayCallbacks(true);
    m_avfWrapper->seekToTime(time, negativeTolerance, positiveTolerance);
    setDelayCallbacks(false);
}

void MediaPlayerPrivateAVFoundationCF::setVolume(float volume)
{
    if (!metaDataAvailable() || !avPlayer(m_avfWrapper))
        return;

    AVCFPlayerSetVolume(avPlayer(m_avfWrapper), volume);
}

void MediaPlayerPrivateAVFoundationCF::setClosedCaptionsVisible(bool closedCaptionsVisible)
{
    if (!metaDataAvailable() || !avPlayer(m_avfWrapper))
        return;

    LOG(Media, "MediaPlayerPrivateAVFoundationCF::setClosedCaptionsVisible(%p) - setting to %s", this, boolString(closedCaptionsVisible));
    AVCFPlayerSetClosedCaptionDisplayEnabled(avPlayer(m_avfWrapper), closedCaptionsVisible);
}

void MediaPlayerPrivateAVFoundationCF::setRate(float rate)
{
    LOG(Media, "MediaPlayerPrivateAVFoundationCF::setRate(%p) - rate: %f", this, rate);
    if (!metaDataAvailable() || !avPlayer(m_avfWrapper))
        return;

    setDelayCallbacks(true);
    AVCFPlayerSetRate(avPlayer(m_avfWrapper), rate);
    setDelayCallbacks(false);
}

double MediaPlayerPrivateAVFoundationCF::rate() const
{
    if (!metaDataAvailable() || !avPlayer(m_avfWrapper))
        return 0;

    setDelayCallbacks(true);
    double currentRate = AVCFPlayerGetRate(avPlayer(m_avfWrapper));
    setDelayCallbacks(false);

    return currentRate;
}

static bool timeRangeIsValidAndNotEmpty(CMTime start, CMTime duration)
{
    // Is the range valid?
    if (!CMTIME_IS_VALID(start) || !CMTIME_IS_VALID(duration) || duration.epoch || duration.value < 0)
        return false;

    if (CMTIME_COMPARE_INLINE(duration, ==, kCMTimeZero))
        return false;

    return true;
}

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateAVFoundationCF::platformBufferedTimeRanges() const
{
    auto timeRanges = makeUnique<PlatformTimeRanges>();

    if (!avPlayerItem(m_avfWrapper))
        return timeRanges;

    RetainPtr<CFArrayRef> loadedRanges = adoptCF(AVCFPlayerItemCopyLoadedTimeRanges(avPlayerItem(m_avfWrapper)));
    if (!loadedRanges)
        return timeRanges;

    CFIndex rangeCount = CFArrayGetCount(loadedRanges.get());
    for (CFIndex i = 0; i < rangeCount; i++) {
        CFDictionaryRef range = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(loadedRanges.get(), i));
        CMTime start = CMTimeMakeFromDictionary(static_cast<CFDictionaryRef>(CFDictionaryGetValue(range, CMTimeRangeStartKey())));
        CMTime duration = CMTimeMakeFromDictionary(static_cast<CFDictionaryRef>(CFDictionaryGetValue(range, CMTimeRangeDurationKey())));
        
        if (timeRangeIsValidAndNotEmpty(start, duration)) {
            MediaTime rangeStart = PAL::toMediaTime(start);
            MediaTime rangeEnd = rangeStart + PAL::toMediaTime(duration);
            timeRanges->add(rangeStart, rangeEnd);
        }
    }

    return timeRanges;
}

MediaTime MediaPlayerPrivateAVFoundationCF::platformMinTimeSeekable() const 
{ 
    RetainPtr<CFArrayRef> seekableRanges = adoptCF(AVCFPlayerItemCopySeekableTimeRanges(avPlayerItem(m_avfWrapper)));
    if (!seekableRanges) 
        return MediaTime::zeroTime(); 

    MediaTime minTimeSeekable = MediaTime::positiveInfiniteTime();
    bool hasValidRange = false; 
    CFIndex rangeCount = CFArrayGetCount(seekableRanges.get());
    for (CFIndex i = 0; i < rangeCount; i++) {
        CFDictionaryRef range = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(seekableRanges.get(), i));
        CMTime start = CMTimeMakeFromDictionary(static_cast<CFDictionaryRef>(CFDictionaryGetValue(range, CMTimeRangeStartKey())));
        CMTime duration = CMTimeMakeFromDictionary(static_cast<CFDictionaryRef>(CFDictionaryGetValue(range, CMTimeRangeDurationKey())));
        if (!timeRangeIsValidAndNotEmpty(start, duration))
            continue;

        hasValidRange = true; 
        MediaTime startOfRange = PAL::toMediaTime(start);
        if (minTimeSeekable > startOfRange) 
            minTimeSeekable = startOfRange; 
    } 
    return hasValidRange ? minTimeSeekable : MediaTime::zeroTime(); 
} 

MediaTime MediaPlayerPrivateAVFoundationCF::platformMaxTimeSeekable() const
{
    if (!avPlayerItem(m_avfWrapper))
        return MediaTime::zeroTime();

    RetainPtr<CFArrayRef> seekableRanges = adoptCF(AVCFPlayerItemCopySeekableTimeRanges(avPlayerItem(m_avfWrapper)));
    if (!seekableRanges)
        return MediaTime::zeroTime();

    MediaTime maxTimeSeekable;
    CFIndex rangeCount = CFArrayGetCount(seekableRanges.get());
    for (CFIndex i = 0; i < rangeCount; i++) {
        CFDictionaryRef range = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(seekableRanges.get(), i));
        CMTime start = CMTimeMakeFromDictionary(static_cast<CFDictionaryRef>(CFDictionaryGetValue(range, CMTimeRangeStartKey())));
        CMTime duration = CMTimeMakeFromDictionary(static_cast<CFDictionaryRef>(CFDictionaryGetValue(range, CMTimeRangeDurationKey())));
        if (!timeRangeIsValidAndNotEmpty(start, duration))
            continue;
        
        MediaTime endOfRange = PAL::toMediaTime(CMTimeAdd(start, duration));
        if (maxTimeSeekable < endOfRange)
            maxTimeSeekable = endOfRange;
    }

    return maxTimeSeekable;   
}

MediaTime MediaPlayerPrivateAVFoundationCF::platformMaxTimeLoaded() const
{
    if (!avPlayerItem(m_avfWrapper))
        return MediaTime::zeroTime();

    RetainPtr<CFArrayRef> loadedRanges = adoptCF(AVCFPlayerItemCopyLoadedTimeRanges(avPlayerItem(m_avfWrapper)));
    if (!loadedRanges)
        return MediaTime::zeroTime();

    MediaTime maxTimeLoaded;
    CFIndex rangeCount = CFArrayGetCount(loadedRanges.get());
    for (CFIndex i = 0; i < rangeCount; i++) {
        CFDictionaryRef range = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(loadedRanges.get(), i));
        CMTime start = CMTimeMakeFromDictionary(static_cast<CFDictionaryRef>(CFDictionaryGetValue(range, CMTimeRangeStartKey())));
        CMTime duration = CMTimeMakeFromDictionary(static_cast<CFDictionaryRef>(CFDictionaryGetValue(range, CMTimeRangeDurationKey())));
        if (!timeRangeIsValidAndNotEmpty(start, duration))
            continue;
        
        MediaTime endOfRange = PAL::toMediaTime(CMTimeAdd(start, duration));
        if (maxTimeLoaded < endOfRange)
            maxTimeLoaded = endOfRange;
    }

    return maxTimeLoaded;   
}

unsigned long long MediaPlayerPrivateAVFoundationCF::totalBytes() const
{
    if (!metaDataAvailable() || !avAsset(m_avfWrapper))
        return 0;

    int64_t totalMediaSize = 0;
    RetainPtr<CFArrayRef> tracks = adoptCF(AVCFAssetCopyAssetTracks(avAsset(m_avfWrapper)));
    CFIndex trackCount = CFArrayGetCount(tracks.get());
    for (CFIndex i = 0; i < trackCount; i++) {
        AVCFAssetTrackRef assetTrack = (AVCFAssetTrackRef)CFArrayGetValueAtIndex(tracks.get(), i);
        totalMediaSize += AVCFAssetTrackGetTotalSampleDataLength(assetTrack);
    }

    return static_cast<unsigned long long>(totalMediaSize);
}

MediaPlayerPrivateAVFoundation::AssetStatus MediaPlayerPrivateAVFoundationCF::assetStatus() const
{
    if (!avAsset(m_avfWrapper))
        return MediaPlayerAVAssetStatusDoesNotExist;

    // First, make sure all metadata properties we rely on are loaded.
    CFArrayRef keys = metadataKeyNames();
    CFIndex keyCount = CFArrayGetCount(keys);
    for (CFIndex i = 0; i < keyCount; i++) {
        CFStringRef keyName = static_cast<CFStringRef>(CFArrayGetValueAtIndex(keys, i));
        AVCFPropertyValueStatus keyStatus = AVCFAssetGetStatusOfValueForProperty(avAsset(m_avfWrapper), keyName, 0);

        if (keyStatus < AVCFPropertyValueStatusLoaded)
            return MediaPlayerAVAssetStatusLoading;
        if (keyStatus == AVCFPropertyValueStatusFailed) {
            if (CFStringCompare(keyName, AVCFAssetPropertyNaturalSize, 0) == kCFCompareEqualTo) {
                // Don't treat a failure to retrieve @"naturalSize" as fatal. We will use @"presentationSize" instead.
                // <rdar://problem/15966685>
                continue;
            }
#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
            if (CFStringCompare(keyName, AVCFAssetPropertyAvailableMediaCharacteristicsWithMediaSelectionOptions, 0) == kCFCompareEqualTo) {
                // On Windows, the media selection options are not available when initially interacting with a streaming source.
                // <rdar://problem/16160699>
                continue;
            }
#endif
            return MediaPlayerAVAssetStatusFailed;
        }
        if (keyStatus == AVCFPropertyValueStatusCancelled)
            return MediaPlayerAVAssetStatusCancelled;
    }

    if (AVCFAssetIsPlayable(avAsset(m_avfWrapper)))
        return MediaPlayerAVAssetStatusPlayable;

    return MediaPlayerAVAssetStatusLoaded;
}

void MediaPlayerPrivateAVFoundationCF::paintCurrentFrameInContext(GraphicsContext& context, const FloatRect& rect)
{
    ASSERT(isMainThread());
    if (!metaDataAvailable() || context.paintingDisabled())
        return;

    if (currentRenderingMode() == MediaRenderingToLayer && !imageGenerator(m_avfWrapper)) {
        // We're being told to render into a context, but we already have the
        // video layer, which probably means we've been called from <canvas>.
        createContextVideoRenderer();
    }

    paint(context, rect);
}

void MediaPlayerPrivateAVFoundationCF::paint(GraphicsContext& context, const FloatRect& rect)
{
    ASSERT(isMainThread());
    if (!metaDataAvailable() || context.paintingDisabled() || !imageGenerator(m_avfWrapper))
        return;

    LOG(Media, "MediaPlayerPrivateAVFoundationCF::paint(%p)", this);

    setDelayCallbacks(true);
    RetainPtr<CGImageRef> image = m_avfWrapper->createImageForTimeInRect(currentMediaTime(), rect);
    if (image) {
        context.save();
        context.translate(rect.x(), rect.y() + rect.height());
        context.scale(FloatSize(1.0f, -1.0f));
        context.setImageInterpolationQuality(InterpolationQuality::Low);
        FloatRect paintRect(FloatPoint(), rect.size());
#if USE(DIRECT2D)
        notImplemented();
#else
        CGContextDrawImage(context.platformContext(), CGRectMake(0, 0, paintRect.width(), paintRect.height()), image.get());
#endif
        context.restore();
        image = 0;
    }
    setDelayCallbacks(false);
    
    m_videoFrameHasDrawn = true;
}

#if HAVE(AVFOUNDATION_LOADER_DELEGATE) && ENABLE(LEGACY_ENCRYPTED_MEDIA)

static bool keySystemIsSupported(const String& keySystem)
{
    return equalLettersIgnoringASCIICase(keySystem, "com.apple.fps")
        || equalLettersIgnoringASCIICase(keySystem, "com.apple.fps.1_0");
}

#endif

static const HashSet<String, ASCIICaseInsensitiveHash>& avfMIMETypes()
{
    static NeverDestroyed<HashSet<String, ASCIICaseInsensitiveHash>> cache = []() {
        HashSet<String, ASCIICaseInsensitiveHash> types;
        RetainPtr<CFArrayRef> avTypes = adoptCF(AVCFURLAssetCopyAudiovisualMIMETypes());
        CFIndex typeCount = CFArrayGetCount(avTypes.get());
        for (CFIndex i = 0; i < typeCount; ++i)
            types.add((CFStringRef)CFArrayGetValueAtIndex(avTypes.get(), i));
        return types;
    }();

    return cache;
}

void MediaPlayerPrivateAVFoundationCF::getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& supportedTypes)
{
    supportedTypes = avfMIMETypes();
}

MediaPlayer::SupportsType MediaPlayerPrivateAVFoundationCF::supportsType(const MediaEngineSupportParameters& parameters)
{
    auto containerType = parameters.type.containerType();
    if (isUnsupportedMIMEType(containerType))
        return MediaPlayer::SupportsType::IsNotSupported;

    if (!staticMIMETypeList().contains(containerType) && !avfMIMETypes().contains(containerType))
        return MediaPlayer::SupportsType::IsNotSupported;

    auto codecs = parameters.type.parameter(ContentType::codecsParameter());
#if HAVE(AVCFURL_PLAYABLE_MIMETYPE)
    // The spec says:
    // "Implementors are encouraged to return "maybe" unless the type can be confidently established as being supported or not."
    if (codecs.isEmpty())
        return MediaPlayer::SupportsType::MayBeSupported;

    String typeString = containerType + "; codecs=\"" + codecs + "\"";
    return AVCFURLAssetIsPlayableExtendedMIMEType(typeString.createCFString().get()) ? MediaPlayer::SupportsType::IsSupported : MediaPlayer::SupportsType::MayBeSupported;
#else
    if (avfMIMETypes().contains(containerType))
        return codecs.isEmpty() ? MediaPlayer::SupportsType::MayBeSupported : MediaPlayer::SupportsType::IsSupported;
    return MediaPlayer::SupportsType::IsNotSupported;
#endif
}

bool MediaPlayerPrivateAVFoundationCF::supportsKeySystem(const String& keySystem, const String& mimeType)
{
#if HAVE(AVFOUNDATION_LOADER_DELEGATE) && ENABLE(LEGACY_ENCRYPTED_MEDIA)
    if (keySystem.isEmpty())
        return false;

    if (!keySystemIsSupported(keySystem))
        return false;

    if (!mimeType.isEmpty() && !avfMIMETypes().contains(mimeType))
        return false;

    return true;
#else
    UNUSED_PARAM(keySystem);
    UNUSED_PARAM(mimeType);
    return false;
#endif
}

bool MediaPlayerPrivateAVFoundationCF::isAvailable()
{
    return AVFoundationCFLibrary() && isCoreMediaFrameworkAvailable();
}

#if HAVE(AVFOUNDATION_LOADER_DELEGATE)
void MediaPlayerPrivateAVFoundationCF::didCancelLoadingRequest(AVCFAssetResourceLoadingRequestRef avRequest)
{
    WebCoreAVCFResourceLoader* resourceLoader = m_resourceLoaderMap.get(avRequest);

    if (resourceLoader)
        resourceLoader->stopLoading();
}

void MediaPlayerPrivateAVFoundationCF::didStopLoadingRequest(AVCFAssetResourceLoadingRequestRef avRequest)
{
    m_resourceLoaderMap.remove(avRequest);
}
#endif

MediaTime MediaPlayerPrivateAVFoundationCF::mediaTimeForTimeValue(const MediaTime& timeValue) const
{
    if (!metaDataAvailable())
        return timeValue;

    // FIXME - can not implement until rdar://8721669 is fixed.
    return timeValue;
}

void MediaPlayerPrivateAVFoundationCF::tracksChanged()
{
    String primaryAudioTrackLanguage = m_languageOfPrimaryAudioTrack;
    m_languageOfPrimaryAudioTrack = String();

    if (!avAsset(m_avfWrapper))
        return;

    setDelayCharacteristicsChangedNotification(true);

    bool haveCCTrack = false;
    bool hasCaptions = false;

    // This is called whenever the tracks collection changes so cache hasVideo and hasAudio since we are
    // asked about those fairly frequently.
    if (!avPlayerItem(m_avfWrapper)) {
        // We don't have a player item yet, so check with the asset because some assets support inspection
        // prior to becoming ready to play.
        RetainPtr<CFArrayRef> visualTracks = adoptCF(AVCFAssetCopyTracksWithMediaCharacteristic(avAsset(m_avfWrapper), AVCFMediaCharacteristicVisual));
        setHasVideo(CFArrayGetCount(visualTracks.get()));

        RetainPtr<CFArrayRef> audioTracks = adoptCF(AVCFAssetCopyTracksWithMediaCharacteristic(avAsset(m_avfWrapper), AVCFMediaCharacteristicAudible));
        setHasAudio(CFArrayGetCount(audioTracks.get()));

#if !HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
        RetainPtr<CFArrayRef> captionTracks = adoptCF(AVCFAssetCopyTracksWithMediaType(avAsset(m_avfWrapper), AVCFMediaTypeClosedCaption));
        hasCaptions = CFArrayGetCount(captionTracks.get());
#endif
    } else {
        bool hasVideo = false;
        bool hasAudio = false;

        RetainPtr<CFArrayRef> tracks = adoptCF(AVCFPlayerItemCopyTracks(avPlayerItem(m_avfWrapper)));

        CFIndex trackCount = CFArrayGetCount(tracks.get());
        for (CFIndex i = 0; i < trackCount; i++) {
            AVCFPlayerItemTrackRef track = (AVCFPlayerItemTrackRef)(CFArrayGetValueAtIndex(tracks.get(), i));
            
            if (AVCFPlayerItemTrackIsEnabled(track)) {
                RetainPtr<AVCFAssetTrackRef> assetTrack = adoptCF(AVCFPlayerItemTrackCopyAssetTrack(track));
                if (!assetTrack) {
                    // Asset tracks may not be available yet when streaming. <rdar://problem/16160699>
                    LOG(Media, "MediaPlayerPrivateAVFoundationCF:tracksChanged(%p) - track = %d is enabled, but has no asset track.", this, track);
                    continue;
                }
                CFStringRef mediaType = AVCFAssetTrackGetMediaType(assetTrack.get());
                if (!mediaType)
                    continue;
                
                if (CFStringCompare(mediaType, AVCFMediaTypeVideo, kCFCompareCaseInsensitive) == kCFCompareEqualTo)
                    hasVideo = true;
                else if (CFStringCompare(mediaType, AVCFMediaTypeAudio, kCFCompareCaseInsensitive) == kCFCompareEqualTo)
                    hasAudio = true;
                else if (CFStringCompare(mediaType, AVCFMediaTypeClosedCaption, kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
#if !HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
                    hasCaptions = true;
#endif
                    haveCCTrack = true;
                }
            }
        }

        setHasVideo(hasVideo);
        setHasAudio(hasAudio);
    }

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
    AVCFMediaSelectionGroupRef legibleGroup = safeMediaSelectionGroupForLegibleMedia(m_avfWrapper);
    if (legibleGroup) {
        RetainPtr<CFArrayRef> playableOptions = adoptCF(AVCFMediaSelectionCopyPlayableOptionsFromArray(AVCFMediaSelectionGroupGetOptions(legibleGroup)));
        hasCaptions = CFArrayGetCount(playableOptions.get());
        if (hasCaptions)
            processMediaSelectionOptions();
    }
#endif

#if !HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
    if (haveCCTrack)
        processLegacyClosedCaptionsTracks();
#endif

    setHasClosedCaptions(hasCaptions);

    LOG(Media, "MediaPlayerPrivateAVFoundationCF:tracksChanged(%p) - hasVideo = %s, hasAudio = %s, hasCaptions = %s", 
        this, boolString(hasVideo()), boolString(hasAudio()), boolString(hasClosedCaptions()));

    sizeChanged();

    if (primaryAudioTrackLanguage != languageOfPrimaryAudioTrack())
        characteristicsChanged();

    setDelayCharacteristicsChangedNotification(false);
}

void MediaPlayerPrivateAVFoundationCF::sizeChanged()
{
    ASSERT(isMainThread());
    if (!avAsset(m_avfWrapper))
        return;
    
    // AVAsset's 'naturalSize' property only considers the movie's first video track, so we need to compute
    // the union of all visual track rects.
    CGRect trackRectUnion = CGRectZero;
    RetainPtr<CFArrayRef> tracks = adoptCF(AVCFAssetCopyTracksWithMediaType(avAsset(m_avfWrapper), AVCFMediaCharacteristicVisual));
    CFIndex trackCount = CFArrayGetCount(tracks.get());
    for (CFIndex i = 0; i < trackCount; i++) {
        AVCFAssetTrackRef assetTrack = (AVCFAssetTrackRef)(CFArrayGetValueAtIndex(tracks.get(), i));
        
        CGSize trackSize = AVCFAssetTrackGetNaturalSize(assetTrack);
        CGRect trackRect = CGRectMake(0, 0, trackSize.width, trackSize.height);
        trackRectUnion = CGRectUnion(trackRectUnion, CGRectApplyAffineTransform(trackRect, AVCFAssetTrackGetPreferredTransform(assetTrack)));
    }
    // The movie is always displayed at 0,0 so move the track rect to the origin before using width and height.
    trackRectUnion = CGRectOffset(trackRectUnion, trackRectUnion.origin.x, trackRectUnion.origin.y);
    CGSize naturalSize = trackRectUnion.size;

    if (!naturalSize.height && !naturalSize.width && avPlayerItem(m_avfWrapper))
        naturalSize = AVCFPlayerItemGetPresentationSize(avPlayerItem(m_avfWrapper));

    // Also look at the asset's preferred transform so we account for a movie matrix.
    CGSize movieSize = CGSizeApplyAffineTransform(AVCFAssetGetNaturalSize(avAsset(m_avfWrapper)), AVCFAssetGetPreferredTransform(avAsset(m_avfWrapper)));
    if (movieSize.width > naturalSize.width)
        naturalSize.width = movieSize.width;
    if (movieSize.height > naturalSize.height)
        naturalSize.height = movieSize.height;
    setNaturalSize(IntSize(naturalSize));
}

void MediaPlayerPrivateAVFoundationCF::resolvedURLChanged()
{
    if (m_avfWrapper && m_avfWrapper->avAsset())
        setResolvedURL(URL(adoptCF(AVCFAssetCopyResolvedURL(m_avfWrapper->avAsset())).get()));
    else
        setResolvedURL({ });
}

bool MediaPlayerPrivateAVFoundationCF::requiresImmediateCompositing() const
{
    // The AVFoundationCF player needs to have the root compositor available at construction time
    // so it can attach to the rendering device. Otherwise it falls back to CPU-only mode.
    //
    // It would be nice if AVCFPlayer had some way to switch to hardware-accelerated mode
    // when asked, then we could follow AVFoundation's model and switch to compositing
    // mode when beginning to play media.
    return true;
}

#if HAVE(AVFOUNDATION_LOADER_DELEGATE) && ENABLE(LEGACY_ENCRYPTED_MEDIA)

RetainPtr<AVCFAssetResourceLoadingRequestRef> MediaPlayerPrivateAVFoundationCF::takeRequestForKeyURI(const String& keyURI)
{
    if (!m_avfWrapper)
        return nullptr;

    return m_avfWrapper->takeRequestForKeyURI(keyURI);
}

std::unique_ptr<LegacyCDMSession> MediaPlayerPrivateAVFoundationCF::createSession(const String& keySystem, LegacyCDMSessionClient* client)
{
    if (!keySystemIsSupported(keySystem))
        return nullptr;

    return makeUnique<CDMSessionAVFoundationCF>(*this, client);
}

#elif ENABLE(LEGACY_ENCRYPTED_MEDIA)

std::unique_ptr<LegacyCDMSession> MediaPlayerPrivateAVFoundationCF::createSession(const String& keySystem, LegacyCDMSessionClient*)
{
    return nullptr;
}

#endif

long MediaPlayerPrivateAVFoundationCF::assetErrorCode() const
{
    if (!avAsset(m_avfWrapper))
        return 0;

    CFErrorRef error = nullptr;
    AVCFAssetGetStatusOfValueForProperty(avAsset(m_avfWrapper), AVCFAssetPropertyPlayable, &error);
    if (!error)
        return 0;

    long code = CFErrorGetCode(error);
    CFRelease(error);
    return code;
}

#if !HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
void MediaPlayerPrivateAVFoundationCF::processLegacyClosedCaptionsTracks()
{
#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
    AVCFPlayerItemSelectMediaOptionInMediaSelectionGroup(avPlayerItem(m_avfWrapper), 0, safeMediaSelectionGroupForLegibleMedia(m_avfWrapper));
#endif

    Vector<RefPtr<InbandTextTrackPrivateAVF> > removedTextTracks = m_textTracks;
    RetainPtr<CFArrayRef> tracks = adoptCF(AVCFPlayerItemCopyTracks(avPlayerItem(m_avfWrapper)));
    CFIndex trackCount = CFArrayGetCount(tracks.get());
    for (CFIndex i = 0; i < trackCount; ++i) {
        AVCFPlayerItemTrackRef playerItemTrack = (AVCFPlayerItemTrackRef)(CFArrayGetValueAtIndex(tracks.get(), i));

        RetainPtr<AVCFAssetTrackRef> assetTrack = adoptCF(AVCFPlayerItemTrackCopyAssetTrack(playerItemTrack));
        if (!assetTrack) {
            // Asset tracks may not be available yet when streaming. <rdar://problem/16160699>
            LOG(Media, "MediaPlayerPrivateAVFoundationCF:tracksChanged(%p) - track = %d is enabled, but has no asset track.", this, track);
            continue;
        }
        CFStringRef mediaType = AVCFAssetTrackGetMediaType(assetTrack.get());
        if (!mediaType)
            continue;
                
        if (CFStringCompare(mediaType, AVCFMediaTypeClosedCaption, kCFCompareCaseInsensitive) != kCFCompareEqualTo)
            continue;

        bool newCCTrack = true;
        for (unsigned i = removedTextTracks.size(); i > 0; --i) {
            if (removedTextTracks[i - 1]->textTrackCategory() != InbandTextTrackPrivateAVF::LegacyClosedCaption)
                continue;

            RefPtr<InbandTextTrackPrivateLegacyAVCF> track = static_cast<InbandTextTrackPrivateLegacyAVCF*>(m_textTracks[i - 1].get());
            if (track->avPlayerItemTrack() == playerItemTrack) {
                removedTextTracks.remove(i - 1);
                newCCTrack = false;
                break;
            }
        }

        if (!newCCTrack)
            continue;
        
        m_textTracks.append(InbandTextTrackPrivateLegacyAVCF::create(this, playerItemTrack));
    }

    processNewAndRemovedTextTracks(removedTextTracks);
}
#endif

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
void MediaPlayerPrivateAVFoundationCF::processMediaSelectionOptions()
{
    AVCFMediaSelectionGroupRef legibleGroup = safeMediaSelectionGroupForLegibleMedia(m_avfWrapper);
    if (!legibleGroup) {
        LOG(Media, "MediaPlayerPrivateAVFoundationCF::processMediaSelectionOptions(%p) - nil mediaSelectionGroup", this);
        return;
    }

    // We enabled automatic media selection because we want alternate audio tracks to be enabled/disabled automatically,
    // but set the selected legible track to nil so text tracks will not be automatically configured.
    if (!m_textTracks.size() && AVCFMediaSelectionGroupAllowsEmptySelection(legibleGroup)) {
        if (AVCFPlayerItemRef playerItem = avPlayerItem(m_avfWrapper))
            AVCFPlayerItemSelectMediaOptionInMediaSelectionGroup(playerItem, 0, legibleGroup);
    }

    Vector<RefPtr<InbandTextTrackPrivateAVF> > removedTextTracks = m_textTracks;
    RetainPtr<CFArrayRef> legibleOptions = adoptCF(AVCFMediaSelectionCopyPlayableOptionsFromArray(AVCFMediaSelectionGroupGetOptions(legibleGroup)));
    CFIndex legibleOptionsCount = CFArrayGetCount(legibleOptions.get());
    for (CFIndex i = 0; i < legibleOptionsCount; ++i) {
        AVCFMediaSelectionOptionRef option = static_cast<AVCFMediaSelectionOptionRef>(CFArrayGetValueAtIndex(legibleOptions.get(), i));
        bool newTrack = true;
        for (unsigned i = removedTextTracks.size(); i > 0; --i) {
            if (removedTextTracks[i - 1]->textTrackCategory() == InbandTextTrackPrivateAVF::LegacyClosedCaption)
                continue;

            RefPtr<InbandTextTrackPrivateAVCF> track = static_cast<InbandTextTrackPrivateAVCF*>(removedTextTracks[i - 1].get());
            if (CFEqual(track->mediaSelectionOption(), option)) {
                removedTextTracks.remove(i - 1);
                newTrack = false;
                break;
            }
        }
        if (!newTrack)
            continue;

        m_textTracks.append(InbandTextTrackPrivateAVCF::create(this, option, InbandTextTrackPrivate::CueFormat::Generic));
    }

    processNewAndRemovedTextTracks(removedTextTracks);
}

#endif // HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)

void AVFWrapper::setCurrentTextTrack(InbandTextTrackPrivateAVF* track)
{
    if (m_currentTextTrack == track)
        return;

    LOG(Media, "MediaPlayerPrivateAVFoundationCF::setCurrentTextTrack(%p) - selecting track %p, language = %s", this, track, track ? track->language().string().utf8().data() : "");
        
    m_currentTextTrack = track;

    if (track) {
        if (track->textTrackCategory() == InbandTextTrackPrivateAVF::LegacyClosedCaption)
            AVCFPlayerSetClosedCaptionDisplayEnabled(avPlayer(), TRUE);
#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
        else
            AVCFPlayerItemSelectMediaOptionInMediaSelectionGroup(avPlayerItem(), static_cast<InbandTextTrackPrivateAVCF*>(track)->mediaSelectionOption(), safeMediaSelectionGroupForLegibleMedia());
#endif
    } else {
#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
        AVCFPlayerItemSelectMediaOptionInMediaSelectionGroup(avPlayerItem(), 0, safeMediaSelectionGroupForLegibleMedia());
#endif
        AVCFPlayerSetClosedCaptionDisplayEnabled(avPlayer(), FALSE);
    }
}

String MediaPlayerPrivateAVFoundationCF::languageOfPrimaryAudioTrack() const
{
    if (!m_languageOfPrimaryAudioTrack.isNull())
        return m_languageOfPrimaryAudioTrack;

    if (!avPlayerItem(m_avfWrapper))
        return emptyString();

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
    // If AVFoundation has an audible group, return the language of the currently selected audible option.
    AVCFMediaSelectionGroupRef audibleGroup = AVCFAssetGetSelectionGroupForMediaCharacteristic(avAsset(m_avfWrapper), AVCFMediaCharacteristicAudible);
    AVCFMediaSelectionOptionRef currentlySelectedAudibleOption = AVCFPlayerItemGetSelectedMediaOptionInMediaSelectionGroup(avPlayerItem(m_avfWrapper), audibleGroup);
    if (currentlySelectedAudibleOption) {
        RetainPtr<CFLocaleRef> audibleOptionLocale = adoptCF(AVCFMediaSelectionOptionCopyLocale(currentlySelectedAudibleOption));
        if (audibleOptionLocale)
            m_languageOfPrimaryAudioTrack = CFLocaleGetIdentifier(audibleOptionLocale.get());
        else
            m_languageOfPrimaryAudioTrack = emptyString();

        LOG(Media, "MediaPlayerPrivateAVFoundationCF::languageOfPrimaryAudioTrack(%p) - returning language of selected audible option: %s", this, m_languageOfPrimaryAudioTrack.utf8().data());

        return m_languageOfPrimaryAudioTrack;
    }
#endif // HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)

    // AVFoundation synthesizes an audible group when there is only one ungrouped audio track if there is also a legible group (one or
    // more in-band text tracks). It doesn't know about out-of-band tracks, so if there is a single audio track return its language.
    RetainPtr<CFArrayRef> tracks = adoptCF(AVCFAssetCopyTracksWithMediaType(avAsset(m_avfWrapper), AVCFMediaTypeAudio));
    CFIndex trackCount = CFArrayGetCount(tracks.get());
    if (!tracks || trackCount != 1) {
        m_languageOfPrimaryAudioTrack = emptyString();
        LOG(Media, "MediaPlayerPrivateAVFoundationCF::languageOfPrimaryAudioTrack(%p) - %i audio tracks, returning emptyString()", this, (tracks ? trackCount : 0));
        return m_languageOfPrimaryAudioTrack;
    }

    AVCFAssetTrackRef track = (AVCFAssetTrackRef)CFArrayGetValueAtIndex(tracks.get(), 0);
    RetainPtr<CFStringRef> language = adoptCF(AVCFAssetTrackCopyExtendedLanguageTag(track));

    // If the language code is stored as a QuickTime 5-bit packed code there aren't enough bits for a full
    // RFC 4646 language tag so extendedLanguageTag returns null. In this case languageCode will return the
    // ISO 639-2/T language code so check it.
    if (!language)
        language = adoptCF(AVCFAssetTrackCopyLanguageCode(track));

    // Some legacy tracks have "und" as a language, treat that the same as no language at all.
    if (language && CFStringCompare(language.get(), CFSTR("und"), kCFCompareCaseInsensitive) != kCFCompareEqualTo) {
        m_languageOfPrimaryAudioTrack = language.get();
        LOG(Media, "MediaPlayerPrivateAVFoundationCF::languageOfPrimaryAudioTrack(%p) - returning language of single audio track: %s", this, m_languageOfPrimaryAudioTrack.utf8().data());
        return m_languageOfPrimaryAudioTrack;
    }

    LOG(Media, "MediaPlayerPrivateAVFoundationCF::languageOfPrimaryAudioTrack(%p) - single audio track has no language, returning emptyString()", this);
    m_languageOfPrimaryAudioTrack = emptyString();
    return m_languageOfPrimaryAudioTrack;
}

void MediaPlayerPrivateAVFoundationCF::contentsNeedsDisplay()
{
    if (m_avfWrapper)
        m_avfWrapper->setVideoLayerNeedsCommit();
}

AVFWrapper::AVFWrapper(MediaPlayerPrivateAVFoundationCF* owner)
    : m_owner(owner)
    , m_objectID(s_nextAVFWrapperObjectID++)
    , m_currentTextTrack(0)
{
    ASSERT(isMainThread());
    ASSERT(dispatch_get_main_queue() == dispatch_get_current_queue());
    LOG(Media, "AVFWrapper::AVFWrapper(%p)", this);

    m_notificationQueue = dispatch_queue_create("MediaPlayerPrivateAVFoundationCF.notificationQueue", 0);

#if HAVE(AVFOUNDATION_LOADER_DELEGATE)
    m_resourceLoaderCallbacks.version = kAVCFAssetResourceLoader_CallbacksVersion_1;
    m_resourceLoaderCallbacks.context = nullptr;
    m_resourceLoaderCallbacks.resourceLoaderShouldWaitForLoadingOfRequestedResource = AVFWrapper::resourceLoaderShouldWaitForLoadingOfRequestedResource;
#endif

    addToMap();
}

AVFWrapper::~AVFWrapper()
{
    ASSERT(isMainThread());
    ASSERT(dispatch_get_main_queue() == dispatch_get_current_queue());
    LOG(Media, "AVFWrapper::~AVFWrapper(%p %d)", this, m_objectID);

    destroyVideoLayer();
    destroyImageGenerator();

    if (m_notificationQueue)
        dispatch_release(m_notificationQueue);

    if (avAsset()) {
        AVCFAssetCancelLoading(avAsset());
        m_avAsset = 0;
    }

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP) && HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
    if (legibleOutput()) {
        if (avPlayerItem())
            AVCFPlayerItemRemoveOutput(avPlayerItem(), legibleOutput());
        m_legibleOutput = 0;
    }
#endif

    m_avPlayerItem = 0;
    m_timeObserver = 0;
    m_avPlayer = 0;
}

Lock AVFWrapper::mapLock;
HashMap<uintptr_t, AVFWrapper*>& AVFWrapper::map()
{
    static HashMap<uintptr_t, AVFWrapper*>& map = *new HashMap<uintptr_t, AVFWrapper*>;
    return map;
}

void AVFWrapper::addToMap()
{
    Locker locker { mapLock };
    
    // HashMap doesn't like a key of 0, and also make sure we aren't
    // using an object ID that's already in use.
    while (!m_objectID || (map().find(m_objectID) != map().end()))
        m_objectID = s_nextAVFWrapperObjectID++;
       
    LOG(Media, "AVFWrapper::addToMap(%p %d)", this, m_objectID);

    map().add(m_objectID, this);
}

void AVFWrapper::removeFromMap() const
{
    LOG(Media, "AVFWrapper::removeFromMap(%p %d)", this, m_objectID);

    Locker locker { mapLock };
    map().remove(m_objectID);
}

AVFWrapper* AVFWrapper::avfWrapperForCallbackContext(void* context)
{
    // Assumes caller has locked mapLock.
    HashMap<uintptr_t, AVFWrapper*>::iterator it = map().find(reinterpret_cast<uintptr_t>(context));
    if (it == map().end())
        return 0;

    return it->value;
}

void AVFWrapper::scheduleDisconnectAndDelete()
{
    // Ignore any subsequent notifications we might receive in notificationCallback().
    removeFromMap();

    dispatch_async_f(dispatchQueue(), this, disconnectAndDeleteAVFWrapper);
}

static void destroyAVFWrapper(void* context)
{
    ASSERT(isMainThread());
    AVFWrapper* avfWrapper = static_cast<AVFWrapper*>(context);
    if (!avfWrapper)
        return;

    delete avfWrapper;
}

void AVFWrapper::disconnectAndDeleteAVFWrapper(void* context)
{
    AVFWrapper* avfWrapper = static_cast<AVFWrapper*>(context);

    LOG(Media, "AVFWrapper::disconnectAndDeleteAVFWrapper(%p)", avfWrapper);

    if (avfWrapper->avPlayerItem()) {
        CFNotificationCenterRef center = CFNotificationCenterGetLocalCenter();
        CFNotificationCenterRemoveObserver(center, avfWrapper->callbackContext(), AVCFPlayerItemDidPlayToEndTimeNotification, avfWrapper->avPlayerItem());
        CFNotificationCenterRemoveObserver(center, avfWrapper->callbackContext(), AVCFPlayerItemStatusChangedNotification, avfWrapper->avPlayerItem());
        CFNotificationCenterRemoveObserver(center, avfWrapper->callbackContext(), AVCFPlayerItemTracksChangedNotification, avfWrapper->avPlayerItem());
        CFNotificationCenterRemoveObserver(center, avfWrapper->callbackContext(), AVCFPlayerItemSeekableTimeRangesChangedNotification, avfWrapper->avPlayerItem());
        CFNotificationCenterRemoveObserver(center, avfWrapper->callbackContext(), AVCFPlayerItemLoadedTimeRangesChangedNotification, avfWrapper->avPlayerItem());
        CFNotificationCenterRemoveObserver(center, avfWrapper->callbackContext(), AVCFPlayerItemPresentationSizeChangedNotification, avfWrapper->avPlayerItem());
        CFNotificationCenterRemoveObserver(center, avfWrapper->callbackContext(), AVCFPlayerItemIsPlaybackLikelyToKeepUpChangedNotification, avfWrapper->avPlayerItem());
        CFNotificationCenterRemoveObserver(center, avfWrapper->callbackContext(), AVCFPlayerItemIsPlaybackBufferEmptyChangedNotification, avfWrapper->avPlayerItem());
        CFNotificationCenterRemoveObserver(center, avfWrapper->callbackContext(), AVCFPlayerItemIsPlaybackBufferFullChangedNotification, avfWrapper->avPlayerItem());
        CFNotificationCenterRemoveObserver(center, avfWrapper->callbackContext(), AVCFPlayerItemDurationChangedNotification, avfWrapper->avPlayerItem());
        CFNotificationCenterRemoveObserver(center, avfWrapper->callbackContext(), CACFContextNeedsFlushNotification(), 0);
    }

    if (avfWrapper->avPlayer()) {
        if (avfWrapper->timeObserver())
            AVCFPlayerRemoveObserver(avfWrapper->avPlayer(), avfWrapper->timeObserver());

        CFNotificationCenterRemoveObserver(CFNotificationCenterGetLocalCenter(), avfWrapper->callbackContext(), AVCFPlayerRateChangedNotification, avfWrapper->avPlayer());
    }

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP) && HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
    AVCFPlayerItemRemoveOutput(avfWrapper->avPlayerItem(), avfWrapper->legibleOutput());
#endif

    // We must release the AVCFPlayer and other items on the same thread that created them.
    dispatch_async_f(dispatch_get_main_queue(), context, destroyAVFWrapper);
}

void AVFWrapper::createAssetForURL(const URL& url, bool inheritURI)
{
    ASSERT(!avAsset());

    RetainPtr<CFURLRef> urlRef = url.createCFURL();

    RetainPtr<CFMutableDictionaryRef> optionsRef = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    if (inheritURI)
        CFDictionarySetValue(optionsRef.get(), AVCFURLAssetInheritURIQueryComponentFromReferencingURIKey, kCFBooleanTrue);

    const int restrictions = AVAssetReferenceRestrictionForbidRemoteReferenceToLocal | AVAssetReferenceRestrictionForbidLocalReferenceToRemote;
    auto cfRestrictions = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &restrictions));

    CFDictionarySetValue(optionsRef.get(), AVCFURLAssetReferenceRestrictionsKey, cfRestrictions.get());

    m_avAsset = adoptCF(AVCFURLAssetCreateWithURLAndOptions(kCFAllocatorDefault, urlRef.get(), optionsRef.get(), m_notificationQueue));

#if HAVE(AVFOUNDATION_LOADER_DELEGATE)
    ASSERT(callbackContext());
    m_resourceLoaderCallbacks.context = callbackContext();

    AVCFAssetResourceLoaderRef resourceLoader = AVCFURLAssetGetResourceLoader(m_avAsset.get());
    AVCFAssetResourceLoaderSetCallbacks(resourceLoader, &m_resourceLoaderCallbacks, globalLoaderDelegateQueue());
#endif
}

void AVFWrapper::createPlayer(IDirect3DDevice9* d3dDevice)
{
    ASSERT(isMainThread());
    ASSERT(dispatch_get_main_queue() == dispatch_get_current_queue());
    ASSERT(avPlayerItem());

    if (avPlayer())
        return;

    RetainPtr<CFMutableDictionaryRef> optionsRef = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    if (d3dDevice) {
        // QI for an IDirect3DDevice9Ex interface, it is required to do HW video decoding.
        COMPtr<IDirect3DDevice9Ex> d3dEx(Query, d3dDevice);
        m_d3dDevice = d3dEx;
    } else
        m_d3dDevice = 0;

    if (m_d3dDevice && AVCFPlayerEnableHardwareAcceleratedVideoDecoderKey)
        CFDictionarySetValue(optionsRef.get(), AVCFPlayerEnableHardwareAcceleratedVideoDecoderKey, kCFBooleanTrue);

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP) && HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
    CFDictionarySetValue(optionsRef.get(), AVCFPlayerAppliesMediaSelectionCriteriaAutomaticallyKey, kCFBooleanTrue);
#endif

    // FIXME: We need a way to create a AVPlayer without an AVPlayerItem, see <rdar://problem/9877730>.
    m_avPlayer = adoptCF(AVCFPlayerCreateWithPlayerItemAndOptions(kCFAllocatorDefault, avPlayerItem(), optionsRef.get(), m_notificationQueue));
#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP) && HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
    AVCFPlayerSetClosedCaptionDisplayEnabled(m_avPlayer.get(), FALSE);
#endif

    if (m_d3dDevice && AVCFPlayerSetDirect3DDevicePtr())
        AVCFPlayerSetDirect3DDevicePtr()(m_avPlayer.get(), m_d3dDevice.get());

    CFNotificationCenterRef center = CFNotificationCenterGetLocalCenter();
    ASSERT(center);

    CFNotificationCenterAddObserver(center, callbackContext(), notificationCallback, AVCFPlayerRateChangedNotification, m_avPlayer.get(), CFNotificationSuspensionBehaviorDeliverImmediately);

    // Add a time observer, ask to be called infrequently because we don't really want periodic callbacks but
    // our observer will also be called whenever a seek happens.
    const double veryLongInterval = 60*60*60*24*30;
    m_timeObserver = adoptCF(AVCFPlayerCreatePeriodicTimeObserverForInterval(m_avPlayer.get(), CMTimeMake(veryLongInterval, 10), m_notificationQueue, &periodicTimeObserverCallback, callbackContext()));
}

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP) && HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
static RetainPtr<CFArrayRef> createLegibleOutputSubtypes()
{
    int webVTTInt = 'wvtt'; // kCMSubtitleFormatType_WebVTT;
    RetainPtr<CFNumberRef> webVTTNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &webVTTInt));
    CFTypeRef formatTypes[] = { webVTTNumber.get() };
    return adoptCF(CFArrayCreate(0, formatTypes, WTF_ARRAY_LENGTH(formatTypes), &kCFTypeArrayCallBacks));
}
#endif

void AVFWrapper::createPlayerItem()
{
    ASSERT(isMainThread());
    ASSERT(dispatch_get_main_queue() == dispatch_get_current_queue());
    ASSERT(avAsset());

    if (avPlayerItem())
        return;

    // Create the player item so we begin loading media data.
    m_avPlayerItem = adoptCF(AVCFPlayerItemCreateWithAsset(kCFAllocatorDefault, avAsset(), m_notificationQueue));

    CFNotificationCenterRef center = CFNotificationCenterGetLocalCenter();
    ASSERT(center);

    CFNotificationCenterAddObserver(center, callbackContext(), notificationCallback, AVCFPlayerItemDidPlayToEndTimeNotification, m_avPlayerItem.get(), CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(center, callbackContext(), notificationCallback, AVCFPlayerItemStatusChangedNotification, m_avPlayerItem.get(), CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(center, callbackContext(), notificationCallback, AVCFPlayerItemTracksChangedNotification, m_avPlayerItem.get(), CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(center, callbackContext(), notificationCallback, AVCFPlayerItemSeekableTimeRangesChangedNotification, m_avPlayerItem.get(), CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(center, callbackContext(), notificationCallback, AVCFPlayerItemLoadedTimeRangesChangedNotification, m_avPlayerItem.get(), CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(center, callbackContext(), notificationCallback, AVCFPlayerItemPresentationSizeChangedNotification, m_avPlayerItem.get(), CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(center, callbackContext(), notificationCallback, AVCFPlayerItemIsPlaybackLikelyToKeepUpChangedNotification, m_avPlayerItem.get(), CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(center, callbackContext(), notificationCallback, AVCFPlayerItemIsPlaybackBufferEmptyChangedNotification, m_avPlayerItem.get(), CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(center, callbackContext(), notificationCallback, AVCFPlayerItemIsPlaybackBufferFullChangedNotification, m_avPlayerItem.get(), CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(center, callbackContext(), notificationCallback, AVCFPlayerItemDurationChangedNotification, m_avPlayerItem.get(), CFNotificationSuspensionBehaviorDeliverImmediately);
    // FIXME: Are there other legible output things we need to register for? asset and hasEnabledAudio are not exposed by AVCF

    CFNotificationCenterAddObserver(center, callbackContext(), notificationCallback, CACFContextNeedsFlushNotification(), 0, CFNotificationSuspensionBehaviorDeliverImmediately);

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP) && HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
    const CFTimeInterval legibleOutputAdvanceInterval = 2;

    m_legibleOutput = adoptCF(AVCFPlayerItemLegibleOutputCreateWithMediaSubtypesForNativeRepresentation(kCFAllocatorDefault, createLegibleOutputSubtypes().get()));
    AVCFPlayerItemOutputSetSuppressPlayerRendering(m_legibleOutput.get(), TRUE);

    AVCFPlayerItemLegibleOutputCallbacks callbackInfo;
#if HAVE(AVCFPLAYERITEM_CALLBACK_VERSION_2)
    callbackInfo.version = kAVCFPlayerItemLegibleOutput_CallbacksVersion_2;
#else
    callbackInfo.version = kAVCFPlayerItemLegibleOutput_CallbacksVersion_1;
#endif
    ASSERT(callbackContext());
    callbackInfo.context = callbackContext();
    callbackInfo.legibleOutputCallback = AVFWrapper::legibleOutputCallback;

    AVCFPlayerItemLegibleOutputSetCallbacks(m_legibleOutput.get(), &callbackInfo, dispatchQueue());
    AVCFPlayerItemLegibleOutputSetAdvanceIntervalForCallbackInvocation(m_legibleOutput.get(), legibleOutputAdvanceInterval);
    AVCFPlayerItemLegibleOutputSetTextStylingResolution(m_legibleOutput.get(), AVCFPlayerItemLegibleOutputTextStylingResolutionSourceAndRulesOnly);
    AVCFPlayerItemAddOutput(m_avPlayerItem.get(), m_legibleOutput.get());
#endif
}

void AVFWrapper::periodicTimeObserverCallback(AVCFPlayerRef, CMTime cmTime, void* context)
{
    Locker locker { mapLock };
    AVFWrapper* self = avfWrapperForCallbackContext(context);
    if (!self) {
        LOG(Media, "AVFWrapper::periodicTimeObserverCallback invoked for deleted AVFWrapper %d", reinterpret_cast<uintptr_t>(context));
        return;
    }

    double time = std::max(0.0, CMTimeGetSeconds(cmTime)); // Clamp to zero, negative values are sometimes reported.
    self->m_owner->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::PlayerTimeChanged, time);
}

struct NotificationCallbackData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    RetainPtr<CFStringRef> m_propertyName;
    void* m_context;

    NotificationCallbackData(CFStringRef propertyName, void* context)
        : m_propertyName(propertyName), m_context(context)
    {
    }
};

void AVFWrapper::processNotification(void* context)
{
    ASSERT(isMainThread());
    ASSERT(context);

    if (!context)
        return;

    std::unique_ptr<NotificationCallbackData> notificationData { static_cast<NotificationCallbackData*>(context) };

    Locker locker { mapLock };
    AVFWrapper* self = avfWrapperForCallbackContext(notificationData->m_context);
    if (!self) {
        LOG(Media, "AVFWrapper::processNotification invoked for deleted AVFWrapper %d", reinterpret_cast<uintptr_t>(context));
        return;
    }

    CFStringRef propertyName = notificationData->m_propertyName.get();

    if (CFEqual(propertyName, AVCFPlayerItemDidPlayToEndTimeNotification))
        self->m_owner->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::ItemDidPlayToEndTime);
    else if (CFEqual(propertyName, AVCFPlayerItemTracksChangedNotification))
        self->m_owner->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::ItemTracksChanged);
    else if (CFEqual(propertyName, AVCFPlayerItemStatusChangedNotification)) {
        AVCFURLAssetRef asset = AVCFPlayerItemGetAsset(self->avPlayerItem());
        if (asset)
            self->setAsset(asset);
        self->m_owner->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::ItemStatusChanged);
    } else if (CFEqual(propertyName, AVCFPlayerItemSeekableTimeRangesChangedNotification))
        self->m_owner->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::ItemSeekableTimeRangesChanged);
    else if (CFEqual(propertyName, AVCFPlayerItemLoadedTimeRangesChangedNotification))
        self->m_owner->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::ItemLoadedTimeRangesChanged);
    else if (CFEqual(propertyName, AVCFPlayerItemPresentationSizeChangedNotification))
        self->m_owner->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::ItemPresentationSizeChanged);
    else if (CFEqual(propertyName, AVCFPlayerItemIsPlaybackLikelyToKeepUpChangedNotification))
        self->m_owner->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::ItemIsPlaybackLikelyToKeepUpChanged);
    else if (CFEqual(propertyName, AVCFPlayerItemIsPlaybackBufferEmptyChangedNotification))
        self->m_owner->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::ItemIsPlaybackBufferEmptyChanged);
    else if (CFEqual(propertyName, AVCFPlayerItemIsPlaybackBufferFullChangedNotification))
        self->m_owner->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::ItemIsPlaybackBufferFullChanged);
    else if (CFEqual(propertyName, AVCFPlayerRateChangedNotification))
        self->m_owner->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::PlayerRateChanged);
    else if (CFEqual(propertyName, CACFContextNeedsFlushNotification()))
        self->m_owner->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::ContentsNeedsDisplay);
    else if (CFEqual(propertyName, AVCFPlayerItemDurationChangedNotification))
        self->m_owner->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::DurationChanged);
    else
        ASSERT_NOT_REACHED();
}

void AVFWrapper::notificationCallback(CFNotificationCenterRef, void* observer, CFStringRef propertyName, const void* object, CFDictionaryRef)
{
#if !LOG_DISABLED
    char notificationName[256];
    CFStringGetCString(propertyName, notificationName, sizeof(notificationName), kCFStringEncodingASCII);
    LOG(Media, "AVFWrapper::notificationCallback(if=%d) %s", reinterpret_cast<uintptr_t>(observer), notificationName);
#endif

    auto notificationData = makeUnique<NotificationCallbackData>(propertyName, observer);

    dispatch_async_f(dispatch_get_main_queue(), notificationData.release(), processNotification);
}

void AVFWrapper::loadPlayableCompletionCallback(AVCFAssetRef, void* context)
{
    Locker locker { mapLock };
    AVFWrapper* self = avfWrapperForCallbackContext(context);
    if (!self) {
        LOG(Media, "AVFWrapper::loadPlayableCompletionCallback invoked for deleted AVFWrapper %d", reinterpret_cast<uintptr_t>(context));
        return;
    }

    LOG(Media, "AVFWrapper::loadPlayableCompletionCallback(%p)", self);
    self->m_owner->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::AssetPlayabilityKnown);
}

void AVFWrapper::checkPlayability()
{
    LOG(Media, "AVFWrapper::checkPlayability(%p)", this);

    static auto propertyKeyName = makeNeverDestroyed([] {
        const void* keyNames[] = {
            AVCFAssetPropertyPlayable
        };
        return adoptCF(CFArrayCreate(0, keyNames, std::size(keyNames), &kCFTypeArrayCallBacks));
    }());

    AVCFAssetLoadValuesAsynchronouslyForProperties(avAsset(), propertyKeyName.get().get(), loadPlayableCompletionCallback, callbackContext());
}

void AVFWrapper::loadMetadataCompletionCallback(AVCFAssetRef, void* context)
{
    Locker locker { mapLock };
    AVFWrapper* self = avfWrapperForCallbackContext(context);
    if (!self) {
        LOG(Media, "AVFWrapper::loadMetadataCompletionCallback invoked for deleted AVFWrapper %d", reinterpret_cast<uintptr_t>(context));
        return;
    }

    LOG(Media, "AVFWrapper::loadMetadataCompletionCallback(%p)", self);
    self->m_owner->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::AssetMetadataLoaded);
}

void AVFWrapper::beginLoadingMetadata()
{
    ASSERT(avAsset());
    LOG(Media, "AVFWrapper::beginLoadingMetadata(%p) - requesting metadata loading", this);
    AVCFAssetLoadValuesAsynchronouslyForProperties(avAsset(), metadataKeyNames(), loadMetadataCompletionCallback, callbackContext());
}

void AVFWrapper::seekCompletedCallback(AVCFPlayerItemRef, Boolean finished, void* context)
{
    Locker locker { mapLock };
    AVFWrapper* self = avfWrapperForCallbackContext(context);
    if (!self) {
        LOG(Media, "AVFWrapper::seekCompletedCallback invoked for deleted AVFWrapper %d", reinterpret_cast<uintptr_t>(context));
        return;
    }

    LOG(Media, "AVFWrapper::seekCompletedCallback(%p)", self);
    self->m_owner->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::SeekCompleted, static_cast<bool>(finished));
}

void AVFWrapper::seekToTime(const MediaTime& time, const MediaTime& negativeTolerance, const MediaTime& positiveTolerance)
{
    ASSERT(avPlayerItem());
    CMTime cmTime = PAL::toCMTime(time);
    CMTime cmBefore = PAL::toCMTime(negativeTolerance);
    CMTime cmAfter = PAL::toCMTime(positiveTolerance);
    AVCFPlayerItemSeekToTimeWithToleranceAndCompletionCallback(avPlayerItem(), cmTime, cmBefore, cmAfter, &seekCompletedCallback, callbackContext());
}

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP) && HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
struct LegibleOutputData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    RetainPtr<CFArrayRef> m_attributedStrings;
    RetainPtr<CFArrayRef> m_samples;
    MediaTime m_time;
    void* m_context;

    LegibleOutputData(CFArrayRef strings, CFArrayRef samples, const MediaTime &time, void* context)
        : m_attributedStrings(strings), m_samples(samples), m_time(time), m_context(context)
    {
    }
};

void AVFWrapper::processCue(void* context)
{
    ASSERT(isMainThread());
    ASSERT(context);

    if (!context)
        return;

    std::unique_ptr<LegibleOutputData> legibleOutputData(reinterpret_cast<LegibleOutputData*>(context));

    Locker locker { mapLock };
    AVFWrapper* self = avfWrapperForCallbackContext(legibleOutputData->m_context);
    if (!self) {
        LOG(Media, "AVFWrapper::processCue invoked for deleted AVFWrapper %d", reinterpret_cast<uintptr_t>(context));
        return;
    }

    if (!self->m_currentTextTrack)
        return;

    self->m_currentTextTrack->processCue(legibleOutputData->m_attributedStrings.get(), legibleOutputData->m_samples.get(), legibleOutputData->m_time);
}

void AVFWrapper::legibleOutputCallback(void* context, AVCFPlayerItemLegibleOutputRef legibleOutput, CFArrayRef attributedStrings, CFArrayRef nativeSampleBuffers, CMTime itemTime)
{
    ASSERT(!isMainThread());
    Locker locker { mapLock };
    AVFWrapper* self = avfWrapperForCallbackContext(context);
    if (!self) {
        LOG(Media, "AVFWrapper::legibleOutputCallback invoked for deleted AVFWrapper %d", reinterpret_cast<uintptr_t>(context));
        return;
    }

    LOG(Media, "AVFWrapper::legibleOutputCallback(%p)", self);

    ASSERT(legibleOutput == self->m_legibleOutput);

    auto legibleOutputData = makeUnique<LegibleOutputData>(attributedStrings, nativeSampleBuffers, PAL::toMediaTime(itemTime), context);

    dispatch_async_f(dispatch_get_main_queue(), legibleOutputData.release(), processCue);
}
#endif

#if HAVE(AVFOUNDATION_LOADER_DELEGATE)
struct LoadRequestData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    RetainPtr<AVCFAssetResourceLoadingRequestRef> m_request;
    void* m_context;

    LoadRequestData(AVCFAssetResourceLoadingRequestRef request, void* context)
        : m_request(request), m_context(context)
    {
    }
};

void AVFWrapper::processShouldWaitForLoadingOfResource(void* context)
{
    ASSERT(dispatch_get_main_queue() == dispatch_get_current_queue());
    ASSERT(context);

    if (!context)
        return;

    std::unique_ptr<LoadRequestData> loadRequestData(reinterpret_cast<LoadRequestData*>(context));

    Locker locker { mapLock };
    AVFWrapper* self = avfWrapperForCallbackContext(loadRequestData->m_context);
    if (!self) {
        LOG(Media, "AVFWrapper::processShouldWaitForLoadingOfResource invoked for deleted AVFWrapper %d", reinterpret_cast<uintptr_t>(context));
        RetainPtr<CFErrorRef> error = adoptCF(CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainCFNetwork, kCFURLErrorUnknown, nullptr));
        AVCFAssetResourceLoadingRequestFinishLoadingWithError(loadRequestData->m_request.get(), error.get());
        return;
    }

    if (!self->shouldWaitForLoadingOfResource(loadRequestData->m_request.get())) {
        RetainPtr<CFErrorRef> error = adoptCF(CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainCFNetwork, kCFURLErrorUnknown, nullptr));
        AVCFAssetResourceLoadingRequestFinishLoadingWithError(loadRequestData->m_request.get(), error.get());
    }
}

bool AVFWrapper::shouldWaitForLoadingOfResource(AVCFAssetResourceLoadingRequestRef avRequest)
{
#if HAVE(AVFOUNDATION_LOADER_DELEGATE) && ENABLE(LEGACY_ENCRYPTED_MEDIA)
    RetainPtr<CFURLRequestRef> urlRequest = AVCFAssetResourceLoadingRequestGetURLRequest(avRequest);
    RetainPtr<CFURLRef> requestURL = CFURLRequestGetURL(urlRequest.get());
    RetainPtr<CFStringRef> schemeRef = adoptCF(CFURLCopyScheme(requestURL.get()));
    String scheme = schemeRef.get();

    if (scheme == "skd") {
        RetainPtr<CFURLRef> absoluteURL = adoptCF(CFURLCopyAbsoluteURL(requestURL.get()));
        RetainPtr<CFStringRef> keyURIRef = CFURLGetString(absoluteURL.get());
        String keyURI = keyURIRef.get();

        // Create an initData with the following layout:
        // [4 bytes: keyURI size], [keyURI size bytes: keyURI]
        unsigned keyURISize = keyURI.length() * sizeof(UChar);
        auto initDataBuffer = ArrayBuffer::create(4 + keyURISize, 1);
        auto initDataView = JSC::DataView::create(initDataBuffer.copyRef(), 0, initDataBuffer->byteLength());
        initDataView->set<uint32_t>(0, keyURISize, true);

        auto keyURIArray = Uint16Array::create(initDataBuffer.copyRef(), 4, keyURI.length());
        keyURIArray->setRange(reinterpret_cast<const uint16_t*>(StringView(keyURI).upconvertedCharacters().get()), keyURI.length() / sizeof(unsigned char), 0);

        unsigned byteLength = initDataBuffer->byteLength();
        auto initData = Uint8Array::create(WTFMove(initDataBuffer), 0, byteLength);
        m_owner->player()->keyNeeded(initData.ptr());
        setRequestForKey(keyURI, avRequest);
        return true;
    }
#endif

    auto resourceLoader = WebCoreAVCFResourceLoader::create(m_owner, avRequest);
    m_owner->m_resourceLoaderMap.add(avRequest, resourceLoader.copyRef());
    resourceLoader->startLoading();
    return true;
}

Boolean AVFWrapper::resourceLoaderShouldWaitForLoadingOfRequestedResource(AVCFAssetResourceLoaderRef resourceLoader, AVCFAssetResourceLoadingRequestRef loadingRequest, void *context)
{
    ASSERT(dispatch_get_main_queue() != dispatch_get_current_queue());
    Locker locker { mapLock };
    AVFWrapper* self = avfWrapperForCallbackContext(context);
    if (!self) {
        LOG(Media, "AVFWrapper::resourceLoaderShouldWaitForLoadingOfRequestedResource invoked for deleted AVFWrapper %d", reinterpret_cast<uintptr_t>(context));
        return false;
    }

    LOG(Media, "AVFWrapper::resourceLoaderShouldWaitForLoadingOfRequestedResource(%p)", self);

    auto loadRequestData = makeUnique<LoadRequestData>(loadingRequest, context);

    dispatch_async_f(dispatch_get_main_queue(), loadRequestData.release(), processShouldWaitForLoadingOfResource);

    return true;
}
#endif

void AVFWrapper::setAsset(AVCFURLAssetRef asset)
{
    if (asset == avAsset())
        return;

    AVCFAssetCancelLoading(avAsset());
    m_avAsset = asset;
}

PlatformLayer* AVFWrapper::platformLayer()
{
    ASSERT(isMainThread());
    if (m_videoLayerWrapper)
        return m_videoLayerWrapper->platformLayer();

    if (!videoLayer())
        return 0;

    // Create a PlatformCALayer so we can resize the video layer to match the element size.
    m_layerClient = makeUnique<LayerClient>(this);
    if (!m_layerClient)
        return 0;

    m_videoLayerWrapper = PlatformCALayerWin::create(PlatformCALayer::LayerTypeLayer, m_layerClient.get());
    if (!m_videoLayerWrapper)
        return 0;

    m_caVideoLayer = adoptCF(AVCFPlayerLayerCopyCACFLayer(m_avCFVideoLayer.get()));

    CACFLayerInsertSublayer(m_videoLayerWrapper->platformLayer(), m_caVideoLayer.get(), 0);
    m_videoLayerWrapper->setAnchorPoint(FloatPoint3D());
    m_videoLayerWrapper->setNeedsLayout();
    updateVideoLayerGravity();

    return m_videoLayerWrapper->platformLayer();
}

void AVFWrapper::createAVCFVideoLayer()
{
    ASSERT(isMainThread());
    ASSERT(dispatch_get_main_queue() == dispatch_get_current_queue());
    if (!avPlayer() || m_avCFVideoLayer)
        return;

    // The layer will get hooked up via RenderLayerBacking::updateConfiguration().
    m_avCFVideoLayer = adoptCF(AVCFPlayerLayerCreateWithAVCFPlayer(kCFAllocatorDefault, avPlayer(), m_notificationQueue));
    LOG(Media, "AVFWrapper::createAVCFVideoLayer(%p) - returning %p", this, videoLayer());
}

void AVFWrapper::destroyVideoLayer()
{
    ASSERT(isMainThread());
    ASSERT(dispatch_get_main_queue() == dispatch_get_current_queue());
    LOG(Media, "AVFWrapper::destroyVideoLayer(%p)", this);
    m_layerClient = nullptr;
    m_caVideoLayer = nullptr;
    m_videoLayerWrapper = nullptr;
    if (!m_avCFVideoLayer.get())
        return;

    AVCFPlayerLayerSetPlayer((AVCFPlayerLayerRef)m_avCFVideoLayer.get(), nullptr);
    m_avCFVideoLayer = nullptr;
}

void AVFWrapper::setVideoLayerNeedsCommit()
{
    if (m_videoLayerWrapper)
        m_videoLayerWrapper->setNeedsCommit();
}

void AVFWrapper::setVideoLayerHidden(bool value)
{
    if (m_videoLayerWrapper)
        m_videoLayerWrapper->setHidden(value);
}

void AVFWrapper::createImageGenerator()
{
    ASSERT(isMainThread());
    ASSERT(dispatch_get_main_queue() == dispatch_get_current_queue());
    if (!avAsset() || m_imageGenerator)
        return;

    m_imageGenerator = adoptCF(AVCFAssetImageGeneratorCreateWithAsset(kCFAllocatorDefault, avAsset()));

    AVCFAssetImageGeneratorSetApertureMode(m_imageGenerator.get(), AVCFAssetImageGeneratorApertureModeCleanAperture);
    AVCFAssetImageGeneratorSetRequestedTimeToleranceBefore(m_imageGenerator.get(), kCMTimeZero);
    AVCFAssetImageGeneratorSetRequestedTimeToleranceAfter(m_imageGenerator.get(), kCMTimeZero);
    AVCFAssetImageGeneratorSetAppliesPreferredTrackTransform(m_imageGenerator.get(), true);

    LOG(Media, "AVFWrapper::createImageGenerator(%p) - returning %p", this, m_imageGenerator.get());
}

void AVFWrapper::destroyImageGenerator()
{
    ASSERT(isMainThread());
    ASSERT(dispatch_get_main_queue() == dispatch_get_current_queue());
    LOG(Media, "AVFWrapper::destroyImageGenerator(%p)", this);
    m_imageGenerator = 0;
}

RetainPtr<CGImageRef> AVFWrapper::createImageForTimeInRect(const MediaTime& time, const FloatRect& rect)
{
    if (!m_imageGenerator)
        return 0;

#if !LOG_DISABLED
    MonotonicTime start = MonotonicTime::now();
#endif

    AVCFAssetImageGeneratorSetMaximumSize(m_imageGenerator.get(), CGSize(rect.size()));
    RetainPtr<CGImageRef> rawimage = adoptCF(AVCFAssetImageGeneratorCopyCGImageAtTime(m_imageGenerator.get(), PAL::toCMTime(time), 0, 0));
    RetainPtr<CGImageRef> image = adoptCF(CGImageCreateCopyWithColorSpace(rawimage.get(), adoptCF(CGColorSpaceCreateDeviceRGB()).get()));

#if !LOG_DISABLED
    Seconds duration = MonotonicTime::now() - start;
    LOG(Media, "AVFWrapper::createImageForTimeInRect(%p) - creating image took %.4f", this, narrowPrecisionToFloat(duration.seconds()));
#endif

    return image;
}

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
AVCFMediaSelectionGroupRef AVFWrapper::safeMediaSelectionGroupForLegibleMedia() const
{
    if (!avAsset())
        return 0;

    if (AVCFAssetGetStatusOfValueForProperty(avAsset(), AVCFAssetPropertyAvailableMediaCharacteristicsWithMediaSelectionOptions, 0) != AVCFPropertyValueStatusLoaded)
        return 0;

    return AVCFAssetGetSelectionGroupForMediaCharacteristic(avAsset(), AVCFMediaCharacteristicLegible);
}
#endif

void AVFWrapper::updateVideoLayerGravity()
{
    // We should call AVCFPlayerLayerSetVideoGravity() here, but it is not yet implemented.
    // FIXME: <rdar://problem/14884340>
}

#if HAVE(AVFOUNDATION_LOADER_DELEGATE) && ENABLE(LEGACY_ENCRYPTED_MEDIA)
void AVFWrapper::setRequestForKey(const String& keyURI, AVCFAssetResourceLoadingRequestRef avRequest)
{
    auto requestsIterator = m_keyURIToRequestMap.find(keyURI);
    if (requestsIterator != m_keyURIToRequestMap.end()) {
        requestsIterator->value.append(avRequest);
        return;
    }

    Vector<RetainPtr<AVCFAssetResourceLoadingRequestRef>> requests;
    requests.append(avRequest);
    m_keyURIToRequestMap.set(keyURI, requests);
}

RetainPtr<AVCFAssetResourceLoadingRequestRef> AVFWrapper::takeRequestForKeyURI(const String& keyURI)
{
    auto requestsIterator = m_keyURIToRequestMap.find(keyURI);
    if (requestsIterator == m_keyURIToRequestMap.end())
        return RetainPtr<AVCFAssetResourceLoadingRequestRef>();

    auto request = requestsIterator->value.takeLast();
    if (requestsIterator->value.isEmpty())
        m_keyURIToRequestMap.take(keyURI);

    return request;
}
#endif

void LayerClient::platformCALayerLayoutSublayersOfLayer(PlatformCALayer* wrapperLayer)
{
    ASSERT(isMainThread());
    ASSERT(m_parent);
    ASSERT(m_parent->videoLayerWrapper() == wrapperLayer->platformLayer());

    CGRect bounds = wrapperLayer->bounds();
    CGPoint anchor = CACFLayerGetAnchorPoint(m_parent->caVideoLayer());
    FloatPoint position(bounds.size.width * anchor.x, bounds.size.height * anchor.y); 

    CACFLayerSetPosition(m_parent->caVideoLayer(), position);
    CACFLayerSetBounds(m_parent->caVideoLayer(), bounds);

    AVCFPlayerLayerSetFrame(m_parent->videoLayer(), CGRectMake(0, 0, bounds.size.width, bounds.size.height));
}

} // namespace WebCore

#else
// AVFoundation should always be enabled for Apple production builds.
#if __PRODUCTION__ && !USE(AVFOUNDATION)
#error AVFoundation is not enabled!
#endif // __PRODUCTION__ && !USE(AVFOUNDATION)
#endif // USE(AVFOUNDATION)
#endif // PLATFORM(WIN) && ENABLE(VIDEO)
