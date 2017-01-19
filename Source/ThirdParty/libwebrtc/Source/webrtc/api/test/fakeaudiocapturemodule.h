/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This class implements an AudioCaptureModule that can be used to detect if
// audio is being received properly if it is fed by another AudioCaptureModule
// in some arbitrary audio pipeline where they are connected. It does not play
// out or record any audio so it does not need access to any hardware and can
// therefore be used in the gtest testing framework.

// Note P postfix of a function indicates that it should only be called by the
// processing thread.

#ifndef WEBRTC_API_TEST_FAKEAUDIOCAPTUREMODULE_H_
#define WEBRTC_API_TEST_FAKEAUDIOCAPTUREMODULE_H_

#include <memory>

#include "webrtc/base/basictypes.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/messagehandler.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_device/include/audio_device.h"

namespace rtc {
class Thread;
}  // namespace rtc

class FakeAudioCaptureModule
    : public webrtc::AudioDeviceModule,
      public rtc::MessageHandler {
 public:
  typedef uint16_t Sample;

  // The value for the following constants have been derived by running VoE
  // using a real ADM. The constants correspond to 10ms of mono audio at 44kHz.
  static const size_t kNumberSamples = 440;
  static const size_t kNumberBytesPerSample = sizeof(Sample);

  // Creates a FakeAudioCaptureModule or returns NULL on failure.
  static rtc::scoped_refptr<FakeAudioCaptureModule> Create();

  // Returns the number of frames that have been successfully pulled by the
  // instance. Note that correctly detecting success can only be done if the
  // pulled frame was generated/pushed from a FakeAudioCaptureModule.
  int frames_received() const;

  // Following functions are inherited from webrtc::AudioDeviceModule.
  // Only functions called by PeerConnection are implemented, the rest do
  // nothing and return success. If a function is not expected to be called by
  // PeerConnection an assertion is triggered if it is in fact called.
  int64_t TimeUntilNextProcess() override;
  void Process() override;

  int32_t ActiveAudioLayer(AudioLayer* audio_layer) const override;

  ErrorCode LastError() const override;
  int32_t RegisterEventObserver(
      webrtc::AudioDeviceObserver* event_callback) override;

  // Note: Calling this method from a callback may result in deadlock.
  int32_t RegisterAudioCallback(
      webrtc::AudioTransport* audio_callback) override;

  int32_t Init() override;
  int32_t Terminate() override;
  bool Initialized() const override;

  int16_t PlayoutDevices() override;
  int16_t RecordingDevices() override;
  int32_t PlayoutDeviceName(uint16_t index,
                            char name[webrtc::kAdmMaxDeviceNameSize],
                            char guid[webrtc::kAdmMaxGuidSize]) override;
  int32_t RecordingDeviceName(uint16_t index,
                              char name[webrtc::kAdmMaxDeviceNameSize],
                              char guid[webrtc::kAdmMaxGuidSize]) override;

  int32_t SetPlayoutDevice(uint16_t index) override;
  int32_t SetPlayoutDevice(WindowsDeviceType device) override;
  int32_t SetRecordingDevice(uint16_t index) override;
  int32_t SetRecordingDevice(WindowsDeviceType device) override;

  int32_t PlayoutIsAvailable(bool* available) override;
  int32_t InitPlayout() override;
  bool PlayoutIsInitialized() const override;
  int32_t RecordingIsAvailable(bool* available) override;
  int32_t InitRecording() override;
  bool RecordingIsInitialized() const override;

  int32_t StartPlayout() override;
  int32_t StopPlayout() override;
  bool Playing() const override;
  int32_t StartRecording() override;
  int32_t StopRecording() override;
  bool Recording() const override;

  int32_t SetAGC(bool enable) override;
  bool AGC() const override;

  int32_t SetWaveOutVolume(uint16_t volume_left,
                           uint16_t volume_right) override;
  int32_t WaveOutVolume(uint16_t* volume_left,
                        uint16_t* volume_right) const override;

  int32_t InitSpeaker() override;
  bool SpeakerIsInitialized() const override;
  int32_t InitMicrophone() override;
  bool MicrophoneIsInitialized() const override;

  int32_t SpeakerVolumeIsAvailable(bool* available) override;
  int32_t SetSpeakerVolume(uint32_t volume) override;
  int32_t SpeakerVolume(uint32_t* volume) const override;
  int32_t MaxSpeakerVolume(uint32_t* max_volume) const override;
  int32_t MinSpeakerVolume(uint32_t* min_volume) const override;
  int32_t SpeakerVolumeStepSize(uint16_t* step_size) const override;

  int32_t MicrophoneVolumeIsAvailable(bool* available) override;
  int32_t SetMicrophoneVolume(uint32_t volume) override;
  int32_t MicrophoneVolume(uint32_t* volume) const override;
  int32_t MaxMicrophoneVolume(uint32_t* max_volume) const override;

  int32_t MinMicrophoneVolume(uint32_t* min_volume) const override;
  int32_t MicrophoneVolumeStepSize(uint16_t* step_size) const override;

  int32_t SpeakerMuteIsAvailable(bool* available) override;
  int32_t SetSpeakerMute(bool enable) override;
  int32_t SpeakerMute(bool* enabled) const override;

  int32_t MicrophoneMuteIsAvailable(bool* available) override;
  int32_t SetMicrophoneMute(bool enable) override;
  int32_t MicrophoneMute(bool* enabled) const override;

  int32_t MicrophoneBoostIsAvailable(bool* available) override;
  int32_t SetMicrophoneBoost(bool enable) override;
  int32_t MicrophoneBoost(bool* enabled) const override;

  int32_t StereoPlayoutIsAvailable(bool* available) const override;
  int32_t SetStereoPlayout(bool enable) override;
  int32_t StereoPlayout(bool* enabled) const override;
  int32_t StereoRecordingIsAvailable(bool* available) const override;
  int32_t SetStereoRecording(bool enable) override;
  int32_t StereoRecording(bool* enabled) const override;
  int32_t SetRecordingChannel(const ChannelType channel) override;
  int32_t RecordingChannel(ChannelType* channel) const override;

  int32_t SetPlayoutBuffer(const BufferType type,
                           uint16_t size_ms = 0) override;
  int32_t PlayoutBuffer(BufferType* type, uint16_t* size_ms) const override;
  int32_t PlayoutDelay(uint16_t* delay_ms) const override;
  int32_t RecordingDelay(uint16_t* delay_ms) const override;

  int32_t CPULoad(uint16_t* load) const override;

  int32_t StartRawOutputFileRecording(
      const char pcm_file_name_utf8[webrtc::kAdmMaxFileNameSize]) override;
  int32_t StopRawOutputFileRecording() override;
  int32_t StartRawInputFileRecording(
      const char pcm_file_name_utf8[webrtc::kAdmMaxFileNameSize]) override;
  int32_t StopRawInputFileRecording() override;

  int32_t SetRecordingSampleRate(const uint32_t samples_per_sec) override;
  int32_t RecordingSampleRate(uint32_t* samples_per_sec) const override;
  int32_t SetPlayoutSampleRate(const uint32_t samples_per_sec) override;
  int32_t PlayoutSampleRate(uint32_t* samples_per_sec) const override;

  int32_t ResetAudioDevice() override;
  int32_t SetLoudspeakerStatus(bool enable) override;
  int32_t GetLoudspeakerStatus(bool* enabled) const override;
  bool BuiltInAECIsAvailable() const override { return false; }
  int32_t EnableBuiltInAEC(bool enable) override { return -1; }
  bool BuiltInAGCIsAvailable() const override { return false; }
  int32_t EnableBuiltInAGC(bool enable) override { return -1; }
  bool BuiltInNSIsAvailable() const override { return false; }
  int32_t EnableBuiltInNS(bool enable) override { return -1; }
#if defined(WEBRTC_IOS)
  int GetPlayoutAudioParameters(
      webrtc::AudioParameters* params) const override {
    return -1;
  }
  int GetRecordAudioParameters(webrtc::AudioParameters* params) const override {
    return -1;
  }
#endif  // WEBRTC_IOS

  // End of functions inherited from webrtc::AudioDeviceModule.

  // The following function is inherited from rtc::MessageHandler.
  void OnMessage(rtc::Message* msg) override;

 protected:
  // The constructor is protected because the class needs to be created as a
  // reference counted object (for memory managment reasons). It could be
  // exposed in which case the burden of proper instantiation would be put on
  // the creator of a FakeAudioCaptureModule instance. To create an instance of
  // this class use the Create(..) API.
  explicit FakeAudioCaptureModule();
  // The destructor is protected because it is reference counted and should not
  // be deleted directly.
  virtual ~FakeAudioCaptureModule();

 private:
  // Initializes the state of the FakeAudioCaptureModule. This API is called on
  // creation by the Create() API.
  bool Initialize();
  // SetBuffer() sets all samples in send_buffer_ to |value|.
  void SetSendBuffer(int value);
  // Resets rec_buffer_. I.e., sets all rec_buffer_ samples to 0.
  void ResetRecBuffer();
  // Returns true if rec_buffer_ contains one or more sample greater than or
  // equal to |value|.
  bool CheckRecBuffer(int value);

  // Returns true/false depending on if recording or playback has been
  // enabled/started.
  bool ShouldStartProcessing();

  // Starts or stops the pushing and pulling of audio frames.
  void UpdateProcessing(bool start);

  // Starts the periodic calling of ProcessFrame() in a thread safe way.
  void StartProcessP();
  // Periodcally called function that ensures that frames are pulled and pushed
  // periodically if enabled/started.
  void ProcessFrameP();
  // Pulls frames from the registered webrtc::AudioTransport.
  void ReceiveFrameP();
  // Pushes frames to the registered webrtc::AudioTransport.
  void SendFrameP();

  // The time in milliseconds when Process() was last called or 0 if no call
  // has been made.
  int64_t last_process_time_ms_;

  // Callback for playout and recording.
  webrtc::AudioTransport* audio_callback_;

  bool recording_; // True when audio is being pushed from the instance.
  bool playing_; // True when audio is being pulled by the instance.

  bool play_is_initialized_; // True when the instance is ready to pull audio.
  bool rec_is_initialized_; // True when the instance is ready to push audio.

  // Input to and output from RecordedDataIsAvailable(..) makes it possible to
  // modify the current mic level. The implementation does not care about the
  // mic level so it just feeds back what it receives.
  uint32_t current_mic_level_;

  // next_frame_time_ is updated in a non-drifting manner to indicate the next
  // wall clock time the next frame should be generated and received. started_
  // ensures that next_frame_time_ can be initialized properly on first call.
  bool started_;
  int64_t next_frame_time_;

  std::unique_ptr<rtc::Thread> process_thread_;

  // Buffer for storing samples received from the webrtc::AudioTransport.
  char rec_buffer_[kNumberSamples * kNumberBytesPerSample];
  // Buffer for samples to send to the webrtc::AudioTransport.
  char send_buffer_[kNumberSamples * kNumberBytesPerSample];

  // Counter of frames received that have samples of high enough amplitude to
  // indicate that the frames are not faked somewhere in the audio pipeline
  // (e.g. by a jitter buffer).
  int frames_received_;

  // Protects variables that are accessed from process_thread_ and
  // the main thread.
  rtc::CriticalSection crit_;
  // Protects |audio_callback_| that is accessed from process_thread_ and
  // the main thread.
  rtc::CriticalSection crit_callback_;
};

#endif  // WEBRTC_API_TEST_FAKEAUDIOCAPTUREMODULE_H_
