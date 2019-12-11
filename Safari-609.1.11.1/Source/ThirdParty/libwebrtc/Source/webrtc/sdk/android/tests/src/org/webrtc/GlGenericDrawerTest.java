/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.webrtc.GlShader;

@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GlGenericDrawerTest {
  // Simplest possible valid generic fragment shader.
  private static final String FRAGMENT_SHADER = "void main() {\n"
      + "  gl_FragColor = sample(tc);\n"
      + "}\n";
  private static final int TEXTURE_ID = 3;
  private static final float[] TEX_MATRIX =
      new float[] {1, 2, 3, 4, -1, -2, -3, -4, 0, 0, 1, 0, 0, 0, 0, 1};
  private static final int FRAME_WIDTH = 640;
  private static final int FRAME_HEIGHT = 480;
  private static final int VIEWPORT_X = 3;
  private static final int VIEWPORT_Y = 5;
  private static final int VIEWPORT_WIDTH = 500;
  private static final int VIEWPORT_HEIGHT = 500;

  // Replace OpenGLES GlShader dependency with a mock.
  private class GlGenericDrawerForTest extends GlGenericDrawer {
    public GlGenericDrawerForTest(String genericFragmentSource, ShaderCallbacks shaderCallbacks) {
      super(genericFragmentSource, shaderCallbacks);
    }

    @Override
    GlShader createShader(ShaderType shaderType) {
      return mockedShader;
    }
  }

  private GlShader mockedShader;
  private GlGenericDrawer glGenericDrawer;
  private GlGenericDrawer.ShaderCallbacks mockedCallbacks;

  @Before
  public void setUp() {
    mockedShader = mock(GlShader.class);
    mockedCallbacks = mock(GlGenericDrawer.ShaderCallbacks.class);
    glGenericDrawer = new GlGenericDrawerForTest(FRAGMENT_SHADER, mockedCallbacks);
  }

  @After
  public void tearDown() {
    verifyNoMoreInteractions(mockedCallbacks);
  }

  @Test
  public void testOesFragmentShader() {
    final String expectedOesFragmentShader = "#extension GL_OES_EGL_image_external : require\n"
        + "precision mediump float;\n"
        + "varying vec2 tc;\n"
        + "uniform samplerExternalOES tex;\n"
        + "void main() {\n"
        + "  gl_FragColor = texture2D(tex, tc);\n"
        + "}\n";
    final String oesFragmentShader =
        GlGenericDrawer.createFragmentShaderString(FRAGMENT_SHADER, GlGenericDrawer.ShaderType.OES);
    assertEquals(expectedOesFragmentShader, oesFragmentShader);
  }

  @Test
  public void testRgbFragmentShader() {
    final String expectedRgbFragmentShader = "precision mediump float;\n"
        + "varying vec2 tc;\n"
        + "uniform sampler2D tex;\n"
        + "void main() {\n"
        + "  gl_FragColor = texture2D(tex, tc);\n"
        + "}\n";
    final String rgbFragmentShader =
        GlGenericDrawer.createFragmentShaderString(FRAGMENT_SHADER, GlGenericDrawer.ShaderType.RGB);
    assertEquals(expectedRgbFragmentShader, rgbFragmentShader);
  }

  @Test
  public void testYuvFragmentShader() {
    final String expectedYuvFragmentShader = "precision mediump float;\n"
        + "varying vec2 tc;\n"
        + "uniform sampler2D y_tex;\n"
        + "uniform sampler2D u_tex;\n"
        + "uniform sampler2D v_tex;\n"
        + "vec4 sample(vec2 p) {\n"
        + "  float y = texture2D(y_tex, p).r * 1.16438;\n"
        + "  float u = texture2D(u_tex, p).r;\n"
        + "  float v = texture2D(v_tex, p).r;\n"
        + "  return vec4(y + 1.59603 * v - 0.874202,\n"
        + "    y - 0.391762 * u - 0.812968 * v + 0.531668,\n"
        + "    y + 2.01723 * u - 1.08563, 1);\n"
        + "}\n"
        + "void main() {\n"
        + "  gl_FragColor = sample(tc);\n"
        + "}\n";
    final String yuvFragmentShader =
        GlGenericDrawer.createFragmentShaderString(FRAGMENT_SHADER, GlGenericDrawer.ShaderType.YUV);
    assertEquals(expectedYuvFragmentShader, yuvFragmentShader);
  }

  @Test
  public void testShaderCallbacksOneRgbFrame() {
    glGenericDrawer.drawRgb(TEXTURE_ID, TEX_MATRIX, FRAME_WIDTH, FRAME_HEIGHT, VIEWPORT_X,
        VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    verify(mockedCallbacks).onNewShader(mockedShader);
    verify(mockedCallbacks)
        .onPrepareShader(
            mockedShader, TEX_MATRIX, FRAME_WIDTH, FRAME_HEIGHT, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
  }

  @Test
  public void testShaderCallbacksTwoRgbFrames() {
    glGenericDrawer.drawRgb(TEXTURE_ID, TEX_MATRIX, FRAME_WIDTH, FRAME_HEIGHT, VIEWPORT_X,
        VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
    glGenericDrawer.drawRgb(TEXTURE_ID, TEX_MATRIX, FRAME_WIDTH, FRAME_HEIGHT, VIEWPORT_X,
        VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    // Expect only one shader to be created, but two frames to be drawn.
    verify(mockedCallbacks, times(1)).onNewShader(mockedShader);
    verify(mockedCallbacks, times(2))
        .onPrepareShader(
            mockedShader, TEX_MATRIX, FRAME_WIDTH, FRAME_HEIGHT, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
  }

  @Test
  public void testShaderCallbacksChangingShaderType() {
    glGenericDrawer.drawRgb(TEXTURE_ID, TEX_MATRIX, FRAME_WIDTH, FRAME_HEIGHT, VIEWPORT_X,
        VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
    glGenericDrawer.drawOes(TEXTURE_ID, TEX_MATRIX, FRAME_WIDTH, FRAME_HEIGHT, VIEWPORT_X,
        VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    // Expect two shaders to be created, and two frames to be drawn.
    verify(mockedCallbacks, times(2)).onNewShader(mockedShader);
    verify(mockedCallbacks, times(2))
        .onPrepareShader(
            mockedShader, TEX_MATRIX, FRAME_WIDTH, FRAME_HEIGHT, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
  }
}
