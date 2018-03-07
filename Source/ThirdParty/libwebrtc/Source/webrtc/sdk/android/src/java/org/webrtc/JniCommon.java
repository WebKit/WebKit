/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import java.nio.ByteBuffer;

/** Class with static JNI helper functions that are used in many places. */
class JniCommon {
  /** Functions to increment/decrement an rtc::RefCountInterface pointer. */
  static native void nativeAddRef(long nativeRefCountedPointer);
  static native void nativeReleaseRef(long nativeRefCountedPointer);

  public static native ByteBuffer allocateNativeByteBuffer(int size);
  public static native void freeNativeByteBuffer(ByteBuffer buffer);
}
