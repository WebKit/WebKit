/*
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ImageData.h"

#include <JavaScriptCore/JSCInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

Checked<unsigned, RecordOverflow> ImageData::dataSize(const IntSize& size)
{
    Checked<unsigned, RecordOverflow> checkedDataSize = 4;
    checkedDataSize *= static_cast<unsigned>(size.width());
    checkedDataSize *= static_cast<unsigned>(size.height());
    return checkedDataSize;
}

ExceptionOr<Ref<ImageData>> ImageData::create(unsigned sw, unsigned sh)
{
    if (!sw || !sh)
        return Exception { IndexSizeError };
    IntSize size(sw, sh);
    auto dataSize = ImageData::dataSize(size);
    if (dataSize.hasOverflowed())
        return Exception { RangeError, "Cannot allocate a buffer of this size"_s };
    auto byteArray = Uint8ClampedArray::tryCreateUninitialized(dataSize.unsafeGet());
    if (!byteArray) {
        // FIXME: Does this need to be a "real" out of memory error with setOutOfMemoryError called on it?
        return Exception { RangeError, "Out of memory"_s };
    }
    byteArray->zeroFill();
    return adoptRef(*new ImageData(size, byteArray.releaseNonNull()));
}

RefPtr<ImageData> ImageData::create(const IntSize& size)
{
    auto dataSize = ImageData::dataSize(size);
    if (dataSize.hasOverflowed())
        return nullptr;
    auto byteArray = Uint8ClampedArray::tryCreateUninitialized(dataSize.unsafeGet());
    if (!byteArray)
        return nullptr;
    return adoptRef(*new ImageData(size, byteArray.releaseNonNull()));
}

RefPtr<ImageData> ImageData::create(const IntSize& size, Ref<Uint8ClampedArray>&& byteArray)
{
    auto dataSize = ImageData::dataSize(size);
    if (dataSize.hasOverflowed() || dataSize.unsafeGet() > byteArray->length())
        return nullptr;
    return adoptRef(*new ImageData(size, WTFMove(byteArray)));
}

ExceptionOr<Ref<ImageData>> ImageData::create(Ref<Uint8ClampedArray>&& byteArray, unsigned sw, Optional<unsigned> sh)
{
    unsigned length = byteArray->length();
    if (!length || length % 4)
        return Exception { InvalidStateError, "Length is not a non-zero multiple of 4"_s };

    length /= 4;
    if (!sw || length % sw)
        return Exception { IndexSizeError, "Length is not a multiple of sw"_s };

    unsigned height = length / sw;
    if (sh && sh.value() != height)
        return Exception { IndexSizeError, "sh value is not equal to height"_s };

    auto result = create(IntSize(sw, height), WTFMove(byteArray));
    if (!result)
        return Exception { RangeError };
    return result.releaseNonNull();
}

ImageData::ImageData(const IntSize& size, Ref<Uint8ClampedArray>&& byteArray)
    : m_size(size)
    , m_data(WTFMove(byteArray))
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION((size.area() * 4).unsafeGet() <= m_data->length());
}

Ref<ImageData> ImageData::deepClone() const
{
    return adoptRef(*new ImageData(m_size, Uint8ClampedArray::create(m_data->data(), m_data->length())));
}

TextStream& operator<<(TextStream& ts, const ImageData& imageData)
{
    // Print out the address of the pixel data array
    return ts << imageData.data();
}

}
