/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.os.Build;
import org.webrtc.CalledByNative;

public final class BuildInfo {
  public static String getDevice() {
    return Build.DEVICE;
  }

  @CalledByNative
  public static String getDeviceModel() {
    return Build.MODEL;
  }

  public static String getProduct() {
    return Build.PRODUCT;
  }

  @CalledByNative
  public static String getBrand() {
    return Build.BRAND;
  }

  @CalledByNative
  public static String getDeviceManufacturer() {
    return Build.MANUFACTURER;
  }

  @CalledByNative
  public static String getAndroidBuildId() {
    return Build.ID;
  }

  @CalledByNative
  public static String getBuildType() {
    return Build.TYPE;
  }

  @CalledByNative
  public static String getBuildRelease() {
    return Build.VERSION.RELEASE;
  }

  @CalledByNative
  public static int getSdkVersion() {
    return Build.VERSION.SDK_INT;
  }
}
