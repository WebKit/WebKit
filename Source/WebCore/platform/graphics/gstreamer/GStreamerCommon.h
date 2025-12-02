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
#include "GRefPtrGStreamer.h"

#include <gst/audio/audio-buffer.h>
#include <gst/audio/audio-info.h>
#include <gst/gst.h>
#include <gst/video/video-format.h>
#include <gst/video/video-info.h>
#include <wtf/Logger.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/CStringView.h>

namespace WTF {
class MediaTime;
class URL;
}

namespace WebCore {

class FloatSize;
class IntSize;
class SharedBuffer;

struct PlatformVideoColorSpace;

using TrackID = uint64_t;

template<typename MappedArg>
using TrackIDHashMap = HashMap<TrackID, MappedArg, WTF::IntHash<TrackID>, WTF::UnsignedWithZeroKeyHashTraits<TrackID>>;

#define GST_CHECK_VERSION_FULL(major, minor, micro, nano) \
    (GST_CHECK_VERSION(major, minor, micro) && (GST_VERSION_NANO >= nano))

#if !GST_CHECK_VERSION_FULL(1, 27, 2, 1)
inline bool gst_check_version(guint major, guint minor, guint micro)
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
#endif

#define GST_VIDEO_CAPS_TYPE_PREFIX  "video/"_s
#define GST_AUDIO_CAPS_TYPE_PREFIX  "audio/"_s
#define GST_TEXT_CAPS_TYPE_PREFIX   "text/"_s

WARN_UNUSED_RETURN GstPad* webkitGstGhostPadFromStaticTemplate(GstStaticPadTemplate*, CStringView name, GstPad* target);
#if ENABLE(VIDEO)
bool getVideoSizeAndFormatFromCaps(const GstCaps*, WebCore::IntSize&, GstVideoFormat&, int& pixelAspectRatioNumerator, int& pixelAspectRatioDenominator, int& stride, double& frameRate, PlatformVideoColorSpace&);
std::optional<FloatSize> getVideoResolutionFromCaps(const GstCaps*);
bool getSampleVideoInfo(GstSample*, GstVideoInfo&);
std::optional<WebCore::IntSize> getDisplaySize(WebCore::IntSize, int, int);
bool isProtocolAllowed(const WTF::URL&);
#endif
CStringView capsMediaType(const GstCaps*);
std::optional<TrackID> getStreamIdFromPad(const GRefPtr<GstPad>&);
std::optional<TrackID> getStreamIdFromStream(const GRefPtr<GstStream>&);
std::optional<TrackID> parseStreamId(StringView stringId);
bool doCapsHaveType(const GstCaps*, ASCIILiteral);
bool areEncryptedCaps(const GstCaps*);
Vector<String> extractGStreamerOptionsFromCommandLine();
void setGStreamerOptionsFromUIProcess(Vector<String>&&);
bool ensureGStreamerInitialized();
void registerWebKitGStreamerElements();
void registerWebKitGStreamerVideoEncoder();
void deinitializeGStreamer();

unsigned getGstPlayFlag(const char* nick);
uint64_t toGstUnsigned64Time(const WTF::MediaTime&);

inline GstClockTime toGstClockTime(const WTF::MediaTime& mediaTime)
{
    return static_cast<GstClockTime>(toGstUnsigned64Time(mediaTime));
}

GstClockTime toGstClockTime(const Seconds&);
WTF::MediaTime fromGstClockTime(GstClockTime);

template<typename MapType, gboolean(mapFunction)(GstBuffer*, MapType*, GstMapFlags), void(unmapFunction)(GstBuffer*, MapType*)>
class GstBufferMapper {
    WTF_MAKE_NONCOPYABLE(GstBufferMapper);
public:

    GstBufferMapper(GstBufferMapper&& other)
        : m_buffer(other.m_buffer)
        , m_info(other.m_info)
        , m_isValid(other.m_isValid)
    {
        other.m_isValid = false;
    }

    // This GstBuffer is [ transfer none ], meaning that no reference
    // is increased. Hence, this buffer must outlive the mapped
    // buffer.
    GstBufferMapper(GstBuffer* buffer, GstMapFlags flags)
        : m_buffer(buffer)
    {
        ASSERT(GST_IS_BUFFER(buffer));
        m_isValid = mapFunction(m_buffer, &m_info, flags);
    }

    // Unfortunately, GST_MAP_READWRITE is defined out of line from the MapFlags
    // enum as an int, and C++ is careful to not implicity convert it to an enum.
    GstBufferMapper(GstBuffer* buffer, int flags)
        : GstBufferMapper(buffer, static_cast<GstMapFlags>(flags)) { }
    GstBufferMapper(const GRefPtr<GstBuffer>& buffer, GstMapFlags flags)
        : GstBufferMapper(buffer.get(), flags) { }

    virtual ~GstBufferMapper()
    {
        unmapEarly();
    }

    void unmapEarly()
    {
        if (!m_isValid)
            return;

        m_isValid = false;
        unmapFunction(m_buffer, &m_info);
    }

    bool isValid() const { return m_isValid; }
    uint8_t* data() { RELEASE_ASSERT(m_isValid); return static_cast<uint8_t*>(m_info.data); }
    const uint8_t* data() const { RELEASE_ASSERT(m_isValid); return static_cast<uint8_t*>(m_info.data); }
    template<typename T> std::span<T> mutableSpan() { return unsafeMakeSpan(reinterpret_cast<T*>(data()), size() / sizeof(T)); }
    template<typename T> std::span<const T> span() const { return unsafeMakeSpan(reinterpret_cast<const T*>(data()), size() / sizeof(T)); }
    size_t size() const { ASSERT(m_isValid); return m_isValid ? static_cast<size_t>(m_info.size) : 0; }
    MapType* mappedData() const  { ASSERT(m_isValid); return m_isValid ? const_cast<MapType*>(&m_info) : nullptr; }
    Vector<uint8_t> createVector() const;

    explicit operator bool() const { return m_isValid; }
    bool operator!() const { return !m_isValid; }

private:
    friend bool operator==(const GstBufferMapper& a, const GstBufferMapper& b)
    {
        ASSERT(a.isValid());
        ASSERT(b.isValid());
        return a.isValid() && b.isValid() && a.size() == b.size() && !gst_buffer_memcmp(a.m_buffer, 0, b.data(), b.size());
    }
    friend bool operator==(const GstBufferMapper& a, const GstBuffer* b)
    {
        ASSERT(a.isValid());
        ASSERT(GST_IS_BUFFER(b));
        GstBuffer* nonConstB = const_cast<GstBuffer*>(b);
        return a.isValid() && GST_IS_BUFFER(b) && a.size() == gst_buffer_get_size(nonConstB) && !gst_buffer_memcmp(nonConstB, 0, a.data(), a.size());
    }

    GstBuffer* m_buffer { nullptr };
    MapType m_info;
    bool m_isValid { false };
};

using GstMappedBuffer = GstBufferMapper<GstMapInfo, gst_buffer_map, gst_buffer_unmap>;

// This class maps only buffers in GST_MAP_READ mode to be able to
// bump the reference count and keep it alive during the life of this
// object.
class GstMappedOwnedBuffer : public GstMappedBuffer, public ThreadSafeRefCounted<GstMappedOwnedBuffer> {

public:
    static RefPtr<GstMappedOwnedBuffer> create(GRefPtr<GstBuffer>&&);
    static RefPtr<GstMappedOwnedBuffer> create(const GRefPtr<GstBuffer>&);

    // This GstBuffer is [ transfer none ], meaning the reference
    // count is increased during the life of this object.
    static RefPtr<GstMappedOwnedBuffer> create(GstBuffer*);

    virtual ~GstMappedOwnedBuffer();

    Ref<SharedBuffer> createSharedBuffer();

private:
    GstMappedOwnedBuffer(GRefPtr<GstBuffer>&& buffer)
        : GstMappedBuffer(buffer, GST_MAP_READ)
        , m_ownedBuffer(WTFMove(buffer)) { }

    GRefPtr<GstBuffer> m_ownedBuffer;
};

class GstMappedFrame {
    WTF_MAKE_TZONE_ALLOCATED(GstMappedFrame);
    WTF_MAKE_NONCOPYABLE(GstMappedFrame);
public:
    GstMappedFrame(GstBuffer*, const GstVideoInfo*, GstMapFlags);
    GstMappedFrame(const GRefPtr<GstSample>&, GstMapFlags);

    ~GstMappedFrame();

    GstVideoFrame* get();

    std::span<uint8_t> componentData(int) const;
    int componentStride(int) const;
    int componentWidth(int) const;

    GstVideoInfo* info();

    int width() const;
    int height() const;

    int format() const;
    std::span<uint8_t> planeData(uint32_t) const;
    int planeStride(uint32_t) const;

    bool isValid() const { return m_frame.buffer; }
    explicit operator bool() const { return m_frame.buffer; }
    bool operator!() const { return !m_frame.buffer; }

private:
    GstVideoFrame m_frame;
};

class GstMappedAudioBuffer {
    WTF_MAKE_TZONE_ALLOCATED(GstMappedAudioBuffer);
    WTF_MAKE_NONCOPYABLE(GstMappedAudioBuffer);
public:
    GstMappedAudioBuffer(GstBuffer*, GstAudioInfo, GstMapFlags);
    GstMappedAudioBuffer(const GRefPtr<GstSample>&, GstMapFlags);
    ~GstMappedAudioBuffer();

    GstAudioBuffer* get();
    GstAudioInfo* info();

    template<typename T> Vector<std::span<T>> samples(size_t offset) const;

    explicit operator bool() const { return m_isValid; }

private:
    GstAudioBuffer m_buffer;
    bool m_isValid { false };
};

enum class AsynchronousPipelineDumping : bool { No, Yes };
void connectSimpleBusMessageCallback(GstElement*, Function<void(GstMessage*)>&& = [](GstMessage*) { }, AsynchronousPipelineDumping = AsynchronousPipelineDumping::No);
void disconnectSimpleBusMessageCallback(GstElement*);

enum class GstVideoDecoderPlatform { ImxVPU, Video4Linux, OpenMAX };

bool isGStreamerPluginAvailable(ASCIILiteral name);
bool gstElementFactoryEquals(GstElement*, ASCIILiteral name);

GstElement* createAutoAudioSink(const String& role);
GstElement* createPlatformAudioSink(const String& role);

bool webkitGstSetElementStateSynchronously(GstElement*, GstState, Function<bool(GstMessage*)>&& = [](GstMessage*) -> bool {
    return true;
});

GstBuffer* gstBufferNewWrappedFast(void* data, size_t length);

// These functions should be used for elements not provided by WebKit itself and not provided by GStreamer -core.
GstElement* makeGStreamerElement(ASCIILiteral factoryName, const String& name = emptyString());

template<typename T>
std::optional<T> gstStructureGet(const GstStructure*, CStringView key);

CStringView gstStructureGetString(const GstStructure*, CStringView key);

CStringView gstStructureGetName(const GstStructure*);

template<typename T>
Vector<T> gstStructureGetArray(const GstStructure*, CStringView key);

template<typename T>
Vector<T> gstStructureGetList(const GstStructure*, CStringView key);

String gstStructureToJSONString(const GstStructure*);

GstClockTime webkitGstInitTime();

PlatformVideoColorSpace videoColorSpaceFromCaps(const GstCaps*);
PlatformVideoColorSpace videoColorSpaceFromInfo(const GstVideoInfo&);
void fillVideoInfoColorimetryFromColorSpace(GstVideoInfo*, const PlatformVideoColorSpace&);

void configureAudioDecoderForHarnessing(const GRefPtr<GstElement>&);
void configureVideoDecoderForHarnessing(const GRefPtr<GstElement>&);

void configureMediaStreamAudioDecoder(GstElement*);

void configureMediaStreamVideoDecoder(GstElement*);
void configureVideoRTPDepayloader(GstElement*);

bool gstObjectHasProperty(GstObject*, ASCIILiteral name);
bool gstObjectHasProperty(GstElement*, ASCIILiteral name);
bool gstObjectHasProperty(GstPad*, ASCIILiteral name);

bool gstElementMatchesFactoryAndHasProperty(GstElement*, ASCIILiteral factoryNamePattern, ASCIILiteral propertyName);

GRefPtr<GstBuffer> wrapSpanData(const std::span<const uint8_t>&);

std::optional<unsigned> gstGetAutoplugSelectResult(ASCIILiteral);

void registerActivePipeline(const GRefPtr<GstElement>&);
void unregisterPipeline(const GRefPtr<GstElement>&);

class WebCoreLogObserver : public Logger::Observer {
    WTF_MAKE_TZONE_ALLOCATED(WebCoreLogObserver);
    WTF_MAKE_NONCOPYABLE(WebCoreLogObserver);
    friend NeverDestroyed<WebCoreLogObserver>;
public:
    explicit WebCoreLogObserver() = default;
    void didLogMessage(const WTFLogChannel&, WTFLogLevel, Vector<JSONLogValue>&&) final;

    virtual GstDebugCategory* debugCategory() const = 0;
    virtual bool shouldEmitLogMessage(const WTFLogChannel&) const = 0;

    void addWatch(const Logger&);
    void removeWatch(const Logger&);

private:
    Atomic<uint64_t> m_totalObservers;
};

#if GST_CHECK_VERSION(1, 26, 0)
using GstId = const GstIdStr*;
#else
using GstId = GQuark;
#endif

bool gstStructureForeach(const GstStructure*, Function<bool(GstId, const GValue*)>&&);
void gstStructureIdSetValue(GstStructure*, GstId, const GValue*);
bool gstStructureMapInPlace(GstStructure*, Function<bool(GstId, GValue*)>&&);
StringView gstIdToString(GstId);
void gstStructureFilterAndMapInPlace(GstStructure*, Function<bool(GstId, GValue*)>&&);

#if USE(GBM)
WARN_UNUSED_RETURN GRefPtr<GstCaps> buildDMABufCaps();
#endif

#if USE(GSTREAMER_GL)
bool setGstElementGLContext(GstElement*, ASCIILiteral contextType);
#endif

GstStateChangeReturn gstElementLockAndSetState(GstElement*, GstState);

GRefPtr<GstElement> createVideoConvertScaleElement(const String& name = emptyString());

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
    GstIteratorAdaptor(GstIterator* /* transfer full */ iter)
        : m_iter(iter)
    { }

    ~GstIteratorAdaptor()
    {
        gst_iterator_free(m_iter);
    }

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

    private:
        GstIterator* m_iter;
        gboolean m_done;
        T* m_currentValue { nullptr };
    };

    iterator begin()
    {
        ASSERT(!m_started);
        m_started = true;
        iterator iter { m_iter };
        return ++iter;
    }

    iterator end()
    {
        return { m_iter, TRUE };
    }

private:
    GstIterator* m_iter;
    bool m_started { false };
};

#if !GST_CHECK_VERSION(1, 20, 0)
GstBuffer* gst_buffer_new_memdup(gconstpointer data, gsize size);
#endif

#if !GST_CHECK_VERSION_FULL(1, 27, 2, 1) && !GST_CHECK_VERSION(1, 27, 3) && !GST_CHECK_VERSION(1, 28, 0)
void gst_pad_probe_info_set_buffer(GstPadProbeInfo*, GstBuffer*);
void gst_pad_probe_info_set_event(GstPadProbeInfo*, GstEvent*);
#endif

#endif // USE(GSTREAMER)
