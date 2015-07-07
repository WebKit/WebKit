/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "ExceptionCode.h"
#include <runtime/JSCInlines.h>
#include <runtime/TypedArrayInlines.h>

namespace WebCore {

PassRefPtr<ImageData> ImageData::create(unsigned sw, unsigned sh, ExceptionCode& ec)
{
    if (!sw || !sh) {
        ec = INDEX_SIZE_ERR;
        return nullptr;
    }

    Checked<int, RecordOverflow> dataSize = 4;
    dataSize *= sw;
    dataSize *= sh;
    if (dataSize.hasOverflowed()) {
        ec = TypeError;
        return nullptr;
    }

    IntSize size(sw, sh);
    RefPtr<ImageData> data = adoptRef(new ImageData(size));
    data->data()->zeroFill();
    return data.release();
}

PassRefPtr<ImageData> ImageData::create(const IntSize& size)
{
    Checked<int, RecordOverflow> dataSize = 4;
    dataSize *= size.width();
    dataSize *= size.height();
    if (dataSize.hasOverflowed())
        return 0;

    return adoptRef(new ImageData(size));
}

PassRefPtr<ImageData> ImageData::create(const IntSize& size, PassRefPtr<Uint8ClampedArray> byteArray)
{
    Checked<int, RecordOverflow> dataSize = 4;
    dataSize *= size.width();
    dataSize *= size.height();
    if (dataSize.hasOverflowed())
        return 0;

    if (dataSize.unsafeGet() < 0
        || static_cast<unsigned>(dataSize.unsafeGet()) > byteArray->length())
        return 0;

    return adoptRef(new ImageData(size, byteArray));
}

PassRefPtr<ImageData> ImageData::create(PassRefPtr<Uint8ClampedArray> byteArray, unsigned sw, unsigned sh, ExceptionCode& ec)
{
    unsigned length = byteArray->length();
    if (!length || length % 4 != 0) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    if (!sw) {
        ec = INDEX_SIZE_ERR;
        return nullptr;
    }

    length /= 4;
    if (length % sw != 0) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    unsigned height = length / sw;
    if (sh && sh != height) {
        ec = INDEX_SIZE_ERR;
        return nullptr;
    }

    return create(IntSize(sw, height), byteArray);
}

ImageData::ImageData(const IntSize& size)
    : m_size(size)
    , m_data(Uint8ClampedArray::createUninitialized(size.width() * size.height() * 4))
{
}

ImageData::ImageData(const IntSize& size, PassRefPtr<Uint8ClampedArray> byteArray)
    : m_size(size)
    , m_data(byteArray)
{
    ASSERT_WITH_SECURITY_IMPLICATION(static_cast<unsigned>(size.width() * size.height() * 4) <= m_data->length());
}

}

