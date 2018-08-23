/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/win/core_audio_output_win.h"

#include "absl/memory/memory.h"
#include "modules/audio_device/audio_device_buffer.h"
#include "modules/audio_device/fine_audio_buffer.h"
#include "rtc_base/bind.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

using Microsoft::WRL::ComPtr;

namespace webrtc {
namespace webrtc_win {

CoreAudioOutput::CoreAudioOutput()
    : CoreAudioBase(CoreAudioBase::Direction::kOutput,
                    [this](uint64_t freq) { return OnDataCallback(freq); }) {
  RTC_DLOG(INFO) << __FUNCTION__;
  RTC_DCHECK_RUN_ON(&thread_checker_);
  thread_checker_audio_.DetachFromThread();
}

CoreAudioOutput::~CoreAudioOutput() {
  RTC_DLOG(INFO) << __FUNCTION__;
  RTC_DCHECK_RUN_ON(&thread_checker_);
  Terminate();
}

int CoreAudioOutput::Init() {
  RTC_DLOG(INFO) << __FUNCTION__;
  RTC_DCHECK_RUN_ON(&thread_checker_);
  return 0;
}

int CoreAudioOutput::Terminate() {
  RTC_DLOG(INFO) << __FUNCTION__;
  RTC_DCHECK_RUN_ON(&thread_checker_);
  StopPlayout();
  return 0;
}

int CoreAudioOutput::NumDevices() const {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  return core_audio_utility::NumberOfActiveDevices(eRender);
}

int CoreAudioOutput::SetDevice(int index) {
  RTC_DLOG(INFO) << __FUNCTION__ << ": " << index;
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (initialized_) {
    return -1;
  }

  std::string device_id = GetDeviceID(index);
  RTC_DLOG(INFO) << "index=" << index << " => device_id: " << device_id;
  device_id_ = device_id;

  return device_id_.empty() ? -1 : 0;
}

int CoreAudioOutput::SetDevice(AudioDeviceModule::WindowsDeviceType device) {
  RTC_DLOG(INFO) << __FUNCTION__ << ": " << device;
  RTC_DCHECK_RUN_ON(&thread_checker_);
  return SetDevice((device == AudioDeviceModule::kDefaultDevice) ? 0 : 1);
}

int CoreAudioOutput::DeviceName(int index,
                                std::string* name,
                                std::string* guid) {
  RTC_DLOG(INFO) << __FUNCTION__ << ": " << index;
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(name);
  return CoreAudioBase::DeviceName(index, name, guid);
}

void CoreAudioOutput::AttachAudioBuffer(AudioDeviceBuffer* audio_buffer) {
  RTC_DLOG(INFO) << __FUNCTION__;
  RTC_DCHECK_RUN_ON(&thread_checker_);
  audio_device_buffer_ = audio_buffer;
}

bool CoreAudioOutput::PlayoutIsInitialized() const {
  RTC_DLOG(INFO) << __FUNCTION__;
  RTC_DCHECK_RUN_ON(&thread_checker_);
  return initialized_;
}

int CoreAudioOutput::InitPlayout() {
  RTC_DLOG(INFO) << __FUNCTION__;
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(!initialized_);
  RTC_DCHECK(!Playing());
  RTC_DCHECK(!audio_client_.Get());
  RTC_DCHECK(!audio_render_client_.Get());

  // Create an IAudioClient client and store the valid interface pointer in
  // |audio_client_|. The base class will use optimal output parameters and do
  // an event driven shared mode initialization. The utilized format will be
  // stored in |format_| and can be used for configuration and allocation of
  // audio buffers.
  if (!CoreAudioBase::Init()) {
    return -1;
  }
  RTC_DCHECK(audio_client_.Get());

  // Configure the playout side of the audio device buffer using |format_|
  // after a trivial sanity check of the format structure.
  RTC_DCHECK(audio_device_buffer_);
  WAVEFORMATEX* format = &format_.Format;
  RTC_DCHECK_EQ(format->wFormatTag, WAVE_FORMAT_EXTENSIBLE);
  audio_device_buffer_->SetPlayoutSampleRate(format->nSamplesPerSec);
  audio_device_buffer_->SetPlayoutChannels(format->nChannels);

  // Create a modified audio buffer class which allows us to ask for any number
  // of samples (and not only multiple of 10ms) to match the optimal
  // buffer size per callback used by Core Audio.
  // TODO(henrika): can we use a shared buffer instead?
  fine_audio_buffer_ = absl::make_unique<FineAudioBuffer>(audio_device_buffer_);

  // Create an IAudioRenderClient for an initialized IAudioClient.
  // The IAudioRenderClient interface enables us to write output data to
  // a rendering endpoint buffer.
  ComPtr<IAudioRenderClient> audio_render_client =
      core_audio_utility::CreateRenderClient(audio_client_.Get());
  if (!audio_render_client.Get())
    return -1;

  ComPtr<IAudioClock> audio_clock =
      core_audio_utility::CreateAudioClock(audio_client_.Get());
  if (!audio_clock.Get())
    return -1;

  // Store valid COM interfaces. Note that, |audio_client_| has already been
  // set in CoreAudioBase::Init().
  audio_render_client_ = audio_render_client;
  audio_clock_ = audio_clock;

  initialized_ = true;
  return 0;
}

int CoreAudioOutput::StartPlayout() {
  RTC_DLOG(INFO) << __FUNCTION__;
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(!Playing());
  if (!initialized_) {
    RTC_DLOG(LS_WARNING)
        << "Playout can not start since InitPlayout must succeed first";
    return 0;
  }
  if (fine_audio_buffer_) {
    fine_audio_buffer_->ResetPlayout();
  }

  if (!core_audio_utility::FillRenderEndpointBufferWithSilence(
          audio_client_.Get(), audio_render_client_.Get())) {
    RTC_LOG(LS_WARNING) << "Failed to prepare output endpoint with silence";
  }

  num_frames_written_ = endpoint_buffer_size_frames_;

  if (!Start()) {
    return -1;
  }

  return 0;
}

int CoreAudioOutput::StopPlayout() {
  RTC_DLOG(INFO) << __FUNCTION__;
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (!initialized_) {
    return 0;
  }

  // Release resources allocated in InitPlayout() and then return if this
  // method is called without any active output audio.
  if (!Playing()) {
    RTC_DLOG(WARNING) << "No output stream is active";
    audio_client_.Reset();
    audio_render_client_.Reset();
    initialized_ = false;
    return 0;
  }

  if (!Stop()) {
    RTC_LOG(LS_ERROR) << "StopPlayout failed";
    return -1;
  }

  thread_checker_audio_.DetachFromThread();
  initialized_ = false;
  return 0;
}

bool CoreAudioOutput::Playing() {
  RTC_DLOG(INFO) << __FUNCTION__;
  RTC_DCHECK_RUN_ON(&thread_checker_);
  return audio_thread_ != nullptr;
}

// TODO(henrika): finalize support of audio session volume control. As is, we
// are not compatible with the old ADM implementation since it allows accessing
// the volume control with any active audio output stream.
int CoreAudioOutput::VolumeIsAvailable(bool* available) {
  RTC_DLOG(INFO) << __FUNCTION__;
  RTC_DCHECK_RUN_ON(&thread_checker_);
  return IsVolumeControlAvailable(available) ? 0 : -1;
}

bool CoreAudioOutput::OnDataCallback(uint64_t device_frequency) {
  RTC_DCHECK_RUN_ON(&thread_checker_audio_);
  // Get the padding value which indicates the amount of valid unread data that
  // the endpoint buffer currently contains.
  UINT32 num_unread_frames = 0;
  _com_error error = audio_client_->GetCurrentPadding(&num_unread_frames);
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "IAudioClient::GetCurrentPadding failed: "
                      << core_audio_utility::ErrorToString(error);
    return false;
  }

  // Contains how much new data we can write to the buffer without the risk of
  // overwriting previously written data that the audio engine has not yet read
  // from the buffer. I.e., it is the maximum buffer size we can request when
  // calling IAudioRenderClient::GetBuffer().
  UINT32 num_requested_frames =
      endpoint_buffer_size_frames_ - num_unread_frames;

  // Request all available space in the rendering endpoint buffer into which the
  // client can later write an audio packet.
  uint8_t* audio_data;
  error = audio_render_client_->GetBuffer(num_requested_frames, &audio_data);
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "IAudioRenderClient::GetBuffer failed: "
                      << core_audio_utility::ErrorToString(error);
    return false;
  }

  // TODO(henrika): only update the latency estimate N times per second to
  // save resources.
  // TODO(henrika): note that FineAudioBuffer adds latency as well.
  int playout_delay_ms = EstimateOutputLatencyMillis(device_frequency);
  // RTC_DLOG(INFO) << "playout_delay_ms: " << playout_delay_ms;

  // Get audio data from WebRTC and write it to the allocated buffer in
  // |audio_data|.
  fine_audio_buffer_->GetPlayoutData(
      rtc::MakeArrayView(reinterpret_cast<int16_t*>(audio_data),
                         num_requested_frames * format_.Format.nChannels),
      playout_delay_ms);

  // Release the buffer space acquired in IAudioRenderClient::GetBuffer.
  error = audio_render_client_->ReleaseBuffer(num_requested_frames, 0);
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "IAudioRenderClient::ReleaseBuffer failed: "
                      << core_audio_utility::ErrorToString(error);
    return false;
  }

  num_frames_written_ += num_requested_frames;

  return true;
}

// TODO(henrika): IAudioClock2::GetDevicePosition could perhaps be used here
// instead. Tried it once, but it crashed for capture devices.
int CoreAudioOutput::EstimateOutputLatencyMillis(uint64_t device_frequency) {
  UINT64 position = 0;
  UINT64 qpc_position = 0;
  int delay_ms = 0;
  // Get the device position through output parameter |position|. This is the
  // stream position of the sample that is currently playing through the
  // speakers.
  _com_error error = audio_clock_->GetPosition(&position, &qpc_position);
  if (error.Error() == S_OK) {
    // Number of frames already played out through the speaker.
    const uint64_t num_played_out_frames =
        format_.Format.nSamplesPerSec * position / device_frequency;

    // Number of frames that have been written to the buffer but not yet
    // played out corresponding to the estimated latency measured in number
    // of audio frames.
    const uint64_t delay_frames = num_frames_written_ - num_played_out_frames;

    // Convert latency in number of frames into milliseconds.
    webrtc::TimeDelta delay = webrtc::TimeDelta::us(
        delay_frames * kNumMicrosecsPerSec / format_.Format.nSamplesPerSec);
    delay_ms = delay.ms();
  }
  return delay_ms;
}

}  // namespace webrtc_win

}  // namespace webrtc
