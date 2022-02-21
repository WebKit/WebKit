//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// dma_buf_utils.cpp: Utilities to interact with Linux dma bufs.

#include "dma_buf_utils.h"

#include "common/debug.h"

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>

// Refer to:
// https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/include/uapi/drm/drm_fourcc.h
// https://source.chromium.org/chromium/chromium/src/+/main:ui/gl/gl_image_native_pixmap.cc;l=24
#define FOURCC(a, b, c, d)                                          \
    ((static_cast<uint32_t>(a)) | (static_cast<uint32_t>(b) << 8) | \
     (static_cast<uint32_t>(c) << 16) | (static_cast<uint32_t>(d) << 24))

#define DRM_FORMAT_R8 FOURCC('R', '8', ' ', ' ')
#define DRM_FORMAT_R16 FOURCC('R', '1', '6', ' ')
#define DRM_FORMAT_GR88 FOURCC('G', 'R', '8', '8')
#define DRM_FORMAT_RGB565 FOURCC('R', 'G', '1', '6')
#define DRM_FORMAT_ARGB8888 FOURCC('A', 'R', '2', '4')
#define DRM_FORMAT_ABGR8888 FOURCC('A', 'B', '2', '4')
#define DRM_FORMAT_XRGB8888 FOURCC('X', 'R', '2', '4')
#define DRM_FORMAT_XBGR8888 FOURCC('X', 'B', '2', '4')
#define DRM_FORMAT_ABGR2101010 FOURCC('A', 'B', '3', '0')
#define DRM_FORMAT_ARGB2101010 FOURCC('A', 'R', '3', '0')
#define DRM_FORMAT_YVU420 FOURCC('Y', 'V', '1', '2')
#define DRM_FORMAT_NV12 FOURCC('N', 'V', '1', '2')
#define DRM_FORMAT_P010 FOURCC('P', '0', '1', '0')

namespace angle
{
GLenum DrmFourCCFormatToGLInternalFormat(int fourccFormat, bool *isYUV)
{
    *isYUV = false;

    switch (fourccFormat)
    {
        case DRM_FORMAT_R8:
            return GL_R8;
        case DRM_FORMAT_R16:
            return GL_R16_EXT;
        case DRM_FORMAT_GR88:
            return GL_RG8_EXT;
        case DRM_FORMAT_ABGR8888:
            return GL_RGBA8;
        case DRM_FORMAT_XBGR8888:
            return GL_RGB8;
        case DRM_FORMAT_ARGB8888:
            return GL_BGRA8_EXT;
        case DRM_FORMAT_XRGB8888:
            return GL_RGB8;
        case DRM_FORMAT_ABGR2101010:
        case DRM_FORMAT_ARGB2101010:
            return GL_RGB10_A2_EXT;
        case DRM_FORMAT_RGB565:
            return GL_RGB565;
        case DRM_FORMAT_NV12:
        case DRM_FORMAT_YVU420:
        case DRM_FORMAT_P010:
            *isYUV = true;
            return GL_RGB8;
        default:
            UNREACHABLE();
            WARN() << "Unknown dma_buf format " << fourccFormat
                   << " used to initialize an EGL image.";
            return GL_RGB8;
    }
}

}  // namespace angle
