/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import java.util.concurrent.atomic.AtomicInteger;
import javax.annotation.Nullable;

/**
 * Implementation of RefCounted that executes a Runnable once the ref count reaches zero.
 */
class RefCountDelegate implements RefCounted {
  private final AtomicInteger refCount = new AtomicInteger(1);
  private final @Nullable Runnable releaseCallback;

  /**
   * @param releaseCallback Callback that will be executed once the ref count reaches zero.
   */
  public RefCountDelegate(@Nullable Runnable releaseCallback) {
    this.releaseCallback = releaseCallback;
  }

  @Override
  public void retain() {
    refCount.incrementAndGet();
  }

  @Override
  public void release() {
    if (refCount.decrementAndGet() == 0 && releaseCallback != null) {
      releaseCallback.run();
    }
  }
}
