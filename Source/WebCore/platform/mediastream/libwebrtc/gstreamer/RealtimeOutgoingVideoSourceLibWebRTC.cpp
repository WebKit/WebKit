/*
 * Copyright (C) 2017 Igalia S.L. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"

#if USE(LIBWEBRTC) && USE(GSTREAMER)
#include "RealtimeOutgoingVideoSourceLibWebRTC.h"

#include "GStreamerElementHarness.h"
#include "GStreamerRegistryScanner.h"
#include "GStreamerVideoFrameLibWebRTC.h"
#include "RealtimeIncomingVideoSource.h"
#include "VideoFrameGStreamer.h"

namespace WebCore {

GST_DEBUG_CATEGORY(webkit_libwebrtc_outgoing_video_debug);
#define GST_CAT_DEFAULT webkit_libwebrtc_outgoing_video_debug

Ref<RealtimeOutgoingVideoSource> RealtimeOutgoingVideoSource::create(Ref<MediaStreamTrackPrivate>&& videoSource)
{
    return RealtimeOutgoingVideoSourceLibWebRTC::create(WTF::move(videoSource));
}

Ref<RealtimeOutgoingVideoSourceLibWebRTC> RealtimeOutgoingVideoSourceLibWebRTC::create(Ref<MediaStreamTrackPrivate>&& videoSource)
{
    return adoptRef(*new RealtimeOutgoingVideoSourceLibWebRTC(WTF::move(videoSource)));
}

RealtimeOutgoingVideoSourceLibWebRTC::RealtimeOutgoingVideoSourceLibWebRTC(Ref<MediaStreamTrackPrivate>&& videoSource)
    : RealtimeOutgoingVideoSource(WTF::move(videoSource))
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_libwebrtc_outgoing_video_debug, "webkitlibwebrtcvideooutgoing", 0, "WebKit LibWebRTC outgoing video source");
    });
}

RealtimeOutgoingVideoSourceLibWebRTC::~RealtimeOutgoingVideoSourceLibWebRTC() = default;

void RealtimeOutgoingVideoSourceLibWebRTC::videoFrameAvailable(VideoFrame& videoFrame, VideoFrameTimeMetadata)
{
    switch (videoFrame.rotation()) {
    case VideoFrame::Rotation::None:
        m_currentRotation = webrtc::kVideoRotation_0;
        break;
    case VideoFrame::Rotation::UpsideDown:
        m_currentRotation = webrtc::kVideoRotation_180;
        break;
    case VideoFrame::Rotation::Right:
        m_currentRotation = webrtc::kVideoRotation_90;
        break;
    case VideoFrame::Rotation::Left:
        m_currentRotation = webrtc::kVideoRotation_270;
        break;
    }

    auto& gstFrame = downcast<VideoFrameGStreamer>(videoFrame);
    // A remote track whose codec the player decodes itself carries encoded frames.
    if (gstFrame.isEncoded()) {
        decodeAndSend(gstFrame);
        return;
    }
    sendRawFrame(gstFrame);
}

void RealtimeOutgoingVideoSourceLibWebRTC::sendRawFrame(VideoFrameGStreamer& videoFrame)
{
    auto colorSpace = [&] {
        if (auto pixelFormat = convertVideoFramePixelFormat(videoFrame.pixelFormat(), true)) {
            if (isRGBVideoPixelFormat(*pixelFormat))
                return PlatformVideoColorSpace { .primaries = PlatformVideoColorPrimaries::Bt709, .transfer = PlatformVideoTransferCharacteristics::Bt709, .matrix = PlatformVideoMatrixCoefficients::Bt709, .fullRange = false };
        }
        return videoFrame.colorSpace();
    }();

    auto frameSize = videoFrame.presentationSize();
    if (frameSize.isEmpty())
        return;

    frameSize.scale(this->videoFrameScaling());
    auto sample = videoFrame.convert(GST_VIDEO_FORMAT_I420, frameSize, colorSpace);
    if (!sample)
        return;

    // Clear the segment otherwise a bytes segment will be pushed on the encoder, leading to
    // potential critical warnings in videoflip which expects a time segment.
    GRefPtr writableSample = adoptGRef(gst_sample_make_writable(sample.leakRef()));
    gst_sample_set_segment(writableSample.get(), nullptr);
    GST_TRACE("Sending video frame with caps %" GST_PTR_FORMAT, gst_sample_get_caps(writableSample.get()));
    sendFrame(GStreamerVideoFrameLibWebRTC::create(WTF::move(writableSample)), colorSpace);
}

void RealtimeOutgoingVideoSourceLibWebRTC::decodeAndSend(VideoFrameGStreamer& videoFrame)
{
    const auto& sample = videoFrame.sample();
    auto* caps = gst_sample_get_caps(sample.get());
    auto mediaType = String::fromLatin1(gstStructureGetName(gst_caps_get_structure(caps, 0)).utf8());
    if (mediaType != m_decoderMediaType) {
        m_decoderMediaType = mediaType;
        m_decoderHarness = nullptr;
        auto factory = GStreamerRegistryScanner::singleton().isCodecSupported(GStreamerRegistryScanner::Configuration::Decoding, mediaType).factory;
        GRefPtr<GstElement> element = factory ? gst_element_factory_create(factory.get(), nullptr) : nullptr;
        if (!element) {
            GST_WARNING("No decoder for the source's %s frames, which are not sent", mediaType.ascii().data());
            return;
        }
        configureVideoDecoderForHarnessing(element);
        // A decoder that wants what a parser adds (stream format, profile) gets one in front, as WebCodecs' do.
        static constexpr std::array<std::pair<ASCIILiteral, ASCIILiteral>, 4> parsers { {
            { "video/x-h264"_s, "h264parse"_s },
            { "video/x-h265"_s, "h265parse"_s },
            { "video/x-vp9"_s, "vp9parse"_s },
            { "video/x-av1"_s, "av1parse"_s },
        } };
        auto parser = std::ranges::find_if(parsers, [&](const auto& entry) {
            return mediaType == entry.first;
        });
        if (parser != parsers.end() && !gst_element_factory_can_sink_all_caps(factory.get(), caps)) {
            if (auto* parserElement = makeGStreamerElement(parser->second)) {
                GRefPtr<GstElement> bin = gst_bin_new(nullptr);
                gst_bin_add_many(GST_BIN_CAST(bin.get()), parserElement, element.get(), nullptr);
                gst_element_link(parserElement, element.get());
                GRefPtr sinkPad = adoptGRef(gst_element_get_static_pad(parserElement, "sink"));
                gst_element_add_pad(bin.get(), gst_ghost_pad_new("sink", sinkPad.get()));
                GRefPtr srcPad = adoptGRef(gst_element_get_static_pad(element.get(), "src"));
                gst_element_add_pad(bin.get(), gst_ghost_pad_new("src", srcPad.get()));
                element = WTF::move(bin);
            }
        }
        // Decoded in the delivering thread: each frame is sent on as the decoder outputs it.
        m_decoderHarness = GStreamerElementHarness::create(WTF::move(element), [this](auto&, GRefPtr<GstSample>&& outputSample) {
            auto size = getVideoResolutionFromCaps(gst_sample_get_caps(outputSample.get())).value_or(FloatSize { });
            Ref decodedFrame = VideoFrameGStreamer::create(WTF::move(outputSample), { IntSize(size) }, PlatformVideoColorSpace { m_decoderColorSpace });
            sendRawFrame(decodedFrame.get());
        }, std::nullopt, adoptGRef(gst_caps_new_empty_simple("video/x-raw")));
        m_decoderHarness->setIsLive(true);
        GST_DEBUG("Decoding the source's %s frames for sending", mediaType.ascii().data());
        // Started mid-call, the decoder needs a key frame: a remote source asks its sender for one.
        if (RefPtr incomingSource = dynamicDowncast<RealtimeIncomingVideoSource>(source().source()))
            incomingSource->requestKeyFrame();
    }
    if (!m_decoderHarness)
        return;

    m_decoderColorSpace = videoFrame.colorSpace();
    if (!m_decoderHarness->pushSample(GRefPtr { sample }))
        GST_DEBUG("The decoder refused a frame");
    m_decoderHarness->processOutputSamples();
}

webrtc::scoped_refptr<webrtc::VideoFrameBuffer> RealtimeOutgoingVideoSourceLibWebRTC::createBlackFrame(size_t width, size_t height)
{
    GST_TRACE("Creating black video frame");

    GstVideoInfo info;
    gst_video_info_set_format(&info, GST_VIDEO_FORMAT_RGB, width, height);

    GRefPtr<GstBuffer> buffer = adoptGRef(gst_buffer_new_allocate(nullptr, info.size, nullptr));
    GRefPtr<GstCaps> caps = adoptGRef(gst_video_info_to_caps(&info));

    {
        GstMappedBuffer map(buffer.get(), GST_MAP_WRITE);
        memsetSpan(map.mutableSpan<uint8_t>(), 0);
    }

    return GStreamerVideoFrameLibWebRTC::create(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(LIBWEBRTC) && USE(GSTREAMER)
