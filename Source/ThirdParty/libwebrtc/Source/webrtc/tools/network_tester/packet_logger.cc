/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/tools/network_tester/packet_logger.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/protobuf_utils.h"

namespace webrtc {

PacketLogger::PacketLogger(const std::string& log_file_path)
    : packet_logger_stream_(log_file_path,
                            std::ios_base::out | std::ios_base::binary) {
  RTC_DCHECK(packet_logger_stream_.is_open());
  RTC_DCHECK(packet_logger_stream_.good());
}

PacketLogger::~PacketLogger() = default;

void PacketLogger::LogPacket(const NetworkTesterPacket& packet) {
  // The protobuffer message will be saved in the following format to the file:
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |      1 byte      |  X byte |      1 byte      | ... |  X byte |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // | Size of the next | proto   | Size of the next | ... | proto   |
  // | proto message    | message | proto message    |     | message |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  ProtoString packet_data;
  packet.SerializeToString(&packet_data);
  RTC_DCHECK_LE(packet_data.length(), 255);
  RTC_DCHECK_GE(packet_data.length(), 0);
  char proto_size = packet_data.length();
  packet_logger_stream_.write(&proto_size, sizeof(proto_size));
  packet_logger_stream_.write(packet_data.data(), packet_data.length());
}

}  // namespace webrtc
