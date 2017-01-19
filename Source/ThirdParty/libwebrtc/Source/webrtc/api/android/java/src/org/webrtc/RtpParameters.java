/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import java.util.LinkedList;

/**
 * The parameters for an {@code RtpSender}, as defined in
 * http://w3c.github.io/webrtc-pc/#rtcrtpsender-interface.
 */
public class RtpParameters {
  public static class Encoding {
    public boolean active = true;
    // A null value means "no maximum bitrate".
    public Integer maxBitrateBps;
  }

  public static class Codec {
    int payloadType;
    String mimeType;
    int clockRate;
    int channels = 1;
  }

  public final LinkedList<Encoding> encodings;
  public final LinkedList<Codec> codecs;

  public RtpParameters() {
    encodings = new LinkedList<Encoding>();
    codecs = new LinkedList<Codec>();
  }
}
