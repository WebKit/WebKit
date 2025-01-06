<!-- go/cmark -->
<!--* freshness: {owner: 'eshr' reviewed: '2024-11-06'} *-->

# Video Adaptation

Video adaptation is a mechanism which reduces the bandwidth or CPU consumption
by reducing encoded video quality.

## Overview

Adaptation occurs when a _Resource_ signals that it is currently underused or
overused. When overused, the video quality is decreased and when underused, the
video quality is increased. There are currently two dimensions in which the
quality can be adapted: frame-rate and resolution. The dimension that is adapted
is based on the degradation preference for the video track.

## Resources

_Resources_ monitor metrics from the system or the video stream. For example, a
resource could monitor system temperature or the bandwidth usage of the video
stream. A resource implements the [Resource][resource.h] interface. When a
resource detects that it is overused, it calls `SetUsageState(kOveruse)`. When
the resource is no longer overused, it can signal this using
`SetUsageState(kUnderuse)`.

There are two resources that are used by default on all video tracks: the
quality scaler and encode overuse resources.

### QP Scaler Resource

The quality scaler resource monitors the quantization parameter (QP) of the
encoded video frames for video send stream and ensures that the quality of the
stream is acceptable for the current resolution. After each frame is encoded the
[QualityScaler][quality_scaler.h] is given the QP of the encoded frame. Overuse
or underuse is signalled when the average QP is outside of the
[QP thresholds][VideoEncoder::QpThresholds]. If the average QP is above the
_high_ threshold, the QP scaler signals _overuse_, and when below the _low_
threshold the QP scaler signals _underuse_.

The thresholds are set by the video encoder in the `scaling_settings` property
of the [EncoderInfo][EncoderInfo].

*Note:* that the QP scaler is only enabled when the degradation preference is
`MAINTAIN_FRAMERATE` or `BALANCED`.

### Encode Usage Resource

The [encoder usage resource][encode_usage_resource.h] monitors how long it takes
to encode a video frame. This works as a good proxy measurement for CPU usage as
contention increases when CPU usage is high, increasing the encode times of the
video frames.

The time is tracked from when frame encoding starts to when it is completed. If
the average encoder usage exceeds the thresholds set, *overuse* is triggered.

### Injecting other Resources

A custom resource can be injected into the call using the
[Call::AddAdaptationResource][Call::AddAdaptationResource] method.

## Adaptation

When a a *resource* signals the it is over or underused, this signal reaches the
`ResourceAdaptationProcessor` who requests an `Adaptation` proposal from the
[VideoStreamAdapter][VideoStreamAdapter]. This proposal is based on the
degradation preference of the video stream. `ResourceAdaptationProcessor` will
determine if the `Adaptation` should be applied based on the current adaptation
status and the `Adaptation` proposal.

### Degradation Preference

There are 3 degradation preferences, described in the
[RtpParameters][RtpParameters] header. These are

*   `MAINTAIN_FRAMERATE`: Adapt video resolution
*   `MAINTAIN_RESOLUTION`: Adapt video frame-rate.
*   `BALANCED`: Adapt video frame-rate or resolution.

The degradation preference is set for a video track using the
`degradation_preference` property in the [RtpParameters][RtpParameters].

## VideoSinkWants and video stream adaptation

Once an adaptation is applied it notifies the video stream. The video stream
converts this adaptation to a [VideoSinkWants][VideoSinkWants]. These sink wants
indicate to the video stream that some restrictions should be applied to the
stream before it is sent to encoding. It has a few properties, but for
adaptation the properties that might be set are:

*   `target_pixel_count`: The desired number of pixels for each video frame. The
    actual pixel count should be close to this but does not have to be exact so
    that aspect ratio can be maintained.
*   `max_pixel_count`: The maximum number of pixels in each video frame. This
    value can not be exceeded if set.
*   `max_framerate_fps`: The maximum frame-rate for the video source. The source
    is expected to drop frames that cause this threshold to be exceeded.

The `VideoSinkWants` can be applied by any video source, or one may use the
[AdaptedVideoTraceSource][adapted_video_track_source.h] which is a base class
for sources that need video adaptation.

[RtpParameters]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/api/rtp_parameters.h?q=%22RTC_EXPORT%20RtpParameters%22
[resource.h]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/api/adaptation/resource.h
[Call::AddAdaptationResource]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/call/call.h?q=Call::AddAdaptationResource
[quality_scaler.h]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/utility/quality_scaler.h
[VideoEncoder::QpThresholds]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/api/video_codecs/video_encoder.h?q=VideoEncoder::QpThresholds
[EncoderInfo]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/api/video_codecs/video_encoder.h?q=VideoEncoder::EncoderInfo
[encode_usage_resource.h]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/video/adaptation/encode_usage_resource.h
[VideoStreamAdapter]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/call/adaptation/video_stream_adapter.h
[adaptation_constraint.h]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/call/adaptation/adaptation_constraint.h
[bitrate_constraint.h]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/video/adaptation/bitrate_constraint.h
[AddOrUpdateSink]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/api/video/video_source_interface.h?q=AddOrUpdateSink
[VideoSinkWants]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/api/video/video_source_interface.h?q=%22RTC_EXPORT%20VideoSinkWants%22
[adapted_video_track_source.h]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/media/base/adapted_video_track_source.h
