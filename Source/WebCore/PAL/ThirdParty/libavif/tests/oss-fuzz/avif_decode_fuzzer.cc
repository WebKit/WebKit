// Copyright 2019 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/avif.h"
#include "avif/avif_cxx.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size) {
  avif::DecoderPtr decoder(avifDecoderCreate());
  if (decoder == nullptr) return 0;
  decoder->allowProgressive = AVIF_TRUE;

  // ClusterFuzz passes -rss_limit_mb=2560 to avif_decode_fuzzer. Empirically
  // setting decoder->imageSizeLimit to this value allows avif_decode_fuzzer to
  // consume no more than 2560 MB of memory. Also limit the dimensions to avoid
  // timeouts and to speed the fuzzing up.
  // avifDecoderParse returns AVIF_RESULT_NOT_IMPLEMENTED if kImageSizeLimit is
  // bigger than AVIF_DEFAULT_IMAGE_SIZE_LIMIT.
  constexpr uint32_t kImageSizeLimit = 4 * 1024 * 4 * 1024;
  static_assert(kImageSizeLimit <= AVIF_DEFAULT_IMAGE_SIZE_LIMIT,
                "Too big an image size limit");
  decoder->imageSizeLimit = kImageSizeLimit;

  avifIO* const io = avifIOCreateMemoryReader(Data, Size);
  if (io == nullptr) return 0;
  // Simulate Chrome's avifIO object, which is not persistent.
  io->persistent = AVIF_FALSE;
  avifDecoderSetIO(decoder.get(), io);

  if (avifDecoderParse(decoder.get()) != AVIF_RESULT_OK) return 0;
  while (avifDecoderNextImage(decoder.get()) == AVIF_RESULT_OK) {
  }

  // Loop once.
  if (avifDecoderReset(decoder.get()) != AVIF_RESULT_OK) return 0;
  while (avifDecoderNextImage(decoder.get()) == AVIF_RESULT_OK) {
  }

  return 0;  // Non-zero return values are reserved for future use.
}
