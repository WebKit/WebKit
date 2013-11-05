/*
 * Copyright (C) 2007, 2009 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Collabora Ltd.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009 Gustavo Noronha Silva <gns@gnome.org>
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

#include "config.h"
#include "MediaPlayerPrivateGStreamerBase.h"

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "ColorSpace.h"
#include "GStreamerUtilities.h"
#include "GStreamerVersioning.h"
#include "GraphicsContext.h"
#include "GraphicsTypes.h"
#include "ImageGStreamer.h"
#include "ImageOrientation.h"
#include "IntRect.h"
#include "MediaPlayer.h"
#include "NotImplemented.h"
#include "VideoSinkGStreamer.h"
#include "WebKitWebSourceGStreamer.h"
#include <gst/gst.h>
#include <wtf/text/CString.h>

#ifdef GST_API_VERSION_1
#include <gst/audio/streamvolume.h>
#else
#include <gst/interfaces/streamvolume.h>
#endif

#if GST_CHECK_VERSION(1, 1, 0) && USE(ACCELERATED_COMPOSITING) && USE(TEXTURE_MAPPER_GL)
#include "TextureMapperGL.h"
#endif

GST_DEBUG_CATEGORY(webkit_media_player_debug);
#define GST_CAT_DEFAULT webkit_media_player_debug

using namespace std;

namespace WebCore {

static int greatestCommonDivisor(int a, int b)
{
    while (b) {
        int temp = a;
        a = b;
        b = temp % b;
    }

    return ABS(a);
}

static void mediaPlayerPrivateVolumeChangedCallback(GObject*, GParamSpec*, MediaPlayerPrivateGStreamerBase* player)
{
    // This is called when m_volumeElement receives the notify::volume signal.
    LOG_MEDIA_MESSAGE("Volume changed to: %f", player->volume());
    player->volumeChanged();
}

static gboolean mediaPlayerPrivateVolumeChangeTimeoutCallback(MediaPlayerPrivateGStreamerBase* player)
{
    // This is the callback of the timeout source created in ::volumeChanged.
    player->notifyPlayerOfVolumeChange();
    return FALSE;
}

static void mediaPlayerPrivateMuteChangedCallback(GObject*, GParamSpec*, MediaPlayerPrivateGStreamerBase* player)
{
    // This is called when m_volumeElement receives the notify::mute signal.
    player->muteChanged();
}

static gboolean mediaPlayerPrivateMuteChangeTimeoutCallback(MediaPlayerPrivateGStreamerBase* player)
{
    // This is the callback of the timeout source created in ::muteChanged.
    player->notifyPlayerOfMute();
    return FALSE;
}

static void mediaPlayerPrivateRepaintCallback(WebKitVideoSink*, GstBuffer *buffer, MediaPlayerPrivateGStreamerBase* playerPrivate)
{
    playerPrivate->triggerRepaint(buffer);
}

MediaPlayerPrivateGStreamerBase::MediaPlayerPrivateGStreamerBase(MediaPlayer* player)
    : m_player(player)
    , m_fpsSink(0)
    , m_readyState(MediaPlayer::HaveNothing)
    , m_networkState(MediaPlayer::Empty)
    , m_buffer(0)
    , m_volumeTimerHandler(0)
    , m_muteTimerHandler(0)
    , m_repaintHandler(0)
    , m_volumeSignalHandler(0)
    , m_muteSignalHandler(0)
{
#if GLIB_CHECK_VERSION(2, 31, 0)
    m_bufferMutex = new GMutex;
    g_mutex_init(m_bufferMutex);
#else
    m_bufferMutex = g_mutex_new();
#endif
}

MediaPlayerPrivateGStreamerBase::~MediaPlayerPrivateGStreamerBase()
{
    g_signal_handler_disconnect(m_webkitVideoSink.get(), m_repaintHandler);

#if GLIB_CHECK_VERSION(2, 31, 0)
    g_mutex_clear(m_bufferMutex);
    delete m_bufferMutex;
#else
    g_mutex_free(m_bufferMutex);
#endif

    if (m_buffer)
        gst_buffer_unref(m_buffer);
    m_buffer = 0;

    m_player = 0;

    if (m_muteTimerHandler)
        g_source_remove(m_muteTimerHandler);

    if (m_volumeTimerHandler)
        g_source_remove(m_volumeTimerHandler);

    if (m_volumeSignalHandler) {
        g_signal_handler_disconnect(m_volumeElement.get(), m_volumeSignalHandler);
        m_volumeSignalHandler = 0;
    }

    if (m_muteSignalHandler) {
        g_signal_handler_disconnect(m_volumeElement.get(), m_muteSignalHandler);
        m_muteSignalHandler = 0;
    }
}

// Returns the size of the video
IntSize MediaPlayerPrivateGStreamerBase::naturalSize() const
{
    if (!hasVideo())
        return IntSize();

    if (!m_videoSize.isEmpty())
        return m_videoSize;

#ifdef GST_API_VERSION_1
    GRefPtr<GstCaps> caps = currentVideoSinkCaps();
#else
    g_mutex_lock(m_bufferMutex);
    GRefPtr<GstCaps> caps = m_buffer ? GST_BUFFER_CAPS(m_buffer) : 0;
    g_mutex_unlock(m_bufferMutex);
#endif
    if (!caps)
        return IntSize();


    // TODO: handle possible clean aperture data. See
    // https://bugzilla.gnome.org/show_bug.cgi?id=596571
    // TODO: handle possible transformation matrix. See
    // https://bugzilla.gnome.org/show_bug.cgi?id=596326

    // Get the video PAR and original size, if this fails the
    // video-sink has likely not yet negotiated its caps.
    int pixelAspectRatioNumerator, pixelAspectRatioDenominator, stride;
    IntSize originalSize;
    GstVideoFormat format;
    if (!getVideoSizeAndFormatFromCaps(caps.get(), originalSize, format, pixelAspectRatioNumerator, pixelAspectRatioDenominator, stride))
        return IntSize();

    LOG_MEDIA_MESSAGE("Original video size: %dx%d", originalSize.width(), originalSize.height());
    LOG_MEDIA_MESSAGE("Pixel aspect ratio: %d/%d", pixelAspectRatioNumerator, pixelAspectRatioDenominator);

    // Calculate DAR based on PAR and video size.
    int displayWidth = originalSize.width() * pixelAspectRatioNumerator;
    int displayHeight = originalSize.height() * pixelAspectRatioDenominator;

    // Divide display width and height by their GCD to avoid possible overflows.
    int displayAspectRatioGCD = greatestCommonDivisor(displayWidth, displayHeight);
    displayWidth /= displayAspectRatioGCD;
    displayHeight /= displayAspectRatioGCD;

    // Apply DAR to original video size. This is the same behavior as in xvimagesink's setcaps function.
    guint64 width = 0, height = 0;
    if (!(originalSize.height() % displayHeight)) {
        LOG_MEDIA_MESSAGE("Keeping video original height");
        width = gst_util_uint64_scale_int(originalSize.height(), displayWidth, displayHeight);
        height = static_cast<guint64>(originalSize.height());
    } else if (!(originalSize.width() % displayWidth)) {
        LOG_MEDIA_MESSAGE("Keeping video original width");
        height = gst_util_uint64_scale_int(originalSize.width(), displayHeight, displayWidth);
        width = static_cast<guint64>(originalSize.width());
    } else {
        LOG_MEDIA_MESSAGE("Approximating while keeping original video height");
        width = gst_util_uint64_scale_int(originalSize.height(), displayWidth, displayHeight);
        height = static_cast<guint64>(originalSize.height());
    }

    LOG_MEDIA_MESSAGE("Natural size: %" G_GUINT64_FORMAT "x%" G_GUINT64_FORMAT, width, height);
    m_videoSize = IntSize(static_cast<int>(width), static_cast<int>(height));
    return m_videoSize;
}

void MediaPlayerPrivateGStreamerBase::setVolume(float volume)
{
    if (!m_volumeElement)
        return;

    LOG_MEDIA_MESSAGE("Setting volume: %f", volume);
    gst_stream_volume_set_volume(m_volumeElement.get(), GST_STREAM_VOLUME_FORMAT_CUBIC, static_cast<double>(volume));
}

float MediaPlayerPrivateGStreamerBase::volume() const
{
    if (!m_volumeElement)
        return 0;

    return gst_stream_volume_get_volume(m_volumeElement.get(), GST_STREAM_VOLUME_FORMAT_CUBIC);
}


void MediaPlayerPrivateGStreamerBase::notifyPlayerOfVolumeChange()
{
    m_volumeTimerHandler = 0;

    if (!m_player || !m_volumeElement)
        return;
    double volume;
    volume = gst_stream_volume_get_volume(m_volumeElement.get(), GST_STREAM_VOLUME_FORMAT_CUBIC);
    // get_volume() can return values superior to 1.0 if the user
    // applies software user gain via third party application (GNOME
    // volume control for instance).
    volume = CLAMP(volume, 0.0, 1.0);
    m_player->volumeChanged(static_cast<float>(volume));
}

void MediaPlayerPrivateGStreamerBase::volumeChanged()
{
    if (m_volumeTimerHandler)
        g_source_remove(m_volumeTimerHandler);
    m_volumeTimerHandler = g_idle_add_full(G_PRIORITY_DEFAULT, reinterpret_cast<GSourceFunc>(mediaPlayerPrivateVolumeChangeTimeoutCallback), this, 0);
}

MediaPlayer::NetworkState MediaPlayerPrivateGStreamerBase::networkState() const
{
    return m_networkState;
}

MediaPlayer::ReadyState MediaPlayerPrivateGStreamerBase::readyState() const
{
    return m_readyState;
}

void MediaPlayerPrivateGStreamerBase::sizeChanged()
{
    notImplemented();
}

void MediaPlayerPrivateGStreamerBase::setMuted(bool muted)
{
    if (!m_volumeElement)
        return;

    g_object_set(m_volumeElement.get(), "mute", muted, NULL);
}

bool MediaPlayerPrivateGStreamerBase::muted() const
{
    if (!m_volumeElement)
        return false;

    bool muted;
    g_object_get(m_volumeElement.get(), "mute", &muted, NULL);
    return muted;
}

void MediaPlayerPrivateGStreamerBase::notifyPlayerOfMute()
{
    m_muteTimerHandler = 0;

    if (!m_player || !m_volumeElement)
        return;

    gboolean muted;
    g_object_get(m_volumeElement.get(), "mute", &muted, NULL);
    m_player->muteChanged(static_cast<bool>(muted));
}

void MediaPlayerPrivateGStreamerBase::muteChanged()
{
    if (m_muteTimerHandler)
        g_source_remove(m_muteTimerHandler);
    m_muteTimerHandler = g_idle_add_full(G_PRIORITY_DEFAULT, reinterpret_cast<GSourceFunc>(mediaPlayerPrivateMuteChangeTimeoutCallback), this, 0);
}


#if USE(ACCELERATED_COMPOSITING) && USE(TEXTURE_MAPPER_GL) && !USE(COORDINATED_GRAPHICS)
PassRefPtr<BitmapTexture> MediaPlayerPrivateGStreamerBase::updateTexture(TextureMapper* textureMapper)
{
    g_mutex_lock(m_bufferMutex);
    if (!m_buffer) {
        g_mutex_unlock(m_bufferMutex);
        return 0;
    }

    const void* srcData = 0;
#ifdef GST_API_VERSION_1
    GRefPtr<GstCaps> caps = currentVideoSinkCaps();
#else
    GRefPtr<GstCaps> caps = GST_BUFFER_CAPS(m_buffer);
#endif
    if (!caps) {
        g_mutex_unlock(m_bufferMutex);
        return 0;
    }

    IntSize size;
    GstVideoFormat format;
    int pixelAspectRatioNumerator, pixelAspectRatioDenominator, stride;
    if (!getVideoSizeAndFormatFromCaps(caps.get(), size, format, pixelAspectRatioNumerator, pixelAspectRatioDenominator, stride)) {
        g_mutex_unlock(m_bufferMutex);
        return 0;
    }

    RefPtr<BitmapTexture> texture = textureMapper->acquireTextureFromPool(size);

#if GST_CHECK_VERSION(1, 1, 0)
    GstVideoGLTextureUploadMeta* meta;
    if ((meta = gst_buffer_get_video_gl_texture_upload_meta(m_buffer))) {
        if (meta->n_textures == 1) { // BRGx & BGRA formats use only one texture.
            const BitmapTextureGL* textureGL = static_cast<const BitmapTextureGL*>(texture.get());
            guint ids[4] = { textureGL->id(), 0, 0, 0 };

            if (gst_video_gl_texture_upload_meta_upload(meta, ids)) {
                g_mutex_unlock(m_bufferMutex);
                return texture;
            }
        }
    }
#endif

#ifdef GST_API_VERSION_1
    GstMapInfo srcInfo;
    gst_buffer_map(m_buffer, &srcInfo, GST_MAP_READ);
    srcData = srcInfo.data;
#else
    srcData = GST_BUFFER_DATA(m_buffer);
#endif

    texture->updateContents(srcData, WebCore::IntRect(WebCore::IntPoint(0, 0), size), WebCore::IntPoint(0, 0), stride, BitmapTexture::UpdateCannotModifyOriginalImageData);

#ifdef GST_API_VERSION_1
    gst_buffer_unmap(m_buffer, &srcInfo);
#endif

    g_mutex_unlock(m_bufferMutex);
    return texture;
}
#endif

void MediaPlayerPrivateGStreamerBase::triggerRepaint(GstBuffer* buffer)
{
    g_return_if_fail(GST_IS_BUFFER(buffer));

    g_mutex_lock(m_bufferMutex);
    gst_buffer_replace(&m_buffer, buffer);
    g_mutex_unlock(m_bufferMutex);

#if USE(ACCELERATED_COMPOSITING) && USE(TEXTURE_MAPPER_GL) && !USE(COORDINATED_GRAPHICS)
    if (supportsAcceleratedRendering() && m_player->mediaPlayerClient()->mediaPlayerRenderingCanBeAccelerated(m_player) && client()) {
        client()->setPlatformLayerNeedsDisplay();
        return;
    }
#endif

    m_player->repaint();
}

void MediaPlayerPrivateGStreamerBase::setSize(const IntSize& size)
{
    m_size = size;
}

void MediaPlayerPrivateGStreamerBase::paint(GraphicsContext* context, const IntRect& rect)
{
#if USE(ACCELERATED_COMPOSITING) && USE(TEXTURE_MAPPER_GL) && !USE(COORDINATED_GRAPHICS)
    if (client())
        return;
#endif

    if (context->paintingDisabled())
        return;

    if (!m_player->visible())
        return;

    g_mutex_lock(m_bufferMutex);
    if (!m_buffer) {
        g_mutex_unlock(m_bufferMutex);
        return;
    }

#ifdef GST_API_VERSION_1
    GRefPtr<GstCaps> caps = currentVideoSinkCaps();
#else
    GRefPtr<GstCaps> caps = GST_BUFFER_CAPS(m_buffer);
#endif
    if (!caps) {
        g_mutex_unlock(m_bufferMutex);
        return;
    }

    RefPtr<ImageGStreamer> gstImage = ImageGStreamer::createImage(m_buffer, caps.get());
    if (!gstImage) {
        g_mutex_unlock(m_bufferMutex);
        return;
    }

    context->drawImage(reinterpret_cast<Image*>(gstImage->image().get()), ColorSpaceSRGB,
        rect, gstImage->rect(), CompositeCopy, ImageOrientationDescription(), false);
    g_mutex_unlock(m_bufferMutex);
}

#if USE(ACCELERATED_COMPOSITING) && USE(TEXTURE_MAPPER_GL) && !USE(COORDINATED_GRAPHICS)
void MediaPlayerPrivateGStreamerBase::paintToTextureMapper(TextureMapper* textureMapper, const FloatRect& targetRect, const TransformationMatrix& matrix, float opacity)
{
    if (textureMapper->accelerationMode() != TextureMapper::OpenGLMode)
        return;

    if (!m_player->visible())
        return;

    RefPtr<BitmapTexture> texture = updateTexture(textureMapper);
    if (texture)
        textureMapper->drawTexture(*texture.get(), targetRect, matrix, opacity);
}
#endif

bool MediaPlayerPrivateGStreamerBase::supportsFullscreen() const
{
    return true;
}

PlatformMedia MediaPlayerPrivateGStreamerBase::platformMedia() const
{
    return NoPlatformMedia;
}

MediaPlayer::MovieLoadType MediaPlayerPrivateGStreamerBase::movieLoadType() const
{
    if (m_readyState == MediaPlayer::HaveNothing)
        return MediaPlayer::Unknown;

    if (isLiveStream())
        return MediaPlayer::LiveStream;

    return MediaPlayer::Download;
}

GRefPtr<GstCaps> MediaPlayerPrivateGStreamerBase::currentVideoSinkCaps() const
{
    if (!m_webkitVideoSink)
        return 0;

    GRefPtr<GstCaps> currentCaps;
    g_object_get(G_OBJECT(m_webkitVideoSink.get()), "current-caps", &currentCaps.outPtr(), NULL);
    return currentCaps;
}

void MediaPlayerPrivateGStreamerBase::createVideoSink(GstElement* pipeline)
{
    if (!initializeGStreamer())
        return;

    UNUSED_PARAM(pipeline);
    m_webkitVideoSink = webkitVideoSinkNew();

    m_repaintHandler = g_signal_connect(m_webkitVideoSink.get(), "repaint-requested", G_CALLBACK(mediaPlayerPrivateRepaintCallback), this);

    m_videoSinkBin = gst_bin_new(nullptr);

    GstElement* videoSink = nullptr;
    m_fpsSink = gst_element_factory_make("fpsdisplaysink", "sink");
    if (m_fpsSink) {
        // The verbose property has been added in -bad 0.10.22. Making
        // this whole code depend on it because we don't want
        // fpsdiplaysink to spit data on stdout.
        GstElementFactory* factory = GST_ELEMENT_FACTORY(GST_ELEMENT_GET_CLASS(m_fpsSink)->elementfactory);
        if (gst_plugin_feature_check_version(GST_PLUGIN_FEATURE(factory), 0, 10, 22)) {
            g_object_set(m_fpsSink, "silent", TRUE , nullptr);

            // Turn off text overlay unless logging is enabled.
#if LOG_DISABLED
            g_object_set(m_fpsSink, "text-overlay", FALSE , nullptr);
#else
            WTFLogChannel* channel = logChannelByName("Media");
            if (channel->state != WTFLogChannelOn)
                g_object_set(m_fpsSink, "text-overlay", FALSE , nullptr);
#endif // LOG_DISABLED

            if (g_object_class_find_property(G_OBJECT_GET_CLASS(m_fpsSink), "video-sink")) {
                g_object_set(m_fpsSink, "video-sink", m_webkitVideoSink.get(), nullptr);
                gst_bin_add(GST_BIN(m_videoSinkBin.get()), m_fpsSink);
                videoSink = m_fpsSink;
            } else
                m_fpsSink = nullptr;
        } else
            m_fpsSink = nullptr;
    }

    if (!m_fpsSink) {
        gst_bin_add(GST_BIN(m_videoSinkBin.get()), m_webkitVideoSink.get());
        videoSink = m_webkitVideoSink.get();
    }

    ASSERT(videoSink);

    GstElement* firstChild = videoSink;
    GRefPtr<GstPad> pad = adoptGRef(gst_element_get_static_pad(firstChild, "sink"));
    gst_element_add_pad(m_videoSinkBin.get(), gst_ghost_pad_new("sink", pad.get()));
}

void MediaPlayerPrivateGStreamerBase::setStreamVolumeElement(GstStreamVolume* volume)
{
    ASSERT(!m_volumeElement);
    m_volumeElement = volume;

    // We don't set the initial volume because we trust the sink to keep it for us. See
    // https://bugs.webkit.org/show_bug.cgi?id=118974 for more information.
    if (!m_player->platformVolumeConfigurationRequired()) {
        LOG_MEDIA_MESSAGE("Setting stream volume to %f", m_player->volume());
        g_object_set(m_volumeElement.get(), "volume", m_player->volume(), NULL);
    } else
        LOG_MEDIA_MESSAGE("Not setting stream volume, trusting system one");

    LOG_MEDIA_MESSAGE("Setting stream muted %d",  m_player->muted());
    g_object_set(m_volumeElement.get(), "mute", m_player->muted(), NULL);

    m_volumeSignalHandler = g_signal_connect(m_volumeElement.get(), "notify::volume", G_CALLBACK(mediaPlayerPrivateVolumeChangedCallback), this);
    m_muteSignalHandler = g_signal_connect(m_volumeElement.get(), "notify::mute", G_CALLBACK(mediaPlayerPrivateMuteChangedCallback), this);
}

unsigned MediaPlayerPrivateGStreamerBase::decodedFrameCount() const
{
    guint64 decodedFrames = 0;
    if (m_fpsSink)
        g_object_get(m_fpsSink, "frames-rendered", &decodedFrames, NULL);
    return static_cast<unsigned>(decodedFrames);
}

unsigned MediaPlayerPrivateGStreamerBase::droppedFrameCount() const
{
    guint64 framesDropped = 0;
    if (m_fpsSink)
        g_object_get(m_fpsSink, "frames-dropped", &framesDropped, NULL);
    return static_cast<unsigned>(framesDropped);
}

unsigned MediaPlayerPrivateGStreamerBase::audioDecodedByteCount() const
{
    GstQuery* query = gst_query_new_position(GST_FORMAT_BYTES);
    gint64 position = 0;

    if (audioSink() && gst_element_query(audioSink(), query))
        gst_query_parse_position(query, 0, &position);

    gst_query_unref(query);
    return static_cast<unsigned>(position);
}

unsigned MediaPlayerPrivateGStreamerBase::videoDecodedByteCount() const
{
    GstQuery* query = gst_query_new_position(GST_FORMAT_BYTES);
    gint64 position = 0;

    if (gst_element_query(m_webkitVideoSink.get(), query))
        gst_query_parse_position(query, 0, &position);

    gst_query_unref(query);
    return static_cast<unsigned>(position);
}

}

#endif // USE(GSTREAMER)
