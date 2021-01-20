/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/rtp_header_parser.h"

#include <memory>

#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class RtpHeaderParserImpl : public RtpHeaderParser {
 public:
  RtpHeaderParserImpl();
  ~RtpHeaderParserImpl() override = default;

  bool Parse(const uint8_t* packet,
             size_t length,
             RTPHeader* header) const override;

  bool RegisterRtpHeaderExtension(RTPExtensionType type, uint8_t id) override;
  bool RegisterRtpHeaderExtension(RtpExtension extension) override;

  bool DeregisterRtpHeaderExtension(RTPExtensionType type) override;
  bool DeregisterRtpHeaderExtension(RtpExtension extension) override;

 private:
  mutable Mutex mutex_;
  RtpHeaderExtensionMap rtp_header_extension_map_ RTC_GUARDED_BY(mutex_);
};

std::unique_ptr<RtpHeaderParser> RtpHeaderParser::CreateForTest() {
  return std::make_unique<RtpHeaderParserImpl>();
}

RtpHeaderParserImpl::RtpHeaderParserImpl() {}

bool RtpHeaderParser::IsRtcp(const uint8_t* packet, size_t length) {
  RtpUtility::RtpHeaderParser rtp_parser(packet, length);
  return rtp_parser.RTCP();
}

absl::optional<uint32_t> RtpHeaderParser::GetSsrc(const uint8_t* packet,
                                                  size_t length) {
  RtpUtility::RtpHeaderParser rtp_parser(packet, length);
  RTPHeader header;
  if (rtp_parser.Parse(&header, nullptr)) {
    return header.ssrc;
  }
  return absl::nullopt;
}

bool RtpHeaderParserImpl::Parse(const uint8_t* packet,
                                size_t length,
                                RTPHeader* header) const {
  RtpUtility::RtpHeaderParser rtp_parser(packet, length);
  *header = RTPHeader();

  RtpHeaderExtensionMap map;
  {
    MutexLock lock(&mutex_);
    map = rtp_header_extension_map_;
  }

  const bool valid_rtpheader = rtp_parser.Parse(header, &map);
  if (!valid_rtpheader) {
    return false;
  }
  return true;
}
bool RtpHeaderParserImpl::RegisterRtpHeaderExtension(RtpExtension extension) {
  MutexLock lock(&mutex_);
  return rtp_header_extension_map_.RegisterByUri(extension.id, extension.uri);
}

bool RtpHeaderParserImpl::RegisterRtpHeaderExtension(RTPExtensionType type,
                                                     uint8_t id) {
  MutexLock lock(&mutex_);
  return rtp_header_extension_map_.RegisterByType(id, type);
}

bool RtpHeaderParserImpl::DeregisterRtpHeaderExtension(RtpExtension extension) {
  MutexLock lock(&mutex_);
  return rtp_header_extension_map_.Deregister(
      rtp_header_extension_map_.GetType(extension.id));
}

bool RtpHeaderParserImpl::DeregisterRtpHeaderExtension(RTPExtensionType type) {
  MutexLock lock(&mutex_);
  return rtp_header_extension_map_.Deregister(type) == 0;
}
}  // namespace webrtc
