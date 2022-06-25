<?% config.freshness.owner = 'brandtr' %?>
<?% config.freshness.reviewed = '2021-04-15' %?>

# Video coding in WebRTC

## Introduction to layered video coding

[Video coding][video-coding-wiki] is the process of encoding a stream of
uncompressed video frames into a compressed bitstream, whose bitrate is lower
than that of the original stream.

### Block-based hybrid video coding

All video codecs in WebRTC are based on the block-based hybrid video coding
paradigm, which entails prediction of the original video frame using either
[information from previously encoded frames][motion-compensation-wiki] or
information from previously encoded portions of the current frame, subtraction
of the prediction from the original video, and
[transform][transform-coding-wiki] and [quantization][quantization-wiki] of the
resulting difference. The output of the quantization process, quantized
transform coefficients, is losslessly [entropy coded][entropy-coding-wiki] along
with other encoder parameters (e.g., those related to the prediction process)
and then a reconstruction is constructed by inverse quantizing and inverse
transforming the quantized transform coefficients and adding the result to the
prediction. Finally, in-loop filtering is applied and the resulting
reconstruction is stored as a reference frame to be used to develop predictions
for future frames.

### Frame types

When an encoded frame depends on previously encoded frames (i.e., it has one or
more inter-frame dependencies), the prior frames must be available at the
receiver before the current frame can be decoded. In order for a receiver to
start decoding an encoded bitstream, a frame which has no prior dependencies is
required. Such a frame is called a "key frame". For real-time-communications
encoding, key frames typically compress less efficiently than "delta frames"
(i.e., frames whose predictions are derived from previously encoded frames).

### Single-layer coding

In 1:1 calls, the encoded bitstream has a single recipient. Using end-to-end
bandwidth estimation, the target bitrate can thus be well tailored for the
intended recipient. The number of key frames can be kept to a minimum and the
compressability of the stream can be maximized. One way of achiving this is by
using "single-layer coding", where each delta frame only depends on the frame
that was most recently encoded.

### Scalable video coding

In multiway conferences, on the other hand, the encoded bitstream has multiple
recipients each of whom may have different downlink bandwidths. In order to
tailor the encoded bitstreams to a heterogeneous network of receivers,
[scalable video coding][svc-wiki] can be used. The idea is to introduce
structure into the dependency graph of the encoded bitstream, such that _layers_ of
the full stream can be decoded using only available lower layers. This structure
allows for a [selective forwarding unit][sfu-webrtc-glossary] to discard upper
layers of the of the bitstream in order to achieve the intended downlink
bandwidth.

There are multiple types of scalability:

* _Temporal scalability_ are layers whose framerate (and bitrate) is lower than that of the upper layer(s)
* _Spatial scalability_ are layers whose resolution (and bitrate) is lower than that of the upper layer(s)
* _Quality scalability_ are layers whose bitrate is lower than that of the upper layer(s)

WebRTC supports temporal scalability for `VP8`, `VP9` and `AV1`, and spatial
scalability for `VP9` and `AV1`.

### Simulcast

Simulcast is another approach for multiway conferencing, where multiple
_independent_ bitstreams are produced by the encoder.

In cases where multiple encodings of the same source are required (e.g., uplink
transmission in a multiway call), spatial scalability with inter-layer
prediction generally offers superior coding efficiency compared with simulcast.
When a single encoding is required (e.g., downlink transmission in any call),
simulcast generally provides better coding efficiency for the upper spatial
layers. The `K-SVC` concept, where spatial inter-layer dependencies are only
used to encode key frames, for which inter-layer prediction is typically
significantly more effective than it is for delta frames, can be seen as a
compromise between full spatial scalability and simulcast.

## Overview of implementation in `modules/video_coding`

Given the general introduction to video coding above, we now describe some
specifics of the [`modules/video_coding`][modules-video-coding] folder in WebRTC.

### Built-in software codecs in [`modules/video_coding/codecs`][modules-video-coding-codecs]

This folder contains WebRTC-specific classes that wrap software codec
implementations for different video coding standards:

* [libaom][libaom-src] for [AV1][av1-spec]
* [libvpx][libvpx-src] for [VP8][vp8-spec] and [VP9][vp9-spec]
* [OpenH264][openh264-src] for [H.264 constrained baseline profile][h264-spec]

Users of the library can also inject their own codecs, using the
[VideoEncoderFactory][video-encoder-factory-interface] and
[VideoDecoderFactory][video-decoder-factory-interface] interfaces. This is how
platform-supported codecs, such as hardware backed codecs, are implemented.

### Video codec test framework in [`modules/video_coding/codecs/test`][modules-video-coding-codecs-test]

This folder contains a test framework that can be used to evaluate video quality
performance of different video codec implementations.

### SVC helper classes in [`modules/video_coding/svc`][modules-video-coding-svc]

*   [`ScalabilityStructure*`][scalabilitystructure] - different
    [standardized scalability structures][scalability-structure-spec]
*   [`ScalableVideoController`][scalablevideocontroller] - provides instructions to the video encoder how
    to create a scalable stream
*   [`SvcRateAllocator`][svcrateallocator] - bitrate allocation to different spatial and temporal
    layers

### Utility classes in [`modules/video_coding/utility`][modules-video-coding-utility]

*   [`FrameDropper`][framedropper] - drops incoming frames when encoder systematically
    overshoots its target bitrate
*   [`FramerateController`][frameratecontroller] - drops incoming frames to achieve a target framerate
*   [`QpParser`][qpparser] - parses the quantization parameter from a bitstream
*   [`QualityScaler`][qualityscaler] - signals when an encoder generates encoded frames whose
    quantization parameter is outside the window of acceptable values
*   [`SimulcastRateAllocator`][simulcastrateallocator] - bitrate allocation to simulcast layers

### General helper classes in [`modules/video_coding`][modules-video-coding]

*   [`FecControllerDefault`][feccontrollerdefault] - provides a default implementation for rate
    allocation to [forward error correction][fec-wiki]
*   [`VideoCodecInitializer`][videocodecinitializer] - converts between different encoder configuration
    structs

### Receiver buffer classes in [`modules/video_coding`][modules-video-coding]

*   [`PacketBuffer`][packetbuffer] - (re-)combines RTP packets into frames
*   [`RtpFrameReferenceFinder`][rtpframereferencefinder] - determines dependencies between frames based on information in the RTP header, payload header and RTP extensions
*   [`FrameBuffer`][framebuffer] - order frames based on their dependencies to be fed to the decoder

[video-coding-wiki]: https://en.wikipedia.org/wiki/Video_coding_format
[motion-compensation-wiki]: https://en.wikipedia.org/wiki/Motion_compensation
[transform-coding-wiki]: https://en.wikipedia.org/wiki/Transform_coding
[motion-vector-wiki]: https://en.wikipedia.org/wiki/Motion_vector
[mpeg-wiki]: https://en.wikipedia.org/wiki/Moving_Picture_Experts_Group
[svc-wiki]: https://en.wikipedia.org/wiki/Scalable_Video_Coding
[sfu-webrtc-glossary]: https://webrtcglossary.com/sfu/
[libvpx-src]: https://chromium.googlesource.com/webm/libvpx/
[libaom-src]: https://aomedia.googlesource.com/aom/
[openh264-src]: https://github.com/cisco/openh264
[vp8-spec]: https://tools.ietf.org/html/rfc6386
[vp9-spec]: https://storage.googleapis.com/downloads.webmproject.org/docs/vp9/vp9-bitstream-specification-v0.6-20160331-draft.pdf
[av1-spec]: https://aomediacodec.github.io/av1-spec/
[h264-spec]: https://www.itu.int/rec/T-REC-H.264-201906-I/en
[video-encoder-factory-interface]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/api/video_codecs/video_encoder_factory.h;l=27;drc=afadfb24a5e608da6ae102b20b0add53a083dcf3
[video-decoder-factory-interface]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/api/video_codecs/video_decoder_factory.h;l=27;drc=49c293f03d8f593aa3aca282577fcb14daa63207
[scalability-structure-spec]: https://w3c.github.io/webrtc-svc/#scalabilitymodes*
[fec-wiki]: https://en.wikipedia.org/wiki/Error_correction_code#Forward_error_correction
[entropy-coding-wiki]: https://en.wikipedia.org/wiki/Entropy_encoding
[modules-video-coding]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/
[modules-video-coding-codecs]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/codecs/
[modules-video-coding-codecs-test]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/codecs/test/
[modules-video-coding-svc]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/svc/
[modules-video-coding-utility]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/utility/
[scalabilitystructure]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/svc/create_scalability_structure.h?q=CreateScalabilityStructure
[scalablevideocontroller]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/svc/scalable_video_controller.h?q=ScalableVideoController
[svcrateallocator]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/svc/svc_rate_allocator.h?q=SvcRateAllocator
[framedropper]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/utility/frame_dropper.h?q=FrameDropper
[frameratecontroller]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/utility/framerate_controller.h?q=FramerateController
[qpparser]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/utility/qp_parser.h?q=QpParser
[qualityscaler]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/utility/quality_scaler.h?q=QualityScaler
[simulcastrateallocator]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/utility/simulcast_rate_allocator.h?q=SimulcastRateAllocator
[feccontrollerdefault]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/fec_controller_default.h?q=FecControllerDefault
[videocodecinitializer]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/include/video_codec_initializer.h?q=VideoCodecInitializer
[packetbuffer]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/packet_buffer.h?q=PacketBuffer
[rtpframereferencefinder]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/rtp_frame_reference_finder.h?q=RtpFrameReferenceFinder
[framebuffer]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/frame_buffer2.h?q=FrameBuffer
[quantization-wiki]: https://en.wikipedia.org/wiki/Quantization_(signal_processing)
