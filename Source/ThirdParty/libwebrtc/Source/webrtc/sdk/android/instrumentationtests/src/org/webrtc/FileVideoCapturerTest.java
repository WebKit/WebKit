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
import static org.junit.Assert.assertTrue;

import android.os.Environment;
import android.support.test.filters.LargeTest;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import java.io.IOException;
import java.lang.Thread;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.Arrays;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(BaseJUnit4ClassRunner.class)
public class FileVideoCapturerTest {
  private static class Frame {
    public byte[] data;
    public int width;
    public int height;
  }

  public class MockCapturerObserver implements VideoCapturer.CapturerObserver {
    private final ArrayList<Frame> frameDatas = new ArrayList<Frame>();

    @Override
    public void onCapturerStarted(boolean success) {
      assertTrue(success);
    }

    @Override
    public void onCapturerStopped() {
      // Empty on purpose.
    }

    @Override
    public synchronized void onByteBufferFrameCaptured(
        byte[] data, int width, int height, int rotation, long timeStamp) {
      Frame frame = new Frame();
      frame.data = data;
      frame.width = width;
      frame.height = height;
      assertTrue(data.length != 0);
      frameDatas.add(frame);
      notify();
    }

    @Override
    public void onTextureFrameCaptured(int width, int height, int oesTextureId,
        float[] transformMatrix, int rotation, long timestamp) {
      // Empty on purpose.
    }

    public synchronized ArrayList<Frame> getMinimumFramesBlocking(int minFrames)
        throws InterruptedException {
      while (frameDatas.size() < minFrames) {
        wait();
      }
      return new ArrayList<Frame>(frameDatas);
    }
  }

  @Test
  @SmallTest
  public void testVideoCaptureFromFile() throws InterruptedException, IOException {
    final int FRAME_WIDTH = 4;
    final int FRAME_HEIGHT = 4;
    final FileVideoCapturer fileVideoCapturer =
        new FileVideoCapturer(Environment.getExternalStorageDirectory().getPath()
            + "/chromium_tests_root/webrtc/sdk/android/instrumentationtests/src/org/webrtc/"
            + "capturetestvideo.y4m");
    final MockCapturerObserver capturerObserver = new MockCapturerObserver();
    fileVideoCapturer.initialize(null, null, capturerObserver);
    fileVideoCapturer.startCapture(FRAME_WIDTH, FRAME_HEIGHT, 33);

    final String[] expectedFrames = {
        "THIS IS JUST SOME TEXT x", "THE SECOND FRAME qwerty.", "HERE IS THE THRID FRAME!"};

    final ArrayList<Frame> frameDatas;
    frameDatas = capturerObserver.getMinimumFramesBlocking(expectedFrames.length);

    assertEquals(expectedFrames.length, frameDatas.size());

    fileVideoCapturer.stopCapture();
    fileVideoCapturer.dispose();

    for (int i = 0; i < expectedFrames.length; ++i) {
      Frame frame = frameDatas.get(i);

      assertEquals(FRAME_WIDTH, frame.width);
      assertEquals(FRAME_HEIGHT, frame.height);
      assertEquals(FRAME_WIDTH * FRAME_HEIGHT * 3 / 2, frame.data.length);

      byte[] expectedNV12Bytes = new byte[frame.data.length];
      FileVideoCapturer.nativeI420ToNV21(expectedFrames[i].getBytes(Charset.forName("US-ASCII")),
          FRAME_WIDTH, FRAME_HEIGHT, expectedNV12Bytes);

      assertTrue(Arrays.equals(expectedNV12Bytes, frame.data));
    }
  }
}
