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

#include <algorithm>

#include "modules/audio_coding/acm2/acm_receiver.h"
#include "modules/audio_coding/acm2/acm_resampler.h"
#include "modules/audio_coding/acm2/codec_manager.h"
#include "modules/audio_coding/acm2/rent_a_codec.h"
#include "modules/include/module_common_types.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

namespace {

struct EncoderFactory {
  AudioEncoder* external_speech_encoder = nullptr;
  acm2::CodecManager codec_manager;
  acm2::RentACodec rent_a_codec;
};

class AudioCodingModuleImpl final : public AudioCodingModule {
 public:
  explicit AudioCodingModuleImpl(const AudioCodingModule::Config& config);
  ~AudioCodingModuleImpl() override;

  /////////////////////////////////////////
  //   Sender
  //

  // Can be called multiple times for Codec, CNG, RED.
  int RegisterSendCodec(const CodecInst& send_codec) override;

  void RegisterExternalSendCodec(
      AudioEncoder* external_speech_encoder) override;

  void ModifyEncoder(rtc::FunctionView<void(std::unique_ptr<AudioEncoder>*)>
                         modifier) override;

  // Get current send codec.
  absl::optional<CodecInst> SendCodec() const override;

  // Sets the bitrate to the specified value in bits/sec. In case the codec does
  // not support the requested value it will choose an appropriate value
  // instead.
  void SetBitRate(int bitrate_bps) override;

  // Register a transport callback which will be
  // called to deliver the encoded buffers.
  int RegisterTransportCallback(AudioPacketizationCallback* transport) override;

  // Add 10 ms of raw (PCM) audio data to the encoder.
  int Add10MsData(const AudioFrame& audio_frame) override;

  /////////////////////////////////////////
  // (RED) Redundant Coding
  //

  // Configure RED status i.e. on/off.
  int SetREDStatus(bool enable_red) override;

  // Get RED status.
  bool REDStatus() const override;

  /////////////////////////////////////////
  // (FEC) Forward Error Correction (codec internal)
  //

  // Configure FEC status i.e. on/off.
  int SetCodecFEC(bool enabled_codec_fec) override;

  // Get FEC status.
  bool CodecFEC() const override;

  // Set target packet loss rate
  int SetPacketLossRate(int loss_rate) override;

  /////////////////////////////////////////
  //   (VAD) Voice Activity Detection
  //   and
  //   (CNG) Comfort Noise Generation
  //

  int SetVAD(bool enable_dtx = true,
             bool enable_vad = false,
             ACMVADMode mode = VADNormal) override;

  int VAD(bool* dtx_enabled,
          bool* vad_enabled,
          ACMVADMode* mode) const override;

  int RegisterVADCallback(ACMVADCallback* vad_callback) override;

  /////////////////////////////////////////
  //   Receiver
  //

  // Initialize receiver, resets codec database etc.
  int InitializeReceiver() override;

  // Get current receive frequency.
  int ReceiveFrequency() const override;

  // Get current playout frequency.
  int PlayoutFrequency() const override;

  void SetReceiveCodecs(const std::map<int, SdpAudioFormat>& codecs) override;

  bool RegisterReceiveCodec(int rtp_payload_type,
                            const SdpAudioFormat& audio_format) override;

  int RegisterReceiveCodec(const CodecInst& receive_codec) override;
  int RegisterReceiveCodec(
      const CodecInst& receive_codec,
      rtc::FunctionView<std::unique_ptr<AudioDecoder>()> isac_factory) override;

  int RegisterExternalReceiveCodec(int rtp_payload_type,
                                   AudioDecoder* external_decoder,
                                   int sample_rate_hz,
                                   int num_channels,
                                   const std::string& name) override;

  // Get current received codec.
  int ReceiveCodec(CodecInst* current_codec) const override;

  absl::optional<SdpAudioFormat> ReceiveFormat() const override;

  // Incoming packet from network parsed and ready for decode.
  int IncomingPacket(const uint8_t* incoming_payload,
                     const size_t payload_length,
                     const WebRtcRTPHeader& rtp_info) override;

  // Minimum playout delay.
  int SetMinimumPlayoutDelay(int time_ms) override;

  // Maximum playout delay.
  int SetMaximumPlayoutDelay(int time_ms) override;

  absl::optional<uint32_t> PlayoutTimestamp() override;

  int FilteredCurrentDelayMs() const override;

  int TargetDelayMs() const override;

  // Get 10 milliseconds of raw audio data to play out, and
  // automatic resample to the requested frequency if > 0.
  int PlayoutData10Ms(int desired_freq_hz,
                      AudioFrame* audio_frame,
                      bool* muted) override;

  /////////////////////////////////////////
  //   Statistics
  //

  int GetNetworkStatistics(NetworkStatistics* statistics) override;

  // If current send codec is Opus, informs it about the maximum playback rate
  // the receiver will render.
  int SetOpusMaxPlaybackRate(int frequency_hz) override;

  int EnableOpusDtx() override;

  int DisableOpusDtx() override;

  int UnregisterReceiveCodec(uint8_t payload_type) override;

  int EnableNack(size_t max_nack_list_size) override;

  void DisableNack() override;

  std::vector<uint16_t> GetNackList(int64_t round_trip_time_ms) const override;

  void GetDecodingCallStatistics(AudioDecodingCallStats* stats) const override;

  ANAStats GetANAStats() const override;

 private:
  struct InputData {
    uint32_t input_timestamp;
    const int16_t* audio;
    size_t length_per_channel;
    size_t audio_channel;
    // If a re-mix is required (up or down), this buffer will store a re-mixed
    // version of the input.
    int16_t buffer[WEBRTC_10MS_PCM_AUDIO];
  };

  // This member class writes values to the named UMA histogram, but only if
  // the value has changed since the last time (and always for the first call).
  class ChangeLogger {
   public:
    explicit ChangeLogger(const std::string& histogram_name)
        : histogram_name_(histogram_name) {}
    // Logs the new value if it is different from the last logged value, or if
    // this is the first call.
    void MaybeLog(int value);

   private:
    int last_value_ = 0;
    int first_time_ = true;
    const std::string histogram_name_;
  };

  int RegisterReceiveCodecUnlocked(
      const CodecInst& codec,
      rtc::FunctionView<std::unique_ptr<AudioDecoder>()> isac_factory)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(acm_crit_sect_);

  int Add10MsDataInternal(const AudioFrame& audio_frame, InputData* input_data)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(acm_crit_sect_);
  int Encode(const InputData& input_data)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(acm_crit_sect_);

  int InitializeReceiverSafe() RTC_EXCLUSIVE_LOCKS_REQUIRED(acm_crit_sect_);

  bool HaveValidEncoder(const char* caller_name) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(acm_crit_sect_);

  // Preprocessing of input audio, including resampling and down-mixing if
  // required, before pushing audio into encoder's buffer.
  //
  // in_frame: input audio-frame
  // ptr_out: pointer to output audio_frame. If no preprocessing is required
  //          |ptr_out| will be pointing to |in_frame|, otherwise pointing to
  //          |preprocess_frame_|.
  //
  // Return value:
  //   -1: if encountering an error.
  //    0: otherwise.
  int PreprocessToAddData(const AudioFrame& in_frame,
                          const AudioFrame** ptr_out)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(acm_crit_sect_);

  // Change required states after starting to receive the codec corresponding
  // to |index|.
  int UpdateUponReceivingCodec(int index);

  rtc::CriticalSection acm_crit_sect_;
  rtc::Buffer encode_buffer_ RTC_GUARDED_BY(acm_crit_sect_);
  uint32_t expected_codec_ts_ RTC_GUARDED_BY(acm_crit_sect_);
  uint32_t expected_in_ts_ RTC_GUARDED_BY(acm_crit_sect_);
  acm2::ACMResampler resampler_ RTC_GUARDED_BY(acm_crit_sect_);
  acm2::AcmReceiver receiver_;  // AcmReceiver has it's own internal lock.
  ChangeLogger bitrate_logger_ RTC_GUARDED_BY(acm_crit_sect_);

  std::unique_ptr<EncoderFactory> encoder_factory_
      RTC_GUARDED_BY(acm_crit_sect_);

  // Current encoder stack, either obtained from
  // encoder_factory_->rent_a_codec.RentEncoderStack or provided by a call to
  // RegisterEncoder.
  std::unique_ptr<AudioEncoder> encoder_stack_ RTC_GUARDED_BY(acm_crit_sect_);

  std::unique_ptr<AudioDecoder> isac_decoder_16k_
      RTC_GUARDED_BY(acm_crit_sect_);
  std::unique_ptr<AudioDecoder> isac_decoder_32k_
      RTC_GUARDED_BY(acm_crit_sect_);

  // This is to keep track of CN instances where we can send DTMFs.
  uint8_t previous_pltype_ RTC_GUARDED_BY(acm_crit_sect_);

  bool receiver_initialized_ RTC_GUARDED_BY(acm_crit_sect_);

  AudioFrame preprocess_frame_ RTC_GUARDED_BY(acm_crit_sect_);
  bool first_10ms_data_ RTC_GUARDED_BY(acm_crit_sect_);

  bool first_frame_ RTC_GUARDED_BY(acm_crit_sect_);
  uint32_t last_timestamp_ RTC_GUARDED_BY(acm_crit_sect_);
  uint32_t last_rtp_timestamp_ RTC_GUARDED_BY(acm_crit_sect_);

  rtc::CriticalSection callback_crit_sect_;
  AudioPacketizationCallback* packetization_callback_
      RTC_GUARDED_BY(callback_crit_sect_);
  ACMVADCallback* vad_callback_ RTC_GUARDED_BY(callback_crit_sect_);

  int codec_histogram_bins_log_[static_cast<size_t>(
      AudioEncoder::CodecType::kMaxLoggedAudioCodecTypes)];
  int number_of_consecutive_empty_packets_;
};

// Adds a codec usage sample to the histogram.
void UpdateCodecTypeHistogram(size_t codec_type) {
  RTC_HISTOGRAM_ENUMERATION(
      "WebRTC.Audio.Encoder.CodecType", static_cast<int>(codec_type),
      static_cast<int>(
          webrtc::AudioEncoder::CodecType::kMaxLoggedAudioCodecTypes));
}

// Stereo-to-mono can be used as in-place.
int DownMix(const AudioFrame& frame,
            size_t length_out_buff,
            int16_t* out_buff) {
  RTC_DCHECK_EQ(frame.num_channels_, 2);
  RTC_DCHECK_GE(length_out_buff, frame.samples_per_channel_);

  if (!frame.muted()) {
    const int16_t* frame_data = frame.data();
    for (size_t n = 0; n < frame.samples_per_channel_; ++n) {
      out_buff[n] =
          static_cast<int16_t>((static_cast<int32_t>(frame_data[2 * n]) +
                                static_cast<int32_t>(frame_data[2 * n + 1])) >>
                               1);
    }
  } else {
    std::fill(out_buff, out_buff + frame.samples_per_channel_, 0);
  }
  return 0;
}

// Mono-to-stereo can be used as in-place.
int UpMix(const AudioFrame& frame, size_t length_out_buff, int16_t* out_buff) {
  RTC_DCHECK_EQ(frame.num_channels_, 1);
  RTC_DCHECK_GE(length_out_buff, 2 * frame.samples_per_channel_);

  if (!frame.muted()) {
    const int16_t* frame_data = frame.data();
    for (size_t n = frame.samples_per_channel_; n != 0; --n) {
      size_t i = n - 1;
      int16_t sample = frame_data[i];
      out_buff[2 * i + 1] = sample;
      out_buff[2 * i] = sample;
    }
  } else {
    std::fill(out_buff, out_buff + frame.samples_per_channel_ * 2, 0);
  }
  return 0;
}

void ConvertEncodedInfoToFragmentationHeader(
    const AudioEncoder::EncodedInfo& info,
    RTPFragmentationHeader* frag) {
  if (info.redundant.empty()) {
    frag->fragmentationVectorSize = 0;
    return;
  }

  frag->VerifyAndAllocateFragmentationHeader(
      static_cast<uint16_t>(info.redundant.size()));
  frag->fragmentationVectorSize = static_cast<uint16_t>(info.redundant.size());
  size_t offset = 0;
  for (size_t i = 0; i < info.redundant.size(); ++i) {
    frag->fragmentationOffset[i] = offset;
    offset += info.redundant[i].encoded_bytes;
    frag->fragmentationLength[i] = info.redundant[i].encoded_bytes;
    frag->fragmentationTimeDiff[i] = rtc::dchecked_cast<uint16_t>(
        info.encoded_timestamp - info.redundant[i].encoded_timestamp);
    frag->fragmentationPlType[i] = info.redundant[i].payload_type;
  }
}

// Wraps a raw AudioEncoder pointer. The idea is that you can put one of these
// in a unique_ptr, to protect the contained raw pointer from being deleted
// when the unique_ptr expires. (This is of course a bad idea in general, but
// backwards compatibility.)
class RawAudioEncoderWrapper final : public AudioEncoder {
 public:
  RawAudioEncoderWrapper(AudioEncoder* enc) : enc_(enc) {}
  int SampleRateHz() const override { return enc_->SampleRateHz(); }
  size_t NumChannels() const override { return enc_->NumChannels(); }
  int RtpTimestampRateHz() const override { return enc_->RtpTimestampRateHz(); }
  size_t Num10MsFramesInNextPacket() const override {
    return enc_->Num10MsFramesInNextPacket();
  }
  size_t Max10MsFramesInAPacket() const override {
    return enc_->Max10MsFramesInAPacket();
  }
  int GetTargetBitrate() const override { return enc_->GetTargetBitrate(); }
  EncodedInfo EncodeImpl(uint32_t rtp_timestamp,
                         rtc::ArrayView<const int16_t> audio,
                         rtc::Buffer* encoded) override {
    return enc_->Encode(rtp_timestamp, audio, encoded);
  }
  void Reset() override { return enc_->Reset(); }
  bool SetFec(bool enable) override { return enc_->SetFec(enable); }
  bool SetDtx(bool enable) override { return enc_->SetDtx(enable); }
  bool SetApplication(Application application) override {
    return enc_->SetApplication(application);
  }
  void SetMaxPlaybackRate(int frequency_hz) override {
    return enc_->SetMaxPlaybackRate(frequency_hz);
  }

 private:
  AudioEncoder* enc_;
};

// Return false on error.
bool CreateSpeechEncoderIfNecessary(EncoderFactory* ef) {
  auto* sp = ef->codec_manager.GetStackParams();
  if (sp->speech_encoder) {
    // Do nothing; we already have a speech encoder.
  } else if (ef->codec_manager.GetCodecInst()) {
    RTC_DCHECK(!ef->external_speech_encoder);
    // We have no speech encoder, but we have a specification for making one.
    std::unique_ptr<AudioEncoder> enc =
        ef->rent_a_codec.RentEncoder(*ef->codec_manager.GetCodecInst());
    if (!enc)
      return false;  // Encoder spec was bad.
    sp->speech_encoder = std::move(enc);
  } else if (ef->external_speech_encoder) {
    RTC_DCHECK(!ef->codec_manager.GetCodecInst());
    // We have an external speech encoder.
    sp->speech_encoder = std::unique_ptr<AudioEncoder>(
        new RawAudioEncoderWrapper(ef->external_speech_encoder));
  }
  return true;
}

void AudioCodingModuleImpl::ChangeLogger::MaybeLog(int value) {
  if (value != last_value_ || first_time_) {
    first_time_ = false;
    last_value_ = value;
    RTC_HISTOGRAM_COUNTS_SPARSE_100(histogram_name_, value);
  }
}

AudioCodingModuleImpl::AudioCodingModuleImpl(
    const AudioCodingModule::Config& config)
    : expected_codec_ts_(0xD87F3F9F),
      expected_in_ts_(0xD87F3F9F),
      receiver_(config),
      bitrate_logger_("WebRTC.Audio.TargetBitrateInKbps"),
      encoder_factory_(new EncoderFactory),
      encoder_stack_(nullptr),
      previous_pltype_(255),
      receiver_initialized_(false),
      first_10ms_data_(false),
      first_frame_(true),
      packetization_callback_(NULL),
      vad_callback_(NULL),
      codec_histogram_bins_log_(),
      number_of_consecutive_empty_packets_(0) {
  if (InitializeReceiverSafe() < 0) {
    RTC_LOG(LS_ERROR) << "Cannot initialize receiver";
  }
  RTC_LOG(LS_INFO) << "Created";
}

AudioCodingModuleImpl::~AudioCodingModuleImpl() = default;

int32_t AudioCodingModuleImpl::Encode(const InputData& input_data) {
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
      first_frame_ ? input_data.input_timestamp
                   : last_rtp_timestamp_ +
                         rtc::CheckedDivExact(
                             input_data.input_timestamp - last_timestamp_,
                             static_cast<uint32_t>(rtc::CheckedDivExact(
                                 encoder_stack_->SampleRateHz(),
                                 encoder_stack_->RtpTimestampRateHz())));
  last_timestamp_ = input_data.input_timestamp;
  last_rtp_timestamp_ = rtp_timestamp;
  first_frame_ = false;

  // Clear the buffer before reuse - encoded data will get appended.
  encode_buffer_.Clear();
  encoded_info = encoder_stack_->Encode(
      rtp_timestamp,
      rtc::ArrayView<const int16_t>(
          input_data.audio,
          input_data.audio_channel * input_data.length_per_channel),
      &encode_buffer_);

  bitrate_logger_.MaybeLog(encoder_stack_->GetTargetBitrate() / 1000);
  if (encode_buffer_.size() == 0 && !encoded_info.send_even_if_empty) {
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

  RTPFragmentationHeader my_fragmentation;
  ConvertEncodedInfoToFragmentationHeader(encoded_info, &my_fragmentation);
  FrameType frame_type;
  if (encode_buffer_.size() == 0 && encoded_info.send_even_if_empty) {
    frame_type = kEmptyFrame;
    encoded_info.payload_type = previous_pltype;
  } else {
    RTC_DCHECK_GT(encode_buffer_.size(), 0);
    frame_type = encoded_info.speech ? kAudioFrameSpeech : kAudioFrameCN;
  }

  {
    rtc::CritScope lock(&callback_crit_sect_);
    if (packetization_callback_) {
      packetization_callback_->SendData(
          frame_type, encoded_info.payload_type, encoded_info.encoded_timestamp,
          encode_buffer_.data(), encode_buffer_.size(),
          my_fragmentation.fragmentationVectorSize > 0 ? &my_fragmentation
                                                       : nullptr);
    }

    if (vad_callback_) {
      // Callback with VAD decision.
      vad_callback_->InFrameType(frame_type);
    }
  }
  previous_pltype_ = encoded_info.payload_type;
  return static_cast<int32_t>(encode_buffer_.size());
}

/////////////////////////////////////////
//   Sender
//

// Can be called multiple times for Codec, CNG, RED.
int AudioCodingModuleImpl::RegisterSendCodec(const CodecInst& send_codec) {
  rtc::CritScope lock(&acm_crit_sect_);
  if (!encoder_factory_->codec_manager.RegisterEncoder(send_codec)) {
    return -1;
  }
  if (encoder_factory_->codec_manager.GetCodecInst()) {
    encoder_factory_->external_speech_encoder = nullptr;
  }
  if (!CreateSpeechEncoderIfNecessary(encoder_factory_.get())) {
    return -1;
  }
  auto* sp = encoder_factory_->codec_manager.GetStackParams();
  if (sp->speech_encoder)
    encoder_stack_ = encoder_factory_->rent_a_codec.RentEncoderStack(sp);
  return 0;
}

void AudioCodingModuleImpl::RegisterExternalSendCodec(
    AudioEncoder* external_speech_encoder) {
  rtc::CritScope lock(&acm_crit_sect_);
  encoder_factory_->codec_manager.UnsetCodecInst();
  encoder_factory_->external_speech_encoder = external_speech_encoder;
  RTC_CHECK(CreateSpeechEncoderIfNecessary(encoder_factory_.get()));
  auto* sp = encoder_factory_->codec_manager.GetStackParams();
  RTC_CHECK(sp->speech_encoder);
  encoder_stack_ = encoder_factory_->rent_a_codec.RentEncoderStack(sp);
}

void AudioCodingModuleImpl::ModifyEncoder(
    rtc::FunctionView<void(std::unique_ptr<AudioEncoder>*)> modifier) {
  rtc::CritScope lock(&acm_crit_sect_);

  // Wipe the encoder factory, so that everything that relies on it will fail.
  // We don't want the complexity of supporting swapping back and forth.
  if (encoder_factory_) {
    encoder_factory_.reset();
    RTC_CHECK(!encoder_stack_);  // Ensure we hadn't started using the factory.
  }

  modifier(&encoder_stack_);
}

// Get current send codec.
absl::optional<CodecInst> AudioCodingModuleImpl::SendCodec() const {
  rtc::CritScope lock(&acm_crit_sect_);
  if (encoder_factory_) {
    auto* ci = encoder_factory_->codec_manager.GetCodecInst();
    if (ci) {
      return *ci;
    }
    CreateSpeechEncoderIfNecessary(encoder_factory_.get());
    const std::unique_ptr<AudioEncoder>& enc =
        encoder_factory_->codec_manager.GetStackParams()->speech_encoder;
    if (enc) {
      return acm2::CodecManager::ForgeCodecInst(enc.get());
    }
    return absl::nullopt;
  } else {
    return encoder_stack_
               ? absl::optional<CodecInst>(
                     acm2::CodecManager::ForgeCodecInst(encoder_stack_.get()))
               : absl::nullopt;
  }
}

void AudioCodingModuleImpl::SetBitRate(int bitrate_bps) {
  rtc::CritScope lock(&acm_crit_sect_);
  if (encoder_stack_) {
    encoder_stack_->OnReceivedUplinkBandwidth(bitrate_bps, absl::nullopt);
  }
}

// Register a transport callback which will be called to deliver
// the encoded buffers.
int AudioCodingModuleImpl::RegisterTransportCallback(
    AudioPacketizationCallback* transport) {
  rtc::CritScope lock(&callback_crit_sect_);
  packetization_callback_ = transport;
  return 0;
}

// Add 10MS of raw (PCM) audio data to the encoder.
int AudioCodingModuleImpl::Add10MsData(const AudioFrame& audio_frame) {
  InputData input_data;
  rtc::CritScope lock(&acm_crit_sect_);
  int r = Add10MsDataInternal(audio_frame, &input_data);
  return r < 0 ? r : Encode(input_data);
}

int AudioCodingModuleImpl::Add10MsDataInternal(const AudioFrame& audio_frame,
                                               InputData* input_data) {
  if (audio_frame.samples_per_channel_ == 0) {
    assert(false);
    RTC_LOG(LS_ERROR) << "Cannot Add 10 ms audio, payload length is zero";
    return -1;
  }

  if (audio_frame.sample_rate_hz_ > 48000) {
    assert(false);
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

  if (audio_frame.num_channels_ != 1 && audio_frame.num_channels_ != 2) {
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

  if (!same_num_channels) {
    if (ptr_frame->num_channels_ == 1) {
      if (UpMix(*ptr_frame, WEBRTC_10MS_PCM_AUDIO, input_data->buffer) < 0)
        return -1;
    } else {
      if (DownMix(*ptr_frame, WEBRTC_10MS_PCM_AUDIO, input_data->buffer) < 0)
        return -1;
    }
  }

  // When adding data to encoders this pointer is pointing to an audio buffer
  // with correct number of channels.
  const int16_t* ptr_audio = ptr_frame->data();

  // For pushing data to primary, point the |ptr_audio| to correct buffer.
  if (!same_num_channels)
    ptr_audio = input_data->buffer;

  // TODO(yujo): Skip encode of muted frames.
  input_data->input_timestamp = ptr_frame->timestamp_;
  input_data->audio = ptr_audio;
  input_data->length_per_channel = ptr_frame->samples_per_channel_;
  input_data->audio_channel = current_num_channels;

  return 0;
}

// Perform a resampling and down-mix if required. We down-mix only if
// encoder is mono and input is stereo. In case of dual-streaming, both
// encoders has to be mono for down-mix to take place.
// |*ptr_out| will point to the pre-processed audio-frame. If no pre-processing
// is required, |*ptr_out| points to |in_frame|.
// TODO(yujo): Make this more efficient for muted frames.
int AudioCodingModuleImpl::PreprocessToAddData(const AudioFrame& in_frame,
                                               const AudioFrame** ptr_out) {
  const bool resample =
      in_frame.sample_rate_hz_ != encoder_stack_->SampleRateHz();

  // This variable is true if primary codec and secondary codec (if exists)
  // are both mono and input is stereo.
  // TODO(henrik.lundin): This condition should probably be
  //   in_frame.num_channels_ > encoder_stack_->NumChannels()
  const bool down_mix =
      in_frame.num_channels_ == 2 && encoder_stack_->NumChannels() == 1;

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

  if (!down_mix && !resample) {
    // No pre-processing is required.
    if (expected_in_ts_ == expected_codec_ts_) {
      // If we've never resampled, we can use the input frame as-is
      *ptr_out = &in_frame;
    } else {
      // Otherwise we'll need to alter the timestamp. Since in_frame is const,
      // we'll have to make a copy of it.
      preprocess_frame_.CopyFrom(in_frame);
      preprocess_frame_.timestamp_ = expected_codec_ts_;
      *ptr_out = &preprocess_frame_;
    }

    expected_in_ts_ += static_cast<uint32_t>(in_frame.samples_per_channel_);
    expected_codec_ts_ += static_cast<uint32_t>(in_frame.samples_per_channel_);
    return 0;
  }

  *ptr_out = &preprocess_frame_;
  preprocess_frame_.num_channels_ = in_frame.num_channels_;
  int16_t audio[WEBRTC_10MS_PCM_AUDIO];
  const int16_t* src_ptr_audio = in_frame.data();
  if (down_mix) {
    // If a resampling is required the output of a down-mix is written into a
    // local buffer, otherwise, it will be written to the output frame.
    int16_t* dest_ptr_audio =
        resample ? audio : preprocess_frame_.mutable_data();
    if (DownMix(in_frame, WEBRTC_10MS_PCM_AUDIO, dest_ptr_audio) < 0)
      return -1;
    preprocess_frame_.num_channels_ = 1;
    // Set the input of the resampler is the down-mixed signal.
    src_ptr_audio = audio;
  }

  preprocess_frame_.timestamp_ = expected_codec_ts_;
  preprocess_frame_.samples_per_channel_ = in_frame.samples_per_channel_;
  preprocess_frame_.sample_rate_hz_ = in_frame.sample_rate_hz_;
  // If it is required, we have to do a resampling.
  if (resample) {
    // The result of the resampler is written to output frame.
    int16_t* dest_ptr_audio = preprocess_frame_.mutable_data();

    int samples_per_channel = resampler_.Resample10Msec(
        src_ptr_audio, in_frame.sample_rate_hz_, encoder_stack_->SampleRateHz(),
        preprocess_frame_.num_channels_, AudioFrame::kMaxDataSizeSamples,
        dest_ptr_audio);

    if (samples_per_channel < 0) {
      RTC_LOG(LS_ERROR) << "Cannot add 10 ms audio, resampling failed";
      return -1;
    }
    preprocess_frame_.samples_per_channel_ =
        static_cast<size_t>(samples_per_channel);
    preprocess_frame_.sample_rate_hz_ = encoder_stack_->SampleRateHz();
  }

  expected_codec_ts_ +=
      static_cast<uint32_t>(preprocess_frame_.samples_per_channel_);
  expected_in_ts_ += static_cast<uint32_t>(in_frame.samples_per_channel_);

  return 0;
}

/////////////////////////////////////////
//   (RED) Redundant Coding
//

bool AudioCodingModuleImpl::REDStatus() const {
  rtc::CritScope lock(&acm_crit_sect_);
  return encoder_factory_->codec_manager.GetStackParams()->use_red;
}

// Configure RED status i.e on/off.
int AudioCodingModuleImpl::SetREDStatus(bool enable_red) {
#ifdef WEBRTC_CODEC_RED
  rtc::CritScope lock(&acm_crit_sect_);
  CreateSpeechEncoderIfNecessary(encoder_factory_.get());
  if (!encoder_factory_->codec_manager.SetCopyRed(enable_red)) {
    return -1;
  }
  auto* sp = encoder_factory_->codec_manager.GetStackParams();
  if (sp->speech_encoder)
    encoder_stack_ = encoder_factory_->rent_a_codec.RentEncoderStack(sp);
  return 0;
#else
  RTC_LOG(LS_WARNING) << "  WEBRTC_CODEC_RED is undefined";
  return -1;
#endif
}

/////////////////////////////////////////
//   (FEC) Forward Error Correction (codec internal)
//

bool AudioCodingModuleImpl::CodecFEC() const {
  rtc::CritScope lock(&acm_crit_sect_);
  return encoder_factory_->codec_manager.GetStackParams()->use_codec_fec;
}

int AudioCodingModuleImpl::SetCodecFEC(bool enable_codec_fec) {
  rtc::CritScope lock(&acm_crit_sect_);
  CreateSpeechEncoderIfNecessary(encoder_factory_.get());
  if (!encoder_factory_->codec_manager.SetCodecFEC(enable_codec_fec)) {
    return -1;
  }
  auto* sp = encoder_factory_->codec_manager.GetStackParams();
  if (sp->speech_encoder)
    encoder_stack_ = encoder_factory_->rent_a_codec.RentEncoderStack(sp);
  if (enable_codec_fec) {
    return sp->use_codec_fec ? 0 : -1;
  } else {
    RTC_DCHECK(!sp->use_codec_fec);
    return 0;
  }
}

int AudioCodingModuleImpl::SetPacketLossRate(int loss_rate) {
  rtc::CritScope lock(&acm_crit_sect_);
  if (HaveValidEncoder("SetPacketLossRate")) {
    encoder_stack_->OnReceivedUplinkPacketLossFraction(loss_rate / 100.0);
  }
  return 0;
}

/////////////////////////////////////////
//   (VAD) Voice Activity Detection
//
int AudioCodingModuleImpl::SetVAD(bool enable_dtx,
                                  bool enable_vad,
                                  ACMVADMode mode) {
  // Note: |enable_vad| is not used; VAD is enabled based on the DTX setting.
  RTC_DCHECK_EQ(enable_dtx, enable_vad);
  rtc::CritScope lock(&acm_crit_sect_);
  CreateSpeechEncoderIfNecessary(encoder_factory_.get());
  if (!encoder_factory_->codec_manager.SetVAD(enable_dtx, mode)) {
    return -1;
  }
  auto* sp = encoder_factory_->codec_manager.GetStackParams();
  if (sp->speech_encoder)
    encoder_stack_ = encoder_factory_->rent_a_codec.RentEncoderStack(sp);
  return 0;
}

// Get VAD/DTX settings.
int AudioCodingModuleImpl::VAD(bool* dtx_enabled,
                               bool* vad_enabled,
                               ACMVADMode* mode) const {
  rtc::CritScope lock(&acm_crit_sect_);
  const auto* sp = encoder_factory_->codec_manager.GetStackParams();
  *dtx_enabled = *vad_enabled = sp->use_cng;
  *mode = sp->vad_mode;
  return 0;
}

/////////////////////////////////////////
//   Receiver
//

int AudioCodingModuleImpl::InitializeReceiver() {
  rtc::CritScope lock(&acm_crit_sect_);
  return InitializeReceiverSafe();
}

// Initialize receiver, resets codec database etc.
int AudioCodingModuleImpl::InitializeReceiverSafe() {
  // If the receiver is already initialized then we want to destroy any
  // existing decoders. After a call to this function, we should have a clean
  // start-up.
  if (receiver_initialized_)
    receiver_.RemoveAllCodecs();
  receiver_.ResetInitialDelay();
  receiver_.SetMinimumDelay(0);
  receiver_.SetMaximumDelay(0);
  receiver_.FlushBuffers();

  receiver_initialized_ = true;
  return 0;
}

// Get current receive frequency.
int AudioCodingModuleImpl::ReceiveFrequency() const {
  const auto last_packet_sample_rate = receiver_.last_packet_sample_rate_hz();
  return last_packet_sample_rate ? *last_packet_sample_rate
                                 : receiver_.last_output_sample_rate_hz();
}

// Get current playout frequency.
int AudioCodingModuleImpl::PlayoutFrequency() const {
  return receiver_.last_output_sample_rate_hz();
}

void AudioCodingModuleImpl::SetReceiveCodecs(
    const std::map<int, SdpAudioFormat>& codecs) {
  rtc::CritScope lock(&acm_crit_sect_);
  receiver_.SetCodecs(codecs);
}

bool AudioCodingModuleImpl::RegisterReceiveCodec(
    int rtp_payload_type,
    const SdpAudioFormat& audio_format) {
  rtc::CritScope lock(&acm_crit_sect_);
  RTC_DCHECK(receiver_initialized_);

  if (!acm2::RentACodec::IsPayloadTypeValid(rtp_payload_type)) {
    RTC_LOG_F(LS_ERROR) << "Invalid payload-type " << rtp_payload_type
                        << " for decoder.";
    return false;
  }

  return receiver_.AddCodec(rtp_payload_type, audio_format);
}

int AudioCodingModuleImpl::RegisterReceiveCodec(const CodecInst& codec) {
  rtc::CritScope lock(&acm_crit_sect_);
  auto* ef = encoder_factory_.get();
  return RegisterReceiveCodecUnlocked(
      codec, [&] { return ef->rent_a_codec.RentIsacDecoder(codec.plfreq); });
}

int AudioCodingModuleImpl::RegisterReceiveCodec(
    const CodecInst& codec,
    rtc::FunctionView<std::unique_ptr<AudioDecoder>()> isac_factory) {
  rtc::CritScope lock(&acm_crit_sect_);
  return RegisterReceiveCodecUnlocked(codec, isac_factory);
}

int AudioCodingModuleImpl::RegisterReceiveCodecUnlocked(
    const CodecInst& codec,
    rtc::FunctionView<std::unique_ptr<AudioDecoder>()> isac_factory) {
  RTC_DCHECK(receiver_initialized_);
  if (codec.channels > 2) {
    RTC_LOG_F(LS_ERROR) << "Unsupported number of channels: " << codec.channels;
    return -1;
  }

  auto codec_id = acm2::RentACodec::CodecIdByParams(codec.plname, codec.plfreq,
                                                    codec.channels);
  if (!codec_id) {
    RTC_LOG_F(LS_ERROR)
        << "Wrong codec params to be registered as receive codec";
    return -1;
  }
  auto codec_index = acm2::RentACodec::CodecIndexFromId(*codec_id);
  RTC_CHECK(codec_index) << "Invalid codec ID: " << static_cast<int>(*codec_id);

  // Check if the payload-type is valid.
  if (!acm2::RentACodec::IsPayloadTypeValid(codec.pltype)) {
    RTC_LOG_F(LS_ERROR) << "Invalid payload type " << codec.pltype << " for "
                        << codec.plname;
    return -1;
  }

  AudioDecoder* isac_decoder = nullptr;
  if (STR_CASE_CMP(codec.plname, "isac") == 0) {
    std::unique_ptr<AudioDecoder>& saved_isac_decoder =
        codec.plfreq == 16000 ? isac_decoder_16k_ : isac_decoder_32k_;
    if (!saved_isac_decoder) {
      saved_isac_decoder = isac_factory();
    }
    isac_decoder = saved_isac_decoder.get();
  }
  return receiver_.AddCodec(*codec_index, codec.pltype, codec.channels,
                            codec.plfreq, isac_decoder, codec.plname);
}

int AudioCodingModuleImpl::RegisterExternalReceiveCodec(
    int rtp_payload_type,
    AudioDecoder* external_decoder,
    int sample_rate_hz,
    int num_channels,
    const std::string& name) {
  rtc::CritScope lock(&acm_crit_sect_);
  RTC_DCHECK(receiver_initialized_);
  if (num_channels > 2 || num_channels < 0) {
    RTC_LOG_F(LS_ERROR) << "Unsupported number of channels: " << num_channels;
    return -1;
  }

  // Check if the payload-type is valid.
  if (!acm2::RentACodec::IsPayloadTypeValid(rtp_payload_type)) {
    RTC_LOG_F(LS_ERROR) << "Invalid payload-type " << rtp_payload_type
                        << " for external decoder.";
    return -1;
  }

  return receiver_.AddCodec(-1 /* external */, rtp_payload_type, num_channels,
                            sample_rate_hz, external_decoder, name);
}

// Get current received codec.
int AudioCodingModuleImpl::ReceiveCodec(CodecInst* current_codec) const {
  rtc::CritScope lock(&acm_crit_sect_);
  return receiver_.LastAudioCodec(current_codec);
}

absl::optional<SdpAudioFormat> AudioCodingModuleImpl::ReceiveFormat() const {
  rtc::CritScope lock(&acm_crit_sect_);
  return receiver_.LastAudioFormat();
}

// Incoming packet from network parsed and ready for decode.
int AudioCodingModuleImpl::IncomingPacket(const uint8_t* incoming_payload,
                                          const size_t payload_length,
                                          const WebRtcRTPHeader& rtp_header) {
  RTC_DCHECK_EQ(payload_length == 0, incoming_payload == nullptr);
  return receiver_.InsertPacket(
      rtp_header,
      rtc::ArrayView<const uint8_t>(incoming_payload, payload_length));
}

// Minimum playout delay (Used for lip-sync).
int AudioCodingModuleImpl::SetMinimumPlayoutDelay(int time_ms) {
  if ((time_ms < 0) || (time_ms > 10000)) {
    RTC_LOG(LS_ERROR) << "Delay must be in the range of 0-10000 milliseconds.";
    return -1;
  }
  return receiver_.SetMinimumDelay(time_ms);
}

int AudioCodingModuleImpl::SetMaximumPlayoutDelay(int time_ms) {
  if ((time_ms < 0) || (time_ms > 10000)) {
    RTC_LOG(LS_ERROR) << "Delay must be in the range of 0-10000 milliseconds.";
    return -1;
  }
  return receiver_.SetMaximumDelay(time_ms);
}

// Get 10 milliseconds of raw audio data to play out.
// Automatic resample to the requested frequency.
int AudioCodingModuleImpl::PlayoutData10Ms(int desired_freq_hz,
                                           AudioFrame* audio_frame,
                                           bool* muted) {
  // GetAudio always returns 10 ms, at the requested sample rate.
  if (receiver_.GetAudio(desired_freq_hz, audio_frame, muted) != 0) {
    RTC_LOG(LS_ERROR) << "PlayoutData failed, RecOut Failed";
    return -1;
  }
  return 0;
}

/////////////////////////////////////////
//   Statistics
//

// TODO(turajs) change the return value to void. Also change the corresponding
// NetEq function.
int AudioCodingModuleImpl::GetNetworkStatistics(NetworkStatistics* statistics) {
  receiver_.GetNetworkStatistics(statistics);
  return 0;
}

int AudioCodingModuleImpl::RegisterVADCallback(ACMVADCallback* vad_callback) {
  RTC_LOG(LS_VERBOSE) << "RegisterVADCallback()";
  rtc::CritScope lock(&callback_crit_sect_);
  vad_callback_ = vad_callback;
  return 0;
}

// Informs Opus encoder of the maximum playback rate the receiver will render.
int AudioCodingModuleImpl::SetOpusMaxPlaybackRate(int frequency_hz) {
  rtc::CritScope lock(&acm_crit_sect_);
  if (!HaveValidEncoder("SetOpusMaxPlaybackRate")) {
    return -1;
  }
  encoder_stack_->SetMaxPlaybackRate(frequency_hz);
  return 0;
}

int AudioCodingModuleImpl::EnableOpusDtx() {
  rtc::CritScope lock(&acm_crit_sect_);
  if (!HaveValidEncoder("EnableOpusDtx")) {
    return -1;
  }
  return encoder_stack_->SetDtx(true) ? 0 : -1;
}

int AudioCodingModuleImpl::DisableOpusDtx() {
  rtc::CritScope lock(&acm_crit_sect_);
  if (!HaveValidEncoder("DisableOpusDtx")) {
    return -1;
  }
  return encoder_stack_->SetDtx(false) ? 0 : -1;
}

absl::optional<uint32_t> AudioCodingModuleImpl::PlayoutTimestamp() {
  return receiver_.GetPlayoutTimestamp();
}

int AudioCodingModuleImpl::FilteredCurrentDelayMs() const {
  return receiver_.FilteredCurrentDelayMs();
}

int AudioCodingModuleImpl::TargetDelayMs() const {
  return receiver_.TargetDelayMs();
}

bool AudioCodingModuleImpl::HaveValidEncoder(const char* caller_name) const {
  if (!encoder_stack_) {
    RTC_LOG(LS_ERROR) << caller_name << " failed: No send codec is registered.";
    return false;
  }
  return true;
}

int AudioCodingModuleImpl::UnregisterReceiveCodec(uint8_t payload_type) {
  return receiver_.RemoveCodec(payload_type);
}

int AudioCodingModuleImpl::EnableNack(size_t max_nack_list_size) {
  return receiver_.EnableNack(max_nack_list_size);
}

void AudioCodingModuleImpl::DisableNack() {
  receiver_.DisableNack();
}

std::vector<uint16_t> AudioCodingModuleImpl::GetNackList(
    int64_t round_trip_time_ms) const {
  return receiver_.GetNackList(round_trip_time_ms);
}

void AudioCodingModuleImpl::GetDecodingCallStatistics(
    AudioDecodingCallStats* call_stats) const {
  receiver_.GetDecodingCallStatistics(call_stats);
}

ANAStats AudioCodingModuleImpl::GetANAStats() const {
  rtc::CritScope lock(&acm_crit_sect_);
  if (encoder_stack_)
    return encoder_stack_->GetANAStats();
  // If no encoder is set, return default stats.
  return ANAStats();
}

}  // namespace

AudioCodingModule::Config::Config(
    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory)
    : neteq_config(),
      clock(Clock::GetRealTimeClock()),
      decoder_factory(decoder_factory) {
  // Post-decode VAD is disabled by default in NetEq, however, Audio
  // Conference Mixer relies on VAD decisions and fails without them.
  neteq_config.enable_post_decode_vad = true;
}

AudioCodingModule::Config::Config(const Config&) = default;
AudioCodingModule::Config::~Config() = default;

AudioCodingModule* AudioCodingModule::Create(const Config& config) {
  return new AudioCodingModuleImpl(config);
}

int AudioCodingModule::NumberOfCodecs() {
  return static_cast<int>(acm2::RentACodec::NumberOfCodecs());
}

int AudioCodingModule::Codec(int list_id, CodecInst* codec) {
  auto codec_id = acm2::RentACodec::CodecIdFromIndex(list_id);
  if (!codec_id)
    return -1;
  auto ci = acm2::RentACodec::CodecInstById(*codec_id);
  if (!ci)
    return -1;
  *codec = *ci;
  return 0;
}

int AudioCodingModule::Codec(const char* payload_name,
                             CodecInst* codec,
                             int sampling_freq_hz,
                             size_t channels) {
  absl::optional<CodecInst> ci = acm2::RentACodec::CodecInstByParams(
      payload_name, sampling_freq_hz, channels);
  if (ci) {
    *codec = *ci;
    return 0;
  } else {
    // We couldn't find a matching codec, so set the parameters to unacceptable
    // values and return.
    codec->plname[0] = '\0';
    codec->pltype = -1;
    codec->pacsize = 0;
    codec->rate = 0;
    codec->plfreq = 0;
    return -1;
  }
}

int AudioCodingModule::Codec(const char* payload_name,
                             int sampling_freq_hz,
                             size_t channels) {
  absl::optional<acm2::RentACodec::CodecId> ci =
      acm2::RentACodec::CodecIdByParams(payload_name, sampling_freq_hz,
                                        channels);
  if (!ci)
    return -1;
  absl::optional<int> i = acm2::RentACodec::CodecIndexFromId(*ci);
  return i ? *i : -1;
}

}  // namespace webrtc
