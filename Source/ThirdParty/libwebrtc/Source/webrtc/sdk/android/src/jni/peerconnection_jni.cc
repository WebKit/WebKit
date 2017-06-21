/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Hints for future visitors:
// This entire file is an implementation detail of the org.webrtc Java package,
// the most interesting bits of which are org.webrtc.PeerConnection{,Factory}.
// The layout of this file is roughly:
// - various helper C++ functions & classes that wrap Java counterparts and
//   expose a C++ interface that can be passed to the C++ PeerConnection APIs
// - implementations of methods declared "static" in the Java package (named
//   things like Java_org_webrtc_OMG_Can_This_Name_Be_Any_Longer, prescribed by
//   the JNI spec).
//
// Lifecycle notes: objects are owned where they will be called; in other words
// FooObservers are owned by C++-land, and user-callable objects (e.g.
// PeerConnection and VideoTrack) are owned by Java-land.
// When this file allocates C++ RefCountInterfaces it AddRef()s an artificial
// ref simulating the jlong held in Java-land, and then Release()s the ref in
// the respective free call.  Sometimes this AddRef is implicit in the
// construction of a scoped_refptr<> which is then .release()d.
// Any persistent (non-local) references from C++ to Java must be global or weak
// (in which case they must be checked before use)!
//
// Exception notes: pretty much all JNI calls can throw Java exceptions, so each
// call through a JNIEnv* pointer needs to be followed by an ExceptionCheck()
// call.  In this file this is done in CHECK_EXCEPTION, making for much easier
// debugging in case of failure (the alternative is to wait for control to
// return to the Java frame that called code in this file, at which point it's
// impossible to tell which JNI call broke).

#include <jni.h>
#undef JNIEXPORT
#define JNIEXPORT __attribute__((visibility("default")))

#include <limits>
#include <memory>
#include <utility>

#include "webrtc/api/mediaconstraintsinterface.h"
#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/api/rtpreceiverinterface.h"
#include "webrtc/api/rtpsenderinterface.h"
#include "webrtc/api/videosourceproxy.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/event_tracer.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/logsinks.h"
#include "webrtc/base/messagequeue.h"
#include "webrtc/base/networkmonitor.h"
#include "webrtc/base/rtccertificategenerator.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/media/base/mediaengine.h"
#include "webrtc/media/base/videocapturer.h"
#include "webrtc/modules/utility/include/jvm_android.h"
#include "webrtc/pc/webrtcsdp.h"
#include "webrtc/sdk/android/src/jni/androidnetworkmonitor_jni.h"
// Adding 'nogncheck' to disable the gn include headers check.
// We don't want to always depend on audio and video related targets.
#include "webrtc/sdk/android/src/jni/androidvideotracksource.h"  // nogncheck
#include "webrtc/sdk/android/src/jni/audio_jni.h"
#include "webrtc/sdk/android/src/jni/classreferenceholder.h"
#include "webrtc/sdk/android/src/jni/jni_helpers.h"
#include "webrtc/sdk/android/src/jni/media_jni.h"
#include "webrtc/sdk/android/src/jni/ownedfactoryandthreads.h"
#include "webrtc/sdk/android/src/jni/rtcstatscollectorcallbackwrapper.h"
#include "webrtc/sdk/android/src/jni/video_jni.h"
#include "webrtc/system_wrappers/include/field_trial.h"
// Adding 'nogncheck' to disable the gn include headers check.
// We don't want to depend on 'system_wrappers:field_trial_default' because
// clients should be able to provide their own implementation.
#include "webrtc/system_wrappers/include/field_trial_default.h" // nogncheck
#include "webrtc/system_wrappers/include/logcat_trace_context.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/voice_engine/include/voe_base.h"  // nogncheck

using cricket::WebRtcVideoDecoderFactory;
using cricket::WebRtcVideoEncoderFactory;
using rtc::Bind;
using rtc::Thread;
using rtc::ThreadManager;
using webrtc::AudioSourceInterface;
using webrtc::AudioTrackInterface;
using webrtc::AudioTrackVector;
using webrtc::CreateSessionDescriptionObserver;
using webrtc::DataBuffer;
using webrtc::DataChannelInit;
using webrtc::DataChannelInterface;
using webrtc::DataChannelObserver;
using webrtc::DtmfSenderInterface;
using webrtc::IceCandidateInterface;
using webrtc::LogcatTraceContext;
using webrtc::MediaConstraintsInterface;
using webrtc::MediaSourceInterface;
using webrtc::MediaStreamInterface;
using webrtc::MediaStreamTrackInterface;
using webrtc::PeerConnectionFactoryInterface;
using webrtc::PeerConnectionInterface;
using webrtc::PeerConnectionObserver;
using webrtc::RtpReceiverInterface;
using webrtc::RtpReceiverObserverInterface;
using webrtc::RtpSenderInterface;
using webrtc::SessionDescriptionInterface;
using webrtc::SetSessionDescriptionObserver;
using webrtc::StatsObserver;
using webrtc::StatsReport;
using webrtc::StatsReports;
using webrtc::VideoTrackInterface;

namespace webrtc_jni {

// Field trials initialization string
static char *field_trials_init_string = NULL;

// Set in PeerConnectionFactory_initializeAndroidGlobals().
static bool factory_static_initialized = false;
static bool video_hw_acceleration_enabled = true;

// Return the (singleton) Java Enum object corresponding to |index|;
// |state_class_fragment| is something like "MediaSource$State".
static jobject JavaEnumFromIndex(
    JNIEnv* jni, const std::string& state_class_fragment, int index) {
  const std::string state_class = "org/webrtc/" + state_class_fragment;
  return JavaEnumFromIndex(jni, FindClass(jni, state_class.c_str()),
                           state_class, index);
}

static DataChannelInit JavaDataChannelInitToNative(
    JNIEnv* jni, jobject j_init) {
  DataChannelInit init;

  jclass j_init_class = FindClass(jni, "org/webrtc/DataChannel$Init");
  jfieldID ordered_id = GetFieldID(jni, j_init_class, "ordered", "Z");
  jfieldID max_retransmit_time_id =
      GetFieldID(jni, j_init_class, "maxRetransmitTimeMs", "I");
  jfieldID max_retransmits_id =
      GetFieldID(jni, j_init_class, "maxRetransmits", "I");
  jfieldID protocol_id =
      GetFieldID(jni, j_init_class, "protocol", "Ljava/lang/String;");
  jfieldID negotiated_id = GetFieldID(jni, j_init_class, "negotiated", "Z");
  jfieldID id_id = GetFieldID(jni, j_init_class, "id", "I");

  init.ordered = GetBooleanField(jni, j_init, ordered_id);
  init.maxRetransmitTime = GetIntField(jni, j_init, max_retransmit_time_id);
  init.maxRetransmits = GetIntField(jni, j_init, max_retransmits_id);
  init.protocol = JavaToStdString(
      jni, GetStringField(jni, j_init, protocol_id));
  init.negotiated = GetBooleanField(jni, j_init, negotiated_id);
  init.id = GetIntField(jni, j_init, id_id);

  return init;
}

static cricket::MediaType JavaMediaTypeToJsepMediaType(JNIEnv* jni,
                                                       jobject j_media_type) {
  jclass j_media_type_class =
      FindClass(jni, "org/webrtc/MediaStreamTrack$MediaType");
  jmethodID j_name_id =
      GetMethodID(jni, j_media_type_class, "name", "()Ljava/lang/String;");
  jstring j_type_string =
      (jstring)jni->CallObjectMethod(j_media_type, j_name_id);
  CHECK_EXCEPTION(jni) << "error during CallObjectMethod";
  std::string type_string = JavaToStdString(jni, j_type_string);

  RTC_DCHECK(type_string == "MEDIA_TYPE_AUDIO" ||
             type_string == "MEDIA_TYPE_VIDEO")
      << "Media type: " << type_string;
  return type_string == "MEDIA_TYPE_AUDIO" ? cricket::MEDIA_TYPE_AUDIO
                                           : cricket::MEDIA_TYPE_VIDEO;
}

static jobject JsepMediaTypeToJavaMediaType(JNIEnv* jni,
                                            cricket::MediaType media_type) {
  jclass j_media_type_class =
      FindClass(jni, "org/webrtc/MediaStreamTrack$MediaType");

  const char* media_type_str = nullptr;
  switch (media_type) {
    case cricket::MEDIA_TYPE_AUDIO:
      media_type_str = "MEDIA_TYPE_AUDIO";
      break;
    case cricket::MEDIA_TYPE_VIDEO:
      media_type_str = "MEDIA_TYPE_VIDEO";
      break;
    case cricket::MEDIA_TYPE_DATA:
      RTC_NOTREACHED();
      break;
  }
  jfieldID j_media_type_fid =
      GetStaticFieldID(jni, j_media_type_class, media_type_str,
                       "Lorg/webrtc/MediaStreamTrack$MediaType;");
  return GetStaticObjectField(jni, j_media_type_class, j_media_type_fid);
}

class ConstraintsWrapper;

// Adapter between the C++ PeerConnectionObserver interface and the Java
// PeerConnection.Observer interface.  Wraps an instance of the Java interface
// and dispatches C++ callbacks to Java.
class PCOJava : public PeerConnectionObserver {
 public:
  PCOJava(JNIEnv* jni, jobject j_observer)
      : j_observer_global_(jni, j_observer),
        j_observer_class_(jni, GetObjectClass(jni, *j_observer_global_)),
        j_media_stream_class_(jni, FindClass(jni, "org/webrtc/MediaStream")),
        j_media_stream_ctor_(
            GetMethodID(jni, *j_media_stream_class_, "<init>", "(J)V")),
        j_audio_track_class_(jni, FindClass(jni, "org/webrtc/AudioTrack")),
        j_audio_track_ctor_(
            GetMethodID(jni, *j_audio_track_class_, "<init>", "(J)V")),
        j_video_track_class_(jni, FindClass(jni, "org/webrtc/VideoTrack")),
        j_video_track_ctor_(
            GetMethodID(jni, *j_video_track_class_, "<init>", "(J)V")),
        j_data_channel_class_(jni, FindClass(jni, "org/webrtc/DataChannel")),
        j_data_channel_ctor_(
            GetMethodID(jni, *j_data_channel_class_, "<init>", "(J)V")),
        j_rtp_receiver_class_(jni, FindClass(jni, "org/webrtc/RtpReceiver")),
        j_rtp_receiver_ctor_(
            GetMethodID(jni, *j_rtp_receiver_class_, "<init>", "(J)V")) {}

  virtual ~PCOJava() {
    ScopedLocalRefFrame local_ref_frame(jni());
    while (!remote_streams_.empty())
      DisposeRemoteStream(remote_streams_.begin());
    while (!rtp_receivers_.empty())
      DisposeRtpReceiver(rtp_receivers_.begin());
  }

  void OnIceCandidate(const IceCandidateInterface* candidate) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    std::string sdp;
    RTC_CHECK(candidate->ToString(&sdp)) << "got so far: " << sdp;
    jclass candidate_class = FindClass(jni(), "org/webrtc/IceCandidate");
    jmethodID ctor = GetMethodID(
        jni(), candidate_class, "<init>",
        "(Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;)V");
    jstring j_mid = JavaStringFromStdString(jni(), candidate->sdp_mid());
    jstring j_sdp = JavaStringFromStdString(jni(), sdp);
    jstring j_url =
        JavaStringFromStdString(jni(), candidate->candidate().url());
    jobject j_candidate =
        jni()->NewObject(candidate_class, ctor, j_mid,
                         candidate->sdp_mline_index(), j_sdp, j_url);
    CHECK_EXCEPTION(jni()) << "error during NewObject";
    jmethodID m = GetMethodID(jni(), *j_observer_class_,
                              "onIceCandidate", "(Lorg/webrtc/IceCandidate;)V");
    jni()->CallVoidMethod(*j_observer_global_, m, j_candidate);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

  void OnIceCandidatesRemoved(
      const std::vector<cricket::Candidate>& candidates) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    jobjectArray candidates_array = ToJavaCandidateArray(jni(), candidates);
    jmethodID m =
        GetMethodID(jni(), *j_observer_class_, "onIceCandidatesRemoved",
                    "([Lorg/webrtc/IceCandidate;)V");
    jni()->CallVoidMethod(*j_observer_global_, m, candidates_array);
    CHECK_EXCEPTION(jni()) << "Error during CallVoidMethod";
  }

  void OnSignalingChange(
      PeerConnectionInterface::SignalingState new_state) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    jmethodID m = GetMethodID(
        jni(), *j_observer_class_, "onSignalingChange",
        "(Lorg/webrtc/PeerConnection$SignalingState;)V");
    jobject new_state_enum =
        JavaEnumFromIndex(jni(), "PeerConnection$SignalingState", new_state);
    jni()->CallVoidMethod(*j_observer_global_, m, new_state_enum);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

  void OnIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    jmethodID m = GetMethodID(
        jni(), *j_observer_class_, "onIceConnectionChange",
        "(Lorg/webrtc/PeerConnection$IceConnectionState;)V");
    jobject new_state_enum = JavaEnumFromIndex(
        jni(), "PeerConnection$IceConnectionState", new_state);
    jni()->CallVoidMethod(*j_observer_global_, m, new_state_enum);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

  void OnIceConnectionReceivingChange(bool receiving) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    jmethodID m = GetMethodID(
        jni(), *j_observer_class_, "onIceConnectionReceivingChange", "(Z)V");
    jni()->CallVoidMethod(*j_observer_global_, m, receiving);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

  void OnIceGatheringChange(
      PeerConnectionInterface::IceGatheringState new_state) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    jmethodID m = GetMethodID(
        jni(), *j_observer_class_, "onIceGatheringChange",
        "(Lorg/webrtc/PeerConnection$IceGatheringState;)V");
    jobject new_state_enum = JavaEnumFromIndex(
        jni(), "PeerConnection$IceGatheringState", new_state);
    jni()->CallVoidMethod(*j_observer_global_, m, new_state_enum);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

  void OnAddStream(rtc::scoped_refptr<MediaStreamInterface> stream) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    // The stream could be added into the remote_streams_ map when calling
    // OnAddTrack.
    jobject j_stream = GetOrCreateJavaStream(stream);

    for (const auto& track : stream->GetAudioTracks()) {
      jstring id = JavaStringFromStdString(jni(), track->id());
      // Java AudioTrack holds one reference. Corresponding Release() is in
      // MediaStreamTrack_free, triggered by AudioTrack.dispose().
      track->AddRef();
      jobject j_track =
          jni()->NewObject(*j_audio_track_class_, j_audio_track_ctor_,
                           reinterpret_cast<jlong>(track.get()), id);
      CHECK_EXCEPTION(jni()) << "error during NewObject";
      jfieldID audio_tracks_id = GetFieldID(jni(),
                                            *j_media_stream_class_,
                                            "audioTracks",
                                            "Ljava/util/LinkedList;");
      jobject audio_tracks = GetObjectField(jni(), j_stream, audio_tracks_id);
      jmethodID add = GetMethodID(jni(),
                                  GetObjectClass(jni(), audio_tracks),
                                  "add",
                                  "(Ljava/lang/Object;)Z");
      jboolean added = jni()->CallBooleanMethod(audio_tracks, add, j_track);
      CHECK_EXCEPTION(jni()) << "error during CallBooleanMethod";
      RTC_CHECK(added);
    }

    for (const auto& track : stream->GetVideoTracks()) {
      jstring id = JavaStringFromStdString(jni(), track->id());
      // Java VideoTrack holds one reference. Corresponding Release() is in
      // MediaStreamTrack_free, triggered by VideoTrack.dispose().
      track->AddRef();
      jobject j_track =
          jni()->NewObject(*j_video_track_class_, j_video_track_ctor_,
                           reinterpret_cast<jlong>(track.get()), id);
      CHECK_EXCEPTION(jni()) << "error during NewObject";
      jfieldID video_tracks_id = GetFieldID(jni(),
                                            *j_media_stream_class_,
                                            "videoTracks",
                                            "Ljava/util/LinkedList;");
      jobject video_tracks = GetObjectField(jni(), j_stream, video_tracks_id);
      jmethodID add = GetMethodID(jni(),
                                  GetObjectClass(jni(), video_tracks),
                                  "add",
                                  "(Ljava/lang/Object;)Z");
      jboolean added = jni()->CallBooleanMethod(video_tracks, add, j_track);
      CHECK_EXCEPTION(jni()) << "error during CallBooleanMethod";
      RTC_CHECK(added);
    }

    jmethodID m = GetMethodID(jni(), *j_observer_class_, "onAddStream",
                              "(Lorg/webrtc/MediaStream;)V");
    jni()->CallVoidMethod(*j_observer_global_, m, j_stream);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

  void OnRemoveStream(
      rtc::scoped_refptr<MediaStreamInterface> stream) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    NativeToJavaStreamsMap::iterator it = remote_streams_.find(stream);
    RTC_CHECK(it != remote_streams_.end()) << "unexpected stream: " << std::hex
                                           << stream;
    jobject j_stream = it->second;
    jmethodID m = GetMethodID(jni(), *j_observer_class_, "onRemoveStream",
                              "(Lorg/webrtc/MediaStream;)V");
    jni()->CallVoidMethod(*j_observer_global_, m, j_stream);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
    // Release the refptr reference so that DisposeRemoteStream can assert
    // it removes the final reference.
    stream = nullptr;
    DisposeRemoteStream(it);
  }

  void OnDataChannel(
      rtc::scoped_refptr<DataChannelInterface> channel) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    jobject j_channel =
        jni()->NewObject(*j_data_channel_class_, j_data_channel_ctor_,
                         jlongFromPointer(channel.get()));
    CHECK_EXCEPTION(jni()) << "error during NewObject";

    jmethodID m = GetMethodID(jni(), *j_observer_class_, "onDataChannel",
                              "(Lorg/webrtc/DataChannel;)V");
    jni()->CallVoidMethod(*j_observer_global_, m, j_channel);

    // Channel is now owned by Java object, and will be freed from
    // DataChannel.dispose().  Important that this be done _after_ the
    // CallVoidMethod above as Java code might call back into native code and be
    // surprised to see a refcount of 2.
    int bumped_count = channel->AddRef();
    RTC_CHECK(bumped_count == 2) << "Unexpected refcount OnDataChannel";

    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

  void OnRenegotiationNeeded() override {
    ScopedLocalRefFrame local_ref_frame(jni());
    jmethodID m =
        GetMethodID(jni(), *j_observer_class_, "onRenegotiationNeeded", "()V");
    jni()->CallVoidMethod(*j_observer_global_, m);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

  void OnAddTrack(rtc::scoped_refptr<RtpReceiverInterface> receiver,
                  const std::vector<rtc::scoped_refptr<MediaStreamInterface>>&
                      streams) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    jobject j_rtp_receiver =
        jni()->NewObject(*j_rtp_receiver_class_, j_rtp_receiver_ctor_,
                         jlongFromPointer(receiver.get()));
    CHECK_EXCEPTION(jni()) << "error during NewObject";
    receiver->AddRef();
    rtp_receivers_[receiver] = NewGlobalRef(jni(), j_rtp_receiver);

    jobjectArray j_stream_array = ToJavaMediaStreamArray(jni(), streams);
    jmethodID m =
        GetMethodID(jni(), *j_observer_class_, "onAddTrack",
                    "(Lorg/webrtc/RtpReceiver;[Lorg/webrtc/MediaStream;)V");
    jni()->CallVoidMethod(*j_observer_global_, m, j_rtp_receiver,
                          j_stream_array);
    CHECK_EXCEPTION(jni()) << "Error during CallVoidMethod";
  }

  void SetConstraints(ConstraintsWrapper* constraints) {
    RTC_CHECK(!constraints_.get()) << "constraints already set!";
    constraints_.reset(constraints);
  }

  const ConstraintsWrapper* constraints() { return constraints_.get(); }

 private:
  typedef std::map<MediaStreamInterface*, jobject> NativeToJavaStreamsMap;
  typedef std::map<RtpReceiverInterface*, jobject> NativeToJavaRtpReceiverMap;

  void DisposeRemoteStream(const NativeToJavaStreamsMap::iterator& it) {
    jobject j_stream = it->second;
    remote_streams_.erase(it);
    jni()->CallVoidMethod(
        j_stream, GetMethodID(jni(), *j_media_stream_class_, "dispose", "()V"));
    CHECK_EXCEPTION(jni()) << "error during MediaStream.dispose()";
    DeleteGlobalRef(jni(), j_stream);
  }

  void DisposeRtpReceiver(const NativeToJavaRtpReceiverMap::iterator& it) {
    jobject j_rtp_receiver = it->second;
    rtp_receivers_.erase(it);
    jni()->CallVoidMethod(
        j_rtp_receiver,
        GetMethodID(jni(), *j_rtp_receiver_class_, "dispose", "()V"));
    CHECK_EXCEPTION(jni()) << "error during RtpReceiver.dispose()";
    DeleteGlobalRef(jni(), j_rtp_receiver);
  }

  jobject ToJavaCandidate(JNIEnv* jni,
                          jclass* candidate_class,
                          const cricket::Candidate& candidate) {
    std::string sdp = webrtc::SdpSerializeCandidate(candidate);
    RTC_CHECK(!sdp.empty()) << "got an empty ICE candidate";
    jmethodID ctor = GetMethodID(jni, *candidate_class, "<init>",
                                 "(Ljava/lang/String;ILjava/lang/String;)V");
    jstring j_mid = JavaStringFromStdString(jni, candidate.transport_name());
    jstring j_sdp = JavaStringFromStdString(jni, sdp);
    // sdp_mline_index is not used, pass an invalid value -1.
    jobject j_candidate =
        jni->NewObject(*candidate_class, ctor, j_mid, -1, j_sdp);
    CHECK_EXCEPTION(jni) << "error during Java Candidate NewObject";
    return j_candidate;
  }

  jobjectArray ToJavaCandidateArray(
      JNIEnv* jni,
      const std::vector<cricket::Candidate>& candidates) {
    jclass candidate_class = FindClass(jni, "org/webrtc/IceCandidate");
    jobjectArray java_candidates =
        jni->NewObjectArray(candidates.size(), candidate_class, NULL);
    int i = 0;
    for (const cricket::Candidate& candidate : candidates) {
      jobject j_candidate = ToJavaCandidate(jni, &candidate_class, candidate);
      jni->SetObjectArrayElement(java_candidates, i++, j_candidate);
    }
    return java_candidates;
  }

  jobjectArray ToJavaMediaStreamArray(
      JNIEnv* jni,
      const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams) {
    jobjectArray java_streams =
        jni->NewObjectArray(streams.size(), *j_media_stream_class_, nullptr);
    CHECK_EXCEPTION(jni) << "error during NewObjectArray";
    for (size_t i = 0; i < streams.size(); ++i) {
      jobject j_stream = GetOrCreateJavaStream(streams[i]);
      jni->SetObjectArrayElement(java_streams, i, j_stream);
    }
    return java_streams;
  }

  // If the NativeToJavaStreamsMap contains the stream, return it.
  // Otherwise, create a new Java MediaStream.
  jobject GetOrCreateJavaStream(
      const rtc::scoped_refptr<MediaStreamInterface>& stream) {
    NativeToJavaStreamsMap::iterator it = remote_streams_.find(stream);
    if (it != remote_streams_.end()) {
      return it->second;
    }

    // Java MediaStream holds one reference. Corresponding Release() is in
    // MediaStream_free, triggered by MediaStream.dispose().
    stream->AddRef();
    jobject j_stream =
        jni()->NewObject(*j_media_stream_class_, j_media_stream_ctor_,
                         reinterpret_cast<jlong>(stream.get()));
    CHECK_EXCEPTION(jni()) << "error during NewObject";

    remote_streams_[stream] = NewGlobalRef(jni(), j_stream);
    return j_stream;
  }

  JNIEnv* jni() {
    return AttachCurrentThreadIfNeeded();
  }

  const ScopedGlobalRef<jobject> j_observer_global_;
  const ScopedGlobalRef<jclass> j_observer_class_;
  const ScopedGlobalRef<jclass> j_media_stream_class_;
  const jmethodID j_media_stream_ctor_;
  const ScopedGlobalRef<jclass> j_audio_track_class_;
  const jmethodID j_audio_track_ctor_;
  const ScopedGlobalRef<jclass> j_video_track_class_;
  const jmethodID j_video_track_ctor_;
  const ScopedGlobalRef<jclass> j_data_channel_class_;
  const jmethodID j_data_channel_ctor_;
  const ScopedGlobalRef<jclass> j_rtp_receiver_class_;
  const jmethodID j_rtp_receiver_ctor_;
  // C++ -> Java remote streams. The stored jobects are global refs and must be
  // manually deleted upon removal. Use DisposeRemoteStream().
  NativeToJavaStreamsMap remote_streams_;
  NativeToJavaRtpReceiverMap rtp_receivers_;
  std::unique_ptr<ConstraintsWrapper> constraints_;
};

// Wrapper for a Java MediaConstraints object.  Copies all needed data so when
// the constructor returns the Java object is no longer needed.
class ConstraintsWrapper : public MediaConstraintsInterface {
 public:
  ConstraintsWrapper(JNIEnv* jni, jobject j_constraints) {
    PopulateConstraintsFromJavaPairList(
        jni, j_constraints, "mandatory", &mandatory_);
    PopulateConstraintsFromJavaPairList(
        jni, j_constraints, "optional", &optional_);
  }

  virtual ~ConstraintsWrapper() {}

  // MediaConstraintsInterface.
  const Constraints& GetMandatory() const override { return mandatory_; }

  const Constraints& GetOptional() const override { return optional_; }

 private:
  // Helper for translating a List<Pair<String, String>> to a Constraints.
  static void PopulateConstraintsFromJavaPairList(
      JNIEnv* jni, jobject j_constraints,
      const char* field_name, Constraints* field) {
    jfieldID j_id = GetFieldID(jni,
        GetObjectClass(jni, j_constraints), field_name, "Ljava/util/List;");
    jobject j_list = GetObjectField(jni, j_constraints, j_id);
    for (jobject entry : Iterable(jni, j_list)) {
      jmethodID get_key = GetMethodID(jni,
          GetObjectClass(jni, entry), "getKey", "()Ljava/lang/String;");
      jstring j_key = reinterpret_cast<jstring>(
          jni->CallObjectMethod(entry, get_key));
      CHECK_EXCEPTION(jni) << "error during CallObjectMethod";
      jmethodID get_value = GetMethodID(jni,
          GetObjectClass(jni, entry), "getValue", "()Ljava/lang/String;");
      jstring j_value = reinterpret_cast<jstring>(
          jni->CallObjectMethod(entry, get_value));
      CHECK_EXCEPTION(jni) << "error during CallObjectMethod";
      field->push_back(Constraint(JavaToStdString(jni, j_key),
                                  JavaToStdString(jni, j_value)));
    }
  }

  Constraints mandatory_;
  Constraints optional_;
};

static jobject JavaSdpFromNativeSdp(
    JNIEnv* jni, const SessionDescriptionInterface* desc) {
  std::string sdp;
  RTC_CHECK(desc->ToString(&sdp)) << "got so far: " << sdp;
  jstring j_description = JavaStringFromStdString(jni, sdp);

  jclass j_type_class = FindClass(
      jni, "org/webrtc/SessionDescription$Type");
  jmethodID j_type_from_canonical = GetStaticMethodID(
      jni, j_type_class, "fromCanonicalForm",
      "(Ljava/lang/String;)Lorg/webrtc/SessionDescription$Type;");
  jstring j_type_string = JavaStringFromStdString(jni, desc->type());
  jobject j_type = jni->CallStaticObjectMethod(
      j_type_class, j_type_from_canonical, j_type_string);
  CHECK_EXCEPTION(jni) << "error during CallObjectMethod";

  jclass j_sdp_class = FindClass(jni, "org/webrtc/SessionDescription");
  jmethodID j_sdp_ctor = GetMethodID(
      jni, j_sdp_class, "<init>",
      "(Lorg/webrtc/SessionDescription$Type;Ljava/lang/String;)V");
  jobject j_sdp = jni->NewObject(
      j_sdp_class, j_sdp_ctor, j_type, j_description);
  CHECK_EXCEPTION(jni) << "error during NewObject";
  return j_sdp;
}

template <class T>  // T is one of {Create,Set}SessionDescriptionObserver.
class SdpObserverWrapper : public T {
 public:
  SdpObserverWrapper(JNIEnv* jni, jobject j_observer,
                     ConstraintsWrapper* constraints)
      : constraints_(constraints),
        j_observer_global_(jni, j_observer),
        j_observer_class_(jni, GetObjectClass(jni, j_observer)) {
  }

  virtual ~SdpObserverWrapper() {}

  // Can't mark override because of templating.
  virtual void OnSuccess() {
    ScopedLocalRefFrame local_ref_frame(jni());
    jmethodID m = GetMethodID(jni(), *j_observer_class_, "onSetSuccess", "()V");
    jni()->CallVoidMethod(*j_observer_global_, m);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

  // Can't mark override because of templating.
  virtual void OnSuccess(SessionDescriptionInterface* desc) {
    ScopedLocalRefFrame local_ref_frame(jni());
    jmethodID m = GetMethodID(
        jni(), *j_observer_class_, "onCreateSuccess",
        "(Lorg/webrtc/SessionDescription;)V");
    jobject j_sdp = JavaSdpFromNativeSdp(jni(), desc);
    jni()->CallVoidMethod(*j_observer_global_, m, j_sdp);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
    // OnSuccess transfers ownership of the description (there's a TODO to make
    // it use unique_ptr...).
    delete desc;
  }

 protected:
  // Common implementation for failure of Set & Create types, distinguished by
  // |op| being "Set" or "Create".
  void DoOnFailure(const std::string& op, const std::string& error) {
    jmethodID m = GetMethodID(jni(), *j_observer_class_, "on" + op + "Failure",
                              "(Ljava/lang/String;)V");
    jstring j_error_string = JavaStringFromStdString(jni(), error);
    jni()->CallVoidMethod(*j_observer_global_, m, j_error_string);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

  JNIEnv* jni() {
    return AttachCurrentThreadIfNeeded();
  }

 private:
  std::unique_ptr<ConstraintsWrapper> constraints_;
  const ScopedGlobalRef<jobject> j_observer_global_;
  const ScopedGlobalRef<jclass> j_observer_class_;
};

class CreateSdpObserverWrapper
    : public SdpObserverWrapper<CreateSessionDescriptionObserver> {
 public:
  CreateSdpObserverWrapper(JNIEnv* jni, jobject j_observer,
                           ConstraintsWrapper* constraints)
      : SdpObserverWrapper(jni, j_observer, constraints) {}

  void OnFailure(const std::string& error) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    SdpObserverWrapper::DoOnFailure(std::string("Create"), error);
  }
};

class SetSdpObserverWrapper
    : public SdpObserverWrapper<SetSessionDescriptionObserver> {
 public:
  SetSdpObserverWrapper(JNIEnv* jni, jobject j_observer,
                        ConstraintsWrapper* constraints)
      : SdpObserverWrapper(jni, j_observer, constraints) {}

  void OnFailure(const std::string& error) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    SdpObserverWrapper::DoOnFailure(std::string("Set"), error);
  }
};

// Adapter for a Java DataChannel$Observer presenting a C++ DataChannelObserver
// and dispatching the callback from C++ back to Java.
class DataChannelObserverWrapper : public DataChannelObserver {
 public:
  DataChannelObserverWrapper(JNIEnv* jni, jobject j_observer)
      : j_observer_global_(jni, j_observer),
        j_observer_class_(jni, GetObjectClass(jni, j_observer)),
        j_buffer_class_(jni, FindClass(jni, "org/webrtc/DataChannel$Buffer")),
        j_on_buffered_amount_change_mid_(GetMethodID(
            jni, *j_observer_class_, "onBufferedAmountChange", "(J)V")),
        j_on_state_change_mid_(
            GetMethodID(jni, *j_observer_class_, "onStateChange", "()V")),
        j_on_message_mid_(GetMethodID(jni, *j_observer_class_, "onMessage",
                                      "(Lorg/webrtc/DataChannel$Buffer;)V")),
        j_buffer_ctor_(GetMethodID(jni, *j_buffer_class_, "<init>",
                                   "(Ljava/nio/ByteBuffer;Z)V")) {}

  virtual ~DataChannelObserverWrapper() {}

  void OnBufferedAmountChange(uint64_t previous_amount) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    jni()->CallVoidMethod(*j_observer_global_, j_on_buffered_amount_change_mid_,
                          previous_amount);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

  void OnStateChange() override {
    ScopedLocalRefFrame local_ref_frame(jni());
    jni()->CallVoidMethod(*j_observer_global_, j_on_state_change_mid_);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

  void OnMessage(const DataBuffer& buffer) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    jobject byte_buffer = jni()->NewDirectByteBuffer(
        const_cast<char*>(buffer.data.data<char>()), buffer.data.size());
    jobject j_buffer = jni()->NewObject(*j_buffer_class_, j_buffer_ctor_,
                                        byte_buffer, buffer.binary);
    jni()->CallVoidMethod(*j_observer_global_, j_on_message_mid_, j_buffer);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

 private:
  JNIEnv* jni() {
    return AttachCurrentThreadIfNeeded();
  }

  const ScopedGlobalRef<jobject> j_observer_global_;
  const ScopedGlobalRef<jclass> j_observer_class_;
  const ScopedGlobalRef<jclass> j_buffer_class_;
  const jmethodID j_on_buffered_amount_change_mid_;
  const jmethodID j_on_state_change_mid_;
  const jmethodID j_on_message_mid_;
  const jmethodID j_buffer_ctor_;
};

// Adapter for a Java StatsObserver presenting a C++ StatsObserver and
// dispatching the callback from C++ back to Java.
class StatsObserverWrapper : public StatsObserver {
 public:
  StatsObserverWrapper(JNIEnv* jni, jobject j_observer)
      : j_observer_global_(jni, j_observer),
        j_observer_class_(jni, GetObjectClass(jni, j_observer)),
        j_stats_report_class_(jni, FindClass(jni, "org/webrtc/StatsReport")),
        j_stats_report_ctor_(GetMethodID(
            jni, *j_stats_report_class_, "<init>",
            "(Ljava/lang/String;Ljava/lang/String;D"
            "[Lorg/webrtc/StatsReport$Value;)V")),
        j_value_class_(jni, FindClass(
            jni, "org/webrtc/StatsReport$Value")),
        j_value_ctor_(GetMethodID(
            jni, *j_value_class_, "<init>",
            "(Ljava/lang/String;Ljava/lang/String;)V")) {
  }

  virtual ~StatsObserverWrapper() {}

  void OnComplete(const StatsReports& reports) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    jobjectArray j_reports = ReportsToJava(jni(), reports);
    jmethodID m = GetMethodID(jni(), *j_observer_class_, "onComplete",
                              "([Lorg/webrtc/StatsReport;)V");
    jni()->CallVoidMethod(*j_observer_global_, m, j_reports);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

 private:
  jobjectArray ReportsToJava(
      JNIEnv* jni, const StatsReports& reports) {
    jobjectArray reports_array = jni->NewObjectArray(
        reports.size(), *j_stats_report_class_, NULL);
    int i = 0;
    for (const auto* report : reports) {
      ScopedLocalRefFrame local_ref_frame(jni);
      jstring j_id = JavaStringFromStdString(jni, report->id()->ToString());
      jstring j_type = JavaStringFromStdString(jni, report->TypeToString());
      jobjectArray j_values = ValuesToJava(jni, report->values());
      jobject j_report = jni->NewObject(*j_stats_report_class_,
                                        j_stats_report_ctor_,
                                        j_id,
                                        j_type,
                                        report->timestamp(),
                                        j_values);
      jni->SetObjectArrayElement(reports_array, i++, j_report);
    }
    return reports_array;
  }

  jobjectArray ValuesToJava(JNIEnv* jni, const StatsReport::Values& values) {
    jobjectArray j_values = jni->NewObjectArray(
        values.size(), *j_value_class_, NULL);
    int i = 0;
    for (const auto& it : values) {
      ScopedLocalRefFrame local_ref_frame(jni);
      // Should we use the '.name' enum value here instead of converting the
      // name to a string?
      jstring j_name = JavaStringFromStdString(jni, it.second->display_name());
      jstring j_value = JavaStringFromStdString(jni, it.second->ToString());
      jobject j_element_value =
          jni->NewObject(*j_value_class_, j_value_ctor_, j_name, j_value);
      jni->SetObjectArrayElement(j_values, i++, j_element_value);
    }
    return j_values;
  }

  JNIEnv* jni() {
    return AttachCurrentThreadIfNeeded();
  }

  const ScopedGlobalRef<jobject> j_observer_global_;
  const ScopedGlobalRef<jclass> j_observer_class_;
  const ScopedGlobalRef<jclass> j_stats_report_class_;
  const jmethodID j_stats_report_ctor_;
  const ScopedGlobalRef<jclass> j_value_class_;
  const jmethodID j_value_ctor_;
};

// Adapter between the C++ RtpReceiverObserverInterface and the Java
// RtpReceiver.Observer interface. Wraps an instance of the Java interface and
// dispatches C++ callbacks to Java.
class RtpReceiverObserver : public RtpReceiverObserverInterface {
 public:
  RtpReceiverObserver(JNIEnv* jni, jobject j_observer)
      : j_observer_global_(jni, j_observer) {}

  ~RtpReceiverObserver() override {}

  void OnFirstPacketReceived(cricket::MediaType media_type) override {
    JNIEnv* const jni = AttachCurrentThreadIfNeeded();

    jmethodID j_on_first_packet_received_mid = GetMethodID(
        jni, GetObjectClass(jni, *j_observer_global_), "onFirstPacketReceived",
        "(Lorg/webrtc/MediaStreamTrack$MediaType;)V");
    // Get the Java version of media type.
    jobject JavaMediaType = JsepMediaTypeToJavaMediaType(jni, media_type);
    // Trigger the callback function.
    jni->CallVoidMethod(*j_observer_global_, j_on_first_packet_received_mid,
                        JavaMediaType);
    CHECK_EXCEPTION(jni) << "error during CallVoidMethod";
  }

 private:
  const ScopedGlobalRef<jobject> j_observer_global_;
};

static DataChannelInterface* ExtractNativeDC(JNIEnv* jni, jobject j_dc) {
  jfieldID native_dc_id = GetFieldID(jni,
      GetObjectClass(jni, j_dc), "nativeDataChannel", "J");
  jlong j_d = GetLongField(jni, j_dc, native_dc_id);
  return reinterpret_cast<DataChannelInterface*>(j_d);
}

JOW(jlong, DataChannel_registerObserverNative)(
    JNIEnv* jni, jobject j_dc, jobject j_observer) {
  std::unique_ptr<DataChannelObserverWrapper> observer(
      new DataChannelObserverWrapper(jni, j_observer));
  ExtractNativeDC(jni, j_dc)->RegisterObserver(observer.get());
  return jlongFromPointer(observer.release());
}

JOW(void, DataChannel_unregisterObserverNative)(
    JNIEnv* jni, jobject j_dc, jlong native_observer) {
  ExtractNativeDC(jni, j_dc)->UnregisterObserver();
  delete reinterpret_cast<DataChannelObserverWrapper*>(native_observer);
}

JOW(jstring, DataChannel_label)(JNIEnv* jni, jobject j_dc) {
  return JavaStringFromStdString(jni, ExtractNativeDC(jni, j_dc)->label());
}

JOW(jint, DataChannel_id)(JNIEnv* jni, jobject j_dc) {
  int id = ExtractNativeDC(jni, j_dc)->id();
  RTC_CHECK_LE(id, std::numeric_limits<int32_t>::max())
      << "id overflowed jint!";
  return static_cast<jint>(id);
}

JOW(jobject, DataChannel_state)(JNIEnv* jni, jobject j_dc) {
  return JavaEnumFromIndex(
      jni, "DataChannel$State", ExtractNativeDC(jni, j_dc)->state());
}

JOW(jlong, DataChannel_bufferedAmount)(JNIEnv* jni, jobject j_dc) {
  uint64_t buffered_amount = ExtractNativeDC(jni, j_dc)->buffered_amount();
  RTC_CHECK_LE(buffered_amount, std::numeric_limits<int64_t>::max())
      << "buffered_amount overflowed jlong!";
  return static_cast<jlong>(buffered_amount);
}

JOW(void, DataChannel_close)(JNIEnv* jni, jobject j_dc) {
  ExtractNativeDC(jni, j_dc)->Close();
}

JOW(jboolean, DataChannel_sendNative)(JNIEnv* jni, jobject j_dc,
                                      jbyteArray data, jboolean binary) {
  jbyte* bytes = jni->GetByteArrayElements(data, NULL);
  bool ret = ExtractNativeDC(jni, j_dc)->Send(DataBuffer(
      rtc::CopyOnWriteBuffer(bytes, jni->GetArrayLength(data)),
      binary));
  jni->ReleaseByteArrayElements(data, bytes, JNI_ABORT);
  return ret;
}

JOW(void, DataChannel_dispose)(JNIEnv* jni, jobject j_dc) {
  CHECK_RELEASE(ExtractNativeDC(jni, j_dc));
}

JOW(void, Logging_nativeEnableTracing)(
    JNIEnv* jni, jclass, jstring j_path, jint nativeLevels) {
  std::string path = JavaToStdString(jni, j_path);
  if (nativeLevels != webrtc::kTraceNone) {
    webrtc::Trace::set_level_filter(nativeLevels);
    if (path != "logcat:") {
      RTC_CHECK_EQ(0, webrtc::Trace::SetTraceFile(path.c_str(), false))
          << "SetTraceFile failed";
    } else {
      // Intentionally leak this to avoid needing to reason about its lifecycle.
      // It keeps no state and functions only as a dispatch point.
      static LogcatTraceContext* g_trace_callback = new LogcatTraceContext();
    }
  }
}

JOW(void, Logging_nativeEnableLogToDebugOutput)
    (JNIEnv *jni, jclass, jint nativeSeverity) {
  if (nativeSeverity >= rtc::LS_SENSITIVE && nativeSeverity <= rtc::LS_NONE) {
    rtc::LogMessage::LogToDebug(
        static_cast<rtc::LoggingSeverity>(nativeSeverity));
  }
}

JOW(void, Logging_nativeEnableLogThreads)(JNIEnv* jni, jclass) {
  rtc::LogMessage::LogThreads(true);
}

JOW(void, Logging_nativeEnableLogTimeStamps)(JNIEnv* jni, jclass) {
  rtc::LogMessage::LogTimestamps(true);
}

JOW(void, Logging_nativeLog)(
    JNIEnv* jni, jclass, jint j_severity, jstring j_tag, jstring j_message) {
  std::string message = JavaToStdString(jni, j_message);
  std::string tag = JavaToStdString(jni, j_tag);
  LOG_TAG(static_cast<rtc::LoggingSeverity>(j_severity), tag) << message;
}

JOW(void, PeerConnection_freePeerConnection)(JNIEnv*, jclass, jlong j_p) {
  CHECK_RELEASE(reinterpret_cast<PeerConnectionInterface*>(j_p));
}

JOW(void, PeerConnection_freeObserver)(JNIEnv*, jclass, jlong j_p) {
  PCOJava* p = reinterpret_cast<PCOJava*>(j_p);
  delete p;
}

JOW(void, MediaSource_free)(JNIEnv*, jclass, jlong j_p) {
  reinterpret_cast<rtc::RefCountInterface*>(j_p)->Release();
}

JOW(void, MediaStreamTrack_free)(JNIEnv*, jclass, jlong j_p) {
  reinterpret_cast<MediaStreamTrackInterface*>(j_p)->Release();
}

JOW(jboolean, MediaStream_nativeAddAudioTrack)(
    JNIEnv* jni, jclass, jlong pointer, jlong j_audio_track_pointer) {
  return reinterpret_cast<MediaStreamInterface*>(pointer)->AddTrack(
      reinterpret_cast<AudioTrackInterface*>(j_audio_track_pointer));
}

JOW(jboolean, MediaStream_nativeAddVideoTrack)(
    JNIEnv* jni, jclass, jlong pointer, jlong j_video_track_pointer) {
  return reinterpret_cast<MediaStreamInterface*>(pointer)
      ->AddTrack(reinterpret_cast<VideoTrackInterface*>(j_video_track_pointer));
}

JOW(jboolean, MediaStream_nativeRemoveAudioTrack)(
    JNIEnv* jni, jclass, jlong pointer, jlong j_audio_track_pointer) {
  return reinterpret_cast<MediaStreamInterface*>(pointer)->RemoveTrack(
      reinterpret_cast<AudioTrackInterface*>(j_audio_track_pointer));
}

JOW(jboolean, MediaStream_nativeRemoveVideoTrack)(
    JNIEnv* jni, jclass, jlong pointer, jlong j_video_track_pointer) {
  return reinterpret_cast<MediaStreamInterface*>(pointer)->RemoveTrack(
      reinterpret_cast<VideoTrackInterface*>(j_video_track_pointer));
}

JOW(jstring, MediaStream_nativeLabel)(JNIEnv* jni, jclass, jlong j_p) {
  return JavaStringFromStdString(
      jni, reinterpret_cast<MediaStreamInterface*>(j_p)->label());
}

JOW(void, MediaStream_free)(JNIEnv*, jclass, jlong j_p) {
  CHECK_RELEASE(reinterpret_cast<MediaStreamInterface*>(j_p));
}

JOW(jlong, PeerConnectionFactory_nativeCreateObserver)(
    JNIEnv * jni, jclass, jobject j_observer) {
  return (jlong)new PCOJava(jni, j_observer);
}

JOW(void, PeerConnectionFactory_nativeInitializeAndroidGlobals)
(JNIEnv* jni,
 jclass,
 jobject context,
 jboolean video_hw_acceleration) {
  video_hw_acceleration_enabled = video_hw_acceleration;
  if (!factory_static_initialized) {
    webrtc::JVM::Initialize(GetJVM());
    factory_static_initialized = true;
  }
}

JOW(void, PeerConnectionFactory_initializeFieldTrials)(
    JNIEnv* jni, jclass, jstring j_trials_init_string) {
  field_trials_init_string = NULL;
  if (j_trials_init_string != NULL) {
    const char* init_string =
        jni->GetStringUTFChars(j_trials_init_string, NULL);
    int init_string_length = jni->GetStringUTFLength(j_trials_init_string);
    field_trials_init_string = new char[init_string_length + 1];
    rtc::strcpyn(field_trials_init_string, init_string_length + 1, init_string);
    jni->ReleaseStringUTFChars(j_trials_init_string, init_string);
    LOG(LS_INFO) << "initializeFieldTrials: " << field_trials_init_string;
  }
  webrtc::field_trial::InitFieldTrialsFromString(field_trials_init_string);
}

JOW(void, PeerConnectionFactory_initializeInternalTracer)(JNIEnv* jni, jclass) {
  rtc::tracing::SetupInternalTracer();
}

JOW(jstring, PeerConnectionFactory_nativeFieldTrialsFindFullName)
(JNIEnv* jni, jclass, jstring j_name) {
  return JavaStringFromStdString(
      jni, webrtc::field_trial::FindFullName(JavaToStdString(jni, j_name)));
}

JOW(jboolean, PeerConnectionFactory_startInternalTracingCapture)(
    JNIEnv* jni, jclass, jstring j_event_tracing_filename) {
  if (!j_event_tracing_filename)
    return false;

  const char* init_string =
      jni->GetStringUTFChars(j_event_tracing_filename, NULL);
  LOG(LS_INFO) << "Starting internal tracing to: " << init_string;
  bool ret = rtc::tracing::StartInternalCapture(init_string);
  jni->ReleaseStringUTFChars(j_event_tracing_filename, init_string);
  return ret;
}

JOW(void, PeerConnectionFactory_stopInternalTracingCapture)(
    JNIEnv* jni, jclass) {
  rtc::tracing::StopInternalCapture();
}

JOW(void, PeerConnectionFactory_shutdownInternalTracer)(JNIEnv* jni, jclass) {
  rtc::tracing::ShutdownInternalTracer();
}

JOW(void, AudioTrack_nativeSetVolume)
(JNIEnv*, jclass, jlong j_p, jdouble volume) {
  rtc::scoped_refptr<AudioSourceInterface> source(
      reinterpret_cast<AudioTrackInterface*>(j_p)->GetSource());
  source->SetVolume(volume);
}

PeerConnectionFactoryInterface::Options ParseOptionsFromJava(JNIEnv* jni,
                                                             jobject options) {
  jclass options_class = jni->GetObjectClass(options);
  jfieldID network_ignore_mask_field =
      jni->GetFieldID(options_class, "networkIgnoreMask", "I");
  int network_ignore_mask =
      jni->GetIntField(options, network_ignore_mask_field);

  jfieldID disable_encryption_field =
      jni->GetFieldID(options_class, "disableEncryption", "Z");
  bool disable_encryption =
      jni->GetBooleanField(options, disable_encryption_field);

  jfieldID disable_network_monitor_field =
      jni->GetFieldID(options_class, "disableNetworkMonitor", "Z");
  bool disable_network_monitor =
      jni->GetBooleanField(options, disable_network_monitor_field);

  PeerConnectionFactoryInterface::Options native_options;

  // This doesn't necessarily match the c++ version of this struct; feel free
  // to add more parameters as necessary.
  native_options.network_ignore_mask = network_ignore_mask;
  native_options.disable_encryption = disable_encryption;
  native_options.disable_network_monitor = disable_network_monitor;
  return native_options;
}

JOW(jlong, PeerConnectionFactory_nativeCreatePeerConnectionFactory)(
    JNIEnv* jni, jclass, jobject joptions) {
  // talk/ assumes pretty widely that the current Thread is ThreadManager'd, but
  // ThreadManager only WrapCurrentThread()s the thread where it is first
  // created.  Since the semantics around when auto-wrapping happens in
  // webrtc/base/ are convoluted, we simply wrap here to avoid having to think
  // about ramifications of auto-wrapping there.
  rtc::ThreadManager::Instance()->WrapCurrentThread();
  webrtc::Trace::CreateTrace();

  std::unique_ptr<Thread> network_thread =
      rtc::Thread::CreateWithSocketServer();
  network_thread->SetName("network_thread", nullptr);
  RTC_CHECK(network_thread->Start()) << "Failed to start thread";

  std::unique_ptr<Thread> worker_thread = rtc::Thread::Create();
  worker_thread->SetName("worker_thread", nullptr);
  RTC_CHECK(worker_thread->Start()) << "Failed to start thread";

  std::unique_ptr<Thread> signaling_thread = rtc::Thread::Create();
  signaling_thread->SetName("signaling_thread", NULL);
  RTC_CHECK(signaling_thread->Start()) << "Failed to start thread";

  WebRtcVideoEncoderFactory* video_encoder_factory = nullptr;
  WebRtcVideoDecoderFactory* video_decoder_factory = nullptr;
  rtc::NetworkMonitorFactory* network_monitor_factory = nullptr;
  auto audio_encoder_factory = CreateAudioEncoderFactory();
  auto audio_decoder_factory = CreateAudioDecoderFactory();

  PeerConnectionFactoryInterface::Options options;
  bool has_options = joptions != NULL;
  if (has_options) {
    options = ParseOptionsFromJava(jni, joptions);
  }

  if (video_hw_acceleration_enabled) {
    video_encoder_factory = CreateVideoEncoderFactory();
    video_decoder_factory = CreateVideoDecoderFactory();
  }
  // Do not create network_monitor_factory only if the options are
  // provided and disable_network_monitor therein is set to true.
  if (!(has_options && options.disable_network_monitor)) {
    network_monitor_factory = new AndroidNetworkMonitorFactory();
    rtc::NetworkMonitorFactory::SetFactory(network_monitor_factory);
  }

  webrtc::AudioDeviceModule* adm = nullptr;
  rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer = nullptr;
  std::unique_ptr<webrtc::CallFactoryInterface> call_factory(
      CreateCallFactory());
  std::unique_ptr<webrtc::RtcEventLogFactoryInterface> rtc_event_log_factory(
      CreateRtcEventLogFactory());
  std::unique_ptr<cricket::MediaEngineInterface> media_engine(CreateMediaEngine(
      adm, audio_encoder_factory, audio_decoder_factory, video_encoder_factory,
      video_decoder_factory, audio_mixer));

  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory(
      CreateModularPeerConnectionFactory(
          network_thread.get(), worker_thread.get(), signaling_thread.get(),
          adm, audio_encoder_factory, audio_decoder_factory,
          video_encoder_factory, video_decoder_factory, audio_mixer,
          std::move(media_engine), std::move(call_factory),
          std::move(rtc_event_log_factory)));
  RTC_CHECK(factory) << "Failed to create the peer connection factory; "
                     << "WebRTC/libjingle init likely failed on this device";
  // TODO(honghaiz): Maybe put the options as the argument of
  // CreatePeerConnectionFactory.
  if (has_options) {
    factory->SetOptions(options);
  }
  OwnedFactoryAndThreads* owned_factory = new OwnedFactoryAndThreads(
      std::move(network_thread), std::move(worker_thread),
      std::move(signaling_thread), video_encoder_factory, video_decoder_factory,
      network_monitor_factory, factory.release());
  owned_factory->InvokeJavaCallbacksOnFactoryThreads();
  return jlongFromPointer(owned_factory);
}

JOW(void, PeerConnectionFactory_nativeFreeFactory)(JNIEnv*, jclass, jlong j_p) {
  delete reinterpret_cast<OwnedFactoryAndThreads*>(j_p);
  if (field_trials_init_string) {
    webrtc::field_trial::InitFieldTrialsFromString(NULL);
    delete field_trials_init_string;
    field_trials_init_string = NULL;
  }
  webrtc::Trace::ReturnTrace();
}

JOW(void, PeerConnectionFactory_nativeThreadsCallbacks)(
    JNIEnv*, jclass, jlong j_p) {
  OwnedFactoryAndThreads *factory =
      reinterpret_cast<OwnedFactoryAndThreads*>(j_p);
  factory->InvokeJavaCallbacksOnFactoryThreads();
}

JOW(jlong, PeerConnectionFactory_nativeCreateLocalMediaStream)(
    JNIEnv* jni, jclass, jlong native_factory, jstring label) {
  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory(
      factoryFromJava(native_factory));
  rtc::scoped_refptr<MediaStreamInterface> stream(
      factory->CreateLocalMediaStream(JavaToStdString(jni, label)));
  return (jlong)stream.release();
}

JOW(jlong, PeerConnectionFactory_nativeCreateAudioSource)(
    JNIEnv* jni, jclass, jlong native_factory, jobject j_constraints) {
  std::unique_ptr<ConstraintsWrapper> constraints(
      new ConstraintsWrapper(jni, j_constraints));
  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory(
      factoryFromJava(native_factory));
  cricket::AudioOptions options;
  CopyConstraintsIntoAudioOptions(constraints.get(), &options);
  rtc::scoped_refptr<AudioSourceInterface> source(
      factory->CreateAudioSource(options));
  return (jlong)source.release();
}

JOW(jlong, PeerConnectionFactory_nativeCreateAudioTrack)(
    JNIEnv* jni, jclass, jlong native_factory, jstring id,
    jlong native_source) {
  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory(
      factoryFromJava(native_factory));
  rtc::scoped_refptr<AudioTrackInterface> track(factory->CreateAudioTrack(
      JavaToStdString(jni, id),
      reinterpret_cast<AudioSourceInterface*>(native_source)));
  return (jlong)track.release();
}

JOW(jboolean, PeerConnectionFactory_nativeStartAecDump)(
    JNIEnv* jni, jclass, jlong native_factory, jint file,
    jint filesize_limit_bytes) {
  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory(
      factoryFromJava(native_factory));
  return factory->StartAecDump(file, filesize_limit_bytes);
}

JOW(void, PeerConnectionFactory_nativeStopAecDump)(
    JNIEnv* jni, jclass, jlong native_factory) {
  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory(
      factoryFromJava(native_factory));
  factory->StopAecDump();
}

JOW(void, PeerConnectionFactory_nativeSetOptions)(
    JNIEnv* jni, jclass, jlong native_factory, jobject options) {
  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory(
      factoryFromJava(native_factory));
  PeerConnectionFactoryInterface::Options options_to_set =
      ParseOptionsFromJava(jni, options);
  factory->SetOptions(options_to_set);

  if (options_to_set.disable_network_monitor) {
    OwnedFactoryAndThreads* owner =
        reinterpret_cast<OwnedFactoryAndThreads*>(native_factory);
    if (owner->network_monitor_factory()) {
      rtc::NetworkMonitorFactory::ReleaseFactory(
          owner->network_monitor_factory());
      owner->clear_network_monitor_factory();
    }
  }
}

static PeerConnectionInterface::IceTransportsType
JavaIceTransportsTypeToNativeType(JNIEnv* jni, jobject j_ice_transports_type) {
  std::string enum_name = GetJavaEnumName(
      jni, "org/webrtc/PeerConnection$IceTransportsType",
      j_ice_transports_type);

  if (enum_name == "ALL")
    return PeerConnectionInterface::kAll;

  if (enum_name == "RELAY")
    return PeerConnectionInterface::kRelay;

  if (enum_name == "NOHOST")
    return PeerConnectionInterface::kNoHost;

  if (enum_name == "NONE")
    return PeerConnectionInterface::kNone;

  RTC_CHECK(false) << "Unexpected IceTransportsType enum_name " << enum_name;
  return PeerConnectionInterface::kAll;
}

static PeerConnectionInterface::BundlePolicy
JavaBundlePolicyToNativeType(JNIEnv* jni, jobject j_bundle_policy) {
  std::string enum_name = GetJavaEnumName(
      jni, "org/webrtc/PeerConnection$BundlePolicy",
      j_bundle_policy);

  if (enum_name == "BALANCED")
    return PeerConnectionInterface::kBundlePolicyBalanced;

  if (enum_name == "MAXBUNDLE")
    return PeerConnectionInterface::kBundlePolicyMaxBundle;

  if (enum_name == "MAXCOMPAT")
    return PeerConnectionInterface::kBundlePolicyMaxCompat;

  RTC_CHECK(false) << "Unexpected BundlePolicy enum_name " << enum_name;
  return PeerConnectionInterface::kBundlePolicyBalanced;
}

static PeerConnectionInterface::RtcpMuxPolicy
JavaRtcpMuxPolicyToNativeType(JNIEnv* jni, jobject j_rtcp_mux_policy) {
  std::string enum_name = GetJavaEnumName(
      jni, "org/webrtc/PeerConnection$RtcpMuxPolicy",
      j_rtcp_mux_policy);

  if (enum_name == "NEGOTIATE")
    return PeerConnectionInterface::kRtcpMuxPolicyNegotiate;

  if (enum_name == "REQUIRE")
    return PeerConnectionInterface::kRtcpMuxPolicyRequire;

  RTC_CHECK(false) << "Unexpected RtcpMuxPolicy enum_name " << enum_name;
  return PeerConnectionInterface::kRtcpMuxPolicyNegotiate;
}

static PeerConnectionInterface::TcpCandidatePolicy
JavaTcpCandidatePolicyToNativeType(
    JNIEnv* jni, jobject j_tcp_candidate_policy) {
  std::string enum_name = GetJavaEnumName(
      jni, "org/webrtc/PeerConnection$TcpCandidatePolicy",
      j_tcp_candidate_policy);

  if (enum_name == "ENABLED")
    return PeerConnectionInterface::kTcpCandidatePolicyEnabled;

  if (enum_name == "DISABLED")
    return PeerConnectionInterface::kTcpCandidatePolicyDisabled;

  RTC_CHECK(false) << "Unexpected TcpCandidatePolicy enum_name " << enum_name;
  return PeerConnectionInterface::kTcpCandidatePolicyEnabled;
}

static PeerConnectionInterface::CandidateNetworkPolicy
JavaCandidateNetworkPolicyToNativeType(JNIEnv* jni,
                                       jobject j_candidate_network_policy) {
  std::string enum_name =
      GetJavaEnumName(jni, "org/webrtc/PeerConnection$CandidateNetworkPolicy",
                      j_candidate_network_policy);

  if (enum_name == "ALL")
    return PeerConnectionInterface::kCandidateNetworkPolicyAll;

  if (enum_name == "LOW_COST")
    return PeerConnectionInterface::kCandidateNetworkPolicyLowCost;

  RTC_CHECK(false) << "Unexpected CandidateNetworkPolicy enum_name "
                   << enum_name;
  return PeerConnectionInterface::kCandidateNetworkPolicyAll;
}

static rtc::KeyType JavaKeyTypeToNativeType(JNIEnv* jni, jobject j_key_type) {
  std::string enum_name = GetJavaEnumName(
      jni, "org/webrtc/PeerConnection$KeyType", j_key_type);

  if (enum_name == "RSA")
    return rtc::KT_RSA;
  if (enum_name == "ECDSA")
    return rtc::KT_ECDSA;

  RTC_CHECK(false) << "Unexpected KeyType enum_name " << enum_name;
  return rtc::KT_ECDSA;
}

static PeerConnectionInterface::ContinualGatheringPolicy
    JavaContinualGatheringPolicyToNativeType(
        JNIEnv* jni, jobject j_gathering_policy) {
  std::string enum_name = GetJavaEnumName(
      jni, "org/webrtc/PeerConnection$ContinualGatheringPolicy",
      j_gathering_policy);
  if (enum_name == "GATHER_ONCE")
    return PeerConnectionInterface::GATHER_ONCE;

  if (enum_name == "GATHER_CONTINUALLY")
    return PeerConnectionInterface::GATHER_CONTINUALLY;

  RTC_CHECK(false) << "Unexpected ContinualGatheringPolicy enum name "
                   << enum_name;
  return PeerConnectionInterface::GATHER_ONCE;
}

static PeerConnectionInterface::TlsCertPolicy JavaTlsCertPolicyTypeToNativeType(
    JNIEnv* jni,
    jobject j_ice_server_tls_cert_policy) {
  std::string enum_name =
      GetJavaEnumName(jni, "org/webrtc/PeerConnection$TlsCertPolicy",
                      j_ice_server_tls_cert_policy);

  if (enum_name == "TLS_CERT_POLICY_SECURE")
    return PeerConnectionInterface::kTlsCertPolicySecure;

  if (enum_name == "TLS_CERT_POLICY_INSECURE_NO_CHECK")
    return PeerConnectionInterface::kTlsCertPolicyInsecureNoCheck;

  RTC_CHECK(false) << "Unexpected TlsCertPolicy enum_name " << enum_name;
  return PeerConnectionInterface::kTlsCertPolicySecure;
}

static void JavaIceServersToJsepIceServers(
    JNIEnv* jni, jobject j_ice_servers,
    PeerConnectionInterface::IceServers* ice_servers) {
  for (jobject j_ice_server : Iterable(jni, j_ice_servers)) {
    jclass j_ice_server_class = GetObjectClass(jni, j_ice_server);
    jfieldID j_ice_server_uri_id =
        GetFieldID(jni, j_ice_server_class, "uri", "Ljava/lang/String;");
    jfieldID j_ice_server_username_id =
        GetFieldID(jni, j_ice_server_class, "username", "Ljava/lang/String;");
    jfieldID j_ice_server_password_id =
        GetFieldID(jni, j_ice_server_class, "password", "Ljava/lang/String;");
    jfieldID j_ice_server_tls_cert_policy_id =
        GetFieldID(jni, j_ice_server_class, "tlsCertPolicy",
                   "Lorg/webrtc/PeerConnection$TlsCertPolicy;");
    jobject j_ice_server_tls_cert_policy =
        GetObjectField(jni, j_ice_server, j_ice_server_tls_cert_policy_id);
    jfieldID j_ice_server_hostname_id =
        GetFieldID(jni, j_ice_server_class, "hostname", "Ljava/lang/String;");
    jstring uri = reinterpret_cast<jstring>(
        GetObjectField(jni, j_ice_server, j_ice_server_uri_id));
    jstring username = reinterpret_cast<jstring>(
        GetObjectField(jni, j_ice_server, j_ice_server_username_id));
    jstring password = reinterpret_cast<jstring>(
        GetObjectField(jni, j_ice_server, j_ice_server_password_id));
    PeerConnectionInterface::TlsCertPolicy tls_cert_policy =
        JavaTlsCertPolicyTypeToNativeType(jni, j_ice_server_tls_cert_policy);
    jstring hostname = reinterpret_cast<jstring>(
        GetObjectField(jni, j_ice_server, j_ice_server_hostname_id));
    PeerConnectionInterface::IceServer server;
    server.uri = JavaToStdString(jni, uri);
    server.username = JavaToStdString(jni, username);
    server.password = JavaToStdString(jni, password);
    server.tls_cert_policy = tls_cert_policy;
    server.hostname = JavaToStdString(jni, hostname);
    ice_servers->push_back(server);
  }
}

static void JavaRTCConfigurationToJsepRTCConfiguration(
    JNIEnv* jni,
    jobject j_rtc_config,
    PeerConnectionInterface::RTCConfiguration* rtc_config) {
  jclass j_rtc_config_class = GetObjectClass(jni, j_rtc_config);

  jfieldID j_ice_transports_type_id = GetFieldID(
      jni, j_rtc_config_class, "iceTransportsType",
      "Lorg/webrtc/PeerConnection$IceTransportsType;");
  jobject j_ice_transports_type = GetObjectField(
      jni, j_rtc_config, j_ice_transports_type_id);

  jfieldID j_bundle_policy_id = GetFieldID(
      jni, j_rtc_config_class, "bundlePolicy",
      "Lorg/webrtc/PeerConnection$BundlePolicy;");
  jobject j_bundle_policy = GetObjectField(
      jni, j_rtc_config, j_bundle_policy_id);

  jfieldID j_rtcp_mux_policy_id = GetFieldID(
      jni, j_rtc_config_class, "rtcpMuxPolicy",
      "Lorg/webrtc/PeerConnection$RtcpMuxPolicy;");
  jobject j_rtcp_mux_policy = GetObjectField(
      jni, j_rtc_config, j_rtcp_mux_policy_id);

  jfieldID j_tcp_candidate_policy_id = GetFieldID(
      jni, j_rtc_config_class, "tcpCandidatePolicy",
      "Lorg/webrtc/PeerConnection$TcpCandidatePolicy;");
  jobject j_tcp_candidate_policy = GetObjectField(
      jni, j_rtc_config, j_tcp_candidate_policy_id);

  jfieldID j_candidate_network_policy_id = GetFieldID(
      jni, j_rtc_config_class, "candidateNetworkPolicy",
      "Lorg/webrtc/PeerConnection$CandidateNetworkPolicy;");
  jobject j_candidate_network_policy = GetObjectField(
      jni, j_rtc_config, j_candidate_network_policy_id);

  jfieldID j_ice_servers_id = GetFieldID(
      jni, j_rtc_config_class, "iceServers", "Ljava/util/List;");
  jobject j_ice_servers = GetObjectField(jni, j_rtc_config, j_ice_servers_id);

  jfieldID j_audio_jitter_buffer_max_packets_id =
      GetFieldID(jni, j_rtc_config_class, "audioJitterBufferMaxPackets", "I");
  jfieldID j_audio_jitter_buffer_fast_accelerate_id = GetFieldID(
      jni, j_rtc_config_class, "audioJitterBufferFastAccelerate", "Z");

  jfieldID j_ice_connection_receiving_timeout_id =
      GetFieldID(jni, j_rtc_config_class, "iceConnectionReceivingTimeout", "I");

  jfieldID j_ice_backup_candidate_pair_ping_interval_id = GetFieldID(
      jni, j_rtc_config_class, "iceBackupCandidatePairPingInterval", "I");

  jfieldID j_continual_gathering_policy_id =
      GetFieldID(jni, j_rtc_config_class, "continualGatheringPolicy",
                 "Lorg/webrtc/PeerConnection$ContinualGatheringPolicy;");
  jobject j_continual_gathering_policy =
      GetObjectField(jni, j_rtc_config, j_continual_gathering_policy_id);

  jfieldID j_ice_candidate_pool_size_id =
      GetFieldID(jni, j_rtc_config_class, "iceCandidatePoolSize", "I");
  jfieldID j_presume_writable_when_fully_relayed_id = GetFieldID(
      jni, j_rtc_config_class, "presumeWritableWhenFullyRelayed", "Z");

  jfieldID j_prune_turn_ports_id =
      GetFieldID(jni, j_rtc_config_class, "pruneTurnPorts", "Z");

  jfieldID j_ice_check_min_interval_id = GetFieldID(
      jni, j_rtc_config_class, "iceCheckMinInterval", "Ljava/lang/Integer;");
  jclass j_integer_class = jni->FindClass("java/lang/Integer");
  jmethodID int_value_id = GetMethodID(jni, j_integer_class, "intValue", "()I");

  jfieldID j_disable_ipv6_on_wifi_id =
      GetFieldID(jni, j_rtc_config_class, "disableIPv6OnWifi", "Z");

  rtc_config->type =
      JavaIceTransportsTypeToNativeType(jni, j_ice_transports_type);
  rtc_config->bundle_policy =
      JavaBundlePolicyToNativeType(jni, j_bundle_policy);
  rtc_config->rtcp_mux_policy =
      JavaRtcpMuxPolicyToNativeType(jni, j_rtcp_mux_policy);
  rtc_config->tcp_candidate_policy =
      JavaTcpCandidatePolicyToNativeType(jni, j_tcp_candidate_policy);
  rtc_config->candidate_network_policy =
      JavaCandidateNetworkPolicyToNativeType(jni, j_candidate_network_policy);
  JavaIceServersToJsepIceServers(jni, j_ice_servers, &rtc_config->servers);
  rtc_config->audio_jitter_buffer_max_packets =
      GetIntField(jni, j_rtc_config, j_audio_jitter_buffer_max_packets_id);
  rtc_config->audio_jitter_buffer_fast_accelerate = GetBooleanField(
      jni, j_rtc_config, j_audio_jitter_buffer_fast_accelerate_id);
  rtc_config->ice_connection_receiving_timeout =
      GetIntField(jni, j_rtc_config, j_ice_connection_receiving_timeout_id);
  rtc_config->ice_backup_candidate_pair_ping_interval = GetIntField(
      jni, j_rtc_config, j_ice_backup_candidate_pair_ping_interval_id);
  rtc_config->continual_gathering_policy =
      JavaContinualGatheringPolicyToNativeType(
          jni, j_continual_gathering_policy);
  rtc_config->ice_candidate_pool_size =
      GetIntField(jni, j_rtc_config, j_ice_candidate_pool_size_id);
  rtc_config->prune_turn_ports =
      GetBooleanField(jni, j_rtc_config, j_prune_turn_ports_id);
  rtc_config->presume_writable_when_fully_relayed = GetBooleanField(
      jni, j_rtc_config, j_presume_writable_when_fully_relayed_id);
  jobject j_ice_check_min_interval =
      GetNullableObjectField(jni, j_rtc_config, j_ice_check_min_interval_id);
  if (!IsNull(jni, j_ice_check_min_interval)) {
    int ice_check_min_interval_value =
        jni->CallIntMethod(j_ice_check_min_interval, int_value_id);
    rtc_config->ice_check_min_interval =
        rtc::Optional<int>(ice_check_min_interval_value);
  }
  rtc_config->disable_ipv6_on_wifi =
      GetBooleanField(jni, j_rtc_config, j_disable_ipv6_on_wifi_id);
}

JOW(jlong, PeerConnectionFactory_nativeCreatePeerConnection)(
    JNIEnv *jni, jclass, jlong factory, jobject j_rtc_config,
    jobject j_constraints, jlong observer_p) {
  rtc::scoped_refptr<PeerConnectionFactoryInterface> f(
      reinterpret_cast<PeerConnectionFactoryInterface*>(
          factoryFromJava(factory)));

  PeerConnectionInterface::RTCConfiguration rtc_config(
      PeerConnectionInterface::RTCConfigurationType::kAggressive);
  JavaRTCConfigurationToJsepRTCConfiguration(jni, j_rtc_config, &rtc_config);

  jclass j_rtc_config_class = GetObjectClass(jni, j_rtc_config);
  jfieldID j_key_type_id = GetFieldID(jni, j_rtc_config_class, "keyType",
                                      "Lorg/webrtc/PeerConnection$KeyType;");
  jobject j_key_type = GetObjectField(jni, j_rtc_config, j_key_type_id);

  // Generate non-default certificate.
  rtc::KeyType key_type = JavaKeyTypeToNativeType(jni, j_key_type);
  if (key_type != rtc::KT_DEFAULT) {
    rtc::scoped_refptr<rtc::RTCCertificate> certificate =
        rtc::RTCCertificateGenerator::GenerateCertificate(
            rtc::KeyParams(key_type), rtc::Optional<uint64_t>());
    if (!certificate) {
      LOG(LS_ERROR) << "Failed to generate certificate. KeyType: " << key_type;
      return 0;
    }
    rtc_config.certificates.push_back(certificate);
  }

  PCOJava* observer = reinterpret_cast<PCOJava*>(observer_p);
  observer->SetConstraints(new ConstraintsWrapper(jni, j_constraints));
  CopyConstraintsIntoRtcConfiguration(observer->constraints(), &rtc_config);
  rtc::scoped_refptr<PeerConnectionInterface> pc(
      f->CreatePeerConnection(rtc_config, nullptr, nullptr, observer));
  return (jlong)pc.release();
}

static rtc::scoped_refptr<PeerConnectionInterface> ExtractNativePC(
    JNIEnv* jni, jobject j_pc) {
  jfieldID native_pc_id = GetFieldID(jni,
      GetObjectClass(jni, j_pc), "nativePeerConnection", "J");
  jlong j_p = GetLongField(jni, j_pc, native_pc_id);
  return rtc::scoped_refptr<PeerConnectionInterface>(
      reinterpret_cast<PeerConnectionInterface*>(j_p));
}

JOW(jobject, PeerConnection_getLocalDescription)(JNIEnv* jni, jobject j_pc) {
  const SessionDescriptionInterface* sdp =
      ExtractNativePC(jni, j_pc)->local_description();
  return sdp ? JavaSdpFromNativeSdp(jni, sdp) : NULL;
}

JOW(jobject, PeerConnection_getRemoteDescription)(JNIEnv* jni, jobject j_pc) {
  const SessionDescriptionInterface* sdp =
      ExtractNativePC(jni, j_pc)->remote_description();
  return sdp ? JavaSdpFromNativeSdp(jni, sdp) : NULL;
}

JOW(jobject, PeerConnection_createDataChannel)(
    JNIEnv* jni, jobject j_pc, jstring j_label, jobject j_init) {
  DataChannelInit init = JavaDataChannelInitToNative(jni, j_init);
  rtc::scoped_refptr<DataChannelInterface> channel(
      ExtractNativePC(jni, j_pc)->CreateDataChannel(
          JavaToStdString(jni, j_label), &init));
  // Mustn't pass channel.get() directly through NewObject to avoid reading its
  // vararg parameter as 64-bit and reading memory that doesn't belong to the
  // 32-bit parameter.
  jlong nativeChannelPtr = jlongFromPointer(channel.get());
  if (!nativeChannelPtr) {
    LOG(LS_ERROR) << "Failed to create DataChannel";
    return nullptr;
  }
  jclass j_data_channel_class = FindClass(jni, "org/webrtc/DataChannel");
  jmethodID j_data_channel_ctor = GetMethodID(
      jni, j_data_channel_class, "<init>", "(J)V");
  jobject j_channel = jni->NewObject(
      j_data_channel_class, j_data_channel_ctor, nativeChannelPtr);
  CHECK_EXCEPTION(jni) << "error during NewObject";
  // Channel is now owned by Java object, and will be freed from there.
  int bumped_count = channel->AddRef();
  RTC_CHECK(bumped_count == 2) << "Unexpected refcount";
  return j_channel;
}

JOW(void, PeerConnection_createOffer)(
    JNIEnv* jni, jobject j_pc, jobject j_observer, jobject j_constraints) {
  ConstraintsWrapper* constraints =
      new ConstraintsWrapper(jni, j_constraints);
  rtc::scoped_refptr<CreateSdpObserverWrapper> observer(
      new rtc::RefCountedObject<CreateSdpObserverWrapper>(
          jni, j_observer, constraints));
  ExtractNativePC(jni, j_pc)->CreateOffer(observer, constraints);
}

JOW(void, PeerConnection_createAnswer)(
    JNIEnv* jni, jobject j_pc, jobject j_observer, jobject j_constraints) {
  ConstraintsWrapper* constraints =
      new ConstraintsWrapper(jni, j_constraints);
  rtc::scoped_refptr<CreateSdpObserverWrapper> observer(
      new rtc::RefCountedObject<CreateSdpObserverWrapper>(
          jni, j_observer, constraints));
  ExtractNativePC(jni, j_pc)->CreateAnswer(observer, constraints);
}

// Helper to create a SessionDescriptionInterface from a SessionDescription.
static SessionDescriptionInterface* JavaSdpToNativeSdp(
    JNIEnv* jni, jobject j_sdp) {
  jfieldID j_type_id = GetFieldID(
      jni, GetObjectClass(jni, j_sdp), "type",
      "Lorg/webrtc/SessionDescription$Type;");
  jobject j_type = GetObjectField(jni, j_sdp, j_type_id);
  jmethodID j_canonical_form_id = GetMethodID(
      jni, GetObjectClass(jni, j_type), "canonicalForm",
      "()Ljava/lang/String;");
  jstring j_type_string = (jstring)jni->CallObjectMethod(
      j_type, j_canonical_form_id);
  CHECK_EXCEPTION(jni) << "error during CallObjectMethod";
  std::string std_type = JavaToStdString(jni, j_type_string);

  jfieldID j_description_id = GetFieldID(
      jni, GetObjectClass(jni, j_sdp), "description", "Ljava/lang/String;");
  jstring j_description = (jstring)GetObjectField(jni, j_sdp, j_description_id);
  std::string std_description = JavaToStdString(jni, j_description);

  return webrtc::CreateSessionDescription(
      std_type, std_description, NULL);
}

JOW(void, PeerConnection_setLocalDescription)(
    JNIEnv* jni, jobject j_pc,
    jobject j_observer, jobject j_sdp) {
  rtc::scoped_refptr<SetSdpObserverWrapper> observer(
      new rtc::RefCountedObject<SetSdpObserverWrapper>(
          jni, j_observer, reinterpret_cast<ConstraintsWrapper*>(NULL)));
  ExtractNativePC(jni, j_pc)->SetLocalDescription(
      observer, JavaSdpToNativeSdp(jni, j_sdp));
}

JOW(void, PeerConnection_setRemoteDescription)(
    JNIEnv* jni, jobject j_pc,
    jobject j_observer, jobject j_sdp) {
  rtc::scoped_refptr<SetSdpObserverWrapper> observer(
      new rtc::RefCountedObject<SetSdpObserverWrapper>(
          jni, j_observer, reinterpret_cast<ConstraintsWrapper*>(NULL)));
  ExtractNativePC(jni, j_pc)->SetRemoteDescription(
      observer, JavaSdpToNativeSdp(jni, j_sdp));
}

JOW(jboolean, PeerConnection_nativeSetConfiguration)(
    JNIEnv* jni, jobject j_pc, jobject j_rtc_config, jlong native_observer) {
  // Need to merge constraints into RTCConfiguration again, which are stored
  // in the observer object.
  PCOJava* observer = reinterpret_cast<PCOJava*>(native_observer);
  PeerConnectionInterface::RTCConfiguration rtc_config(
      PeerConnectionInterface::RTCConfigurationType::kAggressive);
  JavaRTCConfigurationToJsepRTCConfiguration(jni, j_rtc_config, &rtc_config);
  CopyConstraintsIntoRtcConfiguration(observer->constraints(), &rtc_config);
  return ExtractNativePC(jni, j_pc)->SetConfiguration(rtc_config);
}

JOW(jboolean, PeerConnection_nativeAddIceCandidate)(
    JNIEnv* jni, jobject j_pc, jstring j_sdp_mid,
    jint j_sdp_mline_index, jstring j_candidate_sdp) {
  std::string sdp_mid = JavaToStdString(jni, j_sdp_mid);
  std::string sdp = JavaToStdString(jni, j_candidate_sdp);
  std::unique_ptr<IceCandidateInterface> candidate(
      webrtc::CreateIceCandidate(sdp_mid, j_sdp_mline_index, sdp, NULL));
  return ExtractNativePC(jni, j_pc)->AddIceCandidate(candidate.get());
}

static cricket::Candidate GetCandidateFromJava(JNIEnv* jni,
                                               jobject j_candidate) {
  jclass j_candidate_class = GetObjectClass(jni, j_candidate);
  jfieldID j_sdp_mid_id =
      GetFieldID(jni, j_candidate_class, "sdpMid", "Ljava/lang/String;");
  std::string sdp_mid =
      JavaToStdString(jni, GetStringField(jni, j_candidate, j_sdp_mid_id));
  jfieldID j_sdp_id =
      GetFieldID(jni, j_candidate_class, "sdp", "Ljava/lang/String;");
  std::string sdp =
      JavaToStdString(jni, GetStringField(jni, j_candidate, j_sdp_id));
  cricket::Candidate candidate;
  if (!webrtc::SdpDeserializeCandidate(sdp_mid, sdp, &candidate, NULL)) {
    LOG(LS_ERROR) << "SdpDescrializeCandidate failed with sdp " << sdp;
  }
  return candidate;
}

JOW(jboolean, PeerConnection_nativeRemoveIceCandidates)
(JNIEnv* jni, jobject j_pc, jobjectArray j_candidates) {
  std::vector<cricket::Candidate> candidates;
  size_t num_candidates = jni->GetArrayLength(j_candidates);
  for (size_t i = 0; i < num_candidates; ++i) {
    jobject j_candidate = jni->GetObjectArrayElement(j_candidates, i);
    candidates.push_back(GetCandidateFromJava(jni, j_candidate));
  }
  return ExtractNativePC(jni, j_pc)->RemoveIceCandidates(candidates);
}

JOW(jboolean, PeerConnection_nativeAddLocalStream)(
    JNIEnv* jni, jobject j_pc, jlong native_stream) {
  return ExtractNativePC(jni, j_pc)->AddStream(
      reinterpret_cast<MediaStreamInterface*>(native_stream));
}

JOW(void, PeerConnection_nativeRemoveLocalStream)(
    JNIEnv* jni, jobject j_pc, jlong native_stream) {
  ExtractNativePC(jni, j_pc)->RemoveStream(
      reinterpret_cast<MediaStreamInterface*>(native_stream));
}

JOW(jobject, PeerConnection_nativeCreateSender)(
    JNIEnv* jni, jobject j_pc, jstring j_kind, jstring j_stream_id) {
  jclass j_rtp_sender_class = FindClass(jni, "org/webrtc/RtpSender");
  jmethodID j_rtp_sender_ctor =
      GetMethodID(jni, j_rtp_sender_class, "<init>", "(J)V");

  std::string kind = JavaToStdString(jni, j_kind);
  std::string stream_id = JavaToStdString(jni, j_stream_id);
  rtc::scoped_refptr<RtpSenderInterface> sender =
      ExtractNativePC(jni, j_pc)->CreateSender(kind, stream_id);
  if (!sender.get()) {
    return nullptr;
  }
  jlong nativeSenderPtr = jlongFromPointer(sender.get());
  jobject j_sender =
      jni->NewObject(j_rtp_sender_class, j_rtp_sender_ctor, nativeSenderPtr);
  CHECK_EXCEPTION(jni) << "error during NewObject";
  // Sender is now owned by the Java object, and will be freed from
  // RtpSender.dispose(), called by PeerConnection.dispose() or getSenders().
  sender->AddRef();
  return j_sender;
}

JOW(jobject, PeerConnection_nativeGetSenders)(JNIEnv* jni, jobject j_pc) {
  jclass j_array_list_class = FindClass(jni, "java/util/ArrayList");
  jmethodID j_array_list_ctor =
      GetMethodID(jni, j_array_list_class, "<init>", "()V");
  jmethodID j_array_list_add =
      GetMethodID(jni, j_array_list_class, "add", "(Ljava/lang/Object;)Z");
  jobject j_senders = jni->NewObject(j_array_list_class, j_array_list_ctor);
  CHECK_EXCEPTION(jni) << "error during NewObject";

  jclass j_rtp_sender_class = FindClass(jni, "org/webrtc/RtpSender");
  jmethodID j_rtp_sender_ctor =
      GetMethodID(jni, j_rtp_sender_class, "<init>", "(J)V");

  auto senders = ExtractNativePC(jni, j_pc)->GetSenders();
  for (const auto& sender : senders) {
    jlong nativeSenderPtr = jlongFromPointer(sender.get());
    jobject j_sender =
        jni->NewObject(j_rtp_sender_class, j_rtp_sender_ctor, nativeSenderPtr);
    CHECK_EXCEPTION(jni) << "error during NewObject";
    // Sender is now owned by the Java object, and will be freed from
    // RtpSender.dispose(), called by PeerConnection.dispose() or getSenders().
    sender->AddRef();
    jni->CallBooleanMethod(j_senders, j_array_list_add, j_sender);
    CHECK_EXCEPTION(jni) << "error during CallBooleanMethod";
  }
  return j_senders;
}

JOW(jobject, PeerConnection_nativeGetReceivers)(JNIEnv* jni, jobject j_pc) {
  jclass j_array_list_class = FindClass(jni, "java/util/ArrayList");
  jmethodID j_array_list_ctor =
      GetMethodID(jni, j_array_list_class, "<init>", "()V");
  jmethodID j_array_list_add =
      GetMethodID(jni, j_array_list_class, "add", "(Ljava/lang/Object;)Z");
  jobject j_receivers = jni->NewObject(j_array_list_class, j_array_list_ctor);
  CHECK_EXCEPTION(jni) << "error during NewObject";

  jclass j_rtp_receiver_class = FindClass(jni, "org/webrtc/RtpReceiver");
  jmethodID j_rtp_receiver_ctor =
      GetMethodID(jni, j_rtp_receiver_class, "<init>", "(J)V");

  auto receivers = ExtractNativePC(jni, j_pc)->GetReceivers();
  for (const auto& receiver : receivers) {
    jlong nativeReceiverPtr = jlongFromPointer(receiver.get());
    jobject j_receiver = jni->NewObject(j_rtp_receiver_class,
                                        j_rtp_receiver_ctor, nativeReceiverPtr);
    CHECK_EXCEPTION(jni) << "error during NewObject";
    // Receiver is now owned by Java object, and will be freed from there.
    receiver->AddRef();
    jni->CallBooleanMethod(j_receivers, j_array_list_add, j_receiver);
    CHECK_EXCEPTION(jni) << "error during CallBooleanMethod";
  }
  return j_receivers;
}

JOW(bool, PeerConnection_nativeOldGetStats)
(JNIEnv* jni, jobject j_pc, jobject j_observer, jlong native_track) {
  rtc::scoped_refptr<StatsObserverWrapper> observer(
      new rtc::RefCountedObject<StatsObserverWrapper>(jni, j_observer));
  return ExtractNativePC(jni, j_pc)->GetStats(
      observer,
      reinterpret_cast<MediaStreamTrackInterface*>(native_track),
      PeerConnectionInterface::kStatsOutputLevelStandard);
}

JOW(void, PeerConnection_nativeNewGetStats)
(JNIEnv* jni, jobject j_pc, jobject j_callback) {
  rtc::scoped_refptr<RTCStatsCollectorCallbackWrapper> callback(
      new rtc::RefCountedObject<RTCStatsCollectorCallbackWrapper>(jni,
                                                                  j_callback));
  ExtractNativePC(jni, j_pc)->GetStats(callback);
}

JOW(bool, PeerConnection_nativeStartRtcEventLog)(
    JNIEnv* jni, jobject j_pc, int file_descriptor, int max_size_bytes) {
  return ExtractNativePC(jni, j_pc)->StartRtcEventLog(file_descriptor,
                                                      max_size_bytes);
}

JOW(void, PeerConnection_nativeStopRtcEventLog)(JNIEnv* jni, jobject j_pc) {
  ExtractNativePC(jni, j_pc)->StopRtcEventLog();
}

JOW(jobject, PeerConnection_signalingState)(JNIEnv* jni, jobject j_pc) {
  PeerConnectionInterface::SignalingState state =
      ExtractNativePC(jni, j_pc)->signaling_state();
  return JavaEnumFromIndex(jni, "PeerConnection$SignalingState", state);
}

JOW(jobject, PeerConnection_iceConnectionState)(JNIEnv* jni, jobject j_pc) {
  PeerConnectionInterface::IceConnectionState state =
      ExtractNativePC(jni, j_pc)->ice_connection_state();
  return JavaEnumFromIndex(jni, "PeerConnection$IceConnectionState", state);
}

JOW(jobject, PeerConnection_iceGatheringState)(JNIEnv* jni, jobject j_pc) {
  PeerConnectionInterface::IceGatheringState state =
      ExtractNativePC(jni, j_pc)->ice_gathering_state();
  return JavaEnumFromIndex(jni, "PeerConnection$IceGatheringState", state);
}

JOW(void, PeerConnection_close)(JNIEnv* jni, jobject j_pc) {
  ExtractNativePC(jni, j_pc)->Close();
  return;
}

JOW(jobject, MediaSource_nativeState)(JNIEnv* jni, jclass, jlong j_p) {
  rtc::scoped_refptr<MediaSourceInterface> p(
      reinterpret_cast<MediaSourceInterface*>(j_p));
  return JavaEnumFromIndex(jni, "MediaSource$State", p->state());
}

JOW(jstring, MediaStreamTrack_nativeId)(JNIEnv* jni, jclass, jlong j_p) {
  return JavaStringFromStdString(
      jni, reinterpret_cast<MediaStreamTrackInterface*>(j_p)->id());
}

JOW(jstring, MediaStreamTrack_nativeKind)(JNIEnv* jni, jclass, jlong j_p) {
  return JavaStringFromStdString(
      jni, reinterpret_cast<MediaStreamTrackInterface*>(j_p)->kind());
}

JOW(jboolean, MediaStreamTrack_nativeEnabled)(JNIEnv* jni, jclass, jlong j_p) {
  return reinterpret_cast<MediaStreamTrackInterface*>(j_p)->enabled();
}

JOW(jobject, MediaStreamTrack_nativeState)(JNIEnv* jni, jclass, jlong j_p) {
  return JavaEnumFromIndex(
      jni,
      "MediaStreamTrack$State",
      reinterpret_cast<MediaStreamTrackInterface*>(j_p)->state());
}

JOW(jboolean, MediaStreamTrack_nativeSetEnabled)(
    JNIEnv* jni, jclass, jlong j_p, jboolean enabled) {
  return reinterpret_cast<MediaStreamTrackInterface*>(j_p)
      ->set_enabled(enabled);
}

JOW(jlong, CallSessionFileRotatingLogSink_nativeAddSink)(
    JNIEnv* jni, jclass,
    jstring j_dirPath, jint j_maxFileSize, jint j_severity) {
  std::string dir_path = JavaToStdString(jni, j_dirPath);
  rtc::CallSessionFileRotatingLogSink* sink =
      new rtc::CallSessionFileRotatingLogSink(dir_path, j_maxFileSize);
  if (!sink->Init()) {
    LOG_V(rtc::LoggingSeverity::LS_WARNING) <<
        "Failed to init CallSessionFileRotatingLogSink for path " << dir_path;
    delete sink;
    return 0;
  }
  rtc::LogMessage::AddLogToStream(
      sink, static_cast<rtc::LoggingSeverity>(j_severity));
  return (jlong) sink;
}

JOW(void, CallSessionFileRotatingLogSink_nativeDeleteSink)(
    JNIEnv* jni, jclass, jlong j_sink) {
  rtc::CallSessionFileRotatingLogSink* sink =
      reinterpret_cast<rtc::CallSessionFileRotatingLogSink*>(j_sink);
  rtc::LogMessage::RemoveLogToStream(sink);
  delete sink;
}

JOW(jbyteArray, CallSessionFileRotatingLogSink_nativeGetLogData)(
    JNIEnv* jni, jclass, jstring j_dirPath) {
  std::string dir_path = JavaToStdString(jni, j_dirPath);
  std::unique_ptr<rtc::CallSessionFileRotatingStream> stream(
      new rtc::CallSessionFileRotatingStream(dir_path));
  if (!stream->Open()) {
    LOG_V(rtc::LoggingSeverity::LS_WARNING) <<
        "Failed to open CallSessionFileRotatingStream for path " << dir_path;
    return jni->NewByteArray(0);
  }
  size_t log_size = 0;
  if (!stream->GetSize(&log_size) || log_size == 0) {
    LOG_V(rtc::LoggingSeverity::LS_WARNING) <<
        "CallSessionFileRotatingStream returns 0 size for path " << dir_path;
    return jni->NewByteArray(0);
  }

  size_t read = 0;
  std::unique_ptr<jbyte> buffer(static_cast<jbyte*>(malloc(log_size)));
  stream->ReadAll(buffer.get(), log_size, &read, nullptr);

  jbyteArray result = jni->NewByteArray(read);
  jni->SetByteArrayRegion(result, 0, read, buffer.get());

  return result;
}

JOW(jboolean, RtpSender_nativeSetTrack)(JNIEnv* jni,
                                    jclass,
                                    jlong j_rtp_sender_pointer,
                                    jlong j_track_pointer) {
  return reinterpret_cast<RtpSenderInterface*>(j_rtp_sender_pointer)
      ->SetTrack(reinterpret_cast<MediaStreamTrackInterface*>(j_track_pointer));
}

JOW(jlong, RtpSender_nativeGetTrack)
(JNIEnv* jni, jclass, jlong j_rtp_sender_pointer) {
  return jlongFromPointer(
      reinterpret_cast<RtpSenderInterface*>(j_rtp_sender_pointer)
          ->track()
          .release());
}

JOW(jlong, RtpSender_nativeGetDtmfSender)
(JNIEnv* jni, jclass, jlong j_rtp_sender_pointer) {
  return jlongFromPointer(
      reinterpret_cast<RtpSenderInterface*>(j_rtp_sender_pointer)
          ->GetDtmfSender()
          .release());
}

static void JavaRtpParametersToJsepRtpParameters(
    JNIEnv* jni,
    jobject j_parameters,
    webrtc::RtpParameters* parameters) {
  RTC_CHECK(parameters != nullptr);
  jclass parameters_class = jni->FindClass("org/webrtc/RtpParameters");
  jfieldID encodings_id =
      GetFieldID(jni, parameters_class, "encodings", "Ljava/util/LinkedList;");
  jfieldID codecs_id =
      GetFieldID(jni, parameters_class, "codecs", "Ljava/util/LinkedList;");

  // Convert encodings.
  jobject j_encodings = GetObjectField(jni, j_parameters, encodings_id);
  jclass j_encoding_parameters_class =
      jni->FindClass("org/webrtc/RtpParameters$Encoding");
  jfieldID active_id =
      GetFieldID(jni, j_encoding_parameters_class, "active", "Z");
  jfieldID bitrate_id = GetFieldID(jni, j_encoding_parameters_class,
                                   "maxBitrateBps", "Ljava/lang/Integer;");
  jfieldID ssrc_id =
      GetFieldID(jni, j_encoding_parameters_class, "ssrc", "Ljava/lang/Long;");
  jclass j_integer_class = jni->FindClass("java/lang/Integer");
  jclass j_long_class = jni->FindClass("java/lang/Long");
  jmethodID int_value_id = GetMethodID(jni, j_integer_class, "intValue", "()I");
  jmethodID long_value_id = GetMethodID(jni, j_long_class, "longValue", "()J");

  for (jobject j_encoding_parameters : Iterable(jni, j_encodings)) {
    webrtc::RtpEncodingParameters encoding;
    encoding.active = GetBooleanField(jni, j_encoding_parameters, active_id);
    jobject j_bitrate =
        GetNullableObjectField(jni, j_encoding_parameters, bitrate_id);
    if (!IsNull(jni, j_bitrate)) {
      int bitrate_value = jni->CallIntMethod(j_bitrate, int_value_id);
      CHECK_EXCEPTION(jni) << "error during CallIntMethod";
      encoding.max_bitrate_bps = rtc::Optional<int>(bitrate_value);
    }
    jobject j_ssrc =
        GetNullableObjectField(jni, j_encoding_parameters, ssrc_id);
    if (!IsNull(jni, j_ssrc)) {
      jlong ssrc_value = jni->CallLongMethod(j_ssrc, long_value_id);
      CHECK_EXCEPTION(jni) << "error during CallLongMethod";
      encoding.ssrc = rtc::Optional<uint32_t>(ssrc_value);
    }
    parameters->encodings.push_back(encoding);
  }

  // Convert codecs.
  jobject j_codecs = GetObjectField(jni, j_parameters, codecs_id);
  jclass codec_class = jni->FindClass("org/webrtc/RtpParameters$Codec");
  jfieldID payload_type_id = GetFieldID(jni, codec_class, "payloadType", "I");
  jfieldID name_id = GetFieldID(jni, codec_class, "name", "Ljava/lang/String;");
  jfieldID kind_id = GetFieldID(jni, codec_class, "kind",
                                "Lorg/webrtc/MediaStreamTrack$MediaType;");
  jfieldID clock_rate_id =
      GetFieldID(jni, codec_class, "clockRate", "Ljava/lang/Integer;");
  jfieldID num_channels_id =
      GetFieldID(jni, codec_class, "numChannels", "Ljava/lang/Integer;");

  for (jobject j_codec : Iterable(jni, j_codecs)) {
    webrtc::RtpCodecParameters codec;
    codec.payload_type = GetIntField(jni, j_codec, payload_type_id);
    codec.name = JavaToStdString(jni, GetStringField(jni, j_codec, name_id));
    codec.kind = JavaMediaTypeToJsepMediaType(
        jni, GetObjectField(jni, j_codec, kind_id));
    jobject j_clock_rate = GetNullableObjectField(jni, j_codec, clock_rate_id);
    if (!IsNull(jni, j_clock_rate)) {
      int clock_rate_value = jni->CallIntMethod(j_clock_rate, int_value_id);
      CHECK_EXCEPTION(jni) << "error during CallIntMethod";
      codec.clock_rate = rtc::Optional<int>(clock_rate_value);
    }
    jobject j_num_channels =
        GetNullableObjectField(jni, j_codec, num_channels_id);
    if (!IsNull(jni, j_num_channels)) {
      int num_channels_value = jni->CallIntMethod(j_num_channels, int_value_id);
      CHECK_EXCEPTION(jni) << "error during CallIntMethod";
      codec.num_channels = rtc::Optional<int>(num_channels_value);
    }
    parameters->codecs.push_back(codec);
  }
}

static jobject JsepRtpParametersToJavaRtpParameters(
    JNIEnv* jni,
    const webrtc::RtpParameters& parameters) {
  jclass parameters_class = jni->FindClass("org/webrtc/RtpParameters");
  jmethodID parameters_ctor =
      GetMethodID(jni, parameters_class, "<init>", "()V");
  jobject j_parameters = jni->NewObject(parameters_class, parameters_ctor);
  CHECK_EXCEPTION(jni) << "error during NewObject";

  // Add encodings.
  jclass encoding_class = jni->FindClass("org/webrtc/RtpParameters$Encoding");
  jmethodID encoding_ctor = GetMethodID(jni, encoding_class, "<init>", "()V");
  jfieldID encodings_id =
      GetFieldID(jni, parameters_class, "encodings", "Ljava/util/LinkedList;");
  jobject j_encodings = GetObjectField(jni, j_parameters, encodings_id);
  jmethodID encodings_add = GetMethodID(jni, GetObjectClass(jni, j_encodings),
                                        "add", "(Ljava/lang/Object;)Z");
  jfieldID active_id = GetFieldID(jni, encoding_class, "active", "Z");
  jfieldID bitrate_id =
      GetFieldID(jni, encoding_class, "maxBitrateBps", "Ljava/lang/Integer;");
  jfieldID ssrc_id =
      GetFieldID(jni, encoding_class, "ssrc", "Ljava/lang/Long;");

  jclass integer_class = jni->FindClass("java/lang/Integer");
  jclass long_class = jni->FindClass("java/lang/Long");
  jmethodID integer_ctor = GetMethodID(jni, integer_class, "<init>", "(I)V");
  jmethodID long_ctor = GetMethodID(jni, long_class, "<init>", "(J)V");

  for (const webrtc::RtpEncodingParameters& encoding : parameters.encodings) {
    jobject j_encoding_parameters =
        jni->NewObject(encoding_class, encoding_ctor);
    CHECK_EXCEPTION(jni) << "error during NewObject";
    jni->SetBooleanField(j_encoding_parameters, active_id, encoding.active);
    CHECK_EXCEPTION(jni) << "error during SetBooleanField";
    if (encoding.max_bitrate_bps) {
      jobject j_bitrate_value = jni->NewObject(integer_class, integer_ctor,
                                               *(encoding.max_bitrate_bps));
      CHECK_EXCEPTION(jni) << "error during NewObject";
      jni->SetObjectField(j_encoding_parameters, bitrate_id, j_bitrate_value);
      CHECK_EXCEPTION(jni) << "error during SetObjectField";
    }
    if (encoding.ssrc) {
      jobject j_ssrc_value = jni->NewObject(long_class, long_ctor,
                                            static_cast<jlong>(*encoding.ssrc));
      CHECK_EXCEPTION(jni) << "error during NewObject";
      jni->SetObjectField(j_encoding_parameters, ssrc_id, j_ssrc_value);
      CHECK_EXCEPTION(jni) << "error during SetObjectField";
    }
    jboolean added = jni->CallBooleanMethod(j_encodings, encodings_add,
                                            j_encoding_parameters);
    CHECK_EXCEPTION(jni) << "error during CallBooleanMethod";
    RTC_CHECK(added);
  }

  // Add codecs.
  jclass codec_class = jni->FindClass("org/webrtc/RtpParameters$Codec");
  jmethodID codec_ctor = GetMethodID(jni, codec_class, "<init>", "()V");
  jfieldID codecs_id =
      GetFieldID(jni, parameters_class, "codecs", "Ljava/util/LinkedList;");
  jobject j_codecs = GetObjectField(jni, j_parameters, codecs_id);
  jmethodID codecs_add = GetMethodID(jni, GetObjectClass(jni, j_codecs), "add",
                                     "(Ljava/lang/Object;)Z");
  jfieldID payload_type_id = GetFieldID(jni, codec_class, "payloadType", "I");
  jfieldID name_id = GetFieldID(jni, codec_class, "name", "Ljava/lang/String;");
  jfieldID kind_id = GetFieldID(jni, codec_class, "kind",
                                "Lorg/webrtc/MediaStreamTrack$MediaType;");
  jfieldID clock_rate_id =
      GetFieldID(jni, codec_class, "clockRate", "Ljava/lang/Integer;");
  jfieldID num_channels_id =
      GetFieldID(jni, codec_class, "numChannels", "Ljava/lang/Integer;");

  for (const webrtc::RtpCodecParameters& codec : parameters.codecs) {
    jobject j_codec = jni->NewObject(codec_class, codec_ctor);
    CHECK_EXCEPTION(jni) << "error during NewObject";
    jni->SetIntField(j_codec, payload_type_id, codec.payload_type);
    CHECK_EXCEPTION(jni) << "error during SetIntField";
    jni->SetObjectField(j_codec, name_id,
                        JavaStringFromStdString(jni, codec.name));
    CHECK_EXCEPTION(jni) << "error during SetObjectField";
    jni->SetObjectField(j_codec, kind_id,
                        JsepMediaTypeToJavaMediaType(jni, codec.kind));
    CHECK_EXCEPTION(jni) << "error during SetObjectField";
    if (codec.clock_rate) {
      jobject j_clock_rate_value =
          jni->NewObject(integer_class, integer_ctor, *(codec.clock_rate));
      CHECK_EXCEPTION(jni) << "error during NewObject";
      jni->SetObjectField(j_codec, clock_rate_id, j_clock_rate_value);
      CHECK_EXCEPTION(jni) << "error during SetObjectField";
    }
    if (codec.num_channels) {
      jobject j_num_channels_value =
          jni->NewObject(integer_class, integer_ctor, *(codec.num_channels));
      CHECK_EXCEPTION(jni) << "error during NewObject";
      jni->SetObjectField(j_codec, num_channels_id, j_num_channels_value);
      CHECK_EXCEPTION(jni) << "error during SetObjectField";
    }
    jboolean added = jni->CallBooleanMethod(j_codecs, codecs_add, j_codec);
    CHECK_EXCEPTION(jni) << "error during CallBooleanMethod";
    RTC_CHECK(added);
  }

  return j_parameters;
}

JOW(jboolean, RtpSender_nativeSetParameters)
(JNIEnv* jni, jclass, jlong j_rtp_sender_pointer, jobject j_parameters) {
  if (IsNull(jni, j_parameters)) {
    return false;
  }
  webrtc::RtpParameters parameters;
  JavaRtpParametersToJsepRtpParameters(jni, j_parameters, &parameters);
  return reinterpret_cast<RtpSenderInterface*>(j_rtp_sender_pointer)
      ->SetParameters(parameters);
}

JOW(jobject, RtpSender_nativeGetParameters)
(JNIEnv* jni, jclass, jlong j_rtp_sender_pointer) {
  webrtc::RtpParameters parameters =
      reinterpret_cast<RtpSenderInterface*>(j_rtp_sender_pointer)
          ->GetParameters();
  return JsepRtpParametersToJavaRtpParameters(jni, parameters);
}

JOW(jstring, RtpSender_nativeId)(
    JNIEnv* jni, jclass, jlong j_rtp_sender_pointer) {
  return JavaStringFromStdString(
      jni, reinterpret_cast<RtpSenderInterface*>(j_rtp_sender_pointer)->id());
}

JOW(void, RtpSender_free)(JNIEnv* jni, jclass, jlong j_rtp_sender_pointer) {
  reinterpret_cast<RtpSenderInterface*>(j_rtp_sender_pointer)->Release();
}

JOW(jlong, RtpReceiver_nativeGetTrack)(JNIEnv* jni,
                                    jclass,
                                    jlong j_rtp_receiver_pointer,
                                    jlong j_track_pointer) {
  return jlongFromPointer(
      reinterpret_cast<RtpReceiverInterface*>(j_rtp_receiver_pointer)
          ->track()
          .release());
}

JOW(jboolean, RtpReceiver_nativeSetParameters)
(JNIEnv* jni, jclass, jlong j_rtp_receiver_pointer, jobject j_parameters) {
  if (IsNull(jni, j_parameters)) {
    return false;
  }
  webrtc::RtpParameters parameters;
  JavaRtpParametersToJsepRtpParameters(jni, j_parameters, &parameters);
  return reinterpret_cast<RtpReceiverInterface*>(j_rtp_receiver_pointer)
      ->SetParameters(parameters);
}

JOW(jobject, RtpReceiver_nativeGetParameters)
(JNIEnv* jni, jclass, jlong j_rtp_receiver_pointer) {
  webrtc::RtpParameters parameters =
      reinterpret_cast<RtpReceiverInterface*>(j_rtp_receiver_pointer)
          ->GetParameters();
  return JsepRtpParametersToJavaRtpParameters(jni, parameters);
}

JOW(jstring, RtpReceiver_nativeId)(
    JNIEnv* jni, jclass, jlong j_rtp_receiver_pointer) {
  return JavaStringFromStdString(
      jni,
      reinterpret_cast<RtpReceiverInterface*>(j_rtp_receiver_pointer)->id());
}

JOW(void, RtpReceiver_free)
(JNIEnv* jni, jclass, jlong j_rtp_receiver_pointer) {
  reinterpret_cast<RtpReceiverInterface*>(j_rtp_receiver_pointer)->Release();
}

JOW(jlong, RtpReceiver_nativeSetObserver)
(JNIEnv* jni, jclass, jlong j_rtp_receiver_pointer, jobject j_observer) {
  RtpReceiverObserver* rtpReceiverObserver =
      new RtpReceiverObserver(jni, j_observer);
  reinterpret_cast<RtpReceiverInterface*>(j_rtp_receiver_pointer)
      ->SetObserver(rtpReceiverObserver);
  return jlongFromPointer(rtpReceiverObserver);
}

JOW(void, RtpReceiver_nativeUnsetObserver)
(JNIEnv* jni, jclass, jlong j_rtp_receiver_pointer, jlong j_observer_pointer) {
  reinterpret_cast<RtpReceiverInterface*>(j_rtp_receiver_pointer)
      ->SetObserver(nullptr);
  RtpReceiverObserver* observer =
      reinterpret_cast<RtpReceiverObserver*>(j_observer_pointer);
  if (observer) {
    delete observer;
  }
}

JOW(jboolean, DtmfSender_nativeCanInsertDtmf)
(JNIEnv* jni, jclass, jlong j_dtmf_sender_pointer) {
  return reinterpret_cast<DtmfSenderInterface*>(j_dtmf_sender_pointer)
      ->CanInsertDtmf();
}

JOW(jboolean, DtmfSender_nativeInsertDtmf)
(JNIEnv* jni,
 jclass,
 jlong j_dtmf_sender_pointer,
 jstring tones,
 jint duration,
 jint inter_tone_gap) {
  return reinterpret_cast<DtmfSenderInterface*>(j_dtmf_sender_pointer)
      ->InsertDtmf(JavaToStdString(jni, tones), duration, inter_tone_gap);
}

JOW(jstring, DtmfSender_nativeTones)
(JNIEnv* jni, jclass, jlong j_dtmf_sender_pointer) {
  return JavaStringFromStdString(
      jni,
      reinterpret_cast<DtmfSenderInterface*>(j_dtmf_sender_pointer)->tones());
}

JOW(jint, DtmfSender_nativeDuration)
(JNIEnv* jni, jclass, jlong j_dtmf_sender_pointer) {
  return reinterpret_cast<DtmfSenderInterface*>(j_dtmf_sender_pointer)
      ->duration();
}

JOW(jint, DtmfSender_nativeInterToneGap)
(JNIEnv* jni, jclass, jlong j_dtmf_sender_pointer) {
  return reinterpret_cast<DtmfSenderInterface*>(j_dtmf_sender_pointer)
      ->inter_tone_gap();
}

JOW(void, DtmfSender_free)
(JNIEnv* jni, jclass, jlong j_dtmf_sender_pointer) {
  reinterpret_cast<DtmfSenderInterface*>(j_dtmf_sender_pointer)->Release();
}

}  // namespace webrtc_jni
