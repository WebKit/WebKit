/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import <WebRTC/RTCMacros.h>

/**
 * Initialize and clean up the SSL library. Failure is fatal. These call the
 * corresponding functions in webrtc/base/ssladapter.h.
 */
RTC_EXTERN BOOL RTCInitializeSSL();
RTC_EXTERN BOOL RTCCleanupSSL();
