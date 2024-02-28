package org.aomedia.avif.android;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import androidx.test.platform.app.InstrumentationRegistry;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.channels.Channels;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;
import org.aomedia.avif.android.AvifDecoder.Info;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameter;
import org.junit.runners.Parameterized.Parameters;

/** Instrumentation tests for the libavif JNI API, which will execute on an Android device. */
@RunWith(Parameterized.class)
public class AvifDecoderTest {

  private static class Image {
    public final String directory;
    public final String filename;
    public final int width;
    public final int height;
    public final int depth;
    public final boolean alphaPresent;
    public final int frameCount;
    public final int repetitionCount;
    public final double frameDuration;
    public final int threads;
    public final boolean isAnimated;

    public Image(
        String directory,
        String filename,
        int width,
        int height,
        int depth,
        boolean alphaPresent,
        int threads) {
      this(
          directory,
          filename,
          width,
          height,
          depth,
          alphaPresent,
          /* frameCount= */ 1,
          /* repetitionCount= */ 0,
          /* frameDuration= */ 0.0,
          threads);
    }

    public Image(
        String directory,
        String filename,
        int width,
        int height,
        int depth,
        boolean alphaPresent,
        int frameCount,
        int repetitionCount,
        double frameDuration,
        int threads) {
      this.directory = directory;
      this.filename = filename;
      this.width = width;
      this.height = height;
      this.depth = depth;
      this.alphaPresent = alphaPresent;
      this.frameCount = frameCount;
      this.repetitionCount = repetitionCount;
      this.frameDuration = frameDuration;
      this.threads = threads;
      this.isAnimated = frameCount > 1;
    }

    public ByteBuffer getBuffer() throws IOException {
      Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
      String assetPath = Paths.get(directory, filename).toString();
      InputStream is = context.getAssets().open(assetPath);
      ByteBuffer buffer = ByteBuffer.allocateDirect(is.available());
      Channels.newChannel(is).read(buffer);
      buffer.rewind();
      return buffer;
    }
  }

  private static final int AVIF_RESULT_OK = 0;

  private static final float[] SCALE_FACTORS = {0.5f, 1.3f};

  private static final Image[] IMAGES = {
    // Parameter ordering for still images: directory, filename, width, height, depth, alphaPresent,
    // threads.
    new Image("avif", "fox.profile0.10bpc.yuv420.avif", 1204, 800, 10, false, 1),
    new Image("avif", "fox.profile0.10bpc.yuv420.monochrome.avif", 1204, 800, 10, false, 1),
    new Image("avif", "fox.profile0.8bpc.yuv420.avif", 1204, 800, 8, false, 1),
    new Image("avif", "fox.profile0.8bpc.yuv420.monochrome.avif", 1204, 800, 8, false, 1),
    new Image("avif", "fox.profile1.10bpc.yuv444.avif", 1204, 800, 10, false, 1),
    new Image("avif", "fox.profile1.8bpc.yuv444.avif", 1204, 800, 8, false, 1),
    new Image("avif", "fox.profile2.10bpc.yuv422.avif", 1204, 800, 10, false, 1),
    new Image("avif", "fox.profile2.12bpc.yuv420.avif", 1204, 800, 12, false, 1),
    new Image("avif", "fox.profile2.12bpc.yuv420.monochrome.avif", 1204, 800, 12, false, 1),
    new Image("avif", "fox.profile2.12bpc.yuv422.avif", 1204, 800, 12, false, 1),
    new Image("avif", "fox.profile2.12bpc.yuv444.avif", 1204, 800, 12, false, 1),
    new Image("avif", "fox.profile2.8bpc.yuv422.avif", 1204, 800, 8, false, 1),
    new Image("avif", "blue-and-magenta-crop.avif", 180, 100, 8, true, 1),
    // Parameter ordering for animated images: directory, filename, width, height, depth,
    // alphaPresent, frameCount, repetitionCount, frameDuration, threads.
    new Image("animated_avif", "alpha_video.avif", 640, 480, 8, true, 48, -2, 0.04, 1),
    new Image(
        "animated_avif", "Chimera-AV1-10bit-480x270.avif", 480, 270, 10, false, 95, -2, 0.04, 2),
  };

  @Parameters
  public static List<Object[]> data() throws IOException {
    ArrayList<Object[]> list = new ArrayList<>();
    for (Image image : IMAGES) {
      // Test ARGB_8888 for all files.
      list.add(new Object[] {Config.ARGB_8888, image});
      // For 8bpc files and animated files, test only RGB_565 (F16 is flaky for animated files on
      // x86 emulators). For other files, test only RGBA_F16.
      Config testConfig = (image.depth == 8 || image.isAnimated) ? Config.RGB_565 : Config.RGBA_F16;
      list.add(new Object[] {testConfig, image});
    }
    return list;
  }

  @Parameter(0)
  public Bitmap.Config config;

  @Parameter(1)
  public Image image;

  // Tests AvifDecoder by using it as a utility class without instantiating it. Only still images
  // can be decoded this way.
  @Test
  public void testDecodeUtilityClass() throws IOException {
    if (image.isAnimated) {
      return;
    }
    ByteBuffer buffer = image.getBuffer();
    assertThat(buffer).isNotNull();
    assertThat(AvifDecoder.isAvifImage(buffer)).isTrue();
    Info info = new Info();
    assertThat(AvifDecoder.getInfo(buffer, buffer.remaining(), info)).isTrue();
    assertThat(info.width).isEqualTo(image.width);
    assertThat(info.height).isEqualTo(image.height);
    assertThat(info.depth).isEqualTo(image.depth);
    assertThat(info.alphaPresent).isEqualTo(image.alphaPresent);
    Bitmap bitmap = Bitmap.createBitmap(info.width, info.height, config);
    assertThat(bitmap).isNotNull();
    assertThat(AvifDecoder.decode(buffer, buffer.remaining(), bitmap)).isTrue();

    // Test scaling. These tests can be a bit slow on emulators, so only run them when config is
    // ARGB_8888.
    if (config == Config.ARGB_8888) {
      for (float scaleFactor : SCALE_FACTORS) {
        // Scale both width and height.
        bitmap =
            Bitmap.createBitmap(
                (int) (info.width * scaleFactor), (int) (info.height * scaleFactor), config);
        assertThat(bitmap).isNotNull();
        assertThat(AvifDecoder.decode(buffer, buffer.remaining(), bitmap)).isTrue();

        // Scale width only.
        bitmap = Bitmap.createBitmap((int) (info.width * scaleFactor), info.height, config);
        assertThat(bitmap).isNotNull();
        assertThat(AvifDecoder.decode(buffer, buffer.remaining(), bitmap)).isTrue();

        // Scale height only.
        bitmap = Bitmap.createBitmap(info.width, (int) (info.height * scaleFactor), config);
        assertThat(bitmap).isNotNull();
        assertThat(AvifDecoder.decode(buffer, buffer.remaining(), bitmap)).isTrue();
      }
    }
  }

  // Tests AvifDecoder by using it as a regular instantiated class.
  @Test
  public void testDecodeRegularClass() throws IOException {
    ByteBuffer buffer = image.getBuffer();
    assertThat(buffer).isNotNull();
    AvifDecoder decoder = AvifDecoder.create(buffer, image.threads);
    assertThat(decoder).isNotNull();
    assertThat(decoder.getWidth()).isEqualTo(image.width);
    assertThat(decoder.getHeight()).isEqualTo(image.height);
    assertThat(decoder.getDepth()).isEqualTo(image.depth);
    assertThat(decoder.getAlphaPresent()).isEqualTo(image.alphaPresent);
    assertThat(decoder.getFrameCount()).isEqualTo(image.frameCount);
    Bitmap bitmap = Bitmap.createBitmap(image.width, image.height, config);
    assertThat(bitmap).isNotNull();
    for (int i = 0; i < image.frameCount; i++) {
      assertThat(decoder.nextFrameIndex()).isEqualTo(i);
      assertThat(decoder.nextFrame(bitmap)).isEqualTo(AVIF_RESULT_OK);
    }
    if (image.isAnimated) {
      assertThat(decoder.getRepetitionCount()).isEqualTo(image.repetitionCount);
      double[] frameDurations = decoder.getFrameDurations();
      assertThat(frameDurations).isNotNull();
      assertThat(frameDurations).hasLength(image.frameCount);
      for (int i = 0; i < image.frameCount; i++) {
        assertThat(frameDurations[i]).isWithin(1.0e-2).of(image.frameDuration);
      }
      assertThat(decoder.nextFrameIndex()).isEqualTo(image.frameCount);
      // Fetch the first frame again.
      assertThat(decoder.nthFrame(0, bitmap)).isEqualTo(AVIF_RESULT_OK);
      // Now nextFrame will return the second frame.
      assertThat(decoder.nextFrameIndex()).isEqualTo(1);
      assertThat(decoder.nextFrame(bitmap)).isEqualTo(AVIF_RESULT_OK);
      // Fetch the (frameCount/2)th frame.
      assertThat(decoder.nthFrame(image.frameCount / 2, bitmap)).isEqualTo(AVIF_RESULT_OK);
      // Fetch the last frame.
      assertThat(decoder.nthFrame(image.frameCount - 1, bitmap)).isEqualTo(AVIF_RESULT_OK);
      // Now nextFrame should return false.
      assertThat(decoder.nextFrameIndex()).isEqualTo(image.frameCount);
      assertThat(decoder.nextFrame(bitmap)).isNotEqualTo(AVIF_RESULT_OK);
      // Passing out of bound values for n should fail.
      assertThat(decoder.nthFrame(-1, bitmap)).isNotEqualTo(AVIF_RESULT_OK);
      assertThat(decoder.nthFrame(image.frameCount, bitmap)).isNotEqualTo(AVIF_RESULT_OK);

      // The following block of code that tests scaling assumes that the animated image under test
      // has at least 7 frames. These tests can be a bit slow on emulators, so only run them when
      // config is ARGB_8888.
      if (image.frameCount >= 7 && config == Config.ARGB_8888) {
        // Reset the decoder to the first frame.
        assertThat(decoder.nthFrame(0, bitmap)).isEqualTo(AVIF_RESULT_OK);
        for (float scaleFactor : SCALE_FACTORS) {
          // Scale both width and height.
          bitmap =
              Bitmap.createBitmap(
                  (int) (image.width * scaleFactor), (int) (image.height * scaleFactor), config);
          assertThat(bitmap).isNotNull();
          assertThat(decoder.nextFrame(bitmap)).isEqualTo(AVIF_RESULT_OK);

          // Scale width only.
          bitmap = Bitmap.createBitmap((int) (image.width * scaleFactor), image.height, config);
          assertThat(bitmap).isNotNull();
          assertThat(decoder.nextFrame(bitmap)).isEqualTo(AVIF_RESULT_OK);

          // Scale height only.
          bitmap = Bitmap.createBitmap(image.width, (int) (image.height * scaleFactor), config);
          assertThat(bitmap).isNotNull();
          assertThat(decoder.nextFrame(bitmap)).isEqualTo(AVIF_RESULT_OK);
        }
      }
    }
    decoder.release();
  }

  @Test
  public void testUtilityFunctions() throws IOException {
    // Test the avifResult value whose value and string representations are least likely to change.
    assertThat(AvifDecoder.resultToString(AVIF_RESULT_OK)).isEqualTo("OK");
    // Ensure that the version string starts with "libavif".
    assertThat(AvifDecoder.versionString()).startsWith("libavif");
    // Ensure that the version string contains "libyuv".
    assertThat(AvifDecoder.versionString()).contains("libyuv");
    // Ensure that the version string contains "dav1d".
    assertThat(AvifDecoder.versionString()).contains("dav1d");
  }
}
