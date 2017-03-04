/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_PC_MEDIACONTROLLER_H_
#define WEBRTC_PC_MEDIACONTROLLER_H_

#include "webrtc/base/thread.h"

namespace cricket {
class ChannelManager;
struct MediaConfig;
}  // namespace cricket

namespace webrtc {
class Call;
class VoiceEngine;
class RtcEventLog;

// The MediaController currently owns shared state between media channels.
// Abstract interface is defined here such that it can be faked/mocked for
// tests, but no other real reason.
class MediaControllerInterface {
 public:
  // Will never return nullptr.
  static MediaControllerInterface* Create(
      const cricket::MediaConfig& config,
      rtc::Thread* worker_thread,
      cricket::ChannelManager* channel_manager,
      webrtc::RtcEventLog* event_log);

  virtual ~MediaControllerInterface() {}
  virtual void Close() = 0;
  virtual webrtc::Call* call_w() = 0;
  virtual cricket::ChannelManager* channel_manager() const = 0;
  virtual const cricket::MediaConfig& config() const = 0;
};
}  // namespace webrtc

#endif  // WEBRTC_PC_MEDIACONTROLLER_H_
