/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_SRTP_TEST_UTIL_H_
#define PC_TEST_SRTP_TEST_UTIL_H_

#include <cstdint>

#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/ssl_stream_adapter.h"

namespace webrtc {

static const ZeroOnFreeBuffer<uint8_t> kTestKey1{
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234", 30};
static const ZeroOnFreeBuffer<uint8_t> kTestKey2{
    "4321ZYXWVUTSRQPONMLKJIHGFEDCBA", 30};

static int rtp_auth_tag_len(int crypto_suite) {
  switch (crypto_suite) {
    case webrtc::kSrtpAes128CmSha1_32:
      return 4;
    case webrtc::kSrtpAes128CmSha1_80:
      return 10;
    case webrtc::kSrtpAeadAes128Gcm:
    case webrtc::kSrtpAeadAes256Gcm:
      return 16;
    default:
      RTC_CHECK_NOTREACHED();
  }
}

static int rtcp_auth_tag_len(int crypto_suite) {
  switch (crypto_suite) {
    case webrtc::kSrtpAes128CmSha1_32:
    case webrtc::kSrtpAes128CmSha1_80:
      return 10;
    case webrtc::kSrtpAeadAes128Gcm:
    case webrtc::kSrtpAeadAes256Gcm:
      return 16;
    default:
      RTC_CHECK_NOTREACHED();
  }
}

}  //  namespace webrtc


#endif  // PC_TEST_SRTP_TEST_UTIL_H_
