/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains mock implementations of observers used in PeerConnection.

#ifndef WEBRTC_API_TEST_MOCKPEERCONNECTIONOBSERVERS_H_
#define WEBRTC_API_TEST_MOCKPEERCONNECTIONOBSERVERS_H_

#include <memory>
#include <string>

#include "webrtc/api/datachannelinterface.h"

namespace webrtc {

class MockCreateSessionDescriptionObserver
    : public webrtc::CreateSessionDescriptionObserver {
 public:
  MockCreateSessionDescriptionObserver()
      : called_(false),
        result_(false) {}
  virtual ~MockCreateSessionDescriptionObserver() {}
  virtual void OnSuccess(SessionDescriptionInterface* desc) {
    called_ = true;
    result_ = true;
    desc_.reset(desc);
  }
  virtual void OnFailure(const std::string& error) {
    called_ = true;
    result_ = false;
  }
  bool called() const { return called_; }
  bool result() const { return result_; }
  SessionDescriptionInterface* release_desc() {
    return desc_.release();
  }

 private:
  bool called_;
  bool result_;
  std::unique_ptr<SessionDescriptionInterface> desc_;
};

class MockSetSessionDescriptionObserver
    : public webrtc::SetSessionDescriptionObserver {
 public:
  MockSetSessionDescriptionObserver()
      : called_(false),
        result_(false) {}
  virtual ~MockSetSessionDescriptionObserver() {}
  virtual void OnSuccess() {
    called_ = true;
    result_ = true;
  }
  virtual void OnFailure(const std::string& error) {
    called_ = true;
    result_ = false;
  }
  bool called() const { return called_; }
  bool result() const { return result_; }

 private:
  bool called_;
  bool result_;
};

class MockDataChannelObserver : public webrtc::DataChannelObserver {
 public:
  explicit MockDataChannelObserver(webrtc::DataChannelInterface* channel)
      : channel_(channel) {
    channel_->RegisterObserver(this);
    state_ = channel_->state();
  }
  virtual ~MockDataChannelObserver() {
    channel_->UnregisterObserver();
  }

  void OnBufferedAmountChange(uint64_t previous_amount) override {}

  void OnStateChange() override { state_ = channel_->state(); }
  void OnMessage(const DataBuffer& buffer) override {
    messages_.push_back(
        std::string(buffer.data.data<char>(), buffer.data.size()));
  }

  bool IsOpen() const { return state_ == DataChannelInterface::kOpen; }
  std::vector<std::string> messages() const { return messages_; }
  std::string last_message() const {
    return messages_.empty() ? std::string() : messages_.back();
  }
  size_t received_message_count() const { return messages_.size(); }

 private:
  rtc::scoped_refptr<webrtc::DataChannelInterface> channel_;
  DataChannelInterface::DataState state_;
  std::vector<std::string> messages_;
};

class MockStatsObserver : public webrtc::StatsObserver {
 public:
  MockStatsObserver() : called_(false), stats_() {}
  virtual ~MockStatsObserver() {}

  virtual void OnComplete(const StatsReports& reports) {
    ASSERT(!called_);
    called_ = true;
    stats_.Clear();
    stats_.number_of_reports = reports.size();
    for (const auto* r : reports) {
      if (r->type() == StatsReport::kStatsReportTypeSsrc) {
        stats_.timestamp = r->timestamp();
        GetIntValue(r, StatsReport::kStatsValueNameAudioOutputLevel,
            &stats_.audio_output_level);
        GetIntValue(r, StatsReport::kStatsValueNameAudioInputLevel,
            &stats_.audio_input_level);
        GetIntValue(r, StatsReport::kStatsValueNameBytesReceived,
            &stats_.bytes_received);
        GetIntValue(r, StatsReport::kStatsValueNameBytesSent,
            &stats_.bytes_sent);
      } else if (r->type() == StatsReport::kStatsReportTypeBwe) {
        stats_.timestamp = r->timestamp();
        GetIntValue(r, StatsReport::kStatsValueNameAvailableReceiveBandwidth,
            &stats_.available_receive_bandwidth);
      } else if (r->type() == StatsReport::kStatsReportTypeComponent) {
        stats_.timestamp = r->timestamp();
        GetStringValue(r, StatsReport::kStatsValueNameDtlsCipher,
            &stats_.dtls_cipher);
        GetStringValue(r, StatsReport::kStatsValueNameSrtpCipher,
            &stats_.srtp_cipher);
      }
    }
  }

  bool called() const { return called_; }
  size_t number_of_reports() const { return stats_.number_of_reports; }
  double timestamp() const { return stats_.timestamp; }

  int AudioOutputLevel() const {
    ASSERT(called_);
    return stats_.audio_output_level;
  }

  int AudioInputLevel() const {
    ASSERT(called_);
    return stats_.audio_input_level;
  }

  int BytesReceived() const {
    ASSERT(called_);
    return stats_.bytes_received;
  }

  int BytesSent() const {
    ASSERT(called_);
    return stats_.bytes_sent;
  }

  int AvailableReceiveBandwidth() const {
    ASSERT(called_);
    return stats_.available_receive_bandwidth;
  }

  std::string DtlsCipher() const {
    ASSERT(called_);
    return stats_.dtls_cipher;
  }

  std::string SrtpCipher() const {
    ASSERT(called_);
    return stats_.srtp_cipher;
  }

 private:
  bool GetIntValue(const StatsReport* report,
                   StatsReport::StatsValueName name,
                   int* value) {
    const StatsReport::Value* v = report->FindValue(name);
    if (v) {
      // TODO(tommi): We should really just be using an int here :-/
      *value = rtc::FromString<int>(v->ToString());
    }
    return v != nullptr;
  }

  bool GetStringValue(const StatsReport* report,
                      StatsReport::StatsValueName name,
                      std::string* value) {
    const StatsReport::Value* v = report->FindValue(name);
    if (v)
      *value = v->ToString();
    return v != nullptr;
  }

  bool called_;
  struct {
    void Clear() {
      number_of_reports = 0;
      timestamp = 0;
      audio_output_level = 0;
      audio_input_level = 0;
      bytes_received = 0;
      bytes_sent = 0;
      available_receive_bandwidth = 0;
      dtls_cipher.clear();
      srtp_cipher.clear();
    }

    size_t number_of_reports;
    double timestamp;
    int audio_output_level;
    int audio_input_level;
    int bytes_received;
    int bytes_sent;
    int available_receive_bandwidth;
    std::string dtls_cipher;
    std::string srtp_cipher;
  } stats_;
};

}  // namespace webrtc

#endif  // WEBRTC_API_TEST_MOCKPEERCONNECTIONOBSERVERS_H_
