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
import androidx.test.filters.SmallTest;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.util.ArrayList;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(BaseJUnit4ClassRunner.class)
public class FileVideoCapturerTest {
  public static class MockCapturerObserver implements CapturerObserver {
    private final ArrayList<VideoFrame> frames = new ArrayList<VideoFrame>();

    @Override
    public void onCapturerStarted(boolean success) {
      assertTrue(success);
    }

    @Override
    public void onCapturerStopped() {
      // Empty on purpose.
    }

    @Override
    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void onFrameCaptured(VideoFrame frame) {
      frame.retain();
      frames.add(frame);
      notify();
    }

    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized ArrayList<VideoFrame> getMinimumFramesBlocking(int minFrames)
        throws InterruptedException {
      while (frames.size() < minFrames) {
        wait();
      }
      return new ArrayList<VideoFrame>(frames);
    }
  }

  @Before
  public void setUp() {
    NativeLibrary.initialize(new NativeLibrary.DefaultLoader(), TestConstants.NATIVE_LIBRARY);
  }

  @Test
  @SmallTest
  public void testVideoCaptureFromFile() throws InterruptedException, IOException {
    final int FRAME_WIDTH = 4;
    final int FRAME_HEIGHT = 4;
    final int FRAME_CHROMA_WIDTH = (FRAME_WIDTH + 1) / 2;
    final int FRAME_CHROMA_HEIGHT = (FRAME_HEIGHT + 1) / 2;
    final int FRAME_SIZE_Y = FRAME_WIDTH * FRAME_HEIGHT;
    final int FRAME_SIZE_CHROMA = FRAME_CHROMA_WIDTH * FRAME_CHROMA_HEIGHT;

    final FileVideoCapturer fileVideoCapturer =
        new FileVideoCapturer(Environment.getExternalStorageDirectory().getPath()
            + "/chromium_tests_root/sdk/android/instrumentationtests/src/org/webrtc/"
            + "capturetestvideo.y4m");
    final MockCapturerObserver capturerObserver = new MockCapturerObserver();
    fileVideoCapturer.initialize(
        null /* surfaceTextureHelper */, null /* applicationContext */, capturerObserver);
    fileVideoCapturer.startCapture(FRAME_WIDTH, FRAME_HEIGHT, 33 /* fps */);

    final String[] expectedFrames = {
        "THIS IS JUST SOME TEXT x", "THE SECOND FRAME qwerty.", "HERE IS THE THRID FRAME!"};

    final ArrayList<VideoFrame> frames =
        capturerObserver.getMinimumFramesBlocking(expectedFrames.length);
    assertEquals(expectedFrames.length, frames.size());

    fileVideoCapturer.stopCapture();
    fileVideoCapturer.dispose();

    // Check the content of the frames.
    for (int i = 0; i < expectedFrames.length; ++i) {
      final VideoFrame frame = frames.get(i);
      final VideoFrame.Buffer buffer = frame.getBuffer();
      assertTrue(buffer instanceof VideoFrame.I420Buffer);
      final VideoFrame.I420Buffer i420Buffer = (VideoFrame.I420Buffer) buffer;

      assertEquals(FRAME_WIDTH, i420Buffer.getWidth());
      assertEquals(FRAME_HEIGHT, i420Buffer.getHeight());

      final ByteBuffer dataY = i420Buffer.getDataY();
      final ByteBuffer dataU = i420Buffer.getDataU();
      final ByteBuffer dataV = i420Buffer.getDataV();

      assertEquals(FRAME_SIZE_Y, dataY.remaining());
      assertEquals(FRAME_SIZE_CHROMA, dataU.remaining());
      assertEquals(FRAME_SIZE_CHROMA, dataV.remaining());

      ByteBuffer frameContents = ByteBuffer.allocate(FRAME_SIZE_Y + 2 * FRAME_SIZE_CHROMA);
      frameContents.put(dataY);
      frameContents.put(dataU);
      frameContents.put(dataV);
      frameContents.rewind(); // Move back to the beginning.

      assertByteBufferContents(
          expectedFrames[i].getBytes(Charset.forName("US-ASCII")), frameContents);
      frame.release();
    }
  }

  private static void assertByteBufferContents(byte[] expected, ByteBuffer actual) {
    assertEquals("Unexpected ByteBuffer size.", expected.length, actual.remaining());
    for (int i = 0; i < expected.length; i++) {
      assertEquals("Unexpected byte at index: " + i, expected[i], actual.get());
    }
  }
}
