/*
 * Copyright (C) 2007, 2009 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Collabora Ltd. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009, 2010, 2015, 2016 Igalia S.L
 * Copyright (C) 2015, 2016 Metrological Group B.V.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef MediaPlayerPrivateGStreamerBase_h
#define MediaPlayerPrivateGStreamerBase_h
#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "GStreamerCommon.h"
#include "MainThreadNotifier.h"
#include "MediaPlayerPrivate.h"
#include "PlatformLayer.h"
#include <glib.h>
#include <gst/gst.h>
#include <wtf/Condition.h>
#include <wtf/Forward.h>
#include <wtf/RunLoop.h>
#include <wtf/WeakPtr.h>

#if USE(TEXTURE_MAPPER_GL)
#if USE(NICOSIA)
#include "NicosiaContentLayerTextureMapperImpl.h"
#else
#include "TextureMapperPlatformLayerProxyProvider.h"
#endif
#endif

typedef struct _GstStreamVolume GstStreamVolume;
typedef struct _GstVideoInfo GstVideoInfo;
typedef struct _GstGLContext GstGLContext;
typedef struct _GstGLDisplay GstGLDisplay;

namespace WebCore {

class BitmapTextureGL;
class GLContext;
class GraphicsContext;
class GraphicsContext3D;
class IntSize;
class IntRect;
class VideoTextureCopierGStreamer;

#if USE(TEXTURE_MAPPER_GL)
class TextureMapperPlatformLayerProxy;
#endif

void registerWebKitGStreamerElements();

class MediaPlayerPrivateGStreamerBase : public MediaPlayerPrivateInterface, public CanMakeWeakPtr<MediaPlayerPrivateGStreamerBase>
#if USE(TEXTURE_MAPPER_GL)
#if USE(NICOSIA)
    , public Nicosia::ContentLayerTextureMapperImpl::Client
#else
    , public PlatformLayer
#endif
#endif
{

public:
    static void initializeDebugCategory();

    virtual ~MediaPlayerPrivateGStreamerBase();

    FloatSize naturalSize() const override;

    void setVolume(float) override;
    float volume() const override;

#if USE(GSTREAMER_GL)
    bool ensureGstGLContext();
    GstContext* requestGLContext(const char* contextType);
#endif
    bool supportsMuting() const override { return true; }
    void setMuted(bool) override;
    bool muted() const;

    MediaPlayer::NetworkState networkState() const override;
    MediaPlayer::ReadyState readyState() const override;

    void setVisible(bool) override { }
    void setSize(const IntSize&) override;
    void sizeChanged();

    // Prefer MediaTime based methods over float based.
    float duration() const override { return durationMediaTime().toFloat(); }
    double durationDouble() const override { return durationMediaTime().toDouble(); }
    MediaTime durationMediaTime() const override { return MediaTime::zeroTime(); }
    float currentTime() const override { return currentMediaTime().toFloat(); }
    double currentTimeDouble() const override { return currentMediaTime().toDouble(); }
    MediaTime currentMediaTime() const override { return MediaTime::zeroTime(); }
    void seek(float time) override { seek(MediaTime::createWithFloat(time)); }
    void seekDouble(double time) override { seek(MediaTime::createWithDouble(time)); }
    void seek(const MediaTime&) override { }
    float maxTimeSeekable() const override { return maxMediaTimeSeekable().toFloat(); }
    MediaTime maxMediaTimeSeekable() const override { return MediaTime::zeroTime(); }
    double minTimeSeekable() const override { return minMediaTimeSeekable().toFloat(); }
    MediaTime minMediaTimeSeekable() const override { return MediaTime::zeroTime(); }

    void paint(GraphicsContext&, const FloatRect&) override;

    bool hasSingleSecurityOrigin() const override { return true; }
    virtual MediaTime maxTimeLoaded() const { return MediaTime::zeroTime(); }

    bool supportsFullscreen() const override;

    MediaPlayer::MovieLoadType movieLoadType() const override;
    virtual bool isLiveStream() const = 0;

    MediaPlayer* mediaPlayer() const { return m_player; }

    unsigned decodedFrameCount() const override;
    unsigned droppedFrameCount() const override;
    unsigned audioDecodedByteCount() const override;
    unsigned videoDecodedByteCount() const override;

    void acceleratedRenderingStateChanged() override;

#if USE(TEXTURE_MAPPER_GL)
    PlatformLayer* platformLayer() const override;
#if PLATFORM(WIN_CAIRO)
    // FIXME: Accelerated rendering has not been implemented for WinCairo yet.
    bool supportsAcceleratedRendering() const override { return false; }
#else
    bool supportsAcceleratedRendering() const override { return true; }
#endif
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    void cdmInstanceAttached(CDMInstance&) override;
    void cdmInstanceDetached(CDMInstance&) override;
    void dispatchDecryptionKey(GstBuffer*);
    void handleProtectionEvent(GstEvent*);
    void attemptToDecryptWithLocalInstance();
    void attemptToDecryptWithInstance(CDMInstance&) override;
    void dispatchCDMInstance();
    void initializationDataEncountered(GstEvent*);
    void setWaitingForKey(bool);
    bool waitingForKey() const override;
#endif

    static bool supportsKeySystem(const String& keySystem, const String& mimeType);
    static MediaPlayer::SupportsType extendedSupportsType(const MediaEngineSupportParameters&, MediaPlayer::SupportsType);

#if USE(GSTREAMER_GL)
    bool copyVideoTextureToPlatformTexture(GraphicsContext3D*, Platform3DObject, GC3Denum, GC3Dint, GC3Denum, GC3Denum, GC3Denum, bool, bool) override;
    NativeImagePtr nativeImageForCurrentTime() override;
#endif

    void setVideoSourceOrientation(const ImageOrientation&);
    GstElement* pipeline() const { return m_pipeline.get(); }

    virtual bool handleSyncMessage(GstMessage*);

protected:
    MediaPlayerPrivateGStreamerBase(MediaPlayer*);
    virtual GstElement* createVideoSink();

#if USE(GSTREAMER_GL)
    static GstFlowReturn newSampleCallback(GstElement*, MediaPlayerPrivateGStreamerBase*);
    static GstFlowReturn newPrerollCallback(GstElement*, MediaPlayerPrivateGStreamerBase*);
    void flushCurrentBuffer();
    GstElement* createGLAppSink();
    GstElement* createVideoSinkGL();
    GstGLContext* gstGLContext() const { return m_glContext.get(); }
    GstGLDisplay* gstGLDisplay() const { return m_glDisplay.get(); }
    void ensureGLVideoSinkContext();
#endif

#if USE(TEXTURE_MAPPER_GL)
    void pushTextureToCompositor();
#if USE(NICOSIA)
    void swapBuffersIfNeeded() override;
#else
    RefPtr<TextureMapperPlatformLayerProxy> proxy() const override;
    void swapBuffersIfNeeded() override;
#endif
#endif

    GstElement* videoSink() const { return m_videoSink.get(); }

    void setStreamVolumeElement(GstStreamVolume*);
    virtual GstElement* createAudioSink() { return 0; }
    virtual GstElement* audioSink() const { return 0; }

    void setPipeline(GstElement*);

    void triggerRepaint(GstSample*);
    void repaint();
    void cancelRepaint(bool destroying = false);

    static void repaintCallback(MediaPlayerPrivateGStreamerBase*, GstSample*);
    static void repaintCancelledCallback(MediaPlayerPrivateGStreamerBase*);

    void notifyPlayerOfVolumeChange();
    void notifyPlayerOfMute();

    static void volumeChangedCallback(MediaPlayerPrivateGStreamerBase*);
    static void muteChangedCallback(MediaPlayerPrivateGStreamerBase*);

    enum MainThreadNotification {
        VideoChanged = 1 << 0,
        VideoCapsChanged = 1 << 1,
        AudioChanged = 1 << 2,
        VolumeChanged = 1 << 3,
        MuteChanged = 1 << 4,
#if ENABLE(VIDEO_TRACK)
        TextChanged = 1 << 5,
#endif
        SizeChanged = 1 << 6,
#if GST_CHECK_VERSION(1, 10, 0)
        StreamCollectionChanged = 1 << 7
#endif
    };

    Ref<MainThreadNotifier<MainThreadNotification>> m_notifier;
    MediaPlayer* m_player;
    GRefPtr<GstElement> m_pipeline;
    GRefPtr<GstStreamVolume> m_volumeElement;
    GRefPtr<GstElement> m_videoSink;
    GRefPtr<GstElement> m_fpsSink;
    MediaPlayer::ReadyState m_readyState;
    mutable MediaPlayer::NetworkState m_networkState;
    IntSize m_size;
    mutable Lock m_sampleMutex;
    GRefPtr<GstSample> m_sample;

    mutable FloatSize m_videoSize;
    bool m_usingFallbackVideoSink { false };
    bool m_renderingCanBeAccelerated { false };

    Condition m_drawCondition;
    Lock m_drawMutex;
    bool m_destroying { false };
    RunLoop::Timer<MediaPlayerPrivateGStreamerBase> m_drawTimer;

#if USE(TEXTURE_MAPPER_GL)
#if USE(NICOSIA)
    Ref<Nicosia::ContentLayer> m_nicosiaLayer;
#else
    RefPtr<TextureMapperPlatformLayerProxy> m_platformLayerProxy;
#endif
#endif

#if USE(GSTREAMER_GL)
    GRefPtr<GstGLContext> m_glContext;
    GRefPtr<GstGLDisplay> m_glDisplay;
    GRefPtr<GstContext> m_glDisplayElementContext;
    GRefPtr<GstContext> m_glAppElementContext;
    std::unique_ptr<VideoTextureCopierGStreamer> m_videoTextureCopier;
#endif

    ImageOrientation m_videoSourceOrientation;

#if ENABLE(ENCRYPTED_MEDIA)
    Lock m_protectionMutex;
    Condition m_protectionCondition;
    RefPtr<const CDMInstance> m_cdmInstance;
    HashSet<uint32_t> m_handledProtectionEvents;
    bool m_needToResendCredentials { false };
    bool m_waitingForKey { false };
#endif
};

}

#endif // USE(GSTREAMER)
#endif
