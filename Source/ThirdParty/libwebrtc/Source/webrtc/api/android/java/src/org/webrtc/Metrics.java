/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import java.util.HashMap;
import java.util.Map;

// Java-side of androidmetrics_jni.cc.
//
// Rtc histograms can be queried through the API, getAndReset().
// The returned map holds the name of a histogram and its samples.
//
// Example of |map| with one histogram:
// |name|: "WebRTC.Video.InputFramesPerSecond"
//     |min|: 1
//     |max|: 100
//     |bucketCount|: 50
//     |samples|: [30]:1
//
// Most histograms are not updated frequently (e.g. most video metrics are an
// average over the call and recorded when a stream is removed).
// The metrics can for example be retrieved when a peer connection is closed.

public class Metrics {
  private static final String TAG = "Metrics";

  static {
    System.loadLibrary("jingle_peerconnection_so");
  }
  public final Map<String, HistogramInfo> map =
      new HashMap<String, HistogramInfo>(); // <name, HistogramInfo>

  /**
   * Class holding histogram information.
   */
  public static class HistogramInfo {
    public final int min;
    public final int max;
    public final int bucketCount;
    public final Map<Integer, Integer> samples =
        new HashMap<Integer, Integer>(); // <value, # of events>

    public HistogramInfo(int min, int max, int bucketCount) {
      this.min = min;
      this.max = max;
      this.bucketCount = bucketCount;
    }

    public void addSample(int value, int numEvents) {
      samples.put(value, numEvents);
    }
  }

  /**
   * Class for holding the native pointer of a histogram. Since there is no way to destroy a
   * histogram, please don't create unnecessary instances of this object. This class is thread safe.
   *
   * Usage example:
   * private static final Histogram someMetricHistogram =
   *     Histogram.createCounts("WebRTC.Video.SomeMetric", 1, 10000, 50);
   * someMetricHistogram.addSample(someVariable);
   */
  static class Histogram {
    private final long handle;
    private final String name; // Only used for logging.

    private Histogram(long handle, String name) {
      this.handle = handle;
      this.name = name;
    }

    static public Histogram createCounts(String name, int min, int max, int bucketCount) {
      return new Histogram(nativeCreateCounts(name, min, max, bucketCount), name);
    }

    static public Histogram createEnumeration(String name, int max) {
      return new Histogram(nativeCreateEnumeration(name, max), name);
    }

    public void addSample(int sample) {
      nativeAddSample(handle, sample);
    }

    private static native long nativeCreateCounts(String name, int min, int max, int bucketCount);
    private static native long nativeCreateEnumeration(String name, int max);
    private static native void nativeAddSample(long handle, int sample);
  }

  private void add(String name, HistogramInfo info) {
    map.put(name, info);
  }

  // Enables gathering of metrics (which can be fetched with getAndReset()).
  // Must be called before PeerConnectionFactory is created.
  public static void enable() {
    nativeEnable();
  }

  // Gets and clears native histograms.
  public static Metrics getAndReset() {
    return nativeGetAndReset();
  }

  private static native void nativeEnable();
  private static native Metrics nativeGetAndReset();
}
