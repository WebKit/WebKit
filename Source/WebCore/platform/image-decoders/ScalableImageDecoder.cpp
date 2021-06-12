/*
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
 * Copyright (C) 2008-2009 Torch Mobile, Inc.
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "ScalableImageDecoder.h"

#include "BMPImageDecoder.h"
#include "GIFImageDecoder.h"
#include "ICOImageDecoder.h"
#include "JPEGImageDecoder.h"
#include "NotImplemented.h"
#include "PNGImageDecoder.h"
#include "SharedBuffer.h"
#if USE(AVIF)
#include "AVIFImageDecoder.h"
#endif
#if USE(OPENJPEG)
#include "JPEG2000ImageDecoder.h"
#endif
#if USE(WEBP)
#include "WEBPImageDecoder.h"
#endif

#include <algorithm>
#include <cmath>


namespace WebCore {

namespace {

static unsigned copyFromSharedBuffer(char* buffer, unsigned bufferLength, const SharedBuffer& sharedBuffer)
{
    unsigned bytesExtracted = 0;
    for (const auto& element : sharedBuffer) {
        if (bytesExtracted + element.segment->size() <= bufferLength) {
            memcpy(buffer + bytesExtracted, element.segment->data(), element.segment->size());
            bytesExtracted += element.segment->size();
        } else {
            ASSERT(bufferLength - bytesExtracted < element.segment->size());
            memcpy(buffer + bytesExtracted, element.segment->data(), bufferLength - bytesExtracted);
            bytesExtracted = bufferLength;
            break;
        }
    }
    return bytesExtracted;
}

bool matchesGIFSignature(char* contents)
{
    return !memcmp(contents, "GIF87a", 6) || !memcmp(contents, "GIF89a", 6);
}

bool matchesPNGSignature(char* contents)
{
    return !memcmp(contents, "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8);
}

bool matchesJPEGSignature(char* contents)
{
    return !memcmp(contents, "\xFF\xD8\xFF", 3);
}

#if USE(AVIF)
bool matchesAVIFSignature(char* contents)
{
    return !memcmp(contents + 4, "\x66\x74\x79\x70", 4);
}
#endif

#if USE(OPENJPEG)
bool matchesJP2Signature(char* contents)
{
    return !memcmp(contents, "\x00\x00\x00\x0C\x6A\x50\x20\x20\x0D\x0A\x87\x0A", 12)
        || !memcmp(contents, "\x0D\x0A\x87\x0A", 4);
}

bool matchesJ2KSignature(char* contents)
{
    return !memcmp(contents, "\xFF\x4F\xFF\x51", 4);
}
#endif

#if USE(WEBP)
bool matchesWebPSignature(char* contents)
{
    return !memcmp(contents, "RIFF", 4) && !memcmp(contents + 8, "WEBPVP", 6);
}
#endif

bool matchesBMPSignature(char* contents)
{
    return !memcmp(contents, "BM", 2);
}

bool matchesICOSignature(char* contents)
{
    return !memcmp(contents, "\x00\x00\x01\x00", 4);
}

bool matchesCURSignature(char* contents)
{
    return !memcmp(contents, "\x00\x00\x02\x00", 4);
}

}

RefPtr<ScalableImageDecoder> ScalableImageDecoder::create(SharedBuffer& data, AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
{
    static const unsigned lengthOfLongestSignature = 14; // To wit: "RIFF????WEBPVP"
    char contents[lengthOfLongestSignature];
    unsigned length = copyFromSharedBuffer(contents, lengthOfLongestSignature, data);
    if (length < lengthOfLongestSignature)
        return nullptr;

    if (matchesGIFSignature(contents))
        return GIFImageDecoder::create(alphaOption, gammaAndColorProfileOption);

    if (matchesPNGSignature(contents))
        return PNGImageDecoder::create(alphaOption, gammaAndColorProfileOption);

    if (matchesICOSignature(contents) || matchesCURSignature(contents))
        return ICOImageDecoder::create(alphaOption, gammaAndColorProfileOption);

    if (matchesJPEGSignature(contents))
        return JPEGImageDecoder::create(alphaOption, gammaAndColorProfileOption);

#if USE(AVIF)
    if (matchesAVIFSignature(contents))
        return AVIFImageDecoder::create(alphaOption, gammaAndColorProfileOption);
#endif

#if USE(OPENJPEG)
    if (matchesJP2Signature(contents))
        return JPEG2000ImageDecoder::create(JPEG2000ImageDecoder::Format::JP2, alphaOption, gammaAndColorProfileOption);

    if (matchesJ2KSignature(contents))
        return JPEG2000ImageDecoder::create(JPEG2000ImageDecoder::Format::J2K, alphaOption, gammaAndColorProfileOption);
#endif

#if USE(WEBP)
    if (matchesWebPSignature(contents))
        return WEBPImageDecoder::create(alphaOption, gammaAndColorProfileOption);
#endif

    if (matchesBMPSignature(contents))
        return BMPImageDecoder::create(alphaOption, gammaAndColorProfileOption);

    return nullptr;
}

bool ScalableImageDecoder::frameIsCompleteAtIndex(size_t index) const
{
    Locker locker { m_lock };
    if (index >= m_frameBufferCache.size())
        return false;

    auto& frame = m_frameBufferCache[index];
    return frame.isComplete();
}

bool ScalableImageDecoder::frameHasAlphaAtIndex(size_t index) const
{
    Locker locker { m_lock };
    if (m_frameBufferCache.size() <= index)
        return true;

    auto& frame = m_frameBufferCache[index];
    if (!frame.isComplete())
        return true;
    return frame.hasAlpha();
}

unsigned ScalableImageDecoder::frameBytesAtIndex(size_t index, SubsamplingLevel) const
{
    Locker locker { m_lock };
    if (m_frameBufferCache.size() <= index)
        return 0;
    // FIXME: Use the dimension of the requested frame.
    return m_size.area() * sizeof(uint32_t);
}

Seconds ScalableImageDecoder::frameDurationAtIndex(size_t index) const
{
    Locker locker { m_lock };
    if (index >= m_frameBufferCache.size())
        return 0_s;

    auto& frame = m_frameBufferCache[index];
    if (!frame.isComplete())
        return 0_s;

    // Many annoying ads specify a 0 duration to make an image flash as quickly as possible.
    // We follow Firefox's behavior and use a duration of 100 ms for any frames that specify
    // a duration of <= 10 ms. See <rdar://problem/7689300> and <http://webkit.org/b/36082>
    // for more information.
    Seconds duration = frame.duration();
    if (duration < 11_ms)
        return 100_ms;
    return duration;
}

PlatformImagePtr ScalableImageDecoder::createFrameImageAtIndex(size_t index, SubsamplingLevel, const DecodingOptions&)
{
    Locker locker { m_lock };
    // Zero-height images can cause problems for some ports. If we have an empty image dimension, just bail.
    if (size().isEmpty())
        return nullptr;

    auto* buffer = frameBufferAtIndex(index);
    if (!buffer || buffer->isInvalid() || !buffer->hasBackingStore())
        return nullptr;

    // Return the buffer contents as a native image. For some ports, the data
    // is already in a native container, and this just increments its refcount.
    return buffer->backingStore()->image();
}

#if USE(DIRECT2D)
void ScalableImageDecoder::setTargetContext(ID2D1RenderTarget*)
{
    notImplemented();
}
#endif

}
