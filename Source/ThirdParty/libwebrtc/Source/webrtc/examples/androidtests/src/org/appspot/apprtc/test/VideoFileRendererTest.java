/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.lang.Thread;
import java.nio.charset.StandardCharsets;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Random;

public class VideoFileRendererTest extends InstrumentationTestCase {
  @SmallTest
  public void testYuvRenderingToFile() throws InterruptedException, IOException {
    EglBase eglBase = EglBase.create();
    final String videoOutPath = "/sdcard/chromium_tests_root/testvideoout.y4m";
    int frameWidth = 4;
    int frameHeight = 4;
    VideoFileRenderer videoFileRenderer =
        new VideoFileRenderer(videoOutPath, frameWidth, frameHeight, eglBase.getEglBaseContext());

    String[] frames = {
        "THIS IS JUST SOME TEXT x", "THE SECOND FRAME qwerty.", "HERE IS THE THRID FRAME!"};

    for (String frameStr : frames) {
      int[] planeSizes = {
          frameWidth * frameWidth, frameWidth * frameHeight / 4, frameWidth * frameHeight / 4};

      byte[] frameBytes = frameStr.getBytes(StandardCharsets.US_ASCII);
      ByteBuffer[] yuvPlanes = new ByteBuffer[3];
      int pos = 0;
      for (int i = 0; i < 3; i++) {
        yuvPlanes[i] = ByteBuffer.allocateDirect(planeSizes[i]);
        yuvPlanes[i].put(frameBytes, pos, planeSizes[i]);
        pos += planeSizes[i];
      }

      int[] yuvStrides = {frameWidth, frameWidth / 2, frameWidth / 2};

      VideoRenderer.I420Frame frame =
          new VideoRenderer.I420Frame(frameWidth, frameHeight, 0, yuvStrides, yuvPlanes, 0);

      videoFileRenderer.renderFrame(frame);
    }
    videoFileRenderer.release();

    RandomAccessFile writtenFile = new RandomAccessFile(videoOutPath, "r");
    try {
      int length = (int) writtenFile.length();
      byte[] data = new byte[length];
      writtenFile.readFully(data);
      String fileContent = new String(data, StandardCharsets.US_ASCII);
      String expected = "YUV4MPEG2 C420 W4 H4 Ip F30:1 A1:1\n"
          + "FRAME\n"
          + "THIS IS JUST SOME TEXT xFRAME\n"
          + "THE SECOND FRAME qwerty.FRAME\n"
          + "HERE IS THE THRID FRAME!";
      assertEquals(expected, fileContent);
    } finally {
      writtenFile.close();
    }

    new File(videoOutPath).delete();
  }
}
