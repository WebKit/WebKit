<?% config.freshness.reviewed = '2021-04-14' %?>
<?% config.freshness.owner = 'asapersson' %?>

# Video stats

Overview of collected statistics for [VideoSendStream] and [VideoReceiveStream].

## VideoSendStream

[VideoSendStream::Stats] for a sending stream can be gathered via `VideoSendStream::GetStats()`.

Some statistics are collected per RTP stream (see [StreamStats]) and can be of `StreamType`: `kMedia`, `kRtx`, `kFlexfec`.

Multiple `StreamStats` objects are for example present if simulcast is used (multiple `kMedia` objects) or if RTX or FlexFEC is negotiated.

### SendStatisticsProxy
`VideoSendStream` owns a [SendStatisticsProxy] which implements
`VideoStreamEncoderObserver`,
`RtcpStatisticsCallback`,
`ReportBlockDataObserver`,
`RtcpPacketTypeCounterObserver`,
`StreamDataCountersCallback`,
`BitrateStatisticsObserver`,
`FrameCountObserver`,
`SendSideDelayObserver`
and holds a `VideoSendStream::Stats` object.

`SendStatisticsProxy` is called via these interfaces by different components (e.g. `RtpRtcp` module) to update stats.

#### StreamStats
*   `type` - kMedia, kRtx or kFlexfec.
*   `referenced_media_ssrc` - only present for type kRtx/kFlexfec. The SSRC for the kMedia stream that retransmissions or FEC is performed for.

Updated when a frame has been encoded, `VideoStreamEncoder::OnEncodedImage`.
*   `frames_encoded `- total number of encoded frames.
*   `encode_frame_rate` - number of encoded frames during the last second.
*   `width` - width of last encoded frame [[rtcoutboundrtpstreamstats-framewidth]].
*   `height` - height of last encoded frame [[rtcoutboundrtpstreamstats-frameheight]].
*   `total_encode_time_ms` - total encode time for encoded frames.
*   `qp_sum` - sum of quantizer values of encoded frames [[rtcoutboundrtpstreamstats-qpsum]].
*   `frame_counts` - total number of encoded key/delta frames [[rtcoutboundrtpstreamstats-keyframesencoded]].

Updated when a RTP packet is transmitted to the network, `RtpSenderEgress::SendPacket`.
*   `rtp_stats` - total number of sent bytes/packets.
*   `total_bitrate_bps` - total bitrate sent in bits per second (over a one second window).
*   `retransmit_bitrate_bps` - total retransmit bitrate sent in bits per second (over a one second window).
*   `avg_delay_ms` - average capture-to-send delay for sent packets (over a one second window).
*   `max_delay_ms` - maximum capture-to-send delay for sent packets (over a one second window).
*   `total_packet_send_delay_ms` - total capture-to-send delay for sent packets [[rtcoutboundrtpstreamstats-totalpacketsenddelay]].

Updated when an incoming RTCP packet is parsed, `RTCPReceiver::ParseCompoundPacket`.
*   `rtcp_packet_type_counts` - total number of received NACK/FIR/PLI packets [rtcoutboundrtpstreamstats-[nackcount], [fircount], [plicount]].

Updated when a RTCP report block packet is received, `RTCPReceiver::TriggerCallbacksFromRtcpPacket`.
*   `rtcp_stats` - RTCP report block data.
*   `report_block_data` - RTCP report block data.

#### Stats
*   `std::map<uint32_t, StreamStats> substreams` - StreamStats mapped per SSRC.

Updated when a frame is received from the source, `VideoStreamEncoder::OnFrame`.
*   `frames` - total number of frames fed to VideoStreamEncoder.
*   `input_frame_rate` - number of frames fed to VideoStreamEncoder during the last second.
*   `frames_dropped_by_congestion_window` - total number of dropped frames due to congestion window pushback.
*   `frames_dropped_by_encoder_queue` - total number of dropped frames due to that the encoder is blocked.

Updated if a frame from the source is dropped, `VideoStreamEncoder::OnDiscardedFrame`.
*   `frames_dropped_by_capturer` - total number dropped frames by the source.

Updated if a frame is dropped by `FrameDropper`, `VideoStreamEncoder::MaybeEncodeVideoFrame`.
*   `frames_dropped_by_rate_limiter` - total number of dropped frames to avoid bitrate overuse.

Updated (if changed) before a frame is passed to the encoder, `VideoStreamEncoder::EncodeVideoFrame`.
*   `encoder_implementation_name` - name of encoder implementation [[rtcoutboundrtpstreamstats-encoderimplementation]].

Updated after a frame has been encoded, `VideoStreamEncoder::OnEncodedImage`.
*   `frames_encoded `- total number of encoded frames [[rtcoutboundrtpstreamstats-framesencoded]].
*   `encode_frame_rate` - number of encoded frames during the last second [[rtcoutboundrtpstreamstats-framespersecond]].
*   `total_encoded_bytes_target` - total target frame size in bytes [[rtcoutboundrtpstreamstats-totalencodedbytestarget]].
*   `huge_frames_sent` - total number of huge frames sent [[rtcoutboundrtpstreamstats-hugeframessent]].
*   `media_bitrate_bps` - the actual bitrate the encoder is producing.
*   `avg_encode_time_ms` - average encode time for encoded frames.
*   `total_encode_time_ms` - total encode time for encoded frames [[rtcoutboundrtpstreamstats-totalencodetime]].
*   `frames_dropped_by_encoder`- total number of dropped frames by the encoder.

Adaptation stats.
*   `bw_limited_resolution` - shows if resolution is limited due to restricted bandwidth.
*   `cpu_limited_resolution` - shows if resolution is limited due to cpu.
*   `bw_limited_framerate` - shows if framerate is limited due to restricted bandwidth.
*   `cpu_limited_framerate` - shows if framerate is limited due to cpu.
*   `quality_limitation_reason` - current reason for limiting resolution and/or framerate [[rtcoutboundrtpstreamstats-qualitylimitationreason]].
*   `quality_limitation_durations_ms` - total time spent in quality limitation state [[rtcoutboundrtpstreamstats-qualitylimitationdurations]].
*   `quality_limitation_resolution_changes` - total number of times that resolution has changed due to quality limitation [[rtcoutboundrtpstreamstats-qualitylimitationresolutionchanges]].
*   `number_of_cpu_adapt_changes` - total number of times resolution/framerate has changed due to cpu limitation.
*   `number_of_quality_adapt_changes` - total number of times resolution/framerate has changed due to quality limitation.

Updated when the encoder is configured, `VideoStreamEncoder::ReconfigureEncoder`.
*   `content_type` - configured content type (UNSPECIFIED/SCREENSHARE).

Updated when the available bitrate changes, `VideoSendStreamImpl::OnBitrateUpdated`.
*   `target_media_bitrate_bps` - the bitrate the encoder is configured to use.
*   `suspended` - shows if video is suspended due to zero target bitrate.

## VideoReceiveStream
[VideoReceiveStream::Stats] for a receiving stream can be gathered via `VideoReceiveStream::GetStats()`.

### ReceiveStatisticsProxy
`VideoReceiveStream` owns a [ReceiveStatisticsProxy] which implements
`VCMReceiveStatisticsCallback`,
`RtcpCnameCallback`,
`RtcpPacketTypeCounterObserver`,
`CallStatsObserver`
and holds a `VideoReceiveStream::Stats` object.

`ReceiveStatisticsProxy` is called via these interfaces by different components (e.g. `RtpRtcp` module) to update stats.

#### Stats
*   `current_payload_type` - current payload type.
*   `ssrc` - configured SSRC for the received stream.

Updated when a complete frame is received, `FrameBuffer::InsertFrame`.
*   `frame_counts` - total number of key/delta frames received [[rtcinboundrtpstreamstats-keyframesdecoded]].
*   `network_frame_rate` - number of frames received during the last second.

Updated when a frame is ready for decoding, `FrameBuffer::GetNextFrame`. From `VCMTiming`:
*   `jitter_buffer_ms` - jitter buffer delay in ms.
*   `max_decode_ms` - the 95th percentile observed decode time within a time window (10 sec).
*   `render_delay_ms` - render delay in ms.
*   `min_playout_delay_ms` - minimum playout delay in ms.
*   `target_delay_ms` - target playout delay in ms. Max(`min_playout_delay_ms`, `jitter_delay_ms` + `max_decode_ms` + `render_delay_ms`).
*   `current_delay_ms` - actual playout delay in ms.
*   `jitter_buffer_delay_seconds` - total jitter buffer delay in seconds [[rtcinboundrtpstreamstats-jitterbufferdelay]].
*   `jitter_buffer_emitted_count` - total number of frames that have come out from the jitter buffer [[rtcinboundrtpstreamstats-jitterbufferemittedcount]].

Updated (if changed) after a frame is passed to the decoder, `VCMGenericDecoder::Decode`.
*   `decoder_implementation_name` - name of decoder implementation [[rtcinboundrtpstreamstats-decoderimplementation]].

Updated when a frame is ready for decoding, `FrameBuffer::GetNextFrame`.
*   `timing_frame_info` - timestamps for a full lifetime of a frame.
*   `first_frame_received_to_decoded_ms` - initial decoding latency between the first arrived frame and the first decoded frame.
*   `frames_dropped` - total number of dropped frames prior to decoding or if the system is too slow [[rtcreceivedrtpstreamstats-framesdropped]].

Updated after a frame has been decoded, `VCMDecodedFrameCallback::Decoded`.
*   `frames_decoded` - total number of decoded frames [[rtcinboundrtpstreamstats-framesdecoded]].
*   `decode_frame_rate` - number of decoded frames during the last second [[rtcinboundrtpstreamstats-framespersecond]].
*   `decode_ms` - time to decode last frame in ms.
*   `total_decode_time_ms` - total decode time for decoded frames [[rtcinboundrtpstreamstats-totaldecodetime]].
*   `qp_sum` - sum of quantizer values of decoded frames [[rtcinboundrtpstreamstats-qpsum]].
*   `content_type` - content type (UNSPECIFIED/SCREENSHARE).
*   `interframe_delay_max_ms` - max inter-frame delay within a time window between decoded frames.
*   `total_inter_frame_delay` - sum of inter-frame delay in seconds between decoded frames [[rtcinboundrtpstreamstats-totalinterframedelay]].
*   `total_squared_inter_frame_delay` - sum of squared inter-frame delays in seconds between decoded frames [[rtcinboundrtpstreamstats-totalsquaredinterframedelay]].

Updated before a frame is sent to the renderer, `VideoReceiveStream2::OnFrame`.
*   `frames_rendered` - total number of rendered frames.
*   `render_frame_rate` - number of rendered frames during the last second.
*   `width` - width of last frame fed to renderer [[rtcinboundrtpstreamstats-framewidth]].
*   `height` - height of last frame fed to renderer [[rtcinboundrtpstreamstats-frameheight]].
*   `estimated_playout_ntp_timestamp_ms` - estimated playout NTP timestamp [[rtcinboundrtpstreamstats-estimatedplayouttimestamp]].
*   `sync_offset_ms` - NTP timestamp difference between the last played out audio and video frame.
*   `freeze_count` - total number of detected freezes.
*   `pause_count` - total number of detected pauses.
*   `total_freezes_duration_ms` - total duration of freezes in ms.
*   `total_pauses_duration_ms` - total duration of pauses in ms.
*   `total_frames_duration_ms` - time in ms between the last rendered frame and the first rendered frame.
*   `sum_squared_frame_durations` - sum of squared inter-frame delays in seconds between rendered frames.

`ReceiveStatisticsImpl::OnRtpPacket` is updated for received RTP packets. From `ReceiveStatistics`:
*   `total_bitrate_bps` - incoming bitrate in bps.
*   `rtp_stats` - RTP statistics for the received stream.

Updated when a RTCP packet is sent, `RTCPSender::ComputeCompoundRTCPPacket`.
*   `rtcp_packet_type_counts` - total number of sent NACK/FIR/PLI packets [rtcinboundrtpstreamstats-[nackcount], [fircount], [plicount]].


[VideoSendStream]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/call/video_send_stream.h
[VideoSendStream::Stats]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/call/video_send_stream.h?q=VideoSendStream::Stats
[StreamStats]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/call/video_send_stream.h?q=VideoSendStream::StreamStats
[SendStatisticsProxy]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/video/send_statistics_proxy.h
[rtcoutboundrtpstreamstats-framewidth]: https://w3c.github.io/webrtc-stats/#dom-rtcoutboundrtpstreamstats-framewidth
[rtcoutboundrtpstreamstats-frameheight]: https://w3c.github.io/webrtc-stats/#dom-rtcoutboundrtpstreamstats-frameheight
[rtcoutboundrtpstreamstats-qpsum]: https://w3c.github.io/webrtc-stats/#dom-rtcoutboundrtpstreamstats-qpsum
[rtcoutboundrtpstreamstats-keyframesencoded]: https://w3c.github.io/webrtc-stats/#dom-rtcoutboundrtpstreamstats-keyframesencoded
[rtcoutboundrtpstreamstats-totalpacketsenddelay]: https://w3c.github.io/webrtc-stats/#dom-rtcoutboundrtpstreamstats-totalpacketsenddelay
[nackcount]: https://w3c.github.io/webrtc-stats/#dom-rtcoutboundrtpstreamstats-nackcount
[fircount]: https://w3c.github.io/webrtc-stats/#dom-rtcoutboundrtpstreamstats-fircount
[plicount]: https://w3c.github.io/webrtc-stats/#dom-rtcoutboundrtpstreamstats-plicount
[rtcoutboundrtpstreamstats-encoderimplementation]: https://w3c.github.io/webrtc-stats/#dom-rtcoutboundrtpstreamstats-encoderimplementation
[rtcoutboundrtpstreamstats-framesencoded]: https://w3c.github.io/webrtc-stats/#dom-rtcoutboundrtpstreamstats-framesencoded
[rtcoutboundrtpstreamstats-framespersecond]: https://w3c.github.io/webrtc-stats/#dom-rtcoutboundrtpstreamstats-framespersecond
[rtcoutboundrtpstreamstats-totalencodedbytestarget]: https://w3c.github.io/webrtc-stats/#dom-rtcoutboundrtpstreamstats-totalencodedbytestarget
[rtcoutboundrtpstreamstats-hugeframessent]: https://w3c.github.io/webrtc-stats/#dom-rtcoutboundrtpstreamstats-hugeframessent
[rtcoutboundrtpstreamstats-totalencodetime]: https://w3c.github.io/webrtc-stats/#dom-rtcoutboundrtpstreamstats-totalencodetime
[rtcoutboundrtpstreamstats-qualitylimitationreason]: https://w3c.github.io/webrtc-stats/#dom-rtcoutboundrtpstreamstats-qualitylimitationreason
[rtcoutboundrtpstreamstats-qualitylimitationdurations]: https://w3c.github.io/webrtc-stats/#dom-rtcoutboundrtpstreamstats-qualitylimitationdurations
[rtcoutboundrtpstreamstats-qualitylimitationresolutionchanges]: https://w3c.github.io/webrtc-stats/#dom-rtcoutboundrtpstreamstats-qualitylimitationresolutionchanges

[VideoReceiveStream]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/call/video_receive_stream.h
[VideoReceiveStream::Stats]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/call/video_receive_stream.h?q=VideoReceiveStream::Stats
[ReceiveStatisticsProxy]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/video/receive_statistics_proxy2.h
[rtcinboundrtpstreamstats-keyframesdecoded]: https://w3c.github.io/webrtc-stats/#dom-rtcinboundrtpstreamstats-keyframesdecoded
[rtcinboundrtpstreamstats-jitterbufferdelay]: https://w3c.github.io/webrtc-stats/#dom-rtcinboundrtpstreamstats-jitterbufferdelay
[rtcinboundrtpstreamstats-jitterbufferemittedcount]: https://w3c.github.io/webrtc-stats/#dom-rtcinboundrtpstreamstats-jitterbufferemittedcount
[rtcinboundrtpstreamstats-decoderimplementation]: https://w3c.github.io/webrtc-stats/#dom-rtcinboundrtpstreamstats-decoderimplementation
[rtcreceivedrtpstreamstats-framesdropped]: https://www.w3.org/TR/webrtc-stats/#dom-rtcreceivedrtpstreamstats-framesdropped
[rtcinboundrtpstreamstats-framesdecoded]: https://w3c.github.io/webrtc-stats/#dom-rtcinboundrtpstreamstats-framesdecoded
[rtcinboundrtpstreamstats-framespersecond]: https://w3c.github.io/webrtc-stats/#dom-rtcinboundrtpstreamstats-framespersecond
[rtcinboundrtpstreamstats-totaldecodetime]: https://w3c.github.io/webrtc-stats/#dom-rtcinboundrtpstreamstats-totaldecodetime
[rtcinboundrtpstreamstats-qpsum]: https://w3c.github.io/webrtc-stats/#dom-rtcinboundrtpstreamstats-qpsum
[rtcinboundrtpstreamstats-totalinterframedelay]: https://w3c.github.io/webrtc-stats/#dom-rtcinboundrtpstreamstats-totalinterframedelay
[rtcinboundrtpstreamstats-totalsquaredinterframedelay]: https://w3c.github.io/webrtc-stats/#dom-rtcinboundrtpstreamstats-totalsquaredinterframedelay
[rtcinboundrtpstreamstats-estimatedplayouttimestamp]: https://w3c.github.io/webrtc-stats/#dom-rtcinboundrtpstreamstats-estimatedplayouttimestamp
[rtcinboundrtpstreamstats-framewidth]: https://w3c.github.io/webrtc-stats/#dom-rtcinboundrtpstreamstats-framewidth
[rtcinboundrtpstreamstats-frameheight]: https://w3c.github.io/webrtc-stats/#dom-rtcinboundrtpstreamstats-frameheight
[nackcount]: https://w3c.github.io/webrtc-stats/#dom-rtcinboundrtpstreamstats-nackcount
[fircount]: https://w3c.github.io/webrtc-stats/#dom-rtcinboundrtpstreamstats-fircount
[plicount]: https://w3c.github.io/webrtc-stats/#dom-rtcinboundrtpstreamstats-plicount
