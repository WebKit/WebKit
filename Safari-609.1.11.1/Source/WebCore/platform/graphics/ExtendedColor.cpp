/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "ExtendedColor.h"

#include "ColorSpace.h"
#include <wtf/MathExtras.h>
#include <wtf/dtoa.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

Ref<ExtendedColor> ExtendedColor::create(float r, float g, float b, float a, ColorSpace colorSpace)
{
    return adoptRef(*new ExtendedColor(r, g, b, a, colorSpace));
}

String ExtendedColor::cssText() const
{
    StringBuilder builder;
    builder.reserveCapacity(40);
    builder.appendLiteral("color(");

    switch (m_colorSpace) {
    case ColorSpaceSRGB:
        builder.appendLiteral("srgb ");
        break;
    case ColorSpaceDisplayP3:
        builder.appendLiteral("display-p3 ");
        break;
    default:
        ASSERT_NOT_REACHED();
        return WTF::emptyString();
    }

    builder.appendFixedPrecisionNumber(red());
    builder.append(' ');

    builder.appendFixedPrecisionNumber(green());
    builder.append(' ');

    builder.appendFixedPrecisionNumber(blue());
    if (!WTF::areEssentiallyEqual(alpha(), 1.0f)) {
        builder.appendLiteral(" / ");
        builder.appendFixedPrecisionNumber(alpha());
    }
    builder.append(')');

    return builder.toString();
}

}
