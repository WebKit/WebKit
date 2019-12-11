/* Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/data_channel_transport_interface.h"

namespace webrtc {

// TODO(mellem):  Delete these default implementations and make these functions
// pure virtual as soon as downstream implementations override them.

RTCError DataChannelTransportInterface::OpenChannel(int channel_id) {
  return RTCError(RTCErrorType::UNSUPPORTED_OPERATION);
}

RTCError DataChannelTransportInterface::SendData(
    int channel_id,
    const SendDataParams& params,
    const rtc::CopyOnWriteBuffer& buffer) {
  return RTCError(RTCErrorType::UNSUPPORTED_OPERATION);
}

RTCError DataChannelTransportInterface::CloseChannel(int channel_id) {
  return RTCError(RTCErrorType::UNSUPPORTED_OPERATION);
}

void DataChannelTransportInterface::SetDataSink(DataChannelSink* /*sink*/) {}

bool DataChannelTransportInterface::IsReadyToSend() const {
  return false;
}

void DataChannelSink::OnReadyToSend() {}

}  // namespace webrtc
