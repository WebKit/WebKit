/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "video/video_quality_test.h"

#include <stdio.h>
#include <algorithm>
#include <deque>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "logging/rtc_event_log/output/rtc_event_log_output_file.h"
#include "media/engine/internalencoderfactory.h"
#include "media/engine/webrtcvideoengine.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/rtp_rtcp/source/rtp_format.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "modules/video_coding/codecs/vp8/include/vp8_common_types.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "rtc_base/cpu_time.h"
#include "rtc_base/flags.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/memory_usage.h"
#include "rtc_base/pathutils.h"
#include "system_wrappers/include/cpu_info.h"
#include "test/rtp_file_writer.h"
#include "test/run_loop.h"
#include "test/statistics.h"
#include "test/testsupport/fileutils.h"
#include "test/testsupport/frame_writer.h"
#include "test/testsupport/perf_test.h"
#include "test/testsupport/test_artifacts.h"
#include "test/vcm_capturer.h"
#include "test/video_renderer.h"

DEFINE_bool(save_worst_frame,
            false,
            "Enable saving a frame with the lowest PSNR to a jpeg file in the "
            "test_artifacts_dir");

namespace {

constexpr int kSendStatsPollingIntervalMs = 1000;

constexpr size_t kMaxComparisons = 10;
constexpr char kSyncGroup[] = "av_sync";
constexpr int kOpusMinBitrateBps = 6000;
constexpr int kOpusBitrateFbBps = 32000;
constexpr int kFramesSentInQuickTest = 1;
constexpr uint32_t kThumbnailSendSsrcStart = 0xE0000;
constexpr uint32_t kThumbnailRtxSsrcStart = 0xF0000;

constexpr int kDefaultMaxQp = cricket::WebRtcVideoChannel::kDefaultQpMax;

class VideoStreamFactory
    : public webrtc::VideoEncoderConfig::VideoStreamFactoryInterface {
 public:
  explicit VideoStreamFactory(const std::vector<webrtc::VideoStream>& streams)
      : streams_(streams) {}

 private:
  std::vector<webrtc::VideoStream> CreateEncoderStreams(
      int width,
      int height,
      const webrtc::VideoEncoderConfig& encoder_config) override {
    // The highest layer must match the incoming resolution.
    std::vector<webrtc::VideoStream> streams = streams_;
    streams[streams_.size() - 1].height = height;
    streams[streams_.size() - 1].width = width;

    streams[0].bitrate_priority = encoder_config.bitrate_priority;
    return streams;
  }

  std::vector<webrtc::VideoStream> streams_;
};

bool IsFlexfec(int payload_type) {
  return payload_type == webrtc::VideoQualityTest::kFlexfecPayloadType;
}

}  // namespace

namespace webrtc {

class VideoAnalyzer : public PacketReceiver,
                      public Transport,
                      public rtc::VideoSinkInterface<VideoFrame> {
 public:
  VideoAnalyzer(test::LayerFilteringTransport* transport,
                const std::string& test_label,
                double avg_psnr_threshold,
                double avg_ssim_threshold,
                int duration_frames,
                FILE* graph_data_output_file,
                const std::string& graph_title,
                uint32_t ssrc_to_analyze,
                uint32_t rtx_ssrc_to_analyze,
                size_t selected_stream,
                int selected_sl,
                int selected_tl,
                bool is_quick_test_enabled,
                Clock* clock,
                std::string rtp_dump_name)
      : transport_(transport),
        receiver_(nullptr),
        call_(nullptr),
        send_stream_(nullptr),
        receive_stream_(nullptr),
        captured_frame_forwarder_(this, clock),
        test_label_(test_label),
        graph_data_output_file_(graph_data_output_file),
        graph_title_(graph_title),
        ssrc_to_analyze_(ssrc_to_analyze),
        rtx_ssrc_to_analyze_(rtx_ssrc_to_analyze),
        selected_stream_(selected_stream),
        selected_sl_(selected_sl),
        selected_tl_(selected_tl),
        pre_encode_proxy_(this),
        encode_timing_proxy_(this),
        last_fec_bytes_(0),
        frames_to_process_(duration_frames),
        frames_recorded_(0),
        frames_processed_(0),
        dropped_frames_(0),
        dropped_frames_before_first_encode_(0),
        dropped_frames_before_rendering_(0),
        last_render_time_(0),
        rtp_timestamp_delta_(0),
        total_media_bytes_(0),
        first_sending_time_(0),
        last_sending_time_(0),
        cpu_time_(0),
        wallclock_time_(0),
        avg_psnr_threshold_(avg_psnr_threshold),
        avg_ssim_threshold_(avg_ssim_threshold),
        is_quick_test_enabled_(is_quick_test_enabled),
        stats_polling_thread_(&PollStatsThread, this, "StatsPoller"),
        comparison_available_event_(false, false),
        done_(true, false),
        clock_(clock),
        start_ms_(clock->TimeInMilliseconds()) {
    // Create thread pool for CPU-expensive PSNR/SSIM calculations.

    // Try to use about as many threads as cores, but leave kMinCoresLeft alone,
    // so that we don't accidentally starve "real" worker threads (codec etc).
    // Also, don't allocate more than kMaxComparisonThreads, even if there are
    // spare cores.

    uint32_t num_cores = CpuInfo::DetectNumberOfCores();
    RTC_DCHECK_GE(num_cores, 1);
    static const uint32_t kMinCoresLeft = 4;
    static const uint32_t kMaxComparisonThreads = 8;

    if (num_cores <= kMinCoresLeft) {
      num_cores = 1;
    } else {
      num_cores -= kMinCoresLeft;
      num_cores = std::min(num_cores, kMaxComparisonThreads);
    }

    for (uint32_t i = 0; i < num_cores; ++i) {
      rtc::PlatformThread* thread =
          new rtc::PlatformThread(&FrameComparisonThread, this, "Analyzer");
      thread->Start();
      comparison_thread_pool_.push_back(thread);
    }

    if (!rtp_dump_name.empty()) {
      fprintf(stdout, "Writing rtp dump to %s\n", rtp_dump_name.c_str());
      rtp_file_writer_.reset(test::RtpFileWriter::Create(
          test::RtpFileWriter::kRtpDump, rtp_dump_name));
    }
  }

  ~VideoAnalyzer() {
    for (rtc::PlatformThread* thread : comparison_thread_pool_) {
      thread->Stop();
      delete thread;
    }
  }

  virtual void SetReceiver(PacketReceiver* receiver) { receiver_ = receiver; }

  void SetSource(test::VideoCapturer* video_capturer, bool respect_sink_wants) {
    if (respect_sink_wants)
      captured_frame_forwarder_.SetSource(video_capturer);
    rtc::VideoSinkWants wants;
    video_capturer->AddOrUpdateSink(InputInterface(), wants);
  }

  void SetCall(Call* call) {
    rtc::CritScope lock(&crit_);
    RTC_DCHECK(!call_);
    call_ = call;
  }

  void SetSendStream(VideoSendStream* stream) {
    rtc::CritScope lock(&crit_);
    RTC_DCHECK(!send_stream_);
    send_stream_ = stream;
  }

  void SetReceiveStream(VideoReceiveStream* stream) {
    rtc::CritScope lock(&crit_);
    RTC_DCHECK(!receive_stream_);
    receive_stream_ = stream;
  }

  rtc::VideoSinkInterface<VideoFrame>* InputInterface() {
    return &captured_frame_forwarder_;
  }
  rtc::VideoSourceInterface<VideoFrame>* OutputInterface() {
    return &captured_frame_forwarder_;
  }

  DeliveryStatus DeliverPacket(MediaType media_type,
                               rtc::CopyOnWriteBuffer packet,
                               const PacketTime& packet_time) override {
    // Ignore timestamps of RTCP packets. They're not synchronized with
    // RTP packet timestamps and so they would confuse wrap_handler_.
    if (RtpHeaderParser::IsRtcp(packet.cdata(), packet.size())) {
      return receiver_->DeliverPacket(media_type, std::move(packet),
                                      packet_time);
    }

    if (rtp_file_writer_) {
      test::RtpPacket p;
      memcpy(p.data, packet.cdata(), packet.size());
      p.length = packet.size();
      p.original_length = packet.size();
      p.time_ms = clock_->TimeInMilliseconds() - start_ms_;
      rtp_file_writer_->WritePacket(&p);
    }

    RtpUtility::RtpHeaderParser parser(packet.cdata(), packet.size());
    RTPHeader header;
    parser.Parse(&header);
    if (!IsFlexfec(header.payloadType) &&
        (header.ssrc == ssrc_to_analyze_ ||
         header.ssrc == rtx_ssrc_to_analyze_)) {
      // Ignore FlexFEC timestamps, to avoid collisions with media timestamps.
      // (FlexFEC and media are sent on different SSRCs, which have different
      // timestamps spaces.)
      // Also ignore packets from wrong SSRC, but include retransmits.
      rtc::CritScope lock(&crit_);
      int64_t timestamp =
          wrap_handler_.Unwrap(header.timestamp - rtp_timestamp_delta_);
      recv_times_[timestamp] =
          Clock::GetRealTimeClock()->CurrentNtpInMilliseconds();
    }

    return receiver_->DeliverPacket(media_type, std::move(packet), packet_time);
  }

  void PreEncodeOnFrame(const VideoFrame& video_frame) {
    rtc::CritScope lock(&crit_);
    if (!first_encoded_timestamp_) {
      while (frames_.front().timestamp() != video_frame.timestamp()) {
        ++dropped_frames_before_first_encode_;
        frames_.pop_front();
        RTC_CHECK(!frames_.empty());
      }
      first_encoded_timestamp_ = video_frame.timestamp();
    }
  }

  void PostEncodeFrameCallback(const EncodedFrame& encoded_frame) {
    rtc::CritScope lock(&crit_);
    if (!first_sent_timestamp_ &&
        encoded_frame.stream_id_ == selected_stream_) {
      first_sent_timestamp_ = encoded_frame.timestamp_;
    }
  }

  bool SendRtp(const uint8_t* packet,
               size_t length,
               const PacketOptions& options) override {
    RtpUtility::RtpHeaderParser parser(packet, length);
    RTPHeader header;
    parser.Parse(&header);

    int64_t current_time =
        Clock::GetRealTimeClock()->CurrentNtpInMilliseconds();

    bool result = transport_->SendRtp(packet, length, options);
    {
      rtc::CritScope lock(&crit_);
      if (rtp_timestamp_delta_ == 0 && header.ssrc == ssrc_to_analyze_) {
        RTC_CHECK(static_cast<bool>(first_sent_timestamp_));
        rtp_timestamp_delta_ = header.timestamp - *first_sent_timestamp_;
      }

      if (!IsFlexfec(header.payloadType) && header.ssrc == ssrc_to_analyze_) {
        // Ignore FlexFEC timestamps, to avoid collisions with media timestamps.
        // (FlexFEC and media are sent on different SSRCs, which have different
        // timestamps spaces.)
        // Also ignore packets from wrong SSRC and retransmits.
        int64_t timestamp =
            wrap_handler_.Unwrap(header.timestamp - rtp_timestamp_delta_);
        send_times_[timestamp] = current_time;

        if (IsInSelectedSpatialAndTemporalLayer(packet, length, header)) {
          encoded_frame_sizes_[timestamp] +=
              length - (header.headerLength + header.paddingLength);
          total_media_bytes_ +=
              length - (header.headerLength + header.paddingLength);
        }
        if (first_sending_time_ == 0)
          first_sending_time_ = current_time;
        last_sending_time_ = current_time;
      }
    }
    return result;
  }

  bool SendRtcp(const uint8_t* packet, size_t length) override {
    return transport_->SendRtcp(packet, length);
  }

  void OnFrame(const VideoFrame& video_frame) override {
    int64_t render_time_ms =
        Clock::GetRealTimeClock()->CurrentNtpInMilliseconds();

    rtc::CritScope lock(&crit_);

    StartExcludingCpuThreadTime();

    int64_t send_timestamp =
        wrap_handler_.Unwrap(video_frame.timestamp() - rtp_timestamp_delta_);

    while (wrap_handler_.Unwrap(frames_.front().timestamp()) < send_timestamp) {
      if (!last_rendered_frame_) {
        // No previous frame rendered, this one was dropped after sending but
        // before rendering.
        ++dropped_frames_before_rendering_;
      } else {
        AddFrameComparison(frames_.front(), *last_rendered_frame_, true,
                           render_time_ms);
      }
      frames_.pop_front();
      RTC_DCHECK(!frames_.empty());
    }

    VideoFrame reference_frame = frames_.front();
    frames_.pop_front();
    int64_t reference_timestamp =
        wrap_handler_.Unwrap(reference_frame.timestamp());
    if (send_timestamp == reference_timestamp - 1) {
      // TODO(ivica): Make this work for > 2 streams.
      // Look at RTPSender::BuildRTPHeader.
      ++send_timestamp;
    }
    ASSERT_EQ(reference_timestamp, send_timestamp);

    AddFrameComparison(reference_frame, video_frame, false, render_time_ms);

    last_rendered_frame_ = video_frame;

    StopExcludingCpuThreadTime();
  }

  void Wait() {
    // Frame comparisons can be very expensive. Wait for test to be done, but
    // at time-out check if frames_processed is going up. If so, give it more
    // time, otherwise fail. Hopefully this will reduce test flakiness.

    stats_polling_thread_.Start();

    int last_frames_processed = -1;
    int iteration = 0;
    while (!done_.Wait(VideoQualityTest::kDefaultTimeoutMs)) {
      int frames_processed;
      {
        rtc::CritScope crit(&comparison_lock_);
        frames_processed = frames_processed_;
      }

      // Print some output so test infrastructure won't think we've crashed.
      const char* kKeepAliveMessages[3] = {
          "Uh, I'm-I'm not quite dead, sir.",
          "Uh, I-I think uh, I could pull through, sir.",
          "Actually, I think I'm all right to come with you--"};
      printf("- %s\n", kKeepAliveMessages[iteration++ % 3]);

      if (last_frames_processed == -1) {
        last_frames_processed = frames_processed;
        continue;
      }
      if (frames_processed == last_frames_processed) {
        EXPECT_GT(frames_processed, last_frames_processed)
            << "Analyzer stalled while waiting for test to finish.";
        done_.Set();
        break;
      }
      last_frames_processed = frames_processed;
    }

    if (iteration > 0)
      printf("- Farewell, sweet Concorde!\n");

    stats_polling_thread_.Stop();
  }

  rtc::VideoSinkInterface<VideoFrame>* pre_encode_proxy() {
    return &pre_encode_proxy_;
  }
  EncodedFrameObserver* encode_timing_proxy() { return &encode_timing_proxy_; }

  void StartMeasuringCpuProcessTime() {
    rtc::CritScope lock(&cpu_measurement_lock_);
    cpu_time_ -= rtc::GetProcessCpuTimeNanos();
    wallclock_time_ -= rtc::SystemTimeNanos();
  }

  void StopMeasuringCpuProcessTime() {
    rtc::CritScope lock(&cpu_measurement_lock_);
    cpu_time_ += rtc::GetProcessCpuTimeNanos();
    wallclock_time_ += rtc::SystemTimeNanos();
  }

  void StartExcludingCpuThreadTime() {
    rtc::CritScope lock(&cpu_measurement_lock_);
    cpu_time_ += rtc::GetThreadCpuTimeNanos();
  }

  void StopExcludingCpuThreadTime() {
    rtc::CritScope lock(&cpu_measurement_lock_);
    cpu_time_ -= rtc::GetThreadCpuTimeNanos();
  }

  double GetCpuUsagePercent() {
    rtc::CritScope lock(&cpu_measurement_lock_);
    return static_cast<double>(cpu_time_) / wallclock_time_ * 100.0;
  }

  test::LayerFilteringTransport* const transport_;
  PacketReceiver* receiver_;

 private:
  struct FrameComparison {
    FrameComparison()
        : dropped(false),
          input_time_ms(0),
          send_time_ms(0),
          recv_time_ms(0),
          render_time_ms(0),
          encoded_frame_size(0) {}

    FrameComparison(const VideoFrame& reference,
                    const VideoFrame& render,
                    bool dropped,
                    int64_t input_time_ms,
                    int64_t send_time_ms,
                    int64_t recv_time_ms,
                    int64_t render_time_ms,
                    size_t encoded_frame_size)
        : reference(reference),
          render(render),
          dropped(dropped),
          input_time_ms(input_time_ms),
          send_time_ms(send_time_ms),
          recv_time_ms(recv_time_ms),
          render_time_ms(render_time_ms),
          encoded_frame_size(encoded_frame_size) {}

    FrameComparison(bool dropped,
                    int64_t input_time_ms,
                    int64_t send_time_ms,
                    int64_t recv_time_ms,
                    int64_t render_time_ms,
                    size_t encoded_frame_size)
        : dropped(dropped),
          input_time_ms(input_time_ms),
          send_time_ms(send_time_ms),
          recv_time_ms(recv_time_ms),
          render_time_ms(render_time_ms),
          encoded_frame_size(encoded_frame_size) {}

    rtc::Optional<VideoFrame> reference;
    rtc::Optional<VideoFrame> render;
    bool dropped;
    int64_t input_time_ms;
    int64_t send_time_ms;
    int64_t recv_time_ms;
    int64_t render_time_ms;
    size_t encoded_frame_size;
  };

  struct Sample {
    Sample(int dropped,
           int64_t input_time_ms,
           int64_t send_time_ms,
           int64_t recv_time_ms,
           int64_t render_time_ms,
           size_t encoded_frame_size,
           double psnr,
           double ssim)
        : dropped(dropped),
          input_time_ms(input_time_ms),
          send_time_ms(send_time_ms),
          recv_time_ms(recv_time_ms),
          render_time_ms(render_time_ms),
          encoded_frame_size(encoded_frame_size),
          psnr(psnr),
          ssim(ssim) {}

    int dropped;
    int64_t input_time_ms;
    int64_t send_time_ms;
    int64_t recv_time_ms;
    int64_t render_time_ms;
    size_t encoded_frame_size;
    double psnr;
    double ssim;
  };

  // This class receives the send-side OnEncodeTiming and is provided to not
  // conflict with the receiver-side pre_decode_callback.
  class OnEncodeTimingProxy : public EncodedFrameObserver {
   public:
    explicit OnEncodeTimingProxy(VideoAnalyzer* parent) : parent_(parent) {}

    void EncodedFrameCallback(const EncodedFrame& frame) override {
      parent_->PostEncodeFrameCallback(frame);
    }

   private:
    VideoAnalyzer* const parent_;
  };

  // This class receives the send-side OnFrame callback and is provided to not
  // conflict with the receiver-side renderer callback.
  class PreEncodeProxy : public rtc::VideoSinkInterface<VideoFrame> {
   public:
    explicit PreEncodeProxy(VideoAnalyzer* parent) : parent_(parent) {}

    void OnFrame(const VideoFrame& video_frame) override {
      parent_->PreEncodeOnFrame(video_frame);
    }

   private:
    VideoAnalyzer* const parent_;
  };

  bool IsInSelectedSpatialAndTemporalLayer(const uint8_t* packet,
                                           size_t length,
                                           const RTPHeader& header) {
    if (header.payloadType != test::CallTest::kPayloadTypeVP9 &&
        header.payloadType != test::CallTest::kPayloadTypeVP8) {
      return true;
    } else {
      // Get VP8 and VP9 specific header to check layers indexes.
      const uint8_t* payload = packet + header.headerLength;
      const size_t payload_length = length - header.headerLength;
      const size_t payload_data_length = payload_length - header.paddingLength;
      const bool is_vp8 = header.payloadType == test::CallTest::kPayloadTypeVP8;
      std::unique_ptr<RtpDepacketizer> depacketizer(
          RtpDepacketizer::Create(is_vp8 ? kRtpVideoVp8 : kRtpVideoVp9));
      RtpDepacketizer::ParsedPayload parsed_payload;
      bool result =
          depacketizer->Parse(&parsed_payload, payload, payload_data_length);
      RTC_DCHECK(result);
      const int temporal_idx = static_cast<int>(
          is_vp8 ? parsed_payload.type.Video.codecHeader.VP8.temporalIdx
                 : parsed_payload.type.Video.codecHeader.VP9.temporal_idx);
      const int spatial_idx = static_cast<int>(
          is_vp8 ? kNoSpatialIdx
                 : parsed_payload.type.Video.codecHeader.VP9.spatial_idx);
      return (selected_tl_ < 0 || temporal_idx == kNoTemporalIdx ||
              temporal_idx <= selected_tl_) &&
             (selected_sl_ < 0 || spatial_idx == kNoSpatialIdx ||
              spatial_idx <= selected_sl_);
    }
  }

  void AddFrameComparison(const VideoFrame& reference,
                          const VideoFrame& render,
                          bool dropped,
                          int64_t render_time_ms)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_) {
    int64_t reference_timestamp = wrap_handler_.Unwrap(reference.timestamp());
    int64_t send_time_ms = send_times_[reference_timestamp];
    send_times_.erase(reference_timestamp);
    int64_t recv_time_ms = recv_times_[reference_timestamp];
    recv_times_.erase(reference_timestamp);

    // TODO(ivica): Make this work for > 2 streams.
    auto it = encoded_frame_sizes_.find(reference_timestamp);
    if (it == encoded_frame_sizes_.end())
      it = encoded_frame_sizes_.find(reference_timestamp - 1);
    size_t encoded_size = it == encoded_frame_sizes_.end() ? 0 : it->second;
    if (it != encoded_frame_sizes_.end())
      encoded_frame_sizes_.erase(it);

    rtc::CritScope crit(&comparison_lock_);
    if (comparisons_.size() < kMaxComparisons) {
      comparisons_.push_back(FrameComparison(reference, render, dropped,
                                             reference.ntp_time_ms(),
                                             send_time_ms, recv_time_ms,
                                             render_time_ms, encoded_size));
    } else {
      comparisons_.push_back(FrameComparison(dropped,
                                             reference.ntp_time_ms(),
                                             send_time_ms, recv_time_ms,
                                             render_time_ms, encoded_size));
    }
    comparison_available_event_.Set();
  }

  static void PollStatsThread(void* obj) {
    static_cast<VideoAnalyzer*>(obj)->PollStats();
  }

  void PollStats() {
    while (!done_.Wait(kSendStatsPollingIntervalMs)) {
      rtc::CritScope crit(&comparison_lock_);

      Call::Stats call_stats = call_->GetStats();
      send_bandwidth_bps_.AddSample(call_stats.send_bandwidth_bps);

      VideoSendStream::Stats send_stats = send_stream_->GetStats();
      // It's not certain that we yet have estimates for any of these stats.
      // Check that they are positive before mixing them in.
      if (send_stats.encode_frame_rate > 0)
        encode_frame_rate_.AddSample(send_stats.encode_frame_rate);
      if (send_stats.avg_encode_time_ms > 0)
        encode_time_ms_.AddSample(send_stats.avg_encode_time_ms);
      if (send_stats.encode_usage_percent > 0)
        encode_usage_percent_.AddSample(send_stats.encode_usage_percent);
      if (send_stats.media_bitrate_bps > 0)
        media_bitrate_bps_.AddSample(send_stats.media_bitrate_bps);
      size_t fec_bytes = 0;
      for (auto kv : send_stats.substreams) {
        fec_bytes += kv.second.rtp_stats.fec.payload_bytes +
                     kv.second.rtp_stats.fec.padding_bytes;
      }
      fec_bitrate_bps_.AddSample((fec_bytes - last_fec_bytes_) * 8);
      last_fec_bytes_ = fec_bytes;

      if (receive_stream_ != nullptr) {
        VideoReceiveStream::Stats receive_stats = receive_stream_->GetStats();
        if (receive_stats.decode_ms > 0)
          decode_time_ms_.AddSample(receive_stats.decode_ms);
        if (receive_stats.max_decode_ms > 0)
          decode_time_max_ms_.AddSample(receive_stats.max_decode_ms);
      }

      memory_usage_.AddSample(rtc::GetProcessResidentSizeBytes());
    }
  }

  static bool FrameComparisonThread(void* obj) {
    return static_cast<VideoAnalyzer*>(obj)->CompareFrames();
  }

  bool CompareFrames() {
    if (AllFramesRecorded())
      return false;

    FrameComparison comparison;

    if (!PopComparison(&comparison)) {
      // Wait until new comparison task is available, or test is done.
      // If done, wake up remaining threads waiting.
      comparison_available_event_.Wait(1000);
      if (AllFramesRecorded()) {
        comparison_available_event_.Set();
        return false;
      }
      return true;  // Try again.
    }

    StartExcludingCpuThreadTime();

    PerformFrameComparison(comparison);

    StopExcludingCpuThreadTime();

    if (FrameProcessed()) {
      PrintResults();
      if (graph_data_output_file_)
        PrintSamplesToFile();
      done_.Set();
      comparison_available_event_.Set();
      return false;
    }

    return true;
  }

  bool PopComparison(FrameComparison* comparison) {
    rtc::CritScope crit(&comparison_lock_);
    // If AllFramesRecorded() is true, it means we have already popped
    // frames_to_process_ frames from comparisons_, so there is no more work
    // for this thread to be done. frames_processed_ might still be lower if
    // all comparisons are not done, but those frames are currently being
    // worked on by other threads.
    if (comparisons_.empty() || AllFramesRecorded())
      return false;

    *comparison = comparisons_.front();
    comparisons_.pop_front();

    FrameRecorded();
    return true;
  }

  // Increment counter for number of frames received for comparison.
  void FrameRecorded() {
    rtc::CritScope crit(&comparison_lock_);
    ++frames_recorded_;
  }

  // Returns true if all frames to be compared have been taken from the queue.
  bool AllFramesRecorded() {
    rtc::CritScope crit(&comparison_lock_);
    assert(frames_recorded_ <= frames_to_process_);
    return frames_recorded_ == frames_to_process_;
  }

  // Increase count of number of frames processed. Returns true if this was the
  // last frame to be processed.
  bool FrameProcessed() {
    rtc::CritScope crit(&comparison_lock_);
    ++frames_processed_;
    assert(frames_processed_ <= frames_to_process_);
    return frames_processed_ == frames_to_process_;
  }

  void PrintResults() {
    StopMeasuringCpuProcessTime();
    rtc::CritScope crit(&comparison_lock_);
    PrintResult("psnr", psnr_, " dB");
    PrintResult("ssim", ssim_, " score");
    PrintResult("sender_time", sender_time_, " ms");
    PrintResult("receiver_time", receiver_time_, " ms");
    PrintResult("total_delay_incl_network", end_to_end_, " ms");
    PrintResult("time_between_rendered_frames", rendered_delta_, " ms");
    PrintResult("encode_frame_rate", encode_frame_rate_, " fps");
    PrintResult("encode_time", encode_time_ms_, " ms");
    PrintResult("media_bitrate", media_bitrate_bps_, " bps");
    PrintResult("fec_bitrate", fec_bitrate_bps_, " bps");
    PrintResult("send_bandwidth", send_bandwidth_bps_, " bps");

    if (worst_frame_) {
      test::PrintResult("min_psnr", "", test_label_.c_str(), worst_frame_->psnr,
                  "dB", false);
    }

    if (receive_stream_ != nullptr) {
      PrintResult("decode_time", decode_time_ms_, " ms");
    }

    test::PrintResult("dropped_frames", "", test_label_.c_str(),
                  dropped_frames_, "frames", false);
    test::PrintResult("cpu_usage", "", test_label_.c_str(),
                      GetCpuUsagePercent(), "%", false);

#if defined(WEBRTC_WIN)
      // On Linux and Mac in Resident Set some unused pages may be counted.
      // Therefore this metric will depend on order in which tests are run and
      // will be flaky.
    PrintResult("memory_usage", memory_usage_, " bytes");
#endif

    // Saving only the worst frame for manual analysis. Intention here is to
    // only detect video corruptions and not to track picture quality. Thus,
    // jpeg is used here.
    if (FLAG_save_worst_frame && worst_frame_) {
      std::string output_dir;
      test::GetTestArtifactsDir(&output_dir);
      std::string output_path =
          rtc::Pathname(output_dir, test_label_ + ".jpg").pathname();
      RTC_LOG(LS_INFO) << "Saving worst frame to " << output_path;
      test::JpegFrameWriter frame_writer(output_path);
      RTC_CHECK(frame_writer.WriteFrame(worst_frame_->frame,
                                        100 /*best quality*/));
    }

    //  Disable quality check for quick test, as quality checks may fail
    //  because too few samples were collected.
    if (!is_quick_test_enabled_) {
      EXPECT_GT(psnr_.Mean(), avg_psnr_threshold_);
      EXPECT_GT(ssim_.Mean(), avg_ssim_threshold_);
    }
  }

  void PerformFrameComparison(const FrameComparison& comparison) {
    // Perform expensive psnr and ssim calculations while not holding lock.
    double psnr = -1.0;
    double ssim = -1.0;
    if (comparison.reference && !comparison.dropped) {
      psnr = I420PSNR(&*comparison.reference, &*comparison.render);
      ssim = I420SSIM(&*comparison.reference, &*comparison.render);
    }

    rtc::CritScope crit(&comparison_lock_);

    if (psnr >= 0.0 && (!worst_frame_ || worst_frame_->psnr > psnr)) {
      worst_frame_.emplace(FrameWithPsnr{psnr, *comparison.render});
    }

    if (graph_data_output_file_) {
      samples_.push_back(Sample(
          comparison.dropped, comparison.input_time_ms, comparison.send_time_ms,
          comparison.recv_time_ms, comparison.render_time_ms,
          comparison.encoded_frame_size, psnr, ssim));
    }
    if (psnr >= 0.0)
      psnr_.AddSample(psnr);
    if (ssim >= 0.0)
      ssim_.AddSample(ssim);

    if (comparison.dropped) {
      ++dropped_frames_;
      return;
    }
    if (last_render_time_ != 0)
      rendered_delta_.AddSample(comparison.render_time_ms - last_render_time_);
    last_render_time_ = comparison.render_time_ms;

    sender_time_.AddSample(comparison.send_time_ms - comparison.input_time_ms);
    if (comparison.recv_time_ms > 0) {
      // If recv_time_ms == 0, this frame consisted of a packets which were all
      // lost in the transport. Since we were able to render the frame, however,
      // the dropped packets were recovered by FlexFEC. The FlexFEC recovery
      // happens internally in Call, and we can therefore here not know which
      // FEC packets that protected the lost media packets. Consequently, we
      // were not able to record a meaningful recv_time_ms. We therefore skip
      // this sample.
      //
      // The reasoning above does not hold for ULPFEC and RTX, as for those
      // strategies the timestamp of the received packets is set to the
      // timestamp of the protected/retransmitted media packet. I.e., then
      // recv_time_ms != 0, even though the media packets were lost.
      receiver_time_.AddSample(comparison.render_time_ms -
                               comparison.recv_time_ms);
    }
    end_to_end_.AddSample(comparison.render_time_ms - comparison.input_time_ms);
    encoded_frame_size_.AddSample(comparison.encoded_frame_size);
  }

  void PrintResult(const char* result_type,
                   test::Statistics stats,
                   const char* unit) {
    test::PrintResultMeanAndError(result_type, "", test_label_.c_str(),
                                  stats.Mean(), stats.StandardDeviation(), unit,
                                  false);
  }

  void PrintSamplesToFile(void) {
    FILE* out = graph_data_output_file_;
    rtc::CritScope crit(&comparison_lock_);
    std::sort(samples_.begin(), samples_.end(),
              [](const Sample& A, const Sample& B) -> bool {
                return A.input_time_ms < B.input_time_ms;
              });

    fprintf(out, "%s\n", graph_title_.c_str());
    fprintf(out, "%" PRIuS "\n", samples_.size());
    fprintf(out,
            "dropped "
            "input_time_ms "
            "send_time_ms "
            "recv_time_ms "
            "render_time_ms "
            "encoded_frame_size "
            "psnr "
            "ssim "
            "encode_time_ms\n");
    for (const Sample& sample : samples_) {
      fprintf(out, "%d %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRIuS
                   " %lf %lf\n",
              sample.dropped, sample.input_time_ms, sample.send_time_ms,
              sample.recv_time_ms, sample.render_time_ms,
              sample.encoded_frame_size, sample.psnr, sample.ssim);
    }
  }

  double GetAverageMediaBitrateBps() {
    if (last_sending_time_ == first_sending_time_) {
      return 0;
    } else {
      return static_cast<double>(total_media_bytes_) * 8 /
             (last_sending_time_ - first_sending_time_) *
             rtc::kNumMillisecsPerSec;
    }
  }

  // Implements VideoSinkInterface to receive captured frames from a
  // FrameGeneratorCapturer. Implements VideoSourceInterface to be able to act
  // as a source to VideoSendStream.
  // It forwards all input frames to the VideoAnalyzer for later comparison and
  // forwards the captured frames to the VideoSendStream.
  class CapturedFrameForwarder : public rtc::VideoSinkInterface<VideoFrame>,
                                 public rtc::VideoSourceInterface<VideoFrame> {
   public:
    explicit CapturedFrameForwarder(VideoAnalyzer* analyzer, Clock* clock)
        : analyzer_(analyzer),
          send_stream_input_(nullptr),
          video_capturer_(nullptr),
          clock_(clock) {}

    void SetSource(test::VideoCapturer* video_capturer) {
      video_capturer_ = video_capturer;
    }

   private:
    void OnFrame(const VideoFrame& video_frame) override {
      VideoFrame copy = video_frame;
      // Frames from the capturer does not have a rtp timestamp.
      // Create one so it can be used for comparison.
      RTC_DCHECK_EQ(0, video_frame.timestamp());
      if (video_frame.ntp_time_ms() == 0)
        copy.set_ntp_time_ms(clock_->CurrentNtpInMilliseconds());
      copy.set_timestamp(copy.ntp_time_ms() * 90);
      analyzer_->AddCapturedFrameForComparison(copy);
      rtc::CritScope lock(&crit_);
      if (send_stream_input_)
        send_stream_input_->OnFrame(copy);
    }

    // Called when |send_stream_.SetSource()| is called.
    void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                         const rtc::VideoSinkWants& wants) override {
      {
        rtc::CritScope lock(&crit_);
        RTC_DCHECK(!send_stream_input_ || send_stream_input_ == sink);
        send_stream_input_ = sink;
      }
      if (video_capturer_) {
        video_capturer_->AddOrUpdateSink(this, wants);
      }
    }

    // Called by |send_stream_| when |send_stream_.SetSource()| is called.
    void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) override {
      rtc::CritScope lock(&crit_);
      RTC_DCHECK(sink == send_stream_input_);
      send_stream_input_ = nullptr;
    }

    VideoAnalyzer* const analyzer_;
    rtc::CriticalSection crit_;
    rtc::VideoSinkInterface<VideoFrame>* send_stream_input_
        RTC_GUARDED_BY(crit_);
    test::VideoCapturer* video_capturer_;
    Clock* clock_;
  };

  void AddCapturedFrameForComparison(const VideoFrame& video_frame) {
    rtc::CritScope lock(&crit_);
    frames_.push_back(video_frame);
  }

  Call* call_;
  VideoSendStream* send_stream_;
  VideoReceiveStream* receive_stream_;
  CapturedFrameForwarder captured_frame_forwarder_;
  const std::string test_label_;
  FILE* const graph_data_output_file_;
  const std::string graph_title_;
  const uint32_t ssrc_to_analyze_;
  const uint32_t rtx_ssrc_to_analyze_;
  const size_t selected_stream_;
  const int selected_sl_;
  const int selected_tl_;
  PreEncodeProxy pre_encode_proxy_;
  OnEncodeTimingProxy encode_timing_proxy_;
  std::vector<Sample> samples_ RTC_GUARDED_BY(comparison_lock_);
  test::Statistics sender_time_ RTC_GUARDED_BY(comparison_lock_);
  test::Statistics receiver_time_ RTC_GUARDED_BY(comparison_lock_);
  test::Statistics psnr_ RTC_GUARDED_BY(comparison_lock_);
  test::Statistics ssim_ RTC_GUARDED_BY(comparison_lock_);
  test::Statistics end_to_end_ RTC_GUARDED_BY(comparison_lock_);
  test::Statistics rendered_delta_ RTC_GUARDED_BY(comparison_lock_);
  test::Statistics encoded_frame_size_ RTC_GUARDED_BY(comparison_lock_);
  test::Statistics encode_frame_rate_ RTC_GUARDED_BY(comparison_lock_);
  test::Statistics encode_time_ms_ RTC_GUARDED_BY(comparison_lock_);
  test::Statistics encode_usage_percent_ RTC_GUARDED_BY(comparison_lock_);
  test::Statistics decode_time_ms_ RTC_GUARDED_BY(comparison_lock_);
  test::Statistics decode_time_max_ms_ RTC_GUARDED_BY(comparison_lock_);
  test::Statistics media_bitrate_bps_ RTC_GUARDED_BY(comparison_lock_);
  test::Statistics fec_bitrate_bps_ RTC_GUARDED_BY(comparison_lock_);
  test::Statistics send_bandwidth_bps_ RTC_GUARDED_BY(comparison_lock_);
  test::Statistics memory_usage_ RTC_GUARDED_BY(comparison_lock_);

  struct FrameWithPsnr {
    double psnr;
    VideoFrame frame;
  };

  // Rendered frame with worst PSNR is saved for further analysis.
  rtc::Optional<FrameWithPsnr> worst_frame_ RTC_GUARDED_BY(comparison_lock_);

  size_t last_fec_bytes_;

  const int frames_to_process_;
  int frames_recorded_;
  int frames_processed_;
  int dropped_frames_;
  int dropped_frames_before_first_encode_;
  int dropped_frames_before_rendering_;
  int64_t last_render_time_;
  uint32_t rtp_timestamp_delta_;
  int64_t total_media_bytes_;
  int64_t first_sending_time_;
  int64_t last_sending_time_;

  int64_t cpu_time_ RTC_GUARDED_BY(cpu_measurement_lock_);
  int64_t wallclock_time_ RTC_GUARDED_BY(cpu_measurement_lock_);
  rtc::CriticalSection cpu_measurement_lock_;

  rtc::CriticalSection crit_;
  std::deque<VideoFrame> frames_ RTC_GUARDED_BY(crit_);
  rtc::Optional<VideoFrame> last_rendered_frame_ RTC_GUARDED_BY(crit_);
  rtc::TimestampWrapAroundHandler wrap_handler_ RTC_GUARDED_BY(crit_);
  std::map<int64_t, int64_t> send_times_ RTC_GUARDED_BY(crit_);
  std::map<int64_t, int64_t> recv_times_ RTC_GUARDED_BY(crit_);
  std::map<int64_t, size_t> encoded_frame_sizes_ RTC_GUARDED_BY(crit_);
  rtc::Optional<uint32_t> first_encoded_timestamp_ RTC_GUARDED_BY(crit_);
  rtc::Optional<uint32_t> first_sent_timestamp_ RTC_GUARDED_BY(crit_);
  const double avg_psnr_threshold_;
  const double avg_ssim_threshold_;
  bool is_quick_test_enabled_;

  rtc::CriticalSection comparison_lock_;
  std::vector<rtc::PlatformThread*> comparison_thread_pool_;
  rtc::PlatformThread stats_polling_thread_;
  rtc::Event comparison_available_event_;
  std::deque<FrameComparison> comparisons_ RTC_GUARDED_BY(comparison_lock_);
  rtc::Event done_;

  std::unique_ptr<test::RtpFileWriter> rtp_file_writer_;
  Clock* const clock_;
  const int64_t start_ms_;
};

VideoQualityTest::VideoQualityTest()
    : clock_(Clock::GetRealTimeClock()), receive_logs_(0), send_logs_(0) {
  payload_type_map_ = test::CallTest::payload_type_map_;
  RTC_DCHECK(payload_type_map_.find(kPayloadTypeH264) ==
             payload_type_map_.end());
  RTC_DCHECK(payload_type_map_.find(kPayloadTypeVP8) ==
             payload_type_map_.end());
  RTC_DCHECK(payload_type_map_.find(kPayloadTypeVP9) ==
             payload_type_map_.end());
  payload_type_map_[kPayloadTypeH264] = webrtc::MediaType::VIDEO;
  payload_type_map_[kPayloadTypeVP8] = webrtc::MediaType::VIDEO;
  payload_type_map_[kPayloadTypeVP9] = webrtc::MediaType::VIDEO;
}

VideoQualityTest::Params::Params()
    : call({false, Call::Config::BitrateConfig(), 0}),
      video{{false, 640, 480, 30, 50, 800, 800, false, "VP8", 1, -1, 0, false,
             false, ""},
            {false, 640, 480, 30, 50, 800, 800, false, "VP8", 1, -1, 0, false,
             false, ""}},
      audio({false, false, false}),
      screenshare{{false, false, 10, 0}, {false, false, 10, 0}},
      analyzer({"", 0.0, 0.0, 0, "", ""}),
      pipe(),
      ss{{std::vector<VideoStream>(), 0, 0, -1, std::vector<SpatialLayer>()},
         {std::vector<VideoStream>(), 0, 0, -1, std::vector<SpatialLayer>()}},
      logging({false, "", "", ""}) {}

VideoQualityTest::Params::~Params() = default;

void VideoQualityTest::TestBody() {}

std::string VideoQualityTest::GenerateGraphTitle() const {
  std::stringstream ss;
  ss << params_.video[0].codec;
  ss << " (" << params_.video[0].target_bitrate_bps / 1000 << "kbps";
  ss << ", " << params_.video[0].fps << " FPS";
  if (params_.screenshare[0].scroll_duration)
    ss << ", " << params_.screenshare[0].scroll_duration << "s scroll";
  if (params_.ss[0].streams.size() > 1)
    ss << ", Stream #" << params_.ss[0].selected_stream;
  if (params_.ss[0].num_spatial_layers > 1)
    ss << ", Layer #" << params_.ss[0].selected_sl;
  ss << ")";
  return ss.str();
}

void VideoQualityTest::CheckParams() {
  for (size_t video_idx = 0; video_idx < num_video_streams_; ++video_idx) {
    // Iterate over primary and secondary video streams.
    if (!params_.video[video_idx].enabled)
      return;
    // Add a default stream in none specified.
    if (params_.ss[video_idx].streams.empty())
      params_.ss[video_idx].streams.push_back(
          VideoQualityTest::DefaultVideoStream(params_, video_idx));
    if (params_.ss[video_idx].num_spatial_layers == 0)
      params_.ss[video_idx].num_spatial_layers = 1;

    if (params_.pipe.loss_percent != 0 ||
        params_.pipe.queue_length_packets != 0) {
      // Since LayerFilteringTransport changes the sequence numbers, we can't
      // use that feature with pack loss, since the NACK request would end up
      // retransmitting the wrong packets.
      RTC_CHECK(params_.ss[video_idx].selected_sl == -1 ||
                params_.ss[video_idx].selected_sl ==
                    params_.ss[video_idx].num_spatial_layers - 1);
      RTC_CHECK(params_.video[video_idx].selected_tl == -1 ||
                params_.video[video_idx].selected_tl ==
                    params_.video[video_idx].num_temporal_layers - 1);
    }

    // TODO(ivica): Should max_bitrate_bps == -1 represent inf max bitrate, as
    // it does in some parts of the code?
    RTC_CHECK_GE(params_.video[video_idx].max_bitrate_bps,
                 params_.video[video_idx].target_bitrate_bps);
    RTC_CHECK_GE(params_.video[video_idx].target_bitrate_bps,
                 params_.video[video_idx].min_bitrate_bps);
    RTC_CHECK_LT(params_.video[video_idx].selected_tl,
                 params_.video[video_idx].num_temporal_layers);
    RTC_CHECK_LE(params_.ss[video_idx].selected_stream,
                 params_.ss[video_idx].streams.size());
    for (const VideoStream& stream : params_.ss[video_idx].streams) {
      RTC_CHECK_GE(stream.min_bitrate_bps, 0);
      RTC_CHECK_GE(stream.target_bitrate_bps, stream.min_bitrate_bps);
      RTC_CHECK_GE(stream.max_bitrate_bps, stream.target_bitrate_bps);
    }
    // TODO(ivica): Should we check if the sum of all streams/layers is equal to
    // the total bitrate? We anyway have to update them in the case bitrate
    // estimator changes the total bitrates.
    RTC_CHECK_GE(params_.ss[video_idx].num_spatial_layers, 1);
    RTC_CHECK_LE(params_.ss[video_idx].selected_sl,
                 params_.ss[video_idx].num_spatial_layers);
    RTC_CHECK(
        params_.ss[video_idx].spatial_layers.empty() ||
        params_.ss[video_idx].spatial_layers.size() ==
            static_cast<size_t>(params_.ss[video_idx].num_spatial_layers));
    if (params_.video[video_idx].codec == "VP8") {
      RTC_CHECK_EQ(params_.ss[video_idx].num_spatial_layers, 1);
    } else if (params_.video[video_idx].codec == "VP9") {
      RTC_CHECK_EQ(params_.ss[video_idx].streams.size(), 1);
    }
    RTC_CHECK_GE(params_.call.num_thumbnails, 0);
    if (params_.call.num_thumbnails > 0) {
      RTC_CHECK_EQ(params_.ss[video_idx].num_spatial_layers, 1);
      RTC_CHECK_EQ(params_.ss[video_idx].streams.size(), 3);
      RTC_CHECK_EQ(params_.video[video_idx].num_temporal_layers, 3);
      RTC_CHECK_EQ(params_.video[video_idx].codec, "VP8");
    }
    // Dual streams with FEC not supported in tests yet.
    RTC_CHECK(!params_.video[video_idx].flexfec || num_video_streams_ == 1);
    RTC_CHECK(!params_.video[video_idx].ulpfec || num_video_streams_ == 1);
  }
}

// Static.
std::vector<int> VideoQualityTest::ParseCSV(const std::string& str) {
  // Parse comma separated nonnegative integers, where some elements may be
  // empty. The empty values are replaced with -1.
  // E.g. "10,-20,,30,40" --> {10, 20, -1, 30,40}
  // E.g. ",,10,,20," --> {-1, -1, 10, -1, 20, -1}
  std::vector<int> result;
  if (str.empty())
    return result;

  const char* p = str.c_str();
  int value = -1;
  int pos;
  while (*p) {
    if (*p == ',') {
      result.push_back(value);
      value = -1;
      ++p;
      continue;
    }
    RTC_CHECK_EQ(sscanf(p, "%d%n", &value, &pos), 1)
        << "Unexpected non-number value.";
    p += pos;
  }
  result.push_back(value);
  return result;
}

// Static.
VideoStream VideoQualityTest::DefaultVideoStream(const Params& params,
                                                 size_t video_idx) {
  VideoStream stream;
  stream.width = params.video[video_idx].width;
  stream.height = params.video[video_idx].height;
  stream.max_framerate = params.video[video_idx].fps;
  stream.min_bitrate_bps = params.video[video_idx].min_bitrate_bps;
  stream.target_bitrate_bps = params.video[video_idx].target_bitrate_bps;
  stream.max_bitrate_bps = params.video[video_idx].max_bitrate_bps;
  stream.max_qp = kDefaultMaxQp;
  // TODO(sprang): Can we make this less of a hack?
  if (params.video[video_idx].num_temporal_layers == 2) {
    stream.temporal_layer_thresholds_bps.push_back(stream.target_bitrate_bps);
  } else if (params.video[video_idx].num_temporal_layers == 3) {
    stream.temporal_layer_thresholds_bps.push_back(stream.max_bitrate_bps / 4);
    stream.temporal_layer_thresholds_bps.push_back(stream.target_bitrate_bps);
  } else {
    RTC_CHECK_LE(params.video[video_idx].num_temporal_layers,
                 kMaxTemporalStreams);
    for (int i = 0; i < params.video[video_idx].num_temporal_layers - 1; ++i) {
      stream.temporal_layer_thresholds_bps.push_back(static_cast<int>(
          stream.max_bitrate_bps * kVp8LayerRateAlloction[0][i] + 0.5));
    }
  }
  return stream;
}

// Static.
VideoStream VideoQualityTest::DefaultThumbnailStream() {
  VideoStream stream;
  stream.width = 320;
  stream.height = 180;
  stream.max_framerate = 7;
  stream.min_bitrate_bps = 7500;
  stream.target_bitrate_bps = 37500;
  stream.max_bitrate_bps = 50000;
  stream.max_qp = kDefaultMaxQp;
  return stream;
}

// Static.
void VideoQualityTest::FillScalabilitySettings(
    Params* params,
    size_t video_idx,
    const std::vector<std::string>& stream_descriptors,
    int num_streams,
    size_t selected_stream,
    int num_spatial_layers,
    int selected_sl,
    const std::vector<std::string>& sl_descriptors) {
  if (params->ss[video_idx].streams.empty() &&
      params->ss[video_idx].infer_streams) {
    webrtc::VideoEncoderConfig encoder_config;
    encoder_config.content_type =
        params->screenshare[video_idx].enabled
            ? webrtc::VideoEncoderConfig::ContentType::kScreen
            : webrtc::VideoEncoderConfig::ContentType::kRealtimeVideo;
    encoder_config.max_bitrate_bps = params->video[video_idx].max_bitrate_bps;
    encoder_config.min_transmit_bitrate_bps =
        params->video[video_idx].min_transmit_bps;
    encoder_config.number_of_streams = num_streams;
    encoder_config.spatial_layers = params->ss[video_idx].spatial_layers;
    encoder_config.video_stream_factory =
        new rtc::RefCountedObject<cricket::EncoderStreamFactory>(
            params->video[video_idx].codec, kDefaultMaxQp,
            params->video[video_idx].fps,
            params->screenshare[video_idx].enabled, true);
    params->ss[video_idx].streams =
        encoder_config.video_stream_factory->CreateEncoderStreams(
            static_cast<int>(params->video[video_idx].width),
            static_cast<int>(params->video[video_idx].height), encoder_config);
  } else {
    // Read VideoStream and SpatialLayer elements from a list of comma separated
    // lists. To use a default value for an element, use -1 or leave empty.
    // Validity checks performed in CheckParams.
    RTC_CHECK(params->ss[video_idx].streams.empty());
    for (auto descriptor : stream_descriptors) {
      if (descriptor.empty())
        continue;
      VideoStream stream =
          VideoQualityTest::DefaultVideoStream(*params, video_idx);
      std::vector<int> v = VideoQualityTest::ParseCSV(descriptor);
      if (v[0] != -1)
        stream.width = static_cast<size_t>(v[0]);
      if (v[1] != -1)
        stream.height = static_cast<size_t>(v[1]);
      if (v[2] != -1)
        stream.max_framerate = v[2];
      if (v[3] != -1)
        stream.min_bitrate_bps = v[3];
      if (v[4] != -1)
        stream.target_bitrate_bps = v[4];
      if (v[5] != -1)
        stream.max_bitrate_bps = v[5];
      if (v.size() > 6 && v[6] != -1)
        stream.max_qp = v[6];
      if (v.size() > 7) {
        stream.temporal_layer_thresholds_bps.clear();
        stream.temporal_layer_thresholds_bps.insert(
            stream.temporal_layer_thresholds_bps.end(), v.begin() + 7, v.end());
      } else {
        // Automatic TL thresholds for more than two layers not supported.
        RTC_CHECK_LE(params->video[video_idx].num_temporal_layers, 2);
      }
      params->ss[video_idx].streams.push_back(stream);
    }
  }

  params->ss[video_idx].num_spatial_layers = std::max(1, num_spatial_layers);
  params->ss[video_idx].selected_stream = selected_stream;

  params->ss[video_idx].selected_sl = selected_sl;
  RTC_CHECK(params->ss[video_idx].spatial_layers.empty());
  for (auto descriptor : sl_descriptors) {
    if (descriptor.empty())
      continue;
    std::vector<int> v = VideoQualityTest::ParseCSV(descriptor);
    RTC_CHECK_GT(v[2], 0);

    SpatialLayer layer;
    layer.scaling_factor_num = v[0] == -1 ? 1 : v[0];
    layer.scaling_factor_den = v[1] == -1 ? 1 : v[1];
    layer.target_bitrate_bps = v[2];
    params->ss[video_idx].spatial_layers.push_back(layer);
  }
}

void VideoQualityTest::SetupVideo(Transport* send_transport,
                                  Transport* recv_transport) {
  size_t total_streams_used = 0;
  size_t num_flexfec_streams = params_.video[0].flexfec ? 1 : 0;
  CreateAudioAndFecSendConfigs(0, num_flexfec_streams, send_transport);
  CreateMatchingAudioAndFecConfigs(recv_transport);
  video_receive_configs_.clear();
  video_send_configs_.clear();
  video_encoders_.clear();
  video_encoder_configs_.clear();
  allocated_decoders_.clear();
  bool decode_all_receive_streams = true;
  size_t num_video_substreams = params_.ss[0].streams.size();
  RTC_CHECK(num_video_streams_ > 0);
  video_encoder_configs_.resize(num_video_streams_);
  for (size_t video_idx = 0; video_idx < num_video_streams_; ++video_idx) {
    video_send_configs_.push_back(VideoSendStream::Config(send_transport));
    video_encoders_.push_back(nullptr);
    video_encoder_configs_.push_back(VideoEncoderConfig());
    num_video_substreams = params_.ss[video_idx].streams.size();
    RTC_CHECK_GT(num_video_substreams, 0);
    CreateVideoSendConfig(&video_send_configs_[video_idx], num_video_substreams,
                          total_streams_used, send_transport);

    int payload_type;
    if (params_.video[video_idx].codec == "H264") {
      video_encoders_[video_idx] =
          H264Encoder::Create(cricket::VideoCodec("H264"));
      payload_type = kPayloadTypeH264;
    } else if (params_.video[video_idx].codec == "VP8") {
      if (params_.screenshare[video_idx].enabled &&
          params_.ss[video_idx].streams.size() > 1) {
        // Simulcast screenshare needs a simulcast encoder adapter to work,
        // since encoders usually can't natively do simulcast with different
        // frame rates for the different layers.
        video_encoders_[video_idx].reset(
            new SimulcastEncoderAdapter(new InternalEncoderFactory()));
      } else {
        video_encoders_[video_idx] = VP8Encoder::Create();
      }
      payload_type = kPayloadTypeVP8;
    } else if (params_.video[video_idx].codec == "VP9") {
      video_encoders_[video_idx] = VP9Encoder::Create();
      payload_type = kPayloadTypeVP9;
    } else {
      RTC_NOTREACHED() << "Codec not supported!";
      return;
    }
    video_send_configs_[video_idx].encoder_settings.encoder =
        video_encoders_[video_idx].get();
    video_send_configs_[video_idx].encoder_settings.payload_name =
        params_.video[video_idx].codec;
    video_send_configs_[video_idx].encoder_settings.payload_type = payload_type;
    video_send_configs_[video_idx].rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
    video_send_configs_[video_idx].rtp.rtx.payload_type = kSendRtxPayloadType;
    for (size_t i = 0; i < num_video_substreams; ++i) {
      video_send_configs_[video_idx].rtp.rtx.ssrcs.push_back(
          kSendRtxSsrcs[i + total_streams_used]);
    }
    video_send_configs_[video_idx].rtp.extensions.clear();
    if (params_.call.send_side_bwe) {
      video_send_configs_[video_idx].rtp.extensions.push_back(
          RtpExtension(RtpExtension::kTransportSequenceNumberUri,
                       test::kTransportSequenceNumberExtensionId));
    } else {
      video_send_configs_[video_idx].rtp.extensions.push_back(RtpExtension(
          RtpExtension::kAbsSendTimeUri, test::kAbsSendTimeExtensionId));
    }
    video_send_configs_[video_idx].rtp.extensions.push_back(
        RtpExtension(RtpExtension::kVideoContentTypeUri,
                     test::kVideoContentTypeExtensionId));
    video_send_configs_[video_idx].rtp.extensions.push_back(RtpExtension(
        RtpExtension::kVideoTimingUri, test::kVideoTimingExtensionId));

    video_encoder_configs_[video_idx].min_transmit_bitrate_bps =
        params_.video[video_idx].min_transmit_bps;

    video_send_configs_[video_idx].suspend_below_min_bitrate =
        params_.video[video_idx].suspend_below_min_bitrate;

    video_encoder_configs_[video_idx].number_of_streams =
        params_.ss[video_idx].streams.size();
    video_encoder_configs_[video_idx].max_bitrate_bps = 0;
    for (size_t i = 0; i < params_.ss[video_idx].streams.size(); ++i) {
      video_encoder_configs_[video_idx].max_bitrate_bps +=
          params_.ss[video_idx].streams[i].max_bitrate_bps;
    }
    if (params_.ss[video_idx].infer_streams) {
      video_encoder_configs_[video_idx].video_stream_factory =
          new rtc::RefCountedObject<cricket::EncoderStreamFactory>(
              params_.video[video_idx].codec,
              params_.ss[video_idx].streams[0].max_qp,
              params_.video[video_idx].fps,
              params_.screenshare[video_idx].enabled, true);
    } else {
      video_encoder_configs_[video_idx].video_stream_factory =
          new rtc::RefCountedObject<VideoStreamFactory>(
              params_.ss[video_idx].streams);
    }

    video_encoder_configs_[video_idx].spatial_layers =
        params_.ss[video_idx].spatial_layers;

    std::vector<VideoReceiveStream::Config> new_receive_configs =
        CreateMatchingVideoReceiveConfigs(video_send_configs_[video_idx],
                                          recv_transport);

    decode_all_receive_streams = params_.ss[video_idx].selected_stream ==
                                 params_.ss[video_idx].streams.size();

    for (size_t i = 0; i < num_video_substreams; ++i) {
      new_receive_configs[i].rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      new_receive_configs[i].rtp.rtx_ssrc =
          kSendRtxSsrcs[i + total_streams_used];
      new_receive_configs[i]
          .rtp.rtx_associated_payload_types[kSendRtxPayloadType] = payload_type;
      new_receive_configs[i].rtp.transport_cc = params_.call.send_side_bwe;
      new_receive_configs[i].rtp.remb = !params_.call.send_side_bwe;
      // Enable RTT calculation so NTP time estimator will work.
      new_receive_configs[i].rtp.rtcp_xr.receiver_reference_time_report = true;
      // Force fake decoders on non-selected simulcast streams.
      if (!decode_all_receive_streams &&
          i != params_.ss[video_idx].selected_stream) {
        VideoReceiveStream::Decoder decoder;
        decoder.decoder = new test::FakeDecoder();
        decoder.payload_type =
            video_send_configs_[video_idx].encoder_settings.payload_type;
        decoder.payload_name =
            video_send_configs_[video_idx].encoder_settings.payload_name;
        new_receive_configs[i].decoders.clear();
        allocated_decoders_.emplace_back(decoder.decoder);
        new_receive_configs[i].decoders.push_back(decoder);
      }
    }

    for (VideoReceiveStream::Config& config : new_receive_configs) {
      video_receive_configs_.push_back(config.Copy());
    }

    if (params_.screenshare[video_idx].enabled) {
      // Fill out codec settings.
      video_encoder_configs_[video_idx].content_type =
          VideoEncoderConfig::ContentType::kScreen;
      degradation_preference_ =
          VideoSendStream::DegradationPreference::kMaintainResolution;
      if (params_.video[video_idx].codec == "VP8") {
        VideoCodecVP8 vp8_settings = VideoEncoder::GetDefaultVp8Settings();
        vp8_settings.denoisingOn = false;
        vp8_settings.frameDroppingOn = false;
        vp8_settings.numberOfTemporalLayers = static_cast<unsigned char>(
            params_.video[video_idx].num_temporal_layers);
        video_encoder_configs_[video_idx].encoder_specific_settings =
            new rtc::RefCountedObject<
                VideoEncoderConfig::Vp8EncoderSpecificSettings>(vp8_settings);
      } else if (params_.video[video_idx].codec == "VP9") {
        VideoCodecVP9 vp9_settings = VideoEncoder::GetDefaultVp9Settings();
        vp9_settings.denoisingOn = false;
        vp9_settings.frameDroppingOn = false;
        vp9_settings.numberOfTemporalLayers = static_cast<unsigned char>(
            params_.video[video_idx].num_temporal_layers);
        vp9_settings.numberOfSpatialLayers = static_cast<unsigned char>(
            params_.ss[video_idx].num_spatial_layers);
        video_encoder_configs_[video_idx].encoder_specific_settings =
            new rtc::RefCountedObject<
                VideoEncoderConfig::Vp9EncoderSpecificSettings>(vp9_settings);
      }
    } else if (params_.ss[video_idx].num_spatial_layers > 1) {
      // If SVC mode without screenshare, still need to set codec specifics.
      RTC_CHECK(params_.video[video_idx].codec == "VP9");
      VideoCodecVP9 vp9_settings = VideoEncoder::GetDefaultVp9Settings();
      vp9_settings.numberOfTemporalLayers = static_cast<unsigned char>(
          params_.video[video_idx].num_temporal_layers);
      vp9_settings.numberOfSpatialLayers =
          static_cast<unsigned char>(params_.ss[video_idx].num_spatial_layers);
      video_encoder_configs_[video_idx].encoder_specific_settings =
          new rtc::RefCountedObject<
              VideoEncoderConfig::Vp9EncoderSpecificSettings>(vp9_settings);
    }
    total_streams_used += num_video_substreams;
  }

  // FEC supported only for single video stream mode yet.
  if (params_.video[0].flexfec) {
    video_send_configs_[0].rtp.flexfec.payload_type = kFlexfecPayloadType;
    video_send_configs_[0].rtp.flexfec.ssrc = kFlexfecSendSsrc;
    if (decode_all_receive_streams) {
      for (uint32_t media_ssrc : video_send_configs_[0].rtp.ssrcs) {
        video_send_configs_[0].rtp.flexfec.protected_media_ssrcs.push_back(
            media_ssrc);
      }
    } else {
      video_send_configs_[0].rtp.flexfec.protected_media_ssrcs = {
          kVideoSendSsrcs[params_.ss[0].selected_stream]};
    }

    // The matching receive config is _not_ created by
    // CreateMatchingReceiveConfigs, since VideoQualityTest is not a BaseTest.
    // Set up the receive config manually instead.
    FlexfecReceiveStream::Config flexfec_receive_config(recv_transport);
    flexfec_receive_config.payload_type =
        video_send_configs_[0].rtp.flexfec.payload_type;
    flexfec_receive_config.remote_ssrc =
        video_send_configs_[0].rtp.flexfec.ssrc;
    flexfec_receive_config.protected_media_ssrcs =
        video_send_configs_[0].rtp.flexfec.protected_media_ssrcs;
    flexfec_receive_config.local_ssrc = kReceiverLocalVideoSsrc;
    flexfec_receive_config.transport_cc = params_.call.send_side_bwe;
    if (params_.call.send_side_bwe) {
      flexfec_receive_config.rtp_header_extensions.push_back(
          RtpExtension(RtpExtension::kTransportSequenceNumberUri,
                       test::kTransportSequenceNumberExtensionId));
    } else {
      flexfec_receive_config.rtp_header_extensions.push_back(RtpExtension(
          RtpExtension::kAbsSendTimeUri, test::kAbsSendTimeExtensionId));
    }
    flexfec_receive_configs_.push_back(flexfec_receive_config);
    if (num_video_substreams > 0) {
      video_receive_configs_[0].rtp.protected_by_flexfec = true;
    }
  }

  if (params_.video[0].ulpfec) {
    video_send_configs_[0].rtp.ulpfec.red_payload_type = kRedPayloadType;
    video_send_configs_[0].rtp.ulpfec.ulpfec_payload_type = kUlpfecPayloadType;
    video_send_configs_[0].rtp.ulpfec.red_rtx_payload_type = kRtxRedPayloadType;

    if (decode_all_receive_streams) {
      for (auto it = video_receive_configs_.begin();
           it != video_receive_configs_.end(); ++it) {
        it->rtp.red_payload_type =
            video_send_configs_[0].rtp.ulpfec.red_payload_type;
        it->rtp.ulpfec_payload_type =
            video_send_configs_[0].rtp.ulpfec.ulpfec_payload_type;
        it->rtp.rtx_associated_payload_types
            [video_send_configs_[0].rtp.ulpfec.red_rtx_payload_type] =
            video_send_configs_[0].rtp.ulpfec.red_payload_type;
      }
    } else {
      video_receive_configs_[params_.ss[0].selected_stream]
          .rtp.red_payload_type =
          video_send_configs_[0].rtp.ulpfec.red_payload_type;
      video_receive_configs_[params_.ss[0].selected_stream]
          .rtp.ulpfec_payload_type =
          video_send_configs_[0].rtp.ulpfec.ulpfec_payload_type;
      video_receive_configs_[params_.ss[0].selected_stream]
          .rtp.rtx_associated_payload_types
              [video_send_configs_[0].rtp.ulpfec.red_rtx_payload_type] =
          video_send_configs_[0].rtp.ulpfec.red_payload_type;
    }
  }
}

void VideoQualityTest::SetupThumbnails(Transport* send_transport,
                                       Transport* recv_transport) {
  for (int i = 0; i < params_.call.num_thumbnails; ++i) {
    thumbnail_encoders_.emplace_back(VP8Encoder::Create());

    // Thumbnails will be send in the other way: from receiver_call to
    // sender_call.
    VideoSendStream::Config thumbnail_send_config(recv_transport);
    thumbnail_send_config.rtp.ssrcs.push_back(kThumbnailSendSsrcStart + i);
    thumbnail_send_config.encoder_settings.encoder =
        thumbnail_encoders_.back().get();
    thumbnail_send_config.encoder_settings.payload_name =
        params_.video[0].codec;
    thumbnail_send_config.encoder_settings.payload_type = kPayloadTypeVP8;
    thumbnail_send_config.rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
    thumbnail_send_config.rtp.rtx.payload_type = kSendRtxPayloadType;
    thumbnail_send_config.rtp.rtx.ssrcs.push_back(kThumbnailRtxSsrcStart + i);
    thumbnail_send_config.rtp.extensions.clear();
    if (params_.call.send_side_bwe) {
      thumbnail_send_config.rtp.extensions.push_back(
          RtpExtension(RtpExtension::kTransportSequenceNumberUri,
                       test::kTransportSequenceNumberExtensionId));
    } else {
      thumbnail_send_config.rtp.extensions.push_back(RtpExtension(
          RtpExtension::kAbsSendTimeUri, test::kAbsSendTimeExtensionId));
    }

    VideoEncoderConfig thumbnail_encoder_config;
    thumbnail_encoder_config.min_transmit_bitrate_bps = 7500;
    thumbnail_send_config.suspend_below_min_bitrate =
        params_.video[0].suspend_below_min_bitrate;
    thumbnail_encoder_config.number_of_streams = 1;
    thumbnail_encoder_config.max_bitrate_bps = 50000;
    if (params_.ss[0].infer_streams) {
      thumbnail_encoder_config.video_stream_factory =
          new rtc::RefCountedObject<VideoStreamFactory>(params_.ss[0].streams);
    } else {
      thumbnail_encoder_config.video_stream_factory =
          new rtc::RefCountedObject<cricket::EncoderStreamFactory>(
              params_.video[0].codec, params_.ss[0].streams[0].max_qp,
              params_.video[0].fps, params_.screenshare[0].enabled, true);
    }
    thumbnail_encoder_config.spatial_layers = params_.ss[0].spatial_layers;

    VideoReceiveStream::Config thumbnail_receive_config(send_transport);
    thumbnail_receive_config.rtp.remb = false;
    thumbnail_receive_config.rtp.transport_cc = true;
    thumbnail_receive_config.rtp.local_ssrc = kReceiverLocalVideoSsrc;
    for (const RtpExtension& extension : thumbnail_send_config.rtp.extensions)
      thumbnail_receive_config.rtp.extensions.push_back(extension);
    thumbnail_receive_config.renderer = &fake_renderer_;

    VideoReceiveStream::Decoder decoder =
        test::CreateMatchingDecoder(thumbnail_send_config.encoder_settings);
    allocated_decoders_.push_back(
        std::unique_ptr<VideoDecoder>(decoder.decoder));
    thumbnail_receive_config.decoders.clear();
    thumbnail_receive_config.decoders.push_back(decoder);
    thumbnail_receive_config.rtp.remote_ssrc =
        thumbnail_send_config.rtp.ssrcs[0];

    thumbnail_receive_config.rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
    thumbnail_receive_config.rtp.rtx_ssrc = kThumbnailRtxSsrcStart + i;
    thumbnail_receive_config.rtp
        .rtx_associated_payload_types[kSendRtxPayloadType] = kPayloadTypeVP8;
    thumbnail_receive_config.rtp.transport_cc = params_.call.send_side_bwe;
    thumbnail_receive_config.rtp.remb = !params_.call.send_side_bwe;

    thumbnail_encoder_configs_.push_back(thumbnail_encoder_config.Copy());
    thumbnail_send_configs_.push_back(thumbnail_send_config.Copy());
    thumbnail_receive_configs_.push_back(thumbnail_receive_config.Copy());
  }

  for (int i = 0; i < params_.call.num_thumbnails; ++i) {
    thumbnail_send_streams_.push_back(receiver_call_->CreateVideoSendStream(
        thumbnail_send_configs_[i].Copy(),
        thumbnail_encoder_configs_[i].Copy()));
    thumbnail_receive_streams_.push_back(sender_call_->CreateVideoReceiveStream(
        thumbnail_receive_configs_[i].Copy()));
  }
}

void VideoQualityTest::DestroyThumbnailStreams() {
  for (VideoSendStream* thumbnail_send_stream : thumbnail_send_streams_) {
    receiver_call_->DestroyVideoSendStream(thumbnail_send_stream);
  }
  thumbnail_send_streams_.clear();
  for (VideoReceiveStream* thumbnail_receive_stream :
       thumbnail_receive_streams_) {
    sender_call_->DestroyVideoReceiveStream(thumbnail_receive_stream);
  }
  thumbnail_send_streams_.clear();
  thumbnail_receive_streams_.clear();
  for (std::unique_ptr<test::VideoCapturer>& video_caputurer :
       thumbnail_capturers_) {
    video_caputurer.reset();
  }
}

void VideoQualityTest::SetupThumbnailCapturers(size_t num_thumbnail_streams) {
  VideoStream thumbnail = DefaultThumbnailStream();
  for (size_t i = 0; i < num_thumbnail_streams; ++i) {
    thumbnail_capturers_.emplace_back(test::FrameGeneratorCapturer::Create(
        static_cast<int>(thumbnail.width), static_cast<int>(thumbnail.height),
        thumbnail.max_framerate, clock_));
    RTC_DCHECK(thumbnail_capturers_.back());
  }
}

std::unique_ptr<test::FrameGenerator> VideoQualityTest::CreateFrameGenerator(
    size_t video_idx) {
  // Setup frame generator.
  const size_t kWidth = 1850;
  const size_t kHeight = 1110;
  std::unique_ptr<test::FrameGenerator> frame_generator;
  if (params_.screenshare[video_idx].generate_slides) {
    frame_generator = test::FrameGenerator::CreateSlideGenerator(
        kWidth, kHeight,
        params_.screenshare[video_idx].slide_change_interval *
            params_.video[video_idx].fps);
  } else {
    std::vector<std::string> slides = params_.screenshare[video_idx].slides;
    if (slides.size() == 0) {
      slides.push_back(test::ResourcePath("web_screenshot_1850_1110", "yuv"));
      slides.push_back(test::ResourcePath("presentation_1850_1110", "yuv"));
      slides.push_back(test::ResourcePath("photo_1850_1110", "yuv"));
      slides.push_back(test::ResourcePath("difficult_photo_1850_1110", "yuv"));
    }
    if (params_.screenshare[video_idx].scroll_duration == 0) {
      // Cycle image every slide_change_interval seconds.
      frame_generator = test::FrameGenerator::CreateFromYuvFile(
          slides, kWidth, kHeight,
          params_.screenshare[video_idx].slide_change_interval *
              params_.video[video_idx].fps);
    } else {
      RTC_CHECK_LE(params_.video[video_idx].width, kWidth);
      RTC_CHECK_LE(params_.video[video_idx].height, kHeight);
      RTC_CHECK_GT(params_.screenshare[video_idx].slide_change_interval, 0);
      const int kPauseDurationMs =
          (params_.screenshare[video_idx].slide_change_interval -
           params_.screenshare[video_idx].scroll_duration) *
          1000;
      RTC_CHECK_LE(params_.screenshare[video_idx].scroll_duration,
                   params_.screenshare[video_idx].slide_change_interval);

      frame_generator = test::FrameGenerator::CreateScrollingInputFromYuvFiles(
          clock_, slides, kWidth, kHeight, params_.video[video_idx].width,
          params_.video[video_idx].height,
          params_.screenshare[video_idx].scroll_duration * 1000,
          kPauseDurationMs);
    }
  }
  return frame_generator;
}

void VideoQualityTest::CreateCapturers() {
  video_capturers_.resize(num_video_streams_);
  for (size_t video_idx = 0; video_idx < num_video_streams_; ++video_idx) {
    if (params_.screenshare[video_idx].enabled) {
      std::unique_ptr<test::FrameGenerator> frame_generator =
          CreateFrameGenerator(video_idx);
      test::FrameGeneratorCapturer* frame_generator_capturer =
          new test::FrameGeneratorCapturer(clock_, std::move(frame_generator),
                                           params_.video[video_idx].fps);
      EXPECT_TRUE(frame_generator_capturer->Init());
      video_capturers_[video_idx].reset(frame_generator_capturer);
    } else {
      if (params_.video[video_idx].clip_name == "Generator") {
        video_capturers_[video_idx].reset(test::FrameGeneratorCapturer::Create(
            static_cast<int>(params_.video[video_idx].width),
            static_cast<int>(params_.video[video_idx].height),
            params_.video[video_idx].fps, clock_));
      } else if (params_.video[video_idx].clip_name.empty()) {
        video_capturers_[video_idx].reset(test::VcmCapturer::Create(
            params_.video[video_idx].width, params_.video[video_idx].height,
            params_.video[video_idx].fps,
            params_.video[video_idx].capture_device_index));
        if (!video_capturers_[video_idx]) {
          // Failed to get actual camera, use chroma generator as backup.
          video_capturers_[video_idx].reset(
              test::FrameGeneratorCapturer::Create(
                  static_cast<int>(params_.video[video_idx].width),
                  static_cast<int>(params_.video[video_idx].height),
                  params_.video[video_idx].fps, clock_));
        }
      } else {
        video_capturers_[video_idx].reset(
            test::FrameGeneratorCapturer::CreateFromYuvFile(
                test::ResourcePath(params_.video[video_idx].clip_name, "yuv"),
                params_.video[video_idx].width, params_.video[video_idx].height,
                params_.video[video_idx].fps, clock_));
        ASSERT_TRUE(video_capturers_[video_idx])
            << "Could not create capturer for "
            << params_.video[video_idx].clip_name
            << ".yuv. Is this resource file present?";
      }
    }
    RTC_DCHECK(video_capturers_[video_idx].get());
  }
}

std::unique_ptr<test::LayerFilteringTransport>
VideoQualityTest::CreateSendTransport() {
  return rtc::MakeUnique<test::LayerFilteringTransport>(
      &task_queue_, params_.pipe, sender_call_.get(), kPayloadTypeVP8,
      kPayloadTypeVP9, params_.video[0].selected_tl, params_.ss[0].selected_sl,
      payload_type_map_, kVideoSendSsrcs[0],
      static_cast<uint32_t>(kVideoSendSsrcs[0] + params_.ss[0].streams.size() -
                            1));
}

std::unique_ptr<test::DirectTransport>
VideoQualityTest::CreateReceiveTransport() {
  return rtc::MakeUnique<test::DirectTransport>(
      &task_queue_, params_.pipe, receiver_call_.get(), payload_type_map_);
}

void VideoQualityTest::CreateVideoStreams() {
  RTC_DCHECK(video_send_streams_.empty());
  RTC_DCHECK(video_receive_streams_.empty());
  RTC_DCHECK_EQ(video_send_configs_.size(), num_video_streams_);
  for (size_t i = 0; i < video_send_configs_.size(); ++i) {
    video_send_streams_.push_back(sender_call_->CreateVideoSendStream(
        video_send_configs_[i].Copy(), video_encoder_configs_[i].Copy()));
  }
  for (size_t i = 0; i < video_receive_configs_.size(); ++i) {
    video_receive_streams_.push_back(receiver_call_->CreateVideoReceiveStream(
        video_receive_configs_[i].Copy()));
  }

  AssociateFlexfecStreamsWithVideoStreams();
}

void VideoQualityTest::DestroyStreams() {
  CallTest::DestroyStreams();

  for (VideoSendStream* video_send_stream : video_send_streams_)
    sender_call_->DestroyVideoSendStream(video_send_stream);
}

void VideoQualityTest::RunWithAnalyzer(const Params& params) {
  num_video_streams_ = params.call.dual_video ? 2 : 1;
  std::unique_ptr<test::LayerFilteringTransport> send_transport;
  std::unique_ptr<test::DirectTransport> recv_transport;
  FILE* graph_data_output_file = nullptr;
  std::unique_ptr<VideoAnalyzer> analyzer;

  params_ = params;

  RTC_CHECK(!params_.audio.enabled);
  // TODO(ivica): Merge with RunWithRenderer and use a flag / argument to
  // differentiate between the analyzer and the renderer case.
  CheckParams();

  if (!params_.analyzer.graph_data_output_filename.empty()) {
    graph_data_output_file =
        fopen(params_.analyzer.graph_data_output_filename.c_str(), "w");
    RTC_CHECK(graph_data_output_file)
        << "Can't open the file " << params_.analyzer.graph_data_output_filename
        << "!";
  }

  if (!params.logging.rtc_event_log_name.empty()) {
    send_event_log_ =
        RtcEventLog::Create(clock_, RtcEventLog::EncodingType::Legacy);
    recv_event_log_ =
        RtcEventLog::Create(clock_, RtcEventLog::EncodingType::Legacy);
    std::unique_ptr<RtcEventLogOutputFile> send_output(
        rtc::MakeUnique<RtcEventLogOutputFile>(
            params.logging.rtc_event_log_name + "_send",
            RtcEventLog::kUnlimitedOutput));
    std::unique_ptr<RtcEventLogOutputFile> recv_output(
        rtc::MakeUnique<RtcEventLogOutputFile>(
            params.logging.rtc_event_log_name + "_recv",
            RtcEventLog::kUnlimitedOutput));
    bool event_log_started =
        send_event_log_->StartLogging(std::move(send_output),
                                      RtcEventLog::kImmediateOutput) &&
        recv_event_log_->StartLogging(std::move(recv_output),
                                      RtcEventLog::kImmediateOutput);
    RTC_DCHECK(event_log_started);
  } else {
    send_event_log_ = RtcEventLog::CreateNull();
    recv_event_log_ = RtcEventLog::CreateNull();
  }

  Call::Config send_call_config(send_event_log_.get());
  Call::Config recv_call_config(recv_event_log_.get());
  send_call_config.bitrate_config = params.call.call_bitrate_config;
  recv_call_config.bitrate_config = params.call.call_bitrate_config;

  task_queue_.SendTask([this, &send_call_config, &recv_call_config,
                        &send_transport, &recv_transport]() {
    CreateCalls(send_call_config, recv_call_config);
    send_transport = CreateSendTransport();
    recv_transport = CreateReceiveTransport();
  });

  std::string graph_title = params_.analyzer.graph_title;
  if (graph_title.empty())
    graph_title = VideoQualityTest::GenerateGraphTitle();
  bool is_quick_test_enabled = field_trial::IsEnabled("WebRTC-QuickPerfTest");
  analyzer = rtc::MakeUnique<VideoAnalyzer>(
      send_transport.get(), params_.analyzer.test_label,
      params_.analyzer.avg_psnr_threshold, params_.analyzer.avg_ssim_threshold,
      is_quick_test_enabled
          ? kFramesSentInQuickTest
          : params_.analyzer.test_durations_secs * params_.video[0].fps,
      graph_data_output_file, graph_title,
      kVideoSendSsrcs[params_.ss[0].selected_stream],
      kSendRtxSsrcs[params_.ss[0].selected_stream],
      static_cast<size_t>(params_.ss[0].selected_stream),
      params.ss[0].selected_sl, params_.video[0].selected_tl,
      is_quick_test_enabled, clock_, params_.logging.rtp_dump_name);

  task_queue_.SendTask([&]() {
    analyzer->SetCall(sender_call_.get());
    analyzer->SetReceiver(receiver_call_->Receiver());
    send_transport->SetReceiver(analyzer.get());
    recv_transport->SetReceiver(sender_call_->Receiver());

    SetupVideo(analyzer.get(), recv_transport.get());
    SetupThumbnails(analyzer.get(), recv_transport.get());
    video_receive_configs_[params_.ss[0].selected_stream].renderer =
        analyzer.get();
    video_send_configs_[0].pre_encode_callback = analyzer->pre_encode_proxy();
    RTC_DCHECK(!video_send_configs_[0].post_encode_callback);
    video_send_configs_[0].post_encode_callback =
        analyzer->encode_timing_proxy();

    CreateFlexfecStreams();
    CreateVideoStreams();
    analyzer->SetSendStream(video_send_streams_[0]);
    if (video_receive_streams_.size() == 1)
      analyzer->SetReceiveStream(video_receive_streams_[0]);

    video_send_streams_[0]->SetSource(analyzer->OutputInterface(),
                                      degradation_preference_);
    SetupThumbnailCapturers(params_.call.num_thumbnails);
    for (size_t i = 0; i < thumbnail_send_streams_.size(); ++i) {
      thumbnail_send_streams_[i]->SetSource(thumbnail_capturers_[i].get(),
                                            degradation_preference_);
    }

    CreateCapturers();

    analyzer->SetSource(video_capturers_[0].get(), params_.ss[0].infer_streams);

    for (size_t video_idx = 1; video_idx < num_video_streams_; ++video_idx) {
      video_send_streams_[video_idx]->SetSource(
          video_capturers_[video_idx].get(), degradation_preference_);
    }

    StartEncodedFrameLogs(video_send_streams_[0]);
    StartEncodedFrameLogs(
        video_receive_streams_[params_.ss[0].selected_stream]);
    for (VideoSendStream* video_send_stream : video_send_streams_)
      video_send_stream->Start();
    for (VideoSendStream* thumbnail_send_stream : thumbnail_send_streams_)
      thumbnail_send_stream->Start();
    for (VideoReceiveStream* receive_stream : video_receive_streams_)
      receive_stream->Start();
    for (VideoReceiveStream* thumbnail_receive_stream :
         thumbnail_receive_streams_)
      thumbnail_receive_stream->Start();

    analyzer->StartMeasuringCpuProcessTime();

    for (size_t video_idx = 0; video_idx < num_video_streams_; ++video_idx) {
      video_capturers_[video_idx]->Start();
    }
    for (std::unique_ptr<test::VideoCapturer>& video_caputurer :
         thumbnail_capturers_) {
      video_caputurer->Start();
    }
  });

  analyzer->Wait();

  event_log_->StopLogging();

  task_queue_.SendTask([&]() {
    for (std::unique_ptr<test::VideoCapturer>& video_caputurer :
         thumbnail_capturers_)
      video_caputurer->Stop();
    for (size_t video_idx = 0; video_idx < num_video_streams_; ++video_idx) {
      video_capturers_[video_idx]->Stop();
    }
    for (VideoReceiveStream* thumbnail_receive_stream :
         thumbnail_receive_streams_)
      thumbnail_receive_stream->Stop();
    for (VideoReceiveStream* receive_stream : video_receive_streams_)
      receive_stream->Stop();
    for (VideoSendStream* thumbnail_send_stream : thumbnail_send_streams_)
      thumbnail_send_stream->Stop();
    for (VideoSendStream* video_send_stream : video_send_streams_)
      video_send_stream->Stop();

    DestroyStreams();
    DestroyThumbnailStreams();

    if (graph_data_output_file)
      fclose(graph_data_output_file);

    video_capturers_.clear();
    send_transport.reset();
    recv_transport.reset();

    DestroyCalls();
  });
}

void VideoQualityTest::SetupAudio(Transport* transport,
                                  AudioReceiveStream** audio_receive_stream) {
  audio_send_config_ = AudioSendStream::Config(transport);
  audio_send_config_.rtp.ssrc = kAudioSendSsrc;

  // Add extension to enable audio send side BWE, and allow audio bit rate
  // adaptation.
  audio_send_config_.rtp.extensions.clear();
  if (params_.call.send_side_bwe) {
    audio_send_config_.rtp.extensions.push_back(
        webrtc::RtpExtension(webrtc::RtpExtension::kTransportSequenceNumberUri,
                             test::kTransportSequenceNumberExtensionId));
    audio_send_config_.min_bitrate_bps = kOpusMinBitrateBps;
    audio_send_config_.max_bitrate_bps = kOpusBitrateFbBps;
  }
  audio_send_config_.send_codec_spec = AudioSendStream::Config::SendCodecSpec(
      kAudioSendPayloadType,
      {"OPUS", 48000, 2,
       {{"usedtx", (params_.audio.dtx ? "1" : "0")},
         {"stereo", "1"}}});
  audio_send_config_.encoder_factory = encoder_factory_;
  audio_send_stream_ = sender_call_->CreateAudioSendStream(audio_send_config_);

  AudioReceiveStream::Config audio_config;
  audio_config.rtp.local_ssrc = kReceiverLocalAudioSsrc;
  audio_config.rtcp_send_transport = transport;
  audio_config.rtp.remote_ssrc = audio_send_config_.rtp.ssrc;
  audio_config.rtp.transport_cc = params_.call.send_side_bwe;
  audio_config.rtp.extensions = audio_send_config_.rtp.extensions;
  audio_config.decoder_factory = decoder_factory_;
  audio_config.decoder_map = {{kAudioSendPayloadType, {"OPUS", 48000, 2}}};
  if (params_.video[0].enabled && params_.audio.sync_video)
    audio_config.sync_group = kSyncGroup;

  *audio_receive_stream =
      receiver_call_->CreateAudioReceiveStream(audio_config);
}

void VideoQualityTest::RunWithRenderers(const Params& params) {
  num_video_streams_ = params.call.dual_video ? 2 : 1;
  std::unique_ptr<test::LayerFilteringTransport> send_transport;
  std::unique_ptr<test::DirectTransport> recv_transport;
  std::unique_ptr<test::VideoRenderer> local_preview;
  std::vector<std::unique_ptr<test::VideoRenderer>> loopback_renderers;
  AudioReceiveStream* audio_receive_stream = nullptr;

  task_queue_.SendTask([&]() {
    params_ = params;
    CheckParams();

    // TODO(ivica): Remove bitrate_config and use the default Call::Config(), to
    // match the full stack tests.
    Call::Config call_config(event_log_.get());
    call_config.bitrate_config = params_.call.call_bitrate_config;

    rtc::scoped_refptr<test::FakeAudioDevice> fake_audio_device =
        new rtc::RefCountedObject<test::FakeAudioDevice>(
            test::FakeAudioDevice::CreatePulsedNoiseCapturer(32000, 48000),
            test::FakeAudioDevice::CreateDiscardRenderer(48000),
            1.f);

    if (params_.audio.enabled) {
      AudioState::Config audio_state_config;
      audio_state_config.audio_mixer = AudioMixerImpl::Create();
      audio_state_config.audio_processing = AudioProcessingBuilder().Create();
      audio_state_config.audio_device_module = fake_audio_device;
      call_config.audio_state = AudioState::Create(audio_state_config);
      fake_audio_device->RegisterAudioCallback(
          call_config.audio_state->audio_transport());
    }

    CreateCalls(call_config, call_config);

    // TODO(minyue): consider if this is a good transport even for audio only
    // calls.
    send_transport = CreateSendTransport();

    recv_transport = CreateReceiveTransport();

    // TODO(ivica): Use two calls to be able to merge with RunWithAnalyzer or at
    // least share as much code as possible. That way this test would also match
    // the full stack tests better.
    send_transport->SetReceiver(receiver_call_->Receiver());
    recv_transport->SetReceiver(sender_call_->Receiver());

    if (params_.video[0].enabled) {
      // Create video renderers.
      local_preview.reset(test::VideoRenderer::Create(
          "Local Preview", params_.video[0].width, params_.video[0].height));

      SetupVideo(send_transport.get(), recv_transport.get());
      video_send_configs_[0].pre_encode_callback = local_preview.get();

      size_t num_streams_processed = 0;
      for (size_t video_idx = 0; video_idx < num_video_streams_; ++video_idx) {
        const size_t selected_stream_id = params_.ss[video_idx].selected_stream;
        const size_t num_streams = params_.ss[video_idx].streams.size();
        if (selected_stream_id == num_streams) {
          for (size_t stream_id = 0; stream_id < num_streams; ++stream_id) {
            std::ostringstream oss;
            oss << "Loopback Video #" << video_idx << " - Stream #"
                << static_cast<int>(stream_id);
            loopback_renderers.emplace_back(test::VideoRenderer::Create(
                oss.str().c_str(),
                params_.ss[video_idx].streams[stream_id].width,
                params_.ss[video_idx].streams[stream_id].height));
            video_receive_configs_[stream_id + num_streams_processed].renderer =
                loopback_renderers.back().get();
            if (params_.audio.enabled && params_.audio.sync_video)
              video_receive_configs_[stream_id + num_streams_processed]
                  .sync_group = kSyncGroup;
          }
        } else {
          std::ostringstream oss;
          oss << "Loopback Video #" << video_idx;
          loopback_renderers.emplace_back(test::VideoRenderer::Create(
              oss.str().c_str(),
              params_.ss[video_idx].streams[selected_stream_id].width,
              params_.ss[video_idx].streams[selected_stream_id].height));
          video_receive_configs_[selected_stream_id + num_streams_processed]
              .renderer = loopback_renderers.back().get();
          if (params_.audio.enabled && params_.audio.sync_video)
            video_receive_configs_[num_streams_processed + selected_stream_id]
                .sync_group = kSyncGroup;
        }
        num_streams_processed += num_streams;
      }
      CreateFlexfecStreams();
      CreateVideoStreams();

      CreateCapturers();
      for (size_t video_idx = 0; video_idx < num_video_streams_; ++video_idx) {
        video_send_streams_[video_idx]->SetSource(
            video_capturers_[video_idx].get(), degradation_preference_);
      }
    }

    if (params_.audio.enabled) {
      SetupAudio(send_transport.get(), &audio_receive_stream);
    }

    for (VideoReceiveStream* receive_stream : video_receive_streams_)
      StartEncodedFrameLogs(receive_stream);
    StartEncodedFrameLogs(video_send_streams_[0]);

    // Start sending and receiving video.
    if (params_.video[0].enabled) {
      for (VideoReceiveStream* video_receive_stream : video_receive_streams_)
        video_receive_stream->Start();
      for (VideoSendStream* video_send_stream : video_send_streams_)
        video_send_stream->Start();
      for (auto& video_capturer : video_capturers_)
        video_capturer->Start();
    }

    if (params_.audio.enabled) {
      // Start receiving audio.
      audio_receive_stream->Start();

      // Start sending audio.
      audio_send_stream_->Start();
    }
  });

  test::PressEnterToContinue();

  task_queue_.SendTask([&]() {
    if (params_.audio.enabled) {
      // Stop sending audio.
      audio_send_stream_->Stop();

      // Stop receiving audio.
      audio_receive_stream->Stop();
    }

    // Stop receiving and sending video.
    if (params_.video[0].enabled) {
      for (auto& video_capturer : video_capturers_)
        video_capturer->Stop();
      for (VideoSendStream* video_send_stream : video_send_streams_)
        video_send_stream->Stop();
      DestroyStreams();
    }

    video_capturers_.clear();
    send_transport.reset();
    recv_transport.reset();

    local_preview.reset();
    loopback_renderers.clear();

    DestroyCalls();
  });
}

void VideoQualityTest::StartEncodedFrameLogs(VideoSendStream* stream) {
  if (!params_.logging.encoded_frame_base_path.empty()) {
    std::ostringstream str;
    str << send_logs_++;
    std::string prefix =
        params_.logging.encoded_frame_base_path + "." + str.str() + ".send.";
    stream->EnableEncodedFrameRecording(
        std::vector<rtc::PlatformFile>(
            {rtc::CreatePlatformFile(prefix + "1.ivf"),
             rtc::CreatePlatformFile(prefix + "2.ivf"),
             rtc::CreatePlatformFile(prefix + "3.ivf")}),
        100000000);
  }
}

void VideoQualityTest::StartEncodedFrameLogs(VideoReceiveStream* stream) {
  if (!params_.logging.encoded_frame_base_path.empty()) {
    std::ostringstream str;
    str << receive_logs_++;
    std::string path =
        params_.logging.encoded_frame_base_path + "." + str.str() + ".recv.ivf";
    stream->EnableEncodedFrameRecording(rtc::CreatePlatformFile(path),
                                        100000000);
  }
}
}  // namespace webrtc
