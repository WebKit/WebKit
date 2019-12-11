/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef EXAMPLES_OBJCNATIVEAPI_OBJCCALLCLIENT_H_
#define EXAMPLES_OBJCNATIVEAPI_OBJCCALLCLIENT_H_

#include <memory>
#include <string>

#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/thread_checker.h"

@class RTCVideoCapturer;
@protocol RTCVideoRenderer;

namespace webrtc_examples {

class ObjCCallClient {
 public:
  ObjCCallClient();

  void Call(RTCVideoCapturer* capturer, id<RTCVideoRenderer> remote_renderer);
  void Hangup();

 private:
  class PCObserver : public webrtc::PeerConnectionObserver {
   public:
    explicit PCObserver(ObjCCallClient* client);

    void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override;
    void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override;
    void OnRenegotiationNeeded() override;
    void OnIceConnectionChange(
        webrtc::PeerConnectionInterface::IceConnectionState new_state) override;
    void OnIceGatheringChange(
        webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
    void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;

   private:
    const ObjCCallClient* client_;
  };

  void CreatePeerConnectionFactory() RTC_RUN_ON(thread_checker_);
  void CreatePeerConnection() RTC_RUN_ON(thread_checker_);
  void Connect() RTC_RUN_ON(thread_checker_);

  rtc::ThreadChecker thread_checker_;

  bool call_started_ RTC_GUARDED_BY(thread_checker_);

  const std::unique_ptr<PCObserver> pc_observer_;

  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> pcf_ RTC_GUARDED_BY(thread_checker_);
  std::unique_ptr<rtc::Thread> network_thread_ RTC_GUARDED_BY(thread_checker_);
  std::unique_ptr<rtc::Thread> worker_thread_ RTC_GUARDED_BY(thread_checker_);
  std::unique_ptr<rtc::Thread> signaling_thread_ RTC_GUARDED_BY(thread_checker_);

  std::unique_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> remote_sink_
      RTC_GUARDED_BY(thread_checker_);
  rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> video_source_
      RTC_GUARDED_BY(thread_checker_);

  rtc::CriticalSection pc_mutex_;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc_ RTC_GUARDED_BY(pc_mutex_);
};

}  // namespace webrtc_examples

#endif  // EXAMPLES_OBJCNATIVEAPI_OBJCCALLCLIENT_H_
