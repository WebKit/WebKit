//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// frame_capture_utils:
//   Helper functions for capture and replay of traces.
//

#ifndef UTIL_FRAME_CAPTURE_UTILS_H_
#define UTIL_FRAME_CAPTURE_UTILS_H_

#include <iostream>
#include <memory>
#include <vector>

#include "common/angleutils.h"

#define USE_SYSTEM_ZLIB
#include "compression_utils_portable.h"

namespace angle
{

inline uint8_t *DecompressBinaryData(const std::vector<uint8_t> &compressedData)
{
    uint32_t uncompressedSize =
        zlib_internal::GetGzipUncompressedSize(compressedData.data(), compressedData.size());

    std::unique_ptr<uint8_t[]> uncompressedData(new uint8_t[uncompressedSize]);
    uLong destLen = uncompressedSize;
    int zResult =
        zlib_internal::GzipUncompressHelper(uncompressedData.get(), &destLen, compressedData.data(),
                                            static_cast<uLong>(compressedData.size()));

    if (zResult != Z_OK)
    {
        std::cerr << "Failure to decompressed binary data: " << zResult << "\n";
        return nullptr;
    }

    return uncompressedData.release();
}

}  // namespace angle

#endif  // UTIL_FRAME_CAPTURE_UTILS_H_
