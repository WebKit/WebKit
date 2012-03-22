/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "config.h"

#if PLATFORM(WIN)&& ENABLE(VIDEO) 

#if USE(AVFOUNDATION)

#include "MediaPlayerPrivateAVFoundationCF.h"

#include "ApplicationCacheResource.h"
#include "FloatConversion.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "KURL.h"
#include "Logging.h"
#include "PlatformCALayer.h"
#include "SoftLinking.h"
#include "TimeRanges.h"

#include <AVFoundationCF/AVCFPlayerLayer.h>
#include <AVFoundationCF/AVFoundationCF.h>
#include <CoreMedia/CoreMedia.h>

#include <delayimp.h>
#include <dispatch/dispatch.h>
#include <wtf/HashMap.h>
#include <wtf/Threading.h>
#include <wtf/UnusedParam.h>

// The softlink header files must be included after the AVCF and CoreMedia header files.
#include "AVFoundationCFSoftLinking.h"
#include "CoreMediaSoftLinking.h"

// We don't bother softlinking against libdispatch since it's already been loaded by AAS.
#pragma comment(lib, "libdispatch.lib")

using namespace std;

namespace WebCore {

class LayerClient;

class AVFWrapper {
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
    RetainPtr<CGImageRef> createImageForTimeInRect(float, const IntRect&);

    void createAssetForURL(const String& url);
    void setAsset(AVCFURLAssetRef);
    
    void createPlayer();
    void createPlayerItem();
    
    void checkPlayability();
    void beginLoadingMetadata();
    
    void seekToTime(float);

    static void loadMetadataCompletionCallback(AVCFAssetRef, void*);
    static void loadPlayableCompletionCallback(AVCFAssetRef, void*);
    static void periodicTimeObserverCallback(AVCFPlayerRef, CMTime, void*);
    static void seekCompletedCallback(AVCFPlayerItemRef, Boolean, void*);
    static void notificationCallback(CFNotificationCenterRef, void*, CFStringRef, const void*, CFDictionaryRef);

    inline AVCFPlayerLayerRef videoLayer() const { return (AVCFPlayerLayerRef)m_avCFVideoLayer.get(); }
    inline AVCFPlayerRef avPlayer() const { return (AVCFPlayerRef)m_avPlayer.get(); }
    inline AVCFURLAssetRef avAsset() const { return (AVCFURLAssetRef)m_avAsset.get(); }
    inline AVCFPlayerItemRef avPlayerItem() const { return (AVCFPlayerItemRef)m_avPlayerItem.get(); }
    inline AVCFPlayerObserverRef timeObserver() const { return (AVCFPlayerObserverRef)m_timeObserver.get(); }
    inline AVCFAssetImageGeneratorRef imageGenerator() const { return m_imageGenerator.get(); }
    inline dispatch_queue_t dispatchQueue() const { return m_notificationQueue; }

private:
    inline void* callbackContext() const { return reinterpret_cast<void*>(m_objectID); }

    static Mutex& mapLock();
    static HashMap<uintptr_t, AVFWrapper*>& map();
    static AVFWrapper* avfWrapperForCallbackContext(void*);
    void addToMap();
    void removeFromMap() const;

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
    dispatch_queue_t m_notificationQueue;

    mutable RetainPtr<CACFLayerRef> m_caVideoLayer;
    RefPtr<PlatformCALayer> m_videoLayerWrapper;

    OwnPtr<LayerClient> m_layerClient;
};

uintptr_t AVFWrapper::s_nextAVFWrapperObjectID;

class LayerClient : public PlatformCALayerClient {
public:
    LayerClient(AVFWrapper* parent) : m_parent(parent) { }
    virtual ~LayerClient() { m_parent = 0; }

private:
    virtual void platformCALayerLayoutSublayersOfLayer(PlatformCALayer*);
    virtual bool platformCALayerRespondsToLayoutChanges() const { return true; }

    virtual void platformCALayerAnimationStarted(CFTimeInterval beginTime) { }
    virtual GraphicsLayer::CompositingCoordinatesOrientation platformCALayerContentsOrientation() const { return GraphicsLayer::CompositingCoordinatesBottomUp; }
    virtual void platformCALayerPaintContents(GraphicsContext&, const IntRect& inClip) { }
    virtual bool platformCALayerShowDebugBorders() const { return false; }
    virtual bool platformCALayerShowRepaintCounter() const { return false; }
    virtual int platformCALayerIncrementRepaintCount() { return 0; }

    virtual bool platformCALayerContentsOpaque() const { return false; }
    virtual bool platformCALayerDrawsContent() const { return false; }
    virtual void platformCALayerLayerDidDisplay(PlatformLayer*) { }
    virtual void platformCALayerDidCreateTiles(const Vector<FloatRect>&) { }
    virtual float platformCALayerDeviceScaleFactor() { return 1; }

    AVFWrapper* m_parent;
};

#if !LOG_DISABLED
static const char* boolString(bool val)
{
    return val ? "true" : "false";
}
#endif

static CFArrayRef createMetadataKeyNames()
{
    static const CFStringRef keyNames[] = {
        AVCFAssetPropertyDuration,
        AVCFAssetPropertyNaturalSize,
        AVCFAssetPropertyPreferredTransform,
        AVCFAssetPropertyPreferredRate,
        AVCFAssetPropertyPlayable,
        AVCFAssetPropertyTracks 
    };
    
    return CFArrayCreate(0, (const void**)keyNames, sizeof(keyNames) / sizeof(keyNames[0]), &kCFTypeArrayCallBacks);
}

static CFArrayRef metadataKeyNames()
{
    static CFArrayRef keys = createMetadataKeyNames();
    return keys;
}

// FIXME: It would be better if AVCFTimedMetadataGroup.h exported this key.
static CFStringRef CMTimeRangeStartKey()
{
    DEFINE_STATIC_LOCAL(CFStringRef, key, (CFSTR("start")));
    return key;
}

// FIXME: It would be better if AVCFTimedMetadataGroup.h exported this key.
static CFStringRef CMTimeRangeDurationKey()
{
    DEFINE_STATIC_LOCAL(CFStringRef, key, (CFSTR("duration")));
    return key;
}

// FIXME: It would be better if AVCF exported this notification name.
static CFStringRef CACFContextNeedsFlushNotification()
{
    DEFINE_STATIC_LOCAL(CFStringRef, name, (CFSTR("kCACFContextNeedsFlushNotification")));
    return name;
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

PassOwnPtr<MediaPlayerPrivateInterface> MediaPlayerPrivateAVFoundationCF::create(MediaPlayer* player) 
{ 
    return adoptPtr(new MediaPlayerPrivateAVFoundationCF(player));
}

void MediaPlayerPrivateAVFoundationCF::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (isAvailable())
        registrar(create, getSupportedTypes, supportsType, 0, 0, 0);
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
    cancelLoad();
}

void MediaPlayerPrivateAVFoundationCF::cancelLoad()
{
    LOG(Media, "MediaPlayerPrivateAVFoundationCF::cancelLoad(%p)", this);

    // Do nothing when our cancellation of pending loading calls its completion handler
    setDelayCallbacks(true);
    setIgnoreLoadStateChanges(true);

    tearDownVideoRendering();

    if (m_avfWrapper) {
        // The AVCF objects have to be destroyed on the same dispatch queue used for notifications, so schedule a call to 
        // disconnectAndDeleteAVFWrapper on that queue. 
        m_avfWrapper->scheduleDisconnectAndDelete();
        m_avfWrapper = 0;
    }

    setIgnoreLoadStateChanges(false);
    setDelayCallbacks(false);
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

    if (imageGenerator(m_avfWrapper))
        return;

    if (m_avfWrapper)
        m_avfWrapper->createImageGenerator();
}

void MediaPlayerPrivateAVFoundationCF::destroyContextVideoRenderer()
{
    if (m_avfWrapper)
        m_avfWrapper->destroyImageGenerator();
}

void MediaPlayerPrivateAVFoundationCF::createVideoLayer()
{
    ASSERT(supportsAcceleratedRendering());

    if (m_avfWrapper)
        m_avfWrapper->createAVCFVideoLayer();
}

void MediaPlayerPrivateAVFoundationCF::destroyVideoLayer()
{
    LOG(Media, "MediaPlayerPrivateAVFoundationCF::destroyVideoLayer(%p) - destroying %p", this, videoLayer(m_avfWrapper));
    if (m_avfWrapper)
        m_avfWrapper->destroyVideoLayer();
}

bool MediaPlayerPrivateAVFoundationCF::hasAvailableVideoFrame() const
{
    return (m_videoFrameHasDrawn || (videoLayer(m_avfWrapper) && AVCFPlayerLayerIsReadyForDisplay(videoLayer(m_avfWrapper))));
}

void MediaPlayerPrivateAVFoundationCF::createAVPlayer()
{
    ASSERT(m_avfWrapper);
    
    setDelayCallbacks(true);
    m_avfWrapper->createPlayer();
    setDelayCallbacks(false);
}

void MediaPlayerPrivateAVFoundationCF::createAVPlayerItem()
{
    ASSERT(m_avfWrapper);
    
    setDelayCallbacks(true);
    m_avfWrapper->createPlayerItem();
    setDelayCallbacks(false);
}

void MediaPlayerPrivateAVFoundationCF::createAVAssetForURL(const String& url)
{
    ASSERT(!m_avfWrapper);
    
    setDelayCallbacks(true);
    m_avfWrapper = new AVFWrapper(this);
    m_avfWrapper->createAssetForURL(url);
    setDelayCallbacks(false);
}

void MediaPlayerPrivateAVFoundationCF::checkPlayability()
{
    ASSERT(m_avfWrapper);
    m_avfWrapper->checkPlayability();
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

PlatformMedia MediaPlayerPrivateAVFoundationCF::platformMedia() const
{
    LOG(Media, "MediaPlayerPrivateAVFoundationCF::platformMedia(%p)", this);
    PlatformMedia pm;
    pm.type = PlatformMedia::AVFoundationCFMediaPlayerType;
    pm.media.avcfMediaPlayer = (AVCFPlayer*)avPlayer(m_avfWrapper);
    return pm;
}

PlatformLayer* MediaPlayerPrivateAVFoundationCF::platformLayer() const
{
    if (!m_avfWrapper)
        return 0;

    return m_avfWrapper->platformLayer();
}

void MediaPlayerPrivateAVFoundationCF::platformSetVisible(bool isVisible)
{
    if (!m_avfWrapper)
        return;
    
    // FIXME: We use a CATransaction here on the Mac, we need to figure out why this was done there and
    // whether we're affected by the same issue.
    setDelayCallbacks(true);
    m_avfWrapper->setVideoLayerHidden(!isVisible);    
    setDelayCallbacks(false);
}

void MediaPlayerPrivateAVFoundationCF::platformPlay()
{
    LOG(Media, "MediaPlayerPrivateAVFoundationCF::play(%p)", this);
    if (!metaDataAvailable() || !avPlayer(m_avfWrapper))
        return;

    setDelayCallbacks(true);
    AVCFPlayerSetRate(avPlayer(m_avfWrapper), requestedRate());
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

void MediaPlayerPrivateAVFoundationCF::updateRate()
{
    LOG(Media, "MediaPlayerPrivateAVFoundationCF::updateRate(%p)", this);
    if (!metaDataAvailable() || !avPlayer(m_avfWrapper))
        return;

    setDelayCallbacks(true);
    AVCFPlayerSetRate(avPlayer(m_avfWrapper), requestedRate());
    setDelayCallbacks(false);
}

float MediaPlayerPrivateAVFoundationCF::platformDuration() const
{
    if (!metaDataAvailable() || !avAsset(m_avfWrapper))
        return 0;

    float duration;
    CMTime cmDuration = AVCFAssetGetDuration(avAsset(m_avfWrapper));
    if (CMTIME_IS_NUMERIC(cmDuration))
        duration = narrowPrecisionToFloat(CMTimeGetSeconds(cmDuration));
    else if (CMTIME_IS_INDEFINITE(cmDuration))
        duration = numeric_limits<float>::infinity();
    else {
        LOG(Media, "MediaPlayerPrivateAVFMac::duration(%p) - invalid duration, returning 0", this);
        return 0;
    }

    return duration;
}

float MediaPlayerPrivateAVFoundationCF::currentTime() const
{
    if (!metaDataAvailable() || !avPlayerItem(m_avfWrapper))
        return 0;

    CMTime itemTime = AVCFPlayerItemGetCurrentTime(avPlayerItem(m_avfWrapper));
    if (CMTIME_IS_NUMERIC(itemTime))
        return narrowPrecisionToFloat(CMTimeGetSeconds(itemTime));

    return 0;
}

void MediaPlayerPrivateAVFoundationCF::seekToTime(float time)
{
    if (!m_avfWrapper)
        return;
    
    // seekToTime generates several event callbacks, update afterwards.
    setDelayCallbacks(true);
    m_avfWrapper->seekToTime(time);
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

float MediaPlayerPrivateAVFoundationCF::rate() const
{
    if (!metaDataAvailable() || !avPlayer(m_avfWrapper))
        return 0;

    setDelayCallbacks(true);
    float currentRate = AVCFPlayerGetRate(avPlayer(m_avfWrapper));
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

PassRefPtr<TimeRanges> MediaPlayerPrivateAVFoundationCF::platformBufferedTimeRanges() const
{
    RefPtr<TimeRanges> timeRanges = TimeRanges::create();

    if (!avPlayerItem(m_avfWrapper))
        return timeRanges.release();

    RetainPtr<CFArrayRef> loadedRanges(AdoptCF, AVCFPlayerItemCopyLoadedTimeRanges(avPlayerItem(m_avfWrapper)));
    if (!loadedRanges)
        return timeRanges.release();

    CFIndex rangeCount = CFArrayGetCount(loadedRanges.get());
    for (CFIndex i = 0; i < rangeCount; i++) {
        CFDictionaryRef range = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(loadedRanges.get(), i));
        CMTime start = CMTimeMakeFromDictionary(static_cast<CFDictionaryRef>(CFDictionaryGetValue(range, CMTimeRangeStartKey())));
        CMTime duration = CMTimeMakeFromDictionary(static_cast<CFDictionaryRef>(CFDictionaryGetValue(range, CMTimeRangeDurationKey())));
        
        if (timeRangeIsValidAndNotEmpty(start, duration)) {
            float rangeStart = narrowPrecisionToFloat(CMTimeGetSeconds(start));
            float rangeEnd = narrowPrecisionToFloat(CMTimeGetSeconds(CMTimeAdd(start, duration)));
            timeRanges->add(rangeStart, rangeEnd);
        }
    }

    return timeRanges.release();
}

float MediaPlayerPrivateAVFoundationCF::platformMaxTimeSeekable() const
{
    if (!avPlayerItem(m_avfWrapper))
        return 0;

    RetainPtr<CFArrayRef> seekableRanges(AdoptCF, AVCFPlayerItemCopySeekableTimeRanges(avPlayerItem(m_avfWrapper)));
    if (!seekableRanges)
        return 0;

    float maxTimeSeekable = 0;
    CFIndex rangeCount = CFArrayGetCount(seekableRanges.get());
    for (CFIndex i = 0; i < rangeCount; i++) {
        CFDictionaryRef range = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(seekableRanges.get(), i));
        CMTime start = CMTimeMakeFromDictionary(static_cast<CFDictionaryRef>(CFDictionaryGetValue(range, CMTimeRangeStartKey())));
        CMTime duration = CMTimeMakeFromDictionary(static_cast<CFDictionaryRef>(CFDictionaryGetValue(range, CMTimeRangeDurationKey())));
        if (!timeRangeIsValidAndNotEmpty(start, duration))
            continue;
        
        float endOfRange = narrowPrecisionToFloat(CMTimeGetSeconds(CMTimeAdd(start, duration)));
        if (maxTimeSeekable < endOfRange)
            maxTimeSeekable = endOfRange;
    }

    return maxTimeSeekable;   
}

float MediaPlayerPrivateAVFoundationCF::platformMaxTimeLoaded() const
{
    if (!avPlayerItem(m_avfWrapper))
        return 0;

    RetainPtr<CFArrayRef> loadedRanges(AdoptCF, AVCFPlayerItemCopyLoadedTimeRanges(avPlayerItem(m_avfWrapper)));
    if (!loadedRanges)
        return 0;

    float maxTimeLoaded = 0;
    CFIndex rangeCount = CFArrayGetCount(loadedRanges.get());
    for (CFIndex i = 0; i < rangeCount; i++) {
        CFDictionaryRef range = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(loadedRanges.get(), i));
        CMTime start = CMTimeMakeFromDictionary(static_cast<CFDictionaryRef>(CFDictionaryGetValue(range, CMTimeRangeStartKey())));
        CMTime duration = CMTimeMakeFromDictionary(static_cast<CFDictionaryRef>(CFDictionaryGetValue(range, CMTimeRangeDurationKey())));
        if (!timeRangeIsValidAndNotEmpty(start, duration))
            continue;
        
        float endOfRange = narrowPrecisionToFloat(CMTimeGetSeconds(CMTimeAdd(start, duration)));
        if (maxTimeLoaded < endOfRange)
            maxTimeLoaded = endOfRange;
    }

    return maxTimeLoaded;   
}

unsigned MediaPlayerPrivateAVFoundationCF::totalBytes() const
{
    if (!metaDataAvailable() || !avAsset(m_avfWrapper))
        return 0;

    int64_t totalMediaSize = 0;
    RetainPtr<CFArrayRef> tracks(AdoptCF, AVCFAssetCopyAssetTracks(avAsset(m_avfWrapper)));
    CFIndex trackCount = CFArrayGetCount(tracks.get());
    for (CFIndex i = 0; i < trackCount; i++) {
        AVCFAssetTrackRef assetTrack = (AVCFAssetTrackRef)CFArrayGetValueAtIndex(tracks.get(), i);
        totalMediaSize += AVCFAssetTrackGetTotalSampleDataLength(assetTrack);
    }

    // FIXME: It doesn't seem safe to cast an int64_t to unsigned.
    return static_cast<unsigned>(totalMediaSize);
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
        if (keyStatus == AVCFPropertyValueStatusFailed)
            return MediaPlayerAVAssetStatusFailed;
        if (keyStatus == AVCFPropertyValueStatusCancelled)
            return MediaPlayerAVAssetStatusCancelled;
    }

    if (AVCFAssetIsPlayable(avAsset(m_avfWrapper)))
        return MediaPlayerAVAssetStatusPlayable;

    return MediaPlayerAVAssetStatusLoaded;
}

void MediaPlayerPrivateAVFoundationCF::paintCurrentFrameInContext(GraphicsContext* context, const IntRect& rect)
{
    if (context->paintingDisabled())
        return;

    if (currentRenderingMode() == MediaRenderingToLayer && !imageGenerator(m_avfWrapper)) {
        // We're being told to render into a context, but we already have the
        // video layer, which probably means we've been called from <canvas>.
        createContextVideoRenderer();
    }

    paint(context, rect);
}

void MediaPlayerPrivateAVFoundationCF::paint(GraphicsContext* context, const IntRect& rect)
{
    if (context->paintingDisabled() || !imageGenerator(m_avfWrapper))
        return;

    LOG(Media, "MediaPlayerPrivateAVFoundationCF::paint(%p)", this);

    setDelayCallbacks(true);
    RetainPtr<CGImageRef> image = m_avfWrapper->createImageForTimeInRect(currentTime(), rect);
    if (image) {
        context->save();
        context->translate(rect.x(), rect.y() + rect.height());
        context->scale(FloatSize(1.0f, -1.0f));
        context->setImageInterpolationQuality(InterpolationLow);
        IntRect paintRect(IntPoint(0, 0), IntSize(rect.width(), rect.height()));
        CGContextDrawImage(context->platformContext(), CGRectMake(0, 0, paintRect.width(), paintRect.height()), image.get());
        context->restore();
        image = 0;
    }
    setDelayCallbacks(false);
    
    m_videoFrameHasDrawn = true;
}

static HashSet<String> mimeTypeCache()
{
    DEFINE_STATIC_LOCAL(HashSet<String>, cache, ());
    static bool typeListInitialized = false;

    if (typeListInitialized)
        return cache;
    typeListInitialized = true;
    
    RetainPtr<CFArrayRef> supportedTypes(AdoptCF, AVCFURLAssetCopyAudiovisualMIMETypes());
    
    ASSERT(supportedTypes);
    if (!supportedTypes)
        return cache;

    CFIndex typeCount = CFArrayGetCount(supportedTypes.get());
    for (CFIndex i = 0; i < typeCount; i++)
        cache.add(static_cast<CFStringRef>(CFArrayGetValueAtIndex(supportedTypes.get(), i)));

    return cache;
} 

void MediaPlayerPrivateAVFoundationCF::getSupportedTypes(HashSet<String>& supportedTypes)
{
    supportedTypes = mimeTypeCache();
} 

MediaPlayer::SupportsType MediaPlayerPrivateAVFoundationCF::supportsType(const String& type, const String& codecs)
{
    // Only return "IsSupported" if there is no codecs parameter for now as there is no way to ask if it supports an
    // extended MIME type until rdar://8721715 is fixed.
    if (mimeTypeCache().contains(type))
        return codecs.isEmpty() ? MediaPlayer::MayBeSupported : MediaPlayer::IsSupported;

    return MediaPlayer::IsNotSupported;
}


bool MediaPlayerPrivateAVFoundationCF::isAvailable()
{
    return AVFoundationCFLibrary() && CoreMediaLibrary();
}

float MediaPlayerPrivateAVFoundationCF::mediaTimeForTimeValue(float timeValue) const
{
    if (!metaDataAvailable())
        return timeValue;

    // FIXME - can not implement until rdar://8721669 is fixed.
    return timeValue;
}

void MediaPlayerPrivateAVFoundationCF::tracksChanged()
{
    if (!avAsset(m_avfWrapper))
        return;
    
    // This is called whenever the tracks collection changes so cache hasVideo and hasAudio since we are
    // asked about those fairly frequently.
    if (!avPlayerItem(m_avfWrapper)) {
        // We don't have a player item yet, so check with the asset because some assets support inspection
        // prior to becoming ready to play.
        RetainPtr<CFArrayRef> visualTracks(AdoptCF, AVCFAssetCopyTracksWithMediaCharacteristic(avAsset(m_avfWrapper), AVCFMediaCharacteristicVisual));
        setHasVideo(CFArrayGetCount(visualTracks.get()));

        RetainPtr<CFArrayRef> audioTracks(AdoptCF, AVCFAssetCopyTracksWithMediaCharacteristic(avAsset(m_avfWrapper), AVCFMediaCharacteristicAudible));
        setHasAudio(CFArrayGetCount(audioTracks.get()));

        RetainPtr<CFArrayRef> captionTracks(AdoptCF, AVCFAssetCopyTracksWithMediaType(avAsset(m_avfWrapper), AVCFMediaTypeClosedCaption));
        setHasAudio(CFArrayGetCount(captionTracks.get()));
    } else {
        bool hasVideo = false;
        bool hasAudio = false;
        bool hasCaptions = false;

        RetainPtr<CFArrayRef> tracks(AdoptCF, AVCFPlayerItemCopyTracks(avPlayerItem(m_avfWrapper)));

        CFIndex trackCount = CFArrayGetCount(tracks.get());
        for (CFIndex i = 0; i < trackCount; i++) {
            AVCFPlayerItemTrackRef track = (AVCFPlayerItemTrackRef)(CFArrayGetValueAtIndex(tracks.get(), i));
            
            if (AVCFPlayerItemTrackIsEnabled(track)) {
                RetainPtr<AVCFAssetTrackRef> assetTrack(AdoptCF, AVCFPlayerItemTrackCopyAssetTrack(track));
                CFStringRef mediaType = AVCFAssetTrackGetMediaType(assetTrack.get());
                if (!mediaType)
                    continue;
                
                if (CFStringCompare(mediaType, AVCFMediaTypeVideo, kCFCompareCaseInsensitive) == kCFCompareEqualTo)
                    hasVideo = true;
                else if (CFStringCompare(mediaType, AVCFMediaTypeAudio, kCFCompareCaseInsensitive) == kCFCompareEqualTo)
                    hasAudio = true;
                else if (CFStringCompare(mediaType, AVCFMediaTypeClosedCaption, kCFCompareCaseInsensitive) == kCFCompareEqualTo)
                    hasCaptions = true;
            }
        }

        setHasVideo(hasVideo);
        setHasAudio(hasAudio);
        setHasClosedCaptions(hasCaptions);
    }

    LOG(Media, "MediaPlayerPrivateAVFoundationCF:tracksChanged(%p) - hasVideo = %s, hasAudio = %s, hasCaptions = %s", 
        this, boolString(hasVideo()), boolString(hasAudio()), boolString(hasClosedCaptions()));

    sizeChanged();
}

void MediaPlayerPrivateAVFoundationCF::sizeChanged()
{
    if (!avAsset(m_avfWrapper))
        return;
    
    // AVAsset's 'naturalSize' property only considers the movie's first video track, so we need to compute
    // the union of all visual track rects.
    CGRect trackRectUnion = CGRectZero;
    RetainPtr<CFArrayRef> tracks(AdoptCF, AVCFAssetCopyTracksWithMediaType(avAsset(m_avfWrapper), AVCFMediaCharacteristicVisual));
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

    // Also look at the asset's preferred transform so we account for a movie matrix.
    CGSize movieSize = CGSizeApplyAffineTransform(AVCFAssetGetNaturalSize(avAsset(m_avfWrapper)), AVCFAssetGetPreferredTransform(avAsset(m_avfWrapper)));
    if (movieSize.width > naturalSize.width)
        naturalSize.width = movieSize.width;
    if (movieSize.height > naturalSize.height)
        naturalSize.height = movieSize.height;
    setNaturalSize(IntSize(naturalSize));
}

void MediaPlayerPrivateAVFoundationCF::contentsNeedsDisplay()
{
    if (m_avfWrapper)
        m_avfWrapper->setVideoLayerNeedsCommit();
}

AVFWrapper::AVFWrapper(MediaPlayerPrivateAVFoundationCF* owner)
    : m_owner(owner)
    , m_objectID(s_nextAVFWrapperObjectID++)
{
    LOG(Media, "AVFWrapper::AVFWrapper(%p)", this);

    m_notificationQueue = dispatch_queue_create("MediaPlayerPrivateAVFoundationCF.notificationQueue", 0);
    addToMap();
}

AVFWrapper::~AVFWrapper()
{
    LOG(Media, "AVFWrapper::~AVFWrapper(%p %d)", this, m_objectID);

    destroyVideoLayer();
    destroyImageGenerator();

    if (m_notificationQueue)
        dispatch_release(m_notificationQueue);

    if (avAsset()) {
        AVCFAssetCancelLoading(avAsset());
        m_avAsset = 0;
    }

    m_avPlayerItem = 0;
    m_timeObserver = 0;
    m_avPlayer = 0;
}

Mutex& AVFWrapper::mapLock()
{
    static Mutex mapLock;
    return mapLock;
}

HashMap<uintptr_t, AVFWrapper*>& AVFWrapper::map()
{
    static HashMap<uintptr_t, AVFWrapper*>& map = *new HashMap<uintptr_t, AVFWrapper*>;
    return map;
}

void AVFWrapper::addToMap()
{
    MutexLocker locker(mapLock());
    
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

    MutexLocker locker(mapLock());
    map().remove(m_objectID);
}

AVFWrapper* AVFWrapper::avfWrapperForCallbackContext(void* context)
{
    // Assumes caller has locked mapLock().
    HashMap<uintptr_t, AVFWrapper*>::iterator it = map().find(reinterpret_cast<uintptr_t>(context));
    if (it == map().end())
        return 0;

    return it->second;
}

void AVFWrapper::scheduleDisconnectAndDelete()
{
    // Ignore any subsequent notifications we might receive in notificationCallback().
    removeFromMap();

    dispatch_async_f(dispatchQueue(), this, disconnectAndDeleteAVFWrapper);
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
        CFNotificationCenterRemoveObserver(center, avfWrapper->callbackContext(), AVCFPlayerItemIsPlaybackLikelyToKeepUpChangedNotification, avfWrapper->avPlayerItem());
        CFNotificationCenterRemoveObserver(center, avfWrapper->callbackContext(), AVCFPlayerItemIsPlaybackBufferEmptyChangedNotification, avfWrapper->avPlayerItem());
        CFNotificationCenterRemoveObserver(center, avfWrapper->callbackContext(), AVCFPlayerItemIsPlaybackBufferFullChangedNotification, avfWrapper->avPlayerItem());
        CFNotificationCenterRemoveObserver(center, avfWrapper->callbackContext(), CACFContextNeedsFlushNotification(), 0);
    }

    if (avfWrapper->avPlayer()) {
        if (avfWrapper->timeObserver())
            AVCFPlayerRemoveObserver(avfWrapper->avPlayer(), avfWrapper->timeObserver());

        CFNotificationCenterRemoveObserver(CFNotificationCenterGetLocalCenter(), avfWrapper->callbackContext(), AVCFPlayerRateChangedNotification, avfWrapper->avPlayer());
    }

    delete avfWrapper;
}

void AVFWrapper::createAssetForURL(const String& url)
{
    ASSERT(!avAsset());
    
    RetainPtr<CFURLRef> urlRef(AdoptCF, KURL(ParsedURLString, url).createCFURL());
    AVCFURLAssetRef assetRef = AVCFURLAssetCreateWithURLAndOptions(kCFAllocatorDefault, urlRef.get(), 0, m_notificationQueue);
    m_avAsset.adoptCF(assetRef);
}

void AVFWrapper::createPlayer()
{
    ASSERT(!avPlayer() && avPlayerItem());

    // FIXME: We need a way to create a AVPlayer without an AVPlayerItem, see <rdar://problem/9877730>.
    AVCFPlayerRef playerRef = AVCFPlayerCreateWithPlayerItemAndOptions(kCFAllocatorDefault, avPlayerItem(), 0, m_notificationQueue);
    m_avPlayer.adoptCF(playerRef);

    CFNotificationCenterRef center = CFNotificationCenterGetLocalCenter();
    ASSERT(center);

    CFNotificationCenterAddObserver(center, callbackContext(), notificationCallback, AVCFPlayerRateChangedNotification, playerRef, CFNotificationSuspensionBehaviorDeliverImmediately);

    // Add a time observer, ask to be called infrequently because we don't really want periodic callbacks but
    // our observer will also be called whenever a seek happens.
    const double veryLongInterval = 60*60*60*24*30;
    m_timeObserver.adoptCF(AVCFPlayerCreatePeriodicTimeObserverForInterval(playerRef, CMTimeMake(veryLongInterval, 10), m_notificationQueue, &periodicTimeObserverCallback, callbackContext()));
}

void AVFWrapper::createPlayerItem()
{
    ASSERT(!avPlayerItem() && avAsset());

    // Create the player item so we begin loading media data.
    AVCFPlayerItemRef itemRef = AVCFPlayerItemCreateWithAsset(kCFAllocatorDefault, avAsset(), m_notificationQueue);
    m_avPlayerItem.adoptCF(itemRef);

    CFNotificationCenterRef center = CFNotificationCenterGetLocalCenter();
    ASSERT(center);

    CFNotificationCenterAddObserver(center, callbackContext(), notificationCallback, AVCFPlayerItemDidPlayToEndTimeNotification, itemRef, CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(center, callbackContext(), notificationCallback, AVCFPlayerItemStatusChangedNotification, itemRef, CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(center, callbackContext(), notificationCallback, AVCFPlayerItemTracksChangedNotification, itemRef, CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(center, callbackContext(), notificationCallback, AVCFPlayerItemSeekableTimeRangesChangedNotification, itemRef, CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(center, callbackContext(), notificationCallback, AVCFPlayerItemLoadedTimeRangesChangedNotification, itemRef, CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(center, callbackContext(), notificationCallback, AVCFPlayerItemIsPlaybackLikelyToKeepUpChangedNotification, itemRef, CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(center, callbackContext(), notificationCallback, AVCFPlayerItemIsPlaybackBufferEmptyChangedNotification, itemRef, CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(center, callbackContext(), notificationCallback, AVCFPlayerItemIsPlaybackBufferFullChangedNotification, itemRef, CFNotificationSuspensionBehaviorDeliverImmediately);

    CFNotificationCenterAddObserver(center, callbackContext(), notificationCallback, CACFContextNeedsFlushNotification(), 0, CFNotificationSuspensionBehaviorDeliverImmediately);
}

void AVFWrapper::periodicTimeObserverCallback(AVCFPlayerRef, CMTime cmTime, void* context)
{
    MutexLocker locker(mapLock());
    AVFWrapper* self = avfWrapperForCallbackContext(context);
    if (!self) {
        LOG(Media, "AVFWrapper::periodicTimeObserverCallback invoked for deleted AVFWrapper %d", reinterpret_cast<uintptr_t>(context));
        return;
    }

    double time = std::max(0.0, CMTimeGetSeconds(cmTime)); // Clamp to zero, negative values are sometimes reported.
    self->m_owner->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::PlayerTimeChanged, time);
}

void AVFWrapper::notificationCallback(CFNotificationCenterRef, void* observer, CFStringRef propertyName, const void* object, CFDictionaryRef)
{
    MutexLocker locker(mapLock());
    AVFWrapper* self = avfWrapperForCallbackContext(observer);
    
    if (!self) {
        LOG(Media, "AVFWrapper::notificationCallback invoked for deleted AVFWrapper %d", reinterpret_cast<uintptr_t>(observer));
        return;
    }

#if !LOG_DISABLED
    char notificationName[256];
    CFStringGetCString(propertyName, notificationName, sizeof(notificationName), kCFStringEncodingASCII);
    LOG(Media, "AVFWrapper::notificationCallback(%p) %s", self, notificationName);
#endif

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
    else
        ASSERT_NOT_REACHED();
}

void AVFWrapper::loadPlayableCompletionCallback(AVCFAssetRef, void* context)
{
    MutexLocker locker(mapLock());
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

    static CFArrayRef propertyKeyName;
    if (!propertyKeyName) {
        static const CFStringRef keyNames[] = { 
            AVCFAssetPropertyPlayable
        };
        propertyKeyName = CFArrayCreate(0, (const void**)keyNames, sizeof(keyNames) / sizeof(keyNames[0]), &kCFTypeArrayCallBacks);
    }

    AVCFAssetLoadValuesAsynchronouslyForProperties(avAsset(), propertyKeyName, loadPlayableCompletionCallback, callbackContext());
}

void AVFWrapper::loadMetadataCompletionCallback(AVCFAssetRef, void* context)
{
    MutexLocker locker(mapLock());
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
    MutexLocker locker(mapLock());
    AVFWrapper* self = avfWrapperForCallbackContext(context);
    if (!self) {
        LOG(Media, "AVFWrapper::seekCompletedCallback invoked for deleted AVFWrapper %d", reinterpret_cast<uintptr_t>(context));
        return;
    }

    LOG(Media, "AVFWrapper::seekCompletedCallback(%p)", self);
    self->m_owner->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::SeekCompleted, static_cast<bool>(finished));
}

void AVFWrapper::seekToTime(float time)
{
    ASSERT(avPlayerItem());
    AVCFPlayerItemSeekToTimeWithToleranceAndCompletionCallback(avPlayerItem(), CMTimeMakeWithSeconds(time, 600),
        kCMTimeZero, kCMTimeZero, &seekCompletedCallback, callbackContext());
}

void AVFWrapper::setAsset(AVCFURLAssetRef asset)
{
    if (asset == avAsset())
        return;

    AVCFAssetCancelLoading(avAsset());
    m_avAsset.adoptCF(asset);
}

PlatformLayer* AVFWrapper::platformLayer()
{
    if (m_videoLayerWrapper)
        return m_videoLayerWrapper->platformLayer();

    if (!videoLayer())
        return 0;

    // Create a PlatformCALayer so we can resize the video layer to match the element size.
    m_layerClient = adoptPtr(new LayerClient(this));
    if (!m_layerClient)
        return 0;

    m_videoLayerWrapper = PlatformCALayer::create(PlatformCALayer::LayerTypeLayer, m_layerClient.get());
    if (!m_videoLayerWrapper)
        return 0;

    CACFLayerRef layerRef = AVCFPlayerLayerCopyCACFLayer(m_avCFVideoLayer.get());
    m_caVideoLayer.adoptCF(layerRef);

    CACFLayerInsertSublayer(m_videoLayerWrapper->platformLayer(), m_caVideoLayer.get(), 0);
    m_videoLayerWrapper->setAnchorPoint(FloatPoint3D());
    m_videoLayerWrapper->setNeedsLayout();

    return m_videoLayerWrapper->platformLayer();
}

void AVFWrapper::createAVCFVideoLayer()
{
    if (!avPlayer() || m_avCFVideoLayer)
        return;

    // The layer will get hooked up via RenderLayerBacking::updateGraphicsLayerConfiguration().
    m_avCFVideoLayer.adoptCF(AVCFPlayerLayerCreateWithAVCFPlayer(kCFAllocatorDefault, avPlayer(), m_notificationQueue));
    LOG(Media, "AVFWrapper::createAVCFVideoLayer(%p) - returning %p", this, videoLayer());
}

void AVFWrapper::destroyVideoLayer()
{
    LOG(Media, "AVFWrapper::destroyVideoLayer(%p)", this);
    m_layerClient = nullptr;
    m_caVideoLayer = 0;
    m_videoLayerWrapper = 0;
    if (!m_avCFVideoLayer.get())
        return;

    AVCFPlayerLayerSetPlayer((AVCFPlayerLayerRef)m_avCFVideoLayer.get(), 0);
    m_avCFVideoLayer = 0;
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
    if (!avAsset() || m_imageGenerator)
        return;

    m_imageGenerator.adoptCF(AVCFAssetImageGeneratorCreateWithAsset(kCFAllocatorDefault, avAsset()));

    AVCFAssetImageGeneratorSetApertureMode(m_imageGenerator.get(), AVCFAssetImageGeneratorApertureModeCleanAperture);
    AVCFAssetImageGeneratorSetRequestedTimeToleranceBefore(m_imageGenerator.get(), kCMTimeZero);
    AVCFAssetImageGeneratorSetRequestedTimeToleranceAfter(m_imageGenerator.get(), kCMTimeZero);
    AVCFAssetImageGeneratorSetAppliesPreferredTrackTransform(m_imageGenerator.get(), true);

    LOG(Media, "AVFWrapper::createImageGenerator(%p) - returning %p", this, m_imageGenerator.get());
}

void AVFWrapper::destroyImageGenerator()
{
    LOG(Media, "AVFWrapper::destroyImageGenerator(%p)", this);
    m_imageGenerator = 0;
}

RetainPtr<CGImageRef> AVFWrapper::createImageForTimeInRect(float time, const IntRect& rect)
{
    if (!m_imageGenerator)
        return 0;

#if !LOG_DISABLED
    double start = WTF::currentTime();
#endif

    AVCFAssetImageGeneratorSetMaximumSize(m_imageGenerator.get(), CGSize(rect.size()));
    CGImageRef image = AVCFAssetImageGeneratorCopyCGImageAtTime(m_imageGenerator.get(), CMTimeMakeWithSeconds(time, 600), 0, 0);

#if !LOG_DISABLED
    double duration = WTF::currentTime() - start;
    LOG(Media, "AVFWrapper::createImageForTimeInRect(%p) - creating image took %.4f", this, narrowPrecisionToFloat(duration));
#endif

    return image;
}

void LayerClient::platformCALayerLayoutSublayersOfLayer(PlatformCALayer* wrapperLayer)
{
    ASSERT(m_parent);
    ASSERT(m_parent->videoLayerWrapper() == wrapperLayer->platformLayer());

    CGRect bounds = wrapperLayer->bounds();
    CGPoint anchor = CACFLayerGetAnchorPoint(m_parent->caVideoLayer());
    FloatPoint position(bounds.size.width * anchor.x, bounds.size.height * anchor.y); 

    CACFLayerSetPosition(m_parent->caVideoLayer(), position);
    CACFLayerSetBounds(m_parent->caVideoLayer(), bounds);
}

} // namespace WebCore

#else
// AVFoundation should always be enabled for Apple production builds.
#if __PRODUCTION__ && !USE(AVFOUNDATION)
#error AVFoundation is not enabled!
#endif // __PRODUCTION__ && !USE(AVFOUNDATION)
#endif // USE(AVFOUNDATION)
#endif // PLATFORM(WIN) && ENABLE(VIDEO)
