/*
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "PixelBuffer.h"

namespace WebCore {
class SharedMemory;

class ShareablePixelBuffer : public PixelBuffer {
public:
    WEBCORE_EXPORT static RefPtr<ShareablePixelBuffer> tryCreate(const PixelBufferFormat&, const IntSize&);
    WEBCORE_EXPORT static std::optional<Ref<ShareablePixelBuffer>> create(const PixelBufferFormat&, const IntSize&);

    SharedMemory& data() const { return m_data.get(); }

    RefPtr<PixelBuffer> createScratchPixelBuffer(const IntSize&) const override;

private:
    ShareablePixelBuffer(const PixelBufferFormat&, const IntSize&, Ref<SharedMemory>&&);

    bool isShareablePixelBuffer() const override { return true; }

    Ref<SharedMemory> m_data;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ShareablePixelBuffer)
    static bool isType(const WebCore::PixelBuffer& pixelBuffer) { return pixelBuffer.isShareablePixelBuffer(); }
SPECIALIZE_TYPE_TRAITS_END()
