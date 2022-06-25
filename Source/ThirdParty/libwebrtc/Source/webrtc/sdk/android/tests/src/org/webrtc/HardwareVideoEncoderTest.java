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
import static java.util.concurrent.TimeUnit.SECONDS;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.os.Bundle;
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
import org.webrtc.VideoEncoder.BitrateAllocation;
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
  private static final EncodeInfo ENCODE_INFO_KEY_FRAME =
      new EncodeInfo(new FrameType[] {FrameType.VideoFrameKey});
  private static final EncodeInfo ENCODE_INFO_DELTA_FRAME =
      new EncodeInfo(new FrameType[] {FrameType.VideoFrameDelta});

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
    private BitrateAdjuster bitrateAdjuster = new BaseBitrateAdjuster();

    public TestEncoderBuilder setCodecType(VideoCodecMimeType codecType) {
      this.codecType = codecType;
      return this;
    }

    public TestEncoderBuilder setBitrateAdjuster(BitrateAdjuster bitrateAdjuster) {
      this.bitrateAdjuster = bitrateAdjuster;
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
          /* forceKeyFrameIntervalMs= */ 0, bitrateAdjuster,
          /* sharedContext= */ null);
    }
  }

  private VideoFrame createTestVideoFrame(long timestampNs) {
    byte[] i420 = CodecTestHelper.generateRandomData(
        TEST_ENCODER_SETTINGS.width * TEST_ENCODER_SETTINGS.height * 3 / 2);
    final VideoFrame.I420Buffer testBuffer =
        CodecTestHelper.wrapI420(TEST_ENCODER_SETTINGS.width, TEST_ENCODER_SETTINGS.height, i420);
    return new VideoFrame(testBuffer, /* rotation= */ 0, timestampNs);
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
    // Set-up.
    TestEncoder encoder = new TestEncoderBuilder().build();
    encoder.initEncode(TEST_ENCODER_SETTINGS, mockEncoderCallback);
    encoder.encode(createTestVideoFrame(/* timestampNs= */ 42), ENCODE_INFO_KEY_FRAME);

    // Test.
    byte[] outputData = CodecTestHelper.generateRandomData(100);
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

  @Test
  public void testFramerateWithFramerateBitrateAdjuster() {
    // Enable FramerateBitrateAdjuster and initialize encoder with frame rate 15fps. Vefity that our
    // initial frame rate setting is ignored and media encoder is initialized with 30fps
    // (FramerateBitrateAdjuster default).
    HardwareVideoEncoder encoder =
        new TestEncoderBuilder().setBitrateAdjuster(new FramerateBitrateAdjuster()).build();
    encoder.initEncode(
        new Settings(
            /* numberOfCores= */ 1,
            /* width= */ 640,
            /* height= */ 480,
            /* startBitrate= */ 10000,
            /* maxFramerate= */ 15,
            /* numberOfSimulcastStreams= */ 1,
            /* automaticResizeOn= */ true,
            /* capabilities= */ new VideoEncoder.Capabilities(false /* lossNotification */)),
        mockEncoderCallback);

    MediaFormat mediaFormat = fakeMediaCodecWrapper.getConfiguredFormat();
    assertThat(mediaFormat.getFloat(MediaFormat.KEY_FRAME_RATE)).isEqualTo(30.0f);
  }

  @Test
  public void testBitrateWithFramerateBitrateAdjuster() throws InterruptedException {
    // Enable FramerateBitrateAdjuster and change frame rate while encoding video. Verify that
    // bitrate setting passed to media encoder is adjusted to compensate for changes in frame rate.
    TestEncoder encoder =
        new TestEncoderBuilder().setBitrateAdjuster(new FramerateBitrateAdjuster()).build();
    encoder.initEncode(TEST_ENCODER_SETTINGS, mockEncoderCallback);

    encoder.encode(createTestVideoFrame(/* timestampNs= */ 0), ENCODE_INFO_KEY_FRAME);

    // Reduce frame rate by half.
    BitrateAllocation bitrateAllocation = new BitrateAllocation(
        /* bitratesBbs= */ new int[][] {new int[] {TEST_ENCODER_SETTINGS.startBitrate}});
    encoder.setRateAllocation(bitrateAllocation, TEST_ENCODER_SETTINGS.maxFramerate / 2);

    // Generate output to trigger bitrate update in encoder wrapper.
    fakeMediaCodecWrapper.addOutputData(
        CodecTestHelper.generateRandomData(100), /* presentationTimestampUs= */ 0, /* flags= */ 0);
    encoder.waitDeliverEncodedImage();

    // Frame rate has been reduced by half. Verify that bitrate doubled.
    ArgumentCaptor<Bundle> bundleCaptor = ArgumentCaptor.forClass(Bundle.class);
    verify(fakeMediaCodecWrapper, times(2)).setParameters(bundleCaptor.capture());
    Bundle params = bundleCaptor.getAllValues().get(1);
    assertThat(params.containsKey(MediaCodec.PARAMETER_KEY_VIDEO_BITRATE)).isTrue();
    assertThat(params.getInt(MediaCodec.PARAMETER_KEY_VIDEO_BITRATE))
        .isEqualTo(TEST_ENCODER_SETTINGS.startBitrate * 2);
  }

  @Test
  public void testTimestampsWithFramerateBitrateAdjuster() throws InterruptedException {
    // Enable FramerateBitrateAdjuster and change frame rate while encoding video. Verify that
    // encoder ignores changes in frame rate and calculates frame timestamps based on fixed frame
    // rate 30fps.
    TestEncoder encoder =
        new TestEncoderBuilder().setBitrateAdjuster(new FramerateBitrateAdjuster()).build();
    encoder.initEncode(TEST_ENCODER_SETTINGS, mockEncoderCallback);

    encoder.encode(createTestVideoFrame(/* timestampNs= */ 0), ENCODE_INFO_KEY_FRAME);

    // Reduce frametate by half.
    BitrateAllocation bitrateAllocation = new BitrateAllocation(
        /* bitratesBbs= */ new int[][] {new int[] {TEST_ENCODER_SETTINGS.startBitrate}});
    encoder.setRateAllocation(bitrateAllocation, TEST_ENCODER_SETTINGS.maxFramerate / 2);

    // Encoder is allowed to buffer up to 2 frames. Generate output to avoid frame dropping.
    fakeMediaCodecWrapper.addOutputData(
        CodecTestHelper.generateRandomData(100), /* presentationTimestampUs= */ 0, /* flags= */ 0);
    encoder.waitDeliverEncodedImage();

    encoder.encode(createTestVideoFrame(/* timestampNs= */ 1), ENCODE_INFO_DELTA_FRAME);
    encoder.encode(createTestVideoFrame(/* timestampNs= */ 2), ENCODE_INFO_DELTA_FRAME);

    ArgumentCaptor<Long> timestampCaptor = ArgumentCaptor.forClass(Long.class);
    verify(fakeMediaCodecWrapper, times(3))
        .queueInputBuffer(
            /* index= */ anyInt(),
            /* offset= */ anyInt(),
            /* size= */ anyInt(), timestampCaptor.capture(),
            /* flags= */ anyInt());

    long frameDurationMs = SECONDS.toMicros(1) / 30;
    assertThat(timestampCaptor.getAllValues())
        .containsExactly(0L, frameDurationMs, 2 * frameDurationMs);
  }
}
