// Copyright 2022 Google LLC. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include <android/bitmap.h>
#include <android/log.h>
#include <cpu-features.h>
#include <jni.h>

#include "avif/avif.h"

#define LOG_TAG "avif_jni"
#define LOGE(...) \
  ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))

#define FUNC(RETURN_TYPE, NAME, ...)                                      \
  extern "C" {                                                            \
  JNIEXPORT RETURN_TYPE Java_org_aomedia_avif_android_AvifDecoder_##NAME( \
      JNIEnv* env, jobject /*thiz*/, ##__VA_ARGS__);                      \
  }                                                                       \
  JNIEXPORT RETURN_TYPE Java_org_aomedia_avif_android_AvifDecoder_##NAME( \
      JNIEnv* env, jobject /*thiz*/, ##__VA_ARGS__)

namespace {

jfieldID global_info_width;
jfieldID global_info_height;
jfieldID global_info_depth;

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

  // Turn off 'clap' (clean aperture) property validation. The JNI wrapper
  // ignores the 'clap' property.
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
  return true;
}

}  // namespace

jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
  JNIEnv* env;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    return -1;
  }
  const jclass info_class =
      env->FindClass("org/aomedia/avif/android/AvifDecoder$Info");
  global_info_width = env->GetFieldID(info_class, "width", "I");
  global_info_height = env->GetFieldID(info_class, "height", "I");
  global_info_depth = env->GetFieldID(info_class, "depth", "I");
  return JNI_VERSION_1_6;
}

FUNC(jboolean, isAvifImage, jobject encoded, int length) {
  const uint8_t* const buffer =
      static_cast<const uint8_t*>(env->GetDirectBufferAddress(encoded));
  const avifROData avif = {buffer, static_cast<size_t>(length)};
  return avifPeekCompatibleFileType(&avif);
}

FUNC(jboolean, getInfo, jobject encoded, int length, jobject info) {
  const uint8_t* const buffer =
      static_cast<const uint8_t*>(env->GetDirectBufferAddress(encoded));
  AvifDecoderWrapper decoder;
  if (!CreateDecoderAndParse(&decoder, buffer, length, /*threads=*/ 1)) {
    return false;
  }
  env->SetIntField(info, global_info_width, decoder.decoder->image->width);
  env->SetIntField(info, global_info_height, decoder.decoder->image->height);
  env->SetIntField(info, global_info_depth, decoder.decoder->image->depth);
  return true;
}

FUNC(jboolean, decode, jobject encoded, int length, jobject bitmap) {
  const uint8_t* const buffer =
      static_cast<const uint8_t*>(env->GetDirectBufferAddress(encoded));
  AvifDecoderWrapper decoder;
  if (!CreateDecoderAndParse(&decoder, buffer, length,
                             android_getCpuCount())) {
    return false;
  }
  avifResult res = avifDecoderNextImage(decoder.decoder);
  if (res != AVIF_RESULT_OK) {
    LOGE("Failed to decode AVIF image. Status: %d", res);
    return false;
  }
  AndroidBitmapInfo bitmap_info;
  if (AndroidBitmap_getInfo(env, bitmap, &bitmap_info) < 0) {
    LOGE("AndroidBitmap_getInfo failed.");
    return false;
  }
  // Ensure that the bitmap is large enough to store the decoded image.
  if (bitmap_info.width < decoder.decoder->image->width ||
      bitmap_info.height < decoder.decoder->image->height) {
    LOGE(
        "Bitmap is not large enough to fit the image. Bitmap %dx%d Image "
        "%dx%d.",
        bitmap_info.width, bitmap_info.height, decoder.decoder->image->width,
        decoder.decoder->image->height);
    return false;
  }
  // Ensure that the bitmap format is RGBA_8888, RGB_565 or RGBA_F16.
  if (bitmap_info.format != ANDROID_BITMAP_FORMAT_RGBA_8888 &&
      bitmap_info.format != ANDROID_BITMAP_FORMAT_RGB_565 &&
      bitmap_info.format != ANDROID_BITMAP_FORMAT_RGBA_F16) {
    LOGE("Bitmap format (%d) is not supported.", bitmap_info.format);
    return false;
  }
  void* bitmap_pixels = nullptr;
  if (AndroidBitmap_lockPixels(env, bitmap, &bitmap_pixels) !=
      ANDROID_BITMAP_RESULT_SUCCESS) {
    LOGE("Failed to lock Bitmap.");
    return false;
  }
  avifRGBImage rgb_image;
  avifRGBImageSetDefaults(&rgb_image, decoder.decoder->image);
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
  res = avifImageYUVToRGB(decoder.decoder->image, &rgb_image);
  AndroidBitmap_unlockPixels(env, bitmap);
  if (res != AVIF_RESULT_OK) {
    LOGE("Failed to convert YUV Pixels to RGB. Status: %d", res);
    return false;
  }
  return true;
}
