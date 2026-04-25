/*
 * Copyright (C) 2024, 2026 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(COORDINATED_GRAPHICS)
#include "FourCC.h"
#include "IntSize.h"
#include <optional>
#include <wtf/Vector.h>
#include <wtf/unix/UnixFileDescriptor.h>

#if USE(GBM)
struct gbm_bo;
#endif

namespace WebCore {

class DMABufBufferAttributes {
public:
    // EGL_EXT_image_dma_buf_import supports up to 4 planes (PLANE0..PLANE3).
    static constexpr unsigned maxPlaneCountForEGLImage = 4;

    enum class EnableModifiers : bool { No, Yes };

    IntSize size;
    FourCC fourcc;
    Vector<WTF::UnixFileDescriptor> fds;
    Vector<uint32_t> offsets;
    Vector<uint32_t> strides;
    uint64_t modifier { 0 };

#if USE(GBM)
    static std::optional<DMABufBufferAttributes> fromGBMBufferObject(struct gbm_bo*, EnableModifiers = EnableModifiers::Yes);
#endif
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
