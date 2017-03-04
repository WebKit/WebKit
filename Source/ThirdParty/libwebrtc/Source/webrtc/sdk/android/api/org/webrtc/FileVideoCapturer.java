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

import android.content.Context;
import android.os.SystemClock;

import java.util.concurrent.TimeUnit;
import java.util.Timer;
import java.util.TimerTask;
import java.io.RandomAccessFile;
import java.io.IOException;

public class FileVideoCapturer implements VideoCapturer {
  private interface VideoReader {
    int getFrameWidth();
    int getFrameHeight();
    byte[] getNextFrame();
    void close();
  }

  /**
   * Read video data from file for the .y4m container.
   */
  private static class VideoReaderY4M implements VideoReader {
    private final static String TAG = "VideoReaderY4M";
    private final int frameWidth;
    private final int frameHeight;
    private final int frameSize;

    // First char after header
    private final long videoStart;

    private static final String Y4M_FRAME_DELIMETER = "FRAME";

    private final RandomAccessFile mediaFileStream;

    public int getFrameWidth() {
      return frameWidth;
    }

    public int getFrameHeight() {
      return frameHeight;
    }

    public VideoReaderY4M(String file) throws IOException {
      mediaFileStream = new RandomAccessFile(file, "r");
      StringBuilder builder = new StringBuilder();
      for (;;) {
        int c = mediaFileStream.read();
        if (c == -1) {
          // End of file reached.
          throw new RuntimeException("Found end of file before end of header for file: " + file);
        }
        if (c == '\n') {
          // End of header found.
          break;
        }
        builder.append((char) c);
      }
      videoStart = mediaFileStream.getFilePointer();
      String header = builder.toString();
      String[] headerTokens = header.split("[ ]");
      int w = 0;
      int h = 0;
      String colorSpace = "";
      for (String tok : headerTokens) {
        char c = tok.charAt(0);
        switch (c) {
          case 'W':
            w = Integer.parseInt(tok.substring(1));
            break;
          case 'H':
            h = Integer.parseInt(tok.substring(1));
            break;
          case 'C':
            colorSpace = tok.substring(1);
            break;
        }
      }
      Logging.d(TAG, "Color space: " + colorSpace);
      if (!colorSpace.equals("420") && !colorSpace.equals("420mpeg2")) {
        throw new IllegalArgumentException(
            "Does not support any other color space than I420 or I420mpeg2");
      }
      if ((w % 2) == 1 || (h % 2) == 1) {
        throw new IllegalArgumentException("Does not support odd width or height");
      }
      frameWidth = w;
      frameHeight = h;
      frameSize = w * h * 3 / 2;
      Logging.d(TAG, "frame dim: (" + w + ", " + h + ") frameSize: " + frameSize);
    }

    public byte[] getNextFrame() {
      byte[] frame = new byte[frameSize];
      try {
        byte[] frameDelim = new byte[Y4M_FRAME_DELIMETER.length() + 1];
        if (mediaFileStream.read(frameDelim) < frameDelim.length) {
          // We reach end of file, loop
          mediaFileStream.seek(videoStart);
          if (mediaFileStream.read(frameDelim) < frameDelim.length) {
            throw new RuntimeException("Error looping video");
          }
        }
        String frameDelimStr = new String(frameDelim);
        if (!frameDelimStr.equals(Y4M_FRAME_DELIMETER + "\n")) {
          throw new RuntimeException(
              "Frames should be delimited by FRAME plus newline, found delimter was: '"
              + frameDelimStr + "'");
        }
        mediaFileStream.readFully(frame);
        byte[] nv21Frame = new byte[frameSize];
        nativeI420ToNV21(frame, frameWidth, frameHeight, nv21Frame);
        return nv21Frame;
      } catch (IOException e) {
        throw new RuntimeException(e);
      }
    }

    public void close() {
      try {
        mediaFileStream.close();
      } catch (IOException e) {
        Logging.e(TAG, "Problem closing file", e);
      }
    }
  }

  private final static String TAG = "FileVideoCapturer";
  private final VideoReader videoReader;
  private CapturerObserver capturerObserver;
  private final Timer timer = new Timer();

  private final TimerTask tickTask = new TimerTask() {
    @Override
    public void run() {
      tick();
    }
  };

  private int getFrameWidth() {
    return videoReader.getFrameWidth();
  }

  private int getFrameHeight() {
    return videoReader.getFrameHeight();
  }

  public FileVideoCapturer(String inputFile) throws IOException {
    try {
      videoReader = new VideoReaderY4M(inputFile);
    } catch (IOException e) {
      Logging.d(TAG, "Could not open video file: " + inputFile);
      throw e;
    }
  }

  private byte[] getNextFrame() {
    return videoReader.getNextFrame();
  }

  public void tick() {
    final long captureTimeNs = TimeUnit.MILLISECONDS.toNanos(SystemClock.elapsedRealtime());

    byte[] frameData = getNextFrame();
    capturerObserver.onByteBufferFrameCaptured(
        frameData, getFrameWidth(), getFrameHeight(), 0, captureTimeNs);
  }

  @Override
  public void initialize(SurfaceTextureHelper surfaceTextureHelper, Context applicationContext,
      CapturerObserver capturerObserver) {
    this.capturerObserver = capturerObserver;
  }

  @Override
  public void startCapture(int width, int height, int framerate) {
    timer.schedule(tickTask, 0, 1000 / framerate);
  }

  @Override
  public void stopCapture() throws InterruptedException {
    timer.cancel();
  }

  @Override
  public void changeCaptureFormat(int width, int height, int framerate) {
    // Empty on purpose
  }

  @Override
  public void dispose() {
    videoReader.close();
  }

  @Override
  public boolean isScreencast() {
    return false;
  }

  public static native void nativeI420ToNV21(byte[] src, int width, int height, byte[] dst);
}
