/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <string>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/match.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/test/create_frame_generator.h"
#include "api/test/frame_generator_interface.h"
#include "api/video/encoded_image.h"
#include "api/video/video_codec_type.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "media/base/media_constants.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/utility/ivf_file_writer.h"
#include "rtc_base/checks.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/system/file_wrapper.h"
#include "rtc_base/task_queue.h"
#include "test/testsupport/frame_reader.h"
#include "test/video_codec_settings.h"

#if defined(WEBRTC_USE_H264)
#include "modules/video_coding/codecs/h264/include/h264.h"
#endif

ABSL_FLAG(std::string, input, "", "Input YUV file to convert to IVF");
ABSL_FLAG(int, width, 0, "Input frame width");
ABSL_FLAG(int, height, 0, "Input frame height");
ABSL_FLAG(std::string, codec, cricket::kVp8CodecName, "Codec to use");
ABSL_FLAG(std::string, output, "", "Output IVF file");

namespace webrtc {
namespace test {
namespace {

constexpr int kMaxFramerate = 30;
// We use very big value here to ensure that codec won't hit any limits.
constexpr uint32_t kBitrateBps = 100000000;
constexpr int kKeyFrameIntervalMs = 30000;
constexpr int kMaxFrameEncodeWaitTimeoutMs = 2000;
constexpr int kFrameLogInterval = 100;
static const VideoEncoder::Capabilities kCapabilities(false);

class IvfFileWriterEncodedCallback : public EncodedImageCallback {
 public:
  IvfFileWriterEncodedCallback(const std::string& file_name,
                               VideoCodecType video_codec_type,
                               int expected_frames_count)
      : file_writer_(
            IvfFileWriter::Wrap(FileWrapper::OpenWriteOnly(file_name), 0)),
        video_codec_type_(video_codec_type),
        expected_frames_count_(expected_frames_count) {
    RTC_CHECK(file_writer_.get());
  }
  ~IvfFileWriterEncodedCallback() { RTC_CHECK(file_writer_->Close()); }

  Result OnEncodedImage(const EncodedImage& encoded_image,
                        const CodecSpecificInfo* codec_specific_info,
                        const RTPFragmentationHeader* fragmentation) override {
    RTC_CHECK(file_writer_->WriteFrame(encoded_image, video_codec_type_));

    rtc::CritScope crit(&lock_);
    received_frames_count_++;
    RTC_CHECK_LE(received_frames_count_, expected_frames_count_);
    if (received_frames_count_ % kFrameLogInterval == 0) {
      RTC_LOG(INFO) << received_frames_count_ << " out of "
                    << expected_frames_count_ << " frames written";
    }
    next_frame_written_.Set();
    return Result(Result::Error::OK);
  }

  void WaitNextFrameWritten(int timeout_ms) {
    RTC_CHECK(next_frame_written_.Wait(timeout_ms));
    next_frame_written_.Reset();
  }

 private:
  std::unique_ptr<IvfFileWriter> file_writer_;
  const VideoCodecType video_codec_type_;
  const int expected_frames_count_;

  rtc::CriticalSection lock_;
  int received_frames_count_ RTC_GUARDED_BY(lock_) = 0;
  rtc::Event next_frame_written_;
};

class Encoder {
 public:
  Encoder(int width,
          int height,
          int frames_count,
          const std::string& output_file_name,
          VideoCodecType video_codec_type,
          std::unique_ptr<VideoEncoder> video_encoder)
      : video_encoder_(std::move(video_encoder)),
        task_queue_(CreateDefaultTaskQueueFactory()->CreateTaskQueue(
            "Encoder",
            TaskQueueFactory::Priority::HIGH)) {
    ivf_writer_callback_ = std::make_unique<IvfFileWriterEncodedCallback>(
        output_file_name, video_codec_type, frames_count);

    task_queue_.PostTask([width, height, video_codec_type, this]() {
      VideoCodec codec_settings;
      CodecSettings(video_codec_type, &codec_settings);
      codec_settings.width = width;
      codec_settings.height = height;
      codec_settings.maxFramerate = kMaxFramerate;
      codec_settings.startBitrate = kBitrateBps;
      codec_settings.minBitrate = kBitrateBps;
      codec_settings.maxBitrate = kBitrateBps;
      switch (video_codec_type) {
        case VideoCodecType::kVideoCodecVP8: {
          VideoCodecVP8* vp8_settings = codec_settings.VP8();
          vp8_settings->frameDroppingOn = false;
          vp8_settings->keyFrameInterval = kKeyFrameIntervalMs;
          vp8_settings->denoisingOn = false;
        } break;
        case VideoCodecType::kVideoCodecVP9: {
          VideoCodecVP9* vp9_settings = codec_settings.VP9();
          vp9_settings->denoisingOn = false;
          vp9_settings->frameDroppingOn = false;
          vp9_settings->keyFrameInterval = kKeyFrameIntervalMs;
          vp9_settings->automaticResizeOn = false;
        } break;
        case VideoCodecType::kVideoCodecH264: {
          VideoCodecH264* h264_settings = codec_settings.H264();
          h264_settings->frameDroppingOn = false;
          h264_settings->keyFrameInterval = kKeyFrameIntervalMs;
        } break;
        default:
          RTC_CHECK(false) << "Unsupported codec type";
      }
      VideoBitrateAllocation bitrate_allocation;
      bitrate_allocation.SetBitrate(0, 0, kBitrateBps);

      video_encoder_->RegisterEncodeCompleteCallback(
          ivf_writer_callback_.get());
      RTC_CHECK_EQ(
          WEBRTC_VIDEO_CODEC_OK,
          video_encoder_->InitEncode(
              &codec_settings,
              VideoEncoder::Settings(kCapabilities, /*number_of_cores=*/4,
                                     /*max_payload_size=*/0)));
      video_encoder_->SetRates(VideoEncoder::RateControlParameters(
          bitrate_allocation,
          static_cast<double>(codec_settings.maxFramerate)));
    });
  }

  void Encode(const VideoFrame& frame) {
    task_queue_.PostTask([frame, this]() {
      RTC_CHECK_EQ(WEBRTC_VIDEO_CODEC_OK,
                   video_encoder_->Encode(frame, nullptr));
    });
  }

  void WaitNextFrameWritten(int timeout_ms) {
    ivf_writer_callback_->WaitNextFrameWritten(timeout_ms);
  }

 private:
  std::unique_ptr<VideoEncoder> video_encoder_;
  std::unique_ptr<IvfFileWriterEncodedCallback> ivf_writer_callback_;

  rtc::TaskQueue task_queue_;
};

int GetFrameCount(std::string yuv_file_name, int width, int height) {
  std::unique_ptr<FrameReader> yuv_reader =
      std::make_unique<YuvFrameReaderImpl>(std::move(yuv_file_name), width,
                                           height);
  RTC_CHECK(yuv_reader->Init());
  int frames_count = yuv_reader->NumberOfFrames();
  yuv_reader->Close();
  return frames_count;
}

VideoFrame BuildFrame(FrameGeneratorInterface::VideoFrameData frame_data,
                      uint32_t rtp_timestamp) {
  return VideoFrame::Builder()
      .set_video_frame_buffer(frame_data.buffer)
      .set_update_rect(frame_data.update_rect)
      .set_timestamp_rtp(rtp_timestamp)
      .build();
}

void WriteVideoFile(std::string input_file_name,
                    int width,
                    int height,
                    std::string output_file_name,
                    VideoCodecType video_codec_type,
                    std::unique_ptr<VideoEncoder> video_encoder) {
  int frames_count = GetFrameCount(input_file_name, width, height);

  std::unique_ptr<FrameGeneratorInterface> frame_generator =
      CreateFromYuvFileFrameGenerator({input_file_name}, width, height,
                                      /*frame_repeat_count=*/1);

  Encoder encoder(width, height, frames_count, output_file_name,
                  video_codec_type, std::move(video_encoder));

  uint32_t last_frame_timestamp = 0;

  for (int i = 0; i < frames_count; ++i) {
    const uint32_t timestamp =
        last_frame_timestamp + kVideoPayloadTypeFrequency / kMaxFramerate;
    VideoFrame frame = BuildFrame(frame_generator->NextFrame(), timestamp);

    last_frame_timestamp = timestamp;

    encoder.Encode(frame);
    encoder.WaitNextFrameWritten(kMaxFrameEncodeWaitTimeoutMs);

    if ((i + 1) % kFrameLogInterval == 0) {
      RTC_LOG(INFO) << i + 1 << " out of " << frames_count
                    << " frames are sent for encoding";
    }
  }
  RTC_LOG(INFO) << "All " << frames_count << " frame are sent for encoding";
}

}  // namespace
}  // namespace test
}  // namespace webrtc

int main(int argc, char* argv[]) {
  // Initialize the symbolizer to get a human-readable stack trace.
  absl::InitializeSymbolizer(argv[0]);

  absl::FailureSignalHandlerOptions options;
  absl::InstallFailureSignalHandler(options);

  absl::ParseCommandLine(argc, argv);

  std::string codec_name = absl::GetFlag(FLAGS_codec);
  std::string input_file_name = absl::GetFlag(FLAGS_input);
  std::string output_file_name = absl::GetFlag(FLAGS_output);
  int width = absl::GetFlag(FLAGS_width);
  int height = absl::GetFlag(FLAGS_height);
  RTC_CHECK_NE(input_file_name, "") << "--input is required";
  RTC_CHECK_NE(output_file_name, "") << "--output is required";
  RTC_CHECK_GT(width, 0) << "width must be greater then 0";
  RTC_CHECK_GT(height, 0) << "height must be greater then 0";
  if (absl::EqualsIgnoreCase(codec_name, cricket::kVp8CodecName)) {
    webrtc::test::WriteVideoFile(
        input_file_name, width, height, output_file_name,
        webrtc::VideoCodecType::kVideoCodecVP8, webrtc::VP8Encoder::Create());
    return 0;
  }
  if (absl::EqualsIgnoreCase(codec_name, cricket::kVp9CodecName)) {
    webrtc::test::WriteVideoFile(
        input_file_name, width, height, output_file_name,
        webrtc::VideoCodecType::kVideoCodecVP9, webrtc::VP9Encoder::Create());
    return 0;
  }
#if defined(WEBRTC_USE_H264)
  if (absl::EqualsIgnoreCase(codec_name, cricket::kH264CodecName)) {
    webrtc::test::WriteVideoFile(
        input_file_name, width, height, output_file_name,
        webrtc::VideoCodecType::kVideoCodecH264,
        webrtc::H264Encoder::Create(
            cricket::VideoCodec(cricket::kH264CodecName)));
    return 0;
  }
#endif
  RTC_CHECK(false) << "Unsupported codec: " << codec_name;
  return 1;
}
