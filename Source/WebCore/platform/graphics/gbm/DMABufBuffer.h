/*
 * Copyright (C) 2024 Igalia S.L.
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

#if USE(COORDINATED_GRAPHICS) && USE(GBM)
#include "DMABufBufferAttributes.h"
#include <wtf/ThreadSafeRefCounted.h>

typedef int32_t EGLint;
typedef void* EGLImage;

namespace WebCore {

class CoordinatedPlatformLayerBuffer;
class GLDisplay;

class DMABufBuffer final : public ThreadSafeRefCounted<DMABufBuffer> {
public:
    using Attributes = DMABufBufferAttributes;

    static Ref<DMABufBuffer> create(Attributes&& attributes)
    {
        return adoptRef(*new DMABufBuffer(WTF::move(attributes)));
    }
    static Ref<DMABufBuffer> create(uint64_t id, Attributes&& attributes)
    {
        return adoptRef(*new DMABufBuffer(id, WTF::move(attributes)));
    }
    ~DMABufBuffer();

    uint64_t id() const { return m_id; }
    const Attributes& attributes() const LIFETIME_BOUND { return m_attributes; }
    std::optional<Attributes> takeAttributes();

    enum class ColorSpace : uint8_t { Bt601, Bt709, Bt2020, Smpte240M };
    std::optional<ColorSpace> colorSpace() const { return m_colorSpace; }
    void setColorSpace(ColorSpace colorSpace) { m_colorSpace = colorSpace; }

    enum class TransferFunction : uint8_t { Bt709, Pq };
    std::optional<TransferFunction> transferFunction() const { return m_transferFunction; }
    void setTransferFunction(TransferFunction transferFunction) { m_transferFunction = transferFunction; }

    EGLImage createEGLImage(GLDisplay&) const;
    static EGLImage createEGLImage(GLDisplay&, const Attributes&);
    static std::optional<Vector<EGLint>> buildEGLImageAttributes(const Attributes&, Attributes::EnableModifiers = Attributes::EnableModifiers::Yes);

    CoordinatedPlatformLayerBuffer* buffer() const LIFETIME_BOUND { return m_buffer.get(); }
    void setBuffer(std::unique_ptr<CoordinatedPlatformLayerBuffer>&&);

private:
    explicit DMABufBuffer(Attributes&&);
    DMABufBuffer(uint64_t id, Attributes&&);

    uint64_t m_id { 0 };
    Attributes m_attributes;
    std::optional<ColorSpace> m_colorSpace;
    std::optional<TransferFunction> m_transferFunction;
    std::unique_ptr<CoordinatedPlatformLayerBuffer> m_buffer;
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(GBM)
