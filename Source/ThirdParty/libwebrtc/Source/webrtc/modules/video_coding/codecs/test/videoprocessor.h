/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_H_
#define MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/video/video_frame.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "modules/video_coding/codecs/test/packet_manipulator.h"
#include "modules/video_coding/codecs/test/stats.h"
#include "modules/video_coding/codecs/test/test_config.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/utility/ivf_file_writer.h"
#include "modules/video_coding/utility/vp8_header_parser.h"
#include "modules/video_coding/utility/vp9_uncompressed_header_parser.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/task_queue.h"
#include "test/testsupport/frame_reader.h"
#include "test/testsupport/frame_writer.h"

namespace webrtc {

class VideoBitrateAllocator;

namespace test {

// Handles encoding/decoding of video using the VideoEncoder/VideoDecoder
// interfaces. This is done in a sequential manner in order to be able to
// measure times properly.
// The class processes a frame at the time for the configured input file.
// It maintains state of where in the source input file the processing is at.
//
// Regarding packet loss: Note that keyframes are excluded (first or all
// depending on the ExcludeFrameTypes setting). This is because if key frames
// would be altered, all the following delta frames would be pretty much
// worthless. VP8 has an error-resilience feature that makes it able to handle
// packet loss in key non-first keyframes, which is why only the first is
// excluded by default.
// Packet loss in such important frames is handled on a higher level in the
// Video Engine, where signaling would request a retransmit of the lost packets,
// since they're so important.
//
// Note this class is not thread safe and is meant for simple testing purposes.
class VideoProcessor {
 public:
  VideoProcessor(webrtc::VideoEncoder* encoder,
                 webrtc::VideoDecoder* decoder,
                 FrameReader* analysis_frame_reader,
                 PacketManipulator* packet_manipulator,
                 const TestConfig& config,
                 Stats* stats,
                 IvfFileWriter* encoded_frame_writer,
                 FrameWriter* decoded_frame_writer);
  ~VideoProcessor();

  // Reads a frame from the analysis frame reader and sends it to the encoder.
  // When the encode callback is received, the encoded frame is sent to the
  // decoder. The decoded frame is written to disk by the analysis frame writer.
  // Objective video quality metrics can thus be calculated after the fact.
  void ProcessFrame();

  // Updates the encoder with target rates. Must be called at least once.
  void SetRates(int bitrate_kbps, int framerate_fps);

  // Returns the number of dropped frames.
  std::vector<int> NumberDroppedFramesPerRateUpdate() const;

  // Returns the number of spatial resizes.
  std::vector<int> NumberSpatialResizesPerRateUpdate() const;

 private:
  class VideoProcessorEncodeCompleteCallback
      : public webrtc::EncodedImageCallback {
   public:
    explicit VideoProcessorEncodeCompleteCallback(
        VideoProcessor* video_processor)
        : video_processor_(video_processor),
          task_queue_(rtc::TaskQueue::Current()) {}

    Result OnEncodedImage(
        const webrtc::EncodedImage& encoded_image,
        const webrtc::CodecSpecificInfo* codec_specific_info,
        const webrtc::RTPFragmentationHeader* fragmentation) override {
      RTC_CHECK(codec_specific_info);

      if (task_queue_ && !task_queue_->IsCurrent()) {
        task_queue_->PostTask(
            std::unique_ptr<rtc::QueuedTask>(new EncodeCallbackTask(
                video_processor_, encoded_image, codec_specific_info)));
        return Result(Result::OK, 0);
      }

      video_processor_->FrameEncoded(codec_specific_info->codecType,
                                     encoded_image);
      return Result(Result::OK, 0);
    }

   private:
    class EncodeCallbackTask : public rtc::QueuedTask {
     public:
      EncodeCallbackTask(VideoProcessor* video_processor,
                         const webrtc::EncodedImage& encoded_image,
                         const webrtc::CodecSpecificInfo* codec_specific_info)
          : video_processor_(video_processor),
            buffer_(encoded_image._buffer, encoded_image._length),
            encoded_image_(encoded_image),
            codec_specific_info_(*codec_specific_info) {
        encoded_image_._buffer = buffer_.data();
      }

      bool Run() override {
        video_processor_->FrameEncoded(codec_specific_info_.codecType,
                                       encoded_image_);
        return true;
      }

     private:
      VideoProcessor* const video_processor_;
      rtc::Buffer buffer_;
      webrtc::EncodedImage encoded_image_;
      const webrtc::CodecSpecificInfo codec_specific_info_;
    };

    VideoProcessor* const video_processor_;
    rtc::TaskQueue* const task_queue_;
  };

  class VideoProcessorDecodeCompleteCallback
      : public webrtc::DecodedImageCallback {
   public:
    explicit VideoProcessorDecodeCompleteCallback(
        VideoProcessor* video_processor)
        : video_processor_(video_processor),
          task_queue_(rtc::TaskQueue::Current()) {}

    int32_t Decoded(webrtc::VideoFrame& image) override {
      if (task_queue_ && !task_queue_->IsCurrent()) {
        task_queue_->PostTask(
            [this, image]() { video_processor_->FrameDecoded(image); });
        return 0;
      }
      video_processor_->FrameDecoded(image);
      return 0;
    }

    int32_t Decoded(webrtc::VideoFrame& image,
                    int64_t decode_time_ms) override {
      return Decoded(image);
    }

    void Decoded(webrtc::VideoFrame& image,
                 rtc::Optional<int32_t> decode_time_ms,
                 rtc::Optional<uint8_t> qp) override {
      Decoded(image);
    }

   private:
    VideoProcessor* const video_processor_;
    rtc::TaskQueue* const task_queue_;
  };

  // Invoked by the callback adapter when a frame has completed encoding.
  void FrameEncoded(webrtc::VideoCodecType codec,
                    const webrtc::EncodedImage& encodedImage);

  // Invoked by the callback adapter when a frame has completed decoding.
  void FrameDecoded(const webrtc::VideoFrame& image);

  void WriteDecodedFrameToFile(rtc::Buffer* buffer);
  bool ExcludeFrame(const EncodedImage& encoded_image);

  TestConfig config_ RTC_GUARDED_BY(sequence_checker_);

  webrtc::VideoEncoder* const encoder_;
  webrtc::VideoDecoder* const decoder_;
  const std::unique_ptr<VideoBitrateAllocator> bitrate_allocator_;

  // Adapters for the codec callbacks.
  VideoProcessorEncodeCompleteCallback encode_callback_;
  VideoProcessorDecodeCompleteCallback decode_callback_;

  // Fake network.
  PacketManipulator* const packet_manipulator_;

  // Input frames. Used as reference at frame quality evaluation.
  // Async codecs might queue frames. To handle that we keep input frame
  // and release it after corresponding coded frame is decoded and quality
  // measurement is done.
  std::map<int, std::unique_ptr<VideoFrame>> input_frames_
      RTC_GUARDED_BY(sequence_checker_);

  // These (mandatory) file manipulators are used for, e.g., objective PSNR and
  // SSIM calculations at the end of a test run.
  FrameReader* const analysis_frame_reader_;

  // These (optional) file writers are used to persistently store the encoded
  // and decoded bitstreams. The purpose is to give the experimenter an option
  // to subjectively evaluate the quality of the processing. Each frame writer
  // is enabled by being non-null.
  IvfFileWriter* const encoded_frame_writer_;
  FrameWriter* const decoded_frame_writer_;

  // Keep track of inputed/encoded/decoded frames, so we can detect frame drops.
  int last_inputed_frame_num_ RTC_GUARDED_BY(sequence_checker_);
  int last_encoded_frame_num_ RTC_GUARDED_BY(sequence_checker_);
  int last_decoded_frame_num_ RTC_GUARDED_BY(sequence_checker_);

  // Store an RTP timestamp -> frame number map, since the timestamps are
  // based off of the frame rate, which can change mid-test.
  std::map<uint32_t, int> rtp_timestamp_to_frame_num_
      RTC_GUARDED_BY(sequence_checker_);

  // Keep track of if we have excluded the first key frame from packet loss.
  bool first_key_frame_has_been_excluded_ RTC_GUARDED_BY(sequence_checker_);

  // Keep track of the last successfully decoded frame, since we write that
  // frame to disk when decoding fails.
  rtc::Buffer last_decoded_frame_buffer_ RTC_GUARDED_BY(sequence_checker_);

  // Statistics.
  Stats* stats_;
  std::vector<int> num_dropped_frames_ RTC_GUARDED_BY(sequence_checker_);
  std::vector<int> num_spatial_resizes_ RTC_GUARDED_BY(sequence_checker_);
  int rate_update_index_ RTC_GUARDED_BY(sequence_checker_);

  rtc::SequencedTaskChecker sequence_checker_;

  RTC_DISALLOW_COPY_AND_ASSIGN(VideoProcessor);
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_H_
