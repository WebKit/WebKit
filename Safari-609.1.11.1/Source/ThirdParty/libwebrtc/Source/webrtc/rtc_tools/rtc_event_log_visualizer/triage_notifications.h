/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_RTC_EVENT_LOG_VISUALIZER_TRIAGE_NOTIFICATIONS_H_
#define RTC_TOOLS_RTC_EVENT_LOG_VISUALIZER_TRIAGE_NOTIFICATIONS_H_

#include <string>

namespace webrtc {

class IncomingRtpReceiveTimeGap {
 public:
  IncomingRtpReceiveTimeGap(float time_seconds, int64_t duration)
      : time_seconds_(time_seconds), duration_(duration) {}
  float Time() const { return time_seconds_; }
  std::string ToString() const {
    return std::string("No RTP packets received for ") +
           std::to_string(duration_) + std::string(" ms");
  }

 private:
  float time_seconds_;
  int64_t duration_;
};

class IncomingRtcpReceiveTimeGap {
 public:
  IncomingRtcpReceiveTimeGap(float time_seconds, int64_t duration)
      : time_seconds_(time_seconds), duration_(duration) {}
  float Time() const { return time_seconds_; }
  std::string ToString() const {
    return std::string("No RTCP packets received for ") +
           std::to_string(duration_) + std::string(" ms");
  }

 private:
  float time_seconds_;
  int64_t duration_;
};

class OutgoingRtpSendTimeGap {
 public:
  OutgoingRtpSendTimeGap(float time_seconds, int64_t duration)
      : time_seconds_(time_seconds), duration_(duration) {}
  float Time() const { return time_seconds_; }
  std::string ToString() const {
    return std::string("No RTP packets sent for ") + std::to_string(duration_) +
           std::string(" ms");
  }

 private:
  float time_seconds_;
  int64_t duration_;
};

class OutgoingRtcpSendTimeGap {
 public:
  OutgoingRtcpSendTimeGap(float time_seconds, int64_t duration)
      : time_seconds_(time_seconds), duration_(duration) {}
  float Time() const { return time_seconds_; }
  std::string ToString() const {
    return std::string("No RTCP packets sent for ") +
           std::to_string(duration_) + std::string(" ms");
  }

 private:
  float time_seconds_;
  int64_t duration_;
};

class IncomingSeqNumJump {
 public:
  IncomingSeqNumJump(float time_seconds, uint32_t ssrc)
      : time_seconds_(time_seconds), ssrc_(ssrc) {}
  float Time() const { return time_seconds_; }
  std::string ToString() const {
    return std::string("Sequence number jumps on incoming SSRC ") +
           std::to_string(ssrc_);
  }

 private:
  float time_seconds_;

  uint32_t ssrc_;
};

class IncomingCaptureTimeJump {
 public:
  IncomingCaptureTimeJump(float time_seconds, uint32_t ssrc)
      : time_seconds_(time_seconds), ssrc_(ssrc) {}
  float Time() const { return time_seconds_; }
  std::string ToString() const {
    return std::string("Capture timestamp jumps on incoming SSRC ") +
           std::to_string(ssrc_);
  }

 private:
  float time_seconds_;

  uint32_t ssrc_;
};

class OutgoingSeqNoJump {
 public:
  OutgoingSeqNoJump(float time_seconds, uint32_t ssrc)
      : time_seconds_(time_seconds), ssrc_(ssrc) {}
  float Time() const { return time_seconds_; }
  std::string ToString() const {
    return std::string("Sequence number jumps on outgoing SSRC ") +
           std::to_string(ssrc_);
  }

 private:
  float time_seconds_;

  uint32_t ssrc_;
};

class OutgoingCaptureTimeJump {
 public:
  OutgoingCaptureTimeJump(float time_seconds, uint32_t ssrc)
      : time_seconds_(time_seconds), ssrc_(ssrc) {}
  float Time() const { return time_seconds_; }
  std::string ToString() const {
    return std::string("Capture timestamp jumps on outgoing SSRC ") +
           std::to_string(ssrc_);
  }

 private:
  float time_seconds_;

  uint32_t ssrc_;
};

class OutgoingHighLoss {
 public:
  explicit OutgoingHighLoss(double avg_loss_fraction)
      : avg_loss_fraction_(avg_loss_fraction) {}
  std::string ToString() const {
    return std::string("High average loss (") +
           std::to_string(avg_loss_fraction_ * 100) +
           std::string("%) across the call.");
  }

 private:
  double avg_loss_fraction_;
};

}  // namespace webrtc

#endif  // RTC_TOOLS_RTC_EVENT_LOG_VISUALIZER_TRIAGE_NOTIFICATIONS_H_
