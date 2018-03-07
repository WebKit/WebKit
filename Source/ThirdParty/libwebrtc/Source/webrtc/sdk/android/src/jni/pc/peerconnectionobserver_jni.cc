/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/peerconnectionobserver_jni.h"

#include <pc/mediastreamobserver.h>
#include <string>

#include "rtc_base/ptr_util.h"
#include "sdk/android/generated_peerconnection_jni/jni/MediaStream_jni.h"
#include "sdk/android/src/jni/classreferenceholder.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/pc/datachannel.h"
#include "sdk/android/src/jni/pc/java_native_conversion.h"

namespace webrtc {
namespace jni {

// Convenience, used since callbacks occur on the signaling thread, which may
// be a non-Java thread.
static JNIEnv* jni() {
  return AttachCurrentThreadIfNeeded();
}

PeerConnectionObserverJni::PeerConnectionObserverJni(JNIEnv* jni,
                                                     jobject j_observer)
    : j_observer_global_(jni, j_observer),
      j_observer_class_(jni, GetObjectClass(jni, *j_observer_global_)),
      j_rtp_receiver_class_(jni, FindClass(jni, "org/webrtc/RtpReceiver")),
      j_rtp_receiver_ctor_(
          GetMethodID(jni, *j_rtp_receiver_class_, "<init>", "(J)V")) {}

PeerConnectionObserverJni::~PeerConnectionObserverJni() {
  ScopedLocalRefFrame local_ref_frame(jni());
  while (!remote_streams_.empty())
    DisposeRemoteStream(remote_streams_.begin());
  while (!rtp_receivers_.empty())
    DisposeRtpReceiver(rtp_receivers_.begin());
}

void PeerConnectionObserverJni::OnIceCandidate(
    const IceCandidateInterface* candidate) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  jobject j_candidate = NativeToJavaIceCandidate(env, *candidate);

  jmethodID m = GetMethodID(env, *j_observer_class_, "onIceCandidate",
                            "(Lorg/webrtc/IceCandidate;)V");
  env->CallVoidMethod(*j_observer_global_, m, j_candidate);
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";
}

void PeerConnectionObserverJni::OnIceCandidatesRemoved(
    const std::vector<cricket::Candidate>& candidates) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jobjectArray candidates_array = NativeToJavaCandidateArray(jni(), candidates);
  jmethodID m = GetMethodID(jni(), *j_observer_class_, "onIceCandidatesRemoved",
                            "([Lorg/webrtc/IceCandidate;)V");
  jni()->CallVoidMethod(*j_observer_global_, m, candidates_array);
  CHECK_EXCEPTION(jni()) << "Error during CallVoidMethod";
}

void PeerConnectionObserverJni::OnSignalingChange(
    PeerConnectionInterface::SignalingState new_state) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jmethodID m = GetMethodID(jni(), *j_observer_class_, "onSignalingChange",
                            "(Lorg/webrtc/PeerConnection$SignalingState;)V");
  jobject new_state_enum = JavaEnumFromIndexAndClassName(
      jni(), "PeerConnection$SignalingState", new_state);
  jni()->CallVoidMethod(*j_observer_global_, m, new_state_enum);
  CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
}

void PeerConnectionObserverJni::OnIceConnectionChange(
    PeerConnectionInterface::IceConnectionState new_state) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jmethodID m =
      GetMethodID(jni(), *j_observer_class_, "onIceConnectionChange",
                  "(Lorg/webrtc/PeerConnection$IceConnectionState;)V");
  jobject new_state_enum = JavaEnumFromIndexAndClassName(
      jni(), "PeerConnection$IceConnectionState", new_state);
  jni()->CallVoidMethod(*j_observer_global_, m, new_state_enum);
  CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
}

void PeerConnectionObserverJni::OnIceConnectionReceivingChange(bool receiving) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jmethodID m = GetMethodID(jni(), *j_observer_class_,
                            "onIceConnectionReceivingChange", "(Z)V");
  jni()->CallVoidMethod(*j_observer_global_, m, receiving);
  CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
}

void PeerConnectionObserverJni::OnIceGatheringChange(
    PeerConnectionInterface::IceGatheringState new_state) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jmethodID m = GetMethodID(jni(), *j_observer_class_, "onIceGatheringChange",
                            "(Lorg/webrtc/PeerConnection$IceGatheringState;)V");
  jobject new_state_enum = JavaEnumFromIndexAndClassName(
      jni(), "PeerConnection$IceGatheringState", new_state);
  jni()->CallVoidMethod(*j_observer_global_, m, new_state_enum);
  CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
}

void PeerConnectionObserverJni::OnAddStream(
    rtc::scoped_refptr<MediaStreamInterface> stream) {
  ScopedLocalRefFrame local_ref_frame(jni());
  // The stream could be added into the remote_streams_ map when calling
  // OnAddTrack.
  jobject j_stream = GetOrCreateJavaStream(stream);

  for (const auto& track : stream->GetAudioTracks()) {
    AddNativeAudioTrackToJavaStream(track, j_stream);
  }
  for (const auto& track : stream->GetVideoTracks()) {
    AddNativeVideoTrackToJavaStream(track, j_stream);
  }

  jmethodID m = GetMethodID(jni(), *j_observer_class_, "onAddStream",
                            "(Lorg/webrtc/MediaStream;)V");
  jni()->CallVoidMethod(*j_observer_global_, m, j_stream);
  CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";

  // Create an observer to update the Java stream when the native stream's set
  // of tracks changes.
  auto observer = rtc::MakeUnique<MediaStreamObserver>(stream);
  observer->SignalAudioTrackRemoved.connect(
      this, &PeerConnectionObserverJni::OnAudioTrackRemovedFromStream);
  observer->SignalVideoTrackRemoved.connect(
      this, &PeerConnectionObserverJni::OnVideoTrackRemovedFromStream);
  observer->SignalAudioTrackAdded.connect(
      this, &PeerConnectionObserverJni::OnAudioTrackAddedToStream);
  observer->SignalVideoTrackAdded.connect(
      this, &PeerConnectionObserverJni::OnVideoTrackAddedToStream);
  stream_observers_.push_back(std::move(observer));
}

void PeerConnectionObserverJni::AddNativeAudioTrackToJavaStream(
    rtc::scoped_refptr<AudioTrackInterface> track,
    jobject j_stream) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_MediaStream_addNativeAudioTrack(env, j_stream,
                                       jlongFromPointer(track.release()));
}

void PeerConnectionObserverJni::AddNativeVideoTrackToJavaStream(
    rtc::scoped_refptr<VideoTrackInterface> track,
    jobject j_stream) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_MediaStream_addNativeVideoTrack(env, j_stream,
                                       jlongFromPointer(track.release()));
}

void PeerConnectionObserverJni::OnAudioTrackAddedToStream(
    AudioTrackInterface* track,
    MediaStreamInterface* stream) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jobject j_stream = GetOrCreateJavaStream(stream);
  AddNativeAudioTrackToJavaStream(track, j_stream);
}

void PeerConnectionObserverJni::OnVideoTrackAddedToStream(
    VideoTrackInterface* track,
    MediaStreamInterface* stream) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jobject j_stream = GetOrCreateJavaStream(stream);
  AddNativeVideoTrackToJavaStream(track, j_stream);
}

void PeerConnectionObserverJni::OnAudioTrackRemovedFromStream(
    AudioTrackInterface* track,
    MediaStreamInterface* stream) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  jobject j_stream = GetOrCreateJavaStream(stream);
  Java_MediaStream_removeAudioTrack(env, j_stream, jlongFromPointer(track));
}

void PeerConnectionObserverJni::OnVideoTrackRemovedFromStream(
    VideoTrackInterface* track,
    MediaStreamInterface* stream) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  jobject j_stream = GetOrCreateJavaStream(stream);
  Java_MediaStream_removeVideoTrack(env, j_stream, jlongFromPointer(track));
}

void PeerConnectionObserverJni::OnRemoveStream(
    rtc::scoped_refptr<MediaStreamInterface> stream) {
  ScopedLocalRefFrame local_ref_frame(jni());
  NativeToJavaStreamsMap::iterator it = remote_streams_.find(stream);
  RTC_CHECK(it != remote_streams_.end())
      << "unexpected stream: " << std::hex << stream;
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

void PeerConnectionObserverJni::OnDataChannel(
    rtc::scoped_refptr<DataChannelInterface> channel) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  jobject j_channel = WrapNativeDataChannel(env, channel);
  jmethodID m = GetMethodID(env, *j_observer_class_, "onDataChannel",
                            "(Lorg/webrtc/DataChannel;)V");
  env->CallVoidMethod(*j_observer_global_, m, j_channel);
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";
}

void PeerConnectionObserverJni::OnRenegotiationNeeded() {
  ScopedLocalRefFrame local_ref_frame(jni());
  jmethodID m =
      GetMethodID(jni(), *j_observer_class_, "onRenegotiationNeeded", "()V");
  jni()->CallVoidMethod(*j_observer_global_, m);
  CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
}

void PeerConnectionObserverJni::OnAddTrack(
    rtc::scoped_refptr<RtpReceiverInterface> receiver,
    const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jobject j_rtp_receiver =
      jni()->NewObject(*j_rtp_receiver_class_, j_rtp_receiver_ctor_,
                       jlongFromPointer(receiver.get()));
  CHECK_EXCEPTION(jni()) << "error during NewObject";
  receiver->AddRef();
  rtp_receivers_[receiver] = NewGlobalRef(jni(), j_rtp_receiver);

  jobjectArray j_stream_array = NativeToJavaMediaStreamArray(jni(), streams);
  jmethodID m =
      GetMethodID(jni(), *j_observer_class_, "onAddTrack",
                  "(Lorg/webrtc/RtpReceiver;[Lorg/webrtc/MediaStream;)V");
  jni()->CallVoidMethod(*j_observer_global_, m, j_rtp_receiver, j_stream_array);
  CHECK_EXCEPTION(jni()) << "Error during CallVoidMethod";
}

void PeerConnectionObserverJni::SetConstraints(
    std::unique_ptr<MediaConstraintsInterface> constraints) {
  RTC_CHECK(!constraints_.get()) << "constraints already set!";
  constraints_ = std::move(constraints);
}

void PeerConnectionObserverJni::DisposeRemoteStream(
    const NativeToJavaStreamsMap::iterator& it) {
  MediaStreamInterface* stream = it->first;
  jobject j_stream = it->second;

  // Remove the observer first, so it doesn't react to events during deletion.
  stream_observers_.erase(
      std::remove_if(
          stream_observers_.begin(), stream_observers_.end(),
          [stream](const std::unique_ptr<MediaStreamObserver>& observer) {
            return observer->stream() == stream;
          }),
      stream_observers_.end());

  remote_streams_.erase(it);
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_MediaStream_dispose(env, j_stream);
  DeleteGlobalRef(env, j_stream);
}

void PeerConnectionObserverJni::DisposeRtpReceiver(
    const NativeToJavaRtpReceiverMap::iterator& it) {
  jobject j_rtp_receiver = it->second;
  rtp_receivers_.erase(it);
  jni()->CallVoidMethod(
      j_rtp_receiver,
      GetMethodID(jni(), *j_rtp_receiver_class_, "dispose", "()V"));
  CHECK_EXCEPTION(jni()) << "error during RtpReceiver.dispose()";
  DeleteGlobalRef(jni(), j_rtp_receiver);
}

// If the NativeToJavaStreamsMap contains the stream, return it.
// Otherwise, create a new Java MediaStream.
jobject PeerConnectionObserverJni::GetOrCreateJavaStream(
    const rtc::scoped_refptr<MediaStreamInterface>& stream) {
  NativeToJavaStreamsMap::iterator it = remote_streams_.find(stream);
  if (it != remote_streams_.end()) {
    return it->second;
  }
  // Java MediaStream holds one reference. Corresponding Release() is in
  // MediaStream_free, triggered by MediaStream.dispose().
  stream->AddRef();
  jobject j_stream =
      Java_MediaStream_Constructor(jni(), jlongFromPointer(stream.get()));

  remote_streams_[stream] = NewGlobalRef(jni(), j_stream);
  return j_stream;
}

jobjectArray PeerConnectionObserverJni::NativeToJavaMediaStreamArray(
    JNIEnv* jni,
    const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams) {
  jobjectArray java_streams = jni->NewObjectArray(
      streams.size(), org_webrtc_MediaStream_clazz(jni), nullptr);
  CHECK_EXCEPTION(jni) << "error during NewObjectArray";
  for (size_t i = 0; i < streams.size(); ++i) {
    jobject j_stream = GetOrCreateJavaStream(streams[i]);
    jni->SetObjectArrayElement(java_streams, i, j_stream);
  }
  return java_streams;
}

}  // namespace jni
}  // namespace webrtc
