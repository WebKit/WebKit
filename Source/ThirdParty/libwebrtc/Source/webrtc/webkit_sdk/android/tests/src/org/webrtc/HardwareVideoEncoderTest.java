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

import static android.media.MediaCodec.BUFFER_FLAG_CODEC_CONFIG;
import static android.media.MediaCodec.BUFFER_FLAG_SYNC_FRAME;
import static android.media.MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Planar;
import static android.media.MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420SemiPlanar;
import static com.google.common.truth.Truth.assertThat;
import static java.util.concurrent.TimeUnit.SECONDS;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.nullable;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.webrtc.VideoCodecMimeType.AV1;
import static org.webrtc.VideoCodecMimeType.H264;
import static org.webrtc.VideoCodecMimeType.H265;
import static org.webrtc.VideoCodecMimeType.VP8;
import static org.webrtc.VideoCodecMimeType.VP9;

import android.media.MediaCodec;
import android.media.MediaFormat;
import android.os.Bundle;
import androidx.test.runner.AndroidJUnit4;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.util.HashMap;
import java.util.Map;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;
import org.robolectric.annotation.Config;
import org.webrtc.EncodedImage;
import org.webrtc.EncodedImage.FrameType;
import org.webrtc.FakeMediaCodecWrapper.State;
import org.webrtc.Logging;
import org.webrtc.VideoCodecStatus;
import org.webrtc.VideoEncoder;
import org.webrtc.VideoEncoder.BitrateAllocation;
import org.webrtc.VideoEncoder.CodecSpecificInfo;
import org.webrtc.VideoEncoder.EncodeInfo;
import org.webrtc.VideoEncoder.Settings;
import org.webrtc.VideoFrame;
import org.webrtc.VideoFrame.Buffer;
import org.webrtc.VideoFrame.I420Buffer;

@RunWith(AndroidJUnit4.class)
@Config(manifest = Config.NONE)
public class HardwareVideoEncoderTest {
  private static final int WIDTH = 640;
  private static final int HEIGHT = 480;
  private static final VideoEncoder.Settings TEST_ENCODER_SETTINGS = new Settings(
      /* numberOfCores= */ 1, WIDTH, HEIGHT,
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
    private boolean isEncodingStatisticsSupported;

    TestEncoder(MediaCodecWrapperFactory mediaCodecWrapperFactory, String codecName,
        VideoCodecMimeType codecType, Integer surfaceColorFormat, Integer yuvColorFormat,
        Map<String, String> params, int keyFrameIntervalSec, int forceKeyFrameIntervalMs,
        BitrateAdjuster bitrateAdjuster, EglBase14.Context sharedContext,
        boolean isEncodingStatisticsSupported) {
      super(mediaCodecWrapperFactory, codecName, codecType, surfaceColorFormat, yuvColorFormat,
          params, keyFrameIntervalSec, forceKeyFrameIntervalMs, bitrateAdjuster, sharedContext);
      this.isEncodingStatisticsSupported = isEncodingStatisticsSupported;
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

    @Override
    protected boolean isEncodingStatisticsSupported() {
      return isEncodingStatisticsSupported;
    }
  }

  private class TestEncoderBuilder {
    private VideoCodecMimeType codecType = VP8;
    private BitrateAdjuster bitrateAdjuster = new BaseBitrateAdjuster();
    private boolean isEncodingStatisticsSupported;
    private int colorFormat = COLOR_FormatYUV420Planar;

    public TestEncoderBuilder setCodecType(VideoCodecMimeType codecType) {
      this.codecType = codecType;
      return this;
    }

    public TestEncoderBuilder setBitrateAdjuster(BitrateAdjuster bitrateAdjuster) {
      this.bitrateAdjuster = bitrateAdjuster;
      return this;
    }

    public TestEncoderBuilder setIsEncodingStatisticsSupported(
        boolean isEncodingStatisticsSupported) {
      this.isEncodingStatisticsSupported = isEncodingStatisticsSupported;
      return this;
    }

    public TestEncoderBuilder setColorFormat(int colorFormat) {
      this.colorFormat = colorFormat;
      return this;
    }

    public TestEncoder build() {
      return new TestEncoder((String name)
                                 -> fakeMediaCodecWrapper,
          "org.webrtc.testencoder", codecType,
          /* surfaceColorFormat= */ null, colorFormat,
          /* params= */ new HashMap<>(),
          /* keyFrameIntervalSec= */ 0,
          /* forceKeyFrameIntervalMs= */ 0, bitrateAdjuster,
          /* sharedContext= */ null, isEncodingStatisticsSupported);
    }
  }

  private VideoFrame createTestVideoFrame(long timestampNs) {
    byte[] i420 = CodecTestHelper.generateRandomData(
        TEST_ENCODER_SETTINGS.width * TEST_ENCODER_SETTINGS.height * 3 / 2);
    final VideoFrame.I420Buffer testBuffer =
        CodecTestHelper.wrapI420(TEST_ENCODER_SETTINGS.width, TEST_ENCODER_SETTINGS.height, i420);
    return new VideoFrame(testBuffer, /* rotation= */ 0, timestampNs);
  }

  @Mock private VideoEncoder.Callback mockEncoderCallback;
  @Spy private FakeMediaCodecWrapper fakeMediaCodecWrapper;

  @Before
  public void setUp() {
    MockitoAnnotations.initMocks(this);
  }

  @Test
  public void testInit() {
    // Set-up.
    HardwareVideoEncoder encoder = new TestEncoderBuilder().setCodecType(VP8).build();

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
    assertThat(mediaFormat.getString(MediaFormat.KEY_MIME)).isEqualTo(VP8.mimeType());

    assertThat(fakeMediaCodecWrapper.getConfiguredFlags())
        .isEqualTo(MediaCodec.CONFIGURE_FLAG_ENCODE);
  }

  @Test
  public void encodingStatistics_unsupported_disabled() throws InterruptedException {
    TestEncoder encoder = new TestEncoderBuilder().setIsEncodingStatisticsSupported(false).build();

    assertThat(encoder.initEncode(TEST_ENCODER_SETTINGS, mockEncoderCallback))
        .isEqualTo(VideoCodecStatus.OK);

    MediaFormat configuredFormat = fakeMediaCodecWrapper.getConfiguredFormat();
    assertThat(configuredFormat).isNotNull();
    assertThat(configuredFormat.containsKey(MediaFormat.KEY_VIDEO_ENCODING_STATISTICS_LEVEL))
        .isFalse();

    // Verify that QP is not set in encoded frame even if reported by MediaCodec.
    MediaFormat outputFormat = new MediaFormat();
    outputFormat.setInteger(MediaFormat.KEY_VIDEO_QP_AVERAGE, 123);
    doReturn(outputFormat).when(fakeMediaCodecWrapper).getOutputFormat(anyInt());

    encoder.encode(createTestVideoFrame(/* timestampNs= */ 42), ENCODE_INFO_KEY_FRAME);

    fakeMediaCodecWrapper.addOutputData(CodecTestHelper.generateRandomData(100),
        /* presentationTimestampUs= */ 0,
        /* flags= */ BUFFER_FLAG_SYNC_FRAME);

    encoder.waitDeliverEncodedImage();

    ArgumentCaptor<EncodedImage> videoFrameCaptor = ArgumentCaptor.forClass(EncodedImage.class);
    verify(mockEncoderCallback)
        .onEncodedFrame(videoFrameCaptor.capture(), any(CodecSpecificInfo.class));

    EncodedImage videoFrame = videoFrameCaptor.getValue();
    assertThat(videoFrame).isNotNull();
    assertThat(videoFrame.qp).isNull();
  }

  @Test
  public void encodingStatistics_supported_enabled() throws InterruptedException {
    TestEncoder encoder = new TestEncoderBuilder().setIsEncodingStatisticsSupported(true).build();

    assertThat(encoder.initEncode(TEST_ENCODER_SETTINGS, mockEncoderCallback))
        .isEqualTo(VideoCodecStatus.OK);

    MediaFormat configuredFormat = fakeMediaCodecWrapper.getConfiguredFormat();
    assertThat(configuredFormat).isNotNull();
    assertThat(configuredFormat.containsKey(MediaFormat.KEY_VIDEO_ENCODING_STATISTICS_LEVEL))
        .isTrue();
    assertThat(configuredFormat.getInteger(MediaFormat.KEY_VIDEO_ENCODING_STATISTICS_LEVEL))
        .isEqualTo(MediaFormat.VIDEO_ENCODING_STATISTICS_LEVEL_1);

    // Verify that QP is set in encoded frame.
    MediaFormat outputFormat = new MediaFormat();
    outputFormat.setInteger(MediaFormat.KEY_VIDEO_QP_AVERAGE, 123);
    doReturn(outputFormat).when(fakeMediaCodecWrapper).getOutputFormat(anyInt());

    encoder.encode(createTestVideoFrame(/* timestampNs= */ 42), ENCODE_INFO_KEY_FRAME);

    fakeMediaCodecWrapper.addOutputData(CodecTestHelper.generateRandomData(100),
        /* presentationTimestampUs= */ 0,
        /* flags= */ BUFFER_FLAG_SYNC_FRAME);

    encoder.waitDeliverEncodedImage();

    ArgumentCaptor<EncodedImage> videoFrameCaptor = ArgumentCaptor.forClass(EncodedImage.class);
    verify(mockEncoderCallback)
        .onEncodedFrame(videoFrameCaptor.capture(), any(CodecSpecificInfo.class));

    EncodedImage videoFrame = videoFrameCaptor.getValue();
    assertThat(videoFrame).isNotNull();
    assertThat(videoFrame.qp).isEqualTo(123);
  }

  @Test
  public void encodingStatistics_fetchedBeforeFrameBufferIsReleased() throws InterruptedException {
    TestEncoder encoder =
        new TestEncoderBuilder().setCodecType(H264).setIsEncodingStatisticsSupported(true).build();
    assertThat(encoder.initEncode(TEST_ENCODER_SETTINGS, mockEncoderCallback))
        .isEqualTo(VideoCodecStatus.OK);

    encoder.encode(createTestVideoFrame(/* timestampNs= */ 42), ENCODE_INFO_KEY_FRAME);
    fakeMediaCodecWrapper.addOutputData(CodecTestHelper.generateRandomData(/* length= */ 100),
        /* presentationTimestampUs= */ 0,
        /* flags= */ MediaCodec.BUFFER_FLAG_CODEC_CONFIG);
    encoder.waitDeliverEncodedImage();

    int frameBufferIndex =
        fakeMediaCodecWrapper.addOutputData(CodecTestHelper.generateRandomData(/* length= */ 100),
            /* presentationTimestampUs= */ 0,
            /* flags= */ MediaCodec.BUFFER_FLAG_SYNC_FRAME);
    encoder.waitDeliverEncodedImage();

    InOrder inOrder = inOrder(fakeMediaCodecWrapper);
    inOrder.verify(fakeMediaCodecWrapper).getOutputFormat(eq(frameBufferIndex));
    inOrder.verify(fakeMediaCodecWrapper)
        .releaseOutputBuffer(eq(frameBufferIndex), /* render= */ eq(false));
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
    ByteBuffer buffer = fakeMediaCodecWrapper.getInputBuffer(indexCaptor.getValue());
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

  private void encodeWithConfigBuffer(VideoCodecMimeType codecType, boolean keyFrame,
      boolean emptyConfig, String expected) throws InterruptedException {
    String configData = emptyConfig ? "" : "config";
    byte[] configBytes = configData.getBytes(Charset.defaultCharset());
    byte[] frameBytes = "frame".getBytes(Charset.defaultCharset());
    byte[] expectedBytes = expected.getBytes(Charset.defaultCharset());

    TestEncoder encoder = new TestEncoderBuilder().setCodecType(codecType).build();
    encoder.initEncode(TEST_ENCODER_SETTINGS, mockEncoderCallback);

    encoder.encode(createTestVideoFrame(/* timestampNs= */ 0), ENCODE_INFO_DELTA_FRAME);

    fakeMediaCodecWrapper.addOutputData(
        configBytes, /* presentationTimestampUs= */ 0, /* flags= */ BUFFER_FLAG_CODEC_CONFIG);
    encoder.waitDeliverEncodedImage();

    fakeMediaCodecWrapper.addOutputData(frameBytes, /* presentationTimestampUs= */ 0,
        /* flags= */ keyFrame ? BUFFER_FLAG_SYNC_FRAME : 0);
    encoder.waitDeliverEncodedImage();

    verify(mockEncoderCallback)
        .onEncodedFrame(
            argThat(
                (EncodedImage encoded) -> encoded.buffer.equals(ByteBuffer.wrap(expectedBytes))),
            nullable(CodecSpecificInfo.class));

    assertThat(encoder.release()).isEqualTo(VideoCodecStatus.OK);
  }

  @Test
  public void encode_vp8KeyFrame_nonEmptyConfig_configNotPrepended() throws InterruptedException {
    encodeWithConfigBuffer(VP8, /*keyFrame=*/true, /* emptyConfig= */ false, "frame");
  }

  @Test
  public void encode_vp9KeyFrame_nonEmptyConfig_configNotPrepended() throws InterruptedException {
    encodeWithConfigBuffer(VP9, /*keyFrame=*/true, /* emptyConfig= */ false, "frame");
  }

  @Test
  public void encode_av1KeyFrame_nonEmptyConfig_configNotPrepended() throws InterruptedException {
    encodeWithConfigBuffer(AV1, /*keyFrame=*/true, /* emptyConfig= */ false, "frame");
  }

  @Test
  public void encode_h264KeyFrame_nonEmptyConfig_configPrepended() throws InterruptedException {
    encodeWithConfigBuffer(H264, /*keyFrame=*/true, /* emptyConfig= */ false, "configframe");
  }

  @Test
  public void encode_h265KeyFrame_nonEmptyConfig_configPrepended() throws InterruptedException {
    encodeWithConfigBuffer(H265, /*keyFrame=*/true, /* emptyConfig= */ false, "configframe");
  }

  @Test
  public void encode_vp8DeltaFrame_nonEmptyConfig_configNotPrepended() throws InterruptedException {
    encodeWithConfigBuffer(VP8, /*keyFrame=*/false, /* emptyConfig= */ false, "frame");
  }

  @Test
  public void encode_vp9DeltaFrame_nonEmptyConfig_configNotPrepended() throws InterruptedException {
    encodeWithConfigBuffer(VP9, /*keyFrame=*/false, /* emptyConfig= */ false, "frame");
  }

  @Test
  public void encode_av1DeltaFrame_nonEmptyConfig_configNotPrepended() throws InterruptedException {
    encodeWithConfigBuffer(AV1, /*keyFrame=*/false, /* emptyConfig= */ false, "frame");
  }

  @Test
  public void encode_h264KeyFrame_emptyConfig_configNotPrepended() throws InterruptedException {
    encodeWithConfigBuffer(H264, /*keyFrame=*/true, /* emptyConfig= */ true, "frame");
  }

  @Test
  public void encode_h265KeyFrame_emptyConfig_configNotPrepended() throws InterruptedException {
    encodeWithConfigBuffer(H265, /*keyFrame=*/true, /* emptyConfig= */ true, "frame");
  }

  private void encodeWithStride(int colorFormat, int stride, int sliceHeight,
      int expectedBufferSize) throws InterruptedException {
    MediaFormat inputFormat = new MediaFormat();
    inputFormat.setInteger(MediaFormat.KEY_STRIDE, stride);
    inputFormat.setInteger(MediaFormat.KEY_SLICE_HEIGHT, sliceHeight);
    doReturn(inputFormat).when(fakeMediaCodecWrapper).getInputFormat();

    ByteBuffer inputBuffer = ByteBuffer.allocateDirect(calcBufferSize(
        colorFormat, HEIGHT, Math.max(stride, WIDTH), Math.max(sliceHeight, HEIGHT)));
    doReturn(inputBuffer).when(fakeMediaCodecWrapper).getInputBuffer(anyInt());

    TestEncoder encoder = new TestEncoderBuilder().setColorFormat(colorFormat).build();
    encoder.initEncode(TEST_ENCODER_SETTINGS, mockEncoderCallback);
    encoder.encode(createTestVideoFrame(/* timestampNs= */ 0), ENCODE_INFO_DELTA_FRAME);

    verify(fakeMediaCodecWrapper)
        .queueInputBuffer(
            /*index=*/anyInt(), /*offset=*/eq(0), /*size=*/eq(expectedBufferSize),
            /*presentationTimeUs=*/anyLong(), /*flags=*/anyInt());
  }

  @Test
  public void encode_invalidStride_planar_ignored() throws InterruptedException {
    encodeWithStride(/*colorFormat=*/COLOR_FormatYUV420Planar,
        /*stride=*/WIDTH / 2,
        /*sliceHeight=*/HEIGHT,
        /*expectedBufferSize=*/WIDTH * HEIGHT * 3 / 2);
  }

  @Test
  public void encode_invalidSliceHeight_planar_ignored() throws InterruptedException {
    encodeWithStride(/*colorFormat=*/COLOR_FormatYUV420Planar,
        /*stride=*/WIDTH,
        /*sliceHeight=*/HEIGHT / 2,
        /*expectedBufferSize=*/WIDTH * HEIGHT * 3 / 2);
  }

  @Test
  public void encode_validStride_planar_applied() throws InterruptedException {
    encodeWithStride(/*colorFormat=*/COLOR_FormatYUV420Planar,
        /*stride=*/WIDTH * 2,
        /*sliceHeight=*/HEIGHT,
        /*expectedBufferSize=*/WIDTH * 2 * HEIGHT * 3 / 2);
  }

  @Test
  public void encode_validSliceHeight_planar_applied() throws InterruptedException {
    encodeWithStride(/*colorFormat=*/COLOR_FormatYUV420Planar,
        /*stride=*/WIDTH,
        /*sliceHeight=*/HEIGHT * 2,
        /*expectedBufferSize=*/WIDTH * HEIGHT * 2 * 3 / 2);
  }

  @Test
  public void encode_validStride_semiPlanar_applied() throws InterruptedException {
    encodeWithStride(/*colorFormat=*/COLOR_FormatYUV420SemiPlanar,
        /*stride=*/WIDTH * 2,
        /*sliceHeight=*/HEIGHT,
        /*expectedBufferSize=*/WIDTH * 2 * HEIGHT * 3 / 2);
  }

  @Test
  public void encode_validSliceHeight_semiPlanar_applied() throws InterruptedException {
    encodeWithStride(/*colorFormat=*/COLOR_FormatYUV420SemiPlanar,
        /*stride=*/WIDTH,
        /*sliceHeight=*/HEIGHT * 2,
        /*expectedBufferSize=*/WIDTH * HEIGHT * 2 + WIDTH * HEIGHT / 2);
  }

  /** Returns buffer size in bytes for the given color format and dimensions. */
  private int calcBufferSize(int colorFormat, int height, int stride, int sliceHeight) {
    if (colorFormat == COLOR_FormatYUV420SemiPlanar) {
      int chromaHeight = (height + 1) / 2;
      return sliceHeight * stride + chromaHeight * stride;
    }
    int chromaStride = (stride + 1) / 2;
    int chromaSliceHeight = (sliceHeight + 1) / 2;
    return sliceHeight * stride + chromaSliceHeight * chromaStride * 2;
  }
}
