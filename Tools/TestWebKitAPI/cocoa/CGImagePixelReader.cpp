/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CGImagePixelReader.h"

namespace TestWebKitAPI {
using namespace WebCore;

CGImagePixelReader::CGImagePixelReader(CGImageRef image)
    : m_width(CGImageGetWidth(image))
    , m_height(CGImageGetHeight(image))
{
    auto colorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto bytesPerPixel = 4;
    auto bytesPerRow = bytesPerPixel * CGImageGetWidth(image);
    auto bitsPerComponent = 8;
IGNORE_WARNINGS_BEGIN("deprecated-enum-enum-conversion")
    CGBitmapInfo bitmapInfo = kCGImageAlphaPremultipliedLast | kCGImageByteOrder32Big;
IGNORE_WARNINGS_END
    m_context = adoptCF(CGBitmapContextCreateWithData(nullptr, m_width, m_height, bitsPerComponent, bytesPerRow, colorSpace.get(), bitmapInfo, nullptr, nullptr));
    CGContextDrawImage(m_context.get(), CGRectMake(0, 0, m_width, m_height), image);
}

bool CGImagePixelReader::isTransparentBlack(unsigned x, unsigned y) const
{
    return at(x, y) == Color::transparentBlack;
}

Color CGImagePixelReader::at(unsigned x, unsigned y) const
{
    auto* data = reinterpret_cast<uint8_t*>(CGBitmapContextGetData(m_context.get()));
    auto offset = 4 * (width() * y + x);
    return makeFromComponentsClampingExceptAlpha<SRGBA<uint8_t>>(data[offset], data[offset + 1], data[offset + 2], data[offset + 3]);
}

} // namespace TestWebKitAPI
