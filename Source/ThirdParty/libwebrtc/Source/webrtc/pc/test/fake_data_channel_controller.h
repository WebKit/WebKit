/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_FAKE_DATA_CHANNEL_CONTROLLER_H_
#define PC_TEST_FAKE_DATA_CHANNEL_CONTROLLER_H_

#include <set>

#include "pc/sctp_data_channel.h"
#include "rtc_base/checks.h"

class FakeDataChannelController
    : public webrtc::SctpDataChannelControllerInterface {
 public:
  FakeDataChannelController()
      : send_blocked_(false),
        transport_available_(false),
        ready_to_send_(false),
        transport_error_(false) {}
  virtual ~FakeDataChannelController() {}

  bool SendData(int sid,
                const webrtc::SendDataParams& params,
                const rtc::CopyOnWriteBuffer& payload,
                cricket::SendDataResult* result) override {
    RTC_CHECK(ready_to_send_);
    RTC_CHECK(transport_available_);
    if (send_blocked_) {
      *result = cricket::SDR_BLOCK;
      return false;
    }

    if (transport_error_) {
      *result = cricket::SDR_ERROR;
      return false;
    }

    last_sid_ = sid;
    last_send_data_params_ = params;
    return true;
  }

  bool ConnectDataChannel(webrtc::SctpDataChannel* data_channel) override {
    RTC_CHECK(connected_channels_.find(data_channel) ==
              connected_channels_.end());
    if (!transport_available_) {
      return false;
    }
    RTC_LOG(LS_INFO) << "DataChannel connected " << data_channel;
    connected_channels_.insert(data_channel);
    return true;
  }

  void DisconnectDataChannel(webrtc::SctpDataChannel* data_channel) override {
    RTC_CHECK(connected_channels_.find(data_channel) !=
              connected_channels_.end());
    RTC_LOG(LS_INFO) << "DataChannel disconnected " << data_channel;
    connected_channels_.erase(data_channel);
  }

  void AddSctpDataStream(int sid) override {
    RTC_CHECK(sid >= 0);
    if (!transport_available_) {
      return;
    }
    send_ssrcs_.insert(sid);
    recv_ssrcs_.insert(sid);
  }

  void RemoveSctpDataStream(int sid) override {
    RTC_CHECK(sid >= 0);
    send_ssrcs_.erase(sid);
    recv_ssrcs_.erase(sid);
    // Unlike the real SCTP transport, act like the closing procedure finished
    // instantly, doing the same snapshot thing as below.
    for (webrtc::SctpDataChannel* ch : std::set<webrtc::SctpDataChannel*>(
             connected_channels_.begin(), connected_channels_.end())) {
      if (connected_channels_.count(ch)) {
        ch->OnClosingProcedureComplete(sid);
      }
    }
  }

  bool ReadyToSendData() const override { return ready_to_send_; }

  // Set true to emulate the SCTP stream being blocked by congestion control.
  void set_send_blocked(bool blocked) {
    send_blocked_ = blocked;
    if (!blocked) {
      // Take a snapshot of the connected channels and check to see whether
      // each value is still in connected_channels_ before calling
      // OnTransportReady().  This avoids problems where the set gets modified
      // in response to OnTransportReady().
      for (webrtc::SctpDataChannel* ch : std::set<webrtc::SctpDataChannel*>(
               connected_channels_.begin(), connected_channels_.end())) {
        if (connected_channels_.count(ch)) {
          ch->OnTransportReady(true);
        }
      }
    }
  }

  // Set true to emulate the transport channel creation, e.g. after
  // setLocalDescription/setRemoteDescription called with data content.
  void set_transport_available(bool available) {
    transport_available_ = available;
  }

  // Set true to emulate the transport ReadyToSendData signal when the transport
  // becomes writable for the first time.
  void set_ready_to_send(bool ready) {
    RTC_CHECK(transport_available_);
    ready_to_send_ = ready;
    if (ready) {
      std::set<webrtc::SctpDataChannel*>::iterator it;
      for (it = connected_channels_.begin(); it != connected_channels_.end();
           ++it) {
        (*it)->OnTransportReady(true);
      }
    }
  }

  void set_transport_error() { transport_error_ = true; }

  int last_sid() const { return last_sid_; }
  const webrtc::SendDataParams& last_send_data_params() const {
    return last_send_data_params_;
  }

  bool IsConnected(webrtc::SctpDataChannel* data_channel) const {
    return connected_channels_.find(data_channel) != connected_channels_.end();
  }

  bool IsSendStreamAdded(uint32_t stream) const {
    return send_ssrcs_.find(stream) != send_ssrcs_.end();
  }

  bool IsRecvStreamAdded(uint32_t stream) const {
    return recv_ssrcs_.find(stream) != recv_ssrcs_.end();
  }

 private:
  int last_sid_;
  webrtc::SendDataParams last_send_data_params_;
  bool send_blocked_;
  bool transport_available_;
  bool ready_to_send_;
  bool transport_error_;
  std::set<webrtc::SctpDataChannel*> connected_channels_;
  std::set<uint32_t> send_ssrcs_;
  std::set<uint32_t> recv_ssrcs_;
};
#endif  // PC_TEST_FAKE_DATA_CHANNEL_CONTROLLER_H_
