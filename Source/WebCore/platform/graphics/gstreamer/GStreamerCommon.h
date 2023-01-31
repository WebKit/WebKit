/*
 *  Copyright (C) 2012, 2015, 2016 Igalia S.L
 *  Copyright (C) 2015, 2016 Metrological Group B.V.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#if USE(GSTREAMER)
#include "FloatSize.h"
#include "GRefPtrGStreamer.h"
#include "GUniquePtrGStreamer.h"
#include "PlatformVideoColorSpace.h"
#include <gst/gst.h>
#include <gst/video/video-format.h>
#include <gst/video/video-info.h>
#include <wtf/MediaTime.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class IntSize;
class SharedBuffer;

inline bool webkitGstCheckVersion(guint major, guint minor, guint micro)
{
    guint currentMajor, currentMinor, currentMicro, currentNano;
    gst_version(&currentMajor, &currentMinor, &currentMicro, &currentNano);

    if (currentMajor < major)
        return false;
    if (currentMajor > major)
        return true;

    if (currentMinor < minor)
        return false;
    if (currentMinor > minor)
        return true;

    if (currentMicro < micro)
        return false;

    return true;
}

// gst_video_format_info_component() is GStreamer 1.18 API, so for older versions we use a local
// vendored copy of the function.
#if !GST_CHECK_VERSION(1, 18, 0)
#define GST_VIDEO_MAX_COMPONENTS 4
void webkitGstVideoFormatInfoComponent(const GstVideoFormatInfo*, guint, gint components[GST_VIDEO_MAX_COMPONENTS]);

#define gst_video_format_info_component webkitGstVideoFormatInfoComponent
#endif

#define GST_VIDEO_CAPS_TYPE_PREFIX  "video/"
#define GST_AUDIO_CAPS_TYPE_PREFIX  "audio/"
#define GST_TEXT_CAPS_TYPE_PREFIX   "text/"

GstPad* webkitGstGhostPadFromStaticTemplate(GstStaticPadTemplate*, const gchar* name, GstPad* target);
#if ENABLE(VIDEO)
bool getVideoSizeAndFormatFromCaps(const GstCaps*, WebCore::IntSize&, GstVideoFormat&, int& pixelAspectRatioNumerator, int& pixelAspectRatioDenominator, int& stride);
std::optional<FloatSize> getVideoResolutionFromCaps(const GstCaps*);
bool getSampleVideoInfo(GstSample*, GstVideoInfo&);
#endif
const char* capsMediaType(const GstCaps*);
bool doCapsHaveType(const GstCaps*, const char*);
bool areEncryptedCaps(const GstCaps*);
Vector<String> extractGStreamerOptionsFromCommandLine();
void setGStreamerOptionsFromUIProcess(Vector<String>&&);
bool ensureGStreamerInitialized();
void registerWebKitGStreamerElements();
void registerWebKitGStreamerVideoEncoder();
unsigned getGstPlayFlag(const char* nick);
uint64_t toGstUnsigned64Time(const MediaTime&);
#if ENABLE(THUNDER)
bool isThunderRanked();
#endif

inline GstClockTime toGstClockTime(const MediaTime& mediaTime)
{
    return static_cast<GstClockTime>(toGstUnsigned64Time(mediaTime));
}

inline GstClockTime toGstClockTime(const Seconds& seconds)
{
    return toGstClockTime(MediaTime::createWithDouble(seconds.seconds()));
}

inline MediaTime fromGstClockTime(GstClockTime time)
{
    if (!GST_CLOCK_TIME_IS_VALID(time))
        return MediaTime::invalidTime();

    return MediaTime(GST_TIME_AS_USECONDS(time), G_USEC_PER_SEC);
}

class GstMappedBuffer {
    WTF_MAKE_NONCOPYABLE(GstMappedBuffer);
public:

    GstMappedBuffer(GstMappedBuffer&& other)
        : m_buffer(other.m_buffer)
        , m_info(other.m_info)
        , m_isValid(other.m_isValid)
    {
        other.m_isValid = false;
    }

    // This GstBuffer is [ transfer none ], meaning that no reference
    // is increased. Hence, this buffer must outlive the mapped
    // buffer.
    GstMappedBuffer(GstBuffer* buffer, GstMapFlags flags)
        : m_buffer(buffer)
    {
        ASSERT(GST_IS_BUFFER(buffer));
        m_isValid = gst_buffer_map(m_buffer, &m_info, flags);
    }

    // Unfortunately, GST_MAP_READWRITE is defined out of line from the MapFlags
    // enum as an int, and C++ is careful to not implicity convert it to an enum.
    GstMappedBuffer(GstBuffer* buffer, int flags)
        : GstMappedBuffer(buffer, static_cast<GstMapFlags>(flags)) { }
    GstMappedBuffer(const GRefPtr<GstBuffer>& buffer, GstMapFlags flags)
        : GstMappedBuffer(buffer.get(), flags) { }

    virtual ~GstMappedBuffer()
    {
        unmapEarly();
    }

    void unmapEarly()
    {
        if (m_isValid) {
            m_isValid = false;
            gst_buffer_unmap(m_buffer, &m_info);
        }
    }

    bool isValid() const { return m_isValid; }
    uint8_t* data() { RELEASE_ASSERT(m_isValid); return static_cast<uint8_t*>(m_info.data); }
    const uint8_t* data() const { RELEASE_ASSERT(m_isValid); return static_cast<uint8_t*>(m_info.data); }
    size_t size() const { ASSERT(m_isValid); return m_isValid ? static_cast<size_t>(m_info.size) : 0; }
    Vector<uint8_t> createVector() const;

    explicit operator bool() const { return m_isValid; }
    bool operator!() const { return !m_isValid; }

private:
    friend bool operator==(const GstMappedBuffer&, const GstMappedBuffer&);
    friend bool operator==(const GstMappedBuffer&, const GstBuffer*);
    friend bool operator==(const GstBuffer* a, const GstMappedBuffer& b) { return operator==(b, a); }

    GstBuffer* m_buffer { nullptr };
    GstMapInfo m_info;
    bool m_isValid { false };
};

// This class maps only buffers in GST_MAP_READ mode to be able to
// bump the reference count and keep it alive during the life of this
// object.
class GstMappedOwnedBuffer : public GstMappedBuffer, public ThreadSafeRefCounted<GstMappedOwnedBuffer> {

public:
    static RefPtr<GstMappedOwnedBuffer> create(GRefPtr<GstBuffer>&& buffer)
    {
        auto* mappedBuffer = new GstMappedOwnedBuffer(WTFMove(buffer));
        if (!mappedBuffer->isValid()) {
            delete mappedBuffer;
            return nullptr;
        }

        return adoptRef(mappedBuffer);
    }

    static RefPtr<GstMappedOwnedBuffer> create(const GRefPtr<GstBuffer>& buffer)
    {
        return create(GRefPtr(buffer));
    }

    // This GstBuffer is [ transfer none ], meaning the reference
    // count is increased during the life of this object.
    static RefPtr<GstMappedOwnedBuffer> create(GstBuffer* buffer)
    {
        return create(GRefPtr(buffer));
    }

    virtual ~GstMappedOwnedBuffer()
    {
        unmapEarly();
    }

    Ref<SharedBuffer> createSharedBuffer();

private:
    GstMappedOwnedBuffer(GRefPtr<GstBuffer>&& buffer)
        : GstMappedBuffer(buffer, GST_MAP_READ)
        , m_ownedBuffer(WTFMove(buffer)) { }

    GRefPtr<GstBuffer> m_ownedBuffer;
};

inline bool operator==(const GstMappedBuffer& a, const GstMappedBuffer& b)
{
    ASSERT(a.isValid());
    ASSERT(b.isValid());
    return a.isValid() && b.isValid() && a.size() == b.size() && !gst_buffer_memcmp(a.m_buffer, 0, b.data(), b.size());
}

inline bool operator==(const GstMappedBuffer& a, const GstBuffer* b)
{
    ASSERT(a.isValid());
    ASSERT(GST_IS_BUFFER(b));
    GstBuffer* nonConstB = const_cast<GstBuffer*>(b);
    return a.isValid() && GST_IS_BUFFER(b) && a.size() == gst_buffer_get_size(nonConstB) && !gst_buffer_memcmp(nonConstB, 0, a.data(), a.size());
}

class GstMappedFrame {
    WTF_MAKE_NONCOPYABLE(GstMappedFrame);
public:

    GstMappedFrame(GstBuffer* buffer, GstVideoInfo info, GstMapFlags flags)
    {
        m_isValid = gst_video_frame_map(&m_frame, &info, buffer, flags);
    }

    GstMappedFrame(GRefPtr<GstSample> sample, GstMapFlags flags)
    {
        GstVideoInfo info;

        if (!gst_video_info_from_caps(&info, gst_sample_get_caps(sample.get()))) {
            m_isValid = false;
            return;
        }

        m_isValid = gst_video_frame_map(&m_frame, &info, gst_sample_get_buffer(sample.get()), flags);
    }

    GstVideoFrame* get()
    {
        if (!m_isValid) {
            GST_INFO("Invalid frame, returning NULL");

            return nullptr;
        }

        return &m_frame;
    }

    uint8_t* ComponentData(int comp)
    {
        return GST_VIDEO_FRAME_COMP_DATA(&m_frame, comp);
    }

    int ComponentStride(int stride)
    {
        return GST_VIDEO_FRAME_COMP_STRIDE(&m_frame, stride);
    }

    GstVideoInfo* info()
    {
        if (!m_isValid) {
            GST_INFO("Invalid frame, returning NULL");

            return nullptr;
        }

        return &m_frame.info;
    }

    int width()
    {
        return m_isValid ? GST_VIDEO_FRAME_WIDTH(&m_frame) : -1;
    }

    int height()
    {
        return m_isValid ? GST_VIDEO_FRAME_HEIGHT(&m_frame) : -1;
    }

    int format()
    {
        return m_isValid ? GST_VIDEO_FRAME_FORMAT(&m_frame) : GST_VIDEO_FORMAT_UNKNOWN;
    }

    ~GstMappedFrame()
    {
        if (m_isValid)
            gst_video_frame_unmap(&m_frame);
        m_isValid = false;
    }

    explicit operator bool() const { return m_isValid; }

private:
    GstVideoFrame m_frame;
    bool m_isValid { false };
};


void connectSimpleBusMessageCallback(GstElement*, Function<void(GstMessage*)>&& = [](GstMessage*) { });
void disconnectSimpleBusMessageCallback(GstElement*);

enum class GstVideoDecoderPlatform { ImxVPU, Video4Linux, OpenMAX };

bool isGStreamerPluginAvailable(const char* name);
bool gstElementFactoryEquals(GstElement*, ASCIILiteral name);

GstElement* createAutoAudioSink(const String& role);
GstElement* createPlatformAudioSink(const String& role);

bool webkitGstSetElementStateSynchronously(GstElement*, GstState, Function<bool(GstMessage*)>&& = [](GstMessage*) -> bool {
    return true;
});

GstBuffer* gstBufferNewWrappedFast(void* data, size_t length);

// These functions should be used for elements not provided by WebKit itself and not provided by GStreamer -core.
GstElement* makeGStreamerElement(const char* factoryName, const char* name);
GstElement* makeGStreamerBin(const char* description, bool ghostUnlinkedPads);

String gstStructureToJSONString(const GstStructure*);

// gst_element_get_current_running_time() is GStreamer 1.18 API, so for older versions we use a local
// vendored copy of the function.
#if !GST_CHECK_VERSION(1, 18, 0)
GstClockTime webkitGstElementGetCurrentRunningTime(GstElement*);
#define gst_element_get_current_running_time webkitGstElementGetCurrentRunningTime
#endif

PlatformVideoColorSpace videoColorSpaceFromCaps(const GstCaps*);
PlatformVideoColorSpace videoColorSpaceFromInfo(const GstVideoInfo&);
void fillVideoInfoColorimetryFromColorSpace(GstVideoInfo*, const PlatformVideoColorSpace&);

void configureVideoDecoderForHarnessing(const GRefPtr<GstElement>&);

bool gstObjectHasProperty(GstElement*, const char* name);
bool gstObjectHasProperty(GstPad*, const char* name);

} // namespace WebCore

#ifndef GST_BUFFER_DTS_OR_PTS
#define GST_BUFFER_DTS_OR_PTS(buffer) (GST_BUFFER_DTS_IS_VALID(buffer) ? GST_BUFFER_DTS(buffer) : GST_BUFFER_PTS(buffer))
#endif

// In GStreamer 1.20 gst_audio_format_fill_silence() will be deprecated in favor of
// gst_audio_format_info_fill_silence().
#if GST_CHECK_VERSION(1, 20, 0)
#define webkitGstAudioFormatFillSilence gst_audio_format_info_fill_silence
#else
#define webkitGstAudioFormatFillSilence gst_audio_format_fill_silence
#endif

// In GStreamer 1.20 gst_element_get_request_pad() was renamed to gst_element_request_pad_simple(),
// so create an alias for older versions.
#if !GST_CHECK_VERSION(1, 20, 0)
#define gst_element_request_pad_simple gst_element_get_request_pad
#endif

// We can't pass macros as template parameters, so we need to wrap them in inline functions.
inline void gstObjectLock(void* object) { GST_OBJECT_LOCK(object); }
inline void gstObjectUnlock(void* object) { GST_OBJECT_UNLOCK(object); }
inline void gstPadStreamLock(GstPad* pad) { GST_PAD_STREAM_LOCK(pad); }
inline void gstPadStreamUnlock(GstPad* pad) { GST_PAD_STREAM_UNLOCK(pad); }
inline void gstStateLock(void* object) { GST_STATE_LOCK(object); }
inline void gstStateUnlock(void* object) { GST_STATE_UNLOCK(object); }

using GstObjectLocker = ExternalLocker<void, gstObjectLock, gstObjectUnlock>;
using GstPadStreamLocker = ExternalLocker<GstPad, gstPadStreamLock, gstPadStreamUnlock>;
using GstStateLocker = ExternalLocker<void, gstStateLock, gstStateUnlock>;

template <typename T>
class GstIteratorAdaptor {
public:
    GstIteratorAdaptor(GUniquePtr<GstIterator>&& iter)
        : m_iter(WTFMove(iter))
    { }

    class iterator {
    public:
        iterator(GstIterator* iter, gboolean done = FALSE)
            : m_iter(iter)
            , m_done(done)
        { }

        T* operator*()
        {
            return m_currentValue;
        }

        iterator& operator++()
        {
            GValue value = G_VALUE_INIT;
            switch (gst_iterator_next(m_iter, &value)) {
            case GST_ITERATOR_OK:
                m_currentValue = static_cast<T*>(g_value_get_object(&value));
                g_value_reset(&value);
                break;
            case GST_ITERATOR_DONE:
                m_done = TRUE;
                m_currentValue = nullptr;
                break;
            default:
                ASSERT_NOT_REACHED_WITH_MESSAGE("Unexpected iterator invalidation");
            }
            return *this;
        }

        bool operator==(const iterator& other) const
        {
            return m_iter == other.m_iter && m_done == other.m_done;
        }
        bool operator!=(const iterator& other) const { return !(*this == other); }

    private:
        GstIterator* m_iter;
        gboolean m_done;
        T* m_currentValue { nullptr };
    };

    iterator begin()
    {
        ASSERT(!m_started);
        m_started = true;
        iterator iter { m_iter.get() };
        return ++iter;
    }

    iterator end()
    {
        return { m_iter.get(), TRUE };
    }

private:
    GUniquePtr<GstIterator> m_iter;
    bool m_started { false };
};

#endif // USE(GSTREAMER)
