/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "APIGeometry.h"

#include "Decoder.h"
#include "Encoder.h"

namespace API {

void Point::encode(IPC::Encoder& encoder) const
{
    encoder << m_point.x;
    encoder << m_point.y;
}

bool Point::decode(IPC::Decoder& decoder, RefPtr<API::Object>& result)
{
    WKPoint point;
    if (!decoder.decode(point.x))
        return false;
    if (!decoder.decode(point.y))
        return false;

    result = Point::create(point);
    return true;
}


void Size::encode(IPC::Encoder& encoder) const
{
    encoder << m_size.width;
    encoder << m_size.height;
}

bool Size::decode(IPC::Decoder& decoder, RefPtr<API::Object>& result)
{
    WKSize size;
    if (!decoder.decode(size.width))
        return false;
    if (!decoder.decode(size.height))
        return false;

    result = Size::create(size);
    return true;
}


void Rect::encode(IPC::Encoder& encoder) const
{
    encoder << m_rect.origin.x;
    encoder << m_rect.origin.y;
    encoder << m_rect.size.width;
    encoder << m_rect.size.height;
}

bool Rect::decode(IPC::Decoder& decoder, RefPtr<API::Object>& result)
{
    WKRect rect;
    if (!decoder.decode(rect.origin.x))
        return false;
    if (!decoder.decode(rect.origin.y))
        return false;
    if (!decoder.decode(rect.size.width))
        return false;
    if (!decoder.decode(rect.size.height))
        return false;

    result = Rect::create(rect);
    return true;
}

} // namespace API
