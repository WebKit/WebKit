//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// loadimage_astc.cpp: Decodes ASTC encoded textures.

#ifdef ANGLE_HAS_ASTCENC
#    include <astcenc.h>
#endif

#include "image_util/loadimage.h"

namespace angle
{

void LoadASTCToRGBA8Inner(size_t width,
                          size_t height,
                          size_t depth,
                          uint32_t blockWidth,
                          uint32_t blockHeight,
                          const uint8_t *input,
                          size_t inputRowPitch,
                          size_t inputDepthPitch,
                          uint8_t *output,
                          size_t outputRowPitch,
                          size_t outputDepthPitch)
{
#ifdef ANGLE_HAS_ASTCENC
    astcenc_config config;

    constexpr unsigned int kBlockZ = 1;

    const float kQuality               = ASTCENC_PRE_MEDIUM;
    constexpr astcenc_profile kProfile = ASTCENC_PRF_LDR;

    astcenc_error status;
    status = astcenc_config_init(kProfile, blockWidth, blockHeight, kBlockZ, kQuality,
                                 ASTCENC_FLG_DECOMPRESS_ONLY, &config);
    if (status != ASTCENC_SUCCESS)
    {
        WARN() << "astcenc config init failed: " << astcenc_get_error_string(status);
        return;
    }

    constexpr unsigned int kThreadCount = 1;

    astcenc_context *context;
    status = astcenc_context_alloc(&config, kThreadCount, &context);
    if (status != ASTCENC_SUCCESS)
    {
        WARN() << "Could not allocate astcenc context: " << astcenc_get_error_string(status);
        return;
    }

    // Compute the number of ASTC blocks in each dimension
    uint32_t blockCountX = (static_cast<uint32_t>(width) + config.block_x - 1) / config.block_x;
    uint32_t blockCountY = (static_cast<uint32_t>(height) + config.block_y - 1) / config.block_y;

    // Space needed for 16 bytes of output per compressed block
    size_t blockSize = blockCountX * blockCountY * 16;

    astcenc_image image;
    image.dim_x     = static_cast<uint32_t>(width);
    image.dim_y     = static_cast<uint32_t>(height);
    image.dim_z     = 1;
    image.data_type = ASTCENC_TYPE_U8;
    image.data      = reinterpret_cast<void **>(&output);

    constexpr astcenc_swizzle swizzle{ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A};

    status = astcenc_decompress_image(context, input, blockSize, &image, &swizzle, 0);
    if (status != ASTCENC_SUCCESS)
    {
        WARN() << "astcenc decompress failed: " << astcenc_get_error_string(status);
    }

    astcenc_context_free(context);
#else
    ERR() << "Trying to decode ASTC without having ASTC support built.";
#endif
}
}  // namespace angle
