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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "PlatformImage.h"

#include <cstdio>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#include <skia/codec/SkPngDecoder.h>
#pragma GCC diagnostic pop

#include <skia/core/SkImage.h>
#include <skia/core/SkPixmap.h>
#include <skia/core/SkStream.h>
#include <skia/encode/SkPngEncoder.h>

namespace ImageDiff {

static std::unique_ptr<PlatformImage> createFromStream(std::unique_ptr<SkStream>&& stream)
{
    if (!stream)
        return nullptr;

    SkCodec::Result decodeResult;
    auto codec = SkPngDecoder::Decode(std::move(stream), &decodeResult, nullptr);
    if (decodeResult != SkCodec::Result::kSuccess) {
        fprintf(stderr, "ImageDiff failed to decode PNG: %s\n", SkCodec::ResultToString(decodeResult));
        return nullptr;
    }

    auto [image, result] = codec->getImage();
    if (result != SkCodec::Result::kSuccess) {
        fprintf(stderr, "ImageDiff failed to decode PNG: %s\n", SkCodec::ResultToString(result));
        return nullptr;
    }

    return std::make_unique<PlatformImage>(std::move(image));
}

std::unique_ptr<PlatformImage> PlatformImage::createFromFile(const char* filename)
{
    return createFromStream(SkStream::MakeFromFile(filename));
}

std::unique_ptr<PlatformImage> PlatformImage::createFromStdin(size_t imageSize)
{
    SkDynamicMemoryWStream stream;
    unsigned char buffer[2048];
    size_t bytesRemaining = imageSize;
    while (bytesRemaining > 0) {
        size_t bytesToRead = std::min<size_t>(bytesRemaining, 2048);
        size_t bytesRead = fread(buffer, 1, bytesToRead, stdin);
        stream.write(buffer, bytesRead);
        bytesRemaining -= static_cast<int>(bytesRead);
    }
    return createFromStream(stream.detachAsStream());
}

std::unique_ptr<PlatformImage> PlatformImage::createFromDiffData(void* data, size_t width, size_t height)
{
    auto imageInfo = SkImageInfo::Make(width, height, SkColorType::kGray_8_SkColorType, SkAlphaType::kOpaque_SkAlphaType);
    SkPixmap pixmap(imageInfo, data, imageInfo.minRowBytes());
    auto image = SkImages::RasterFromPixmap(pixmap, [](const void* pixels, void*) {
        free(const_cast<void*>(pixels));
    }, nullptr);
    if (!image)
        return nullptr;
    return std::make_unique<PlatformImage>(std::move(image));
}

PlatformImage::PlatformImage(sk_sp<SkImage>&& image)
    : m_image(std::move(image))
{
}

PlatformImage::~PlatformImage() = default;

size_t PlatformImage::width() const
{
    return m_image->width();
}

size_t PlatformImage::height() const
{
    return m_image->height();
}

size_t PlatformImage::rowBytes() const
{
    return m_image->imageInfo().minRowBytes();
}

bool PlatformImage::hasAlpha() const
{
    return !m_image->imageInfo().isOpaque();
}

unsigned char* PlatformImage::pixels() const
{
    SkPixmap pixmap;
    if (m_image->peekPixels(&pixmap))
        return static_cast<unsigned char*>(pixmap.writable_addr());
    return nullptr;
}

void PlatformImage::writeAsPNGToStdout()
{
    auto data = SkPngEncoder::Encode(nullptr, m_image.get(), { });
    if (!data)
        return;

    fprintf(stdout, "Content-Length: %zu\n", data->size());
    fwrite(data->data(), 1, data->size(), stdout);
}

} // namespace ImageDiff
