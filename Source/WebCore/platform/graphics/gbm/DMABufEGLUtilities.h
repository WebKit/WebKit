/*
 * Copyright (C) 2022 Metrological Group B.V.
 * Copyright (C) 2022 Igalia S.L.
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

#include "DMABufObject.h"
#include <span>
#include <wtf/Vector.h>

// This is a utilities header that provides helpers for integration of DMABuf operations with EGL.
// No EGL headers are included here to keep the helpers usable across different EGL implementations,
// so the expectation is that this header is included in the place-of-use implementation file after
// the appropriate EGL headers have already been included.

namespace WebCore::DMABufEGLUtilities {

enum class PlaneModifiersUsage : bool {
    Use = true,
    DoNotUse = false,
};

static inline Vector<EGLAttrib> constructEGLCreateImageAttributes(const DMABufObject& object, unsigned planeIndex, PlaneModifiersUsage planeModifiersUsage)
{
    Vector<EGLAttrib> attributes;
    attributes.reserveInitialCapacity(12 + 4 + 1);

    attributes.append(std::span<const EGLAttrib>({
        EGL_WIDTH, EGLint(object.format.planeWidth(planeIndex, object.width)),
        EGL_HEIGHT, EGLint(object.format.planeHeight(planeIndex, object.height)),
        EGL_LINUX_DRM_FOURCC_EXT, EGLint(object.format.planes[planeIndex].fourcc),
        EGL_DMA_BUF_PLANE0_FD_EXT, object.fd[planeIndex].value(),
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, EGLint(object.offset[planeIndex]),
        EGL_DMA_BUF_PLANE0_PITCH_EXT, EGLint(object.stride[planeIndex]),
    }));

    if (planeModifiersUsage == PlaneModifiersUsage::Use && object.modifierPresent[planeIndex]) {
        attributes.append(std::span<const EGLAttrib>({
            EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, EGLint(object.modifierValue[planeIndex] >> 32),
            EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, EGLint(object.modifierValue[planeIndex] & 0xffffffff),
        }));
    }

    attributes.append(EGL_NONE);
    return attributes;
}

} // namespace WebCore::DMABufEGLUtilities
