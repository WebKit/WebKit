/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import <WebRTC/RTCMacros.h>

RTC_EXTERN void RTCSetupInternalTracer(void);
/** Starts capture to specified file. Must be a valid writable path.
 *  Returns YES if capture starts.
 */
RTC_EXTERN BOOL RTCStartInternalCapture(NSString* filePath);
RTC_EXTERN void RTCStopInternalCapture(void);
RTC_EXTERN void RTCShutdownInternalTracer(void);
