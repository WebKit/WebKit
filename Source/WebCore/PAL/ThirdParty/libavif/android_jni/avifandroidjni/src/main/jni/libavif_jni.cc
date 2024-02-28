// Copyright 2022 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <android/bitmap.h>
#include <android/log.h>
#include <cpu-features.h>
#include <jni.h>

#include <cstdio>
#include <memory>
#include <new>

#include "avif/avif.h"

#define LOG_TAG "avif_jni"
#define LOGE(...) \
  ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))

#define FUNC(RETURN_TYPE, NAME, ...)                                      \
  extern "C" {                                                            \
  JNIEXPORT RETURN_TYPE Java_org_aomedia_avif_android_AvifDecoder_##NAME( \
      JNIEnv* env, jobject thiz, ##__VA_ARGS__);                          \
  }                                                                       \
  JNIEXPORT RETURN_TYPE Java_org_aomedia_avif_android_AvifDecoder_##NAME( \
      JNIEnv* env, jobject thiz, ##__VA_ARGS__)

#define IGNORE_UNUSED_JNI_PARAMETERS \
  (void) env; \
  (void) thiz

namespace {

// RAII wrapper class that properly frees the decoder related objects on
// destruction.
struct AvifDecoderWrapper {
 public:
  AvifDecoderWrapper() = default;
  // Not copyable or movable.
  AvifDecoderWrapper(const AvifDecoderWrapper&) = delete;
  AvifDecoderWrapper& operator=(const AvifDecoderWrapper&) = delete;

  ~AvifDecoderWrapper() {
    if (decoder != nullptr) {
      avifDecoderDestroy(decoder);
    }
  }

  avifDecoder* decoder = nullptr;
  avifCropRect crop;
};

bool CreateDecoderAndParse(AvifDecoderWrapper* const decoder,
                           const uint8_t* const buffer, int length,
                           int threads) {
  decoder->decoder = avifDecoderCreate();
  if (decoder->decoder == nullptr) {
    LOGE("Failed to create AVIF Decoder.");
    return false;
  }
  decoder->decoder->maxThreads = threads;
  decoder->decoder->ignoreXMP = AVIF_TRUE;
  decoder->decoder->ignoreExif = AVIF_TRUE;

  // Turn off libavif's 'clap' (clean aperture) property validation. This allows
  // us to detect and ignore streams that have an invalid 'clap' property
  // instead failing.
  decoder->decoder->strictFlags &= ~AVIF_STRICT_CLAP_VALID;
  // Allow 'pixi' (pixel information) property to be missing. Older versions of
  // libheif did not add the 'pixi' item property to AV1 image items (See
  // crbug.com/1198455).
  decoder->decoder->strictFlags &= ~AVIF_STRICT_PIXI_REQUIRED;

  avifResult res = avifDecoderSetIOMemory(decoder->decoder, buffer, length);
  if (res != AVIF_RESULT_OK) {
    LOGE("Failed to set AVIF IO to a memory reader.");
    return false;
  }
  res = avifDecoderParse(decoder->decoder);
  if (res != AVIF_RESULT_OK) {
    LOGE("Failed to parse AVIF image: %s.", avifResultToString(res));
    return false;
  }

  avifDiagnostics diag;
  // If the image does not have a valid 'clap' property, then we simply display
  // the whole image.
  if (!(decoder->decoder->image->transformFlags & AVIF_TRANSFORM_CLAP) ||
      !avifCropRectConvertCleanApertureBox(
          &decoder->crop, &decoder->decoder->image->clap,
          decoder->decoder->image->width, decoder->decoder->image->height,
          decoder->decoder->image->yuvFormat, &diag)) {
    decoder->crop.width = decoder->decoder->image->width;
    decoder->crop.height = decoder->decoder->image->height;
    decoder->crop.x = 0;
    decoder->crop.y = 0;
  }
  return true;
}

avifResult AvifImageToBitmap(JNIEnv* const env,
                             AvifDecoderWrapper* const decoder,
                             jobject bitmap) {
  AndroidBitmapInfo bitmap_info;
  if (AndroidBitmap_getInfo(env, bitmap, &bitmap_info) < 0) {
    LOGE("AndroidBitmap_getInfo failed.");
    return AVIF_RESULT_UNKNOWN_ERROR;
  }
  // Ensure that the bitmap format is RGBA_8888, RGB_565 or RGBA_F16.
  if (bitmap_info.format != ANDROID_BITMAP_FORMAT_RGBA_8888 &&
      bitmap_info.format != ANDROID_BITMAP_FORMAT_RGB_565 &&
      bitmap_info.format != ANDROID_BITMAP_FORMAT_RGBA_F16) {
    LOGE("Bitmap format (%d) is not supported.", bitmap_info.format);
    return AVIF_RESULT_NOT_IMPLEMENTED;
  }
  void* bitmap_pixels = nullptr;
  if (AndroidBitmap_lockPixels(env, bitmap, &bitmap_pixels) !=
      ANDROID_BITMAP_RESULT_SUCCESS) {
    LOGE("Failed to lock Bitmap.");
    return AVIF_RESULT_UNKNOWN_ERROR;
  }
  avifImage* image;
  std::unique_ptr<avifImage, decltype(&avifImageDestroy)> cropped_image(
      nullptr, avifImageDestroy);
  avifResult res;
  if (decoder->decoder->image->width == decoder->crop.width &&
      decoder->decoder->image->height == decoder->crop.height &&
      decoder->crop.x == 0 && decoder->crop.y == 0) {
    image = decoder->decoder->image;
  } else {
    cropped_image.reset(avifImageCreateEmpty());
    if (cropped_image == nullptr) {
      LOGE("Failed to allocate cropped image.");
      return AVIF_RESULT_OUT_OF_MEMORY;
    }
    res = avifImageSetViewRect(cropped_image.get(), decoder->decoder->image,
                               &decoder->crop);
    if (res != AVIF_RESULT_OK) {
      LOGE("Failed to set crop rectangle. Status: %d", res);
      return res;
    }
    image = cropped_image.get();
  }
  std::unique_ptr<avifImage, decltype(&avifImageDestroy)> image_copy(
      nullptr, avifImageDestroy);
  if (image->width != bitmap_info.width ||
      image->height != bitmap_info.height) {
    // If the avifImage does not own the planes, then create a copy for safe
    // scaling.
    if (!image->imageOwnsYUVPlanes || !image->imageOwnsAlphaPlane) {
      image_copy.reset(avifImageCreateEmpty());
      res = avifImageCopy(image_copy.get(), image, AVIF_PLANES_ALL);
      if (res != AVIF_RESULT_OK) {
        LOGE("Failed to make a copy of the image for scaling. Status: %d", res);
        return res;
      }
      image = image_copy.get();
    }
    avifDiagnostics diag;
    res = avifImageScale(image, bitmap_info.width, bitmap_info.height, &diag);
    if (res != AVIF_RESULT_OK) {
      LOGE("Failed to scale image. Status: %d", res);
      return res;
    }
  }

  avifRGBImage rgb_image;
  avifRGBImageSetDefaults(&rgb_image, image);
  if (bitmap_info.format == ANDROID_BITMAP_FORMAT_RGBA_F16) {
    rgb_image.depth = 16;
    rgb_image.isFloat = AVIF_TRUE;
  } else if (bitmap_info.format == ANDROID_BITMAP_FORMAT_RGB_565) {
    rgb_image.format = AVIF_RGB_FORMAT_RGB_565;
    rgb_image.depth = 8;
  } else {
    rgb_image.depth = 8;
  }
  rgb_image.pixels = static_cast<uint8_t*>(bitmap_pixels);
  rgb_image.rowBytes = bitmap_info.stride;
  // Android always sees the Bitmaps as premultiplied with alpha when it renders
  // them:
  // https://developer.android.com/reference/android/graphics/Bitmap#setPremultiplied(boolean)
  rgb_image.alphaPremultiplied = AVIF_TRUE;
  res = avifImageYUVToRGB(image, &rgb_image);
  AndroidBitmap_unlockPixels(env, bitmap);
  if (res != AVIF_RESULT_OK) {
    LOGE("Failed to convert YUV Pixels to RGB. Status: %d", res);
    return res;
  }
  return AVIF_RESULT_OK;
}

avifResult DecodeNextImage(JNIEnv* const env, AvifDecoderWrapper* const decoder,
                           jobject bitmap) {
  avifResult res = avifDecoderNextImage(decoder->decoder);
  if (res != AVIF_RESULT_OK) {
    LOGE("Failed to decode AVIF image. Status: %d", res);
    return res;
  }
  return AvifImageToBitmap(env, decoder, bitmap);
}

avifResult DecodeNthImage(JNIEnv* const env, AvifDecoderWrapper* const decoder,
                          uint32_t n, jobject bitmap) {
  avifResult res = avifDecoderNthImage(decoder->decoder, n);
  if (res != AVIF_RESULT_OK) {
    LOGE("Failed to decode AVIF image. Status: %d", res);
    return res;
  }
  return AvifImageToBitmap(env, decoder, bitmap);
}

int getThreadCount(int threads) {
  return (threads == 0) ? android_getCpuCount() : threads;
}

// Checks if there is a pending JNI exception that will be thrown when the
// control returns to the java layer. If there is none, it will return false. If
// there is one, then it will clear the pending exception and return true.
// Whenever this function returns true, the caller should treat it as a fatal
// error and return with a failure status as early as possible.
bool JniExceptionCheck(JNIEnv* env) {
  if (!env->ExceptionCheck()) {
    return false;
  }
  env->ExceptionClear();
  return true;
}

}  // namespace

jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
  JNIEnv* env;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    return -1;
  }
  return JNI_VERSION_1_6;
}

FUNC(jboolean, isAvifImage, jobject encoded, int length) {
  IGNORE_UNUSED_JNI_PARAMETERS;
  const uint8_t* const buffer =
      static_cast<const uint8_t*>(env->GetDirectBufferAddress(encoded));
  const avifROData avif = {buffer, static_cast<size_t>(length)};
  return avifPeekCompatibleFileType(&avif);
}

#define CHECK_EXCEPTION(ret)                \
  do {                                      \
    if (JniExceptionCheck(env)) return ret; \
  } while (false)

#define FIND_CLASS(var, class_name, ret)         \
  const jclass var = env->FindClass(class_name); \
  CHECK_EXCEPTION(ret);                          \
  if (var == nullptr) return ret

#define GET_FIELD_ID(var, class_name, field_name, signature, ret)          \
  const jfieldID var = env->GetFieldID(class_name, field_name, signature); \
  CHECK_EXCEPTION(ret);                                                    \
  if (var == nullptr) return ret

FUNC(jboolean, getInfo, jobject encoded, int length, jobject info) {
  IGNORE_UNUSED_JNI_PARAMETERS;
  const uint8_t* const buffer =
      static_cast<const uint8_t*>(env->GetDirectBufferAddress(encoded));
  AvifDecoderWrapper decoder;
  if (!CreateDecoderAndParse(&decoder, buffer, length, /*threads=*/1)) {
    return false;
  }
  FIND_CLASS(info_class, "org/aomedia/avif/android/AvifDecoder$Info", false);
  GET_FIELD_ID(width, info_class, "width", "I", false);
  GET_FIELD_ID(height, info_class, "height", "I", false);
  GET_FIELD_ID(depth, info_class, "depth", "I", false);
  GET_FIELD_ID(alpha_present, info_class, "alphaPresent", "Z", false);
  env->SetIntField(info, width, decoder.crop.width);
  CHECK_EXCEPTION(false);
  env->SetIntField(info, height, decoder.crop.height);
  CHECK_EXCEPTION(false);
  env->SetIntField(info, depth, decoder.decoder->image->depth);
  CHECK_EXCEPTION(false);
  env->SetBooleanField(info, alpha_present, decoder.decoder->alphaPresent);
  CHECK_EXCEPTION(false);
  return true;
}

FUNC(jboolean, decode, jobject encoded, int length, jobject bitmap,
     jint threads) {
  IGNORE_UNUSED_JNI_PARAMETERS;
  if (threads < 0) {
    LOGE("Invalid value for threads (%d).", threads);
    return false;
  }
  const uint8_t* const buffer =
      static_cast<const uint8_t*>(env->GetDirectBufferAddress(encoded));
  AvifDecoderWrapper decoder;
  if (!CreateDecoderAndParse(&decoder, buffer, length,
                             getThreadCount(threads))) {
    return false;
  }
  return DecodeNextImage(env, &decoder, bitmap) == AVIF_RESULT_OK;
}

FUNC(jlong, createDecoder, jobject encoded, jint length, jint threads) {
  if (threads < 0) {
    LOGE("Invalid value for threads (%d).", threads);
    return 0;
  }
  const uint8_t* const buffer =
      static_cast<const uint8_t*>(env->GetDirectBufferAddress(encoded));
  std::unique_ptr<AvifDecoderWrapper> decoder(new (std::nothrow)
                                                  AvifDecoderWrapper());
  if (decoder == nullptr) {
    return 0;
  }
  if (!CreateDecoderAndParse(decoder.get(), buffer, length,
                             getThreadCount(threads))) {
    return 0;
  }
  FIND_CLASS(avif_decoder_class, "org/aomedia/avif/android/AvifDecoder", 0);
  GET_FIELD_ID(width_id, avif_decoder_class, "width", "I", 0);
  GET_FIELD_ID(height_id, avif_decoder_class, "height", "I", 0);
  GET_FIELD_ID(depth_id, avif_decoder_class, "depth", "I", 0);
  GET_FIELD_ID(alpha_present_id, avif_decoder_class, "alphaPresent", "Z", 0);
  GET_FIELD_ID(frame_count_id, avif_decoder_class, "frameCount", "I", 0);
  GET_FIELD_ID(repetition_count_id, avif_decoder_class, "repetitionCount", "I",
               0);
  GET_FIELD_ID(frame_durations_id, avif_decoder_class, "frameDurations", "[D",
               0);
  env->SetIntField(thiz, width_id, decoder->crop.width);
  CHECK_EXCEPTION(0);
  env->SetIntField(thiz, height_id, decoder->crop.height);
  CHECK_EXCEPTION(0);
  env->SetIntField(thiz, depth_id, decoder->decoder->image->depth);
  CHECK_EXCEPTION(0);
  env->SetBooleanField(thiz, alpha_present_id, decoder->decoder->alphaPresent);
  CHECK_EXCEPTION(0);
  env->SetIntField(thiz, repetition_count_id,
                   decoder->decoder->repetitionCount);
  CHECK_EXCEPTION(0);
  const int frameCount = decoder->decoder->imageCount;
  env->SetIntField(thiz, frame_count_id, frameCount);
  CHECK_EXCEPTION(0);
  // This native array is needed because setting one element at a time to a Java
  // array from the JNI layer is inefficient.
  std::unique_ptr<double[]> native_durations(
      new (std::nothrow) double[frameCount]);
  if (native_durations == nullptr) {
    return 0;
  }
  for (int i = 0; i < frameCount; ++i) {
    avifImageTiming timing;
    if (avifDecoderNthImageTiming(decoder->decoder, i, &timing) !=
        AVIF_RESULT_OK) {
      return 0;
    }
    native_durations[i] = timing.duration;
  }
  jdoubleArray durations = env->NewDoubleArray(frameCount);
  if (durations == nullptr) {
    return 0;
  }
  env->SetDoubleArrayRegion(durations, /*start=*/0, frameCount,
                            native_durations.get());
  CHECK_EXCEPTION(0);
  env->SetObjectField(thiz, frame_durations_id, durations);
  CHECK_EXCEPTION(0);
  return reinterpret_cast<jlong>(decoder.release());
}

#undef GET_FIELD_ID
#undef FIND_CLASS
#undef CHECK_EXCEPTION

FUNC(jint, nextFrame, jlong jdecoder, jobject bitmap) {
  IGNORE_UNUSED_JNI_PARAMETERS;
  AvifDecoderWrapper* const decoder =
      reinterpret_cast<AvifDecoderWrapper*>(jdecoder);
  return DecodeNextImage(env, decoder, bitmap);
}

FUNC(jint, nextFrameIndex, jlong jdecoder) {
  IGNORE_UNUSED_JNI_PARAMETERS;
  AvifDecoderWrapper* const decoder =
      reinterpret_cast<AvifDecoderWrapper*>(jdecoder);
  return decoder->decoder->imageIndex + 1;
}

FUNC(jint, nthFrame, jlong jdecoder, jint n, jobject bitmap) {
  IGNORE_UNUSED_JNI_PARAMETERS;
  AvifDecoderWrapper* const decoder =
      reinterpret_cast<AvifDecoderWrapper*>(jdecoder);
  return DecodeNthImage(env, decoder, n, bitmap);
}

FUNC(jstring, resultToString, jint result) {
  IGNORE_UNUSED_JNI_PARAMETERS;
  return env->NewStringUTF(avifResultToString(static_cast<avifResult>(result)));
}

FUNC(jstring, versionString) {
  IGNORE_UNUSED_JNI_PARAMETERS;
  char codec_versions[256];
  avifCodecVersions(codec_versions);
  char libyuv_version[64];
  if (avifLibYUVVersion() > 0) {
    snprintf(libyuv_version, sizeof(libyuv_version), " libyuv: %u.",
             avifLibYUVVersion());
  } else {
    libyuv_version[0] = '\0';
  }
  char version_string[512];
  snprintf(version_string, sizeof(version_string),
           "libavif: %s. Codecs: %s.%s", avifVersion(),
           codec_versions, libyuv_version);
  return env->NewStringUTF(version_string);
}

FUNC(void, destroyDecoder, jlong jdecoder) {
  IGNORE_UNUSED_JNI_PARAMETERS;
  AvifDecoderWrapper* const decoder =
      reinterpret_cast<AvifDecoderWrapper*>(jdecoder);
  delete decoder;
}
