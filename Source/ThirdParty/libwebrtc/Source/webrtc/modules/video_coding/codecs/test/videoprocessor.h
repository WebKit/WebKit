/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_H_

#include <memory>
#include <string>

#include "webrtc/api/video/video_frame.h"
#include "webrtc/base/checks.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/codecs/test/packet_manipulator.h"
#include "webrtc/modules/video_coding/codecs/test/stats.h"
#include "webrtc/modules/video_coding/utility/ivf_file_writer.h"
#include "webrtc/test/testsupport/frame_reader.h"
#include "webrtc/test/testsupport/frame_writer.h"

namespace webrtc {

class VideoBitrateAllocator;

namespace test {

// Defines which frame types shall be excluded from packet loss and when.
enum ExcludeFrameTypes {
  // Will exclude the first keyframe in the video sequence from packet loss.
  // Following keyframes will be targeted for packet loss.
  kExcludeOnlyFirstKeyFrame,
  // Exclude all keyframes from packet loss, no matter where in the video
  // sequence they occur.
  kExcludeAllKeyFrames
};

// Returns a string representation of the enum value.
const char* ExcludeFrameTypesToStr(ExcludeFrameTypes e);

// Test configuration for a test run.
struct TestConfig {
  TestConfig();
  ~TestConfig();

  // Name of the test. This is purely metadata and does not affect
  // the test in any way.
  std::string name;

  // More detailed description of the test. This is purely metadata and does
  // not affect the test in any way.
  std::string description;

  // Number of this test. Useful if multiple runs of the same test with
  // different configurations shall be managed.
  int test_number;

  // File to process for the test. This must be a video file in the YUV format.
  std::string input_filename;

  // File to write to during processing for the test. Will be a video file
  // in the YUV format.
  std::string output_filename;

  // Path to the directory where encoded files will be put
  // (absolute or relative to the executable). Default: "out".
  std::string output_dir;

  // Configurations related to networking.
  NetworkingConfig networking_config;

  // Decides how the packet loss simulations shall exclude certain frames
  // from packet loss. Default: kExcludeOnlyFirstKeyFrame.
  ExcludeFrameTypes exclude_frame_types;

  // The length of a single frame of the input video file. This value is
  // calculated out of the width and height according to the video format
  // specification. Must be set before processing.
  size_t frame_length_in_bytes;

  // Force the encoder and decoder to use a single core for processing.
  // Using a single core is necessary to get a deterministic behavior for the
  // encoded frames - using multiple cores will produce different encoded frames
  // since multiple cores are competing to consume the byte budget for each
  // frame in parallel.
  // If set to false, the maximum number of available cores will be used.
  // Default: false.
  bool use_single_core;

  // If set to a value >0 this setting forces the encoder to create a keyframe
  // every Nth frame. Note that the encoder may create a keyframe in other
  // locations in addition to the interval that is set using this parameter.
  // Forcing key frames may also affect encoder planning optimizations in
  // a negative way, since it will suddenly be forced to produce an expensive
  // key frame.
  // Default: 0.
  int keyframe_interval;

  // The codec settings to use for the test (target bitrate, video size,
  // framerate and so on). This struct must be created and filled in using
  // the VideoCodingModule::Codec() method.
  webrtc::VideoCodec* codec_settings;

  // If printing of information to stdout shall be performed during processing.
  bool verbose;
};

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
// Note this class is not thread safe in any way and is meant for simple testing
// purposes.
class VideoProcessor {
 public:
  virtual ~VideoProcessor() {}

  // Performs initial calculations about frame size, sets up callbacks etc.
  // Returns false if an error has occurred, in addition to printing to stderr.
  virtual bool Init() = 0;

  // Processes a single frame. Returns true as long as there's more frames
  // available in the source clip.
  // Frame number must be an integer >= 0.
  virtual bool ProcessFrame(int frame_number) = 0;

  // Updates the encoder with the target bit rate and the frame rate.
  virtual void SetRates(int bit_rate, int frame_rate) = 0;

  // Return the size of the encoded frame in bytes. Dropped frames by the
  // encoder are regarded as zero size.
  virtual size_t EncodedFrameSize() = 0;

  // Return the encoded frame type (key or delta).
  virtual FrameType EncodedFrameType() = 0;

  // Return the number of dropped frames.
  virtual int NumberDroppedFrames() = 0;

  // Return the number of spatial resizes.
  virtual int NumberSpatialResizes() = 0;
};

class VideoProcessorImpl : public VideoProcessor {
 public:
  VideoProcessorImpl(webrtc::VideoEncoder* encoder,
                     webrtc::VideoDecoder* decoder,
                     FrameReader* analysis_frame_reader,
                     FrameWriter* analysis_frame_writer,
                     PacketManipulator* packet_manipulator,
                     const TestConfig& config,
                     Stats* stats,
                     FrameWriter* source_frame_writer,
                     IvfFileWriter* encoded_frame_writer,
                     FrameWriter* decoded_frame_writer);
  virtual ~VideoProcessorImpl();
  bool Init() override;
  bool ProcessFrame(int frame_number) override;

 private:
  // Callback class required to implement according to the VideoEncoder API.
  class VideoProcessorEncodeCompleteCallback
      : public webrtc::EncodedImageCallback {
   public:
    explicit VideoProcessorEncodeCompleteCallback(VideoProcessorImpl* vp)
        : video_processor_(vp) {}
    Result OnEncodedImage(
        const webrtc::EncodedImage& encoded_image,
        const webrtc::CodecSpecificInfo* codec_specific_info,
        const webrtc::RTPFragmentationHeader* fragmentation) override {
      // Forward to parent class.
      RTC_CHECK(codec_specific_info);
      video_processor_->FrameEncoded(codec_specific_info->codecType,
                                     encoded_image, fragmentation);
      return Result(Result::OK, 0);
    }

   private:
    VideoProcessorImpl* const video_processor_;
  };

  // Callback class required to implement according to the VideoDecoder API.
  class VideoProcessorDecodeCompleteCallback
      : public webrtc::DecodedImageCallback {
   public:
    explicit VideoProcessorDecodeCompleteCallback(VideoProcessorImpl* vp)
        : video_processor_(vp) {}
    int32_t Decoded(webrtc::VideoFrame& image) override {
      // Forward to parent class.
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
      Decoded(image,
              decode_time_ms ? static_cast<int32_t>(*decode_time_ms) : -1);
    }

   private:
    VideoProcessorImpl* const video_processor_;
  };

  // Invoked by the callback when a frame has completed encoding.
  void FrameEncoded(webrtc::VideoCodecType codec,
                    const webrtc::EncodedImage& encodedImage,
                    const webrtc::RTPFragmentationHeader* fragmentation);

  // Invoked by the callback when a frame has completed decoding.
  void FrameDecoded(const webrtc::VideoFrame& image);

  // Used for getting a 32-bit integer representing time
  // (checks the size is within signed 32-bit bounds before casting it)
  int GetElapsedTimeMicroseconds(int64_t start, int64_t stop);

  // Updates the encoder with the target bit rate and the frame rate.
  void SetRates(int bit_rate, int frame_rate) override;

  // Return the size of the encoded frame in bytes.
  size_t EncodedFrameSize() override;

  // Return the encoded frame type (key or delta).
  FrameType EncodedFrameType() override;

  // Return the number of dropped frames.
  int NumberDroppedFrames() override;

  // Return the number of spatial resizes.
  int NumberSpatialResizes() override;

  webrtc::VideoEncoder* const encoder_;
  webrtc::VideoDecoder* const decoder_;
  const std::unique_ptr<VideoBitrateAllocator> bitrate_allocator_;

  // Adapters for the codec callbacks.
  const std::unique_ptr<EncodedImageCallback> encode_callback_;
  const std::unique_ptr<DecodedImageCallback> decode_callback_;

  PacketManipulator* const packet_manipulator_;
  const TestConfig& config_;

  // These (mandatory) file manipulators are used for, e.g., objective PSNR and
  // SSIM calculations at the end of a test run.
  FrameReader* const analysis_frame_reader_;
  FrameWriter* const analysis_frame_writer_;

  // These (optional) file writers are used for persistently storing the output
  // of the coding pipeline at different stages: pre encode (source), post
  // encode (encoded), and post decode (decoded). The purpose is to give the
  // experimenter an option to subjectively evaluate the quality of the
  // encoding, given the test settings. Each frame writer is enabled by being
  // non-null.
  FrameWriter* const source_frame_writer_;
  IvfFileWriter* const encoded_frame_writer_;
  FrameWriter* const decoded_frame_writer_;

  // Keep track of the last successful frame, since we need to write that
  // when decoding fails.
  std::unique_ptr<uint8_t[]> last_successful_frame_buffer_;
  // To keep track of if we have excluded the first key frame from packet loss.
  bool first_key_frame_has_been_excluded_;
  // To tell the decoder previous frame have been dropped due to packet loss.
  bool last_frame_missing_;
  // If Init() has executed successfully.
  bool initialized_;
  size_t encoded_frame_size_;
  FrameType encoded_frame_type_;
  int prev_time_stamp_;
  int last_encoder_frame_width_;
  int last_encoder_frame_height_;

  // Statistics.
  Stats* stats_;
  int num_dropped_frames_;
  int num_spatial_resizes_;
  double bit_rate_factor_;  // Multiply frame length with this to get bit rate.
  int64_t encode_start_ns_;
  int64_t decode_start_ns_;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_H_
