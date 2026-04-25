/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCTracing.h"

#include "rtc_base/event_tracer.h"

void RTCSetupInternalTracer(void) {
  rtc::tracing::SetupInternalTracer();
}

BOOL RTCStartInternalCapture(NSString *filePath) {
  return rtc::tracing::StartInternalCapture(filePath.UTF8String);
}

void RTCStopInternalCapture(void) {
  rtc::tracing::StopInternalCapture();
}

void RTCShutdownInternalTracer(void) {
  rtc::tracing::ShutdownInternalTracer();
}
