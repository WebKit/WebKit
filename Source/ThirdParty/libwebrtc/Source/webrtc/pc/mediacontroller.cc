/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/pc/mediacontroller.h"

#include <memory>

#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/call/call.h"
#include "webrtc/pc/channelmanager.h"
#include "webrtc/media/base/mediachannel.h"

namespace {

const int kMinBandwidthBps = 30000;
const int kStartBandwidthBps = 300000;
const int kMaxBandwidthBps = 2000000;

class MediaController : public webrtc::MediaControllerInterface,
                        public sigslot::has_slots<> {
 public:
  MediaController(const cricket::MediaConfig& media_config,
                  rtc::Thread* worker_thread,
                  cricket::ChannelManager* channel_manager,
                  webrtc::RtcEventLog* event_log)
      : worker_thread_(worker_thread),
        media_config_(media_config),
        channel_manager_(channel_manager),
        call_config_(event_log) {
    RTC_DCHECK(worker_thread);
    RTC_DCHECK(event_log);
    worker_thread_->Invoke<void>(RTC_FROM_HERE,
                                 rtc::Bind(&MediaController::Construct_w, this,
                                           channel_manager_->media_engine()));
  }
  ~MediaController() override {
    Close();
  }

  // webrtc::MediaControllerInterface implementation.
  void Close() override {
    worker_thread_->Invoke<void>(RTC_FROM_HERE,
                                 rtc::Bind(&MediaController::Close_w, this));
  }
  webrtc::Call* call_w() override {
    RTC_DCHECK(worker_thread_->IsCurrent());
    if (!call_) {
      call_.reset(webrtc::Call::Create(call_config_));
    }
    return call_.get();
  }
  cricket::ChannelManager* channel_manager() const override {
    return channel_manager_;
  }
  const cricket::MediaConfig& config() const override { return media_config_; }

 private:
  void Construct_w(cricket::MediaEngineInterface* media_engine) {
    RTC_DCHECK(worker_thread_->IsCurrent());
    RTC_DCHECK(media_engine);
    call_config_.audio_state = media_engine->GetAudioState();
    call_config_.bitrate_config.min_bitrate_bps = kMinBandwidthBps;
    call_config_.bitrate_config.start_bitrate_bps = kStartBandwidthBps;
    call_config_.bitrate_config.max_bitrate_bps = kMaxBandwidthBps;
  }
  void Close_w() {
    RTC_DCHECK(worker_thread_->IsCurrent());
    call_.reset();
  }

  rtc::Thread* const worker_thread_;
  const cricket::MediaConfig media_config_;
  cricket::ChannelManager* const channel_manager_;
  webrtc::Call::Config call_config_;
  std::unique_ptr<webrtc::Call> call_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(MediaController);
};
}  // namespace {

namespace webrtc {

MediaControllerInterface* MediaControllerInterface::Create(
    const cricket::MediaConfig& config,
    rtc::Thread* worker_thread,
    cricket::ChannelManager* channel_manager,
    webrtc::RtcEventLog* event_log) {
  return new MediaController(config, worker_thread, channel_manager, event_log);
}
}  // namespace webrtc
