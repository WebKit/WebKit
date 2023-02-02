/*
 * Copyright (C) 2003-2023 Apple Inc.  All rights reserved.§
 * Copyright (C) 2014 Google Inc.  All rights reserved.
 * Copyright (C) 2005 Nokia.  All rights reserved.
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
#include "FloatSize.h"

#include "FloatConversion.h"
#include "IntSize.h"
#include <limits>
#include <math.h>
#include <wtf/JSONValues.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

FloatSize FloatSize::constrainedBetween(const FloatSize& min, const FloatSize& max) const
{
    return {
        std::max(min.width(), std::min(max.width(), m_width)),
        std::max(min.height(), std::min(max.height(), m_height))
    };
}

bool FloatSize::isZero() const
{
    return fabs(m_width) < std::numeric_limits<float>::epsilon() && fabs(m_height) < std::numeric_limits<float>::epsilon();
}

bool FloatSize::isExpressibleAsIntSize() const
{
    return isWithinIntRange(m_width) && isWithinIntRange(m_height);
}

FloatSize FloatSize::narrowPrecision(double width, double height)
{
    return FloatSize(narrowPrecisionToFloat(width), narrowPrecisionToFloat(height));
}

TextStream& operator<<(TextStream& ts, const FloatSize& size)
{
    return ts << "width=" << TextStream::FormatNumberRespectingIntegers(size.width())
        << " height=" << TextStream::FormatNumberRespectingIntegers(size.height());
}

Ref<JSON::Object> FloatSize::toJSONObject() const
{
    auto object = JSON::Object::create();

    object->setDouble("width"_s, m_width);
    object->setDouble("height"_s, m_height);

    return object;
}

String FloatSize::toJSONString() const
{
    return toJSONObject()->toJSONString();
}

}
