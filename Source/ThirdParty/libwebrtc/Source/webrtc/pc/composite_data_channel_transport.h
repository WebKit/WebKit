/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_COMPOSITE_DATA_CHANNEL_TRANSPORT_H_
#define PC_COMPOSITE_DATA_CHANNEL_TRANSPORT_H_

#include <vector>

#include "api/transport/data_channel_transport_interface.h"
#include "rtc_base/critical_section.h"

namespace webrtc {

// Composite implementation of DataChannelTransportInterface.  Allows users to
// receive data channel messages over multiple transports and send over one of
// those transports.
class CompositeDataChannelTransport : public DataChannelTransportInterface,
                                      public DataChannelSink {
 public:
  explicit CompositeDataChannelTransport(
      std::vector<DataChannelTransportInterface*> transports);
  ~CompositeDataChannelTransport() override;

  // Specifies which transport to be used for sending.  Must be called before
  // sending data.
  void SetSendTransport(DataChannelTransportInterface* send_transport);

  // Removes a given transport from the composite, if present.
  void RemoveTransport(DataChannelTransportInterface* transport);

  // DataChannelTransportInterface overrides.
  RTCError OpenChannel(int channel_id) override;
  RTCError SendData(int channel_id,
                    const SendDataParams& params,
                    const rtc::CopyOnWriteBuffer& buffer) override;
  RTCError CloseChannel(int channel_id) override;
  void SetDataSink(DataChannelSink* sink) override;
  bool IsReadyToSend() const override;

  // DataChannelSink overrides.
  void OnDataReceived(int channel_id,
                      DataMessageType type,
                      const rtc::CopyOnWriteBuffer& buffer) override;
  void OnChannelClosing(int channel_id) override;
  void OnChannelClosed(int channel_id) override;
  void OnReadyToSend() override;

 private:
  std::vector<DataChannelTransportInterface*> transports_;
  DataChannelTransportInterface* send_transport_ = nullptr;
  DataChannelSink* sink_ = nullptr;
};

}  // namespace webrtc

#endif  // PC_COMPOSITE_DATA_CHANNEL_TRANSPORT_H_
