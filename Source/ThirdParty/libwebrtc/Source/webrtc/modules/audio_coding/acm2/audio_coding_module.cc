/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/include/audio_coding_module.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/audio/audio_view.h"
#include "api/audio_codecs/audio_encoder.h"
#include "api/function_view.h"
#include "common_audio/resampler/include/push_resampler.h"
#include "modules/audio_coding/acm2/acm_remixing.h"
#include "modules/audio_coding/include/audio_coding_module_typedefs.h"
#include "modules/include/module_common_types_public.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

namespace {

// Initial size for the buffer in InputBuffer. This matches 6 channels of 10 ms
// 48 kHz data.
constexpr size_t kInitialInputDataBufferSize = 6 * 480;

constexpr int32_t kMaxInputSampleRateHz = 192000;

class AudioCodingModuleImpl final : public AudioCodingModule {
 public:
  explicit AudioCodingModuleImpl();
  ~AudioCodingModuleImpl() override;

  void Reset() override;

  /////////////////////////////////////////
  //   Sender
  //

  void ModifyEncoder(
      FunctionView<void(std::unique_ptr<AudioEncoder>*)> modifier) override;

  // Register a transport callback which will be
  // called to deliver the encoded buffers.
  int RegisterTransportCallback(AudioPacketizationCallback* transport) override;

  // Add 10 ms of raw (PCM) audio data to the encoder.
  int Add10MsData(const AudioFrame& audio_frame) override;

  /////////////////////////////////////////
  // (FEC) Forward Error Correction (codec internal)
  //

  // Set target packet loss rate
  int SetPacketLossRate(int loss_rate) override;

  /////////////////////////////////////////
  //   Statistics
  //

  ANAStats GetANAStats() const override;

  int GetTargetBitrate() const override;

 private:
  struct InputData {
    InputData() : buffer(kInitialInputDataBufferSize) {}
    uint32_t input_timestamp;
    const int16_t* audio;
    size_t length_per_channel;
    size_t audio_channel;
    // If a re-mix is required (up or down), this buffer will store a re-mixed
    // version of the input.
    std::vector<int16_t> buffer;
  };

  InputData input_data_ RTC_GUARDED_BY(acm_mutex_);

  // This member class writes values to the named UMA histogram, but only if
  // the value has changed since the last time (and always for the first call).
  class ChangeLogger {
   public:
    explicit ChangeLogger(absl::string_view histogram_name)
        : histogram_name_(histogram_name) {}
    // Logs the new value if it is different from the last logged value, or if
    // this is the first call.
    void MaybeLog(int value);

   private:
    int last_value_ = 0;
    int first_time_ = true;
    const std::string histogram_name_;
  };

  int Add10MsDataInternal(const AudioFrame& audio_frame, InputData* input_data)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(acm_mutex_);

  // TODO(bugs.webrtc.org/10739): change `absolute_capture_timestamp_ms` to
  // int64_t when it always receives a valid value.
  int Encode(const InputData& input_data,
             std::optional<int64_t> absolute_capture_timestamp_ms)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(acm_mutex_);

  bool HaveValidEncoder(absl::string_view caller_name) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(acm_mutex_);

  // Updates or checks `expected_in_ts_` and `expected_codec_ts` based on the
  // timestamps in `in_frame`. If no audio frame has been received, the fields
  // are just set. For subsequent frames, the expected timestamps are checked
  // for consistency.
  void SetInputTimestamps(const AudioFrame& in_frame)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(acm_mutex_);

  // Preprocessing of input audio, including resampling and down-mixing if
  // required, before pushing audio into encoder's buffer.
  //
  // in_frame: input audio-frame
  // ptr_out: pointer to output audio_frame. If no preprocessing is required
  //          `ptr_out` will be pointing to `in_frame`, otherwise pointing to
  //          `preprocess_frame_`.
  //
  // Return value:
  //   -1: if encountering an error.
  //    0: otherwise.
  int PreprocessToAddData(const AudioFrame& in_frame,
                          const AudioFrame** ptr_out)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(acm_mutex_);

  // Called from `PreprocessToAddData` when no resampling or downmixing is
  // required. Returns a pointer to an output audio_frame. If timestamps are as
  // expected the return value will point to `in_frame`, otherwise the data will
  // have been copied into `preprocess_frame_` and the returned pointer points
  // to `preprocess_frame_`.
  const AudioFrame* AddDataNoPreProcess(const AudioFrame& in_frame)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(acm_mutex_);

  // Change required states after starting to receive the codec corresponding
  // to `index`.
  int UpdateUponReceivingCodec(int index);

  mutable Mutex acm_mutex_;
  Buffer encode_buffer_ RTC_GUARDED_BY(acm_mutex_);
  uint32_t expected_codec_ts_ RTC_GUARDED_BY(acm_mutex_);
  uint32_t expected_in_ts_ RTC_GUARDED_BY(acm_mutex_);
  PushResampler<int16_t> resampler_ RTC_GUARDED_BY(acm_mutex_);
  ChangeLogger bitrate_logger_ RTC_GUARDED_BY(acm_mutex_);

  // Current encoder stack, provided by a call to RegisterEncoder.
  std::unique_ptr<AudioEncoder> encoder_stack_ RTC_GUARDED_BY(acm_mutex_);

  // This is to keep track of CN instances where we can send DTMFs.
  uint8_t previous_pltype_ RTC_GUARDED_BY(acm_mutex_);

  AudioFrame preprocess_frame_ RTC_GUARDED_BY(acm_mutex_);
  bool first_10ms_data_ RTC_GUARDED_BY(acm_mutex_);

  bool first_frame_ RTC_GUARDED_BY(acm_mutex_);
  uint32_t last_timestamp_ RTC_GUARDED_BY(acm_mutex_);
  uint32_t last_rtp_timestamp_ RTC_GUARDED_BY(acm_mutex_);
  std::optional<int64_t> absolute_capture_timestamp_ms_
      RTC_GUARDED_BY(acm_mutex_);

  Mutex callback_mutex_;
  AudioPacketizationCallback* packetization_callback_
      RTC_GUARDED_BY(callback_mutex_);

  int codec_histogram_bins_log_[static_cast<size_t>(
      AudioEncoder::CodecType::kMaxLoggedAudioCodecTypes)];
  int number_of_consecutive_empty_packets_;

  mutable Mutex stats_mutex_;
  ANAStats ana_stats_ RTC_GUARDED_BY(stats_mutex_);
  int target_bitrate_ RTC_GUARDED_BY(stats_mutex_) = -1;
};

// Adds a codec usage sample to the histogram.
void UpdateCodecTypeHistogram(size_t codec_type) {
  RTC_HISTOGRAM_ENUMERATION(
      "WebRTC.Audio.Encoder.CodecType", static_cast<int>(codec_type),
      static_cast<int>(AudioEncoder::CodecType::kMaxLoggedAudioCodecTypes));
}

void AudioCodingModuleImpl::ChangeLogger::MaybeLog(int value) {
  if (value != last_value_ || first_time_) {
    first_time_ = false;
    last_value_ = value;
    RTC_HISTOGRAM_COUNTS_SPARSE_100(histogram_name_, value);
  }
}

AudioCodingModuleImpl::AudioCodingModuleImpl()
    : expected_codec_ts_(0xD87F3F9F),
      expected_in_ts_(0xD87F3F9F),
      bitrate_logger_("WebRTC.Audio.TargetBitrateInKbps"),
      encoder_stack_(nullptr),
      previous_pltype_(255),
      first_10ms_data_(false),
      first_frame_(true),
      packetization_callback_(nullptr),
      codec_histogram_bins_log_(),
      number_of_consecutive_empty_packets_(0) {
  RTC_LOG(LS_INFO) << "Created";
}

AudioCodingModuleImpl::~AudioCodingModuleImpl() = default;

int32_t AudioCodingModuleImpl::Encode(
    const InputData& input_data,
    std::optional<int64_t> absolute_capture_timestamp_ms) {
  // TODO(bugs.webrtc.org/10739): add dcheck that
  // `audio_frame.absolute_capture_timestamp_ms()` always has a value.
  AudioEncoder::EncodedInfo encoded_info;
  uint8_t previous_pltype;

  // Check if there is an encoder before.
  if (!HaveValidEncoder("Process"))
    return -1;

  if (!first_frame_) {
    RTC_DCHECK(IsNewerTimestamp(input_data.input_timestamp, last_timestamp_))
        << "Time should not move backwards";
  }

  // Scale the timestamp to the codec's RTP timestamp rate.
  uint32_t rtp_timestamp =
      first_frame_
          ? input_data.input_timestamp
          : last_rtp_timestamp_ +
                dchecked_cast<uint32_t>(CheckedDivExact(
                    int64_t{input_data.input_timestamp - last_timestamp_} *
                        encoder_stack_->RtpTimestampRateHz(),
                    int64_t{encoder_stack_->SampleRateHz()}));

  last_timestamp_ = input_data.input_timestamp;
  last_rtp_timestamp_ = rtp_timestamp;
  first_frame_ = false;

  if (!absolute_capture_timestamp_ms_.has_value()) {
    absolute_capture_timestamp_ms_ = absolute_capture_timestamp_ms;
  }

  // Clear the buffer before reuse - encoded data will get appended.
  encode_buffer_.Clear();
  encoded_info = encoder_stack_->Encode(
      rtp_timestamp,
      ArrayView<const int16_t>(
          input_data.audio,
          input_data.audio_channel * input_data.length_per_channel),
      &encode_buffer_);

  bitrate_logger_.MaybeLog(encoder_stack_->GetTargetBitrate() / 1000);
  if (encode_buffer_.empty() && !encoded_info.send_even_if_empty) {
    // Not enough data.
    return 0;
  }
  previous_pltype = previous_pltype_;  // Read it while we have the critsect.

  // Log codec type to histogram once every 500 packets.
  if (encoded_info.encoded_bytes == 0) {
    ++number_of_consecutive_empty_packets_;
  } else {
    size_t codec_type = static_cast<size_t>(encoded_info.encoder_type);
    codec_histogram_bins_log_[codec_type] +=
        number_of_consecutive_empty_packets_ + 1;
    number_of_consecutive_empty_packets_ = 0;
    if (codec_histogram_bins_log_[codec_type] >= 500) {
      codec_histogram_bins_log_[codec_type] -= 500;
      UpdateCodecTypeHistogram(codec_type);
    }
  }

  AudioFrameType frame_type;
  if (encode_buffer_.empty() && encoded_info.send_even_if_empty) {
    frame_type = AudioFrameType::kEmptyFrame;
    encoded_info.payload_type = previous_pltype;
  } else {
    RTC_DCHECK_GT(encode_buffer_.size(), 0);
    frame_type = encoded_info.speech ? AudioFrameType::kAudioFrameSpeech
                                     : AudioFrameType::kAudioFrameCN;
  }

  {
    MutexLock lock(&callback_mutex_);
    if (packetization_callback_) {
      packetization_callback_->SendData(
          frame_type, encoded_info.payload_type, encoded_info.encoded_timestamp,
          encode_buffer_.data(), encode_buffer_.size(),
          absolute_capture_timestamp_ms_.value_or(-1));
    }
  }
  absolute_capture_timestamp_ms_.reset();
  previous_pltype_ = encoded_info.payload_type;
  {
    MutexLock lock(&stats_mutex_);
    ana_stats_ = encoder_stack_->GetANAStats();
    target_bitrate_ = encoder_stack_->GetTargetBitrate();
  }
  return static_cast<int32_t>(encode_buffer_.size());
}

/////////////////////////////////////////
//   Sender
//

void AudioCodingModuleImpl::Reset() {
  MutexLock lock(&acm_mutex_);
  absolute_capture_timestamp_ms_.reset();
  if (HaveValidEncoder("Reset")) {
    encoder_stack_->Reset();
  }
}

void AudioCodingModuleImpl::ModifyEncoder(
    FunctionView<void(std::unique_ptr<AudioEncoder>*)> modifier) {
  MutexLock lock(&acm_mutex_);
  modifier(&encoder_stack_);
}

// Register a transport callback which will be called to deliver
// the encoded buffers.
int AudioCodingModuleImpl::RegisterTransportCallback(
    AudioPacketizationCallback* transport) {
  MutexLock lock(&callback_mutex_);
  packetization_callback_ = transport;
  return 0;
}

// Add 10MS of raw (PCM) audio data to the encoder.
int AudioCodingModuleImpl::Add10MsData(const AudioFrame& audio_frame) {
  MutexLock lock(&acm_mutex_);
  int r = Add10MsDataInternal(audio_frame, &input_data_);
  // TODO(bugs.webrtc.org/10739): add dcheck that
  // `audio_frame.absolute_capture_timestamp_ms()` always has a value.
  return r < 0
             ? r
             : Encode(input_data_, audio_frame.absolute_capture_timestamp_ms());
}

int AudioCodingModuleImpl::Add10MsDataInternal(const AudioFrame& audio_frame,
                                               InputData* input_data) {
  if (audio_frame.samples_per_channel_ == 0) {
    RTC_DCHECK_NOTREACHED();
    RTC_LOG(LS_ERROR) << "Cannot Add 10 ms audio, payload length is zero";
    return -1;
  }

  if (audio_frame.sample_rate_hz_ > kMaxInputSampleRateHz) {
    RTC_DCHECK_NOTREACHED();
    RTC_LOG(LS_ERROR) << "Cannot Add 10 ms audio, input frequency not valid";
    return -1;
  }

  // If the length and frequency matches. We currently just support raw PCM.
  if (static_cast<size_t>(audio_frame.sample_rate_hz_ / 100) !=
      audio_frame.samples_per_channel_) {
    RTC_LOG(LS_ERROR)
        << "Cannot Add 10 ms audio, input frequency and length doesn't match";
    return -1;
  }

  if (audio_frame.num_channels_ != 1 && audio_frame.num_channels_ != 2 &&
      audio_frame.num_channels_ != 4 && audio_frame.num_channels_ != 6 &&
      audio_frame.num_channels_ != 8) {
    RTC_LOG(LS_ERROR) << "Cannot Add 10 ms audio, invalid number of channels.";
    return -1;
  }

  // Do we have a codec registered?
  if (!HaveValidEncoder("Add10MsData")) {
    return -1;
  }

  const AudioFrame* ptr_frame;
  // Perform a resampling, also down-mix if it is required and can be
  // performed before resampling (a down mix prior to resampling will take
  // place if both primary and secondary encoders are mono and input is in
  // stereo).
  if (PreprocessToAddData(audio_frame, &ptr_frame) < 0) {
    return -1;
  }

  // Check whether we need an up-mix or down-mix?
  const size_t current_num_channels = encoder_stack_->NumChannels();
  const bool same_num_channels =
      ptr_frame->num_channels_ == current_num_channels;

  // TODO(yujo): Skip encode of muted frames.
  input_data->input_timestamp = ptr_frame->timestamp_;
  input_data->length_per_channel = ptr_frame->samples_per_channel_;
  input_data->audio_channel = current_num_channels;

  if (!same_num_channels) {
    // Remixes the input frame to the output data and in the process resize the
    // output data if needed.
    ReMixFrame(*ptr_frame, current_num_channels, &input_data->buffer);

    // For pushing data to primary, point the `ptr_audio` to correct buffer.
    input_data->audio = input_data->buffer.data();
    RTC_DCHECK_GE(input_data->buffer.size(),
                  input_data->length_per_channel * input_data->audio_channel);
  } else {
    // When adding data to encoders this pointer is pointing to an audio buffer
    // with correct number of channels.
    input_data->audio = ptr_frame->data();
  }

  return 0;
}

void AudioCodingModuleImpl::SetInputTimestamps(const AudioFrame& in_frame) {
  if (!first_10ms_data_) {
    expected_in_ts_ = in_frame.timestamp_;
    expected_codec_ts_ = in_frame.timestamp_;
    first_10ms_data_ = true;
  } else if (in_frame.timestamp_ != expected_in_ts_) {
    RTC_LOG(LS_WARNING) << "Unexpected input timestamp: " << in_frame.timestamp_
                        << ", expected: " << expected_in_ts_;
    expected_codec_ts_ +=
        (in_frame.timestamp_ - expected_in_ts_) *
        static_cast<uint32_t>(
            static_cast<double>(encoder_stack_->SampleRateHz()) /
            static_cast<double>(in_frame.sample_rate_hz_));
    expected_in_ts_ = in_frame.timestamp_;
  }
}

// Perform a resampling and down-mix if required. We down-mix only if
// encoder is mono and input is stereo. In case of dual-streaming, both
// encoders has to be mono for down-mix to take place.
// |*ptr_out| will point to the pre-processed audio-frame. If no pre-processing
// is required, |*ptr_out| points to `in_frame`.
// TODO(yujo): Make this more efficient for muted frames.
int AudioCodingModuleImpl::PreprocessToAddData(const AudioFrame& in_frame,
                                               const AudioFrame** ptr_out) {
  SetInputTimestamps(in_frame);

  const bool resample =
      in_frame.sample_rate_hz_ != encoder_stack_->SampleRateHz();

  // This variable is true if primary codec and secondary codec (if exists)
  // are both mono and input is stereo.
  // TODO(henrik.lundin): This condition should probably be
  //   in_frame.num_channels_ > encoder_stack_->NumChannels()
  const bool down_mix =
      in_frame.num_channels_ == 2 && encoder_stack_->NumChannels() == 1;

  if (!down_mix && !resample) {
    // No preprocessing is required.
    *ptr_out = AddDataNoPreProcess(in_frame);
    return 0;
  }

  // Some pre-processing will be required, so we'll use the internal buffer.
  *ptr_out = &preprocess_frame_;
  preprocess_frame_.timestamp_ = expected_codec_ts_;
  preprocess_frame_.samples_per_channel_ = in_frame.samples_per_channel_;

  // Temporary buffer in case both downmixing and resampling is required.
  std::array<int16_t, AudioFrame::kMaxDataSizeSamples> audio;
  // When resampling is needed, this view will represent the buffer to resample.
  InterleavedView<const int16_t> resample_src_audio;

  if (down_mix) {
    RTC_DCHECK_GE(audio.size(), in_frame.samples_per_channel());
    preprocess_frame_.num_channels_ = 1;  // We always downmix to mono.
    // If a resampling is also required, the output of a down-mix is written
    // into a local buffer, otherwise, it will be written to the output frame.
    auto downmixed =
        resample
            ? InterleavedView<int16_t>(audio.data(),
                                       in_frame.samples_per_channel(), 1)
            : preprocess_frame_.mutable_data(in_frame.samples_per_channel(), 1);
    DownMixFrame(in_frame, downmixed.AsMono());
    if (resample) {
      // Set the input for the resampler to the down-mixed signal.
      resample_src_audio = downmixed;
    }
  } else {
    preprocess_frame_.num_channels_ = in_frame.num_channels_;
    if (resample) {
      // Set the input of the resampler to the original data.
      resample_src_audio = in_frame.data_view();
    }
  }

  RTC_DCHECK(resample_src_audio.empty() || resample);
  preprocess_frame_.SetSampleRateAndChannelSize(encoder_stack_->SampleRateHz());

  if (resample) {
    resampler_.Resample(
        resample_src_audio,
        preprocess_frame_.mutable_data(preprocess_frame_.samples_per_channel(),
                                       preprocess_frame_.num_channels()));
  }

  expected_codec_ts_ +=
      static_cast<uint32_t>(preprocess_frame_.samples_per_channel_);
  expected_in_ts_ += static_cast<uint32_t>(in_frame.samples_per_channel_);

  return 0;
}

const AudioFrame* AudioCodingModuleImpl::AddDataNoPreProcess(
    const AudioFrame& in_frame) {
  const AudioFrame* ret = nullptr;
  // No preprocessing is required.
  if (expected_in_ts_ == expected_codec_ts_) {
    // Timestamps as expected, we can use the input frame as-is.
    ret = &in_frame;
  } else {
    // Otherwise we'll need to alter the timestamp. Since in_frame is const,
    // we'll have to make a copy of it.
    preprocess_frame_.CopyFrom(in_frame);
    preprocess_frame_.timestamp_ = expected_codec_ts_;
    ret = &preprocess_frame_;
  }

  expected_in_ts_ += static_cast<uint32_t>(in_frame.samples_per_channel_);
  expected_codec_ts_ += static_cast<uint32_t>(in_frame.samples_per_channel_);

  return ret;
}

/////////////////////////////////////////
//   (FEC) Forward Error Correction (codec internal)
//

int AudioCodingModuleImpl::SetPacketLossRate(int loss_rate) {
  MutexLock lock(&acm_mutex_);
  if (HaveValidEncoder("SetPacketLossRate")) {
    encoder_stack_->OnReceivedUplinkPacketLossFraction(loss_rate / 100.0);
  }
  return 0;
}

/////////////////////////////////////////
//   Statistics
//

bool AudioCodingModuleImpl::HaveValidEncoder(
    absl::string_view caller_name) const {
  if (!encoder_stack_) {
    RTC_LOG(LS_ERROR) << caller_name << " failed: No send codec is registered.";
    return false;
  }
  return true;
}

ANAStats AudioCodingModuleImpl::GetANAStats() const {
  MutexLock lock(&stats_mutex_);
  return ana_stats_;
}

int AudioCodingModuleImpl::GetTargetBitrate() const {
  MutexLock lock(&stats_mutex_);
  return target_bitrate_;
}

}  // namespace

std::unique_ptr<AudioCodingModule> AudioCodingModule::Create() {
  return std::make_unique<AudioCodingModuleImpl>();
}

}  // namespace webrtc
