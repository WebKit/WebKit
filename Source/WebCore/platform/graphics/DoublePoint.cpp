/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "DoublePoint.h"

#include "IntPoint.h"
#include <wtf/JSONValues.h>
#include <wtf/text/TextStream.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

DoublePoint::DoublePoint(const IntPoint& p)
    : m_x(p.x()), m_y(p.y())
{
}

#if USE(CG)

DoublePoint::DoublePoint(const CGPoint& p)
    : m_x(p.x), m_y(p.y)
{
}

DoublePoint::operator CGPoint() const
{
    return CGPointMake(static_cast<CGFloat>(m_x), static_cast<CGFloat>(m_y));
}

#endif

#if PLATFORM(WIN)

DoublePoint::DoublePoint(const POINT& p)
    : m_x(p.x), m_y(p.y)
{
}

#endif

Ref<JSON::Object> DoublePoint::toJSONObject() const
{
    auto object = JSON::Object::create();

    object->setDouble("x"_s, m_x);
    object->setDouble("y"_s, m_y);

    return object;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const DoublePoint& point)
{
    return ts << "(" << point.x() << "," << point.y() << ")";
}

} // namespace WebCore

