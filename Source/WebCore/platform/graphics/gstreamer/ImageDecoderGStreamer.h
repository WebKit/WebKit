/*
 * Copyright (C) 2020 Igalia S.L
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

#pragma once

#if USE(GSTREAMER) && ENABLE(VIDEO)

#include "GRefPtrGStreamer.h"
#include "ImageDecoder.h"
#include "MIMETypeRegistry.h"
#include "SampleMap.h"
#include "SharedBuffer.h"
#include <wtf/Forward.h>
#include <wtf/RunLoop.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class ContentType;
class ImageDecoderGStreamerSample;

class ImageDecoderGStreamer final : public ImageDecoder {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(ImageDecoderGStreamer);
public:
    static RefPtr<ImageDecoderGStreamer> create(SharedBuffer&, const String& mimeType, AlphaOption, GammaAndColorProfileOption);
    ImageDecoderGStreamer(SharedBuffer&, const String& mimeType, AlphaOption, GammaAndColorProfileOption);
    virtual ~ImageDecoderGStreamer() = default;

    static bool supportsMediaType(MediaType type) { return type == MediaType::Video; }
    static bool supportsContainerType(const String&);

    size_t bytesDecodedToDetermineProperties() const override { return 0; }
    static bool canDecodeType(const String& mimeType);

    void setEncodedDataStatusChangeCallback(WTF::Function<void(EncodedDataStatus)>&& callback) final { m_encodedDataStatusChangedCallback = WTFMove(callback); }
    EncodedDataStatus encodedDataStatus() const final;
    IntSize size() const final;
    size_t frameCount() const final { return m_sampleData.size(); }
    RepetitionCount repetitionCount() const final;
    String uti() const final;
    String filenameExtension() const final { return MIMETypeRegistry::preferredExtensionForMIMEType(m_mimeType); }
    Optional<IntPoint> hotSpot() const final { return WTF::nullopt; }

    IntSize frameSizeAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default) const final { return size(); }
    bool frameIsCompleteAtIndex(size_t index) const final { return sampleAtIndex(index); }
    ImageOrientation frameOrientationAtIndex(size_t) const final;

    Seconds frameDurationAtIndex(size_t) const final;
    bool frameHasAlphaAtIndex(size_t) const final;
    bool frameAllowSubsamplingAtIndex(size_t index) const final { return index <= m_sampleData.size(); }
    unsigned frameBytesAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default) const final;

    NativeImagePtr createFrameImageAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default, const DecodingOptions& = DecodingOptions(DecodingMode::Synchronous)) final;

    void setExpectedContentSize(long long) final { }
    void setData(SharedBuffer&, bool allDataReceived) final;
    bool isAllDataReceived() const final { return m_eos; }
    void clearFrameBufferCache(size_t) final;

    void setHasEOS();
    void notifySample(GRefPtr<GstSample>&&);

private:
    class InnerDecoder : public ThreadSafeRefCounted<InnerDecoder>, public CanMakeWeakPtr<InnerDecoder> {
        WTF_MAKE_FAST_ALLOCATED;
        WTF_MAKE_NONCOPYABLE(InnerDecoder);
    public:
        static RefPtr<InnerDecoder> create(ImageDecoderGStreamer& decoder, const char* data, gssize size)
        {
            return adoptRef(*new InnerDecoder(decoder, data, size));
        }

        InnerDecoder(ImageDecoderGStreamer& decoder, const char* data, gssize size)
            : m_decoder(decoder)
            , m_runLoop(RunLoop::current())
        {
            m_memoryStream = adoptGRef(g_memory_input_stream_new_from_data(data, size, nullptr));
        }

        ~InnerDecoder()
        {
            gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);
        }

        void run();
        EncodedDataStatus encodedDataStatus() const;

    private :
        static void decodebinPadAddedCallback(ImageDecoderGStreamer::InnerDecoder*, GstPad*);
        void handleMessage(GstMessage*);
        void preparePipeline();
        void connectDecoderPad(GstPad*);

        ImageDecoderGStreamer& m_decoder;
        GRefPtr<GstElement> m_pipeline;
        GRefPtr<GInputStream> m_memoryStream;
        RunLoop& m_runLoop;
    };

    void handleSample(GRefPtr<GstSample>&&);
    void pushEncodedData(const SharedBuffer&);

    const ImageDecoderGStreamerSample* sampleAtIndex(size_t) const;

    WTF::Function<void(EncodedDataStatus)> m_encodedDataStatusChangedCallback;
    SampleMap m_sampleData;
    DecodeOrderSampleMap::iterator m_cursor;
    Lock m_sampleGeneratorLock;
    bool m_eos { false };
    Optional<IntSize> m_size;
    String m_mimeType;
    RefPtr<ImageDecoderGStreamer::InnerDecoder> m_innerDecoder;
    Condition m_sampleCondition;
    Lock m_sampleMutex;
    GRefPtr<GstSample> m_sample;
    Condition m_handlerCondition;
    Lock m_handlerMutex;
};
}
#endif
