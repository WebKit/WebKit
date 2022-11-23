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

#include "Rect.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

CSSBorderImageWidthValue::CSSBorderImageWidthValue(Ref<Quad>&& widths, bool overridesBorderWidths)
    : CSSValue(BorderImageWidthClass)
    , m_widths(WTFMove(widths))
    , m_overridesBorderWidths(overridesBorderWidths)
{
}

String CSSBorderImageWidthValue::customCSSText() const
{
    // border-image-width can't set m_overridesBorderWidths to true by itself, so serialize as empty string.
    // It can only be true via the -webkit-border-image shorthand, whose serialization will unwrap widths() if needed.
    if (m_overridesBorderWidths)
        return emptyString();

    return m_widths->cssText();
}

bool CSSBorderImageWidthValue::equals(const CSSBorderImageWidthValue& other) const
{
    return m_overridesBorderWidths == other.m_overridesBorderWidths && m_widths->equals(other.m_widths);
}

} // namespace WebCore
