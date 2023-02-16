/*
 * Copyright (C) 2022 Igalia S.L. All rights reserved.
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
#include "CSSBorderImageWidthValue.h"

#include <wtf/text/WTFString.h>

namespace WebCore {

CSSBorderImageWidthValue::CSSBorderImageWidthValue(Quad widths, bool overridesBorderWidths)
    : CSSValue(BorderImageWidthClass)
    , m_widths(WTFMove(widths))
    , m_overridesBorderWidths(overridesBorderWidths)
{
}

CSSBorderImageWidthValue::~CSSBorderImageWidthValue() = default;

Ref<CSSBorderImageWidthValue> CSSBorderImageWidthValue::create(Quad widths, bool overridesBorderWidths)
{
    return adoptRef(*new CSSBorderImageWidthValue(WTFMove(widths), overridesBorderWidths));
}

String CSSBorderImageWidthValue::customCSSText() const
{
    // The border-image-width longhand can't set m_overridesBorderWidths to true, so serialize as empty string.
    // This can only be created by the -webkit-border-image shorthand, which will not serialize as empty string in this case.
    // This is an unconventional relationship between a longhand and a shorthand, which we may want to revise.
    if (m_overridesBorderWidths)
        return String();
    return m_widths.cssText();
}

bool CSSBorderImageWidthValue::equals(const CSSBorderImageWidthValue& other) const
{
    return m_overridesBorderWidths == other.m_overridesBorderWidths && m_widths.equals(other.m_widths);
}

} // namespace WebCore
