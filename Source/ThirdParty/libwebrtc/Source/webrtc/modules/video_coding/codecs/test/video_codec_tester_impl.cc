/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/video_codec_tester_impl.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "api/task_queue/default_task_queue_factory.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "modules/video_coding/codecs/test/video_codec_analyzer.h"
#include "modules/video_coding/utility/ivf_file_writer.h"
#include "rtc_base/event.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/sleep.h"
#include "test/testsupport/video_frame_writer.h"

namespace webrtc {
namespace test {

namespace {
using RawVideoSource = VideoCodecTester::RawVideoSource;
using CodedVideoSource = VideoCodecTester::CodedVideoSource;
using Decoder = VideoCodecTester::Decoder;
using Encoder = VideoCodecTester::Encoder;
using EncoderSettings = VideoCodecTester::EncoderSettings;
using DecoderSettings = VideoCodecTester::DecoderSettings;
using PacingSettings = VideoCodecTester::PacingSettings;
using PacingMode = PacingSettings::PacingMode;

constexpr Frequency k90kHz = Frequency::Hertz(90000);

// A thread-safe wrapper for video source to be shared with the quality analyzer
// that reads reference frames from a separate thread.
class SyncRawVideoSource : public VideoCodecAnalyzer::ReferenceVideoSource {
 public:
  explicit SyncRawVideoSource(RawVideoSource* video_source)
      : video_source_(video_source) {}

  absl::optional<VideoFrame> PullFrame() {
    MutexLock lock(&mutex_);
    return video_source_->PullFrame();
  }

  VideoFrame GetFrame(uint32_t timestamp_rtp, Resolution resolution) override {
    MutexLock lock(&mutex_);
    return video_source_->GetFrame(timestamp_rtp, resolution);
  }

 protected:
  RawVideoSource* const video_source_ RTC_GUARDED_BY(mutex_);
  Mutex mutex_;
};

// Pacer calculates delay necessary to keep frame encode or decode call spaced
// from the previous calls by the pacing time. `Delay` is expected to be called
// as close as possible to posting frame encode or decode task. This class is
// not thread safe.
class Pacer {
 public:
  explicit Pacer(PacingSettings settings)
      : settings_(settings), delay_(TimeDelta::Zero()) {}
  Timestamp Schedule(Timestamp timestamp) {
    Timestamp now = Timestamp::Micros(rtc::TimeMicros());
    if (settings_.mode == PacingMode::kNoPacing) {
      return now;
    }

    Timestamp scheduled = now;
    if (prev_scheduled_) {
      scheduled = *prev_scheduled_ + PacingTime(timestamp);
      if (scheduled < now) {
        scheduled = now;
      }
    }

    prev_timestamp_ = timestamp;
    prev_scheduled_ = scheduled;
    return scheduled;
  }

 private:
  TimeDelta PacingTime(Timestamp timestamp) {
    if (settings_.mode == PacingMode::kRealTime) {
      return timestamp - *prev_timestamp_;
    }
    RTC_CHECK_EQ(PacingMode::kConstantRate, settings_.mode);
    return 1 / settings_.constant_rate;
  }

  PacingSettings settings_;
  absl::optional<Timestamp> prev_timestamp_;
  absl::optional<Timestamp> prev_scheduled_;
  TimeDelta delay_;
};

// Task queue that keeps the number of queued tasks below a certain limit. If
// the limit is reached, posting of a next task is blocked until execution of a
// previously posted task starts. This class is not thread-safe.
class LimitedTaskQueue {
 public:
  // The codec tester reads frames from video source in the main thread.
  // Encoding and decoding are done in separate threads. If encoding or
  // decoding is slow, the reading may go far ahead and may buffer too many
  // frames in memory. To prevent this we limit the encoding/decoding queue
  // size. When the queue is full, the main thread and, hence, reading frames
  // from video source is blocked until a previously posted encoding/decoding
  // task starts.
  static constexpr int kMaxTaskQueueSize = 3;

  LimitedTaskQueue() : queue_size_(0) {}

  void PostScheduledTask(absl::AnyInvocable<void() &&> task, Timestamp start) {
    ++queue_size_;
    task_queue_.PostTask([this, task = std::move(task), start]() mutable {
      int wait_ms = static_cast<int>(start.ms() - rtc::TimeMillis());
      if (wait_ms > 0) {
        SleepMs(wait_ms);
      }

      std::move(task)();
      --queue_size_;
      task_executed_.Set();
    });

    task_executed_.Reset();
    if (queue_size_ > kMaxTaskQueueSize) {
      task_executed_.Wait(rtc::Event::kForever);
    }
    RTC_CHECK(queue_size_ <= kMaxTaskQueueSize);
  }

  void WaitForPreviouslyPostedTasks() {
    task_queue_.SendTask([] {});
  }

  TaskQueueForTest task_queue_;
  std::atomic_int queue_size_;
  rtc::Event task_executed_;
};

class TesterY4mWriter {
 public:
  explicit TesterY4mWriter(absl::string_view base_path)
      : base_path_(base_path) {}

  ~TesterY4mWriter() {
    task_queue_.SendTask([] {});
  }

  void Write(const VideoFrame& frame, int spatial_idx) {
    task_queue_.PostTask([this, frame, spatial_idx] {
      if (y4m_writers_.find(spatial_idx) == y4m_writers_.end()) {
        std::string file_path =
            base_path_ + "_s" + std::to_string(spatial_idx) + ".y4m";

        Y4mVideoFrameWriterImpl* y4m_writer = new Y4mVideoFrameWriterImpl(
            file_path, frame.width(), frame.height(), /*fps=*/30);
        RTC_CHECK(y4m_writer);

        y4m_writers_[spatial_idx] =
            std::unique_ptr<VideoFrameWriter>(y4m_writer);
      }

      y4m_writers_.at(spatial_idx)->WriteFrame(frame);
    });
  }

 protected:
  std::string base_path_;
  std::map<int, std::unique_ptr<VideoFrameWriter>> y4m_writers_;
  TaskQueueForTest task_queue_;
};

class TesterIvfWriter {
 public:
  explicit TesterIvfWriter(absl::string_view base_path)
      : base_path_(base_path) {}

  ~TesterIvfWriter() {
    task_queue_.SendTask([] {});
  }

  void Write(const EncodedImage& encoded_frame) {
    task_queue_.PostTask([this, encoded_frame] {
      int spatial_idx = encoded_frame.SpatialIndex().value_or(0);
      if (ivf_file_writers_.find(spatial_idx) == ivf_file_writers_.end()) {
        std::string ivf_path =
            base_path_ + "_s" + std::to_string(spatial_idx) + ".ivf";

        FileWrapper ivf_file = FileWrapper::OpenWriteOnly(ivf_path);
        RTC_CHECK(ivf_file.is_open());

        std::unique_ptr<IvfFileWriter> ivf_writer =
            IvfFileWriter::Wrap(std::move(ivf_file), /*byte_limit=*/0);
        RTC_CHECK(ivf_writer);

        ivf_file_writers_[spatial_idx] = std::move(ivf_writer);
      }

      // To play: ffplay -vcodec vp8|vp9|av1|hevc|h264 filename
      ivf_file_writers_.at(spatial_idx)
          ->WriteFrame(encoded_frame, VideoCodecType::kVideoCodecGeneric);
    });
  }

 protected:
  std::string base_path_;
  std::map<int, std::unique_ptr<IvfFileWriter>> ivf_file_writers_;
  TaskQueueForTest task_queue_;
};

class TesterDecoder {
 public:
  TesterDecoder(Decoder* decoder,
                VideoCodecAnalyzer* analyzer,
                const DecoderSettings& settings)
      : decoder_(decoder),
        analyzer_(analyzer),
        settings_(settings),
        pacer_(settings.pacing) {
    RTC_CHECK(analyzer_) << "Analyzer must be provided";

    if (settings.decoder_input_base_path) {
      input_writer_ =
          std::make_unique<TesterIvfWriter>(*settings.decoder_input_base_path);
    }

    if (settings.decoder_output_base_path) {
      output_writer_ =
          std::make_unique<TesterY4mWriter>(*settings.decoder_output_base_path);
    }
  }

  void Initialize() {
    task_queue_.PostScheduledTask([this] { decoder_->Initialize(); },
                                  Timestamp::Zero());
    task_queue_.WaitForPreviouslyPostedTasks();
  }

  void Decode(const EncodedImage& input_frame) {
    Timestamp timestamp =
        Timestamp::Micros((input_frame.Timestamp() / k90kHz).us());

    task_queue_.PostScheduledTask(
        [this, input_frame] {
          analyzer_->StartDecode(input_frame);

          decoder_->Decode(
              input_frame,
              [this, spatial_idx = input_frame.SpatialIndex().value_or(0)](
                  const VideoFrame& output_frame) {
                analyzer_->FinishDecode(output_frame, spatial_idx);

                if (output_writer_) {
                  output_writer_->Write(output_frame, spatial_idx);
                }
              });

          if (input_writer_) {
            input_writer_->Write(input_frame);
          }
        },
        pacer_.Schedule(timestamp));
  }

  void Flush() {
    task_queue_.PostScheduledTask([this] { decoder_->Flush(); },
                                  Timestamp::Zero());
    task_queue_.WaitForPreviouslyPostedTasks();
  }

 protected:
  Decoder* const decoder_;
  VideoCodecAnalyzer* const analyzer_;
  const DecoderSettings& settings_;
  Pacer pacer_;
  LimitedTaskQueue task_queue_;
  std::unique_ptr<TesterIvfWriter> input_writer_;
  std::unique_ptr<TesterY4mWriter> output_writer_;
};

class TesterEncoder {
 public:
  TesterEncoder(Encoder* encoder,
                TesterDecoder* decoder,
                VideoCodecAnalyzer* analyzer,
                const EncoderSettings& settings)
      : encoder_(encoder),
        decoder_(decoder),
        analyzer_(analyzer),
        settings_(settings),
        pacer_(settings.pacing) {
    RTC_CHECK(analyzer_) << "Analyzer must be provided";
    if (settings.encoder_input_base_path) {
      input_writer_ =
          std::make_unique<TesterY4mWriter>(*settings.encoder_input_base_path);
    }

    if (settings.encoder_output_base_path) {
      output_writer_ =
          std::make_unique<TesterIvfWriter>(*settings.encoder_output_base_path);
    }
  }

  void Initialize() {
    task_queue_.PostScheduledTask([this] { encoder_->Initialize(); },
                                  Timestamp::Zero());
    task_queue_.WaitForPreviouslyPostedTasks();
  }

  void Encode(const VideoFrame& input_frame) {
    Timestamp timestamp =
        Timestamp::Micros((input_frame.timestamp() / k90kHz).us());

    task_queue_.PostScheduledTask(
        [this, input_frame] {
          analyzer_->StartEncode(input_frame);
          encoder_->Encode(input_frame,
                           [this](const EncodedImage& encoded_frame) {
                             analyzer_->FinishEncode(encoded_frame);

                             if (decoder_ != nullptr) {
                               decoder_->Decode(encoded_frame);
                             }

                             if (output_writer_ != nullptr) {
                               output_writer_->Write(encoded_frame);
                             }
                           });

          if (input_writer_) {
            input_writer_->Write(input_frame, /*spatial_idx=*/0);
          }
        },
        pacer_.Schedule(timestamp));
  }

  void Flush() {
    task_queue_.PostScheduledTask([this] { encoder_->Flush(); },
                                  Timestamp::Zero());
    task_queue_.WaitForPreviouslyPostedTasks();
  }

 protected:
  Encoder* const encoder_;
  TesterDecoder* const decoder_;
  VideoCodecAnalyzer* const analyzer_;
  const EncoderSettings& settings_;
  std::unique_ptr<TesterY4mWriter> input_writer_;
  std::unique_ptr<TesterIvfWriter> output_writer_;
  Pacer pacer_;
  LimitedTaskQueue task_queue_;
};

}  // namespace

std::unique_ptr<VideoCodecStats> VideoCodecTesterImpl::RunDecodeTest(
    CodedVideoSource* video_source,
    Decoder* decoder,
    const DecoderSettings& decoder_settings) {
  VideoCodecAnalyzer perf_analyzer;
  TesterDecoder tester_decoder(decoder, &perf_analyzer, decoder_settings);

  tester_decoder.Initialize();

  while (auto frame = video_source->PullFrame()) {
    tester_decoder.Decode(*frame);
  }

  tester_decoder.Flush();

  return perf_analyzer.GetStats();
}

std::unique_ptr<VideoCodecStats> VideoCodecTesterImpl::RunEncodeTest(
    RawVideoSource* video_source,
    Encoder* encoder,
    const EncoderSettings& encoder_settings) {
  SyncRawVideoSource sync_source(video_source);
  VideoCodecAnalyzer perf_analyzer;
  TesterEncoder tester_encoder(encoder, /*decoder=*/nullptr, &perf_analyzer,
                               encoder_settings);

  tester_encoder.Initialize();

  while (auto frame = sync_source.PullFrame()) {
    tester_encoder.Encode(*frame);
  }

  tester_encoder.Flush();

  return perf_analyzer.GetStats();
}

std::unique_ptr<VideoCodecStats> VideoCodecTesterImpl::RunEncodeDecodeTest(
    RawVideoSource* video_source,
    Encoder* encoder,
    Decoder* decoder,
    const EncoderSettings& encoder_settings,
    const DecoderSettings& decoder_settings) {
  SyncRawVideoSource sync_source(video_source);
  VideoCodecAnalyzer perf_analyzer(&sync_source);
  TesterDecoder tester_decoder(decoder, &perf_analyzer, decoder_settings);
  TesterEncoder tester_encoder(encoder, &tester_decoder, &perf_analyzer,
                               encoder_settings);

  tester_encoder.Initialize();
  tester_decoder.Initialize();

  while (auto frame = sync_source.PullFrame()) {
    tester_encoder.Encode(*frame);
  }

  tester_encoder.Flush();
  tester_decoder.Flush();

  return perf_analyzer.GetStats();
}

}  // namespace test
}  // namespace webrtc
