/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_PC_PEERCONNECTIONOBSERVER_JNI_H_
#define SDK_ANDROID_SRC_JNI_PC_PEERCONNECTIONOBSERVER_JNI_H_

#include <pc/mediastreamobserver.h>
#include <map>
#include <memory>
#include <vector>

#include "api/peerconnectioninterface.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/pc/mediaconstraints_jni.h"

namespace webrtc {
namespace jni {

// Adapter between the C++ PeerConnectionObserver interface and the Java
// PeerConnection.Observer interface.  Wraps an instance of the Java interface
// and dispatches C++ callbacks to Java.
class PeerConnectionObserverJni : public PeerConnectionObserver,
                                  public sigslot::has_slots<> {
 public:
  PeerConnectionObserverJni(JNIEnv* jni, jobject j_observer);
  virtual ~PeerConnectionObserverJni();

  // Implementation of PeerConnectionObserver interface, which propagates
  // the callbacks to the Java observer.
  void OnIceCandidate(const IceCandidateInterface* candidate) override;
  void OnIceCandidatesRemoved(
      const std::vector<cricket::Candidate>& candidates) override;
  void OnSignalingChange(
      PeerConnectionInterface::SignalingState new_state) override;
  void OnIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) override;
  void OnIceConnectionReceivingChange(bool receiving) override;
  void OnIceGatheringChange(
      PeerConnectionInterface::IceGatheringState new_state) override;
  void OnAddStream(rtc::scoped_refptr<MediaStreamInterface> stream) override;
  void OnRemoveStream(rtc::scoped_refptr<MediaStreamInterface> stream) override;
  void OnDataChannel(rtc::scoped_refptr<DataChannelInterface> channel) override;
  void OnRenegotiationNeeded() override;
  void OnAddTrack(rtc::scoped_refptr<RtpReceiverInterface> receiver,
                  const std::vector<rtc::scoped_refptr<MediaStreamInterface>>&
                      streams) override;

  void SetConstraints(std::unique_ptr<MediaConstraintsInterface> constraints);
  const MediaConstraintsInterface* constraints() { return constraints_.get(); }

 private:
  typedef std::map<MediaStreamInterface*, jobject> NativeToJavaStreamsMap;
  typedef std::map<RtpReceiverInterface*, jobject> NativeToJavaRtpReceiverMap;
  typedef std::map<MediaStreamTrackInterface*, jobject>
      NativeToJavaMediaTrackMap;
  typedef std::map<MediaStreamTrackInterface*, RtpReceiverInterface*>
      NativeMediaStreamTrackToNativeRtpReceiver;

  void DisposeRemoteStream(const NativeToJavaStreamsMap::iterator& it);
  void DisposeRtpReceiver(const NativeToJavaRtpReceiverMap::iterator& it);

  // If the NativeToJavaStreamsMap contains the stream, return it.
  // Otherwise, create a new Java MediaStream.
  jobject GetOrCreateJavaStream(
      const rtc::scoped_refptr<MediaStreamInterface>& stream);

  // Converts array of streams, creating or re-using Java streams as necessary.
  jobjectArray NativeToJavaMediaStreamArray(
      JNIEnv* jni,
      const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams);

  // The three methods below must be called from within a local ref
  // frame (e.g., using ScopedLocalRefFrame), otherwise they will
  // leak references.
  //
  // Create a Java track object to wrap |track|, and add it to |j_stream|.
  void AddNativeAudioTrackToJavaStream(
      rtc::scoped_refptr<AudioTrackInterface> track,
      jobject j_stream);
  void AddNativeVideoTrackToJavaStream(
      rtc::scoped_refptr<VideoTrackInterface> track,
      jobject j_stream);

  // Callbacks invoked when a native stream changes, and the Java stream needs
  // to be updated; MediaStreamObserver is used to make this simpler.
  void OnAudioTrackAddedToStream(AudioTrackInterface* track,
                                 MediaStreamInterface* stream);
  void OnVideoTrackAddedToStream(VideoTrackInterface* track,
                                 MediaStreamInterface* stream);
  void OnAudioTrackRemovedFromStream(AudioTrackInterface* track,
                                     MediaStreamInterface* stream);
  void OnVideoTrackRemovedFromStream(VideoTrackInterface* track,
                                     MediaStreamInterface* stream);

  const ScopedGlobalRef<jobject> j_observer_global_;
  const ScopedGlobalRef<jclass> j_observer_class_;
  const ScopedGlobalRef<jclass> j_rtp_receiver_class_;
  const jmethodID j_rtp_receiver_ctor_;

  // C++ -> Java remote streams. The stored jobects are global refs and must be
  // manually deleted upon removal. Use DisposeRemoteStream().
  NativeToJavaStreamsMap remote_streams_;
  NativeToJavaRtpReceiverMap rtp_receivers_;
  std::unique_ptr<MediaConstraintsInterface> constraints_;
  std::vector<std::unique_ptr<webrtc::MediaStreamObserver>> stream_observers_;
};

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_PC_PEERCONNECTIONOBSERVER_JNI_H_
