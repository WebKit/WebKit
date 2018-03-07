/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/android/audio_manager.h"

#include <utility>

#include <android/log.h>

#include "modules/audio_device/android/audio_common.h"
#include "modules/utility/include/helpers_android.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"

#define TAG "AudioManager"
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

namespace webrtc {

// AudioManager::JavaAudioManager implementation
AudioManager::JavaAudioManager::JavaAudioManager(
    NativeRegistration* native_reg,
    std::unique_ptr<GlobalRef> audio_manager)
    : audio_manager_(std::move(audio_manager)),
      init_(native_reg->GetMethodId("init", "()Z")),
      dispose_(native_reg->GetMethodId("dispose", "()V")),
      is_communication_mode_enabled_(
          native_reg->GetMethodId("isCommunicationModeEnabled", "()Z")),
      is_device_blacklisted_for_open_sles_usage_(
          native_reg->GetMethodId("isDeviceBlacklistedForOpenSLESUsage",
                                  "()Z")) {
  ALOGD("JavaAudioManager::ctor%s", GetThreadInfo().c_str());
}

AudioManager::JavaAudioManager::~JavaAudioManager() {
  ALOGD("JavaAudioManager::dtor%s", GetThreadInfo().c_str());
}

bool AudioManager::JavaAudioManager::Init() {
  return audio_manager_->CallBooleanMethod(init_);
}

void AudioManager::JavaAudioManager::Close() {
  audio_manager_->CallVoidMethod(dispose_);
}

bool AudioManager::JavaAudioManager::IsCommunicationModeEnabled() {
  return audio_manager_->CallBooleanMethod(is_communication_mode_enabled_);
}

bool AudioManager::JavaAudioManager::IsDeviceBlacklistedForOpenSLESUsage() {
  return audio_manager_->CallBooleanMethod(
      is_device_blacklisted_for_open_sles_usage_);
}

// AudioManager implementation
AudioManager::AudioManager()
    : j_environment_(JVM::GetInstance()->environment()),
      audio_layer_(AudioDeviceModule::kPlatformDefaultAudio),
      initialized_(false),
      hardware_aec_(false),
      hardware_agc_(false),
      hardware_ns_(false),
      low_latency_playout_(false),
      low_latency_record_(false),
      delay_estimate_in_milliseconds_(0) {
  ALOGD("ctor%s", GetThreadInfo().c_str());
  RTC_CHECK(j_environment_);
  JNINativeMethod native_methods[] = {
      {"nativeCacheAudioParameters", "(IIIZZZZZZIIJ)V",
       reinterpret_cast<void*>(&webrtc::AudioManager::CacheAudioParameters)}};
  j_native_registration_ = j_environment_->RegisterNatives(
      "org/webrtc/voiceengine/WebRtcAudioManager", native_methods,
      arraysize(native_methods));
  j_audio_manager_.reset(
      new JavaAudioManager(j_native_registration_.get(),
                           j_native_registration_->NewObject(
                               "<init>", "(J)V", PointerTojlong(this))));
}

AudioManager::~AudioManager() {
  ALOGD("~dtor%s", GetThreadInfo().c_str());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  Close();
}

void AudioManager::SetActiveAudioLayer(
    AudioDeviceModule::AudioLayer audio_layer) {
  ALOGD("SetActiveAudioLayer(%d)%s", audio_layer, GetThreadInfo().c_str());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(!initialized_);
  // Store the currently utilized audio layer.
  audio_layer_ = audio_layer;
  // The delay estimate can take one of two fixed values depending on if the
  // device supports low-latency output or not. However, it is also possible
  // that the user explicitly selects the high-latency audio path, hence we use
  // the selected |audio_layer| here to set the delay estimate.
  delay_estimate_in_milliseconds_ =
      (audio_layer == AudioDeviceModule::kAndroidJavaAudio)
          ? kHighLatencyModeDelayEstimateInMilliseconds
          : kLowLatencyModeDelayEstimateInMilliseconds;
  ALOGD("delay_estimate_in_milliseconds: %d", delay_estimate_in_milliseconds_);
}

SLObjectItf AudioManager::GetOpenSLEngine() {
  ALOGD("GetOpenSLEngine%s", GetThreadInfo().c_str());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  // Only allow usage of OpenSL ES if such an audio layer has been specified.
  if (audio_layer_ != AudioDeviceModule::kAndroidOpenSLESAudio &&
      audio_layer_ !=
          AudioDeviceModule::kAndroidJavaInputAndOpenSLESOutputAudio) {
    ALOGI("Unable to create OpenSL engine for the current audio layer: %d",
          audio_layer_);
    return nullptr;
  }
  // OpenSL ES for Android only supports a single engine per application.
  // If one already has been created, return existing object instead of
  // creating a new.
  if (engine_object_.Get() != nullptr) {
    ALOGI("The OpenSL ES engine object has already been created");
    return engine_object_.Get();
  }
  // Create the engine object in thread safe mode.
  const SLEngineOption option[] = {
      {SL_ENGINEOPTION_THREADSAFE, static_cast<SLuint32>(SL_BOOLEAN_TRUE)}};
  SLresult result =
      slCreateEngine(engine_object_.Receive(), 1, option, 0, NULL, NULL);
  if (result != SL_RESULT_SUCCESS) {
    ALOGE("slCreateEngine() failed: %s", GetSLErrorString(result));
    engine_object_.Reset();
    return nullptr;
  }
  // Realize the SL Engine in synchronous mode.
  result = engine_object_->Realize(engine_object_.Get(), SL_BOOLEAN_FALSE);
  if (result != SL_RESULT_SUCCESS) {
    ALOGE("Realize() failed: %s", GetSLErrorString(result));
    engine_object_.Reset();
    return nullptr;
  }
  // Finally return the SLObjectItf interface of the engine object.
  return engine_object_.Get();
}

bool AudioManager::Init() {
  ALOGD("Init%s", GetThreadInfo().c_str());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(!initialized_);
  RTC_DCHECK_NE(audio_layer_, AudioDeviceModule::kPlatformDefaultAudio);
  if (!j_audio_manager_->Init()) {
    ALOGE("init failed!");
    return false;
  }
  initialized_ = true;
  return true;
}

bool AudioManager::Close() {
  ALOGD("Close%s", GetThreadInfo().c_str());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialized_)
    return true;
  j_audio_manager_->Close();
  initialized_ = false;
  return true;
}

bool AudioManager::IsCommunicationModeEnabled() const {
  ALOGD("IsCommunicationModeEnabled()");
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return j_audio_manager_->IsCommunicationModeEnabled();
}

bool AudioManager::IsAcousticEchoCancelerSupported() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return hardware_aec_;
}

bool AudioManager::IsAutomaticGainControlSupported() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return hardware_agc_;
}

bool AudioManager::IsNoiseSuppressorSupported() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return hardware_ns_;
}

bool AudioManager::IsLowLatencyPlayoutSupported() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  ALOGD("IsLowLatencyPlayoutSupported()");
  // Some devices are blacklisted for usage of OpenSL ES even if they report
  // that low-latency playout is supported. See b/21485703 for details.
  return j_audio_manager_->IsDeviceBlacklistedForOpenSLESUsage()
             ? false
             : low_latency_playout_;
}

bool AudioManager::IsLowLatencyRecordSupported() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  ALOGD("IsLowLatencyRecordSupported()");
  return low_latency_record_;
}

bool AudioManager::IsProAudioSupported() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  ALOGD("IsProAudioSupported()");
  // TODO(henrika): return the state independently of if OpenSL ES is
  // blacklisted or not for now. We could use the same approach as in
  // IsLowLatencyPlayoutSupported() but I can't see the need for it yet.
  return pro_audio_;
}

bool AudioManager::IsStereoPlayoutSupported() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  ALOGD("IsStereoPlayoutSupported()");
  return (playout_parameters_.channels() == 2);
}

bool AudioManager::IsStereoRecordSupported() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  ALOGD("IsStereoRecordSupported()");
  return (record_parameters_.channels() == 2);
}

int AudioManager::GetDelayEstimateInMilliseconds() const {
  return delay_estimate_in_milliseconds_;
}

void JNICALL AudioManager::CacheAudioParameters(JNIEnv* env,
                                                jobject obj,
                                                jint sample_rate,
                                                jint output_channels,
                                                jint input_channels,
                                                jboolean hardware_aec,
                                                jboolean hardware_agc,
                                                jboolean hardware_ns,
                                                jboolean low_latency_output,
                                                jboolean low_latency_input,
                                                jboolean pro_audio,
                                                jint output_buffer_size,
                                                jint input_buffer_size,
                                                jlong native_audio_manager) {
  webrtc::AudioManager* this_object =
      reinterpret_cast<webrtc::AudioManager*>(native_audio_manager);
  this_object->OnCacheAudioParameters(
      env, sample_rate, output_channels, input_channels, hardware_aec,
      hardware_agc, hardware_ns, low_latency_output, low_latency_input,
      pro_audio, output_buffer_size, input_buffer_size);
}

void AudioManager::OnCacheAudioParameters(JNIEnv* env,
                                          jint sample_rate,
                                          jint output_channels,
                                          jint input_channels,
                                          jboolean hardware_aec,
                                          jboolean hardware_agc,
                                          jboolean hardware_ns,
                                          jboolean low_latency_output,
                                          jboolean low_latency_input,
                                          jboolean pro_audio,
                                          jint output_buffer_size,
                                          jint input_buffer_size) {
  ALOGD("OnCacheAudioParameters%s", GetThreadInfo().c_str());
  ALOGD("hardware_aec: %d", hardware_aec);
  ALOGD("hardware_agc: %d", hardware_agc);
  ALOGD("hardware_ns: %d", hardware_ns);
  ALOGD("low_latency_output: %d", low_latency_output);
  ALOGD("low_latency_input: %d", low_latency_input);
  ALOGD("pro_audio: %d", pro_audio);
  ALOGD("sample_rate: %d", sample_rate);
  ALOGD("output_channels: %d", output_channels);
  ALOGD("input_channels: %d", input_channels);
  ALOGD("output_buffer_size: %d", output_buffer_size);
  ALOGD("input_buffer_size: %d", input_buffer_size);
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  hardware_aec_ = hardware_aec;
  hardware_agc_ = hardware_agc;
  hardware_ns_ = hardware_ns;
  low_latency_playout_ = low_latency_output;
  low_latency_record_ = low_latency_input;
  pro_audio_ = pro_audio;
  playout_parameters_.reset(sample_rate, static_cast<size_t>(output_channels),
                            static_cast<size_t>(output_buffer_size));
  record_parameters_.reset(sample_rate, static_cast<size_t>(input_channels),
                           static_cast<size_t>(input_buffer_size));
}

const AudioParameters& AudioManager::GetPlayoutAudioParameters() {
  RTC_CHECK(playout_parameters_.is_valid());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return playout_parameters_;
}

const AudioParameters& AudioManager::GetRecordAudioParameters() {
  RTC_CHECK(record_parameters_.is_valid());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return record_parameters_;
}

}  // namespace webrtc
