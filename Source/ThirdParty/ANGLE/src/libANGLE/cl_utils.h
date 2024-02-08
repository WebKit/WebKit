//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// cl_utils.h: Helper functions for the CL front end

#ifndef LIBANGLE_CL_UTILS_H_
#define LIBANGLE_CL_UTILS_H_

#include "libANGLE/renderer/cl_types.h"

#define ANGLE_CL_RETURN_ERROR(error) \
    do                               \
    {                                \
        cl::gClErrorTls = error;     \
        return angle::Result::Stop;  \
    } while (0)

#define ANGLE_CL_TRY(expression)                           \
    do                                                     \
    {                                                      \
        const cl_int ANGLE_LOCAL_VAR = expression;         \
        if (ANGLE_UNLIKELY(ANGLE_LOCAL_VAR != CL_SUCCESS)) \
        {                                                  \
            ANGLE_CL_RETURN_ERROR(ANGLE_LOCAL_VAR);        \
        }                                                  \
    } while (0)

#define ANGLE_CL_IMPL_TRY(EXPR) ANGLE_TRY_TEMPLATE(EXPR, (void))

namespace cl
{

size_t GetChannelCount(cl_channel_order channelOrder);

size_t GetElementSize(const cl_image_format &image_format);

inline bool OverlapRegions(size_t offset1, size_t offset2, size_t size)
{
    // From https://www.khronos.org/registry/OpenCL/specs/3.0-unified/html/OpenCL_API.html
    // The regions overlap if src_offset <= dst_offset <= src_offset + size - 1
    // or if dst_offset <= src_offset <= dst_offset + size - 1.
    return (offset1 <= offset2 && offset2 <= offset1 + size - 1u) ||
           (offset2 <= offset1 && offset1 <= offset2 + size - 1u);
}

bool IsValidImageFormat(const cl_image_format *imageFormat, const rx::CLExtensions &extensions);

extern thread_local cl_int gClErrorTls;

}  // namespace cl

#endif  // LIBANGLE_CL_UTILS_H_
