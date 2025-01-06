/*
 * Copyright (C) 2024 Alexsander Borges Damaceno <alexbdamac@gmail.com>. All rights reserved.
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
#include "CSSAttrValue.h"

#include <wtf/text/MakeString.h>
#include "CSSPrimitiveValue.h"

namespace WebCore {

Ref<CSSAttrValue> CSSAttrValue::create(String attributeName, RefPtr<CSSValue>&& fallback)
{
    return adoptRef(*new CSSAttrValue(WTFMove(attributeName), WTFMove(fallback)));
}

bool CSSAttrValue::equals(const CSSAttrValue& other) const
{
    RefPtr fallback = dynamicDowncast<CSSPrimitiveValue>(m_fallback);
    RefPtr otherFallback = dynamicDowncast<CSSPrimitiveValue>(other.m_fallback);

    if (fallback && otherFallback)
        return m_attributeName == other.m_attributeName && fallback->stringValue() == otherFallback->stringValue();
    if (fallback || otherFallback)
        return false;
    return m_attributeName == other.m_attributeName;
}

String CSSAttrValue::customCSSText() const
{
    RefPtr fallback = dynamicDowncast<CSSPrimitiveValue>(m_fallback);
    return makeString(
        "attr("_s,
        m_attributeName.impl(),
        fallback && !fallback->stringValue().isEmpty() ? ", "_s : ""_s,
        fallback && !fallback->stringValue().isEmpty() ? fallback->cssText() : ""_s,
        ')'
    );
}

} // namespace WebCore
