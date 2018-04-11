/*
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
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
#include "ScalableImageDecoderFrame.h"

#include <wtf/NeverDestroyed.h>

namespace WebCore {

ScalableImageDecoderFrame::ScalableImageDecoderFrame()
{
}

ScalableImageDecoderFrame::~ScalableImageDecoderFrame()
{
}

ScalableImageDecoderFrame& ScalableImageDecoderFrame::operator=(const ScalableImageDecoderFrame& other)
{
    if (this == &other)
        return *this;

    m_decodingStatus = other.m_decodingStatus;

    if (other.backingStore())
        initialize(*other.backingStore());
    else
        m_backingStore = nullptr;
    m_disposalMethod = other.m_disposalMethod;

    m_orientation = other.m_orientation;
    m_duration = other.m_duration;
    m_hasAlpha = other.m_hasAlpha;
    return *this;
}

void ScalableImageDecoderFrame::setDecodingStatus(DecodingStatus decodingStatus)
{
    ASSERT(decodingStatus != DecodingStatus::Decoding);
    m_decodingStatus = decodingStatus;
}

DecodingStatus ScalableImageDecoderFrame::decodingStatus() const
{
    ASSERT(m_decodingStatus != DecodingStatus::Decoding);
    return m_decodingStatus;
}

void ScalableImageDecoderFrame::clear()
{
    *this = ScalableImageDecoderFrame();
}

bool ScalableImageDecoderFrame::initialize(const ImageBackingStore& backingStore)
{
    if (&backingStore == this->backingStore())
        return true;

    m_backingStore = ImageBackingStore::create(backingStore);
    return m_backingStore != nullptr;
}

bool ScalableImageDecoderFrame::initialize(const IntSize& size, bool premultiplyAlpha)
{
    if (size.isEmpty())
        return false;

    m_backingStore = ImageBackingStore::create(size, premultiplyAlpha);
    return m_backingStore != nullptr;
}

IntSize ScalableImageDecoderFrame::size() const
{
    if (hasBackingStore())
        return backingStore()->size();
    return { };
}

}
