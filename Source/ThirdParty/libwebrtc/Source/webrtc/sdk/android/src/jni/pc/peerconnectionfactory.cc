/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/peerconnectionfactory.h"

#include <memory>
#include <utility>

#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "media/base/mediaengine.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/utility/include/jvm_android.h"
// We don't depend on the audio processing module implementation.
// The user may pass in a nullptr.
#include "modules/audio_processing/include/audio_processing.h"  // nogncheck
#include "rtc_base/event_tracer.h"
#include "rtc_base/stringutils.h"
#include "rtc_base/thread.h"
#include "sdk/android/generated_peerconnection_jni/jni/PeerConnectionFactory_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/logging/logsink.h"
#include "sdk/android/src/jni/pc/androidnetworkmonitor.h"
#include "sdk/android/src/jni/pc/audio.h"
#include "sdk/android/src/jni/pc/icecandidate.h"
#include "sdk/android/src/jni/pc/media.h"
#include "sdk/android/src/jni/pc/ownedfactoryandthreads.h"
#include "sdk/android/src/jni/pc/peerconnection.h"
#include "sdk/android/src/jni/pc/sslcertificateverifierwrapper.h"
#include "sdk/android/src/jni/pc/video.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace jni {

namespace {

PeerConnectionFactoryInterface::Options
JavaToNativePeerConnectionFactoryOptions(JNIEnv* jni,
                                         const JavaRef<jobject>& options) {
  int network_ignore_mask = Java_Options_getNetworkIgnoreMask(jni, options);
  bool disable_encryption = Java_Options_getDisableEncryption(jni, options);
  bool disable_network_monitor =
      Java_Options_getDisableNetworkMonitor(jni, options);

  PeerConnectionFactoryInterface::Options native_options;

  // This doesn't necessarily match the c++ version of this struct; feel free
  // to add more parameters as necessary.
  native_options.network_ignore_mask = network_ignore_mask;
  native_options.disable_encryption = disable_encryption;
  native_options.disable_network_monitor = disable_network_monitor;

  return native_options;
}

// Place static objects into a container that gets leaked so we avoid
// non-trivial destructor.
struct StaticObjectContainer {
  // Field trials initialization string
  std::unique_ptr<std::string> field_trials_init_string;
  // Set in PeerConnectionFactory_InjectLoggable().
  std::unique_ptr<JNILogSink> jni_log_sink;
};

StaticObjectContainer& GetStaticObjects() {
  static StaticObjectContainer* static_objects = new StaticObjectContainer();
  return *static_objects;
}

}  // namespace

// Note: Some of the video-specific PeerConnectionFactory methods are
// implemented in "video.cc". This is done so that if an application
// doesn't need video support, it can just link with "null_video.cc"
// instead of "video.cc", which doesn't bring in the video-specific
// dependencies.

// Set in PeerConnectionFactory_initializeAndroidGlobals().
static bool factory_static_initialized = false;

void PeerConnectionFactoryNetworkThreadReady() {
  RTC_LOG(LS_INFO) << "Network thread JavaCallback";
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_PeerConnectionFactory_onNetworkThreadReady(env);
}

void PeerConnectionFactoryWorkerThreadReady() {
  RTC_LOG(LS_INFO) << "Worker thread JavaCallback";
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_PeerConnectionFactory_onWorkerThreadReady(env);
}

void PeerConnectionFactorySignalingThreadReady() {
  RTC_LOG(LS_INFO) << "Signaling thread JavaCallback";
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_PeerConnectionFactory_onSignalingThreadReady(env);
}

jobject NativeToJavaPeerConnectionFactory(
    JNIEnv* jni,
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> pcf,
    std::unique_ptr<rtc::Thread> network_thread,
    std::unique_ptr<rtc::Thread> worker_thread,
    std::unique_ptr<rtc::Thread> signaling_thread,
    rtc::NetworkMonitorFactory* network_monitor_factory) {
  jni::OwnedFactoryAndThreads* owned_factory = new jni::OwnedFactoryAndThreads(
      std::move(network_thread), std::move(worker_thread),
      std::move(signaling_thread), network_monitor_factory, pcf.release());
  owned_factory->InvokeJavaCallbacksOnFactoryThreads();

  return Java_PeerConnectionFactory_Constructor(
             jni, NativeToJavaPointer(owned_factory))
      .Release();
}

static void JNI_PeerConnectionFactory_InitializeAndroidGlobals(
    JNIEnv* jni,
    const JavaParamRef<jclass>&) {
  if (!factory_static_initialized) {
    JVM::Initialize(GetJVM());
    factory_static_initialized = true;
  }
}

static void JNI_PeerConnectionFactory_InitializeFieldTrials(
    JNIEnv* jni,
    const JavaParamRef<jclass>&,
    const JavaParamRef<jstring>& j_trials_init_string) {
  std::unique_ptr<std::string>& field_trials_init_string =
      GetStaticObjects().field_trials_init_string;

  if (j_trials_init_string.is_null()) {
    field_trials_init_string = nullptr;
    field_trial::InitFieldTrialsFromString(nullptr);
    return;
  }
  field_trials_init_string = absl::make_unique<std::string>(
      JavaToNativeString(jni, j_trials_init_string));
  RTC_LOG(LS_INFO) << "initializeFieldTrials: " << *field_trials_init_string;
  field_trial::InitFieldTrialsFromString(field_trials_init_string->c_str());
}

static void JNI_PeerConnectionFactory_InitializeInternalTracer(
    JNIEnv* jni,
    const JavaParamRef<jclass>&) {
  rtc::tracing::SetupInternalTracer();
}

static ScopedJavaLocalRef<jstring>
JNI_PeerConnectionFactory_FindFieldTrialsFullName(
    JNIEnv* jni,
    const JavaParamRef<jclass>&,
    const JavaParamRef<jstring>& j_name) {
  return NativeToJavaString(
      jni, field_trial::FindFullName(JavaToStdString(jni, j_name)));
}

static jboolean JNI_PeerConnectionFactory_StartInternalTracingCapture(
    JNIEnv* jni,
    const JavaParamRef<jclass>&,
    const JavaParamRef<jstring>& j_event_tracing_filename) {
  if (j_event_tracing_filename.is_null())
    return false;

  const char* init_string =
      jni->GetStringUTFChars(j_event_tracing_filename.obj(), NULL);
  RTC_LOG(LS_INFO) << "Starting internal tracing to: " << init_string;
  bool ret = rtc::tracing::StartInternalCapture(init_string);
  jni->ReleaseStringUTFChars(j_event_tracing_filename.obj(), init_string);
  return ret;
}

static void JNI_PeerConnectionFactory_StopInternalTracingCapture(
    JNIEnv* jni,
    const JavaParamRef<jclass>&) {
  rtc::tracing::StopInternalCapture();
}

static void JNI_PeerConnectionFactory_ShutdownInternalTracer(
    JNIEnv* jni,
    const JavaParamRef<jclass>&) {
  rtc::tracing::ShutdownInternalTracer();
}

// Following parameters are optional:
// |audio_device_module|, |jencoder_factory|, |jdecoder_factory|,
// |audio_processor|, |media_transport_factory|, |fec_controller_factory|.
jlong CreatePeerConnectionFactoryForJava(
    JNIEnv* jni,
    const JavaParamRef<jobject>& jcontext,
    const JavaParamRef<jobject>& joptions,
    rtc::scoped_refptr<AudioDeviceModule> audio_device_module,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    const JavaParamRef<jobject>& jencoder_factory,
    const JavaParamRef<jobject>& jdecoder_factory,
    rtc::scoped_refptr<AudioProcessing> audio_processor,
    std::unique_ptr<FecControllerFactoryInterface> fec_controller_factory,
    std::unique_ptr<MediaTransportFactory> media_transport_factory) {
  // talk/ assumes pretty widely that the current Thread is ThreadManager'd, but
  // ThreadManager only WrapCurrentThread()s the thread where it is first
  // created.  Since the semantics around when auto-wrapping happens in
  // webrtc/rtc_base/ are convoluted, we simply wrap here to avoid having to
  // think about ramifications of auto-wrapping there.
  rtc::ThreadManager::Instance()->WrapCurrentThread();

  std::unique_ptr<rtc::Thread> network_thread =
      rtc::Thread::CreateWithSocketServer();
  network_thread->SetName("network_thread", nullptr);
  RTC_CHECK(network_thread->Start()) << "Failed to start thread";

  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  worker_thread->SetName("worker_thread", nullptr);
  RTC_CHECK(worker_thread->Start()) << "Failed to start thread";

  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  signaling_thread->SetName("signaling_thread", NULL);
  RTC_CHECK(signaling_thread->Start()) << "Failed to start thread";

  rtc::NetworkMonitorFactory* network_monitor_factory = nullptr;

  PeerConnectionFactoryInterface::Options options;
  bool has_options = !joptions.is_null();
  if (has_options) {
    options = JavaToNativePeerConnectionFactoryOptions(jni, joptions);
  }

  // Do not create network_monitor_factory only if the options are
  // provided and disable_network_monitor therein is set to true.
  if (!(has_options && options.disable_network_monitor)) {
    network_monitor_factory = new AndroidNetworkMonitorFactory();
    rtc::NetworkMonitorFactory::SetFactory(network_monitor_factory);
  }

  rtc::scoped_refptr<AudioMixer> audio_mixer = nullptr;
  std::unique_ptr<CallFactoryInterface> call_factory(CreateCallFactory());
  std::unique_ptr<RtcEventLogFactoryInterface> rtc_event_log_factory(
      CreateRtcEventLogFactory());

  std::unique_ptr<cricket::MediaEngineInterface> media_engine(CreateMediaEngine(
      audio_device_module, audio_encoder_factory, audio_decoder_factory,
      std::unique_ptr<VideoEncoderFactory>(
          CreateVideoEncoderFactory(jni, jencoder_factory)),
      std::unique_ptr<VideoDecoderFactory>(
          CreateVideoDecoderFactory(jni, jdecoder_factory)),
      audio_mixer, audio_processor));
  PeerConnectionFactoryDependencies dependencies;
  dependencies.network_thread = network_thread.get();
  dependencies.worker_thread = worker_thread.get();
  dependencies.signaling_thread = signaling_thread.get();
  dependencies.media_engine = std::move(media_engine);
  dependencies.call_factory = std::move(call_factory);
  dependencies.event_log_factory = std::move(rtc_event_log_factory);
  dependencies.fec_controller_factory = std::move(fec_controller_factory);
  dependencies.media_transport_factory = std::move(media_transport_factory);

  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory(
      CreateModularPeerConnectionFactory(std::move(dependencies)));

  RTC_CHECK(factory) << "Failed to create the peer connection factory; "
                     << "WebRTC/libjingle init likely failed on this device";
  // TODO(honghaiz): Maybe put the options as the argument of
  // CreatePeerConnectionFactory.
  if (has_options) {
    factory->SetOptions(options);
  }
  OwnedFactoryAndThreads* owned_factory = new OwnedFactoryAndThreads(
      std::move(network_thread), std::move(worker_thread),
      std::move(signaling_thread), network_monitor_factory, factory.release());
  owned_factory->InvokeJavaCallbacksOnFactoryThreads();
  return jlongFromPointer(owned_factory);
}

static jlong JNI_PeerConnectionFactory_CreatePeerConnectionFactory(
    JNIEnv* jni,
    const JavaParamRef<jclass>&,
    const JavaParamRef<jobject>& jcontext,
    const JavaParamRef<jobject>& joptions,
    jlong native_audio_device_module,
    jlong native_audio_encoder_factory,
    jlong native_audio_decoder_factory,
    const JavaParamRef<jobject>& jencoder_factory,
    const JavaParamRef<jobject>& jdecoder_factory,
    jlong native_audio_processor,
    jlong native_fec_controller_factory,
    jlong native_media_transport_factory) {
  rtc::scoped_refptr<AudioProcessing> audio_processor =
      reinterpret_cast<AudioProcessing*>(native_audio_processor);
  AudioEncoderFactory* audio_encoder_factory_ptr =
      reinterpret_cast<AudioEncoderFactory*>(native_audio_encoder_factory);
  rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory(
      audio_encoder_factory_ptr);
  // Release the caller's reference count.
  audio_encoder_factory->Release();
  AudioDecoderFactory* audio_decoder_factory_ptr =
      reinterpret_cast<AudioDecoderFactory*>(native_audio_decoder_factory);
  rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory(
      audio_decoder_factory_ptr);
  // Release the caller's reference count.
  audio_decoder_factory->Release();
  std::unique_ptr<FecControllerFactoryInterface> fec_controller_factory(
      reinterpret_cast<FecControllerFactoryInterface*>(
          native_fec_controller_factory));
  std::unique_ptr<MediaTransportFactory> media_transport_factory(
      reinterpret_cast<MediaTransportFactory*>(native_media_transport_factory));
  return CreatePeerConnectionFactoryForJava(
      jni, jcontext, joptions,
      reinterpret_cast<AudioDeviceModule*>(native_audio_device_module),
      audio_encoder_factory, audio_decoder_factory, jencoder_factory,
      jdecoder_factory,
      audio_processor ? audio_processor : CreateAudioProcessing(),
      std::move(fec_controller_factory), std::move(media_transport_factory));
}

static void JNI_PeerConnectionFactory_FreeFactory(JNIEnv*,
                                                  const JavaParamRef<jclass>&,
                                                  jlong j_p) {
  delete reinterpret_cast<OwnedFactoryAndThreads*>(j_p);
  field_trial::InitFieldTrialsFromString(nullptr);
  GetStaticObjects().field_trials_init_string = nullptr;
}

static void JNI_PeerConnectionFactory_InvokeThreadsCallbacks(
    JNIEnv*,
    const JavaParamRef<jclass>&,
    jlong j_p) {
  OwnedFactoryAndThreads* factory =
      reinterpret_cast<OwnedFactoryAndThreads*>(j_p);
  factory->InvokeJavaCallbacksOnFactoryThreads();
}

static jlong JNI_PeerConnectionFactory_CreateLocalMediaStream(
    JNIEnv* jni,
    const JavaParamRef<jclass>&,
    jlong native_factory,
    const JavaParamRef<jstring>& label) {
  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory(
      factoryFromJava(native_factory));
  rtc::scoped_refptr<MediaStreamInterface> stream(
      factory->CreateLocalMediaStream(JavaToStdString(jni, label)));
  return jlongFromPointer(stream.release());
}

static jlong JNI_PeerConnectionFactory_CreateAudioSource(
    JNIEnv* jni,
    const JavaParamRef<jclass>&,
    jlong native_factory,
    const JavaParamRef<jobject>& j_constraints) {
  std::unique_ptr<MediaConstraintsInterface> constraints =
      JavaToNativeMediaConstraints(jni, j_constraints);
  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory(
      factoryFromJava(native_factory));
  cricket::AudioOptions options;
  CopyConstraintsIntoAudioOptions(constraints.get(), &options);
  rtc::scoped_refptr<AudioSourceInterface> source(
      factory->CreateAudioSource(options));
  return jlongFromPointer(source.release());
}

jlong JNI_PeerConnectionFactory_CreateAudioTrack(
    JNIEnv* jni,
    const JavaParamRef<jclass>&,
    jlong native_factory,
    const JavaParamRef<jstring>& id,
    jlong native_source) {
  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory(
      factoryFromJava(native_factory));
  rtc::scoped_refptr<AudioTrackInterface> track(factory->CreateAudioTrack(
      JavaToStdString(jni, id),
      reinterpret_cast<AudioSourceInterface*>(native_source)));
  return jlongFromPointer(track.release());
}

static jboolean JNI_PeerConnectionFactory_StartAecDump(
    JNIEnv* jni,
    const JavaParamRef<jclass>&,
    jlong native_factory,
    jint file,
    jint filesize_limit_bytes) {
  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory(
      factoryFromJava(native_factory));
  return factory->StartAecDump(file, filesize_limit_bytes);
}

static void JNI_PeerConnectionFactory_StopAecDump(JNIEnv* jni,
                                                  const JavaParamRef<jclass>&,
                                                  jlong native_factory) {
  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory(
      factoryFromJava(native_factory));
  factory->StopAecDump();
}

static jlong JNI_PeerConnectionFactory_CreatePeerConnection(
    JNIEnv* jni,
    const JavaParamRef<jclass>&,
    jlong factory,
    const JavaParamRef<jobject>& j_rtc_config,
    const JavaParamRef<jobject>& j_constraints,
    jlong observer_p,
    const JavaParamRef<jobject>& j_sslCertificateVerifier) {
  rtc::scoped_refptr<PeerConnectionFactoryInterface> f(
      reinterpret_cast<PeerConnectionFactoryInterface*>(
          factoryFromJava(factory)));
  std::unique_ptr<PeerConnectionObserver> observer(
      reinterpret_cast<PeerConnectionObserver*>(observer_p));

  PeerConnectionInterface::RTCConfiguration rtc_config(
      PeerConnectionInterface::RTCConfigurationType::kAggressive);
  JavaToNativeRTCConfiguration(jni, j_rtc_config, &rtc_config);

  if (rtc_config.certificates.empty()) {
    // Generate non-default certificate.
    rtc::KeyType key_type = GetRtcConfigKeyType(jni, j_rtc_config);
    if (key_type != rtc::KT_DEFAULT) {
      rtc::scoped_refptr<rtc::RTCCertificate> certificate =
          rtc::RTCCertificateGenerator::GenerateCertificate(
              rtc::KeyParams(key_type), absl::nullopt);
      if (!certificate) {
        RTC_LOG(LS_ERROR) << "Failed to generate certificate. KeyType: "
                          << key_type;
        return 0;
      }
      rtc_config.certificates.push_back(certificate);
    }
  }

  std::unique_ptr<MediaConstraintsInterface> constraints;
  if (!j_constraints.is_null()) {
    constraints = JavaToNativeMediaConstraints(jni, j_constraints);
    CopyConstraintsIntoRtcConfiguration(constraints.get(), &rtc_config);
  }

  PeerConnectionDependencies peer_connection_dependencies(observer.get());
  if (!j_sslCertificateVerifier.is_null()) {
    peer_connection_dependencies.tls_cert_verifier =
        absl::make_unique<SSLCertificateVerifierWrapper>(
            jni, j_sslCertificateVerifier);
  }

  rtc::scoped_refptr<PeerConnectionInterface> pc(f->CreatePeerConnection(
      rtc_config, std::move(peer_connection_dependencies)));
  if (pc == nullptr) {
    return 0;
  }

  return jlongFromPointer(
      new OwnedPeerConnection(pc, std::move(observer), std::move(constraints)));
}

static jlong JNI_PeerConnectionFactory_CreateVideoSource(
    JNIEnv* jni,
    const JavaParamRef<jclass>&,
    jlong native_factory,
    jboolean is_screencast,
    jboolean align_timestamps) {
  OwnedFactoryAndThreads* factory =
      reinterpret_cast<OwnedFactoryAndThreads*>(native_factory);
  return jlongFromPointer(CreateVideoSource(jni, factory->signaling_thread(),
                                            factory->worker_thread(),
                                            is_screencast, align_timestamps));
}

static jlong JNI_PeerConnectionFactory_CreateVideoTrack(
    JNIEnv* jni,
    const JavaParamRef<jclass>&,
    jlong native_factory,
    const JavaParamRef<jstring>& id,
    jlong native_source) {
  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory(
      factoryFromJava(native_factory));
  rtc::scoped_refptr<VideoTrackInterface> track(factory->CreateVideoTrack(
      JavaToStdString(jni, id),
      reinterpret_cast<VideoTrackSourceInterface*>(native_source)));
  return jlongFromPointer(track.release());
}

static jlong JNI_PeerConnectionFactory_GetNativePeerConnectionFactory(
    JNIEnv* jni,
    const JavaParamRef<jclass>&,
    jlong native_factory) {
  return jlongFromPointer(factoryFromJava(native_factory));
}

static void JNI_PeerConnectionFactory_InjectLoggable(
    JNIEnv* jni,
    const JavaParamRef<jclass>&,
    const JavaParamRef<jobject>& j_logging,
    jint nativeSeverity) {
  std::unique_ptr<JNILogSink>& jni_log_sink = GetStaticObjects().jni_log_sink;

  // If there is already a LogSink, remove it from LogMessage.
  if (jni_log_sink) {
    rtc::LogMessage::RemoveLogToStream(jni_log_sink.get());
  }
  jni_log_sink = absl::make_unique<JNILogSink>(jni, j_logging);
  rtc::LogMessage::AddLogToStream(
      jni_log_sink.get(), static_cast<rtc::LoggingSeverity>(nativeSeverity));
  rtc::LogMessage::LogToDebug(rtc::LS_NONE);
}

static void JNI_PeerConnectionFactory_DeleteLoggable(
    JNIEnv* jni,
    const JavaParamRef<jclass>&) {
  std::unique_ptr<JNILogSink>& jni_log_sink = GetStaticObjects().jni_log_sink;

  if (jni_log_sink) {
    rtc::LogMessage::RemoveLogToStream(jni_log_sink.get());
    jni_log_sink.reset();
  }
}

}  // namespace jni
}  // namespace webrtc
