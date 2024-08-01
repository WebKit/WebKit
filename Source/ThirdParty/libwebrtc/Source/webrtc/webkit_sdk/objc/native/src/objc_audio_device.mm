/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "objc_audio_device.h"
#include "objc_audio_device_delegate.h"

#import "components/audio/RTCAudioDevice.h"
#include "modules/audio_device/fine_audio_buffer.h"

#include "api/task_queue/default_task_queue_factory.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/time_utils.h"

namespace {

webrtc::AudioParameters RecordParameters(id<RTC_OBJC_TYPE(RTCAudioDevice)> audio_device) {
  const double sample_rate = static_cast<int>([audio_device deviceInputSampleRate]);
  const size_t channels = static_cast<size_t>([audio_device inputNumberOfChannels]);
  const size_t frames_per_buffer =
      static_cast<size_t>(sample_rate * [audio_device inputIOBufferDuration] + .5);
  return webrtc::AudioParameters(sample_rate, channels, frames_per_buffer);
}

webrtc::AudioParameters PlayoutParameters(id<RTC_OBJC_TYPE(RTCAudioDevice)> audio_device) {
  const double sample_rate = static_cast<int>([audio_device deviceOutputSampleRate]);
  const size_t channels = static_cast<size_t>([audio_device outputNumberOfChannels]);
  const size_t frames_per_buffer =
      static_cast<size_t>(sample_rate * [audio_device outputIOBufferDuration] + .5);
  return webrtc::AudioParameters(sample_rate, channels, frames_per_buffer);
}

}  // namespace

namespace webrtc {
namespace objc_adm {

ObjCAudioDeviceModule::ObjCAudioDeviceModule(id<RTC_OBJC_TYPE(RTCAudioDevice)> audio_device)
    : audio_device_(audio_device), task_queue_factory_(CreateDefaultTaskQueueFactory()) {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK(audio_device_);
  thread_checker_.Detach();
  io_playout_thread_checker_.Detach();
  io_record_thread_checker_.Detach();
}

ObjCAudioDeviceModule::~ObjCAudioDeviceModule() {
  RTC_DLOG_F(LS_VERBOSE) << "";
}

int32_t ObjCAudioDeviceModule::RegisterAudioCallback(AudioTransport* audioCallback) {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK(audio_device_buffer_);
  return audio_device_buffer_->RegisterAudioCallback(audioCallback);
}

int32_t ObjCAudioDeviceModule::Init() {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);

  if (Initialized()) {
    RTC_LOG_F(LS_INFO) << "Already initialized";
    return 0;
  }
  io_playout_thread_checker_.Detach();
  io_record_thread_checker_.Detach();

  thread_ = rtc::Thread::Current();
  audio_device_buffer_.reset(new webrtc::AudioDeviceBuffer(task_queue_factory_.get()));

  if (![audio_device_ isInitialized]) {
    if (audio_device_delegate_ == nil) {
      audio_device_delegate_ = [[ObjCAudioDeviceDelegate alloc]
          initWithAudioDeviceModule:rtc::scoped_refptr<ObjCAudioDeviceModule>(this)
                  audioDeviceThread:thread_];
    }

    if (![audio_device_ initializeWithDelegate:audio_device_delegate_]) {
      RTC_LOG_F(LS_WARNING) << "Failed to initialize audio device";
      [audio_device_delegate_ resetAudioDeviceModule];
      audio_device_delegate_ = nil;
      return -1;
    }
  }

  playout_parameters_.reset([audio_device_delegate_ preferredOutputSampleRate], 1);
  UpdateOutputAudioDeviceBuffer();

  record_parameters_.reset([audio_device_delegate_ preferredInputSampleRate], 1);
  UpdateInputAudioDeviceBuffer();

  is_initialized_ = true;

  RTC_LOG_F(LS_INFO) << "Did initialize";
  return 0;
}

int32_t ObjCAudioDeviceModule::Terminate() {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);

  if (!Initialized()) {
    RTC_LOG_F(LS_INFO) << "Not initialized";
    return 0;
  }

  if ([audio_device_ isInitialized]) {
    if (![audio_device_ terminateDevice]) {
      RTC_LOG_F(LS_ERROR) << "Failed to terminate audio device";
      return -1;
    }
  }

  if (audio_device_delegate_ != nil) {
    [audio_device_delegate_ resetAudioDeviceModule];
    audio_device_delegate_ = nil;
  }

  is_initialized_ = false;
  is_playout_initialized_ = false;
  is_recording_initialized_ = false;
  thread_ = nullptr;

  RTC_LOG_F(LS_INFO) << "Did terminate";
  return 0;
}

bool ObjCAudioDeviceModule::Initialized() const {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  return is_initialized_ && [audio_device_ isInitialized];
}

int32_t ObjCAudioDeviceModule::PlayoutIsAvailable(bool* available) {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  *available = Initialized();
  return 0;
}

bool ObjCAudioDeviceModule::PlayoutIsInitialized() const {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  return Initialized() && is_playout_initialized_ && [audio_device_ isPlayoutInitialized];
}

int32_t ObjCAudioDeviceModule::InitPlayout() {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (!Initialized()) {
    return -1;
  }
  if (PlayoutIsInitialized()) {
    return 0;
  }
  RTC_DCHECK(!playing_.load());

  if (![audio_device_ isPlayoutInitialized]) {
    if (![audio_device_ initializePlayout]) {
      RTC_LOG_F(LS_ERROR) << "Failed to initialize audio device playout";
      return -1;
    }
  }

  if (UpdateAudioParameters(playout_parameters_, PlayoutParameters(audio_device_))) {
    UpdateOutputAudioDeviceBuffer();
  }

  is_playout_initialized_ = true;
  RTC_LOG_F(LS_INFO) << "Did initialize playout";
  return 0;
}

bool ObjCAudioDeviceModule::Playing() const {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  return playing_.load() && [audio_device_ isPlaying];
}

int32_t ObjCAudioDeviceModule::StartPlayout() {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (!PlayoutIsInitialized()) {
    return -1;
  }
  if (Playing()) {
    return 0;
  }

  audio_device_buffer_->StartPlayout();
  if (playout_fine_audio_buffer_) {
    playout_fine_audio_buffer_->ResetPlayout();
  }
  if (![audio_device_ startPlayout]) {
    RTC_LOG_F(LS_ERROR) << "Failed to start audio device playout";
    return -1;
  }
  playing_.store(true, std::memory_order_release);
  RTC_LOG_F(LS_INFO) << "Did start playout";
  return 0;
}

int32_t ObjCAudioDeviceModule::StopPlayout() {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);

  if (![audio_device_ stopPlayout]) {
    RTC_LOG_F(LS_WARNING) << "Failed to stop playout";
    return -1;
  }

  audio_device_buffer_->StopPlayout();
  playing_.store(false, std::memory_order_release);
  RTC_LOG_F(LS_INFO) << "Did stop playout";
  return 0;
}

int32_t ObjCAudioDeviceModule::PlayoutDelay(uint16_t* delayMS) const {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  *delayMS = static_cast<uint16_t>(rtc::SafeClamp<int>(
      cached_playout_delay_ms_.load(), 0, std::numeric_limits<uint16_t>::max()));
  return 0;
}

int32_t ObjCAudioDeviceModule::RecordingIsAvailable(bool* available) {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  *available = Initialized();
  return 0;
}

bool ObjCAudioDeviceModule::RecordingIsInitialized() const {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  return Initialized() && is_recording_initialized_ && [audio_device_ isRecordingInitialized];
}

int32_t ObjCAudioDeviceModule::InitRecording() {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (!Initialized()) {
    return -1;
  }
  if (RecordingIsInitialized()) {
    return 0;
  }
  RTC_DCHECK(!recording_.load());

  if (![audio_device_ isRecordingInitialized]) {
    if (![audio_device_ initializeRecording]) {
      RTC_LOG_F(LS_ERROR) << "Failed to initialize audio device recording";
      return -1;
    }
  }

  if (UpdateAudioParameters(record_parameters_, RecordParameters(audio_device_))) {
    UpdateInputAudioDeviceBuffer();
  }

  is_recording_initialized_ = true;
  RTC_LOG_F(LS_INFO) << "Did initialize recording";
  return 0;
}

bool ObjCAudioDeviceModule::Recording() const {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  return recording_.load() && [audio_device_ isRecording];
}

int32_t ObjCAudioDeviceModule::StartRecording() {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (!RecordingIsInitialized()) {
    return -1;
  }
  if (Recording()) {
    return 0;
  }

  audio_device_buffer_->StartRecording();
  if (record_fine_audio_buffer_) {
    record_fine_audio_buffer_->ResetRecord();
  }

  if (![audio_device_ startRecording]) {
    RTC_LOG_F(LS_ERROR) << "Failed to start audio device recording";
    return -1;
  }
  recording_.store(true, std::memory_order_release);
  RTC_LOG_F(LS_INFO) << "Did start recording";
  return 0;
}

int32_t ObjCAudioDeviceModule::StopRecording() {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);

  if (![audio_device_ stopRecording]) {
    RTC_LOG_F(LS_WARNING) << "Failed to stop recording";
    return -1;
  }
  audio_device_buffer_->StopRecording();
  recording_.store(false, std::memory_order_release);
  RTC_LOG_F(LS_INFO) << "Did stop recording";
  return 0;
}

#if defined(WEBRTC_IOS)

int ObjCAudioDeviceModule::GetPlayoutAudioParameters(AudioParameters* params) const {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK(playout_parameters_.is_valid());
  RTC_DCHECK_RUN_ON(&thread_checker_);
  *params = playout_parameters_;
  return 0;
}

int ObjCAudioDeviceModule::GetRecordAudioParameters(AudioParameters* params) const {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK(record_parameters_.is_valid());
  RTC_DCHECK_RUN_ON(&thread_checker_);
  *params = record_parameters_;
  return 0;
}

#endif  // WEBRTC_IOS

void ObjCAudioDeviceModule::UpdateOutputAudioDeviceBuffer() {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(audio_device_buffer_) << "AttachAudioBuffer must be called first";

  RTC_DCHECK_GT(playout_parameters_.sample_rate(), 0);
  RTC_DCHECK(playout_parameters_.channels() == 1 || playout_parameters_.channels() == 2);

  audio_device_buffer_->SetPlayoutSampleRate(playout_parameters_.sample_rate());
  audio_device_buffer_->SetPlayoutChannels(playout_parameters_.channels());
  playout_fine_audio_buffer_.reset(new FineAudioBuffer(audio_device_buffer_.get()));
}

void ObjCAudioDeviceModule::UpdateInputAudioDeviceBuffer() {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(audio_device_buffer_) << "AttachAudioBuffer must be called first";

  RTC_DCHECK_GT(record_parameters_.sample_rate(), 0);
  RTC_DCHECK(record_parameters_.channels() == 1 || record_parameters_.channels() == 2);

  audio_device_buffer_->SetRecordingSampleRate(record_parameters_.sample_rate());
  audio_device_buffer_->SetRecordingChannels(record_parameters_.channels());
  record_fine_audio_buffer_.reset(new FineAudioBuffer(audio_device_buffer_.get()));
}

void ObjCAudioDeviceModule::UpdateAudioDelay(std::atomic<int>& delay_ms,
                                             const NSTimeInterval device_latency) {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  int latency_ms = static_cast<int>(rtc::kNumMillisecsPerSec * device_latency);
  if (latency_ms <= 0) {
    return;
  }
  const int old_latency_ms = delay_ms.exchange(latency_ms);
  if (old_latency_ms != latency_ms) {
    RTC_LOG_F(LS_INFO) << "Did change audio IO latency from: " << old_latency_ms
                       << " ms to: " << latency_ms << " ms";
  }
}

bool ObjCAudioDeviceModule::UpdateAudioParameters(AudioParameters& params,
                                                  const AudioParameters& device_params) {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (!device_params.is_complete()) {
    RTC_LOG_F(LS_INFO) << "Device params are incomplete: " << device_params.ToString();
    return false;
  }
  if (params.channels() == device_params.channels() &&
      params.frames_per_buffer() == device_params.frames_per_buffer() &&
      params.sample_rate() == device_params.sample_rate()) {
    RTC_LOG_F(LS_INFO) << "Device params: " << device_params.ToString()
                       << " are not different from: " << params.ToString();
    return false;
  }

  RTC_LOG_F(LS_INFO) << "Audio params will be changed from: " << params.ToString()
                     << " to: " << device_params.ToString();
  params.reset(
      device_params.sample_rate(), device_params.channels(), device_params.frames_per_buffer());
  return true;
}

OSStatus ObjCAudioDeviceModule::OnDeliverRecordedData(
    AudioUnitRenderActionFlags* flags,
    const AudioTimeStamp* time_stamp,
    NSInteger bus_number,
    UInt32 num_frames,
    const AudioBufferList* io_data,
    void* render_context,
    RTC_OBJC_TYPE(RTCAudioDeviceRenderRecordedDataBlock) render_block) {
  RTC_DCHECK_RUN_ON(&io_record_thread_checker_);
  OSStatus result = noErr;
  // Simply return if recording is not enabled.
  if (!recording_.load()) return result;

  if (io_data != nullptr) {
    // AudioBuffer already fullfilled with audio data
    RTC_DCHECK_EQ(1, io_data->mNumberBuffers);
    const AudioBuffer* audio_buffer = &io_data->mBuffers[0];
    RTC_DCHECK(audio_buffer->mNumberChannels == 1 || audio_buffer->mNumberChannels == 2);

    record_fine_audio_buffer_->DeliverRecordedData(
        rtc::ArrayView<const int16_t>(static_cast<int16_t*>(audio_buffer->mData), num_frames),
        cached_recording_delay_ms_.load());
    return noErr;
  }
  RTC_DCHECK(render_block != nullptr) << "Either io_data or render_block must be provided";

  // Set the size of our own audio buffer and clear it first to avoid copying
  // in combination with potential reallocations.
  // On real iOS devices, the size will only be set once (at first callback).
  const int channels_count = record_parameters_.channels();
  record_audio_buffer_.Clear();
  record_audio_buffer_.SetSize(num_frames * channels_count);

  // Allocate AudioBuffers to be used as storage for the received audio.
  // The AudioBufferList structure works as a placeholder for the
  // AudioBuffer structure, which holds a pointer to the actual data buffer
  // in `record_audio_buffer_`. Recorded audio will be rendered into this memory
  // at each input callback when calling `render_block`.
  AudioBufferList audio_buffer_list;
  audio_buffer_list.mNumberBuffers = 1;
  AudioBuffer* audio_buffer = &audio_buffer_list.mBuffers[0];
  audio_buffer->mNumberChannels = channels_count;
  audio_buffer->mDataByteSize =
      record_audio_buffer_.size() * sizeof(decltype(record_audio_buffer_)::value_type);
  audio_buffer->mData = reinterpret_cast<int8_t*>(record_audio_buffer_.data());

  // Obtain the recorded audio samples by initiating a rendering cycle into own buffer.
  result =
      render_block(flags, time_stamp, bus_number, num_frames, &audio_buffer_list, render_context);
  if (result != noErr) {
    RTC_LOG_F(LS_ERROR) << "Failed to render audio: " << result;
    return result;
  }

  // Get a pointer to the recorded audio and send it to the WebRTC ADB.
  // Use the FineAudioBuffer instance to convert between native buffer size
  // and the 10ms buffer size used by WebRTC.
  record_fine_audio_buffer_->DeliverRecordedData(record_audio_buffer_,
                                                 cached_recording_delay_ms_.load());
  return noErr;
}

OSStatus ObjCAudioDeviceModule::OnGetPlayoutData(AudioUnitRenderActionFlags* flags,
                                                 const AudioTimeStamp* time_stamp,
                                                 NSInteger bus_number,
                                                 UInt32 num_frames,
                                                 AudioBufferList* io_data) {
  RTC_DCHECK_RUN_ON(&io_playout_thread_checker_);
  // Verify 16-bit, noninterleaved mono or stereo PCM signal format.
  RTC_DCHECK_EQ(1, io_data->mNumberBuffers);
  AudioBuffer* audio_buffer = &io_data->mBuffers[0];
  RTC_DCHECK(audio_buffer->mNumberChannels == 1 || audio_buffer->mNumberChannels == 2);
  RTC_DCHECK_EQ(audio_buffer->mDataByteSize,
                sizeof(int16_t) * num_frames * audio_buffer->mNumberChannels);

  // Produce silence and give player a hint about it if playout is not
  // activated.
  if (!playing_.load()) {
    *flags |= kAudioUnitRenderAction_OutputIsSilence;
    memset(static_cast<int8_t*>(audio_buffer->mData), 0, audio_buffer->mDataByteSize);
    return noErr;
  }

  // Read decoded 16-bit PCM samples from WebRTC into the
  // `io_data` destination buffer.
  playout_fine_audio_buffer_->GetPlayoutData(
      rtc::ArrayView<int16_t>(static_cast<int16_t*>(audio_buffer->mData),
                              num_frames * audio_buffer->mNumberChannels),
      cached_playout_delay_ms_.load());

  return noErr;
}

void ObjCAudioDeviceModule::HandleAudioInputInterrupted() {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  io_record_thread_checker_.Detach();
}

void ObjCAudioDeviceModule::HandleAudioOutputInterrupted() {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  io_playout_thread_checker_.Detach();
}

void ObjCAudioDeviceModule::HandleAudioInputParametersChange() {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);

  if (UpdateAudioParameters(record_parameters_, RecordParameters(audio_device_))) {
    UpdateInputAudioDeviceBuffer();
  }

  UpdateAudioDelay(cached_recording_delay_ms_, [audio_device_ inputLatency]);
}

void ObjCAudioDeviceModule::HandleAudioOutputParametersChange() {
  RTC_DLOG_F(LS_VERBOSE) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);

  if (UpdateAudioParameters(playout_parameters_, PlayoutParameters(audio_device_))) {
    UpdateOutputAudioDeviceBuffer();
  }

  UpdateAudioDelay(cached_playout_delay_ms_, [audio_device_ outputLatency]);
}

#pragma mark - Not implemented/Not relevant methods from AudioDeviceModule

int32_t ObjCAudioDeviceModule::ActiveAudioLayer(AudioLayer* audioLayer) const {
  return -1;
}

int16_t ObjCAudioDeviceModule::PlayoutDevices() {
  return 0;
}

int16_t ObjCAudioDeviceModule::RecordingDevices() {
  return 0;
}

int32_t ObjCAudioDeviceModule::PlayoutDeviceName(uint16_t index,
                                                 char name[kAdmMaxDeviceNameSize],
                                                 char guid[kAdmMaxGuidSize]) {
  return -1;
}

int32_t ObjCAudioDeviceModule::RecordingDeviceName(uint16_t index,
                                                   char name[kAdmMaxDeviceNameSize],
                                                   char guid[kAdmMaxGuidSize]) {
  return -1;
}

int32_t ObjCAudioDeviceModule::SetPlayoutDevice(uint16_t index) {
  return 0;
}

int32_t ObjCAudioDeviceModule::SetPlayoutDevice(WindowsDeviceType device) {
  return -1;
}

int32_t ObjCAudioDeviceModule::SetRecordingDevice(uint16_t index) {
  return 0;
}

int32_t ObjCAudioDeviceModule::SetRecordingDevice(WindowsDeviceType device) {
  return -1;
}

int32_t ObjCAudioDeviceModule::InitSpeaker() {
  return 0;
}

bool ObjCAudioDeviceModule::SpeakerIsInitialized() const {
  return true;
}

int32_t ObjCAudioDeviceModule::InitMicrophone() {
  return 0;
}

bool ObjCAudioDeviceModule::MicrophoneIsInitialized() const {
  return true;
}

int32_t ObjCAudioDeviceModule::SpeakerVolumeIsAvailable(bool* available) {
  *available = false;
  return 0;
}

int32_t ObjCAudioDeviceModule::SetSpeakerVolume(uint32_t volume) {
  return -1;
}

int32_t ObjCAudioDeviceModule::SpeakerVolume(uint32_t* volume) const {
  return -1;
}

int32_t ObjCAudioDeviceModule::MaxSpeakerVolume(uint32_t* maxVolume) const {
  return -1;
}

int32_t ObjCAudioDeviceModule::MinSpeakerVolume(uint32_t* minVolume) const {
  return -1;
}

int32_t ObjCAudioDeviceModule::SpeakerMuteIsAvailable(bool* available) {
  *available = false;
  return 0;
}

int32_t ObjCAudioDeviceModule::SetSpeakerMute(bool enable) {
  return -1;
}

int32_t ObjCAudioDeviceModule::SpeakerMute(bool* enabled) const {
  return -1;
}

int32_t ObjCAudioDeviceModule::MicrophoneMuteIsAvailable(bool* available) {
  *available = false;
  return 0;
}

int32_t ObjCAudioDeviceModule::SetMicrophoneMute(bool enable) {
  return -1;
}

int32_t ObjCAudioDeviceModule::MicrophoneMute(bool* enabled) const {
  return -1;
}

int32_t ObjCAudioDeviceModule::MicrophoneVolumeIsAvailable(bool* available) {
  *available = false;
  return 0;
}

int32_t ObjCAudioDeviceModule::SetMicrophoneVolume(uint32_t volume) {
  return -1;
}

int32_t ObjCAudioDeviceModule::MicrophoneVolume(uint32_t* volume) const {
  return -1;
}

int32_t ObjCAudioDeviceModule::MaxMicrophoneVolume(uint32_t* maxVolume) const {
  return -1;
}

int32_t ObjCAudioDeviceModule::MinMicrophoneVolume(uint32_t* minVolume) const {
  return -1;
}

int32_t ObjCAudioDeviceModule::StereoPlayoutIsAvailable(bool* available) const {
  *available = false;
  return 0;
}

int32_t ObjCAudioDeviceModule::SetStereoPlayout(bool enable) {
  return -1;
}

int32_t ObjCAudioDeviceModule::StereoPlayout(bool* enabled) const {
  *enabled = false;
  return 0;
}

int32_t ObjCAudioDeviceModule::StereoRecordingIsAvailable(bool* available) const {
  *available = false;
  return 0;
}

int32_t ObjCAudioDeviceModule::SetStereoRecording(bool enable) {
  return -1;
}

int32_t ObjCAudioDeviceModule::StereoRecording(bool* enabled) const {
  *enabled = false;
  return 0;
}

bool ObjCAudioDeviceModule::BuiltInAECIsAvailable() const {
  return false;
}

int32_t ObjCAudioDeviceModule::EnableBuiltInAEC(bool enable) {
  return 0;
}

bool ObjCAudioDeviceModule::BuiltInAGCIsAvailable() const {
  return false;
}

int32_t ObjCAudioDeviceModule::EnableBuiltInAGC(bool enable) {
  return 0;
}

bool ObjCAudioDeviceModule::BuiltInNSIsAvailable() const {
  return false;
}

int32_t ObjCAudioDeviceModule::EnableBuiltInNS(bool enable) {
  return 0;
}

int32_t ObjCAudioDeviceModule::GetPlayoutUnderrunCount() const {
  return -1;
}

}  // namespace objc_adm

}  // namespace webrtc
