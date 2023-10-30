/*
 * Copyright (C) 2023 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(GBM)

#include <gbm.h>

#if !HAVE(GBM_BO_GET_FD_FOR_PLANE)
#include <fcntl.h>
#include <xf86drm.h>
#endif

#if !HAVE(GBM_BO_CREATE_WITH_MODIFIERS2)
static inline struct gbm_bo* gbm_bo_create_with_modifiers2(struct gbm_device* gbm, uint32_t width, uint32_t height, uint32_t format, const uint64_t* modifiers, const unsigned count, uint32_t)
{
    return gbm_bo_create_with_modifiers(gbm, width, height, format, modifiers, count);
}
#endif

#if !HAVE(GBM_BO_GET_FD_FOR_PLANE)
static inline int gbm_bo_get_fd_for_plane(struct gbm_bo* bo, int plane)
{
    auto handle = gbm_bo_get_handle_for_plane(bo, plane);
    if (handle.s32 == -1)
        return -1;

    int fd;
    int ret = drmPrimeHandleToFD(gbm_device_get_fd(gbm_bo_get_device(bo)), handle.u32, DRM_CLOEXEC, &fd);
    return ret < 0 ? -1 : fd;
}
#endif

#endif // USE(GBM)
