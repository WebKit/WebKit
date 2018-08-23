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

import android.graphics.SurfaceTexture;
import android.media.MediaCodec;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCrypto;
import android.media.MediaFormat;
import android.os.Bundle;
import android.view.Surface;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;
import javax.annotation.Nullable;

/**
 * Fake MediaCodec that implements the basic state machine.
 *
 * @note This class is only intended for single-threaded tests and is not thread-safe.
 */
public class FakeMediaCodecWrapper implements MediaCodecWrapper {
  private static final int NUM_INPUT_BUFFERS = 10;
  private static final int NUM_OUTPUT_BUFFERS = 10;
  private static final int MAX_ENCODED_DATA_SIZE_BYTES = 1_000;

  /**
   * MediaCodec state as defined by:
   * https://developer.android.com/reference/android/media/MediaCodec.html
   */
  public enum State {
    STOPPED_CONFIGURED(Primary.STOPPED),
    STOPPED_UNINITIALIZED(Primary.STOPPED),
    STOPPED_ERROR(Primary.STOPPED),
    EXECUTING_FLUSHED(Primary.EXECUTING),
    EXECUTING_RUNNING(Primary.EXECUTING),
    EXECUTING_END_OF_STREAM(Primary.EXECUTING),
    RELEASED(Primary.RELEASED);

    public enum Primary { STOPPED, EXECUTING, RELEASED }

    private final Primary primary;

    State(Primary primary) {
      this.primary = primary;
    }

    public Primary getPrimary() {
      return primary;
    }
  }

  /** Represents an output buffer that will be returned by dequeueOutputBuffer. */
  public static class QueuedOutputBufferInfo {
    private int index;
    private int offset;
    private int size;
    private long presentationTimeUs;
    private int flags;

    private QueuedOutputBufferInfo(
        int index, int offset, int size, long presentationTimeUs, int flags) {
      this.index = index;
      this.offset = offset;
      this.size = size;
      this.presentationTimeUs = presentationTimeUs;
      this.flags = flags;
    }

    public static QueuedOutputBufferInfo create(
        int index, int offset, int size, long presentationTimeUs, int flags) {
      return new QueuedOutputBufferInfo(index, offset, size, presentationTimeUs, flags);
    }

    public int getIndex() {
      return index;
    }

    public int getOffset() {
      return offset;
    }

    public int getSize() {
      return size;
    }

    public long getPresentationTimeUs() {
      return presentationTimeUs;
    }

    public int getFlags() {
      return flags;
    }
  }

  private State state = State.STOPPED_UNINITIALIZED;
  private @Nullable MediaFormat configuredFormat;
  private int configuredFlags;
  private final MediaFormat outputFormat;
  private final ByteBuffer[] inputBuffers = new ByteBuffer[NUM_INPUT_BUFFERS];
  private final ByteBuffer[] outputBuffers = new ByteBuffer[NUM_OUTPUT_BUFFERS];
  private final boolean[] inputBufferReserved = new boolean[NUM_INPUT_BUFFERS];
  private final boolean[] outputBufferReserved = new boolean[NUM_OUTPUT_BUFFERS];
  private final List<QueuedOutputBufferInfo> queuedOutputBuffers = new ArrayList<>();

  public FakeMediaCodecWrapper(MediaFormat outputFormat) {
    this.outputFormat = outputFormat;
  }

  /** Returns the current simulated state of MediaCodec. */
  public State getState() {
    return state;
  }

  /** Gets the last configured media format passed to configure. */
  public @Nullable MediaFormat getConfiguredFormat() {
    return configuredFormat;
  }

  /** Returns the last flags passed to configure. */
  public int getConfiguredFlags() {
    return configuredFlags;
  }

  /**
   * Adds a texture buffer that will be returned by dequeueOutputBuffer. Returns index of the
   * buffer.
   */
  public int addOutputTexture(long presentationTimestampUs, int flags) {
    int index = getFreeOutputBuffer();
    queuedOutputBuffers.add(QueuedOutputBufferInfo.create(
        index, /* offset= */ 0, /* size= */ 0, presentationTimestampUs, flags));
    return index;
  }

  /**
   * Adds a byte buffer buffer that will be returned by dequeueOutputBuffer. Returns index of the
   * buffer.
   */
  public int addOutputData(byte[] data, long presentationTimestampUs, int flags) {
    int index = getFreeOutputBuffer();
    ByteBuffer outputBuffer = outputBuffers[index];

    outputBuffer.clear();
    outputBuffer.put(data);
    outputBuffer.rewind();

    queuedOutputBuffers.add(QueuedOutputBufferInfo.create(
        index, /* offset= */ 0, data.length, presentationTimestampUs, flags));
    return index;
  }

  /**
   * Returns the first output buffer that is not reserved and reserves it. It will be stay reserved
   * until released with releaseOutputBuffer.
   */
  private int getFreeOutputBuffer() {
    for (int i = 0; i < NUM_OUTPUT_BUFFERS; i++) {
      if (!outputBufferReserved[i]) {
        outputBufferReserved[i] = true;
        return i;
      }
    }
    throw new RuntimeException("All output buffers reserved!");
  }

  @Override
  public void configure(MediaFormat format, Surface surface, MediaCrypto crypto, int flags) {
    if (state != State.STOPPED_UNINITIALIZED) {
      throw new IllegalStateException("Expected state STOPPED_UNINITIALIZED but was " + state);
    }
    state = State.STOPPED_CONFIGURED;
    configuredFormat = format;
    configuredFlags = flags;

    final int width = configuredFormat.getInteger(MediaFormat.KEY_WIDTH);
    final int height = configuredFormat.getInteger(MediaFormat.KEY_HEIGHT);
    final int yuvSize = width * height * 3 / 2;
    final int inputBufferSize;
    final int outputBufferSize;

    if ((flags & MediaCodec.CONFIGURE_FLAG_ENCODE) != 0) {
      final int colorFormat = configuredFormat.getInteger(MediaFormat.KEY_COLOR_FORMAT);

      inputBufferSize = colorFormat == CodecCapabilities.COLOR_FormatSurface ? 0 : yuvSize;
      outputBufferSize = MAX_ENCODED_DATA_SIZE_BYTES;
    } else {
      inputBufferSize = MAX_ENCODED_DATA_SIZE_BYTES;
      outputBufferSize = surface != null ? 0 : yuvSize;
    }

    for (int i = 0; i < inputBuffers.length; i++) {
      inputBuffers[i] = ByteBuffer.allocateDirect(inputBufferSize);
    }
    for (int i = 0; i < outputBuffers.length; i++) {
      outputBuffers[i] = ByteBuffer.allocateDirect(outputBufferSize);
    }
  }

  @Override
  public void start() {
    if (state != State.STOPPED_CONFIGURED) {
      throw new IllegalStateException("Expected state STOPPED_CONFIGURED but was " + state);
    }
    state = State.EXECUTING_RUNNING;
  }

  @Override
  public void flush() {
    if (state.getPrimary() != State.Primary.EXECUTING) {
      throw new IllegalStateException("Expected state EXECUTING but was " + state);
    }
    state = State.EXECUTING_FLUSHED;
  }

  @Override
  public void stop() {
    if (state.getPrimary() != State.Primary.EXECUTING) {
      throw new IllegalStateException("Expected state EXECUTING but was " + state);
    }
    state = State.STOPPED_UNINITIALIZED;
  }

  @Override
  public void release() {
    state = State.RELEASED;
  }

  @Override
  public int dequeueInputBuffer(long timeoutUs) {
    if (state != State.EXECUTING_FLUSHED && state != State.EXECUTING_RUNNING) {
      throw new IllegalStateException(
          "Expected state EXECUTING_FLUSHED or EXECUTING_RUNNING but was " + state);
    }
    state = State.EXECUTING_RUNNING;

    for (int i = 0; i < NUM_INPUT_BUFFERS; i++) {
      if (!inputBufferReserved[i]) {
        inputBufferReserved[i] = true;
        return i;
      }
    }
    return MediaCodec.INFO_TRY_AGAIN_LATER;
  }

  @Override
  public void queueInputBuffer(
      int index, int offset, int size, long presentationTimeUs, int flags) {
    if (state.getPrimary() != State.Primary.EXECUTING) {
      throw new IllegalStateException("Expected state EXECUTING but was " + state);
    }
    if (flags != 0) {
      throw new UnsupportedOperationException(
          "Flags are not implemented in FakeMediaCodecWrapper.");
    }
  }

  @Override
  public int dequeueOutputBuffer(MediaCodec.BufferInfo info, long timeoutUs) {
    if (state.getPrimary() != State.Primary.EXECUTING) {
      throw new IllegalStateException("Expected state EXECUTING but was " + state);
    }

    if (queuedOutputBuffers.isEmpty()) {
      return MediaCodec.INFO_TRY_AGAIN_LATER;
    }
    QueuedOutputBufferInfo outputBufferInfo = queuedOutputBuffers.remove(/* index= */ 0);
    info.set(outputBufferInfo.getOffset(), outputBufferInfo.getSize(),
        outputBufferInfo.getPresentationTimeUs(), outputBufferInfo.getFlags());
    return outputBufferInfo.getIndex();
  }

  @Override
  public void releaseOutputBuffer(int index, boolean render) {
    if (state.getPrimary() != State.Primary.EXECUTING) {
      throw new IllegalStateException("Expected state EXECUTING but was " + state);
    }
    if (!outputBufferReserved[index]) {
      throw new RuntimeException("Released output buffer was not in use.");
    }
    outputBufferReserved[index] = false;
  }

  @Override
  public ByteBuffer[] getInputBuffers() {
    return inputBuffers;
  }

  @Override
  public ByteBuffer[] getOutputBuffers() {
    return outputBuffers;
  }

  @Override
  public MediaFormat getOutputFormat() {
    return outputFormat;
  }

  @Override
  public Surface createInputSurface() {
    return new Surface(new SurfaceTexture(/* texName= */ 0));
  }

  @Override
  public void setParameters(Bundle params) {}
}
