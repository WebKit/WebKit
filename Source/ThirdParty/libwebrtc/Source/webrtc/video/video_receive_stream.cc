/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/video_receive_stream.h"

#include <stdlib.h>

#include <set>
#include <string>
#include <utility>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/optional.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/congestion_controller/include/congestion_controller.h"
#include "webrtc/modules/utility/include/process_thread.h"
#include "webrtc/modules/video_coding/frame_object.h"
#include "webrtc/modules/video_coding/include/video_coding.h"
#include "webrtc/modules/video_coding/jitter_estimator.h"
#include "webrtc/modules/video_coding/timing.h"
#include "webrtc/modules/video_coding/utility/ivf_file_writer.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/system_wrappers/include/field_trial.h"
#include "webrtc/video/call_stats.h"
#include "webrtc/video/receive_statistics_proxy.h"
#include "webrtc/video_receive_stream.h"
#include "webrtc/voice_engine/include/voe_video_sync.h"

namespace webrtc {

static bool UseSendSideBwe(const VideoReceiveStream::Config& config) {
  if (!config.rtp.transport_cc)
    return false;
  for (const auto& extension : config.rtp.extensions) {
    if (extension.uri == RtpExtension::kTransportSequenceNumberUri)
      return true;
  }
  return false;
}

std::string VideoReceiveStream::Decoder::ToString() const {
  std::stringstream ss;
  ss << "{decoder: " << (decoder ? "(VideoDecoder)" : "nullptr");
  ss << ", payload_type: " << payload_type;
  ss << ", payload_name: " << payload_name;
  ss << ", decoder_specific: {";
  ss << " h264_extra_settings: "
     << (decoder_specific.h264_extra_settings ? "(h264_extra_settings)"
                                              : "nullptr");
  ss << '}';
  ss << '}';

  return ss.str();
}

std::string VideoReceiveStream::Config::ToString() const {
  std::stringstream ss;
  ss << "{decoders: [";
  for (size_t i = 0; i < decoders.size(); ++i) {
    ss << decoders[i].ToString();
    if (i != decoders.size() - 1)
      ss << ", ";
  }
  ss << ']';
  ss << ", rtp: " << rtp.ToString();
  ss << ", renderer: " << (renderer ? "(renderer)" : "nullptr");
  ss << ", render_delay_ms: " << render_delay_ms;
  if (!sync_group.empty())
    ss << ", sync_group: " << sync_group;
  ss << ", pre_decode_callback: "
     << (pre_decode_callback ? "(EncodedFrameObserver)" : "nullptr");
  ss << ", pre_render_callback: "
     << (pre_render_callback ? "(I420FrameCallback)" : "nullptr");
  ss << ", target_delay_ms: " << target_delay_ms;
  ss << '}';

  return ss.str();
}

std::string VideoReceiveStream::Config::Rtp::ToString() const {
  std::stringstream ss;
  ss << "{remote_ssrc: " << remote_ssrc;
  ss << ", local_ssrc: " << local_ssrc;
  ss << ", rtcp_mode: "
     << (rtcp_mode == RtcpMode::kCompound ? "RtcpMode::kCompound"
                                          : "RtcpMode::kReducedSize");
  ss << ", rtcp_xr: ";
  ss << "{receiver_reference_time_report: "
     << (rtcp_xr.receiver_reference_time_report ? "on" : "off");
  ss << '}';
  ss << ", remb: " << (remb ? "on" : "off");
  ss << ", transport_cc: " << (transport_cc ? "on" : "off");
  ss << ", nack: {rtp_history_ms: " << nack.rtp_history_ms << '}';
  ss << ", ulpfec: " << ulpfec.ToString();
  ss << ", rtx: {";
  for (auto& kv : rtx) {
    ss << kv.first << " -> ";
    ss << "{ssrc: " << kv.second.ssrc;
    ss << ", payload_type: " << kv.second.payload_type;
    ss << '}';
  }
  ss << '}';
  ss << ", extensions: [";
  for (size_t i = 0; i < extensions.size(); ++i) {
    ss << extensions[i].ToString();
    if (i != extensions.size() - 1)
      ss << ", ";
  }
  ss << ']';
  ss << '}';
  return ss.str();
}

std::string VideoReceiveStream::Stats::ToString(int64_t time_ms) const {
  std::stringstream ss;
  ss << "VideoReceiveStream stats: " << time_ms << ", {ssrc: " << ssrc << ", ";
  ss << "total_bps: " << total_bitrate_bps << ", ";
  ss << "width: " << width << ", ";
  ss << "height: " << height << ", ";
  ss << "key: " << frame_counts.key_frames << ", ";
  ss << "delta: " << frame_counts.delta_frames << ", ";
  ss << "network_fps: " << network_frame_rate << ", ";
  ss << "decode_fps: " << decode_frame_rate << ", ";
  ss << "render_fps: " << render_frame_rate << ", ";
  ss << "decode_ms: " << decode_ms << ", ";
  ss << "max_decode_ms: " << max_decode_ms << ", ";
  ss << "cur_delay_ms: " << current_delay_ms << ", ";
  ss << "targ_delay_ms: " << target_delay_ms << ", ";
  ss << "jb_delay_ms: " << jitter_buffer_ms << ", ";
  ss << "min_playout_delay_ms: " << min_playout_delay_ms << ", ";
  ss << "discarded: " << discarded_packets << ", ";
  ss << "sync_offset_ms: " << sync_offset_ms << ", ";
  ss << "cum_loss: " << rtcp_stats.cumulative_lost << ", ";
  ss << "max_ext_seq: " << rtcp_stats.extended_max_sequence_number << ", ";
  ss << "nack: " << rtcp_packet_type_counts.nack_packets << ", ";
  ss << "fir: " << rtcp_packet_type_counts.fir_packets << ", ";
  ss << "pli: " << rtcp_packet_type_counts.pli_packets;
  ss << '}';
  return ss.str();
}

namespace {
VideoCodec CreateDecoderVideoCodec(const VideoReceiveStream::Decoder& decoder) {
  VideoCodec codec;
  memset(&codec, 0, sizeof(codec));

  codec.plType = decoder.payload_type;
  strncpy(codec.plName, decoder.payload_name.c_str(), sizeof(codec.plName));
  if (decoder.payload_name == "VP8") {
    codec.codecType = kVideoCodecVP8;
  } else if (decoder.payload_name == "VP9") {
    codec.codecType = kVideoCodecVP9;
  } else if (decoder.payload_name == "H264") {
    codec.codecType = kVideoCodecH264;
  } else {
    codec.codecType = kVideoCodecGeneric;
  }

  if (codec.codecType == kVideoCodecVP8) {
    *(codec.VP8()) = VideoEncoder::GetDefaultVp8Settings();
  } else if (codec.codecType == kVideoCodecVP9) {
    *(codec.VP9()) = VideoEncoder::GetDefaultVp9Settings();
  } else if (codec.codecType == kVideoCodecH264) {
    *(codec.H264()) = VideoEncoder::GetDefaultH264Settings();
  }

  codec.width = 320;
  codec.height = 180;
  const int kDefaultStartBitrate = 300;
  codec.startBitrate = codec.minBitrate = codec.maxBitrate =
      kDefaultStartBitrate;

  return codec;
}
}  // namespace

namespace internal {

VideoReceiveStream::VideoReceiveStream(
    int num_cpu_cores,
    CongestionController* congestion_controller,
    VideoReceiveStream::Config config,
    webrtc::VoiceEngine* voice_engine,
    ProcessThread* process_thread,
    CallStats* call_stats,
    VieRemb* remb)
    : transport_adapter_(config.rtcp_send_transport),
      config_(std::move(config)),
      num_cpu_cores_(num_cpu_cores),
      process_thread_(process_thread),
      clock_(Clock::GetRealTimeClock()),
      decode_thread_(DecodeThreadFunction, this, "DecodingThread"),
      congestion_controller_(congestion_controller),
      call_stats_(call_stats),
      video_receiver_(clock_, nullptr, this, this, this),
      stats_proxy_(&config_, clock_),
      timing_(new VCMTiming(clock_)),
      rtp_stream_receiver_(
          &video_receiver_,
          congestion_controller_->GetRemoteBitrateEstimator(
              UseSendSideBwe(config_)),
          &transport_adapter_,
          call_stats_->rtcp_rtt_stats(),
          congestion_controller_->pacer(),
          congestion_controller_->packet_router(),
          remb,
          &config_,
          &stats_proxy_,
          process_thread_,
          congestion_controller_->GetRetransmissionRateLimiter(),
          this,  // NackSender
          this,  // KeyFrameRequestSender
          this,  // OnCompleteFrameCallback
          timing_.get()),
      rtp_stream_sync_(&video_receiver_, &rtp_stream_receiver_),
      jitter_buffer_experiment_(
          field_trial::FindFullName("WebRTC-NewVideoJitterBuffer") ==
          "Enabled") {
  LOG(LS_INFO) << "VideoReceiveStream: " << config_.ToString();

  RTC_DCHECK(process_thread_);
  RTC_DCHECK(congestion_controller_);
  RTC_DCHECK(call_stats_);

  RTC_DCHECK(!config_.decoders.empty());
  std::set<int> decoder_payload_types;
  for (const Decoder& decoder : config_.decoders) {
    RTC_CHECK(decoder.decoder);
    RTC_CHECK(decoder_payload_types.find(decoder.payload_type) ==
              decoder_payload_types.end())
        << "Duplicate payload type (" << decoder.payload_type
        << ") for different decoders.";
    decoder_payload_types.insert(decoder.payload_type);
  }

  video_receiver_.SetRenderDelay(config.render_delay_ms);

  if (jitter_buffer_experiment_) {
    jitter_estimator_.reset(new VCMJitterEstimator(clock_));
    frame_buffer_.reset(new video_coding::FrameBuffer(
        clock_, jitter_estimator_.get(), timing_.get()));
  }

  process_thread_->RegisterModule(&video_receiver_);
  process_thread_->RegisterModule(&rtp_stream_sync_);
}

VideoReceiveStream::~VideoReceiveStream() {
  LOG(LS_INFO) << "~VideoReceiveStream: " << config_.ToString();
  Stop();

  process_thread_->DeRegisterModule(&rtp_stream_sync_);
  process_thread_->DeRegisterModule(&video_receiver_);

  congestion_controller_->GetRemoteBitrateEstimator(UseSendSideBwe(config_))
      ->RemoveStream(rtp_stream_receiver_.GetRemoteSsrc());
}

void VideoReceiveStream::SignalNetworkState(NetworkState state) {
  rtp_stream_receiver_.SignalNetworkState(state);
}


bool VideoReceiveStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  return rtp_stream_receiver_.DeliverRtcp(packet, length);
}

bool VideoReceiveStream::DeliverRtp(const uint8_t* packet,
                                    size_t length,
                                    const PacketTime& packet_time) {
  return rtp_stream_receiver_.DeliverRtp(packet, length, packet_time);
}

bool VideoReceiveStream::OnRecoveredPacket(const uint8_t* packet,
                                           size_t length) {
  return rtp_stream_receiver_.OnRecoveredPacket(packet, length);
}

void VideoReceiveStream::Start() {
  if (decode_thread_.IsRunning())
    return;
  if (jitter_buffer_experiment_) {
    frame_buffer_->Start();
    call_stats_->RegisterStatsObserver(&rtp_stream_receiver_);

    if (rtp_stream_receiver_.IsRetransmissionsEnabled() &&
        rtp_stream_receiver_.IsFecEnabled()) {
      frame_buffer_->SetProtectionMode(kProtectionNackFEC);
    }
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
    video_receiver_.RegisterExternalDecoder(decoder.decoder,
                                            decoder.payload_type);

    VideoCodec codec = CreateDecoderVideoCodec(decoder);
    RTC_CHECK(rtp_stream_receiver_.SetReceiveCodec(codec));
    RTC_CHECK_EQ(VCM_OK, video_receiver_.RegisterReceiveCodec(
                             &codec, num_cpu_cores_, false));
  }

  video_stream_decoder_.reset(new VideoStreamDecoder(
      &video_receiver_, &rtp_stream_receiver_, &rtp_stream_receiver_,
      rtp_stream_receiver_.IsRetransmissionsEnabled(),
      rtp_stream_receiver_.IsFecEnabled(), &stats_proxy_, renderer,
      config_.pre_render_callback));
  // Register the channel to receive stats updates.
  call_stats_->RegisterStatsObserver(video_stream_decoder_.get());
  // Start the decode thread
  decode_thread_.Start();
  decode_thread_.SetPriority(rtc::kHighestPriority);
  rtp_stream_receiver_.StartReceive();
}

void VideoReceiveStream::Stop() {
  rtp_stream_receiver_.StopReceive();
  // TriggerDecoderShutdown will release any waiting decoder thread and make it
  // stop immediately, instead of waiting for a timeout. Needs to be called
  // before joining the decoder thread thread.
  video_receiver_.TriggerDecoderShutdown();

  if (jitter_buffer_experiment_) {
    frame_buffer_->Stop();
    call_stats_->DeregisterStatsObserver(&rtp_stream_receiver_);
  }

  if (decode_thread_.IsRunning()) {
    decode_thread_.Stop();
    // Deregister external decoders so they are no longer running during
    // destruction. This effectively stops the VCM since the decoder thread is
    // stopped, the VCM is deregistered and no asynchronous decoder threads are
    // running.
    for (const Decoder& decoder : config_.decoders)
      video_receiver_.RegisterExternalDecoder(nullptr, decoder.payload_type);
  }

  call_stats_->DeregisterStatsObserver(video_stream_decoder_.get());
  video_stream_decoder_.reset();
  incoming_video_stream_.reset();
  transport_adapter_.Disable();
}

void VideoReceiveStream::SetSyncChannel(VoiceEngine* voice_engine,
                                        int audio_channel_id) {
  if (voice_engine && audio_channel_id != -1) {
    VoEVideoSync* voe_sync_interface = VoEVideoSync::GetInterface(voice_engine);
    rtp_stream_sync_.ConfigureSync(audio_channel_id, voe_sync_interface);
    voe_sync_interface->Release();
  } else {
    rtp_stream_sync_.ConfigureSync(-1, nullptr);
  }
}

VideoReceiveStream::Stats VideoReceiveStream::GetStats() const {
  return stats_proxy_.GetStats();
}

// TODO(tommi): This method grabs a lock 6 times.
void VideoReceiveStream::OnFrame(const VideoFrame& video_frame) {
  // TODO(tommi): OnDecodedFrame grabs a lock, incidentally the same lock
  // that OnSyncOffsetUpdated() and OnRenderedFrame() below grab.
  stats_proxy_.OnDecodedFrame();

  int64_t sync_offset_ms;
  double estimated_freq_khz;
  // TODO(tommi): GetStreamSyncOffsetInMs grabs three locks.  One inside the
  // function itself, another in GetChannel() and a third in
  // GetPlayoutTimestamp.  Seems excessive.  Anyhow, I'm assuming the function
  // succeeds most of the time, which leads to grabbing a fourth lock.
  if (rtp_stream_sync_.GetStreamSyncOffsetInMs(video_frame, &sync_offset_ms,
                                               &estimated_freq_khz)) {
    // TODO(tommi): OnSyncOffsetUpdated grabs a lock.
    stats_proxy_.OnSyncOffsetUpdated(sync_offset_ms, estimated_freq_khz);
  }

  // config_.renderer must never be null if we're getting this callback.
  config_.renderer->OnFrame(video_frame);

  // TODO(tommi): OnRenderFrame grabs a lock too.
  stats_proxy_.OnRenderedFrame(video_frame);
}

void VideoReceiveStream::OnCompleteFrame(
    std::unique_ptr<video_coding::FrameObject> frame) {
  int last_continuous_pid = frame_buffer_->InsertFrame(std::move(frame));
  if (last_continuous_pid != -1)
    rtp_stream_receiver_.FrameContinuous(last_continuous_pid);
}

// TODO(asapersson): Consider moving callback from video_encoder.h or
// creating a different callback.
EncodedImageCallback::Result VideoReceiveStream::OnEncodedImage(
    const EncodedImage& encoded_image,
    const CodecSpecificInfo* codec_specific_info,
    const RTPFragmentationHeader* fragmentation) {
  stats_proxy_.OnPreDecode(encoded_image, codec_specific_info);
  if (config_.pre_decode_callback) {
    config_.pre_decode_callback->EncodedFrameCallback(
        EncodedFrame(encoded_image._buffer, encoded_image._length,
                     encoded_image._frameType));
  }
  {
    rtc::CritScope lock(&ivf_writer_lock_);
    if (ivf_writer_.get()) {
      RTC_DCHECK(codec_specific_info);
      bool ok = ivf_writer_->WriteFrame(encoded_image,
                                        codec_specific_info->codecType);
      RTC_DCHECK(ok);
    }
  }

  return Result(Result::OK, encoded_image._timeStamp);
}

bool VideoReceiveStream::DecodeThreadFunction(void* ptr) {
  static_cast<VideoReceiveStream*>(ptr)->Decode();
  return true;
}

void VideoReceiveStream::Decode() {
  static const int kMaxDecodeWaitTimeMs = 50;
  if (jitter_buffer_experiment_) {
    static const int kMaxWaitForFrameMs = 3000;
    std::unique_ptr<video_coding::FrameObject> frame;
    video_coding::FrameBuffer::ReturnReason res =
        frame_buffer_->NextFrame(kMaxWaitForFrameMs, &frame);

    if (res == video_coding::FrameBuffer::ReturnReason::kStopped)
      return;

    if (frame) {
      if (video_receiver_.Decode(frame.get()) == VCM_OK)
        rtp_stream_receiver_.FrameDecoded(frame->picture_id);
    } else {
      LOG(LS_WARNING) << "No decodable frame in " << kMaxWaitForFrameMs
                      << " ms, requesting keyframe.";
      RequestKeyFrame();
    }
  } else {
    video_receiver_.Decode(kMaxDecodeWaitTimeMs);
  }
}

void VideoReceiveStream::SendNack(
    const std::vector<uint16_t>& sequence_numbers) {
  rtp_stream_receiver_.RequestPacketRetransmit(sequence_numbers);
}

void VideoReceiveStream::EnableEncodedFrameRecording(rtc::PlatformFile file,
                                                     size_t byte_limit) {
  {
    rtc::CritScope lock(&ivf_writer_lock_);
    if (file == rtc::kInvalidPlatformFileValue) {
      ivf_writer_.reset();
    } else {
      ivf_writer_ = IvfFileWriter::Wrap(rtc::File(file), byte_limit);
    }
  }

  if (file != rtc::kInvalidPlatformFileValue) {
    // Make a keyframe appear as early as possible in the logs, to give actually
    // decodable output.
    RequestKeyFrame();
  }
}

void VideoReceiveStream::RequestKeyFrame() {
  rtp_stream_receiver_.RequestKeyFrame();
}

}  // namespace internal
}  // namespace webrtc
