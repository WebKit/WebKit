/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCSSLAdapter.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/ssladapter.h"

BOOL RTCInitializeSSL() {
  BOOL initialized = rtc::InitializeSSL();
  RTC_DCHECK(initialized);
  return initialized;
}

BOOL RTCCleanupSSL() {
  BOOL cleanedUp = rtc::CleanupSSL();
  RTC_DCHECK(cleanedUp);
  return cleanedUp;
}
