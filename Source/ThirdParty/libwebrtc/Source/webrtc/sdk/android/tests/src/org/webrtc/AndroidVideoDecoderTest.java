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
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Matrix;
import android.graphics.SurfaceTexture;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaFormat;
import android.os.Handler;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.webrtc.EncodedImage.FrameType;
import org.webrtc.FakeMediaCodecWrapper.State;
import org.webrtc.VideoDecoder.DecodeInfo;
import org.webrtc.VideoFrame.I420Buffer;
import org.webrtc.VideoFrame.TextureBuffer.Type;

@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AndroidVideoDecoderTest {
  private static final VideoDecoder.Settings TEST_DECODER_SETTINGS =
      new VideoDecoder.Settings(/* numberOfCores= */ 1, /* width= */ 640, /* height= */ 480);
  private static final int COLOR_FORMAT = CodecCapabilities.COLOR_FormatYUV420Planar;
  private static final long POLL_DELAY_MS = 10;
  private static final long DELIVER_DECODED_IMAGE_DELAY_MS = 10;

  private static final byte[] ENCODED_TEST_DATA = new byte[] {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

  private class TestDecoder extends AndroidVideoDecoder {
    private final Object deliverDecodedFrameLock = new Object();
    private boolean deliverDecodedFrameDone = true;

    public TestDecoder(MediaCodecWrapperFactory mediaCodecFactory, String codecName,
        VideoCodecMimeType codecType, int colorFormat, EglBase.Context sharedContext) {
      super(mediaCodecFactory, codecName, codecType, colorFormat, sharedContext);
    }

    public void waitDeliverDecodedFrame() throws InterruptedException {
      synchronized (deliverDecodedFrameLock) {
        deliverDecodedFrameDone = false;
        deliverDecodedFrameLock.notifyAll();
        while (!deliverDecodedFrameDone) {
          deliverDecodedFrameLock.wait();
        }
      }
    }

    @SuppressWarnings("WaitNotInLoop") // This method is called inside a loop.
    @Override
    protected void deliverDecodedFrame() {
      synchronized (deliverDecodedFrameLock) {
        if (deliverDecodedFrameDone) {
          try {
            deliverDecodedFrameLock.wait(DELIVER_DECODED_IMAGE_DELAY_MS);
          } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            return;
          }
        }
        if (deliverDecodedFrameDone) {
          return;
        }
        super.deliverDecodedFrame();
        deliverDecodedFrameDone = true;
        deliverDecodedFrameLock.notifyAll();
      }
    }

    @Override
    protected SurfaceTextureHelper createSurfaceTextureHelper() {
      return mockSurfaceTextureHelper;
    }

    @Override
    protected void releaseSurface() {}

    @Override
    protected VideoFrame.I420Buffer allocateI420Buffer(int width, int height) {
      int chromaHeight = (height + 1) / 2;
      int strideUV = (width + 1) / 2;
      int yPos = 0;
      int uPos = yPos + width * height;
      int vPos = uPos + strideUV * chromaHeight;

      ByteBuffer buffer = ByteBuffer.allocateDirect(width * height + 2 * strideUV * chromaHeight);

      buffer.position(yPos);
      buffer.limit(uPos);
      ByteBuffer dataY = buffer.slice();

      buffer.position(uPos);
      buffer.limit(vPos);
      ByteBuffer dataU = buffer.slice();

      buffer.position(vPos);
      buffer.limit(vPos + strideUV * chromaHeight);
      ByteBuffer dataV = buffer.slice();

      return JavaI420Buffer.wrap(width, height, dataY, width, dataU, strideUV, dataV, strideUV,
          /* releaseCallback= */ null);
    }

    @Override
    protected void copyPlane(
        ByteBuffer src, int srcStride, ByteBuffer dst, int dstStride, int width, int height) {
      for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
          dst.put(y * dstStride + x, src.get(y * srcStride + x));
        }
      }
    }
  }

  private class TestDecoderBuilder {
    private VideoCodecMimeType codecType = VideoCodecMimeType.VP8;
    private boolean useSurface = true;

    public TestDecoderBuilder setCodecType(VideoCodecMimeType codecType) {
      this.codecType = codecType;
      return this;
    }

    public TestDecoderBuilder setUseSurface(boolean useSurface) {
      this.useSurface = useSurface;
      return this;
    }

    public TestDecoder build() {
      return new TestDecoder((String name)
                                 -> fakeMediaCodecWrapper,
          /* codecName= */ "org.webrtc.testdecoder", codecType, COLOR_FORMAT,
          useSurface ? mockEglBaseContext : null);
    }
  }

  private static class FakeDecoderCallback implements VideoDecoder.Callback {
    public final List<VideoFrame> decodedFrames;

    public FakeDecoderCallback() {
      decodedFrames = new ArrayList<>();
    }

    @Override
    public void onDecodedFrame(VideoFrame frame, Integer decodeTimeMs, Integer qp) {
      frame.retain();
      decodedFrames.add(frame);
    }

    public void release() {
      for (VideoFrame frame : decodedFrames) frame.release();
      decodedFrames.clear();
    }
  }

  private EncodedImage createTestEncodedImage() {
    return EncodedImage.builder()
        .setBuffer(ByteBuffer.wrap(ENCODED_TEST_DATA), null)
        .setFrameType(FrameType.VideoFrameKey)
        .createEncodedImage();
  }

  @Mock private EglBase.Context mockEglBaseContext;
  @Mock private SurfaceTextureHelper mockSurfaceTextureHelper;
  @Mock private VideoDecoder.Callback mockDecoderCallback;
  private FakeMediaCodecWrapper fakeMediaCodecWrapper;
  private FakeDecoderCallback fakeDecoderCallback;

  @Before
  public void setUp() {
    MockitoAnnotations.initMocks(this);
    when(mockSurfaceTextureHelper.getSurfaceTexture())
        .thenReturn(new SurfaceTexture(/*texName=*/0));
    MediaFormat outputFormat = new MediaFormat();
    // TODO(sakal): Add more details to output format as needed.
    fakeMediaCodecWrapper = spy(new FakeMediaCodecWrapper(outputFormat));
    fakeDecoderCallback = new FakeDecoderCallback();
  }

  @After
  public void cleanUp() {
    fakeDecoderCallback.release();
  }

  @Test
  public void testInit() {
    // Set-up.
    AndroidVideoDecoder decoder =
        new TestDecoderBuilder().setCodecType(VideoCodecMimeType.VP8).build();

    // Test.
    assertThat(decoder.initDecode(TEST_DECODER_SETTINGS, mockDecoderCallback))
        .isEqualTo(VideoCodecStatus.OK);

    // Verify.
    assertThat(fakeMediaCodecWrapper.getState()).isEqualTo(State.EXECUTING_RUNNING);

    MediaFormat mediaFormat = fakeMediaCodecWrapper.getConfiguredFormat();
    assertThat(mediaFormat).isNotNull();
    assertThat(mediaFormat.getInteger(MediaFormat.KEY_WIDTH))
        .isEqualTo(TEST_DECODER_SETTINGS.width);
    assertThat(mediaFormat.getInteger(MediaFormat.KEY_HEIGHT))
        .isEqualTo(TEST_DECODER_SETTINGS.height);
    assertThat(mediaFormat.getString(MediaFormat.KEY_MIME))
        .isEqualTo(VideoCodecMimeType.VP8.mimeType());
  }

  @Test
  public void testRelease() {
    // Set-up.
    AndroidVideoDecoder decoder = new TestDecoderBuilder().build();
    decoder.initDecode(TEST_DECODER_SETTINGS, mockDecoderCallback);

    // Test.
    assertThat(decoder.release()).isEqualTo(VideoCodecStatus.OK);

    // Verify.
    assertThat(fakeMediaCodecWrapper.getState()).isEqualTo(State.RELEASED);
  }

  @Test
  public void testReleaseMultipleTimes() {
    // Set-up.
    AndroidVideoDecoder decoder = new TestDecoderBuilder().build();
    decoder.initDecode(TEST_DECODER_SETTINGS, mockDecoderCallback);

    // Test.
    assertThat(decoder.release()).isEqualTo(VideoCodecStatus.OK);
    assertThat(decoder.release()).isEqualTo(VideoCodecStatus.OK);

    // Verify.
    assertThat(fakeMediaCodecWrapper.getState()).isEqualTo(State.RELEASED);
  }

  @Test
  public void testDecodeQueuesData() {
    // Set-up.
    AndroidVideoDecoder decoder = new TestDecoderBuilder().build();
    decoder.initDecode(TEST_DECODER_SETTINGS, mockDecoderCallback);

    // Test.
    assertThat(decoder.decode(createTestEncodedImage(),
                   new DecodeInfo(/* isMissingFrames= */ false, /* renderTimeMs= */ 0)))
        .isEqualTo(VideoCodecStatus.OK);

    // Verify.
    ArgumentCaptor<Integer> indexCaptor = ArgumentCaptor.forClass(Integer.class);
    ArgumentCaptor<Integer> offsetCaptor = ArgumentCaptor.forClass(Integer.class);
    ArgumentCaptor<Integer> sizeCaptor = ArgumentCaptor.forClass(Integer.class);
    verify(fakeMediaCodecWrapper)
        .queueInputBuffer(indexCaptor.capture(), offsetCaptor.capture(), sizeCaptor.capture(),
            /* presentationTimeUs= */ anyLong(),
            /* flags= */ eq(0));

    ByteBuffer inputBuffer = fakeMediaCodecWrapper.getInputBuffers()[indexCaptor.getValue()];
    CodecTestHelper.assertEqualContents(
        ENCODED_TEST_DATA, inputBuffer, offsetCaptor.getValue(), sizeCaptor.getValue());
  }

  @Test
  public void testDeliversOutputByteBuffers() throws InterruptedException {
    final byte[] testOutputData = CodecTestHelper.generateRandomData(
        TEST_DECODER_SETTINGS.width * TEST_DECODER_SETTINGS.height * 3 / 2);
    final I420Buffer expectedDeliveredBuffer = CodecTestHelper.wrapI420(
        TEST_DECODER_SETTINGS.width, TEST_DECODER_SETTINGS.height, testOutputData);

    // Set-up.
    TestDecoder decoder = new TestDecoderBuilder().setUseSurface(/* useSurface = */ false).build();
    decoder.initDecode(TEST_DECODER_SETTINGS, fakeDecoderCallback);
    decoder.decode(createTestEncodedImage(),
        new DecodeInfo(/* isMissingFrames= */ false, /* renderTimeMs= */ 0));
    fakeMediaCodecWrapper.addOutputData(
        testOutputData, /* presentationTimestampUs= */ 0, /* flags= */ 0);

    // Test.
    decoder.waitDeliverDecodedFrame();

    // Verify.
    assertThat(fakeDecoderCallback.decodedFrames).hasSize(1);
    VideoFrame videoFrame = fakeDecoderCallback.decodedFrames.get(0);
    assertThat(videoFrame).isNotNull();
    assertThat(videoFrame.getRotatedWidth()).isEqualTo(TEST_DECODER_SETTINGS.width);
    assertThat(videoFrame.getRotatedHeight()).isEqualTo(TEST_DECODER_SETTINGS.height);
    assertThat(videoFrame.getRotation()).isEqualTo(0);
    I420Buffer deliveredBuffer = videoFrame.getBuffer().toI420();
    assertThat(deliveredBuffer.getDataY()).isEqualTo(expectedDeliveredBuffer.getDataY());
    assertThat(deliveredBuffer.getDataU()).isEqualTo(expectedDeliveredBuffer.getDataU());
    assertThat(deliveredBuffer.getDataV()).isEqualTo(expectedDeliveredBuffer.getDataV());
  }

  @Test
  public void testRendersOutputTexture() throws InterruptedException {
    // Set-up.
    TestDecoder decoder = new TestDecoderBuilder().build();
    decoder.initDecode(TEST_DECODER_SETTINGS, mockDecoderCallback);
    decoder.decode(createTestEncodedImage(),
        new DecodeInfo(/* isMissingFrames= */ false, /* renderTimeMs= */ 0));
    int bufferIndex =
        fakeMediaCodecWrapper.addOutputTexture(/* presentationTimestampUs= */ 0, /* flags= */ 0);

    // Test.
    decoder.waitDeliverDecodedFrame();

    // Verify.
    verify(fakeMediaCodecWrapper).releaseOutputBuffer(bufferIndex, /* render= */ true);
  }

  @Test
  public void testSurfaceTextureStall_FramesDropped() throws InterruptedException {
    final int numFrames = 10;
    // Maximum number of frame the decoder can keep queued on the output side.
    final int maxQueuedBuffers = 3;

    // Set-up.
    TestDecoder decoder = new TestDecoderBuilder().build();
    decoder.initDecode(TEST_DECODER_SETTINGS, mockDecoderCallback);

    // Test.
    int[] bufferIndices = new int[numFrames];
    for (int i = 0; i < 10; i++) {
      decoder.decode(createTestEncodedImage(),
          new DecodeInfo(/* isMissingFrames= */ false, /* renderTimeMs= */ 0));
      bufferIndices[i] =
          fakeMediaCodecWrapper.addOutputTexture(/* presentationTimestampUs= */ 0, /* flags= */ 0);
      decoder.waitDeliverDecodedFrame();
    }

    // Verify.
    InOrder releaseOrder = inOrder(fakeMediaCodecWrapper);
    releaseOrder.verify(fakeMediaCodecWrapper)
        .releaseOutputBuffer(bufferIndices[0], /* render= */ true);
    for (int i = 1; i < numFrames - maxQueuedBuffers; i++) {
      releaseOrder.verify(fakeMediaCodecWrapper)
          .releaseOutputBuffer(bufferIndices[i], /* render= */ false);
    }
  }

  @Test
  public void testDeliversRenderedBuffers() throws InterruptedException {
    // Set-up.
    TestDecoder decoder = new TestDecoderBuilder().build();
    decoder.initDecode(TEST_DECODER_SETTINGS, fakeDecoderCallback);
    decoder.decode(createTestEncodedImage(),
        new DecodeInfo(/* isMissingFrames= */ false, /* renderTimeMs= */ 0));
    fakeMediaCodecWrapper.addOutputTexture(/* presentationTimestampUs= */ 0, /* flags= */ 0);

    // Render the output buffer.
    decoder.waitDeliverDecodedFrame();

    ArgumentCaptor<VideoSink> videoSinkCaptor = ArgumentCaptor.forClass(VideoSink.class);
    verify(mockSurfaceTextureHelper).startListening(videoSinkCaptor.capture());

    // Test.
    Runnable releaseCallback = mock(Runnable.class);
    VideoFrame.TextureBuffer outputTextureBuffer =
        new TextureBufferImpl(TEST_DECODER_SETTINGS.width, TEST_DECODER_SETTINGS.height, Type.OES,
            /* id= */ 0,
            /* transformMatrix= */ new Matrix(),
            /* toI420Handler= */ new Handler(), new YuvConverter(), releaseCallback);
    VideoFrame outputVideoFrame =
        new VideoFrame(outputTextureBuffer, /* rotation= */ 0, /* timestampNs= */ 0);
    videoSinkCaptor.getValue().onFrame(outputVideoFrame);
    outputVideoFrame.release();

    // Verify.
    assertThat(fakeDecoderCallback.decodedFrames).hasSize(1);
    VideoFrame videoFrame = fakeDecoderCallback.decodedFrames.get(0);
    assertThat(videoFrame).isNotNull();
    assertThat(videoFrame.getBuffer()).isEqualTo(outputTextureBuffer);

    fakeDecoderCallback.release();

    verify(releaseCallback).run();
  }

  @Test
  public void testConfigureExceptionTriggerSWFallback() {
    // Set-up.
    doThrow(new IllegalStateException("Fake error"))
        .when(fakeMediaCodecWrapper)
        .configure(any(), any(), any(), anyInt());

    AndroidVideoDecoder decoder = new TestDecoderBuilder().build();

    // Test.
    assertThat(decoder.initDecode(TEST_DECODER_SETTINGS, mockDecoderCallback))
        .isEqualTo(VideoCodecStatus.FALLBACK_SOFTWARE);
  }

  @Test
  public void testStartExceptionTriggerSWFallback() {
    // Set-up.
    doThrow(new IllegalStateException("Fake error")).when(fakeMediaCodecWrapper).start();

    AndroidVideoDecoder decoder = new TestDecoderBuilder().build();

    // Test.
    assertThat(decoder.initDecode(TEST_DECODER_SETTINGS, mockDecoderCallback))
        .isEqualTo(VideoCodecStatus.FALLBACK_SOFTWARE);
  }
}
