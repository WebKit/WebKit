/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.appspot.apprtc;

import android.widget.SeekBar;
import android.widget.TextView;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import org.webrtc.CameraEnumerationAndroid.CaptureFormat;

/**
 * Control capture format based on a seekbar listener.
 */
public class CaptureQualityController implements SeekBar.OnSeekBarChangeListener {
  private final List<CaptureFormat> formats =
      Arrays.asList(new CaptureFormat(1280, 720, 0, 30000), new CaptureFormat(960, 540, 0, 30000),
          new CaptureFormat(640, 480, 0, 30000), new CaptureFormat(480, 360, 0, 30000),
          new CaptureFormat(320, 240, 0, 30000), new CaptureFormat(256, 144, 0, 30000));
  // Prioritize framerate below this threshold and resolution above the threshold.
  private static final int FRAMERATE_THRESHOLD = 15;
  private TextView captureFormatText;
  private CallFragment.OnCallEvents callEvents;
  private int width = 0;
  private int height = 0;
  private int framerate = 0;
  private double targetBandwidth = 0;

  public CaptureQualityController(
      TextView captureFormatText, CallFragment.OnCallEvents callEvents) {
    this.captureFormatText = captureFormatText;
    this.callEvents = callEvents;
  }

  private final Comparator<CaptureFormat> compareFormats = new Comparator<CaptureFormat>() {
    @Override
    public int compare(CaptureFormat first, CaptureFormat second) {
      int firstFps = calculateFramerate(targetBandwidth, first);
      int secondFps = calculateFramerate(targetBandwidth, second);

      if (firstFps >= FRAMERATE_THRESHOLD && secondFps >= FRAMERATE_THRESHOLD
          || firstFps == secondFps) {
        // Compare resolution.
        return first.width * first.height - second.width * second.height;
      } else {
        // Compare fps.
        return firstFps - secondFps;
      }
    }
  };

  @Override
  public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
    if (progress == 0) {
      width = 0;
      height = 0;
      framerate = 0;
      captureFormatText.setText(R.string.muted);
      return;
    }

    // Extract max bandwidth (in millipixels / second).
    long maxCaptureBandwidth = java.lang.Long.MIN_VALUE;
    for (CaptureFormat format : formats) {
      maxCaptureBandwidth =
          Math.max(maxCaptureBandwidth, (long) format.width * format.height * format.framerate.max);
    }

    // Fraction between 0 and 1.
    double bandwidthFraction = (double) progress / 100.0;
    // Make a log-scale transformation, still between 0 and 1.
    final double kExpConstant = 3.0;
    bandwidthFraction =
        (Math.exp(kExpConstant * bandwidthFraction) - 1) / (Math.exp(kExpConstant) - 1);
    targetBandwidth = bandwidthFraction * maxCaptureBandwidth;

    // Choose the best format given a target bandwidth.
    final CaptureFormat bestFormat = Collections.max(formats, compareFormats);
    width = bestFormat.width;
    height = bestFormat.height;
    framerate = calculateFramerate(targetBandwidth, bestFormat);
    captureFormatText.setText(
        String.format(captureFormatText.getContext().getString(R.string.format_description), width,
            height, framerate));
  }

  @Override
  public void onStartTrackingTouch(SeekBar seekBar) {}

  @Override
  public void onStopTrackingTouch(SeekBar seekBar) {
    callEvents.onCaptureFormatChange(width, height, framerate);
  }

  // Return the highest frame rate possible based on bandwidth and format.
  private int calculateFramerate(double bandwidth, CaptureFormat format) {
    return (int) Math.round(
        Math.min(format.framerate.max, (int) Math.round(bandwidth / (format.width * format.height)))
        / 1000.0);
  }
}
