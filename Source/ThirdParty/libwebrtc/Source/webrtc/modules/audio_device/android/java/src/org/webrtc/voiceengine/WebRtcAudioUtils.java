/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.voiceengine;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Process;
import java.lang.Thread;
import java.util.Arrays;
import java.util.List;
import org.webrtc.Logging;

public final class WebRtcAudioUtils {
  private static final String TAG = "WebRtcAudioUtils";

  // List of devices where we have seen issues (e.g. bad audio quality) using
  // the low latency output mode in combination with OpenSL ES.
  // The device name is given by Build.MODEL.
  private static final String[] BLACKLISTED_OPEN_SL_ES_MODELS = new String[] {
      // It is recommended to maintain a list of blacklisted models outside
      // this package and instead call
      // WebRtcAudioManager.setBlacklistDeviceForOpenSLESUsage(true)
      // from the client for devices where OpenSL ES shall be disabled.
  };

  // List of devices where it has been verified that the built-in effect
  // bad and where it makes sense to avoid using it and instead rely on the
  // native WebRTC version instead. The device name is given by Build.MODEL.
  private static final String[] BLACKLISTED_AEC_MODELS = new String[] {
      // It is recommended to maintain a list of blacklisted models outside
      // this package and instead call setWebRtcBasedAcousticEchoCanceler(true)
      // from the client for devices where the built-in AEC shall be disabled.
  };
  private static final String[] BLACKLISTED_NS_MODELS = new String[] {
    // It is recommended to maintain a list of blacklisted models outside
    // this package and instead call setWebRtcBasedNoiseSuppressor(true)
    // from the client for devices where the built-in NS shall be disabled.
  };

  // Use 16kHz as the default sample rate. A higher sample rate might prevent
  // us from supporting communication mode on some older (e.g. ICS) devices.
  private static final int DEFAULT_SAMPLE_RATE_HZ = 16000;
  private static int defaultSampleRateHz = DEFAULT_SAMPLE_RATE_HZ;
  // Set to true if setDefaultSampleRateHz() has been called.
  private static boolean isDefaultSampleRateOverridden = false;

  // By default, utilize hardware based audio effects for AEC and NS when
  // available.
  private static boolean useWebRtcBasedAcousticEchoCanceler = false;
  private static boolean useWebRtcBasedNoiseSuppressor = false;

  // Call these methods if any hardware based effect shall be replaced by a
  // software based version provided by the WebRTC stack instead.
  public static synchronized void setWebRtcBasedAcousticEchoCanceler(boolean enable) {
    useWebRtcBasedAcousticEchoCanceler = enable;
  }
  public static synchronized void setWebRtcBasedNoiseSuppressor(boolean enable) {
    useWebRtcBasedNoiseSuppressor = enable;
  }
  public static synchronized void setWebRtcBasedAutomaticGainControl(boolean enable) {
    // TODO(henrika): deprecated; remove when no longer used by any client.
    Logging.w(TAG, "setWebRtcBasedAutomaticGainControl() is deprecated");
  }

  public static synchronized boolean useWebRtcBasedAcousticEchoCanceler() {
    if (useWebRtcBasedAcousticEchoCanceler) {
      Logging.w(TAG, "Overriding default behavior; now using WebRTC AEC!");
    }
    return useWebRtcBasedAcousticEchoCanceler;
  }
  public static synchronized boolean useWebRtcBasedNoiseSuppressor() {
    if (useWebRtcBasedNoiseSuppressor) {
      Logging.w(TAG, "Overriding default behavior; now using WebRTC NS!");
    }
    return useWebRtcBasedNoiseSuppressor;
  }
  // TODO(henrika): deprecated; remove when no longer used by any client.
  public static synchronized boolean useWebRtcBasedAutomaticGainControl() {
    // Always return true here to avoid trying to use any built-in AGC.
    return true;
  }

  // Returns true if the device supports an audio effect (AEC or NS).
  // Four conditions must be fulfilled if functions are to return true:
  // 1) the platform must support the built-in (HW) effect,
  // 2) explicit use (override) of a WebRTC based version must not be set,
  // 3) the device must not be blacklisted for use of the effect, and
  // 4) the UUID of the effect must be approved (some UUIDs can be excluded).
  public static boolean isAcousticEchoCancelerSupported() {
    return WebRtcAudioEffects.canUseAcousticEchoCanceler();
  }
  public static boolean isNoiseSuppressorSupported() {
    return WebRtcAudioEffects.canUseNoiseSuppressor();
  }
  // TODO(henrika): deprecated; remove when no longer used by any client.
  public static boolean isAutomaticGainControlSupported() {
    // Always return false here to avoid trying to use any built-in AGC.
    return false;
  }

  // Call this method if the default handling of querying the native sample
  // rate shall be overridden. Can be useful on some devices where the
  // available Android APIs are known to return invalid results.
  public static synchronized void setDefaultSampleRateHz(int sampleRateHz) {
    isDefaultSampleRateOverridden = true;
    defaultSampleRateHz = sampleRateHz;
  }

  public static synchronized boolean isDefaultSampleRateOverridden() {
    return isDefaultSampleRateOverridden;
  }

  public static synchronized int getDefaultSampleRateHz() {
    return defaultSampleRateHz;
  }

  public static List<String> getBlackListedModelsForAecUsage() {
    return Arrays.asList(WebRtcAudioUtils.BLACKLISTED_AEC_MODELS);
  }

  public static List<String> getBlackListedModelsForNsUsage() {
    return Arrays.asList(WebRtcAudioUtils.BLACKLISTED_NS_MODELS);
  }

  public static boolean runningOnJellyBeanMR1OrHigher() {
    // November 2012: Android 4.2. API Level 17.
    return Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1;
  }

  public static boolean runningOnJellyBeanMR2OrHigher() {
    // July 24, 2013: Android 4.3. API Level 18.
    return Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2;
  }

  public static boolean runningOnLollipopOrHigher() {
    // API Level 21.
    return Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP;
  }

  public static boolean runningOnMarshmallowOrHigher() {
    // API Level 23.
    return Build.VERSION.SDK_INT >= Build.VERSION_CODES.M;
  }

  public static boolean runningOnNougatOrHigher() {
    // API Level 24.
    return Build.VERSION.SDK_INT >= Build.VERSION_CODES.N;
  }

  // Helper method for building a string of thread information.
  public static String getThreadInfo() {
    return "@[name=" + Thread.currentThread().getName() + ", id=" + Thread.currentThread().getId()
        + "]";
  }

  // Returns true if we're running on emulator.
  public static boolean runningOnEmulator() {
    return Build.HARDWARE.equals("goldfish") && Build.BRAND.startsWith("generic_");
  }

  // Returns true if the device is blacklisted for OpenSL ES usage.
  public static boolean deviceIsBlacklistedForOpenSLESUsage() {
    List<String> blackListedModels = Arrays.asList(BLACKLISTED_OPEN_SL_ES_MODELS);
    return blackListedModels.contains(Build.MODEL);
  }

  // Information about the current build, taken from system properties.
  public static void logDeviceInfo(String tag) {
    Logging.d(tag, "Android SDK: " + Build.VERSION.SDK_INT + ", "
            + "Release: " + Build.VERSION.RELEASE + ", "
            + "Brand: " + Build.BRAND + ", "
            + "Device: " + Build.DEVICE + ", "
            + "Id: " + Build.ID + ", "
            + "Hardware: " + Build.HARDWARE + ", "
            + "Manufacturer: " + Build.MANUFACTURER + ", "
            + "Model: " + Build.MODEL + ", "
            + "Product: " + Build.PRODUCT);
  }

  // Checks if the process has as specified permission or not.
  public static boolean hasPermission(Context context, String permission) {
    return context.checkPermission(permission, Process.myPid(), Process.myUid())
        == PackageManager.PERMISSION_GRANTED;
  }
}
