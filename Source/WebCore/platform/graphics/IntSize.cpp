/*
 * Copyright (C) 2013 Apple Inc.  All rights reserved.
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
#include "IntSize.h"

#include "FloatSize.h"
#include <wtf/JSONValues.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

IntSize::IntSize(const FloatSize& s)
    : m_width(clampToInteger(s.width()))
    , m_height(clampToInteger(s.height()))
{
}

IntSize IntSize::constrainedBetween(const IntSize& min, const IntSize& max) const
{
    return {
        std::max(min.width(), std::min(max.width(), m_width)),
        std::max(min.height(), std::min(max.height(), m_height))
    };
}

TextStream& operator<<(TextStream& ts, const IntSize& size)
{
    return ts << "width=" << size.width() << " height=" << size.height();
}

Ref<JSON::Object> IntSize::toJSONObject() const
{
    auto object = JSON::Object::create();

    object->setDouble("width"_s, m_width);
    object->setDouble("height"_s, m_height);

    return object;
}

String IntSize::toJSONString() const
{
    return toJSONObject()->toJSONString();
}

}
