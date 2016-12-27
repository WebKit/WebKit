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
#include "GraphicsContext.h"
#include "GraphicsTypes.h"
#include "ImageGStreamer.h"
#include "ImageOrientation.h"
#include "IntRect.h"
#include "MediaPlayer.h"
#include "NotImplemented.h"
#include "VideoSinkGStreamer.h"
#include "WebKitWebSourceGStreamer.h"
#include <wtf/glib/GMutexLocker.h>
#include <wtf/text/CString.h>
#include <wtf/MathExtras.h>

#include <gst/audio/streamvolume.h>
#include <gst/video/gstvideometa.h>

#if USE(GSTREAMER_GL)
#include <gst/app/gstappsink.h>
#define GST_USE_UNSTABLE_API
#include <gst/gl/gl.h>
#undef GST_USE_UNSTABLE_API

#include "GLContext.h"
#if USE(GLX)
#include "GLContextGLX.h"
#include <gst/gl/x11/gstgldisplay_x11.h>
#elif USE(EGL)
#include "GLContextEGL.h"
#include <gst/gl/egl/gstgldisplay_egl.h>
#endif

#if PLATFORM(X11)
#include "PlatformDisplayX11.h"
#elif PLATFORM(WAYLAND)
#include "PlatformDisplayWayland.h"
#endif

// gstglapi.h may include eglplatform.h and it includes X.h, which
// defines None, breaking MediaPlayer::None enum
#if PLATFORM(X11) && GST_GL_HAVE_PLATFORM_EGL
#undef None
#endif // PLATFORM(X11) && GST_GL_HAVE_PLATFORM_EGL
#endif // USE(GSTREAMER_GL)

#if GST_CHECK_VERSION(1, 1, 0) && USE(TEXTURE_MAPPER_GL)
#include "BitmapTextureGL.h"
#include "BitmapTexturePool.h"
#include "TextureMapperGL.h"
#endif
#if USE(COORDINATED_GRAPHICS_THREADED)
#include "TextureMapperPlatformLayerBuffer.h"
#endif

#if USE(CAIRO) && ENABLE(ACCELERATED_2D_CANVAS)
#include <cairo-gl.h>
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

#if USE(COORDINATED_GRAPHICS_THREADED) && USE(GSTREAMER_GL)
class GstVideoFrameHolder : public TextureMapperPlatformLayerBuffer::UnmanagedBufferDataHolder {
public:
    explicit GstVideoFrameHolder(GstSample* sample, TextureMapperGL::Flags flags)
    {
        GstVideoInfo videoInfo;
        if (UNLIKELY(!getSampleVideoInfo(sample, videoInfo)))
            return;

        m_size = IntSize(GST_VIDEO_INFO_WIDTH(&videoInfo), GST_VIDEO_INFO_HEIGHT(&videoInfo));
        m_flags = flags | (GST_VIDEO_INFO_HAS_ALPHA(&videoInfo) ? TextureMapperGL::ShouldBlend : 0);

        GstBuffer* buffer = gst_sample_get_buffer(sample);
        if (UNLIKELY(!gst_video_frame_map(&m_videoFrame, &videoInfo, buffer, static_cast<GstMapFlags>(GST_MAP_READ | GST_MAP_GL))))
            return;

        m_textureID = *reinterpret_cast<GLuint*>(m_videoFrame.data[0]);
        m_isValid = true;
    }

    virtual ~GstVideoFrameHolder()
    {
        if (UNLIKELY(!m_isValid))
            return;

        gst_video_frame_unmap(&m_videoFrame);
    }

    const IntSize& size() const { return m_size; }
    TextureMapperGL::Flags flags() const { return m_flags; }
    GLuint textureID() const { return m_textureID; }
    bool isValid() const { return m_isValid; }

private:
    GstVideoFrame m_videoFrame;
    IntSize m_size;
    TextureMapperGL::Flags m_flags;
    GLuint m_textureID;
    bool m_isValid { false };
};
#endif // USE(COORDINATED_GRAPHICS_THREADED) && USE(GSTREAMER_GL)

MediaPlayerPrivateGStreamerBase::MediaPlayerPrivateGStreamerBase(MediaPlayer* player)
    : m_player(player)
    , m_fpsSink(0)
    , m_readyState(MediaPlayer::HaveNothing)
    , m_networkState(MediaPlayer::Empty)
#if USE(GSTREAMER_GL) || USE(COORDINATED_GRAPHICS_THREADED)
    , m_drawTimer(RunLoop::main(), this, &MediaPlayerPrivateGStreamerBase::repaint)
#endif
    , m_usingFallbackVideoSink(false)
#if USE(TEXTURE_MAPPER_GL)
    , m_textureMapperRotationFlag(0)
#endif
{
    g_mutex_init(&m_sampleMutex);
#if USE(COORDINATED_GRAPHICS_THREADED)
    m_platformLayerProxy = adoptRef(new TextureMapperPlatformLayerProxy());
#endif
}

MediaPlayerPrivateGStreamerBase::~MediaPlayerPrivateGStreamerBase()
{
    m_notifier.cancelPendingNotifications();

    if (m_videoSink) {
        g_signal_handlers_disconnect_matched(m_videoSink.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
#if USE(GSTREAMER_GL)
        if (GST_IS_BIN(m_videoSink.get())) {
            GRefPtr<GstElement> appsink = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_videoSink.get()), "webkit-gl-video-sink"));
            g_signal_handlers_disconnect_by_data(appsink.get(), this);
        }
#endif
    }

    g_mutex_clear(&m_sampleMutex);

    m_player = nullptr;

    if (m_volumeElement)
        g_signal_handlers_disconnect_matched(m_volumeElement.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);

#if USE(TEXTURE_MAPPER_GL) && !USE(COORDINATED_GRAPHICS)
    if (client())
        client()->platformLayerWillBeDestroyed();
#endif
}

void MediaPlayerPrivateGStreamerBase::setPipeline(GstElement* pipeline)
{
    m_pipeline = pipeline;
}

bool MediaPlayerPrivateGStreamerBase::handleSyncMessage(GstMessage* message)
{
#if USE(GSTREAMER_GL)
    if (GST_MESSAGE_TYPE(message) != GST_MESSAGE_NEED_CONTEXT)
        return false;

    const gchar* contextType;
    gst_message_parse_context_type(message, &contextType);

    if (!ensureGstGLContext())
        return false;

    if (!g_strcmp0(contextType, GST_GL_DISPLAY_CONTEXT_TYPE)) {
        GRefPtr<GstContext> displayContext = adoptGRef(gst_context_new(GST_GL_DISPLAY_CONTEXT_TYPE, TRUE));
        gst_context_set_gl_display(displayContext.get(), m_glDisplay.get());
        gst_element_set_context(GST_ELEMENT(message->src), displayContext.get());
        return true;
    }

    if (!g_strcmp0(contextType, "gst.gl.app_context")) {
        GRefPtr<GstContext> appContext = adoptGRef(gst_context_new("gst.gl.app_context", TRUE));
        GstStructure* structure = gst_context_writable_structure(appContext.get());
        gst_structure_set(structure, "context", GST_GL_TYPE_CONTEXT, m_glContext.get(), nullptr);
        gst_element_set_context(GST_ELEMENT(message->src), appContext.get());
        return true;
    }
#else
    UNUSED_PARAM(message);
#endif // USE(GSTREAMER_GL)

    return false;
}

#if USE(GSTREAMER_GL)
bool MediaPlayerPrivateGStreamerBase::ensureGstGLContext()
{
    if (m_glContext)
        return true;

    const auto& sharedDisplay = PlatformDisplay::sharedDisplayForCompositing();
    if (!m_glDisplay) {
#if PLATFORM(X11)
        m_glDisplay = GST_GL_DISPLAY(gst_gl_display_x11_new_with_display(downcast<PlatformDisplayX11>(sharedDisplay).native()));
#elif PLATFORM(WAYLAND)
        m_glDisplay = GST_GL_DISPLAY(gst_gl_display_egl_new_with_egl_display(downcast<PlatformDisplayWayland>(sharedDisplay).native()));
#endif
    }

    GLContext* webkitContext = sharedDisplay.sharingGLContext();
    // EGL and GLX are mutually exclusive, no need for ifdefs here.
    GstGLPlatform glPlatform = webkitContext->isEGLContext() ? GST_GL_PLATFORM_EGL : GST_GL_PLATFORM_GLX;

#if USE(OPENGL_ES_2)
    GstGLAPI glAPI = GST_GL_API_GLES2;
#elif USE(OPENGL)
    GstGLAPI glAPI = GST_GL_API_OPENGL;
#else
    ASSERT_NOT_REACHED();
#endif

    PlatformGraphicsContext3D contextHandle = webkitContext->platformContext();
    if (!contextHandle)
        return false;

    m_glContext = gst_gl_context_new_wrapped(m_glDisplay.get(), reinterpret_cast<guintptr>(contextHandle), glPlatform, glAPI);

    return true;
}
#endif // USE(GSTREAMER_GL)

// Returns the size of the video
FloatSize MediaPlayerPrivateGStreamerBase::naturalSize() const
{
    if (!hasVideo())
        return FloatSize();

    if (!m_videoSize.isEmpty())
        return m_videoSize;

    WTF::GMutexLocker<GMutex> lock(m_sampleMutex);
    if (!GST_IS_SAMPLE(m_sample.get()))
        return FloatSize();

    GstCaps* caps = gst_sample_get_caps(m_sample.get());
    if (!caps)
        return FloatSize();


    // TODO: handle possible clean aperture data. See
    // https://bugzilla.gnome.org/show_bug.cgi?id=596571
    // TODO: handle possible transformation matrix. See
    // https://bugzilla.gnome.org/show_bug.cgi?id=596326

    // Get the video PAR and original size, if this fails the
    // video-sink has likely not yet negotiated its caps.
    int pixelAspectRatioNumerator, pixelAspectRatioDenominator, stride;
    IntSize originalSize;
    GstVideoFormat format;
    if (!getVideoSizeAndFormatFromCaps(caps, originalSize, format, pixelAspectRatioNumerator, pixelAspectRatioDenominator, stride))
        return FloatSize();

#if USE(TEXTURE_MAPPER_GL)
    // When using accelerated compositing, if the video is tagged as rotated 90 or 270 degrees, swap width and height.
    if (m_player->client().mediaPlayerRenderingCanBeAccelerated(m_player)) {
        if (m_videoSourceOrientation.usesWidthAsHeight())
            originalSize = originalSize.transposedSize();
    }
#endif

    GST_DEBUG("Original video size: %dx%d", originalSize.width(), originalSize.height());
    GST_DEBUG("Pixel aspect ratio: %d/%d", pixelAspectRatioNumerator, pixelAspectRatioDenominator);

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
        GST_DEBUG("Keeping video original height");
        width = gst_util_uint64_scale_int(originalSize.height(), displayWidth, displayHeight);
        height = static_cast<guint64>(originalSize.height());
    } else if (!(originalSize.width() % displayWidth)) {
        GST_DEBUG("Keeping video original width");
        height = gst_util_uint64_scale_int(originalSize.width(), displayHeight, displayWidth);
        width = static_cast<guint64>(originalSize.width());
    } else {
        GST_DEBUG("Approximating while keeping original video height");
        width = gst_util_uint64_scale_int(originalSize.height(), displayWidth, displayHeight);
        height = static_cast<guint64>(originalSize.height());
    }

    GST_DEBUG("Natural size: %" G_GUINT64_FORMAT "x%" G_GUINT64_FORMAT, width, height);
    m_videoSize = FloatSize(static_cast<int>(width), static_cast<int>(height));
    return m_videoSize;
}

void MediaPlayerPrivateGStreamerBase::setVolume(float volume)
{
    if (!m_volumeElement)
        return;

    GST_DEBUG("Setting volume: %f", volume);
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

void MediaPlayerPrivateGStreamerBase::volumeChangedCallback(MediaPlayerPrivateGStreamerBase* player)
{
    // This is called when m_volumeElement receives the notify::volume signal.
    GST_DEBUG("Volume changed to: %f", player->volume());

    player->m_notifier.notify(MainThreadNotification::VolumeChanged, [player] { player->notifyPlayerOfVolumeChange(); });
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
    if (!m_player || !m_volumeElement)
        return;

    gboolean muted;
    g_object_get(m_volumeElement.get(), "mute", &muted, NULL);
    m_player->muteChanged(static_cast<bool>(muted));
}

void MediaPlayerPrivateGStreamerBase::muteChangedCallback(MediaPlayerPrivateGStreamerBase* player)
{
    // This is called when m_volumeElement receives the notify::mute signal.
    player->m_notifier.notify(MainThreadNotification::MuteChanged, [player] { player->notifyPlayerOfMute(); });
}

#if USE(TEXTURE_MAPPER_GL) && !USE(COORDINATED_GRAPHICS_MULTIPROCESS)
void MediaPlayerPrivateGStreamerBase::updateTexture(BitmapTextureGL& texture, GstVideoInfo& videoInfo)
{
    GstBuffer* buffer = gst_sample_get_buffer(m_sample.get());

#if GST_CHECK_VERSION(1, 1, 0)
    GstVideoGLTextureUploadMeta* meta;
    if ((meta = gst_buffer_get_video_gl_texture_upload_meta(buffer))) {
        if (meta->n_textures == 1) { // BRGx & BGRA formats use only one texture.
            guint ids[4] = { texture.id(), 0, 0, 0 };

            if (gst_video_gl_texture_upload_meta_upload(meta, ids))
                return;
        }
    }
#endif

    // Right now the TextureMapper only supports chromas with one plane
    ASSERT(GST_VIDEO_INFO_N_PLANES(&videoInfo) == 1);

    GstVideoFrame videoFrame;
    if (!gst_video_frame_map(&videoFrame, &videoInfo, buffer, GST_MAP_READ))
        return;

    int stride = GST_VIDEO_FRAME_PLANE_STRIDE(&videoFrame, 0);
    const void* srcData = GST_VIDEO_FRAME_PLANE_DATA(&videoFrame, 0);
    texture.updateContents(srcData, WebCore::IntRect(0, 0, GST_VIDEO_INFO_WIDTH(&videoInfo), GST_VIDEO_INFO_HEIGHT(&videoInfo)), WebCore::IntPoint(0, 0), stride, BitmapTexture::UpdateCannotModifyOriginalImageData);
    gst_video_frame_unmap(&videoFrame);
}
#endif

#if USE(COORDINATED_GRAPHICS_THREADED)
void MediaPlayerPrivateGStreamerBase::pushTextureToCompositor()
{
#if !USE(GSTREAMER_GL)
    class ConditionNotifier {
    public:
        ConditionNotifier(Lock& lock, Condition& condition)
            : m_locker(lock), m_condition(condition)
        {
        }
        ~ConditionNotifier()
        {
            m_condition.notifyOne();
        }
    private:
        LockHolder m_locker;
        Condition& m_condition;
    };
    ConditionNotifier notifier(m_drawMutex, m_drawCondition);
#endif

    WTF::GMutexLocker<GMutex> lock(m_sampleMutex);
    if (!GST_IS_SAMPLE(m_sample.get()))
        return;

    LockHolder holder(m_platformLayerProxy->lock());

    if (!m_platformLayerProxy->isActive())
        return;

#if USE(GSTREAMER_GL)
    std::unique_ptr<GstVideoFrameHolder> frameHolder = std::make_unique<GstVideoFrameHolder>(m_sample.get(), m_textureMapperRotationFlag);
    if (UNLIKELY(!frameHolder->isValid()))
        return;

    std::unique_ptr<TextureMapperPlatformLayerBuffer> layerBuffer = std::make_unique<TextureMapperPlatformLayerBuffer>(frameHolder->textureID(), frameHolder->size(), frameHolder->flags());
    layerBuffer->setUnmanagedBufferDataHolder(WTFMove(frameHolder));
    m_platformLayerProxy->pushNextBuffer(WTFMove(layerBuffer));
#else
    GstVideoInfo videoInfo;
    if (UNLIKELY(!getSampleVideoInfo(m_sample.get(), videoInfo)))
        return;

    IntSize size = IntSize(GST_VIDEO_INFO_WIDTH(&videoInfo), GST_VIDEO_INFO_HEIGHT(&videoInfo));
    std::unique_ptr<TextureMapperPlatformLayerBuffer> buffer = m_platformLayerProxy->getAvailableBuffer(size, GraphicsContext3D::DONT_CARE);
    if (UNLIKELY(!buffer)) {
        if (UNLIKELY(!m_context3D))
            m_context3D = GraphicsContext3D::create(GraphicsContext3D::Attributes(), nullptr, GraphicsContext3D::RenderToCurrentGLContext);

        RefPtr<BitmapTexture> texture = adoptRef(new BitmapTextureGL(m_context3D));
        texture->reset(size, GST_VIDEO_INFO_HAS_ALPHA(&videoInfo) ? BitmapTexture::SupportsAlpha : BitmapTexture::NoFlag);
        buffer = std::make_unique<TextureMapperPlatformLayerBuffer>(WTFMove(texture));
    }
    updateTexture(buffer->textureGL(), videoInfo);
    buffer->setExtraFlags(m_textureMapperRotationFlag | (GST_VIDEO_INFO_HAS_ALPHA(&videoInfo) ? TextureMapperGL::ShouldBlend : 0));
    m_platformLayerProxy->pushNextBuffer(WTFMove(buffer));
#endif
}
#endif

void MediaPlayerPrivateGStreamerBase::repaint()
{
    ASSERT(m_sample);
    ASSERT(isMainThread());

#if USE(TEXTURE_MAPPER_GL) && !USE(COORDINATED_GRAPHICS)
    if (supportsAcceleratedRendering() && m_player->client().mediaPlayerRenderingCanBeAccelerated(m_player) && client()) {
        client()->setPlatformLayerNeedsDisplay();
#if USE(GSTREAMER_GL)
        m_drawCondition.notifyOne();
#endif
        return;
    }
#endif

    m_player->repaint();

#if USE(GSTREAMER_GL) || USE(COORDINATED_GRAPHICS_THREADED)
    m_drawCondition.notifyOne();
#endif
}

void MediaPlayerPrivateGStreamerBase::triggerRepaint(GstSample* sample)
{
    bool triggerResize;
    {
        WTF::GMutexLocker<GMutex> lock(m_sampleMutex);
        triggerResize = !m_sample;
        m_sample = sample;
    }

    if (triggerResize) {
        GST_DEBUG("First sample reached the sink, triggering video dimensions update");
        m_notifier.notify(MainThreadNotification::SizeChanged, [this] { m_player->sizeChanged(); });
    }

#if USE(COORDINATED_GRAPHICS_THREADED)
    if (!m_player->client().mediaPlayerAcceleratedCompositingEnabled()) {
        LockHolder locker(m_drawMutex);
        m_drawTimer.startOneShot(0);
        m_drawCondition.wait(m_drawMutex);
        return;
    }

#if USE(GSTREAMER_GL)
    pushTextureToCompositor();
#else
    {
        LockHolder lock(m_drawMutex);
        if (!m_platformLayerProxy->scheduleUpdateOnCompositorThread([this] { this->pushTextureToCompositor(); }))
            return;
        m_drawCondition.wait(m_drawMutex);
    }
#endif
    return;
#else
#if USE(GSTREAMER_GL)
    {
        ASSERT(!isMainThread());

        LockHolder locker(m_drawMutex);
        m_drawTimer.startOneShot(0);
        m_drawCondition.wait(m_drawMutex);
    }
#else
    repaint();
#endif
#endif
}

void MediaPlayerPrivateGStreamerBase::repaintCallback(MediaPlayerPrivateGStreamerBase* player, GstSample* sample)
{
    player->triggerRepaint(sample);
}

#if USE(GSTREAMER_GL)
GstFlowReturn MediaPlayerPrivateGStreamerBase::newSampleCallback(GstElement* sink, MediaPlayerPrivateGStreamerBase* player)
{
    GRefPtr<GstSample> sample = adoptGRef(gst_app_sink_pull_sample(GST_APP_SINK(sink)));
    player->triggerRepaint(sample.get());
    return GST_FLOW_OK;
}

GstFlowReturn MediaPlayerPrivateGStreamerBase::newPrerollCallback(GstElement* sink, MediaPlayerPrivateGStreamerBase* player)
{
    GRefPtr<GstSample> sample = adoptGRef(gst_app_sink_pull_preroll(GST_APP_SINK(sink)));
    player->triggerRepaint(sample.get());
    return GST_FLOW_OK;
}
#endif

void MediaPlayerPrivateGStreamerBase::setSize(const IntSize& size)
{
    m_size = size;
}

void MediaPlayerPrivateGStreamerBase::paint(GraphicsContext& context, const FloatRect& rect)
{
    if (context.paintingDisabled())
        return;

    if (!m_player->visible())
        return;

    WTF::GMutexLocker<GMutex> lock(m_sampleMutex);
    if (!GST_IS_SAMPLE(m_sample.get()))
        return;

    ImagePaintingOptions paintingOptions(CompositeCopy);
    if (m_player->client().mediaPlayerRenderingCanBeAccelerated(m_player))
        paintingOptions.m_orientationDescription.setImageOrientationEnum(m_videoSourceOrientation);

    RefPtr<ImageGStreamer> gstImage = ImageGStreamer::createImage(m_sample.get());
    if (!gstImage)
        return;

    if (Image* image = reinterpret_cast<Image*>(gstImage->image().get()))
        context.drawImage(*image, rect, gstImage->rect(), paintingOptions);
}

#if USE(TEXTURE_MAPPER_GL) && !USE(COORDINATED_GRAPHICS)
void MediaPlayerPrivateGStreamerBase::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& matrix, float opacity)
{
    if (!m_player->visible())
        return;

    if (m_usingFallbackVideoSink) {
        RefPtr<BitmapTexture> texture;
        IntSize size;
        TextureMapperGL::Flags flags;
        {
            WTF::GMutexLocker<GMutex> lock(m_sampleMutex);

            GstVideoInfo videoInfo;
            if (UNLIKELY(!getSampleVideoInfo(m_sample.get(), videoInfo)))
                return;

            size = IntSize(GST_VIDEO_INFO_WIDTH(&videoInfo), GST_VIDEO_INFO_HEIGHT(&videoInfo));
            flags = m_textureMapperRotationFlag | (GST_VIDEO_INFO_HAS_ALPHA(&videoInfo) ? TextureMapperGL::ShouldBlend : 0);
            texture = textureMapper.acquireTextureFromPool(size, GST_VIDEO_INFO_HAS_ALPHA(&videoInfo) ? BitmapTexture::SupportsAlpha : BitmapTexture::NoFlag);
            updateTexture(static_cast<BitmapTextureGL&>(*texture), videoInfo);
        }
        TextureMapperGL& texmapGL = reinterpret_cast<TextureMapperGL&>(textureMapper);
        BitmapTextureGL* textureGL = static_cast<BitmapTextureGL*>(texture.get());
        texmapGL.drawTexture(textureGL->id(), flags, textureGL->size(), targetRect, matrix, opacity);
        return;
    }

#if USE(GSTREAMER_GL)
    WTF::GMutexLocker<GMutex> lock(m_sampleMutex);

    GstVideoInfo videoInfo;
    if (!getSampleVideoInfo(m_sample.get(), videoInfo))
        return;

    GstBuffer* buffer = gst_sample_get_buffer(m_sample.get());
    GstVideoFrame videoFrame;
    if (!gst_video_frame_map(&videoFrame, &videoInfo, buffer, static_cast<GstMapFlags>(GST_MAP_READ | GST_MAP_GL)))
        return;

    unsigned textureID = *reinterpret_cast<unsigned*>(videoFrame.data[0]);
    TextureMapperGL::Flags flags = m_textureMapperRotationFlag | (GST_VIDEO_INFO_HAS_ALPHA(&videoInfo) ? TextureMapperGL::ShouldBlend : 0);

    IntSize size = IntSize(GST_VIDEO_INFO_WIDTH(&videoInfo), GST_VIDEO_INFO_HEIGHT(&videoInfo));
    TextureMapperGL& textureMapperGL = reinterpret_cast<TextureMapperGL&>(textureMapper);
    textureMapperGL.drawTexture(textureID, flags, size, targetRect, matrix, opacity);
    gst_video_frame_unmap(&videoFrame);
#endif
}
#endif

#if USE(GSTREAMER_GL)
NativeImagePtr MediaPlayerPrivateGStreamerBase::nativeImageForCurrentTime()
{
#if USE(CAIRO) && ENABLE(ACCELERATED_2D_CANVAS)
    if (m_usingFallbackVideoSink)
        return nullptr;

    WTF::GMutexLocker<GMutex> lock(m_sampleMutex);

    GstVideoInfo videoInfo;
    if (!getSampleVideoInfo(m_sample.get(), videoInfo))
        return nullptr;

    GstBuffer* buffer = gst_sample_get_buffer(m_sample.get());
    GstVideoFrame videoFrame;
    if (!gst_video_frame_map(&videoFrame, &videoInfo, buffer, static_cast<GstMapFlags>(GST_MAP_READ | GST_MAP_GL)))
        return nullptr;

    GLContext* context = PlatformDisplay::sharedDisplayForCompositing().sharingGLContext();
    context->makeContextCurrent();
    cairo_device_t* device = context->cairoDevice();

    // Thread-awareness is a huge performance hit on non-Intel drivers.
    cairo_gl_device_set_thread_aware(device, FALSE);

    unsigned textureID = *reinterpret_cast<unsigned*>(videoFrame.data[0]);
    IntSize size = IntSize(GST_VIDEO_INFO_WIDTH(&videoInfo), GST_VIDEO_INFO_HEIGHT(&videoInfo));
    RefPtr<cairo_surface_t> surface = adoptRef(cairo_gl_surface_create_for_texture(device, CAIRO_CONTENT_COLOR_ALPHA, textureID, size.width(), size.height()));

    IntSize rotatedSize = m_videoSourceOrientation.usesWidthAsHeight() ? size.transposedSize() : size;
    RefPtr<cairo_surface_t> rotatedSurface = adoptRef(cairo_gl_surface_create(device, CAIRO_CONTENT_COLOR_ALPHA, rotatedSize.width(), rotatedSize.height()));
    RefPtr<cairo_t> cr = adoptRef(cairo_create(rotatedSurface.get()));

    switch (m_videoSourceOrientation) {
    case DefaultImageOrientation:
        break;
    case OriginRightTop:
        cairo_translate(cr.get(), rotatedSize.width() * 0.5, rotatedSize.height() * 0.5);
        cairo_rotate(cr.get(), piOverTwoDouble);
        cairo_translate(cr.get(), -rotatedSize.height() * 0.5, -rotatedSize.width() * 0.5);
        break;
    case OriginBottomRight:
        cairo_translate(cr.get(), rotatedSize.width() * 0.5, rotatedSize.height() * 0.5);
        cairo_rotate(cr.get(), piDouble);
        cairo_translate(cr.get(), -rotatedSize.width() * 0.5, -rotatedSize.height() * 0.5);
        break;
    case OriginLeftBottom:
        cairo_translate(cr.get(), rotatedSize.width() * 0.5, rotatedSize.height() * 0.5);
        cairo_rotate(cr.get(), 3 * piOverTwoDouble);
        cairo_translate(cr.get(), -rotatedSize.height() * 0.5, -rotatedSize.width() * 0.5);
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    cairo_set_source_surface(cr.get(), surface.get(), 0, 0);
    cairo_set_operator(cr.get(), CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr.get());

    gst_video_frame_unmap(&videoFrame);

    return rotatedSurface;
#else
    return nullptr;
#endif
}
#endif

void MediaPlayerPrivateGStreamerBase::setVideoSourceOrientation(const ImageOrientation& orientation)
{
    if (m_videoSourceOrientation == orientation)
        return;

    m_videoSourceOrientation = orientation;

#if USE(TEXTURE_MAPPER_GL)
    switch (m_videoSourceOrientation) {
    case DefaultImageOrientation:
        m_textureMapperRotationFlag = 0;
        break;
    case OriginRightTop:
        m_textureMapperRotationFlag = TextureMapperGL::ShouldRotateTexture90;
        break;
    case OriginBottomRight:
        m_textureMapperRotationFlag = TextureMapperGL::ShouldRotateTexture180;
        break;
    case OriginLeftBottom:
        m_textureMapperRotationFlag = TextureMapperGL::ShouldRotateTexture270;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
#endif
}

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

#if USE(GSTREAMER_GL)
GstElement* MediaPlayerPrivateGStreamerBase::createGLAppSink()
{
    if (!webkitGstCheckVersion(1, 8, 0))
        return nullptr;

    GstElement* appsink = gst_element_factory_make("appsink", "webkit-gl-video-sink");
    if (!appsink)
        return nullptr;

    g_object_set(appsink, "enable-last-sample", FALSE, "emit-signals", TRUE, "max-buffers", 1, nullptr);
    g_signal_connect(appsink, "new-sample", G_CALLBACK(newSampleCallback), this);
    g_signal_connect(appsink, "new-preroll", G_CALLBACK(newPrerollCallback), this);

    return appsink;
}

GstElement* MediaPlayerPrivateGStreamerBase::createVideoSinkGL()
{
    // FIXME: Currently it's not possible to get the video frames and caps using this approach until
    // the pipeline gets into playing state. Due to this, trying to grab a frame and painting it by some
    // other mean (canvas or webgl) before playing state can result in a crash.
    // This is being handled in https://bugs.webkit.org/show_bug.cgi?id=159460.
    if (!webkitGstCheckVersion(1, 8, 0))
        return nullptr;

    gboolean result = TRUE;
    GstElement* videoSink = gst_bin_new(nullptr);
    GstElement* upload = gst_element_factory_make("glupload", nullptr);
    GstElement* colorconvert = gst_element_factory_make("glcolorconvert", nullptr);
    GstElement* appsink = createGLAppSink();

    if (!appsink || !upload || !colorconvert) {
        GST_WARNING("Failed to create GstGL elements");
        gst_object_unref(videoSink);

        if (upload)
            gst_object_unref(upload);
        if (colorconvert)
            gst_object_unref(colorconvert);
        if (appsink)
            gst_object_unref(appsink);

        return nullptr;
    }

    gst_bin_add_many(GST_BIN(videoSink), upload, colorconvert, appsink, nullptr);

    GRefPtr<GstCaps> caps = adoptGRef(gst_caps_from_string("video/x-raw(" GST_CAPS_FEATURE_MEMORY_GL_MEMORY "), format = (string) { RGBA }"));

    result &= gst_element_link_pads(upload, "src", colorconvert, "sink");
    result &= gst_element_link_pads_filtered(colorconvert, "src", appsink, "sink", caps.get());

    GRefPtr<GstPad> pad = adoptGRef(gst_element_get_static_pad(upload, "sink"));
    gst_element_add_pad(videoSink, gst_ghost_pad_new("sink", pad.get()));

    if (!result) {
        GST_WARNING("Failed to link GstGL elements");
        gst_object_unref(videoSink);
        videoSink = nullptr;
    }
    return videoSink;
}
#endif

GstElement* MediaPlayerPrivateGStreamerBase::createVideoSink()
{
#if USE(GSTREAMER_GL)
    m_videoSink = createVideoSinkGL();
#endif

    if (!m_videoSink) {
        m_usingFallbackVideoSink = true;
        m_videoSink = webkitVideoSinkNew();
        g_signal_connect_swapped(m_videoSink.get(), "repaint-requested", G_CALLBACK(repaintCallback), this);
    }

    GstElement* videoSink = nullptr;
    m_fpsSink = gst_element_factory_make("fpsdisplaysink", "sink");
    if (m_fpsSink) {
        g_object_set(m_fpsSink.get(), "silent", TRUE , nullptr);

        // Turn off text overlay unless logging is enabled.
#if LOG_DISABLED
        g_object_set(m_fpsSink.get(), "text-overlay", FALSE , nullptr);
#else
        if (!isLogChannelEnabled("Media"))
            g_object_set(m_fpsSink.get(), "text-overlay", FALSE , nullptr);
#endif // LOG_DISABLED

        if (g_object_class_find_property(G_OBJECT_GET_CLASS(m_fpsSink.get()), "video-sink")) {
            g_object_set(m_fpsSink.get(), "video-sink", m_videoSink.get(), nullptr);
            videoSink = m_fpsSink.get();
        } else
            m_fpsSink = nullptr;
    }

    if (!m_fpsSink)
        videoSink = m_videoSink.get();

    ASSERT(videoSink);

    return videoSink;
}

void MediaPlayerPrivateGStreamerBase::setStreamVolumeElement(GstStreamVolume* volume)
{
    ASSERT(!m_volumeElement);
    m_volumeElement = volume;

    // We don't set the initial volume because we trust the sink to keep it for us. See
    // https://bugs.webkit.org/show_bug.cgi?id=118974 for more information.
    if (!m_player->platformVolumeConfigurationRequired()) {
        GST_DEBUG("Setting stream volume to %f", m_player->volume());
        g_object_set(m_volumeElement.get(), "volume", m_player->volume(), NULL);
    } else
        GST_DEBUG("Not setting stream volume, trusting system one");

    GST_DEBUG("Setting stream muted %d",  m_player->muted());
    g_object_set(m_volumeElement.get(), "mute", m_player->muted(), NULL);

    g_signal_connect_swapped(m_volumeElement.get(), "notify::volume", G_CALLBACK(volumeChangedCallback), this);
    g_signal_connect_swapped(m_volumeElement.get(), "notify::mute", G_CALLBACK(muteChangedCallback), this);
}

unsigned MediaPlayerPrivateGStreamerBase::decodedFrameCount() const
{
    guint64 decodedFrames = 0;
    if (m_fpsSink)
        g_object_get(m_fpsSink.get(), "frames-rendered", &decodedFrames, NULL);
    return static_cast<unsigned>(decodedFrames);
}

unsigned MediaPlayerPrivateGStreamerBase::droppedFrameCount() const
{
    guint64 framesDropped = 0;
    if (m_fpsSink)
        g_object_get(m_fpsSink.get(), "frames-dropped", &framesDropped, NULL);
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

    if (gst_element_query(m_videoSink.get(), query))
        gst_query_parse_position(query, 0, &position);

    gst_query_unref(query);
    return static_cast<unsigned>(position);
}

}

#endif // USE(GSTREAMER)
