//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// dma_buf_utils.h: Utilities to interact with Linux dma bufs.

#ifndef COMMON_LINUX_DMA_BUF_UTILS_H_
#define COMMON_LINUX_DMA_BUF_UTILS_H_

#include <angle_gl.h>

namespace angle
{
GLenum DrmFourCCFormatToGLInternalFormat(int format, bool *isYUV);
}  // namespace angle

#endif  // COMMON_LINUX_DMA_BUF_UTILS_H_
