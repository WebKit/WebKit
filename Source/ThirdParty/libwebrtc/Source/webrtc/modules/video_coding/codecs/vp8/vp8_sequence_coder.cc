/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/api/video/video_frame.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/common_video/include/video_image.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/testsupport/metrics/video_metrics.h"
#include "webrtc/tools/simple_command_line_parser.h"
#include "webrtc/video_frame.h"

class Vp8SequenceCoderEncodeCallback : public webrtc::EncodedImageCallback {
 public:
  explicit Vp8SequenceCoderEncodeCallback(FILE* encoded_file)
      : encoded_file_(encoded_file), encoded_bytes_(0) {}
  ~Vp8SequenceCoderEncodeCallback();
  Result OnEncodedImage(const webrtc::EncodedImage& encoded_image,
                        const webrtc::CodecSpecificInfo* codec_specific_info,
                        const webrtc::RTPFragmentationHeader*);
  // Returns the encoded image.
  webrtc::EncodedImage encoded_image() { return encoded_image_; }
  size_t encoded_bytes() { return encoded_bytes_; }

 private:
  webrtc::EncodedImage encoded_image_;
  FILE* encoded_file_;
  size_t encoded_bytes_;
};

Vp8SequenceCoderEncodeCallback::~Vp8SequenceCoderEncodeCallback() {
  delete[] encoded_image_._buffer;
  encoded_image_._buffer = NULL;
}

webrtc::EncodedImageCallback::Result
Vp8SequenceCoderEncodeCallback::OnEncodedImage(
    const webrtc::EncodedImage& encoded_image,
    const webrtc::CodecSpecificInfo* codecSpecificInfo,
    const webrtc::RTPFragmentationHeader* fragmentation) {
  if (encoded_image_._size < encoded_image._size) {
    delete[] encoded_image_._buffer;
    encoded_image_._buffer = NULL;
    encoded_image_._buffer = new uint8_t[encoded_image._size];
    encoded_image_._size = encoded_image._size;
  }
  memcpy(encoded_image_._buffer, encoded_image._buffer, encoded_image._size);
  encoded_image_._length = encoded_image._length;
  if (encoded_file_ != NULL) {
    if (fwrite(encoded_image._buffer, 1, encoded_image._length,
               encoded_file_) != encoded_image._length) {
      return Result(Result::ERROR_SEND_FAILED, 0);
    }
  }
  encoded_bytes_ += encoded_image_._length;
  return Result(Result::OK, 0);
}

// TODO(mikhal): Add support for varying the frame size.
class Vp8SequenceCoderDecodeCallback : public webrtc::DecodedImageCallback {
 public:
  explicit Vp8SequenceCoderDecodeCallback(FILE* decoded_file)
      : decoded_file_(decoded_file) {}
  int32_t Decoded(webrtc::VideoFrame& frame) override;
  int32_t Decoded(webrtc::VideoFrame& frame, int64_t decode_time_ms) override {
    RTC_NOTREACHED();
    return -1;
  }
  bool DecodeComplete();

 private:
  FILE* decoded_file_;
};

int Vp8SequenceCoderDecodeCallback::Decoded(webrtc::VideoFrame& image) {
  EXPECT_EQ(0, webrtc::PrintVideoFrame(image, decoded_file_));
  return 0;
}

int SequenceCoder(webrtc::test::CommandLineParser* parser) {
  int width = strtol((parser->GetFlag("w")).c_str(), NULL, 10);
  int height = strtol((parser->GetFlag("h")).c_str(), NULL, 10);
  int framerate = strtol((parser->GetFlag("f")).c_str(), NULL, 10);

  if (width <= 0 || height <= 0 || framerate <= 0) {
    fprintf(stderr, "Error: Resolution cannot be <= 0!\n");
    return -1;
  }
  int target_bitrate = strtol((parser->GetFlag("b")).c_str(), NULL, 10);
  if (target_bitrate <= 0) {
    fprintf(stderr, "Error: Bit-rate cannot be <= 0!\n");
    return -1;
  }

  // SetUp
  // Open input file.
  std::string encoded_file_name = parser->GetFlag("encoded_file");
  FILE* encoded_file = fopen(encoded_file_name.c_str(), "wb");
  if (encoded_file == NULL) {
    fprintf(stderr, "Error: Cannot open encoded file\n");
    return -1;
  }
  std::string input_file_name = parser->GetFlag("input_file");
  FILE* input_file = fopen(input_file_name.c_str(), "rb");
  if (input_file == NULL) {
    fprintf(stderr, "Error: Cannot open input file\n");
    return -1;
  }
  // Open output file.
  std::string output_file_name = parser->GetFlag("output_file");
  FILE* output_file = fopen(output_file_name.c_str(), "wb");
  if (output_file == NULL) {
    fprintf(stderr, "Error: Cannot open output file\n");
    return -1;
  }

  // Get range of frames: will encode num_frames following start_frame).
  int start_frame = strtol((parser->GetFlag("start_frame")).c_str(), NULL, 10);
  int num_frames = strtol((parser->GetFlag("num_frames")).c_str(), NULL, 10);

  // Codec SetUp.
  webrtc::VideoCodec inst;
  memset(&inst, 0, sizeof(inst));
  webrtc::VP8Encoder* encoder = webrtc::VP8Encoder::Create();
  webrtc::VP8Decoder* decoder = webrtc::VP8Decoder::Create();
  inst.codecType = webrtc::kVideoCodecVP8;
  inst.VP8()->feedbackModeOn = false;
  inst.VP8()->denoisingOn = true;
  inst.maxFramerate = framerate;
  inst.startBitrate = target_bitrate;
  inst.maxBitrate = 8000;
  inst.width = width;
  inst.height = height;

  if (encoder->InitEncode(&inst, 1, 1440) < 0) {
    fprintf(stderr, "Error: Cannot initialize vp8 encoder\n");
    return -1;
  }
  EXPECT_EQ(0, decoder->InitDecode(&inst, 1));

  size_t length = webrtc::CalcBufferSize(webrtc::kI420, width, height);
  std::unique_ptr<uint8_t[]> frame_buffer(new uint8_t[length]);

  int half_width = (width + 1) / 2;
  // Set and register callbacks.
  Vp8SequenceCoderEncodeCallback encoder_callback(encoded_file);
  encoder->RegisterEncodeCompleteCallback(&encoder_callback);
  Vp8SequenceCoderDecodeCallback decoder_callback(output_file);
  decoder->RegisterDecodeCompleteCallback(&decoder_callback);
  // Read->Encode->Decode sequence.
  // num_frames = -1 implies unlimited encoding (entire sequence).
  int64_t starttime = rtc::TimeMillis();
  int frame_cnt = 1;
  int frames_processed = 0;
  while (num_frames == -1 || frames_processed < num_frames) {
    rtc::scoped_refptr<VideoFrameBuffer> buffer(
        test::ReadI420Buffer(width, height, input_file));
    if (!buffer) {
      // EOF or read error.
      break;
    }
    if (frame_cnt >= start_frame) {
      encoder->Encode(VideoFrame(buffer, webrtc::kVideoRotation_0, 0),
                      NULL, NULL);
      decoder->Decode(encoder_callback.encoded_image(), false, NULL);
      ++frames_processed;
    }
    ++frame_cnt;
  }
  printf("\nProcessed %d frames\n", frames_processed);
  int64_t endtime = rtc::TimeMillis();
  int64_t totalExecutionTime = endtime - starttime;
  printf("Total execution time: %.2lf ms\n",
         static_cast<double>(totalExecutionTime));
  double actual_bit_rate =
      8.0 * encoder_callback.encoded_bytes() / (frame_cnt / inst.maxFramerate);
  printf("Actual bitrate: %f kbps\n", actual_bit_rate / 1000);
  webrtc::test::QualityMetricsResult psnr_result, ssim_result;
  EXPECT_EQ(0, webrtc::test::I420MetricsFromFiles(
                   input_file_name.c_str(), output_file_name.c_str(),
                   inst.width, inst.height, &psnr_result, &ssim_result));
  printf("PSNR avg: %f[dB], min: %f[dB]\nSSIM avg: %f, min: %f\n",
         psnr_result.average, psnr_result.min, ssim_result.average,
         ssim_result.min);
  return frame_cnt;
}

int main(int argc, char** argv) {
  std::string program_name = argv[0];
  std::string usage =
      "Encode and decodes a video sequence, and writes"
      "results to a file.\n"
      "Example usage:\n" +
      program_name +
      " functionality"
      " --w=352 --h=288 --input_file=input.yuv --output_file=output.yuv "
      " Command line flags:\n"
      "  - width(int): The width of the input file. Default: 352\n"
      "  - height(int): The height of the input file. Default: 288\n"
      "  - input_file(string): The YUV file to encode."
      "      Default: foreman.yuv\n"
      "  - encoded_file(string): The vp8 encoded file (encoder output)."
      "      Default: vp8_encoded.vp8\n"
      "  - output_file(string): The yuv decoded file (decoder output)."
      "      Default: vp8_decoded.yuv\n."
      "  - start_frame - frame number in which encoding will begin. Default: 0"
      "  - num_frames - Number of frames to be processed. "
      "      Default: -1 (entire sequence).";

  webrtc::test::CommandLineParser parser;

  // Init the parser and set the usage message.
  parser.Init(argc, argv);
  parser.SetUsageMessage(usage);

  // Reset flags.
  parser.SetFlag("w", "352");
  parser.SetFlag("h", "288");
  parser.SetFlag("f", "30");
  parser.SetFlag("b", "500");
  parser.SetFlag("start_frame", "0");
  parser.SetFlag("num_frames", "-1");
  parser.SetFlag("output_file", webrtc::test::OutputPath() + "vp8_decoded.yuv");
  parser.SetFlag("encoded_file",
                 webrtc::test::OutputPath() + "vp8_encoded.vp8");
  parser.SetFlag("input_file",
                 webrtc::test::ResourcePath("foreman_cif", "yuv"));
  parser.SetFlag("help", "false");

  parser.ProcessFlags();
  if (parser.GetFlag("help") == "true") {
    parser.PrintUsageMessage();
    exit(EXIT_SUCCESS);
  }
  parser.PrintEnteredFlags();

  return SequenceCoder(&parser);
}
