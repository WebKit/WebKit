/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_MD5DIGEST_H_
#define RTC_BASE_MD5DIGEST_H_

#include "rtc_base/md5.h"
#include "rtc_base/messagedigest.h"

namespace rtc {

// A simple wrapper for our MD5 implementation.
class Md5Digest : public MessageDigest {
 public:
  enum { kSize = 16 };
  Md5Digest() {
    MD5Init(&ctx_);
  }
  size_t Size() const override;
  void Update(const void* buf, size_t len) override;
  size_t Finish(void* buf, size_t len) override;

 private:
  MD5Context ctx_;
};

}  // namespace rtc

#endif  // RTC_BASE_MD5DIGEST_H_
