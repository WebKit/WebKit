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

import static org.junit.Assert.assertEquals;

import android.os.Environment;
import android.support.test.filters.SmallTest;
import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(BaseJUnit4ClassRunner.class)
public class VideoFileRendererTest {
  @Before
  public void setUp() {
    NativeLibrary.initialize(new NativeLibrary.DefaultLoader(), TestConstants.NATIVE_LIBRARY);
  }

  @Test
  @SmallTest
  public void testYuvRenderingToFile() throws InterruptedException, IOException {
    EglBase eglBase = EglBase.create();
    final String videoOutPath = Environment.getExternalStorageDirectory().getPath()
        + "/chromium_tests_root/testvideoout.y4m";
    int frameWidth = 4;
    int frameHeight = 4;
    VideoFileRenderer videoFileRenderer =
        new VideoFileRenderer(videoOutPath, frameWidth, frameHeight, eglBase.getEglBaseContext());

    String[] frames = {
        "THIS IS JUST SOME TEXT x", "THE SECOND FRAME qwerty.", "HERE IS THE THRID FRAME!"};

    for (String frameStr : frames) {
      int[] planeSizes = {
          frameWidth * frameWidth, frameWidth * frameHeight / 4, frameWidth * frameHeight / 4};
      int[] yuvStrides = {frameWidth, frameWidth / 2, frameWidth / 2};

      ByteBuffer[] yuvPlanes = new ByteBuffer[3];
      byte[] frameBytes = frameStr.getBytes(Charset.forName("US-ASCII"));
      int pos = 0;
      for (int i = 0; i < 3; i++) {
        yuvPlanes[i] = ByteBuffer.allocateDirect(planeSizes[i]);
        yuvPlanes[i].put(frameBytes, pos, planeSizes[i]);
        yuvPlanes[i].rewind();
        pos += planeSizes[i];
      }

      VideoFrame.I420Buffer buffer =
          JavaI420Buffer.wrap(frameWidth, frameHeight, yuvPlanes[0], yuvStrides[0], yuvPlanes[1],
              yuvStrides[1], yuvPlanes[2], yuvStrides[2], null /* releaseCallback */);

      VideoFrame frame = new VideoFrame(buffer, 0 /* rotation */, 0 /* timestampNs */);
      videoFileRenderer.onFrame(frame);
      frame.release();
    }
    videoFileRenderer.release();

    RandomAccessFile writtenFile = new RandomAccessFile(videoOutPath, "r");
    try {
      int length = (int) writtenFile.length();
      byte[] data = new byte[length];
      writtenFile.readFully(data);
      String fileContent = new String(data, Charset.forName("US-ASCII"));
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
