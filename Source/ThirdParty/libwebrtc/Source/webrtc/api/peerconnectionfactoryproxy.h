/*
 *  Copyright 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_PEERCONNECTIONFACTORYPROXY_H_
#define WEBRTC_API_PEERCONNECTIONFACTORYPROXY_H_

#include <memory>
#include <string>
#include <utility>

#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/api/proxy.h"
#include "webrtc/base/bind.h"

namespace webrtc {

// TODO(deadbeef): Move this to .cc file and out of api/. What threads methods
// are called on is an implementation detail.
BEGIN_SIGNALING_PROXY_MAP(PeerConnectionFactory)
  PROXY_SIGNALING_THREAD_DESTRUCTOR()
  // Use the overloads of CreateVideoSource that take raw VideoCapturer
  // pointers from PeerConnectionFactoryInterface.
  // TODO(deadbeef): Remove this using statement once those overloads are
  // removed.
  using PeerConnectionFactoryInterface::CreateVideoSource;
  PROXY_METHOD1(void, SetOptions, const Options&)
  PROXY_METHOD5(rtc::scoped_refptr<PeerConnectionInterface>,
                CreatePeerConnection,
                const PeerConnectionInterface::RTCConfiguration&,
                const MediaConstraintsInterface*,
                std::unique_ptr<cricket::PortAllocator>,
                std::unique_ptr<rtc::RTCCertificateGeneratorInterface>,
                PeerConnectionObserver*);
  PROXY_METHOD4(rtc::scoped_refptr<PeerConnectionInterface>,
                CreatePeerConnection,
                const PeerConnectionInterface::RTCConfiguration&,
                std::unique_ptr<cricket::PortAllocator>,
                std::unique_ptr<rtc::RTCCertificateGeneratorInterface>,
                PeerConnectionObserver*);
  PROXY_METHOD1(rtc::scoped_refptr<MediaStreamInterface>,
                CreateLocalMediaStream, const std::string&)
  PROXY_METHOD1(rtc::scoped_refptr<AudioSourceInterface>,
                CreateAudioSource, const MediaConstraintsInterface*)
  PROXY_METHOD1(rtc::scoped_refptr<AudioSourceInterface>,
                CreateAudioSource,
                const cricket::AudioOptions&)
  PROXY_METHOD2(rtc::scoped_refptr<VideoTrackSourceInterface>,
                CreateVideoSource,
                std::unique_ptr<cricket::VideoCapturer>,
                const MediaConstraintsInterface*)
  PROXY_METHOD1(rtc::scoped_refptr<VideoTrackSourceInterface>,
                CreateVideoSource,
                std::unique_ptr<cricket::VideoCapturer>)
  PROXY_METHOD2(rtc::scoped_refptr<VideoTrackInterface>,
                CreateVideoTrack,
                const std::string&,
                VideoTrackSourceInterface*)
  PROXY_METHOD2(rtc::scoped_refptr<AudioTrackInterface>,
                CreateAudioTrack, const std::string&,  AudioSourceInterface*)
  PROXY_METHOD2(bool, StartAecDump, rtc::PlatformFile, int64_t)
  PROXY_METHOD0(void, StopAecDump)
  // TODO(ivoc): Remove the StartRtcEventLog and StopRtcEventLog functions as
  // soon as they are removed from PeerConnectionFactoryInterface.
  PROXY_METHOD1(bool, StartRtcEventLog, rtc::PlatformFile)
  PROXY_METHOD2(bool, StartRtcEventLog, rtc::PlatformFile, int64_t)
  PROXY_METHOD0(void, StopRtcEventLog)
END_PROXY_MAP()

}  // namespace webrtc

#endif  // WEBRTC_API_PEERCONNECTIONFACTORYPROXY_H_
