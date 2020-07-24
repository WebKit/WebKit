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

#define GST_VIDEO_CAPS_TYPE_PREFIX  "video/"
#define GST_AUDIO_CAPS_TYPE_PREFIX  "audio/"
#define GST_TEXT_CAPS_TYPE_PREFIX   "text/"

GstPad* webkitGstGhostPadFromStaticTemplate(GstStaticPadTemplate*, const gchar* name, GstPad* target);
#if ENABLE(VIDEO)
bool getVideoSizeAndFormatFromCaps(GstCaps*, WebCore::IntSize&, GstVideoFormat&, int& pixelAspectRatioNumerator, int& pixelAspectRatioDenominator, int& stride);
Optional<FloatSize> getVideoResolutionFromCaps(const GstCaps*);
bool getSampleVideoInfo(GstSample*, GstVideoInfo&);
#endif
const char* capsMediaType(const GstCaps*);
bool doCapsHaveType(const GstCaps*, const char*);
bool areEncryptedCaps(const GstCaps*);
Vector<String> extractGStreamerOptionsFromCommandLine();
bool initializeGStreamer(Optional<Vector<String>>&& = WTF::nullopt);
bool initializeGStreamerAndRegisterWebKitElements();
unsigned getGstPlayFlag(const char* nick);
uint64_t toGstUnsigned64Time(const MediaTime&);
#if ENABLE(THUNDER)
bool isThunderRanked();
#endif

inline GstClockTime toGstClockTime(const MediaTime &mediaTime)
{
    return static_cast<GstClockTime>(toGstUnsigned64Time(mediaTime));
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


void connectSimpleBusMessageCallback(GstElement* pipeline);
void disconnectSimpleBusMessageCallback(GstElement* pipeline);

enum class GstVideoDecoderPlatform { ImxVPU, Video4Linux, OpenMAX };

bool isGStreamerPluginAvailable(const char* name);

}

#ifndef GST_BUFFER_DTS_OR_PTS
#define GST_BUFFER_DTS_OR_PTS(buffer) (GST_BUFFER_DTS_IS_VALID(buffer) ? GST_BUFFER_DTS(buffer) : GST_BUFFER_PTS(buffer))
#endif
#endif // USE(GSTREAMER)
