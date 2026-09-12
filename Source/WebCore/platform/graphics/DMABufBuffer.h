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

#if USE(COORDINATED_GRAPHICS)
#include "DMABufBufferAttributes.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkImage.h>
#include <skia/core/SkYUVAInfo.h>
#include <skia/gpu/ganesh/GrBackendSurface.h>
#include <skia/gpu/ganesh/GrContextThreadSafeProxy.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/unix/UnixFileDescriptor.h>

typedef int32_t EGLint;
typedef void* EGLImage;

namespace WebCore {
class BitmapTexture;
class CoordinatedPlatformLayerBuffer;
class GLDisplay;
class GLFence;

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

    enum class SampleRange : bool { Narrow, Full };
    std::optional<SampleRange> sampleRange() const { return m_sampleRange; }
    void setSampleRange(SampleRange sampleRange) { m_sampleRange = sampleRange; }

    EGLImage createEGLImage(GLDisplay&) const;
    static EGLImage createEGLImage(GLDisplay&, const Attributes&, std::optional<ColorSpace> = std::nullopt, std::optional<SampleRange> = std::nullopt);
    static std::optional<Vector<EGLint>> buildEGLImageAttributes(const Attributes&, Attributes::EnableModifiers = Attributes::EnableModifiers::Yes);

#if USE(TEXTURE_MAPPER)
    CoordinatedPlatformLayerBuffer* buffer() const LIFETIME_BOUND { return m_buffer.get(); }
    void setBuffer(std::unique_ptr<CoordinatedPlatformLayerBuffer>&&);
#else
    bool importIfNeeded();
    sk_sp<SkColorSpace> skiaColorSpace() const;
    SkYUVAInfo yuvaInfo() const;
    const GrBackendTexture& backendTexture(size_t) const;

    sk_sp<SkImage> createImage(SkColorType, SkAlphaType, GrSurfaceOrigin);
    sk_sp<SkImage> createPromiseImage(const sk_sp<GrContextThreadSafeProxy>&, SkColorType, SkAlphaType, GrSurfaceOrigin, std::unique_ptr<GLFence>&&, WTF::UnixFileDescriptor&&);
#if USE(GSTREAMER)
    sk_sp<SkImage> createPromiseImageForQualcommVideoFrame(const sk_sp<GrContextThreadSafeProxy>&, SkColorType, SkAlphaType, GrSurfaceOrigin);
#endif
#endif

private:
    explicit DMABufBuffer(Attributes&&);
    DMABufBuffer(uint64_t id, Attributes&&);

    uint64_t m_id { 0 };
    Attributes m_attributes;
    std::optional<ColorSpace> m_colorSpace;
    std::optional<TransferFunction> m_transferFunction;
    std::optional<SampleRange> m_sampleRange;
#if USE(TEXTURE_MAPPER)
    std::unique_ptr<CoordinatedPlatformLayerBuffer> m_buffer;
#else
    Vector<GrBackendTexture, 4> m_importedBackendTextures;
    Vector<RefPtr<BitmapTexture>, 4> m_importedTextures;
#endif
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
