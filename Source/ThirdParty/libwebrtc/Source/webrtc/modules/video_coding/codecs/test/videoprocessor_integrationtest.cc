/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/videoprocessor_integrationtest.h"

#include <algorithm>
#include <utility>

#if defined(WEBRTC_ANDROID)
#include "modules/video_coding/codecs/test/android_test_initializer.h"
#include "sdk/android/src/jni/class_loader.h"
#include "sdk/android/src/jni/videodecoderfactorywrapper.h"
#include "sdk/android/src/jni/videoencoderfactorywrapper.h"
#elif defined(WEBRTC_IOS)
#include "modules/video_coding/codecs/test/objc_codec_h264_test.h"
#endif

#include "common_types.h"  // NOLINT(build/include)
#include "media/base/h264_profile_level_id.h"
#include "media/engine/internaldecoderfactory.h"
#include "media/engine/internalencoderfactory.h"
#include "media/engine/videodecodersoftwarefallbackwrapper.h"
#include "media/engine/videoencodersoftwarefallbackwrapper.h"
#include "modules/video_coding/codecs/vp8/include/vp8_common_types.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_coding.h"
#include "rtc_base/checks.h"
#include "rtc_base/cpu_time.h"
#include "rtc_base/event.h"
#include "rtc_base/file.h"
#include "rtc_base/ptr_util.h"
#include "system_wrappers/include/sleep.h"
#include "test/testsupport/fileutils.h"
#include "test/testsupport/metrics/video_metrics.h"

namespace webrtc {
namespace test {

namespace {

const int kMaxBitrateMismatchPercent = 20;

// Parameters from VP8 wrapper, which control target size of key frames.
const float kInitialBufferSize = 0.5f;
const float kOptimalBufferSize = 0.6f;
const float kScaleKeyFrameSize = 0.5f;

bool RunEncodeInRealTime(const TestConfig& config) {
  if (config.measure_cpu) {
    return true;
  }
#if defined(WEBRTC_ANDROID)
  // In order to not overwhelm the OpenMAX buffers in the Android MediaCodec.
  return (config.hw_encoder || config.hw_decoder);
#else
  return false;
#endif
}

SdpVideoFormat CreateSdpVideoFormat(const TestConfig& config) {
  switch (config.codec_settings.codecType) {
    case kVideoCodecVP8:
      return SdpVideoFormat(cricket::kVp8CodecName);

    case kVideoCodecVP9:
      return SdpVideoFormat(cricket::kVp9CodecName);

    case kVideoCodecH264: {
      const char* packetization_mode =
          config.h264_codec_settings.packetization_mode ==
                  H264PacketizationMode::NonInterleaved
              ? "1"
              : "0";
      return SdpVideoFormat(
          cricket::kH264CodecName,
          {{cricket::kH264FmtpProfileLevelId,
            *H264::ProfileLevelIdToString(H264::ProfileLevelId(
                config.h264_codec_settings.profile, H264::kLevel3_1))},
           {cricket::kH264FmtpPacketizationMode, packetization_mode}});
    }
    default:
      RTC_NOTREACHED();
      return SdpVideoFormat("");
  }
}

}  // namespace

void VideoProcessorIntegrationTest::H264KeyframeChecker::CheckEncodedFrame(
    webrtc::VideoCodecType codec,
    const EncodedImage& encoded_frame) const {
  EXPECT_EQ(kVideoCodecH264, codec);
  bool contains_sps = false;
  bool contains_pps = false;
  bool contains_idr = false;
  const std::vector<webrtc::H264::NaluIndex> nalu_indices =
      webrtc::H264::FindNaluIndices(encoded_frame._buffer,
                                    encoded_frame._length);
  for (const webrtc::H264::NaluIndex& index : nalu_indices) {
    webrtc::H264::NaluType nalu_type = webrtc::H264::ParseNaluType(
        encoded_frame._buffer[index.payload_start_offset]);
    if (nalu_type == webrtc::H264::NaluType::kSps) {
      contains_sps = true;
    } else if (nalu_type == webrtc::H264::NaluType::kPps) {
      contains_pps = true;
    } else if (nalu_type == webrtc::H264::NaluType::kIdr) {
      contains_idr = true;
    }
  }
  if (encoded_frame._frameType == kVideoFrameKey) {
    EXPECT_TRUE(contains_sps) << "Keyframe should contain SPS.";
    EXPECT_TRUE(contains_pps) << "Keyframe should contain PPS.";
    EXPECT_TRUE(contains_idr) << "Keyframe should contain IDR.";
  } else if (encoded_frame._frameType == kVideoFrameDelta) {
    EXPECT_FALSE(contains_sps) << "Delta frame should not contain SPS.";
    EXPECT_FALSE(contains_pps) << "Delta frame should not contain PPS.";
    EXPECT_FALSE(contains_idr) << "Delta frame should not contain IDR.";
  } else {
    RTC_NOTREACHED();
  }
}

class VideoProcessorIntegrationTest::CpuProcessTime final {
 public:
  explicit CpuProcessTime(const TestConfig& config) : config_(config) {}
  ~CpuProcessTime() {}

  void Start() {
    if (config_.measure_cpu) {
      cpu_time_ -= rtc::GetProcessCpuTimeNanos();
      wallclock_time_ -= rtc::SystemTimeNanos();
    }
  }
  void Stop() {
    if (config_.measure_cpu) {
      cpu_time_ += rtc::GetProcessCpuTimeNanos();
      wallclock_time_ += rtc::SystemTimeNanos();
    }
  }
  void Print() const {
    if (config_.measure_cpu) {
      printf("CPU usage %%: %f\n", GetUsagePercent() / config_.NumberOfCores());
      printf("\n");
    }
  }

 private:
  double GetUsagePercent() const {
    return static_cast<double>(cpu_time_) / wallclock_time_ * 100.0;
  }

  const TestConfig config_;
  int64_t cpu_time_ = 0;
  int64_t wallclock_time_ = 0;
};

VideoProcessorIntegrationTest::VideoProcessorIntegrationTest() {
#if defined(WEBRTC_ANDROID)
  InitializeAndroidObjects();
#endif
}

VideoProcessorIntegrationTest::~VideoProcessorIntegrationTest() = default;

// Processes all frames in the clip and verifies the result.
void VideoProcessorIntegrationTest::ProcessFramesAndMaybeVerify(
    const std::vector<RateProfile>& rate_profiles,
    const std::vector<RateControlThresholds>* rc_thresholds,
    const QualityThresholds* quality_thresholds,
    const BitstreamThresholds* bs_thresholds,
    const VisualizationParams* visualization_params) {
  RTC_DCHECK(!rate_profiles.empty());
  // The Android HW codec needs to be run on a task queue, so we simply always
  // run the test on a task queue.
  rtc::TaskQueue task_queue("VidProc TQ");
  rtc::Event sync_event(false, false);

  SetUpAndInitObjects(&task_queue, rate_profiles[0].target_kbps,
                      rate_profiles[0].input_fps, visualization_params);
  PrintSettings();

  // Set initial rates.
  int rate_update_index = 0;
  task_queue.PostTask([this, &rate_profiles, rate_update_index] {
    processor_->SetRates(rate_profiles[rate_update_index].target_kbps,
                         rate_profiles[rate_update_index].input_fps);
  });

  cpu_process_time_->Start();

  // Process all frames.
  int frame_number = 0;
  const int num_frames = config_.num_frames;
  RTC_DCHECK_GE(num_frames, 1);
  while (frame_number < num_frames) {
    if (RunEncodeInRealTime(config_)) {
      // Roughly pace the frames.
      SleepMs(rtc::kNumMillisecsPerSec /
              rate_profiles[rate_update_index].input_fps);
    }

    task_queue.PostTask([this] { processor_->ProcessFrame(); });
    ++frame_number;

    if (frame_number ==
        rate_profiles[rate_update_index].frame_index_rate_update) {
      ++rate_update_index;
      RTC_DCHECK_GT(rate_profiles.size(), rate_update_index);

      task_queue.PostTask([this, &rate_profiles, rate_update_index] {
        processor_->SetRates(rate_profiles[rate_update_index].target_kbps,
                             rate_profiles[rate_update_index].input_fps);
      });
    }
  }

  // Give the VideoProcessor pipeline some time to process the last frame,
  // and then release the codecs.
  if (config_.hw_encoder || config_.hw_decoder) {
    SleepMs(1 * rtc::kNumMillisecsPerSec);
  }
  cpu_process_time_->Stop();

  std::vector<int> num_dropped_frames;
  std::vector<int> num_spatial_resizes;
  sync_event.Reset();
  task_queue.PostTask(
      [this, &num_dropped_frames, &num_spatial_resizes, &sync_event]() {
        num_dropped_frames = processor_->NumberDroppedFramesPerRateUpdate();
        num_spatial_resizes = processor_->NumberSpatialResizesPerRateUpdate();
        sync_event.Set();
      });
  sync_event.Wait(rtc::Event::kForever);

  ReleaseAndCloseObjects(&task_queue);

  // Calculate and print rate control statistics.
  rate_update_index = 0;
  frame_number = 0;
  quality_ = QualityMetrics();
  ResetRateControlMetrics(rate_update_index, rate_profiles);
  while (frame_number < num_frames) {
    UpdateRateControlMetrics(frame_number);

    if (quality_thresholds) {
      UpdateQualityMetrics(frame_number);
    }

    if (bs_thresholds) {
      VerifyBitstream(frame_number, *bs_thresholds);
    }

    ++frame_number;

    if (frame_number ==
        rate_profiles[rate_update_index].frame_index_rate_update) {
      PrintRateControlMetrics(rate_update_index, num_dropped_frames,
                              num_spatial_resizes);
      VerifyRateControlMetrics(rate_update_index, rc_thresholds,
                               num_dropped_frames, num_spatial_resizes);
      ++rate_update_index;
      ResetRateControlMetrics(rate_update_index, rate_profiles);
    }
  }

  PrintRateControlMetrics(rate_update_index, num_dropped_frames,
                          num_spatial_resizes);
  VerifyRateControlMetrics(rate_update_index, rc_thresholds, num_dropped_frames,
                           num_spatial_resizes);

  if (quality_thresholds) {
    VerifyQualityMetrics(*quality_thresholds);
  }

  // Calculate and print other statistics.
  EXPECT_EQ(num_frames, static_cast<int>(stats_.size()));
  stats_.PrintSummary();
  cpu_process_time_->Print();
}

void VideoProcessorIntegrationTest::CreateEncoderAndDecoder() {
  std::unique_ptr<VideoEncoderFactory> encoder_factory;
  if (config_.hw_encoder) {
#if defined(WEBRTC_ANDROID)
    JNIEnv* env = jni::AttachCurrentThreadIfNeeded();
    jni::ScopedJavaLocalRef<jclass> factory_class =
        jni::GetClass(env, "org/webrtc/HardwareVideoEncoderFactory");
    jmethodID factory_constructor = env->GetMethodID(
        factory_class.obj(), "<init>", "(Lorg/webrtc/EglBase$Context;ZZ)V");
    jni::ScopedJavaLocalRef<jobject> factory_object(
        env, env->NewObject(factory_class.obj(), factory_constructor,
                            nullptr /* shared_context */,
                            false /* enable_intel_vp8_encoder */,
                            true /* enable_h264_high_profile */));
    encoder_factory = rtc::MakeUnique<webrtc::jni::VideoEncoderFactoryWrapper>(
        env, factory_object);
#elif defined(WEBRTC_IOS)
    EXPECT_EQ(kVideoCodecH264, config_.codec_settings.codecType)
        << "iOS HW codecs only support H264.";
    encoder_factory = CreateObjCEncoderFactory();
#else
    RTC_NOTREACHED() << "Only support HW encoder on Android and iOS.";
#endif
  } else {
    encoder_factory = rtc::MakeUnique<InternalEncoderFactory>();
  }

  std::unique_ptr<VideoDecoderFactory> decoder_factory;
  if (config_.hw_decoder) {
#if defined(WEBRTC_ANDROID)
    JNIEnv* env = jni::AttachCurrentThreadIfNeeded();
    jni::ScopedJavaLocalRef<jclass> factory_class =
        jni::GetClass(env, "org/webrtc/HardwareVideoDecoderFactory");
    jmethodID factory_constructor = env->GetMethodID(
        factory_class.obj(), "<init>", "(Lorg/webrtc/EglBase$Context;)V");
    jni::ScopedJavaLocalRef<jobject> factory_object(
        env, env->NewObject(factory_class.obj(), factory_constructor,
                            nullptr /* shared_context */));
    decoder_factory = rtc::MakeUnique<webrtc::jni::VideoDecoderFactoryWrapper>(
        env, factory_object);
#elif defined(WEBRTC_IOS)
    EXPECT_EQ(kVideoCodecH264, config_.codec_settings.codecType)
        << "iOS HW codecs only support H264.";
    decoder_factory = CreateObjCDecoderFactory();
#else
    RTC_NOTREACHED() << "Only support HW decoder on Android and iOS.";
#endif
  } else {
    decoder_factory = rtc::MakeUnique<InternalDecoderFactory>();
  }

  const SdpVideoFormat format = CreateSdpVideoFormat(config_);
  encoder_ = encoder_factory->CreateVideoEncoder(format);
  decoder_ = decoder_factory->CreateVideoDecoder(format);

  if (config_.sw_fallback_encoder) {
    encoder_ = rtc::MakeUnique<VideoEncoderSoftwareFallbackWrapper>(
        InternalEncoderFactory().CreateVideoEncoder(format),
        std::move(encoder_));
  }
  if (config_.sw_fallback_decoder) {
    decoder_ = rtc::MakeUnique<VideoDecoderSoftwareFallbackWrapper>(
        InternalDecoderFactory().CreateVideoDecoder(format),
        std::move(decoder_));
  }

  EXPECT_TRUE(encoder_) << "Encoder not successfully created.";
  EXPECT_TRUE(decoder_) << "Decoder not successfully created.";
}

void VideoProcessorIntegrationTest::DestroyEncoderAndDecoder() {
  encoder_.reset();
  decoder_.reset();
}

void VideoProcessorIntegrationTest::SetUpAndInitObjects(
    rtc::TaskQueue* task_queue,
    const int initial_bitrate_kbps,
    const int initial_framerate_fps,
    const VisualizationParams* visualization_params) {
  CreateEncoderAndDecoder();

  config_.codec_settings.minBitrate = 0;
  config_.codec_settings.startBitrate = initial_bitrate_kbps;
  config_.codec_settings.maxFramerate = initial_framerate_fps;

  // Create file objects for quality analysis.
  analysis_frame_reader_.reset(new YuvFrameReaderImpl(
      config_.input_filename, config_.codec_settings.width,
      config_.codec_settings.height));
  analysis_frame_writer_.reset(new YuvFrameWriterImpl(
      config_.output_filename, config_.codec_settings.width,
      config_.codec_settings.height));
  EXPECT_TRUE(analysis_frame_reader_->Init());
  EXPECT_TRUE(analysis_frame_writer_->Init());

  if (visualization_params) {
    const std::string output_filename_base =
        OutputPath() + config_.FilenameWithParams();
    if (visualization_params->save_encoded_ivf) {
      rtc::File post_encode_file =
          rtc::File::Create(output_filename_base + ".ivf");
      encoded_frame_writer_ =
          IvfFileWriter::Wrap(std::move(post_encode_file), 0);
    }
    if (visualization_params->save_decoded_y4m) {
      decoded_frame_writer_.reset(new Y4mFrameWriterImpl(
          output_filename_base + ".y4m", config_.codec_settings.width,
          config_.codec_settings.height, initial_framerate_fps));
      EXPECT_TRUE(decoded_frame_writer_->Init());
    }
  }

  cpu_process_time_.reset(new CpuProcessTime(config_));
  packet_manipulator_.reset(new PacketManipulatorImpl(
      &packet_reader_, config_.networking_config, false));

  rtc::Event sync_event(false, false);
  task_queue->PostTask([this, &sync_event]() {
    processor_ = rtc::MakeUnique<VideoProcessor>(
        encoder_.get(), decoder_.get(), analysis_frame_reader_.get(),
        packet_manipulator_.get(), config_, &stats_,
        encoded_frame_writer_.get(), decoded_frame_writer_.get());
    sync_event.Set();
  });
  sync_event.Wait(rtc::Event::kForever);
}

void VideoProcessorIntegrationTest::ReleaseAndCloseObjects(
    rtc::TaskQueue* task_queue) {
  rtc::Event sync_event(false, false);
  task_queue->PostTask([this, &sync_event]() {
    processor_.reset();
    sync_event.Set();
  });
  sync_event.Wait(rtc::Event::kForever);

  // The VideoProcessor must be destroyed before the codecs.
  DestroyEncoderAndDecoder();

  analysis_frame_reader_->Close();

  // Close visualization files.
  if (encoded_frame_writer_) {
    EXPECT_TRUE(encoded_frame_writer_->Close());
  }
  if (decoded_frame_writer_) {
    decoded_frame_writer_->Close();
  }
}

// For every encoded frame, update the rate control metrics.
void VideoProcessorIntegrationTest::UpdateRateControlMetrics(int frame_number) {
  RTC_CHECK_GE(frame_number, 0);

  const int tl_idx = config_.TemporalLayerForFrame(frame_number);
  ++actual_.num_frames_layer[tl_idx];
  ++actual_.num_frames;

  const FrameStatistic* frame_stat = stats_.GetFrame(frame_number);
  FrameType frame_type = frame_stat->frame_type;
  float framesize_kbits = frame_stat->encoded_frame_size_bytes * 8.0f / 1000.0f;

  // Update rate mismatch relative to per-frame bandwidth.
  if (frame_type == kVideoFrameDelta) {
    // TODO(marpan): Should we count dropped (zero size) frames in mismatch?
    actual_.sum_delta_framesize_mismatch_layer[tl_idx] +=
        fabs(framesize_kbits - target_.framesize_kbits_layer[tl_idx]) /
        target_.framesize_kbits_layer[tl_idx];
  } else {
    float key_framesize_kbits = (frame_number == 0)
                                    ? target_.key_framesize_kbits_initial
                                    : target_.key_framesize_kbits;
    actual_.sum_key_framesize_mismatch +=
        fabs(framesize_kbits - key_framesize_kbits) / key_framesize_kbits;
    ++actual_.num_key_frames;
  }
  actual_.sum_framesize_kbits += framesize_kbits;
  actual_.sum_framesize_kbits_layer[tl_idx] += framesize_kbits;

  // Encoded bitrate: from the start of the update/run to current frame.
  actual_.kbps = actual_.sum_framesize_kbits * target_.fps / actual_.num_frames;
  actual_.kbps_layer[tl_idx] = actual_.sum_framesize_kbits_layer[tl_idx] *
                               target_.fps_layer[tl_idx] /
                               actual_.num_frames_layer[tl_idx];

  // Number of frames to hit target bitrate.
  if (actual_.BitrateMismatchPercent(target_.kbps) <
      kMaxBitrateMismatchPercent) {
    actual_.num_frames_to_hit_target =
        std::min(actual_.num_frames, actual_.num_frames_to_hit_target);
  }
}

// Verify expected behavior of rate control.
void VideoProcessorIntegrationTest::VerifyRateControlMetrics(
    int rate_update_index,
    const std::vector<RateControlThresholds>* rc_thresholds,
    const std::vector<int>& num_dropped_frames,
    const std::vector<int>& num_spatial_resizes) const {
  if (!rc_thresholds)
    return;

  const RateControlThresholds& rc_threshold =
      (*rc_thresholds)[rate_update_index];

  EXPECT_LE(num_dropped_frames[rate_update_index],
            rc_threshold.max_num_dropped_frames);
  EXPECT_EQ(rc_threshold.num_spatial_resizes,
            num_spatial_resizes[rate_update_index]);

  EXPECT_LE(actual_.num_frames_to_hit_target,
            rc_threshold.max_num_frames_to_hit_target);
  EXPECT_EQ(rc_threshold.num_key_frames, actual_.num_key_frames);
  EXPECT_LE(actual_.KeyFrameSizeMismatchPercent(),
            rc_threshold.max_key_framesize_mismatch_percent);
  EXPECT_LE(actual_.BitrateMismatchPercent(target_.kbps),
            rc_threshold.max_bitrate_mismatch_percent);

  const int num_temporal_layers = config_.NumberOfTemporalLayers();
  for (int i = 0; i < num_temporal_layers; ++i) {
    EXPECT_LE(actual_.DeltaFrameSizeMismatchPercent(i),
              rc_threshold.max_delta_framesize_mismatch_percent);
    EXPECT_LE(actual_.BitrateMismatchPercent(i, target_.kbps_layer[i]),
              rc_threshold.max_bitrate_mismatch_percent);
  }
}

void VideoProcessorIntegrationTest::UpdateQualityMetrics(int frame_number) {
  FrameStatistic* frame_stat = stats_.GetFrame(frame_number);
  if (frame_stat->decoding_successful) {
    ++quality_.num_decoded_frames;
    quality_.total_psnr += frame_stat->psnr;
    quality_.total_ssim += frame_stat->ssim;
    if (frame_stat->psnr < quality_.min_psnr)
      quality_.min_psnr = frame_stat->psnr;
    if (frame_stat->ssim < quality_.min_ssim)
      quality_.min_ssim = frame_stat->ssim;
  }
}

void VideoProcessorIntegrationTest::PrintRateControlMetrics(
    int rate_update_index,
    const std::vector<int>& num_dropped_frames,
    const std::vector<int>& num_spatial_resizes) const {
  if (rate_update_index == 0) {
    printf("Rate control statistics\n==\n");
  }

  printf("Rate update #%d:\n", rate_update_index);
  printf(" Target bitrate          : %d\n", target_.kbps);
  printf(" Encoded bitrate         : %f\n", actual_.kbps);
  printf(" Frame rate              : %d\n", target_.fps);
  printf(" # processed frames      : %d\n", actual_.num_frames);
  printf(" # frames to convergence : %d\n", actual_.num_frames_to_hit_target);
  printf(" # dropped frames        : %d\n",
         num_dropped_frames[rate_update_index]);
  printf(" # spatial resizes       : %d\n",
         num_spatial_resizes[rate_update_index]);
  printf(" # key frames            : %d\n", actual_.num_key_frames);
  printf(" Key frame rate mismatch : %d\n",
         actual_.KeyFrameSizeMismatchPercent());

  const int num_temporal_layers = config_.NumberOfTemporalLayers();
  for (int i = 0; i < num_temporal_layers; ++i) {
    printf(" Temporal layer #%d:\n", i);
    printf("  TL%d target bitrate        : %f\n", i, target_.kbps_layer[i]);
    printf("  TL%d encoded bitrate       : %f\n", i, actual_.kbps_layer[i]);
    printf("  TL%d frame rate            : %f\n", i, target_.fps_layer[i]);
    printf("  TL%d # processed frames    : %d\n", i,
           actual_.num_frames_layer[i]);
    printf("  TL%d frame size %% mismatch : %d\n", i,
           actual_.DeltaFrameSizeMismatchPercent(i));
    printf("  TL%d bitrate %% mismatch    : %d\n", i,
           actual_.BitrateMismatchPercent(i, target_.kbps_layer[i]));
    printf("  TL%d per-frame bitrate     : %f\n", i,
           target_.framesize_kbits_layer[i]);
  }
  printf("\n");
}

void VideoProcessorIntegrationTest::PrintSettings() const {
  printf("VideoProcessor settings\n==\n");
  printf(" Total # of frames: %d", analysis_frame_reader_->NumberOfFrames());
  printf("%s\n", config_.ToString().c_str());

  printf("VideoProcessorIntegrationTest settings\n==\n");
  const char* encoder_name = encoder_->ImplementationName();
  printf(" Encoder implementation name: %s\n", encoder_name);
  const char* decoder_name = decoder_->ImplementationName();
  printf(" Decoder implementation name: %s\n", decoder_name);
  if (strcmp(encoder_name, decoder_name) == 0) {
    printf(" Codec implementation name : %s_%s\n", config_.CodecName().c_str(),
           encoder_name);
  }
  printf("\n");
}

void VideoProcessorIntegrationTest::VerifyBitstream(
    int frame_number,
    const BitstreamThresholds& bs_thresholds) {
  RTC_CHECK_GE(frame_number, 0);
  const FrameStatistic* frame_stat = stats_.GetFrame(frame_number);
  EXPECT_LE(*(frame_stat->max_nalu_length), bs_thresholds.max_nalu_length);
}

void VideoProcessorIntegrationTest::VerifyQualityMetrics(
    const QualityThresholds& quality_thresholds) {
  EXPECT_GT(quality_.num_decoded_frames, 0);
  EXPECT_GT(quality_.total_psnr / quality_.num_decoded_frames,
            quality_thresholds.min_avg_psnr);
  EXPECT_GT(quality_.min_psnr, quality_thresholds.min_min_psnr);
  EXPECT_GT(quality_.total_ssim / quality_.num_decoded_frames,
            quality_thresholds.min_avg_ssim);
  EXPECT_GT(quality_.min_ssim, quality_thresholds.min_min_ssim);
}

// Reset quantities before each encoder rate update.
void VideoProcessorIntegrationTest::ResetRateControlMetrics(
    int rate_update_index,
    const std::vector<RateProfile>& rate_profiles) {
  RTC_DCHECK_GT(rate_profiles.size(), rate_update_index);
  // Set new rates.
  target_.kbps = rate_profiles[rate_update_index].target_kbps;
  target_.fps = rate_profiles[rate_update_index].input_fps;
  SetRatesPerTemporalLayer();

  // Set key frame target sizes.
  if (rate_update_index == 0) {
    target_.key_framesize_kbits_initial =
        0.5 * kInitialBufferSize * target_.kbps_layer[0];
  }

  // Set maximum size of key frames, following setting in the VP8 wrapper.
  float max_key_size = kScaleKeyFrameSize * kOptimalBufferSize * target_.fps;
  // We don't know exact target size of the key frames (except for first one),
  // but the minimum in libvpx is ~|3 * per_frame_bandwidth| and maximum is
  // set by |max_key_size_ * per_frame_bandwidth|. Take middle point/average
  // as reference for mismatch. Note key frames always correspond to base
  // layer frame in this test.
  target_.key_framesize_kbits =
      0.5 * (3 + max_key_size) * target_.framesize_kbits_layer[0];

  // Reset rate control metrics.
  actual_ = TestResults();
  actual_.num_frames_to_hit_target =  // Set to max number of frames.
      rate_profiles[rate_update_index].frame_index_rate_update;
}

void VideoProcessorIntegrationTest::SetRatesPerTemporalLayer() {
  const int num_temporal_layers = config_.NumberOfTemporalLayers();
  RTC_DCHECK_LE(num_temporal_layers, kMaxNumTemporalLayers);

  for (int i = 0; i < num_temporal_layers; ++i) {
    float bitrate_ratio;
    if (i > 0) {
      bitrate_ratio = kVp8LayerRateAlloction[num_temporal_layers - 1][i] -
                      kVp8LayerRateAlloction[num_temporal_layers - 1][i - 1];
    } else {
      bitrate_ratio = kVp8LayerRateAlloction[num_temporal_layers - 1][i];
    }
    target_.kbps_layer[i] = target_.kbps * bitrate_ratio;
    target_.fps_layer[i] =
        target_.fps / static_cast<float>(1 << (num_temporal_layers - 1));
  }
  if (num_temporal_layers == 3) {
    target_.fps_layer[2] = target_.fps / 2.0f;
  }

  // Update layer per-frame-bandwidth.
  for (int i = 0; i < num_temporal_layers; ++i) {
    target_.framesize_kbits_layer[i] =
        target_.kbps_layer[i] / target_.fps_layer[i];
  }
}

}  // namespace test
}  // namespace webrtc
