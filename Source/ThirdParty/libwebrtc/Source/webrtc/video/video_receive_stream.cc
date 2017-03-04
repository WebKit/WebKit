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
#include "webrtc/base/location.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/optional.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/common_video/h264/profile_level_id.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_receiver.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
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

namespace webrtc {

std::string VideoReceiveStream::Decoder::ToString() const {
  std::stringstream ss;
  ss << "{decoder: " << (decoder ? "(VideoDecoder)" : "nullptr");
  ss << ", payload_type: " << payload_type;
  ss << ", payload_name: " << payload_name;
  ss << ", codec_params: {";
  for (const auto& it : codec_params)
    ss << it.first << ": " << it.second;
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
  ss << ", rtx_ssrc: " << rtx_ssrc;
  ss << ", rtx_payload_types: {";
  for (auto& kv : rtx_payload_types) {
    ss << kv.first << " (apt) -> " << kv.second << " (pt), ";
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
    codec.H264()->profile =
        H264::ParseSdpProfileLevelId(decoder.codec_params)->profile;
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
    bool protected_by_flexfec,
    PacketRouter* packet_router,
    VideoReceiveStream::Config config,
    ProcessThread* process_thread,
    CallStats* call_stats,
    VieRemb* remb)
    : transport_adapter_(config.rtcp_send_transport),
      config_(std::move(config)),
      num_cpu_cores_(num_cpu_cores),
      protected_by_flexfec_(protected_by_flexfec),
      process_thread_(process_thread),
      clock_(Clock::GetRealTimeClock()),
      decode_thread_(DecodeThreadFunction, this, "DecodingThread"),
      call_stats_(call_stats),
      timing_(new VCMTiming(clock_)),
      video_receiver_(clock_, nullptr, this, timing_.get(), this, this),
      stats_proxy_(&config_, clock_),
      rtp_stream_receiver_(&transport_adapter_,
                           call_stats_->rtcp_rtt_stats(),
                           packet_router,
                           remb,
                           &config_,
                           &stats_proxy_,
                           process_thread_,
                           this,  // NackSender
                           this,  // KeyFrameRequestSender
                           this,  // OnCompleteFrameCallback
                           timing_.get()),
      rtp_stream_sync_(this) {
  LOG(LS_INFO) << "VideoReceiveStream: " << config_.ToString();

  RTC_DCHECK(process_thread_);
  RTC_DCHECK(call_stats_);

  module_process_thread_checker_.DetachFromThread();

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

  jitter_estimator_.reset(new VCMJitterEstimator(clock_));
  frame_buffer_.reset(new video_coding::FrameBuffer(
      clock_, jitter_estimator_.get(), timing_.get(), &stats_proxy_));

  process_thread_->RegisterModule(&video_receiver_, RTC_FROM_HERE);
  process_thread_->RegisterModule(&rtp_stream_sync_, RTC_FROM_HERE);
}

VideoReceiveStream::~VideoReceiveStream() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  LOG(LS_INFO) << "~VideoReceiveStream: " << config_.ToString();
  Stop();

  process_thread_->DeRegisterModule(&rtp_stream_sync_);
  process_thread_->DeRegisterModule(&video_receiver_);
}

void VideoReceiveStream::SignalNetworkState(NetworkState state) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  rtp_stream_receiver_.SignalNetworkState(state);
}


bool VideoReceiveStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  return rtp_stream_receiver_.DeliverRtcp(packet, length);
}

void VideoReceiveStream::OnRtpPacket(const RtpPacketReceived& packet) {
  rtp_stream_receiver_.OnRtpPacket(packet);
}

bool VideoReceiveStream::OnRecoveredPacket(const uint8_t* packet,
                                           size_t length) {
  return rtp_stream_receiver_.OnRecoveredPacket(packet, length);
}

void VideoReceiveStream::SetSync(Syncable* audio_syncable) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  rtp_stream_sync_.ConfigureSync(audio_syncable);
}

void VideoReceiveStream::Start() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  if (decode_thread_.IsRunning())
    return;

  bool protected_by_fec =
      protected_by_flexfec_ || rtp_stream_receiver_.IsUlpfecEnabled();

  frame_buffer_->Start();
  call_stats_->RegisterStatsObserver(&rtp_stream_receiver_);

  if (rtp_stream_receiver_.IsRetransmissionsEnabled() && protected_by_fec) {
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
    video_receiver_.RegisterExternalDecoder(decoder.decoder,
                                            decoder.payload_type);
    VideoCodec codec = CreateDecoderVideoCodec(decoder);
    RTC_CHECK(
        rtp_stream_receiver_.AddReceiveCodec(codec, decoder.codec_params));
    RTC_CHECK_EQ(VCM_OK, video_receiver_.RegisterReceiveCodec(
                             &codec, num_cpu_cores_, false));
  }

  video_stream_decoder_.reset(new VideoStreamDecoder(
      &video_receiver_, &rtp_stream_receiver_, &rtp_stream_receiver_,
      rtp_stream_receiver_.IsRetransmissionsEnabled(), protected_by_fec,
      &stats_proxy_, renderer));
  // Register the channel to receive stats updates.
  call_stats_->RegisterStatsObserver(video_stream_decoder_.get());
  // Start the decode thread
  decode_thread_.Start();
  decode_thread_.SetPriority(rtc::kHighestPriority);
  rtp_stream_receiver_.StartReceive();
}

void VideoReceiveStream::Stop() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  rtp_stream_receiver_.StopReceive();
  // TriggerDecoderShutdown will release any waiting decoder thread and make it
  // stop immediately, instead of waiting for a timeout. Needs to be called
  // before joining the decoder thread thread.
  video_receiver_.TriggerDecoderShutdown();

  frame_buffer_->Stop();
  call_stats_->DeregisterStatsObserver(&rtp_stream_receiver_);

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

VideoReceiveStream::Stats VideoReceiveStream::GetStats() const {
  return stats_proxy_.GetStats();
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

// TODO(tommi): This method grabs a lock 6 times.
void VideoReceiveStream::OnFrame(const VideoFrame& video_frame) {
  int64_t sync_offset_ms;
  double estimated_freq_khz;
  // TODO(tommi): GetStreamSyncOffsetInMs grabs three locks.  One inside the
  // function itself, another in GetChannel() and a third in
  // GetPlayoutTimestamp.  Seems excessive.  Anyhow, I'm assuming the function
  // succeeds most of the time, which leads to grabbing a fourth lock.
  if (rtp_stream_sync_.GetStreamSyncOffsetInMs(video_frame.timestamp(),
                                               video_frame.render_time_ms(),
                                               &sync_offset_ms,
                                               &estimated_freq_khz)) {
    // TODO(tommi): OnSyncOffsetUpdated grabs a lock.
    stats_proxy_.OnSyncOffsetUpdated(sync_offset_ms, estimated_freq_khz);
  }
  // config_.renderer must never be null if we're getting this callback.
  config_.renderer->OnFrame(video_frame);

  // TODO(tommi): OnRenderFrame grabs a lock too.
  stats_proxy_.OnRenderedFrame(video_frame);
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
                     encoded_image._frameType, encoded_image._encodedWidth,
                     encoded_image._encodedHeight, encoded_image._timeStamp));
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

void VideoReceiveStream::SendNack(
    const std::vector<uint16_t>& sequence_numbers) {
  rtp_stream_receiver_.RequestPacketRetransmit(sequence_numbers);
}

void VideoReceiveStream::RequestKeyFrame() {
  rtp_stream_receiver_.RequestKeyFrame();
}

void VideoReceiveStream::OnCompleteFrame(
    std::unique_ptr<video_coding::FrameObject> frame) {
  int last_continuous_pid = frame_buffer_->InsertFrame(std::move(frame));
  if (last_continuous_pid != -1)
    rtp_stream_receiver_.FrameContinuous(last_continuous_pid);
}

int VideoReceiveStream::id() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return config_.rtp.remote_ssrc;
}

rtc::Optional<Syncable::Info> VideoReceiveStream::GetInfo() const {
  RTC_DCHECK_RUN_ON(&module_process_thread_checker_);
  Syncable::Info info;

  RtpReceiver* rtp_receiver = rtp_stream_receiver_.GetRtpReceiver();
  RTC_DCHECK(rtp_receiver);
  if (!rtp_receiver->Timestamp(&info.latest_received_capture_timestamp))
    return rtc::Optional<Syncable::Info>();
  if (!rtp_receiver->LastReceivedTimeMs(&info.latest_receive_time_ms))
    return rtc::Optional<Syncable::Info>();

  RtpRtcp* rtp_rtcp = rtp_stream_receiver_.rtp_rtcp();
  RTC_DCHECK(rtp_rtcp);
  if (rtp_rtcp->RemoteNTP(&info.capture_time_ntp_secs,
                          &info.capture_time_ntp_frac,
                          nullptr,
                          nullptr,
                          &info.capture_time_source_clock) != 0) {
    return rtc::Optional<Syncable::Info>();
  }

  info.current_delay_ms = video_receiver_.Delay();
  return rtc::Optional<Syncable::Info>(info);
}

uint32_t VideoReceiveStream::GetPlayoutTimestamp() const {
  RTC_NOTREACHED();
  return 0;
}

void VideoReceiveStream::SetMinimumPlayoutDelay(int delay_ms) {
  RTC_DCHECK_RUN_ON(&module_process_thread_checker_);
  video_receiver_.SetMinimumPlayoutDelay(delay_ms);
}

bool VideoReceiveStream::DecodeThreadFunction(void* ptr) {
  return static_cast<VideoReceiveStream*>(ptr)->Decode();
}

bool VideoReceiveStream::Decode() {
  TRACE_EVENT0("webrtc", "VideoReceiveStream::Decode");
  static const int kMaxWaitForFrameMs = 3000;
  std::unique_ptr<video_coding::FrameObject> frame;
  video_coding::FrameBuffer::ReturnReason res =
      frame_buffer_->NextFrame(kMaxWaitForFrameMs, &frame);

  if (res == video_coding::FrameBuffer::ReturnReason::kStopped)
    return false;

  if (frame) {
    if (video_receiver_.Decode(frame.get()) == VCM_OK)
      rtp_stream_receiver_.FrameDecoded(frame->picture_id);
  } else {
    LOG(LS_WARNING) << "No decodable frame in " << kMaxWaitForFrameMs
                    << " ms, requesting keyframe.";
    RequestKeyFrame();
  }
  return true;
}
}  // namespace internal
}  // namespace webrtc
