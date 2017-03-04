/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_PC_FAKEMEDIACONTROLLER_H_
#define WEBRTC_PC_FAKEMEDIACONTROLLER_H_

#include "webrtc/base/checks.h"
#include "webrtc/media/base/mediachannel.h"
#include "webrtc/pc/mediacontroller.h"

namespace cricket {

class FakeMediaController : public webrtc::MediaControllerInterface {
 public:
  explicit FakeMediaController(cricket::ChannelManager* channel_manager,
                               webrtc::Call* call)
      : channel_manager_(channel_manager), call_(call) {
    RTC_DCHECK(nullptr != channel_manager_);
    RTC_DCHECK(nullptr != call_);
  }
  ~FakeMediaController() override {}
  void Close() override {}
  webrtc::Call* call_w() override { return call_; }
  cricket::ChannelManager* channel_manager() const override {
    return channel_manager_;
  }
  const MediaConfig& config() const override { return media_config_; }

 private:
  const MediaConfig media_config_ = MediaConfig();
  cricket::ChannelManager* channel_manager_;
  webrtc::Call* call_;
};
}  // namespace cricket
#endif  // WEBRTC_PC_FAKEMEDIACONTROLLER_H_
