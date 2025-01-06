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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import org.junit.Before;
import org.junit.Test;

/** Unit tests for {@link VideoTrack}. */
public class VideoTrackTest {
  private PeerConnectionFactory factory;
  private VideoSource videoSource;
  private VideoTrack videoTrack;

  @Before
  public void setUp() {
    PeerConnectionFactory.initialize(PeerConnectionFactory.InitializationOptions
                                         .builder(InstrumentationRegistry.getTargetContext())
                                         .setNativeLibraryName(TestConstants.NATIVE_LIBRARY)
                                         .createInitializationOptions());

    factory = PeerConnectionFactory.builder().createPeerConnectionFactory();
    videoSource = factory.createVideoSource(/* isScreencast= */ false);
    videoTrack = factory.createVideoTrack("video", videoSource);
  }

  @Test
  @SmallTest
  public void testAddingNullVideoSink() {
    try {
      videoTrack.addSink(/* sink= */ null);
      fail("Should have thrown an IllegalArgumentException.");
    } catch (IllegalArgumentException e) {
      // Expected path.
    }
  }

  @Test
  @SmallTest
  public void testRemovingNullVideoSink() {
    videoTrack.removeSink(/* sink= */ null);
  }

  @Test
  @SmallTest
  public void testRemovingNonExistantVideoSink() {
    final VideoSink videoSink = new VideoSink() {
      @Override
      public void onFrame(VideoFrame frame) {}
    };
    videoTrack.removeSink(videoSink);
  }

  @Test
  @SmallTest
  public void testAddingSameVideoSinkMultipleTimes() {
    class FrameCounter implements VideoSink {
      private int count;

      public int getCount() {
        return count;
      }

      @Override
      public void onFrame(VideoFrame frame) {
        count += 1;
      }
    }
    final FrameCounter frameCounter = new FrameCounter();

    final VideoFrame videoFrame = new VideoFrame(
        JavaI420Buffer.allocate(/* width= */ 32, /* height= */ 32), /* rotation= */ 0,
        /* timestampNs= */ 0);

    videoTrack.addSink(frameCounter);
    videoTrack.addSink(frameCounter);
    videoSource.getCapturerObserver().onFrameCaptured(videoFrame);

    // Even though we called addSink() multiple times, we should only get one frame out.
    assertEquals(1, frameCounter.count);
  }

  @Test
  @SmallTest
  public void testAddingAndRemovingVideoSink() {
    final VideoFrame videoFrame = new VideoFrame(
        JavaI420Buffer.allocate(/* width= */ 32, /* height= */ 32), /* rotation= */ 0,
        /* timestampNs= */ 0);

    final VideoSink failSink = new VideoSink() {
      @Override
      public void onFrame(VideoFrame frame) {
        fail("onFrame() should not be called on removed sink");
      }
    };
    videoTrack.addSink(failSink);
    videoTrack.removeSink(failSink);
    videoSource.getCapturerObserver().onFrameCaptured(videoFrame);
  }
}
