/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
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

import android.annotation.SuppressLint;
import android.graphics.Point;
import android.support.test.InstrumentationRegistry;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.MediumTest;
import android.support.test.rule.UiThreadTestRule;
import android.view.View.MeasureSpec;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.List;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(BaseJUnit4ClassRunner.class)
public class SurfaceViewRendererOnMeasureTest {
  @Rule public UiThreadTestRule uiThreadTestRule = new UiThreadTestRule();

  /**
   * List with all possible scaling types.
   */
  private static final List<RendererCommon.ScalingType> scalingTypes = Arrays.asList(
      RendererCommon.ScalingType.SCALE_ASPECT_FIT, RendererCommon.ScalingType.SCALE_ASPECT_FILL,
      RendererCommon.ScalingType.SCALE_ASPECT_BALANCED);

  /**
   * List with MeasureSpec modes.
   */
  private static final List<Integer> measureSpecModes =
      Arrays.asList(MeasureSpec.EXACTLY, MeasureSpec.AT_MOST);

  /**
   * Returns a dummy YUV frame.
   */
  static VideoRenderer.I420Frame createFrame(int width, int height, int rotationDegree) {
    final int[] yuvStrides = new int[] {width, (width + 1) / 2, (width + 1) / 2};
    final int[] yuvHeights = new int[] {height, (height + 1) / 2, (height + 1) / 2};
    final ByteBuffer[] yuvPlanes = new ByteBuffer[3];
    for (int i = 0; i < 3; ++i) {
      yuvPlanes[i] = ByteBuffer.allocateDirect(yuvStrides[i] * yuvHeights[i]);
    }
    return new VideoRenderer.I420Frame(width, height, rotationDegree, yuvStrides, yuvPlanes, 0);
  }

  /**
   * Assert onMeasure() with given parameters will result in expected measured size.
   */
  @SuppressLint("WrongCall")
  private static void assertMeasuredSize(SurfaceViewRenderer surfaceViewRenderer,
      RendererCommon.ScalingType scalingType, String frameDimensions, int expectedWidth,
      int expectedHeight, int widthSpec, int heightSpec) {
    surfaceViewRenderer.setScalingType(scalingType);
    surfaceViewRenderer.onMeasure(widthSpec, heightSpec);
    final int measuredWidth = surfaceViewRenderer.getMeasuredWidth();
    final int measuredHeight = surfaceViewRenderer.getMeasuredHeight();
    if (measuredWidth != expectedWidth || measuredHeight != expectedHeight) {
      fail("onMeasure(" + MeasureSpec.toString(widthSpec) + ", " + MeasureSpec.toString(heightSpec)
          + ")"
          + " with scaling type " + scalingType + " and frame: " + frameDimensions
          + " expected measured size " + expectedWidth + "x" + expectedHeight + ", but was "
          + measuredWidth + "x" + measuredHeight);
    }
  }

  /**
   * Test how SurfaceViewRenderer.onMeasure() behaves when no frame has been delivered.
   */
  @Test
  @UiThreadTest
  @MediumTest
  public void testNoFrame() {
    final SurfaceViewRenderer surfaceViewRenderer =
        new SurfaceViewRenderer(InstrumentationRegistry.getContext());
    final String frameDimensions = "null";

    // Test behaviour before SurfaceViewRenderer.init() is called.
    for (RendererCommon.ScalingType scalingType : scalingTypes) {
      for (int measureSpecMode : measureSpecModes) {
        final int zeroMeasureSize = MeasureSpec.makeMeasureSpec(0, measureSpecMode);
        assertMeasuredSize(surfaceViewRenderer, scalingType, frameDimensions, 0, 0, zeroMeasureSize,
            zeroMeasureSize);
        assertMeasuredSize(surfaceViewRenderer, scalingType, frameDimensions, 1280, 720,
            MeasureSpec.makeMeasureSpec(1280, measureSpecMode),
            MeasureSpec.makeMeasureSpec(720, measureSpecMode));
      }
    }

    // Test behaviour after SurfaceViewRenderer.init() is called, but still no frame.
    surfaceViewRenderer.init((EglBase.Context) null, null);
    for (RendererCommon.ScalingType scalingType : scalingTypes) {
      for (int measureSpecMode : measureSpecModes) {
        final int zeroMeasureSize = MeasureSpec.makeMeasureSpec(0, measureSpecMode);
        assertMeasuredSize(surfaceViewRenderer, scalingType, frameDimensions, 0, 0, zeroMeasureSize,
            zeroMeasureSize);
        assertMeasuredSize(surfaceViewRenderer, scalingType, frameDimensions, 1280, 720,
            MeasureSpec.makeMeasureSpec(1280, measureSpecMode),
            MeasureSpec.makeMeasureSpec(720, measureSpecMode));
      }
    }

    surfaceViewRenderer.release();
  }

  /**
   * Test how SurfaceViewRenderer.onMeasure() behaves with a 1280x720 frame.
   */
  @Test
  @UiThreadTest
  @MediumTest
  public void testFrame1280x720() throws InterruptedException {
    final SurfaceViewRenderer surfaceViewRenderer =
        new SurfaceViewRenderer(InstrumentationRegistry.getContext());
    /**
     * Mock renderer events with blocking wait functionality for frame size changes.
     */
    class MockRendererEvents implements RendererCommon.RendererEvents {
      private int frameWidth;
      private int frameHeight;
      private int rotation;

      public synchronized void waitForFrameSize(int frameWidth, int frameHeight, int rotation)
          throws InterruptedException {
        while (this.frameWidth != frameWidth || this.frameHeight != frameHeight
            || this.rotation != rotation) {
          wait();
        }
      }

      public void onFirstFrameRendered() {}

      public synchronized void onFrameResolutionChanged(
          int frameWidth, int frameHeight, int rotation) {
        this.frameWidth = frameWidth;
        this.frameHeight = frameHeight;
        this.rotation = rotation;
        notifyAll();
      }
    }
    final MockRendererEvents rendererEvents = new MockRendererEvents();
    surfaceViewRenderer.init((EglBase.Context) null, rendererEvents);

    // Test different rotation degress, but same rotated size.
    for (int rotationDegree : new int[] {0, 90, 180, 270}) {
      final int rotatedWidth = 1280;
      final int rotatedHeight = 720;
      final int unrotatedWidth = (rotationDegree % 180 == 0 ? rotatedWidth : rotatedHeight);
      final int unrotatedHeight = (rotationDegree % 180 == 0 ? rotatedHeight : rotatedWidth);
      final VideoRenderer.I420Frame frame =
          createFrame(unrotatedWidth, unrotatedHeight, rotationDegree);
      assertEquals(rotatedWidth, frame.rotatedWidth());
      assertEquals(rotatedHeight, frame.rotatedHeight());
      final String frameDimensions =
          unrotatedWidth + "x" + unrotatedHeight + " with rotation " + rotationDegree;
      surfaceViewRenderer.renderFrame(frame);
      rendererEvents.waitForFrameSize(unrotatedWidth, unrotatedHeight, rotationDegree);

      // Test forcing to zero size.
      for (RendererCommon.ScalingType scalingType : scalingTypes) {
        for (int measureSpecMode : measureSpecModes) {
          final int zeroMeasureSize = MeasureSpec.makeMeasureSpec(0, measureSpecMode);
          assertMeasuredSize(surfaceViewRenderer, scalingType, frameDimensions, 0, 0,
              zeroMeasureSize, zeroMeasureSize);
        }
      }

      // Test perfect fit.
      for (RendererCommon.ScalingType scalingType : scalingTypes) {
        for (int measureSpecMode : measureSpecModes) {
          assertMeasuredSize(surfaceViewRenderer, scalingType, frameDimensions, rotatedWidth,
              rotatedHeight, MeasureSpec.makeMeasureSpec(rotatedWidth, measureSpecMode),
              MeasureSpec.makeMeasureSpec(rotatedHeight, measureSpecMode));
        }
      }

      // Force spec size with different aspect ratio than frame aspect ratio.
      for (RendererCommon.ScalingType scalingType : scalingTypes) {
        assertMeasuredSize(surfaceViewRenderer, scalingType, frameDimensions, 720, 1280,
            MeasureSpec.makeMeasureSpec(720, MeasureSpec.EXACTLY),
            MeasureSpec.makeMeasureSpec(1280, MeasureSpec.EXACTLY));
      }

      final float videoAspectRatio = (float) rotatedWidth / rotatedHeight;
      {
        // Relax both width and height constraints.
        final int widthSpec = MeasureSpec.makeMeasureSpec(720, MeasureSpec.AT_MOST);
        final int heightSpec = MeasureSpec.makeMeasureSpec(1280, MeasureSpec.AT_MOST);
        for (RendererCommon.ScalingType scalingType : scalingTypes) {
          final Point expectedSize =
              RendererCommon.getDisplaySize(scalingType, videoAspectRatio, 720, 1280);
          assertMeasuredSize(surfaceViewRenderer, scalingType, frameDimensions, expectedSize.x,
              expectedSize.y, widthSpec, heightSpec);
        }
      }
      {
        // Force width to 720, but relax height constraint. This will give the same result as
        // above, because width is already the limiting factor and will be maxed out.
        final int widthSpec = MeasureSpec.makeMeasureSpec(720, MeasureSpec.EXACTLY);
        final int heightSpec = MeasureSpec.makeMeasureSpec(1280, MeasureSpec.AT_MOST);
        for (RendererCommon.ScalingType scalingType : scalingTypes) {
          final Point expectedSize =
              RendererCommon.getDisplaySize(scalingType, videoAspectRatio, 720, 1280);
          assertMeasuredSize(surfaceViewRenderer, scalingType, frameDimensions, expectedSize.x,
              expectedSize.y, widthSpec, heightSpec);
        }
      }
      {
        // Force height, but relax width constraint. This will force a bad layout size.
        final int widthSpec = MeasureSpec.makeMeasureSpec(720, MeasureSpec.AT_MOST);
        final int heightSpec = MeasureSpec.makeMeasureSpec(1280, MeasureSpec.EXACTLY);
        for (RendererCommon.ScalingType scalingType : scalingTypes) {
          assertMeasuredSize(
              surfaceViewRenderer, scalingType, frameDimensions, 720, 1280, widthSpec, heightSpec);
        }
      }
    }

    surfaceViewRenderer.release();
  }
}
