// Copyright 2022 Google LLC. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

package org.aomedia.avif.android;

import android.graphics.Bitmap;
import java.nio.ByteBuffer;

/** An AVIF Decoder. AVIF Specification: https://aomediacodec.github.io/av1-avif/. */
@SuppressWarnings("CatchAndPrintStackTrace")
public class AvifDecoder {
  static {
    try {
      System.loadLibrary("avif_android");
    } catch (UnsatisfiedLinkError exception) {
      exception.printStackTrace();
    }
  }

  // This is a utility class and cannot be instantiated.
  private AvifDecoder() {}

  /** Contains information about the AVIF Image. */
  public static class Info {
    public int width;
    public int height;
    public int depth;
  }

  /**
   * Returns true if the bytes in the buffer seem like an AVIF image.
   *
   * @param buffer The encoded image. buffer.position() must be 0.
   * @return true if the bytes seem like an AVIF image, false otherwise.
   */
  public static boolean isAvifImage(ByteBuffer buffer) {
    return AvifDecoder.isAvifImage(buffer, buffer.remaining());
  }

  private static native boolean isAvifImage(ByteBuffer encoded, int length);

  /**
   * Parses the AVIF header and populates the Info.
   *
   * @param encoded The encoded AVIF image. encoded.position() must be 0.
   * @param length Length of the encoded buffer.
   * @param info Output parameter whose fields will be populated.
   * @return true on success and false on failure.
   */
  public static native boolean getInfo(ByteBuffer encoded, int length, Info info);

  /**
   * Decodes the AVIF image into the bitmap.
   *
   * @param encoded The encoded AVIF image. encoded.position() must be 0.
   * @param length Length of the encoded buffer.
   * @param bitmap The decoded pixels will be copied into the bitmap.
   * @return true on success and false on failure. A few possible reasons for failure are: 1) Input
   *     was not valid AVIF. 2) Bitmap was not large enough to store the decoded image.
   */
  public static native boolean decode(ByteBuffer encoded, int length, Bitmap bitmap);
}
