/*
 *  Copyright 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_MEDIASTREAMPROXY_H_
#define WEBRTC_API_MEDIASTREAMPROXY_H_

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/proxy.h"

namespace webrtc {

BEGIN_SIGNALING_PROXY_MAP(MediaStream)
  PROXY_CONSTMETHOD0(std::string, label)
  PROXY_METHOD0(AudioTrackVector, GetAudioTracks)
  PROXY_METHOD0(VideoTrackVector, GetVideoTracks)
  PROXY_METHOD1(rtc::scoped_refptr<AudioTrackInterface>,
                FindAudioTrack, const std::string&)
  PROXY_METHOD1(rtc::scoped_refptr<VideoTrackInterface>,
                FindVideoTrack, const std::string&)
  PROXY_METHOD1(bool, AddTrack, AudioTrackInterface*)
  PROXY_METHOD1(bool, AddTrack, VideoTrackInterface*)
  PROXY_METHOD1(bool, RemoveTrack, AudioTrackInterface*)
  PROXY_METHOD1(bool, RemoveTrack, VideoTrackInterface*)
  PROXY_METHOD1(void, RegisterObserver, ObserverInterface*)
  PROXY_METHOD1(void, UnregisterObserver, ObserverInterface*)
END_SIGNALING_PROXY()

}  // namespace webrtc

#endif  // WEBRTC_API_MEDIASTREAMPROXY_H_
