/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/video/video_quality_test.h"

#include <stdio.h>
#include <algorithm>
#include <deque>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/event.h"
#include "webrtc/base/format_macros.h"
#include "webrtc/base/optional.h"
#include "webrtc/base/platform_file.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/call.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/include/cpu_info.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/layer_filtering_transport.h"
#include "webrtc/test/run_loop.h"
#include "webrtc/test/statistics.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/vcm_capturer.h"
#include "webrtc/test/video_renderer.h"
#include "webrtc/voice_engine/include/voe_base.h"

namespace {

constexpr int kSendStatsPollingIntervalMs = 1000;
constexpr int kPayloadTypeH264 = 122;
constexpr int kPayloadTypeVP8 = 123;
constexpr int kPayloadTypeVP9 = 124;
constexpr size_t kMaxComparisons = 10;
constexpr char kSyncGroup[] = "av_sync";
constexpr int kOpusMinBitrateBps = 6000;
constexpr int kOpusBitrateFbBps = 32000;

struct VoiceEngineState {
  VoiceEngineState()
      : voice_engine(nullptr),
        base(nullptr),
        send_channel_id(-1),
        receive_channel_id(-1) {}

  webrtc::VoiceEngine* voice_engine;
  webrtc::VoEBase* base;
  int send_channel_id;
  int receive_channel_id;
};

void CreateVoiceEngine(VoiceEngineState* voe,
                       rtc::scoped_refptr<webrtc::AudioDecoderFactory>
                           decoder_factory) {
  voe->voice_engine = webrtc::VoiceEngine::Create();
  voe->base = webrtc::VoEBase::GetInterface(voe->voice_engine);
  EXPECT_EQ(0, voe->base->Init(nullptr, nullptr, decoder_factory));
  webrtc::VoEBase::ChannelConfig config;
  config.enable_voice_pacing = true;
  voe->send_channel_id = voe->base->CreateChannel(config);
  EXPECT_GE(voe->send_channel_id, 0);
  voe->receive_channel_id = voe->base->CreateChannel();
  EXPECT_GE(voe->receive_channel_id, 0);
}

void DestroyVoiceEngine(VoiceEngineState* voe) {
  voe->base->DeleteChannel(voe->send_channel_id);
  voe->send_channel_id = -1;
  voe->base->DeleteChannel(voe->receive_channel_id);
  voe->receive_channel_id = -1;
  voe->base->Release();
  voe->base = nullptr;

  webrtc::VoiceEngine::Delete(voe->voice_engine);
  voe->voice_engine = nullptr;
}

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
    return streams_;
  }

  std::vector<webrtc::VideoStream> streams_;
};

}  // namespace

namespace webrtc {

class VideoAnalyzer : public PacketReceiver,
                      public Transport,
                      public rtc::VideoSinkInterface<VideoFrame>,
                      public EncodedFrameObserver {
 public:
  VideoAnalyzer(test::LayerFilteringTransport* transport,
                const std::string& test_label,
                double avg_psnr_threshold,
                double avg_ssim_threshold,
                int duration_frames,
                FILE* graph_data_output_file,
                const std::string& graph_title,
                uint32_t ssrc_to_analyze)
      : transport_(transport),
        receiver_(nullptr),
        send_stream_(nullptr),
        captured_frame_forwarder_(this),
        test_label_(test_label),
        graph_data_output_file_(graph_data_output_file),
        graph_title_(graph_title),
        ssrc_to_analyze_(ssrc_to_analyze),
        pre_encode_proxy_(this),
        encode_timing_proxy_(this),
        frames_to_process_(duration_frames),
        frames_recorded_(0),
        frames_processed_(0),
        dropped_frames_(0),
        dropped_frames_before_first_encode_(0),
        dropped_frames_before_rendering_(0),
        last_render_time_(0),
        rtp_timestamp_delta_(0),
        avg_psnr_threshold_(avg_psnr_threshold),
        avg_ssim_threshold_(avg_ssim_threshold),
        stats_polling_thread_(&PollStatsThread, this, "StatsPoller"),
        comparison_available_event_(false, false),
        done_(true, false) {
    // Create thread pool for CPU-expensive PSNR/SSIM calculations.

    // Try to use about as many threads as cores, but leave kMinCoresLeft alone,
    // so that we don't accidentally starve "real" worker threads (codec etc).
    // Also, don't allocate more than kMaxComparisonThreads, even if there are
    // spare cores.

    uint32_t num_cores = CpuInfo::DetectNumberOfCores();
    RTC_DCHECK_GE(num_cores, 1u);
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
  }

  ~VideoAnalyzer() {
    for (rtc::PlatformThread* thread : comparison_thread_pool_) {
      thread->Stop();
      delete thread;
    }
  }

  virtual void SetReceiver(PacketReceiver* receiver) { receiver_ = receiver; }

  void SetSendStream(VideoSendStream* stream) {
    rtc::CritScope lock(&crit_);
    RTC_DCHECK(!send_stream_);
    send_stream_ = stream;
  }

  rtc::VideoSinkInterface<VideoFrame>* InputInterface() {
    return &captured_frame_forwarder_;
  }
  rtc::VideoSourceInterface<VideoFrame>* OutputInterface() {
    return &captured_frame_forwarder_;
  }

  DeliveryStatus DeliverPacket(MediaType media_type,
                               const uint8_t* packet,
                               size_t length,
                               const PacketTime& packet_time) override {
    // Ignore timestamps of RTCP packets. They're not synchronized with
    // RTP packet timestamps and so they would confuse wrap_handler_.
    if (RtpHeaderParser::IsRtcp(packet, length)) {
      return receiver_->DeliverPacket(media_type, packet, length, packet_time);
    }

    RtpUtility::RtpHeaderParser parser(packet, length);
    RTPHeader header;
    parser.Parse(&header);
    {
      rtc::CritScope lock(&crit_);
      int64_t timestamp =
          wrap_handler_.Unwrap(header.timestamp - rtp_timestamp_delta_);
      recv_times_[timestamp] =
          Clock::GetRealTimeClock()->CurrentNtpInMilliseconds();
    }

    return receiver_->DeliverPacket(media_type, packet, length, packet_time);
  }

  void MeasuredEncodeTiming(int64_t ntp_time_ms, int encode_time_ms) {
    rtc::CritScope crit(&comparison_lock_);
    samples_encode_time_ms_[ntp_time_ms] = encode_time_ms;
  }

  void PreEncodeOnFrame(const VideoFrame& video_frame) {
    rtc::CritScope lock(&crit_);
    if (!first_send_timestamp_ && rtp_timestamp_delta_ == 0) {
      while (frames_.front().timestamp() != video_frame.timestamp()) {
        ++dropped_frames_before_first_encode_;
        frames_.pop_front();
        RTC_CHECK(!frames_.empty());
      }
      first_send_timestamp_ = rtc::Optional<uint32_t>(video_frame.timestamp());
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

      if (rtp_timestamp_delta_ == 0) {
        rtp_timestamp_delta_ = header.timestamp - *first_send_timestamp_;
        first_send_timestamp_ = rtc::Optional<uint32_t>();
      }
      int64_t timestamp =
          wrap_handler_.Unwrap(header.timestamp - rtp_timestamp_delta_);
      send_times_[timestamp] = current_time;
      if (!transport_->DiscardedLastPacket() &&
          header.ssrc == ssrc_to_analyze_) {
        encoded_frame_sizes_[timestamp] +=
            length - (header.headerLength + header.paddingLength);
      }
    }
    return result;
  }

  bool SendRtcp(const uint8_t* packet, size_t length) override {
    return transport_->SendRtcp(packet, length);
  }

  void EncodedFrameCallback(const EncodedFrame& frame) override {
    rtc::CritScope lock(&comparison_lock_);
    if (frames_recorded_ < frames_to_process_)
      encoded_frame_size_.AddSample(frame.length_);
  }

  void OnFrame(const VideoFrame& video_frame) override {
    int64_t render_time_ms =
        Clock::GetRealTimeClock()->CurrentNtpInMilliseconds();

    rtc::CritScope lock(&crit_);
    int64_t send_timestamp =
        wrap_handler_.Unwrap(video_frame.timestamp() - rtp_timestamp_delta_);

    while (wrap_handler_.Unwrap(frames_.front().timestamp()) < send_timestamp) {
      if (!last_rendered_frame_) {
        // No previous frame rendered, this one was dropped after sending but
        // before rendering.
        ++dropped_frames_before_rendering_;
        frames_.pop_front();
        RTC_CHECK(!frames_.empty());
        continue;
      }
      AddFrameComparison(frames_.front(), *last_rendered_frame_, true,
                         render_time_ms);
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

    last_rendered_frame_ = rtc::Optional<VideoFrame>(video_frame);
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

  test::LayerFilteringTransport* const transport_;
  PacketReceiver* receiver_;

 private:
  struct FrameComparison {
    FrameComparison()
        : dropped(false),
          send_time_ms(0),
          recv_time_ms(0),
          render_time_ms(0),
          encoded_frame_size(0) {}

    FrameComparison(const VideoFrame& reference,
                    const VideoFrame& render,
                    bool dropped,
                    int64_t send_time_ms,
                    int64_t recv_time_ms,
                    int64_t render_time_ms,
                    size_t encoded_frame_size)
        : reference(reference),
          render(render),
          dropped(dropped),
          send_time_ms(send_time_ms),
          recv_time_ms(recv_time_ms),
          render_time_ms(render_time_ms),
          encoded_frame_size(encoded_frame_size) {}

    VideoFrame reference;
    VideoFrame render;
    bool dropped;
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

    void OnEncodeTiming(int64_t ntp_time_ms, int encode_time_ms) override {
      parent_->MeasuredEncodeTiming(ntp_time_ms, encode_time_ms);
    }
    void EncodedFrameCallback(const EncodedFrame& frame) override {}

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

  void AddFrameComparison(const VideoFrame& reference,
                          const VideoFrame& render,
                          bool dropped,
                          int64_t render_time_ms)
      EXCLUSIVE_LOCKS_REQUIRED(crit_) {
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

    VideoFrame reference_copy;
    VideoFrame render_copy;

    rtc::CritScope crit(&comparison_lock_);
    if (comparisons_.size() < kMaxComparisons) {
      reference_copy = reference;
      render_copy = render;
    } else {
      // Copy the time to ensure that delay calculations can still be made.
      reference_copy.set_ntp_time_ms(reference.ntp_time_ms());
      render_copy.set_ntp_time_ms(render.ntp_time_ms());
    }
    comparisons_.push_back(FrameComparison(reference_copy, render_copy, dropped,
                                           send_time_ms, recv_time_ms,
                                           render_time_ms, encoded_size));
    comparison_available_event_.Set();
  }

  static bool PollStatsThread(void* obj) {
    return static_cast<VideoAnalyzer*>(obj)->PollStats();
  }

  bool PollStats() {
    if (done_.Wait(kSendStatsPollingIntervalMs))
      return false;

    VideoSendStream::Stats stats = send_stream_->GetStats();

    rtc::CritScope crit(&comparison_lock_);
    // It's not certain that we yet have estimates for any of these stats. Check
    // that they are positive before mixing them in.
    if (stats.encode_frame_rate > 0)
      encode_frame_rate_.AddSample(stats.encode_frame_rate);
    if (stats.avg_encode_time_ms > 0)
      encode_time_ms.AddSample(stats.avg_encode_time_ms);
    if (stats.encode_usage_percent > 0)
      encode_usage_percent.AddSample(stats.encode_usage_percent);
    if (stats.media_bitrate_bps > 0)
      media_bitrate_bps.AddSample(stats.media_bitrate_bps);

    return true;
  }

  static bool FrameComparisonThread(void* obj) {
    return static_cast<VideoAnalyzer*>(obj)->CompareFrames();
  }

  bool CompareFrames() {
    if (AllFramesRecorded())
      return false;

    VideoFrame reference;
    VideoFrame render;
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

    PerformFrameComparison(comparison);

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
    rtc::CritScope crit(&comparison_lock_);
    PrintResult("psnr", psnr_, " dB");
    PrintResult("ssim", ssim_, " score");
    PrintResult("sender_time", sender_time_, " ms");
    PrintResult("receiver_time", receiver_time_, " ms");
    PrintResult("total_delay_incl_network", end_to_end_, " ms");
    PrintResult("time_between_rendered_frames", rendered_delta_, " ms");
    PrintResult("encoded_frame_size", encoded_frame_size_, " bytes");
    PrintResult("encode_frame_rate", encode_frame_rate_, " fps");
    PrintResult("encode_time", encode_time_ms, " ms");
    PrintResult("encode_usage_percent", encode_usage_percent, " percent");
    PrintResult("media_bitrate", media_bitrate_bps, " bps");

    printf("RESULT dropped_frames: %s = %d frames\n", test_label_.c_str(),
           dropped_frames_);
    printf("RESULT dropped_frames_before_first_encode: %s = %d frames\n",
           test_label_.c_str(), dropped_frames_before_first_encode_);
    printf("RESULT dropped_frames_before_rendering: %s = %d frames\n",
           test_label_.c_str(), dropped_frames_before_rendering_);

    EXPECT_GT(psnr_.Mean(), avg_psnr_threshold_);
    EXPECT_GT(ssim_.Mean(), avg_ssim_threshold_);
  }

  void PerformFrameComparison(const FrameComparison& comparison) {
    // Perform expensive psnr and ssim calculations while not holding lock.
    double psnr = -1.0;
    double ssim = -1.0;
    if (!comparison.reference.IsZeroSize()) {
      psnr = I420PSNR(&comparison.reference, &comparison.render);
      ssim = I420SSIM(&comparison.reference, &comparison.render);
    }

    int64_t input_time_ms = comparison.reference.ntp_time_ms();

    rtc::CritScope crit(&comparison_lock_);
    if (graph_data_output_file_) {
      samples_.push_back(
          Sample(comparison.dropped, input_time_ms, comparison.send_time_ms,
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

    sender_time_.AddSample(comparison.send_time_ms - input_time_ms);
    receiver_time_.AddSample(comparison.render_time_ms -
                             comparison.recv_time_ms);
    end_to_end_.AddSample(comparison.render_time_ms - input_time_ms);
    encoded_frame_size_.AddSample(comparison.encoded_frame_size);
  }

  void PrintResult(const char* result_type,
                   test::Statistics stats,
                   const char* unit) {
    printf("RESULT %s: %s = {%f, %f}%s\n",
           result_type,
           test_label_.c_str(),
           stats.Mean(),
           stats.StandardDeviation(),
           unit);
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
    int missing_encode_time_samples = 0;
    for (const Sample& sample : samples_) {
      auto it = samples_encode_time_ms_.find(sample.input_time_ms);
      int encode_time_ms;
      if (it != samples_encode_time_ms_.end()) {
        encode_time_ms = it->second;
      } else {
        ++missing_encode_time_samples;
        encode_time_ms = -1;
      }
      fprintf(out, "%d %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRIuS
                   " %lf %lf %d\n",
              sample.dropped, sample.input_time_ms, sample.send_time_ms,
              sample.recv_time_ms, sample.render_time_ms,
              sample.encoded_frame_size, sample.psnr, sample.ssim,
              encode_time_ms);
    }
    if (missing_encode_time_samples) {
      fprintf(stderr,
              "Warning: Missing encode_time_ms samples for %d frame(s).\n",
              missing_encode_time_samples);
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
    explicit CapturedFrameForwarder(VideoAnalyzer* analyzer)
        : analyzer_(analyzer), send_stream_input_(nullptr) {}

   private:
    void OnFrame(const VideoFrame& video_frame) override {
      VideoFrame copy = video_frame;
      copy.set_timestamp(copy.ntp_time_ms() * 90);

      analyzer_->AddCapturedFrameForComparison(video_frame);
      rtc::CritScope lock(&crit_);
      if (send_stream_input_)
        send_stream_input_->OnFrame(video_frame);
    }

    // Called when |send_stream_.SetSource()| is called.
    void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                         const rtc::VideoSinkWants& wants) override {
      rtc::CritScope lock(&crit_);
      RTC_DCHECK(!send_stream_input_ || send_stream_input_ == sink);
      send_stream_input_ = sink;
    }

    // Called by |send_stream_| when |send_stream_.SetSource()| is called.
    void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) override {
      rtc::CritScope lock(&crit_);
      RTC_DCHECK(sink == send_stream_input_);
      send_stream_input_ = nullptr;
    }

    VideoAnalyzer* const analyzer_;
    rtc::CriticalSection crit_;
    rtc::VideoSinkInterface<VideoFrame>* send_stream_input_ GUARDED_BY(crit_);
  };

  void AddCapturedFrameForComparison(const VideoFrame& video_frame) {
    rtc::CritScope lock(&crit_);
    RTC_DCHECK_EQ(0u, video_frame.timestamp());
    // Frames from the capturer does not have a rtp timestamp. Create one so it
    // can be used for comparison.
    VideoFrame copy = video_frame;
    copy.set_timestamp(copy.ntp_time_ms() * 90);
    frames_.push_back(copy);
  }

  VideoSendStream* send_stream_;
  CapturedFrameForwarder captured_frame_forwarder_;
  const std::string test_label_;
  FILE* const graph_data_output_file_;
  const std::string graph_title_;
  const uint32_t ssrc_to_analyze_;
  PreEncodeProxy pre_encode_proxy_;
  OnEncodeTimingProxy encode_timing_proxy_;
  std::vector<Sample> samples_ GUARDED_BY(comparison_lock_);
  std::map<int64_t, int> samples_encode_time_ms_ GUARDED_BY(comparison_lock_);
  test::Statistics sender_time_ GUARDED_BY(comparison_lock_);
  test::Statistics receiver_time_ GUARDED_BY(comparison_lock_);
  test::Statistics psnr_ GUARDED_BY(comparison_lock_);
  test::Statistics ssim_ GUARDED_BY(comparison_lock_);
  test::Statistics end_to_end_ GUARDED_BY(comparison_lock_);
  test::Statistics rendered_delta_ GUARDED_BY(comparison_lock_);
  test::Statistics encoded_frame_size_ GUARDED_BY(comparison_lock_);
  test::Statistics encode_frame_rate_ GUARDED_BY(comparison_lock_);
  test::Statistics encode_time_ms GUARDED_BY(comparison_lock_);
  test::Statistics encode_usage_percent GUARDED_BY(comparison_lock_);
  test::Statistics media_bitrate_bps GUARDED_BY(comparison_lock_);

  const int frames_to_process_;
  int frames_recorded_;
  int frames_processed_;
  int dropped_frames_;
  int dropped_frames_before_first_encode_;
  int dropped_frames_before_rendering_;
  int64_t last_render_time_;
  uint32_t rtp_timestamp_delta_;

  rtc::CriticalSection crit_;
  std::deque<VideoFrame> frames_ GUARDED_BY(crit_);
  rtc::Optional<VideoFrame> last_rendered_frame_ GUARDED_BY(crit_);
  rtc::TimestampWrapAroundHandler wrap_handler_ GUARDED_BY(crit_);
  std::map<int64_t, int64_t> send_times_ GUARDED_BY(crit_);
  std::map<int64_t, int64_t> recv_times_ GUARDED_BY(crit_);
  std::map<int64_t, size_t> encoded_frame_sizes_ GUARDED_BY(crit_);
  rtc::Optional<uint32_t> first_send_timestamp_ GUARDED_BY(crit_);
  const double avg_psnr_threshold_;
  const double avg_ssim_threshold_;

  rtc::CriticalSection comparison_lock_;
  std::vector<rtc::PlatformThread*> comparison_thread_pool_;
  rtc::PlatformThread stats_polling_thread_;
  rtc::Event comparison_available_event_;
  std::deque<FrameComparison> comparisons_ GUARDED_BY(comparison_lock_);
  rtc::Event done_;
};

VideoQualityTest::VideoQualityTest()
    : clock_(Clock::GetRealTimeClock()), receive_logs_(0), send_logs_(0) {}

VideoQualityTest::Params::Params()
    : call({false, Call::Config::BitrateConfig()}),
      video({false, 640, 480, 30, 50, 800, 800, false, "VP8", 1, -1, 0, false,
             "", ""}),
      audio({false, false}),
      screenshare({false, 10, 0}),
      analyzer({"", 0.0, 0.0, 0, "", ""}),
      pipe(),
      logs(false),
      ss({std::vector<VideoStream>(), 0, 0, -1, std::vector<SpatialLayer>()}) {}

VideoQualityTest::Params::~Params() = default;

void VideoQualityTest::TestBody() {}

std::string VideoQualityTest::GenerateGraphTitle() const {
  std::stringstream ss;
  ss << params_.video.codec;
  ss << " (" << params_.video.target_bitrate_bps / 1000 << "kbps";
  ss << ", " << params_.video.fps << " FPS";
  if (params_.screenshare.scroll_duration)
    ss << ", " << params_.screenshare.scroll_duration << "s scroll";
  if (params_.ss.streams.size() > 1)
    ss << ", Stream #" << params_.ss.selected_stream;
  if (params_.ss.num_spatial_layers > 1)
    ss << ", Layer #" << params_.ss.selected_sl;
  ss << ")";
  return ss.str();
}

void VideoQualityTest::CheckParams() {
  // Add a default stream in none specified.
  if (params_.ss.streams.empty())
    params_.ss.streams.push_back(VideoQualityTest::DefaultVideoStream(params_));
  if (params_.ss.num_spatial_layers == 0)
    params_.ss.num_spatial_layers = 1;

  if (params_.pipe.loss_percent != 0 ||
      params_.pipe.queue_length_packets != 0) {
    // Since LayerFilteringTransport changes the sequence numbers, we can't
    // use that feature with pack loss, since the NACK request would end up
    // retransmitting the wrong packets.
    RTC_CHECK(params_.ss.selected_sl == -1 ||
              params_.ss.selected_sl == params_.ss.num_spatial_layers - 1);
    RTC_CHECK(params_.video.selected_tl == -1 ||
              params_.video.selected_tl ==
                  params_.video.num_temporal_layers - 1);
  }

  // TODO(ivica): Should max_bitrate_bps == -1 represent inf max bitrate, as it
  // does in some parts of the code?
  RTC_CHECK_GE(params_.video.max_bitrate_bps, params_.video.target_bitrate_bps);
  RTC_CHECK_GE(params_.video.target_bitrate_bps, params_.video.min_bitrate_bps);
  RTC_CHECK_LT(params_.video.selected_tl, params_.video.num_temporal_layers);
  RTC_CHECK_LT(params_.ss.selected_stream, params_.ss.streams.size());
  for (const VideoStream& stream : params_.ss.streams) {
    RTC_CHECK_GE(stream.min_bitrate_bps, 0);
    RTC_CHECK_GE(stream.target_bitrate_bps, stream.min_bitrate_bps);
    RTC_CHECK_GE(stream.max_bitrate_bps, stream.target_bitrate_bps);
    RTC_CHECK_EQ(static_cast<int>(stream.temporal_layer_thresholds_bps.size()),
                 params_.video.num_temporal_layers - 1);
  }
  // TODO(ivica): Should we check if the sum of all streams/layers is equal to
  // the total bitrate? We anyway have to update them in the case bitrate
  // estimator changes the total bitrates.
  RTC_CHECK_GE(params_.ss.num_spatial_layers, 1);
  RTC_CHECK_LE(params_.ss.selected_sl, params_.ss.num_spatial_layers);
  RTC_CHECK(params_.ss.spatial_layers.empty() ||
            params_.ss.spatial_layers.size() ==
                static_cast<size_t>(params_.ss.num_spatial_layers));
  if (params_.video.codec == "VP8") {
    RTC_CHECK_EQ(params_.ss.num_spatial_layers, 1);
  } else if (params_.video.codec == "VP9") {
    RTC_CHECK_EQ(params_.ss.streams.size(), 1u);
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
VideoStream VideoQualityTest::DefaultVideoStream(const Params& params) {
  VideoStream stream;
  stream.width = params.video.width;
  stream.height = params.video.height;
  stream.max_framerate = params.video.fps;
  stream.min_bitrate_bps = params.video.min_bitrate_bps;
  stream.target_bitrate_bps = params.video.target_bitrate_bps;
  stream.max_bitrate_bps = params.video.max_bitrate_bps;
  stream.max_qp = 52;
  if (params.video.num_temporal_layers == 2)
    stream.temporal_layer_thresholds_bps.push_back(stream.target_bitrate_bps);
  return stream;
}

// Static.
void VideoQualityTest::FillScalabilitySettings(
    Params* params,
    const std::vector<std::string>& stream_descriptors,
    size_t selected_stream,
    int num_spatial_layers,
    int selected_sl,
    const std::vector<std::string>& sl_descriptors) {
  // Read VideoStream and SpatialLayer elements from a list of comma separated
  // lists. To use a default value for an element, use -1 or leave empty.
  // Validity checks performed in CheckParams.

  RTC_CHECK(params->ss.streams.empty());
  for (auto descriptor : stream_descriptors) {
    if (descriptor.empty())
      continue;
    VideoStream stream = VideoQualityTest::DefaultVideoStream(*params);
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
      RTC_CHECK_LE(params->video.num_temporal_layers, 2);
    }
    params->ss.streams.push_back(stream);
  }
  params->ss.selected_stream = selected_stream;

  params->ss.num_spatial_layers = num_spatial_layers ? num_spatial_layers : 1;
  params->ss.selected_sl = selected_sl;
  RTC_CHECK(params->ss.spatial_layers.empty());
  for (auto descriptor : sl_descriptors) {
    if (descriptor.empty())
      continue;
    std::vector<int> v = VideoQualityTest::ParseCSV(descriptor);
    RTC_CHECK_GT(v[2], 0);

    SpatialLayer layer;
    layer.scaling_factor_num = v[0] == -1 ? 1 : v[0];
    layer.scaling_factor_den = v[1] == -1 ? 1 : v[1];
    layer.target_bitrate_bps = v[2];
    params->ss.spatial_layers.push_back(layer);
  }
}

void VideoQualityTest::SetupVideo(Transport* send_transport,
                                  Transport* recv_transport) {
  if (params_.logs)
    trace_to_stderr_.reset(new test::TraceToStderr);

  size_t num_streams = params_.ss.streams.size();
  CreateSendConfig(num_streams, 0, 0, send_transport);

  int payload_type;
  if (params_.video.codec == "H264") {
    video_encoder_.reset(VideoEncoder::Create(VideoEncoder::kH264));
    payload_type = kPayloadTypeH264;
  } else if (params_.video.codec == "VP8") {
    video_encoder_.reset(VideoEncoder::Create(VideoEncoder::kVp8));
    payload_type = kPayloadTypeVP8;
  } else if (params_.video.codec == "VP9") {
    video_encoder_.reset(VideoEncoder::Create(VideoEncoder::kVp9));
    payload_type = kPayloadTypeVP9;
  } else {
    RTC_NOTREACHED() << "Codec not supported!";
    return;
  }
  video_send_config_.encoder_settings.encoder = video_encoder_.get();
  video_send_config_.encoder_settings.payload_name = params_.video.codec;
  video_send_config_.encoder_settings.payload_type = payload_type;
  video_send_config_.rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
  video_send_config_.rtp.rtx.payload_type = kSendRtxPayloadType;
  for (size_t i = 0; i < num_streams; ++i)
    video_send_config_.rtp.rtx.ssrcs.push_back(kSendRtxSsrcs[i]);

  video_send_config_.rtp.extensions.clear();
  if (params_.call.send_side_bwe) {
    video_send_config_.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kTransportSequenceNumberUri,
                     test::kTransportSequenceNumberExtensionId));
  } else {
    video_send_config_.rtp.extensions.push_back(RtpExtension(
        RtpExtension::kAbsSendTimeUri, test::kAbsSendTimeExtensionId));
  }

  video_encoder_config_.min_transmit_bitrate_bps =
      params_.video.min_transmit_bps;

  video_encoder_config_.number_of_streams = params_.ss.streams.size();
  video_encoder_config_.max_bitrate_bps = 0;
  for (size_t i = 0; i < params_.ss.streams.size(); ++i) {
    video_encoder_config_.max_bitrate_bps +=
        params_.ss.streams[i].max_bitrate_bps;
  }
  video_encoder_config_.video_stream_factory =
      new rtc::RefCountedObject<VideoStreamFactory>(params_.ss.streams);

  video_encoder_config_.spatial_layers = params_.ss.spatial_layers;

  CreateMatchingReceiveConfigs(recv_transport);

  for (size_t i = 0; i < num_streams; ++i) {
    video_receive_configs_[i].rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
    video_receive_configs_[i].rtp.rtx[payload_type].ssrc = kSendRtxSsrcs[i];
    video_receive_configs_[i].rtp.rtx[payload_type].payload_type =
        kSendRtxPayloadType;
    video_receive_configs_[i].rtp.transport_cc = params_.call.send_side_bwe;
  }
}

void VideoQualityTest::SetupScreenshare() {
  RTC_CHECK(params_.screenshare.enabled);

  // Fill out codec settings.
  video_encoder_config_.content_type = VideoEncoderConfig::ContentType::kScreen;
  if (params_.video.codec == "VP8") {
    VideoCodecVP8 vp8_settings = VideoEncoder::GetDefaultVp8Settings();
    vp8_settings.denoisingOn = false;
    vp8_settings.frameDroppingOn = false;
    vp8_settings.numberOfTemporalLayers =
        static_cast<unsigned char>(params_.video.num_temporal_layers);
    video_encoder_config_.encoder_specific_settings = new rtc::RefCountedObject<
        VideoEncoderConfig::Vp8EncoderSpecificSettings>(vp8_settings);
  } else if (params_.video.codec == "VP9") {
    VideoCodecVP9 vp9_settings = VideoEncoder::GetDefaultVp9Settings();
    vp9_settings.denoisingOn = false;
    vp9_settings.frameDroppingOn = false;
    vp9_settings.numberOfTemporalLayers =
        static_cast<unsigned char>(params_.video.num_temporal_layers);
    vp9_settings.numberOfSpatialLayers =
        static_cast<unsigned char>(params_.ss.num_spatial_layers);
    video_encoder_config_.encoder_specific_settings = new rtc::RefCountedObject<
        VideoEncoderConfig::Vp9EncoderSpecificSettings>(vp9_settings);
  }

  // Setup frame generator.
  const size_t kWidth = 1850;
  const size_t kHeight = 1110;
  std::vector<std::string> slides;
  slides.push_back(test::ResourcePath("web_screenshot_1850_1110", "yuv"));
  slides.push_back(test::ResourcePath("presentation_1850_1110", "yuv"));
  slides.push_back(test::ResourcePath("photo_1850_1110", "yuv"));
  slides.push_back(test::ResourcePath("difficult_photo_1850_1110", "yuv"));

  if (params_.screenshare.scroll_duration == 0) {
    // Cycle image every slide_change_interval seconds.
    frame_generator_.reset(test::FrameGenerator::CreateFromYuvFile(
        slides, kWidth, kHeight,
        params_.screenshare.slide_change_interval * params_.video.fps));
  } else {
    RTC_CHECK_LE(params_.video.width, kWidth);
    RTC_CHECK_LE(params_.video.height, kHeight);
    RTC_CHECK_GT(params_.screenshare.slide_change_interval, 0);
    const int kPauseDurationMs = (params_.screenshare.slide_change_interval -
                                  params_.screenshare.scroll_duration) *
                                 1000;
    RTC_CHECK_LE(params_.screenshare.scroll_duration,
                 params_.screenshare.slide_change_interval);

    frame_generator_.reset(
        test::FrameGenerator::CreateScrollingInputFromYuvFiles(
            clock_, slides, kWidth, kHeight, params_.video.width,
            params_.video.height, params_.screenshare.scroll_duration * 1000,
            kPauseDurationMs));
  }
}

void VideoQualityTest::CreateCapturer() {
  if (params_.screenshare.enabled) {
    test::FrameGeneratorCapturer* frame_generator_capturer =
        new test::FrameGeneratorCapturer(clock_, frame_generator_.release(),
                                         params_.video.fps);
    EXPECT_TRUE(frame_generator_capturer->Init());
    video_capturer_.reset(frame_generator_capturer);
  } else {
    if (params_.video.clip_name.empty()) {
      video_capturer_.reset(test::VcmCapturer::Create(
          params_.video.width, params_.video.height, params_.video.fps));
    } else {
      video_capturer_.reset(test::FrameGeneratorCapturer::CreateFromYuvFile(
          test::ResourcePath(params_.video.clip_name, "yuv"),
          params_.video.width, params_.video.height, params_.video.fps,
          clock_));
      ASSERT_TRUE(video_capturer_) << "Could not create capturer for "
                                   << params_.video.clip_name
                                   << ".yuv. Is this resource file present?";
    }
  }
}

void VideoQualityTest::RunWithAnalyzer(const Params& params) {
  params_ = params;

  RTC_CHECK(!params_.audio.enabled);
  // TODO(ivica): Merge with RunWithRenderer and use a flag / argument to
  // differentiate between the analyzer and the renderer case.
  CheckParams();

  FILE* graph_data_output_file = nullptr;
  if (!params_.analyzer.graph_data_output_filename.empty()) {
    graph_data_output_file =
        fopen(params_.analyzer.graph_data_output_filename.c_str(), "w");
    RTC_CHECK(graph_data_output_file)
        << "Can't open the file " << params_.analyzer.graph_data_output_filename
        << "!";
  }

  webrtc::RtcEventLogNullImpl event_log;
  Call::Config call_config(&event_log_);
  call_config.bitrate_config = params.call.call_bitrate_config;
  CreateCalls(call_config, call_config);

  test::LayerFilteringTransport send_transport(
      params_.pipe, sender_call_.get(), kPayloadTypeVP8, kPayloadTypeVP9,
      params_.video.selected_tl, params_.ss.selected_sl);
  test::DirectTransport recv_transport(params_.pipe, receiver_call_.get());

  std::string graph_title = params_.analyzer.graph_title;
  if (graph_title.empty())
    graph_title = VideoQualityTest::GenerateGraphTitle();

  // In the case of different resolutions, the functions calculating PSNR and
  // SSIM return -1.0, instead of a positive value as usual. VideoAnalyzer
  // aborts if the average psnr/ssim are below the given threshold, which is
  // 0.0 by default. Setting the thresholds to -1.1 prevents the unnecessary
  // abort.
  VideoStream& selected_stream = params_.ss.streams[params_.ss.selected_stream];
  int selected_sl = params_.ss.selected_sl != -1
                        ? params_.ss.selected_sl
                        : params_.ss.num_spatial_layers - 1;
  bool disable_quality_check =
      selected_stream.width != params_.video.width ||
      selected_stream.height != params_.video.height ||
      (!params_.ss.spatial_layers.empty() &&
       params_.ss.spatial_layers[selected_sl].scaling_factor_num !=
           params_.ss.spatial_layers[selected_sl].scaling_factor_den);
  if (disable_quality_check) {
    fprintf(stderr,
            "Warning: Calculating PSNR and SSIM for downsized resolution "
            "not implemented yet! Skipping PSNR and SSIM calculations!");
  }

  VideoAnalyzer analyzer(
      &send_transport, params_.analyzer.test_label,
      disable_quality_check ? -1.1 : params_.analyzer.avg_psnr_threshold,
      disable_quality_check ? -1.1 : params_.analyzer.avg_ssim_threshold,
      params_.analyzer.test_durations_secs * params_.video.fps,
      graph_data_output_file, graph_title,
      kVideoSendSsrcs[params_.ss.selected_stream]);

  analyzer.SetReceiver(receiver_call_->Receiver());
  send_transport.SetReceiver(&analyzer);
  recv_transport.SetReceiver(sender_call_->Receiver());

  SetupVideo(&analyzer, &recv_transport);
  video_receive_configs_[params_.ss.selected_stream].renderer = &analyzer;
  video_send_config_.pre_encode_callback = analyzer.pre_encode_proxy();
  for (auto& config : video_receive_configs_)
    config.pre_decode_callback = &analyzer;
  RTC_DCHECK(!video_send_config_.post_encode_callback);
  video_send_config_.post_encode_callback = analyzer.encode_timing_proxy();

  if (params_.screenshare.enabled)
    SetupScreenshare();

  CreateVideoStreams();
  analyzer.SetSendStream(video_send_stream_);
  video_send_stream_->SetSource(
      analyzer.OutputInterface(),
      VideoSendStream::DegradationPreference::kBalanced);

  CreateCapturer();
  rtc::VideoSinkWants wants;
  video_capturer_->AddOrUpdateSink(analyzer.InputInterface(), wants);

  StartEncodedFrameLogs(video_send_stream_);
  StartEncodedFrameLogs(video_receive_streams_[0]);
  video_send_stream_->Start();
  for (VideoReceiveStream* receive_stream : video_receive_streams_)
    receive_stream->Start();
  video_capturer_->Start();

  analyzer.Wait();

  send_transport.StopSending();
  recv_transport.StopSending();

  video_capturer_->Stop();
  for (VideoReceiveStream* receive_stream : video_receive_streams_)
    receive_stream->Stop();
  video_send_stream_->Stop();

  DestroyStreams();

  if (graph_data_output_file)
    fclose(graph_data_output_file);
}

void VideoQualityTest::SetupAudio(int send_channel_id,
                                  int receive_channel_id,
                                  Call* call,
                                  Transport* transport,
                                  AudioReceiveStream** audio_receive_stream) {
  audio_send_config_ = AudioSendStream::Config(transport);
  audio_send_config_.voe_channel_id = send_channel_id;
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
  audio_send_config_.send_codec_spec.codec_inst =
      CodecInst{120, "OPUS", 48000, 960, 2, 64000};

  audio_send_stream_ = call->CreateAudioSendStream(audio_send_config_);

  AudioReceiveStream::Config audio_config;
  audio_config.rtp.local_ssrc = kReceiverLocalAudioSsrc;
  audio_config.rtcp_send_transport = transport;
  audio_config.voe_channel_id = receive_channel_id;
  audio_config.rtp.remote_ssrc = audio_send_config_.rtp.ssrc;
  audio_config.rtp.transport_cc = params_.call.send_side_bwe;
  audio_config.rtp.extensions = audio_send_config_.rtp.extensions;
  audio_config.decoder_factory = decoder_factory_;
  if (params_.video.enabled && params_.audio.sync_video)
    audio_config.sync_group = kSyncGroup;

  *audio_receive_stream = call->CreateAudioReceiveStream(audio_config);
}

void VideoQualityTest::RunWithRenderers(const Params& params) {
  params_ = params;
  CheckParams();

  // TODO(ivica): Remove bitrate_config and use the default Call::Config(), to
  // match the full stack tests.
  webrtc::RtcEventLogNullImpl event_log;
  Call::Config call_config(&event_log_);
  call_config.bitrate_config = params_.call.call_bitrate_config;

  ::VoiceEngineState voe;
  if (params_.audio.enabled) {
    CreateVoiceEngine(&voe, decoder_factory_);
    AudioState::Config audio_state_config;
    audio_state_config.voice_engine = voe.voice_engine;
    call_config.audio_state = AudioState::Create(audio_state_config);
  }

  std::unique_ptr<Call> call(Call::Create(call_config));

  // TODO(minyue): consider if this is a good transport even for audio only
  // calls.
  test::LayerFilteringTransport transport(
      params.pipe, call.get(), kPayloadTypeVP8, kPayloadTypeVP9,
      params.video.selected_tl, params_.ss.selected_sl);
  // TODO(ivica): Use two calls to be able to merge with RunWithAnalyzer or at
  // least share as much code as possible. That way this test would also match
  // the full stack tests better.
  transport.SetReceiver(call->Receiver());

  VideoReceiveStream* video_receive_stream = nullptr;
  std::unique_ptr<test::VideoRenderer> local_preview;
  std::unique_ptr<test::VideoRenderer> loopback_video;
  if (params_.video.enabled) {
    // Create video renderers.
    local_preview.reset(test::VideoRenderer::Create(
        "Local Preview", params_.video.width, params_.video.height));

    size_t stream_id = params_.ss.selected_stream;
    std::string title = "Loopback Video";
    if (params_.ss.streams.size() > 1) {
      std::ostringstream s;
      s << stream_id;
      title += " - Stream #" + s.str();
    }

    loopback_video.reset(test::VideoRenderer::Create(
        title.c_str(), params_.ss.streams[stream_id].width,
        params_.ss.streams[stream_id].height));

    SetupVideo(&transport, &transport);

    // TODO(minyue): maybe move the following to SetupVideo() to make code
    // cleaner.  Currently, RunWithRenderers() and RunWithAnalyzer() differ in
    // the way they treat FEC etc., which makes it complicated to put these
    // additional video setup into SetupVideo().
    video_send_config_.pre_encode_callback = local_preview.get();
    video_receive_configs_[stream_id].renderer = loopback_video.get();
    if (params_.audio.enabled && params_.audio.sync_video)
      video_receive_configs_[stream_id].sync_group = kSyncGroup;

    video_send_config_.suspend_below_min_bitrate =
        params_.video.suspend_below_min_bitrate;

    if (params.video.fec) {
      video_send_config_.rtp.ulpfec.red_payload_type = kRedPayloadType;
      video_send_config_.rtp.ulpfec.ulpfec_payload_type = kUlpfecPayloadType;
      video_send_config_.rtp.ulpfec.red_rtx_payload_type = kRtxRedPayloadType;
      video_receive_configs_[stream_id].rtp.ulpfec.red_payload_type =
          kRedPayloadType;
      video_receive_configs_[stream_id].rtp.ulpfec.ulpfec_payload_type =
          kUlpfecPayloadType;
      video_receive_configs_[stream_id].rtp.ulpfec.red_rtx_payload_type =
          kRtxRedPayloadType;
    }

    if (params_.screenshare.enabled)
      SetupScreenshare();

    video_send_stream_ = call->CreateVideoSendStream(
        video_send_config_.Copy(), video_encoder_config_.Copy());
    video_receive_stream = call->CreateVideoReceiveStream(
        video_receive_configs_[stream_id].Copy());
    CreateCapturer();
    video_send_stream_->SetSource(
        video_capturer_.get(),
        VideoSendStream::DegradationPreference::kBalanced);
  }

  AudioReceiveStream* audio_receive_stream = nullptr;
  if (params_.audio.enabled) {
    SetupAudio(voe.send_channel_id, voe.receive_channel_id, call.get(),
               &transport, &audio_receive_stream);
  }

  StartEncodedFrameLogs(video_receive_stream);
  StartEncodedFrameLogs(video_send_stream_);

  // Start sending and receiving video.
  if (params_.video.enabled) {
    video_receive_stream->Start();
    video_send_stream_->Start();
    video_capturer_->Start();
  }

  if (params_.audio.enabled) {
    // Start receiving audio.
    audio_receive_stream->Start();
    EXPECT_EQ(0, voe.base->StartPlayout(voe.receive_channel_id));

    // Start sending audio.
    audio_send_stream_->Start();
    EXPECT_EQ(0, voe.base->StartSend(voe.send_channel_id));
  }

  test::PressEnterToContinue();

  if (params_.audio.enabled) {
    // Stop sending audio.
    EXPECT_EQ(0, voe.base->StopSend(voe.send_channel_id));
    audio_send_stream_->Stop();

    // Stop receiving audio.
    EXPECT_EQ(0, voe.base->StopPlayout(voe.receive_channel_id));
    audio_receive_stream->Stop();
    call->DestroyAudioSendStream(audio_send_stream_);
    call->DestroyAudioReceiveStream(audio_receive_stream);
  }

  // Stop receiving and sending video.
  if (params_.video.enabled) {
    video_capturer_->Stop();
    video_send_stream_->Stop();
    video_receive_stream->Stop();
    call->DestroyVideoReceiveStream(video_receive_stream);
    call->DestroyVideoSendStream(video_send_stream_);
  }

  transport.StopSending();
  if (params_.audio.enabled)
    DestroyVoiceEngine(&voe);
}

void VideoQualityTest::StartEncodedFrameLogs(VideoSendStream* stream) {
  if (!params_.video.encoded_frame_base_path.empty()) {
    std::ostringstream str;
    str << send_logs_++;
    std::string prefix =
        params_.video.encoded_frame_base_path + "." + str.str() + ".send.";
    stream->EnableEncodedFrameRecording(
        std::vector<rtc::PlatformFile>(
            {rtc::CreatePlatformFile(prefix + "1.ivf"),
             rtc::CreatePlatformFile(prefix + "2.ivf"),
             rtc::CreatePlatformFile(prefix + "3.ivf")}),
        10000000);
  }
}
void VideoQualityTest::StartEncodedFrameLogs(VideoReceiveStream* stream) {
  if (!params_.video.encoded_frame_base_path.empty()) {
    std::ostringstream str;
    str << receive_logs_++;
    std::string path =
        params_.video.encoded_frame_base_path + "." + str.str() + ".recv.ivf";
    stream->EnableEncodedFrameRecording(rtc::CreatePlatformFile(path),
                                        10000000);
  }
}

}  // namespace webrtc
