// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#ifndef AVIF_AVIF_CXX_H
#define AVIF_AVIF_CXX_H

#if !defined(__cplusplus)
#error "This a C++ only header. Use avif/avif.h for C."
#endif

#include <memory>

#include "avif/avif.h"

namespace avif
{

// Struct to call the destroy functions in a unique_ptr.
struct UniquePtrDeleter
{
    void operator()(avifEncoder * encoder) const { avifEncoderDestroy(encoder); }
    void operator()(avifDecoder * decoder) const { avifDecoderDestroy(decoder); }
    void operator()(avifImage * image) const { avifImageDestroy(image); }
};

// Use these unique_ptr to ensure the structs are automatically destroyed.
using EncoderPtr = std::unique_ptr<avifEncoder, UniquePtrDeleter>;
using DecoderPtr = std::unique_ptr<avifDecoder, UniquePtrDeleter>;
using ImagePtr = std::unique_ptr<avifImage, UniquePtrDeleter>;

} // namespace avif

#endif // AVIF_AVIF_CXX_H
