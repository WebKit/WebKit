/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import java.nio.ByteBuffer;
import java.util.HashMap;
import java.util.Map;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.webrtc.EncodedImage;
import org.webrtc.EncodedImage.FrameType;
import org.webrtc.FakeMediaCodecWrapper.State;
import org.webrtc.VideoCodecStatus;
import org.webrtc.VideoEncoder;
import org.webrtc.VideoEncoder.CodecSpecificInfo;
import org.webrtc.VideoEncoder.EncodeInfo;
import org.webrtc.VideoEncoder.Settings;
import org.webrtc.VideoFrame;
import org.webrtc.VideoFrame.Buffer;
import org.webrtc.VideoFrame.I420Buffer;

@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HardwareVideoEncoderTest {
  private static final VideoEncoder.Settings TEST_ENCODER_SETTINGS = new Settings(
      /* numberOfCores= */ 1,
      /* width= */ 640,
      /* height= */ 480,
      /* startBitrate= */ 10000,
      /* maxFramerate= */ 30,
      /* numberOfSimulcastStreams= */ 1,
      /* automaticResizeOn= */ true,
      /* capabilities= */ new VideoEncoder.Capabilities(false /* lossNotification */));
  private static final long POLL_DELAY_MS = 10;
  private static final long DELIVER_ENCODED_IMAGE_DELAY_MS = 10;

  private static class TestEncoder extends HardwareVideoEncoder {
    private final Object deliverEncodedImageLock = new Object();
    private boolean deliverEncodedImageDone = true;

    TestEncoder(MediaCodecWrapperFactory mediaCodecWrapperFactory, String codecName,
        VideoCodecMimeType codecType, Integer surfaceColorFormat, Integer yuvColorFormat,
        Map<String, String> params, int keyFrameIntervalSec, int forceKeyFrameIntervalMs,
        BitrateAdjuster bitrateAdjuster, EglBase14.Context sharedContext) {
      super(mediaCodecWrapperFactory, codecName, codecType, surfaceColorFormat, yuvColorFormat,
          params, keyFrameIntervalSec, forceKeyFrameIntervalMs, bitrateAdjuster, sharedContext);
    }

    public void waitDeliverEncodedImage() throws InterruptedException {
      synchronized (deliverEncodedImageLock) {
        deliverEncodedImageDone = false;
        deliverEncodedImageLock.notifyAll();
        while (!deliverEncodedImageDone) {
          deliverEncodedImageLock.wait();
        }
      }
    }

    @SuppressWarnings("WaitNotInLoop") // This method is called inside a loop.
    @Override
    protected void deliverEncodedImage() {
      synchronized (deliverEncodedImageLock) {
        if (deliverEncodedImageDone) {
          try {
            deliverEncodedImageLock.wait(DELIVER_ENCODED_IMAGE_DELAY_MS);
          } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            return;
          }
        }
        if (deliverEncodedImageDone) {
          return;
        }
        super.deliverEncodedImage();
        deliverEncodedImageDone = true;
        deliverEncodedImageLock.notifyAll();
      }
    }

    @Override
    protected void fillInputBuffer(ByteBuffer buffer, Buffer videoFrameBuffer) {
      I420Buffer i420Buffer = videoFrameBuffer.toI420();
      buffer.put(i420Buffer.getDataY());
      buffer.put(i420Buffer.getDataU());
      buffer.put(i420Buffer.getDataV());
      buffer.flip();
      i420Buffer.release();
    }
  }

  private class TestEncoderBuilder {
    private VideoCodecMimeType codecType = VideoCodecMimeType.VP8;

    public TestEncoderBuilder setCodecType(VideoCodecMimeType codecType) {
      this.codecType = codecType;
      return this;
    }

    public TestEncoder build() {
      return new TestEncoder((String name)
                                 -> fakeMediaCodecWrapper,
          "org.webrtc.testencoder", codecType,
          /* surfaceColorFormat= */ null,
          /* yuvColorFormat= */ MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Planar,
          /* params= */ new HashMap<>(),
          /* keyFrameIntervalSec= */ 0,
          /* forceKeyFrameIntervalMs= */ 0,
          /* bitrateAdjuster= */ new BaseBitrateAdjuster(),
          /* sharedContext= */ null);
    }
  }

  @Mock VideoEncoder.Callback mockEncoderCallback;
  private FakeMediaCodecWrapper fakeMediaCodecWrapper;

  @Before
  public void setUp() {
    MockitoAnnotations.initMocks(this);
    MediaFormat outputFormat = new MediaFormat();
    // TODO(sakal): Add more details to output format as needed.
    fakeMediaCodecWrapper = spy(new FakeMediaCodecWrapper(outputFormat));
  }

  @Test
  public void testInit() {
    // Set-up.
    HardwareVideoEncoder encoder =
        new TestEncoderBuilder().setCodecType(VideoCodecMimeType.VP8).build();

    // Test.
    assertThat(encoder.initEncode(TEST_ENCODER_SETTINGS, mockEncoderCallback))
        .isEqualTo(VideoCodecStatus.OK);

    // Verify.
    assertThat(fakeMediaCodecWrapper.getState()).isEqualTo(State.EXECUTING_RUNNING);

    MediaFormat mediaFormat = fakeMediaCodecWrapper.getConfiguredFormat();
    assertThat(mediaFormat).isNotNull();
    assertThat(mediaFormat.getInteger(MediaFormat.KEY_WIDTH))
        .isEqualTo(TEST_ENCODER_SETTINGS.width);
    assertThat(mediaFormat.getInteger(MediaFormat.KEY_HEIGHT))
        .isEqualTo(TEST_ENCODER_SETTINGS.height);
    assertThat(mediaFormat.getString(MediaFormat.KEY_MIME))
        .isEqualTo(VideoCodecMimeType.VP8.mimeType());

    assertThat(fakeMediaCodecWrapper.getConfiguredFlags())
        .isEqualTo(MediaCodec.CONFIGURE_FLAG_ENCODE);
  }

  @Test
  public void testEncodeByteBuffer() {
    // Set-up.
    HardwareVideoEncoder encoder = new TestEncoderBuilder().build();
    encoder.initEncode(TEST_ENCODER_SETTINGS, mockEncoderCallback);

    // Test.
    byte[] i420 = CodecTestHelper.generateRandomData(
        TEST_ENCODER_SETTINGS.width * TEST_ENCODER_SETTINGS.height * 3 / 2);
    final VideoFrame.I420Buffer testBuffer =
        CodecTestHelper.wrapI420(TEST_ENCODER_SETTINGS.width, TEST_ENCODER_SETTINGS.height, i420);
    final VideoFrame testFrame =
        new VideoFrame(testBuffer, /* rotation= */ 0, /* timestampNs= */ 0);
    assertThat(encoder.encode(testFrame, new EncodeInfo(new FrameType[] {FrameType.VideoFrameKey})))
        .isEqualTo(VideoCodecStatus.OK);

    // Verify.
    ArgumentCaptor<Integer> indexCaptor = ArgumentCaptor.forClass(Integer.class);
    ArgumentCaptor<Integer> offsetCaptor = ArgumentCaptor.forClass(Integer.class);
    ArgumentCaptor<Integer> sizeCaptor = ArgumentCaptor.forClass(Integer.class);
    verify(fakeMediaCodecWrapper)
        .queueInputBuffer(indexCaptor.capture(), offsetCaptor.capture(), sizeCaptor.capture(),
            anyLong(), anyInt());
    ByteBuffer buffer = fakeMediaCodecWrapper.getInputBuffers()[indexCaptor.getValue()];
    CodecTestHelper.assertEqualContents(
        i420, buffer, offsetCaptor.getValue(), sizeCaptor.getValue());
  }

  @Test
  public void testDeliversOutputData() throws InterruptedException {
    final int outputDataLength = 100;

    // Set-up.
    TestEncoder encoder = new TestEncoderBuilder().build();
    encoder.initEncode(TEST_ENCODER_SETTINGS, mockEncoderCallback);
    byte[] i420 = CodecTestHelper.generateRandomData(
        TEST_ENCODER_SETTINGS.width * TEST_ENCODER_SETTINGS.height * 3 / 2);
    final VideoFrame.I420Buffer testBuffer =
        CodecTestHelper.wrapI420(TEST_ENCODER_SETTINGS.width, TEST_ENCODER_SETTINGS.height, i420);
    final VideoFrame testFrame =
        new VideoFrame(testBuffer, /* rotation= */ 0, /* timestampNs= */ 42);
    encoder.encode(testFrame, new EncodeInfo(new FrameType[] {FrameType.VideoFrameKey}));

    // Test.
    byte[] outputData = CodecTestHelper.generateRandomData(outputDataLength);
    fakeMediaCodecWrapper.addOutputData(outputData,
        /* presentationTimestampUs= */ 0,
        /* flags= */ MediaCodec.BUFFER_FLAG_SYNC_FRAME);

    encoder.waitDeliverEncodedImage();

    // Verify.
    ArgumentCaptor<EncodedImage> videoFrameCaptor = ArgumentCaptor.forClass(EncodedImage.class);
    verify(mockEncoderCallback)
        .onEncodedFrame(videoFrameCaptor.capture(), any(CodecSpecificInfo.class));

    EncodedImage videoFrame = videoFrameCaptor.getValue();
    assertThat(videoFrame).isNotNull();
    assertThat(videoFrame.encodedWidth).isEqualTo(TEST_ENCODER_SETTINGS.width);
    assertThat(videoFrame.encodedHeight).isEqualTo(TEST_ENCODER_SETTINGS.height);
    assertThat(videoFrame.rotation).isEqualTo(0);
    assertThat(videoFrame.captureTimeNs).isEqualTo(42);
    assertThat(videoFrame.completeFrame).isTrue();
    assertThat(videoFrame.frameType).isEqualTo(FrameType.VideoFrameKey);
    CodecTestHelper.assertEqualContents(
        outputData, videoFrame.buffer, /* offset= */ 0, videoFrame.buffer.capacity());
  }

  @Test
  public void testRelease() {
    // Set-up.
    HardwareVideoEncoder encoder = new TestEncoderBuilder().build();
    encoder.initEncode(TEST_ENCODER_SETTINGS, mockEncoderCallback);

    // Test.
    assertThat(encoder.release()).isEqualTo(VideoCodecStatus.OK);

    // Verify.
    assertThat(fakeMediaCodecWrapper.getState()).isEqualTo(State.RELEASED);
  }

  @Test
  public void testReleaseMultipleTimes() {
    // Set-up.
    HardwareVideoEncoder encoder = new TestEncoderBuilder().build();
    encoder.initEncode(TEST_ENCODER_SETTINGS, mockEncoderCallback);

    // Test.
    assertThat(encoder.release()).isEqualTo(VideoCodecStatus.OK);
    assertThat(encoder.release()).isEqualTo(VideoCodecStatus.OK);

    // Verify.
    assertThat(fakeMediaCodecWrapper.getState()).isEqualTo(State.RELEASED);
  }
}
