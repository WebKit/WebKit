/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_SCTPUTILS_H_
#define WEBRTC_API_SCTPUTILS_H_

#include <string>

#include "webrtc/api/datachannelinterface.h"

namespace rtc {
class CopyOnWriteBuffer;
}  // namespace rtc

namespace webrtc {
struct DataChannelInit;

// Read the message type and return true if it's an OPEN message.
bool IsOpenMessage(const rtc::CopyOnWriteBuffer& payload);

bool ParseDataChannelOpenMessage(const rtc::CopyOnWriteBuffer& payload,
                                 std::string* label,
                                 DataChannelInit* config);

bool ParseDataChannelOpenAckMessage(const rtc::CopyOnWriteBuffer& payload);

bool WriteDataChannelOpenMessage(const std::string& label,
                                 const DataChannelInit& config,
                                 rtc::CopyOnWriteBuffer* payload);

void WriteDataChannelOpenAckMessage(rtc::CopyOnWriteBuffer* payload);
}  // namespace webrtc

#endif  // WEBRTC_API_SCTPUTILS_H_
