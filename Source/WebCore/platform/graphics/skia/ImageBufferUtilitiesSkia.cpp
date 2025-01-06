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

#include "config.h"
#include "ImageBufferUtilitiesSkia.h"

#if USE(SKIA)
#include "GLContext.h"
#include "MIMETypeRegistry.h"
#include "PlatformDisplay.h"

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkData.h>
#include <skia/core/SkImage.h>
#include <skia/core/SkStream.h>
#include <skia/encode/SkJpegEncoder.h>
#include <skia/encode/SkPngEncoder.h>
#include <skia/encode/SkWebpEncoder.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

#include <wtf/text/WTFString.h>

namespace WebCore {

class VectorSkiaWritableStream final : public SkWStream {
public:
    explicit VectorSkiaWritableStream(Vector<uint8_t>& vector)
        : m_vector(vector)
    {
    }

    bool write(const void* data, size_t length) override
    {
        m_vector.append(std::span { static_cast<const uint8_t*>(data), length });
        return true;
    }

    void flush() override { }

    size_t bytesWritten() const override { return m_vector.size(); }

private:
    Vector<uint8_t>& m_vector;
};

static sk_sp<SkData> encodeAcceleratedImage(SkImage* image, const String& mimeType, std::optional<double> quality)
{
    if (!PlatformDisplay::sharedDisplay().skiaGLContext()->makeContextCurrent())
        return nullptr;

    GrDirectContext* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();

    if (MIMETypeRegistry::isJPEGMIMEType(mimeType)) {
        SkJpegEncoder::Options options;
        if (quality && *quality >= 0.0 && *quality <= 1.0)
            options.fQuality = static_cast<int>(*quality * 100.0 + 0.5);
        return SkJpegEncoder::Encode(grContext, image, options);
    }

    if (equalLettersIgnoringASCIICase(mimeType, "image/webp"_s)) {
        SkWebpEncoder::Options options;
        if (quality && *quality >= 0.0 && *quality <= 1.0)
            options.fQuality = static_cast<int>(*quality * 100.0 + 0.5);
        return SkWebpEncoder::Encode(grContext, image, options);
    }

    if (equalLettersIgnoringASCIICase(mimeType, "image/png"_s))
        return SkPngEncoder::Encode(grContext, image, { });

    return nullptr;
}

static Vector<uint8_t> encodeUnacceleratedImage(const SkPixmap& pixmap, const String& mimeType, std::optional<double> quality)
{
    Vector<uint8_t> result;
    VectorSkiaWritableStream stream(result);

    if (MIMETypeRegistry::isJPEGMIMEType(mimeType)) {
        SkJpegEncoder::Options options;
        if (quality && *quality >= 0.0 && *quality <= 1.0)
            options.fQuality = static_cast<int>(*quality * 100.0 + 0.5);
        if (!SkJpegEncoder::Encode(&stream, pixmap, options))
            return { };
    } else if (equalLettersIgnoringASCIICase(mimeType, "image/webp"_s)) {
        SkWebpEncoder::Options options;
        if (quality && *quality >= 0.0 && *quality <= 1.0)
            options.fQuality = static_cast<int>(*quality * 100.0 + 0.5);
        if (!SkWebpEncoder::Encode(&stream, pixmap, options))
            return { };
    } else if (equalLettersIgnoringASCIICase(mimeType, "image/png"_s)) {
        if (!SkPngEncoder::Encode(&stream, pixmap, { }))
            return { };
    }

    return result;
}

Vector<uint8_t> encodeData(SkImage* image, const String& mimeType, std::optional<double> quality)
{
    if (image->isTextureBacked()) {
        auto data = encodeAcceleratedImage(image, mimeType, quality);
        if (!data)
            return { };

        return std::span<const uint8_t> { reinterpret_cast<const uint8_t*>(data->data()), data->size() };
    }

    SkPixmap pixmap;
    if (!image->peekPixels(&pixmap))
        return { };

    return encodeUnacceleratedImage(pixmap, mimeType, quality);
}

} // namespace WebCore

#endif // USE(SKIA)
