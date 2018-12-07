/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/video_receive_stream.h"

#include <stdlib.h>

#include <set>
#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "call/rtp_stream_receiver_controller_interface.h"
#include "call/rtx_receive_stream.h"
#include "common_video/h264/profile_level_id.h"
#include "common_video/include/incoming_video_stream.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/utility/include/process_thread.h"
#include "modules/video_coding/frame_object.h"
#include "modules/video_coding/include/video_coding.h"
#include "modules/video_coding/jitter_estimator.h"
#include "modules/video_coding/timing.h"
#include "modules/video_coding/utility/vp8_header_parser.h"
#include "rtc_base/checks.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/platform_file.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/field_trial.h"
#include "video/call_stats.h"
#include "video/frame_dumping_decoder.h"
#include "video/receive_statistics_proxy.h"

namespace webrtc {

namespace {
VideoCodec CreateDecoderVideoCodec(const VideoReceiveStream::Decoder& decoder) {
  VideoCodec codec;
  memset(&codec, 0, sizeof(codec));

  codec.plType = decoder.payload_type;
  codec.codecType = PayloadStringToCodecType(decoder.video_format.name);

  if (codec.codecType == kVideoCodecVP8) {
    *(codec.VP8()) = VideoEncoder::GetDefaultVp8Settings();
  } else if (codec.codecType == kVideoCodecVP9) {
    *(codec.VP9()) = VideoEncoder::GetDefaultVp9Settings();
  } else if (codec.codecType == kVideoCodecH264) {
    *(codec.H264()) = VideoEncoder::GetDefaultH264Settings();
    codec.H264()->profile =
        H264::ParseSdpProfileLevelId(decoder.video_format.parameters)->profile;
  } else if (codec.codecType == kVideoCodecMultiplex) {
    VideoReceiveStream::Decoder associated_decoder = decoder;
    associated_decoder.video_format =
        SdpVideoFormat(CodecTypeToPayloadString(kVideoCodecVP9));
    VideoCodec associated_codec = CreateDecoderVideoCodec(associated_decoder);
    associated_codec.codecType = kVideoCodecMultiplex;
    return associated_codec;
  }

  codec.width = 320;
  codec.height = 180;
  const int kDefaultStartBitrate = 300;
  codec.startBitrate = codec.minBitrate = codec.maxBitrate =
      kDefaultStartBitrate;

  return codec;
}

// Video decoder class to be used for unknown codecs. Doesn't support decoding
// but logs messages to LS_ERROR.
class NullVideoDecoder : public webrtc::VideoDecoder {
 public:
  int32_t InitDecode(const webrtc::VideoCodec* codec_settings,
                     int32_t number_of_cores) override {
    RTC_LOG(LS_ERROR) << "Can't initialize NullVideoDecoder.";
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t Decode(const webrtc::EncodedImage& input_image,
                 bool missing_frames,
                 const webrtc::CodecSpecificInfo* codec_specific_info,
                 int64_t render_time_ms) override {
    RTC_LOG(LS_ERROR) << "The NullVideoDecoder doesn't support decoding.";
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* callback) override {
    RTC_LOG(LS_ERROR)
        << "Can't register decode complete callback on NullVideoDecoder.";
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t Release() override { return WEBRTC_VIDEO_CODEC_OK; }

  const char* ImplementationName() const override { return "NullVideoDecoder"; }
};

// TODO(https://bugs.webrtc.org/9974): Consider removing this workaround.
// Maximum time between frames before resetting the FrameBuffer to avoid RTP
// timestamps wraparound to affect FrameBuffer.
constexpr int kInactiveStreamThresholdMs = 600000;  //  10 minutes.

}  // namespace

namespace internal {

VideoReceiveStream::VideoReceiveStream(
    RtpStreamReceiverControllerInterface* receiver_controller,
    int num_cpu_cores,
    PacketRouter* packet_router,
    VideoReceiveStream::Config config,
    ProcessThread* process_thread,
    CallStats* call_stats)
    : transport_adapter_(config.rtcp_send_transport),
      config_(std::move(config)),
      num_cpu_cores_(num_cpu_cores),
      process_thread_(process_thread),
      clock_(Clock::GetRealTimeClock()),
      decode_thread_(&DecodeThreadFunction,
                     this,
                     "DecodingThread",
                     rtc::kHighestPriority),
      call_stats_(call_stats),
      stats_proxy_(&config_, clock_),
      rtp_receive_statistics_(
          ReceiveStatistics::Create(clock_, &stats_proxy_, &stats_proxy_)),
      timing_(new VCMTiming(clock_)),
      video_receiver_(clock_, timing_.get(), this, this),
      rtp_video_stream_receiver_(&transport_adapter_,
                                 call_stats,
                                 packet_router,
                                 &config_,
                                 rtp_receive_statistics_.get(),
                                 &stats_proxy_,
                                 process_thread_,
                                 this,  // NackSender
                                 this,  // KeyFrameRequestSender
                                 this,  // OnCompleteFrameCallback
                                 config_.frame_decryptor),
      rtp_stream_sync_(this) {
  RTC_LOG(LS_INFO) << "VideoReceiveStream: " << config_.ToString();

  RTC_DCHECK(process_thread_);
  RTC_DCHECK(call_stats_);

  module_process_sequence_checker_.Detach();

  RTC_DCHECK(!config_.decoders.empty());
  std::set<int> decoder_payload_types;
  for (const Decoder& decoder : config_.decoders) {
    RTC_CHECK(decoder.decoder_factory);
    RTC_CHECK(decoder_payload_types.find(decoder.payload_type) ==
              decoder_payload_types.end())
        << "Duplicate payload type (" << decoder.payload_type
        << ") for different decoders.";
    decoder_payload_types.insert(decoder.payload_type);
  }

  video_receiver_.SetRenderDelay(config_.render_delay_ms);

  jitter_estimator_.reset(new VCMJitterEstimator(clock_));
  frame_buffer_.reset(new video_coding::FrameBuffer(
      clock_, jitter_estimator_.get(), timing_.get(), &stats_proxy_));

  process_thread_->RegisterModule(&rtp_stream_sync_, RTC_FROM_HERE);

  // Register with RtpStreamReceiverController.
  media_receiver_ = receiver_controller->CreateReceiver(
      config_.rtp.remote_ssrc, &rtp_video_stream_receiver_);
  if (config_.rtp.rtx_ssrc) {
    rtx_receive_stream_ = absl::make_unique<RtxReceiveStream>(
        &rtp_video_stream_receiver_, config.rtp.rtx_associated_payload_types,
        config_.rtp.remote_ssrc, rtp_receive_statistics_.get());
    rtx_receiver_ = receiver_controller->CreateReceiver(
        config_.rtp.rtx_ssrc, rtx_receive_stream_.get());
  } else {
    rtp_receive_statistics_->EnableRetransmitDetection(config.rtp.remote_ssrc,
                                                       true);
  }
}

VideoReceiveStream::~VideoReceiveStream() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&worker_sequence_checker_);
  RTC_LOG(LS_INFO) << "~VideoReceiveStream: " << config_.ToString();
  Stop();

  process_thread_->DeRegisterModule(&rtp_stream_sync_);
}

void VideoReceiveStream::SignalNetworkState(NetworkState state) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&worker_sequence_checker_);
  rtp_video_stream_receiver_.SignalNetworkState(state);
}

bool VideoReceiveStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  return rtp_video_stream_receiver_.DeliverRtcp(packet, length);
}

void VideoReceiveStream::SetSync(Syncable* audio_syncable) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&worker_sequence_checker_);
  rtp_stream_sync_.ConfigureSync(audio_syncable);
}

void VideoReceiveStream::Start() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&worker_sequence_checker_);
  if (decode_thread_.IsRunning())
    return;

  bool protected_by_fec = config_.rtp.protected_by_flexfec ||
                          rtp_video_stream_receiver_.IsUlpfecEnabled();

  frame_buffer_->Start();

  if (rtp_video_stream_receiver_.IsRetransmissionsEnabled() &&
      protected_by_fec) {
    frame_buffer_->SetProtectionMode(kProtectionNackFEC);
  }

  transport_adapter_.Enable();
  rtc::VideoSinkInterface<VideoFrame>* renderer = nullptr;
  if (config_.renderer) {
    if (config_.disable_prerenderer_smoothing) {
      renderer = this;
    } else {
      incoming_video_stream_.reset(
          new IncomingVideoStream(config_.render_delay_ms, this));
      renderer = incoming_video_stream_.get();
    }
  }
  RTC_DCHECK(renderer != nullptr);

  for (const Decoder& decoder : config_.decoders) {
    std::unique_ptr<VideoDecoder> video_decoder =
        decoder.decoder_factory->LegacyCreateVideoDecoder(decoder.video_format,
                                                          config_.stream_id);
    // If we still have no valid decoder, we have to create a "Null" decoder
    // that ignores all calls. The reason we can get into this state is that the
    // old decoder factory interface doesn't have a way to query supported
    // codecs.
    if (!video_decoder) {
      video_decoder = absl::make_unique<NullVideoDecoder>();
    }

    std::string decoded_output_file =
        field_trial::FindFullName("WebRTC-DecoderDataDumpDirectory");
    if (!decoded_output_file.empty()) {
      char filename_buffer[256];
      rtc::SimpleStringBuilder ssb(filename_buffer);
      ssb << decoded_output_file << "/webrtc_receive_stream_"
          << this->config_.rtp.local_ssrc << ".ivf";
      video_decoder = absl::make_unique<FrameDumpingDecoder>(
          std::move(video_decoder), rtc::CreatePlatformFile(ssb.str()));
    }

    video_decoders_.push_back(std::move(video_decoder));

    video_receiver_.RegisterExternalDecoder(video_decoders_.back().get(),
                                            decoder.payload_type);
    VideoCodec codec = CreateDecoderVideoCodec(decoder);
    rtp_video_stream_receiver_.AddReceiveCodec(codec,
                                               decoder.video_format.parameters);
    RTC_CHECK_EQ(VCM_OK, video_receiver_.RegisterReceiveCodec(
                             &codec, num_cpu_cores_, false));
  }

  video_stream_decoder_.reset(new VideoStreamDecoder(
      &video_receiver_, &rtp_video_stream_receiver_,
      &rtp_video_stream_receiver_,
      rtp_video_stream_receiver_.IsRetransmissionsEnabled(), protected_by_fec,
      &stats_proxy_, renderer));

  // Make sure we register as a stats observer *after* we've prepared the
  // |video_stream_decoder_|.
  call_stats_->RegisterStatsObserver(this);

  process_thread_->RegisterModule(&video_receiver_, RTC_FROM_HERE);

  // Start the decode thread
  video_receiver_.DecoderThreadStarting();
  stats_proxy_.DecoderThreadStarting();
  decode_thread_.Start();
  rtp_video_stream_receiver_.StartReceive();
}

void VideoReceiveStream::Stop() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&worker_sequence_checker_);
  rtp_video_stream_receiver_.StopReceive();

  stats_proxy_.OnUniqueFramesCounted(
      rtp_video_stream_receiver_.GetUniqueFramesSeen());

  frame_buffer_->Stop();
  call_stats_->DeregisterStatsObserver(this);
  process_thread_->DeRegisterModule(&video_receiver_);

  if (decode_thread_.IsRunning()) {
    // TriggerDecoderShutdown will release any waiting decoder thread and make
    // it stop immediately, instead of waiting for a timeout. Needs to be called
    // before joining the decoder thread.
    video_receiver_.TriggerDecoderShutdown();

    decode_thread_.Stop();
    video_receiver_.DecoderThreadStopped();
    stats_proxy_.DecoderThreadStopped();
    // Deregister external decoders so they are no longer running during
    // destruction. This effectively stops the VCM since the decoder thread is
    // stopped, the VCM is deregistered and no asynchronous decoder threads are
    // running.
    for (const Decoder& decoder : config_.decoders)
      video_receiver_.RegisterExternalDecoder(nullptr, decoder.payload_type);
  }

  video_stream_decoder_.reset();
  incoming_video_stream_.reset();
  transport_adapter_.Disable();
}

VideoReceiveStream::Stats VideoReceiveStream::GetStats() const {
  return stats_proxy_.GetStats();
}

void VideoReceiveStream::AddSecondarySink(RtpPacketSinkInterface* sink) {
  rtp_video_stream_receiver_.AddSecondarySink(sink);
}

void VideoReceiveStream::RemoveSecondarySink(
    const RtpPacketSinkInterface* sink) {
  rtp_video_stream_receiver_.RemoveSecondarySink(sink);
}

// TODO(tommi): This method grabs a lock 6 times.
void VideoReceiveStream::OnFrame(const VideoFrame& video_frame) {
  int64_t sync_offset_ms;
  double estimated_freq_khz;
  // TODO(tommi): GetStreamSyncOffsetInMs grabs three locks.  One inside the
  // function itself, another in GetChannel() and a third in
  // GetPlayoutTimestamp.  Seems excessive.  Anyhow, I'm assuming the function
  // succeeds most of the time, which leads to grabbing a fourth lock.
  if (rtp_stream_sync_.GetStreamSyncOffsetInMs(
          video_frame.timestamp(), video_frame.render_time_ms(),
          &sync_offset_ms, &estimated_freq_khz)) {
    // TODO(tommi): OnSyncOffsetUpdated grabs a lock.
    stats_proxy_.OnSyncOffsetUpdated(sync_offset_ms, estimated_freq_khz);
  }
  // config_.renderer must never be null if we're getting this callback.
  config_.renderer->OnFrame(video_frame);

  // TODO(tommi): OnRenderFrame grabs a lock too.
  stats_proxy_.OnRenderedFrame(video_frame);
}

void VideoReceiveStream::SendNack(
    const std::vector<uint16_t>& sequence_numbers) {
  rtp_video_stream_receiver_.RequestPacketRetransmit(sequence_numbers);
}

void VideoReceiveStream::RequestKeyFrame() {
  rtp_video_stream_receiver_.RequestKeyFrame();
}

void VideoReceiveStream::OnCompleteFrame(
    std::unique_ptr<video_coding::EncodedFrame> frame) {
  // TODO(webrtc:9249): Workaround to allow decoding of VP9 SVC stream with
  // partially enabled inter-layer prediction.
  frame->id.spatial_layer = 0;

  // TODO(https://bugs.webrtc.org/9974): Consider removing this workaround.
  int64_t time_now_ms = rtc::TimeMillis();
  if (last_complete_frame_time_ms_ > 0 &&
      time_now_ms - last_complete_frame_time_ms_ > kInactiveStreamThresholdMs) {
    frame_buffer_->Clear();
  }
  last_complete_frame_time_ms_ = time_now_ms;

  int64_t last_continuous_pid = frame_buffer_->InsertFrame(std::move(frame));
  if (last_continuous_pid != -1)
    rtp_video_stream_receiver_.FrameContinuous(last_continuous_pid);
}

void VideoReceiveStream::OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&module_process_sequence_checker_);
  frame_buffer_->UpdateRtt(max_rtt_ms);
  rtp_video_stream_receiver_.UpdateRtt(max_rtt_ms);
  video_stream_decoder_->UpdateRtt(max_rtt_ms);
}

int VideoReceiveStream::id() const {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&worker_sequence_checker_);
  return config_.rtp.remote_ssrc;
}

absl::optional<Syncable::Info> VideoReceiveStream::GetInfo() const {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&module_process_sequence_checker_);
  absl::optional<Syncable::Info> info =
      rtp_video_stream_receiver_.GetSyncInfo();

  if (!info)
    return absl::nullopt;

  info->current_delay_ms = video_receiver_.Delay();
  return info;
}

uint32_t VideoReceiveStream::GetPlayoutTimestamp() const {
  RTC_NOTREACHED();
  return 0;
}

void VideoReceiveStream::SetMinimumPlayoutDelay(int delay_ms) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&module_process_sequence_checker_);
  video_receiver_.SetMinimumPlayoutDelay(delay_ms);
}

void VideoReceiveStream::DecodeThreadFunction(void* ptr) {
  while (static_cast<VideoReceiveStream*>(ptr)->Decode()) {
  }
}

bool VideoReceiveStream::Decode() {
  TRACE_EVENT0("webrtc", "VideoReceiveStream::Decode");
  static const int kMaxWaitForFrameMs = 3000;
  static const int kMaxWaitForKeyFrameMs = 200;

  int wait_ms = keyframe_required_ ? kMaxWaitForKeyFrameMs : kMaxWaitForFrameMs;
  std::unique_ptr<video_coding::EncodedFrame> frame;
  // TODO(philipel): Call NextFrame with |keyframe_required| argument when
  //                 downstream project has been fixed.
  video_coding::FrameBuffer::ReturnReason res =
      frame_buffer_->NextFrame(wait_ms, &frame);

  if (res == video_coding::FrameBuffer::ReturnReason::kStopped) {
    return false;
  }

  if (frame) {
    int64_t now_ms = clock_->TimeInMilliseconds();
    RTC_DCHECK_EQ(res, video_coding::FrameBuffer::ReturnReason::kFrameFound);

    // Current OnPreDecode only cares about QP for VP8.
    int qp = -1;
    if (frame->CodecSpecific()->codecType == kVideoCodecVP8) {
      if (!vp8::GetQp(frame->Buffer(), frame->Length(), &qp)) {
        RTC_LOG(LS_WARNING) << "Failed to extract QP from VP8 video frame";
      }
    }
    stats_proxy_.OnPreDecode(frame->CodecSpecific()->codecType, qp);

    int decode_result = video_receiver_.Decode(frame.get());
    if (decode_result == WEBRTC_VIDEO_CODEC_OK ||
        decode_result == WEBRTC_VIDEO_CODEC_OK_REQUEST_KEYFRAME) {
      keyframe_required_ = false;
      frame_decoded_ = true;
      rtp_video_stream_receiver_.FrameDecoded(frame->id.picture_id);

      if (decode_result == WEBRTC_VIDEO_CODEC_OK_REQUEST_KEYFRAME)
        RequestKeyFrame();
    } else if (!frame_decoded_ || !keyframe_required_ ||
               (last_keyframe_request_ms_ + kMaxWaitForKeyFrameMs < now_ms)) {
      keyframe_required_ = true;
      // TODO(philipel): Remove this keyframe request when downstream project
      //                 has been fixed.
      RequestKeyFrame();
      last_keyframe_request_ms_ = now_ms;
    }
  } else {
    RTC_DCHECK_EQ(res, video_coding::FrameBuffer::ReturnReason::kTimeout);
    int64_t now_ms = clock_->TimeInMilliseconds();
    absl::optional<int64_t> last_packet_ms =
        rtp_video_stream_receiver_.LastReceivedPacketMs();
    absl::optional<int64_t> last_keyframe_packet_ms =
        rtp_video_stream_receiver_.LastReceivedKeyframePacketMs();

    // To avoid spamming keyframe requests for a stream that is not active we
    // check if we have received a packet within the last 5 seconds.
    bool stream_is_active = last_packet_ms && now_ms - *last_packet_ms < 5000;
    if (!stream_is_active)
      stats_proxy_.OnStreamInactive();

    // If we recently have been receiving packets belonging to a keyframe then
    // we assume a keyframe is currently being received.
    bool receiving_keyframe =
        last_keyframe_packet_ms &&
        now_ms - *last_keyframe_packet_ms < kMaxWaitForKeyFrameMs;

    if (stream_is_active && !receiving_keyframe) {
      RTC_LOG(LS_WARNING) << "No decodable frame in " << wait_ms
                          << " ms, requesting keyframe.";
      RequestKeyFrame();
    }
  }
  return true;
}

std::vector<webrtc::RtpSource> VideoReceiveStream::GetSources() const {
  return rtp_video_stream_receiver_.GetSources();
}

}  // namespace internal
}  // namespace webrtc
