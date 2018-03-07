/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Lifecycle notes: objects are owned where they will be called; in other words
// FooObservers are owned by C++-land, and user-callable objects (e.g.
// PeerConnection and VideoTrack) are owned by Java-land.
// When this file (or other files in this directory) allocates C++
// RefCountInterfaces it AddRef()s an artificial ref simulating the jlong held
// in Java-land, and then Release()s the ref in the respective free call.
// Sometimes this AddRef is implicit in the construction of a scoped_refptr<>
// which is then .release()d. Any persistent (non-local) references from C++ to
// Java must be global or weak (in which case they must be checked before use)!
//
// Exception notes: pretty much all JNI calls can throw Java exceptions, so each
// call through a JNIEnv* pointer needs to be followed by an ExceptionCheck()
// call. In this file this is done in CHECK_EXCEPTION, making for much easier
// debugging in case of failure (the alternative is to wait for control to
// return to the Java frame that called code in this file, at which point it's
// impossible to tell which JNI call broke).

#include <limits>
#include <memory>
#include <utility>

#include "api/mediaconstraintsinterface.h"
#include "api/peerconnectioninterface.h"
#include "api/rtpreceiverinterface.h"
#include "api/rtpsenderinterface.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "sdk/android/src/jni/classreferenceholder.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/pc/datachannel.h"
#include "sdk/android/src/jni/pc/java_native_conversion.h"
#include "sdk/android/src/jni/pc/mediaconstraints_jni.h"
#include "sdk/android/src/jni/pc/peerconnectionobserver_jni.h"
#include "sdk/android/src/jni/pc/rtcstatscollectorcallbackwrapper.h"
#include "sdk/android/src/jni/pc/sdpobserver_jni.h"
#include "sdk/android/src/jni/pc/statsobserver_jni.h"

namespace webrtc {
namespace jni {

static rtc::scoped_refptr<PeerConnectionInterface> ExtractNativePC(
    JNIEnv* jni,
    jobject j_pc) {
  jfieldID native_pc_id =
      GetFieldID(jni, GetObjectClass(jni, j_pc), "nativePeerConnection", "J");
  jlong j_p = GetLongField(jni, j_pc, native_pc_id);
  return rtc::scoped_refptr<PeerConnectionInterface>(
      reinterpret_cast<PeerConnectionInterface*>(j_p));
}

JNI_FUNCTION_DECLARATION(void,
                         PeerConnection_freeObserver,
                         JNIEnv*,
                         jclass,
                         jlong j_p) {
  PeerConnectionObserverJni* p =
      reinterpret_cast<PeerConnectionObserverJni*>(j_p);
  delete p;
}

JNI_FUNCTION_DECLARATION(jobject,
                         PeerConnection_getLocalDescription,
                         JNIEnv* jni,
                         jobject j_pc) {
  const SessionDescriptionInterface* sdp =
      ExtractNativePC(jni, j_pc)->local_description();
  return sdp ? NativeToJavaSessionDescription(jni, sdp) : NULL;
}

JNI_FUNCTION_DECLARATION(jobject,
                         PeerConnection_getRemoteDescription,
                         JNIEnv* jni,
                         jobject j_pc) {
  const SessionDescriptionInterface* sdp =
      ExtractNativePC(jni, j_pc)->remote_description();
  return sdp ? NativeToJavaSessionDescription(jni, sdp) : NULL;
}

JNI_FUNCTION_DECLARATION(jobject,
                         PeerConnection_createDataChannel,
                         JNIEnv* jni,
                         jobject j_pc,
                         jstring j_label,
                         jobject j_init) {
  DataChannelInit init = JavaToNativeDataChannelInit(jni, j_init);
  rtc::scoped_refptr<DataChannelInterface> channel(
      ExtractNativePC(jni, j_pc)->CreateDataChannel(
          JavaToStdString(jni, j_label), &init));
  return WrapNativeDataChannel(jni, channel);
}

JNI_FUNCTION_DECLARATION(void,
                         PeerConnection_createOffer,
                         JNIEnv* jni,
                         jobject j_pc,
                         jobject j_observer,
                         jobject j_constraints) {
  std::unique_ptr<MediaConstraintsInterface> constraints =
      JavaToNativeMediaConstraints(jni, j_constraints);
  rtc::scoped_refptr<CreateSdpObserverJni> observer(
      new rtc::RefCountedObject<CreateSdpObserverJni>(jni, j_observer,
                                                      std::move(constraints)));
  ExtractNativePC(jni, j_pc)->CreateOffer(observer, observer->constraints());
}

JNI_FUNCTION_DECLARATION(void,
                         PeerConnection_createAnswer,
                         JNIEnv* jni,
                         jobject j_pc,
                         jobject j_observer,
                         jobject j_constraints) {
  std::unique_ptr<MediaConstraintsInterface> constraints =
      JavaToNativeMediaConstraints(jni, j_constraints);
  rtc::scoped_refptr<CreateSdpObserverJni> observer(
      new rtc::RefCountedObject<CreateSdpObserverJni>(jni, j_observer,
                                                      std::move(constraints)));
  ExtractNativePC(jni, j_pc)->CreateAnswer(observer, observer->constraints());
}

JNI_FUNCTION_DECLARATION(void,
                         PeerConnection_setLocalDescription,
                         JNIEnv* jni,
                         jobject j_pc,
                         jobject j_observer,
                         jobject j_sdp) {
  rtc::scoped_refptr<SetSdpObserverJni> observer(
      new rtc::RefCountedObject<SetSdpObserverJni>(jni, j_observer, nullptr));
  ExtractNativePC(jni, j_pc)->SetLocalDescription(
      observer, JavaToNativeSessionDescription(jni, j_sdp));
}

JNI_FUNCTION_DECLARATION(void,
                         PeerConnection_setRemoteDescription,
                         JNIEnv* jni,
                         jobject j_pc,
                         jobject j_observer,
                         jobject j_sdp) {
  rtc::scoped_refptr<SetSdpObserverJni> observer(
      new rtc::RefCountedObject<SetSdpObserverJni>(jni, j_observer, nullptr));
  ExtractNativePC(jni, j_pc)->SetRemoteDescription(
      observer, JavaToNativeSessionDescription(jni, j_sdp));
}

JNI_FUNCTION_DECLARATION(void,
                         PeerConnection_setAudioPlayout,
                         JNIEnv* jni,
                         jobject j_pc,
                         jboolean playout) {
  ExtractNativePC(jni, j_pc)->SetAudioPlayout(playout);
}

JNI_FUNCTION_DECLARATION(void,
                         PeerConnection_setAudioRecording,
                         JNIEnv* jni,
                         jobject j_pc,
                         jboolean recording) {
  ExtractNativePC(jni, j_pc)->SetAudioRecording(recording);
}

JNI_FUNCTION_DECLARATION(jboolean,
                         PeerConnection_nativeSetConfiguration,
                         JNIEnv* jni,
                         jobject j_pc,
                         jobject j_rtc_config,
                         jlong native_observer) {
  // Need to merge constraints into RTCConfiguration again, which are stored
  // in the observer object.
  PeerConnectionObserverJni* observer =
      reinterpret_cast<PeerConnectionObserverJni*>(native_observer);
  PeerConnectionInterface::RTCConfiguration rtc_config(
      PeerConnectionInterface::RTCConfigurationType::kAggressive);
  JavaToNativeRTCConfiguration(jni, j_rtc_config, &rtc_config);
  CopyConstraintsIntoRtcConfiguration(observer->constraints(), &rtc_config);
  return ExtractNativePC(jni, j_pc)->SetConfiguration(rtc_config);
}

JNI_FUNCTION_DECLARATION(jboolean,
                         PeerConnection_nativeAddIceCandidate,
                         JNIEnv* jni,
                         jobject j_pc,
                         jstring j_sdp_mid,
                         jint j_sdp_mline_index,
                         jstring j_candidate_sdp) {
  std::string sdp_mid = JavaToStdString(jni, j_sdp_mid);
  std::string sdp = JavaToStdString(jni, j_candidate_sdp);
  std::unique_ptr<IceCandidateInterface> candidate(
      CreateIceCandidate(sdp_mid, j_sdp_mline_index, sdp, nullptr));
  return ExtractNativePC(jni, j_pc)->AddIceCandidate(candidate.get());
}

JNI_FUNCTION_DECLARATION(jboolean,
                         PeerConnection_nativeRemoveIceCandidates,
                         JNIEnv* jni,
                         jobject j_pc,
                         jobjectArray j_candidates) {
  std::vector<cricket::Candidate> candidates;
  size_t num_candidates = jni->GetArrayLength(j_candidates);
  for (size_t i = 0; i < num_candidates; ++i) {
    jobject j_candidate = jni->GetObjectArrayElement(j_candidates, i);
    candidates.push_back(JavaToNativeCandidate(jni, j_candidate));
  }
  return ExtractNativePC(jni, j_pc)->RemoveIceCandidates(candidates);
}

JNI_FUNCTION_DECLARATION(jboolean,
                         PeerConnection_nativeAddLocalStream,
                         JNIEnv* jni,
                         jobject j_pc,
                         jlong native_stream) {
  return ExtractNativePC(jni, j_pc)->AddStream(
      reinterpret_cast<MediaStreamInterface*>(native_stream));
}

JNI_FUNCTION_DECLARATION(void,
                         PeerConnection_nativeRemoveLocalStream,
                         JNIEnv* jni,
                         jobject j_pc,
                         jlong native_stream) {
  ExtractNativePC(jni, j_pc)->RemoveStream(
      reinterpret_cast<MediaStreamInterface*>(native_stream));
}

JNI_FUNCTION_DECLARATION(jobject,
                         PeerConnection_nativeCreateSender,
                         JNIEnv* jni,
                         jobject j_pc,
                         jstring j_kind,
                         jstring j_stream_id) {
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

JNI_FUNCTION_DECLARATION(jobject,
                         PeerConnection_nativeGetSenders,
                         JNIEnv* jni,
                         jobject j_pc) {
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

JNI_FUNCTION_DECLARATION(jobject,
                         PeerConnection_nativeGetReceivers,
                         JNIEnv* jni,
                         jobject j_pc) {
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

JNI_FUNCTION_DECLARATION(bool,
                         PeerConnection_nativeOldGetStats,
                         JNIEnv* jni,
                         jobject j_pc,
                         jobject j_observer,
                         jlong native_track) {
  rtc::scoped_refptr<StatsObserverJni> observer(
      new rtc::RefCountedObject<StatsObserverJni>(jni, j_observer));
  return ExtractNativePC(jni, j_pc)->GetStats(
      observer, reinterpret_cast<MediaStreamTrackInterface*>(native_track),
      PeerConnectionInterface::kStatsOutputLevelStandard);
}

JNI_FUNCTION_DECLARATION(void,
                         PeerConnection_nativeNewGetStats,
                         JNIEnv* jni,
                         jobject j_pc,
                         jobject j_callback) {
  rtc::scoped_refptr<RTCStatsCollectorCallbackWrapper> callback(
      new rtc::RefCountedObject<RTCStatsCollectorCallbackWrapper>(jni,
                                                                  j_callback));
  ExtractNativePC(jni, j_pc)->GetStats(callback);
}

JNI_FUNCTION_DECLARATION(jboolean,
                         PeerConnection_setBitrate,
                         JNIEnv* jni,
                         jobject j_pc,
                         jobject j_min,
                         jobject j_current,
                         jobject j_max) {
  PeerConnectionInterface::BitrateParameters params;
  params.min_bitrate_bps = JavaToNativeOptionalInt(jni, j_min);
  params.current_bitrate_bps = JavaToNativeOptionalInt(jni, j_current);
  params.max_bitrate_bps = JavaToNativeOptionalInt(jni, j_max);
  return ExtractNativePC(jni, j_pc)->SetBitrate(params).ok();
}

JNI_FUNCTION_DECLARATION(bool,
                         PeerConnection_nativeStartRtcEventLog,
                         JNIEnv* jni,
                         jobject j_pc,
                         int file_descriptor,
                         int max_size_bytes) {
  return ExtractNativePC(jni, j_pc)->StartRtcEventLog(file_descriptor,
                                                      max_size_bytes);
}

JNI_FUNCTION_DECLARATION(void,
                         PeerConnection_nativeStopRtcEventLog,
                         JNIEnv* jni,
                         jobject j_pc) {
  ExtractNativePC(jni, j_pc)->StopRtcEventLog();
}

JNI_FUNCTION_DECLARATION(jobject,
                         PeerConnection_signalingState,
                         JNIEnv* jni,
                         jobject j_pc) {
  PeerConnectionInterface::SignalingState state =
      ExtractNativePC(jni, j_pc)->signaling_state();
  return JavaEnumFromIndexAndClassName(jni, "PeerConnection$SignalingState",
                                       state);
}

JNI_FUNCTION_DECLARATION(jobject,
                         PeerConnection_iceConnectionState,
                         JNIEnv* jni,
                         jobject j_pc) {
  PeerConnectionInterface::IceConnectionState state =
      ExtractNativePC(jni, j_pc)->ice_connection_state();
  return JavaEnumFromIndexAndClassName(jni, "PeerConnection$IceConnectionState",
                                       state);
}

JNI_FUNCTION_DECLARATION(jobject,
                         PeerConnection_iceGatheringState,
                         JNIEnv* jni,
                         jobject j_pc) {
  PeerConnectionInterface::IceGatheringState state =
      ExtractNativePC(jni, j_pc)->ice_gathering_state();
  return JavaEnumFromIndexAndClassName(jni, "PeerConnection$IceGatheringState",
                                       state);
}

JNI_FUNCTION_DECLARATION(void,
                         PeerConnection_close,
                         JNIEnv* jni,
                         jobject j_pc) {
  ExtractNativePC(jni, j_pc)->Close();
  return;
}

}  // namespace jni
}  // namespace webrtc
