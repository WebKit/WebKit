/*
 * Copyright (C) 2007, 2009 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Collabora Ltd. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009, 2010 Igalia S.L
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

#include "GRefPtrGStreamer.h"
#include "MainThreadNotifier.h"
#include "MediaPlayerPrivate.h"
#include <glib.h>
#include <wtf/Condition.h>
#include <wtf/Forward.h>
#include <wtf/RunLoop.h>

#if USE(TEXTURE_MAPPER_GL) && !USE(COORDINATED_GRAPHICS)
#include "TextureMapperPlatformLayer.h"
#endif
#if USE(COORDINATED_GRAPHICS_THREADED)
#include "TextureMapperPlatformLayerProxy.h"
#endif

typedef struct _GstMessage GstMessage;
typedef struct _GstStreamVolume GstStreamVolume;
typedef struct _GstVideoInfo GstVideoInfo;
typedef struct _GstGLContext GstGLContext;
typedef struct _GstGLDisplay GstGLDisplay;

namespace WebCore {

class BitmapTextureGL;
class GraphicsContext;
class GraphicsContext3D;
class IntSize;
class IntRect;

class MediaPlayerPrivateGStreamerBase : public MediaPlayerPrivateInterface
#if USE(TEXTURE_MAPPER_GL) && !USE(COORDINATED_GRAPHICS)
    , public TextureMapperPlatformLayer
#elif USE(COORDINATED_GRAPHICS_THREADED)
    , public TextureMapperPlatformLayerProxyProvider
#endif
{

public:
    virtual ~MediaPlayerPrivateGStreamerBase();

    virtual FloatSize naturalSize() const override;

    virtual void setVolume(float) override;
    virtual float volume() const;

#if USE(GSTREAMER_GL)
    bool ensureGstGLContext();
#endif

    virtual bool supportsMuting() const override { return true; }
    virtual void setMuted(bool) override;
    bool muted() const;

    virtual MediaPlayer::NetworkState networkState() const override;
    virtual MediaPlayer::ReadyState readyState() const override;

    virtual void setVisible(bool) override { }
    virtual void setSize(const IntSize&) override;
    void sizeChanged();

    virtual void paint(GraphicsContext&, const FloatRect&) override;

    virtual bool hasSingleSecurityOrigin() const override { return true; }
    virtual float maxTimeLoaded() const { return 0.0; }

    virtual bool supportsFullscreen() const override;
    virtual PlatformMedia platformMedia() const override;

    virtual MediaPlayer::MovieLoadType movieLoadType() const override;
    virtual bool isLiveStream() const = 0;

    MediaPlayer* mediaPlayer() const { return m_player; }

    virtual unsigned decodedFrameCount() const override;
    virtual unsigned droppedFrameCount() const override;
    virtual unsigned audioDecodedByteCount() const override;
    virtual unsigned videoDecodedByteCount() const override;

#if USE(TEXTURE_MAPPER_GL) && !USE(COORDINATED_GRAPHICS)
    virtual PlatformLayer* platformLayer() const override { return const_cast<MediaPlayerPrivateGStreamerBase*>(this); }
#if PLATFORM(WIN_CAIRO)
    // FIXME: Accelerated rendering has not been implemented for WinCairo yet.
    virtual bool supportsAcceleratedRendering() const override { return false; }
#else
    virtual bool supportsAcceleratedRendering() const override { return true; }
#endif
    virtual void paintToTextureMapper(TextureMapper&, const FloatRect&, const TransformationMatrix&, float) override;
#endif

#if USE(COORDINATED_GRAPHICS_THREADED)
    virtual PlatformLayer* platformLayer() const override { return const_cast<MediaPlayerPrivateGStreamerBase*>(this); }
    virtual bool supportsAcceleratedRendering() const override { return true; }
#endif

#if USE(GSTREAMER_GL)
    virtual PassNativeImagePtr nativeImageForCurrentTime();
#endif

protected:
    MediaPlayerPrivateGStreamerBase(MediaPlayer*);
    virtual GstElement* createVideoSink();

    void setStreamVolumeElement(GstStreamVolume*);
    virtual GstElement* createAudioSink() { return 0; }
    virtual GstElement* audioSink() const { return 0; }

    void setPipeline(GstElement*);

    virtual bool handleSyncMessage(GstMessage*);

    void triggerRepaint(GstSample*);
    void repaint();

    static void repaintCallback(MediaPlayerPrivateGStreamerBase*, GstSample*);
#if USE(GSTREAMER_GL)
    static gboolean drawCallback(MediaPlayerPrivateGStreamerBase*, GstGLContext*, GstSample*);
#endif

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
    };

    MainThreadNotifier<MainThreadNotification> m_notifier;
    MediaPlayer* m_player;
    GRefPtr<GstElement> m_pipeline;
    GRefPtr<GstStreamVolume> m_volumeElement;
    GRefPtr<GstElement> m_videoSink;
    GRefPtr<GstElement> m_fpsSink;
    MediaPlayer::ReadyState m_readyState;
    MediaPlayer::NetworkState m_networkState;
    IntSize m_size;
    mutable GMutex m_sampleMutex;
    GRefPtr<GstSample> m_sample;
#if USE(GSTREAMER_GL)
    RunLoop::Timer<MediaPlayerPrivateGStreamerBase> m_drawTimer;
#endif
    mutable FloatSize m_videoSize;
    bool m_usingFallbackVideoSink;
#if USE(TEXTURE_MAPPER_GL) && !USE(COORDINATED_GRAPHICS_MULTIPROCESS)
    void updateTexture(BitmapTextureGL&, GstVideoInfo&);
#endif
#if USE(GSTREAMER_GL)
    GRefPtr<GstGLContext> m_glContext;
    GRefPtr<GstGLDisplay> m_glDisplay;
#endif

#if USE(COORDINATED_GRAPHICS_THREADED)
    virtual RefPtr<TextureMapperPlatformLayerProxy> proxy() const override { return m_platformLayerProxy.copyRef(); }
    virtual void swapBuffersIfNeeded() override { };
    void pushTextureToCompositor();
    RefPtr<TextureMapperPlatformLayerProxy> m_platformLayerProxy;
#endif

#if USE(GSTREAMER_GL) || USE(COORDINATED_GRAPHICS_THREADED)
    RefPtr<GraphicsContext3D> m_context3D;
    Condition m_drawCondition;
    Lock m_drawMutex;
#endif
};
}

#endif // USE(GSTREAMER)
#endif
