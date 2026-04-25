/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSFontFamilyNameValue.h"

namespace WebCore {

Ref<CSSFontFamilyNameValue> CSSFontFamilyNameValue::create(CSS::FontFamilyName fontFamilyName)
{
    return adoptRef(*new CSSFontFamilyNameValue(WTF::move(fontFamilyName)));
}

CSSFontFamilyNameValue::CSSFontFamilyNameValue(CSS::FontFamilyName&& fontFamilyName)
    : CSSValue(ClassType::FontFamilyName)
    , m_fontFamilyName(WTF::move(fontFamilyName))
{
}

String CSSFontFamilyNameValue::customCSSText(const CSS::SerializationContext& context) const
{
    return CSS::serializationForCSS(context, m_fontFamilyName);
}

bool CSSFontFamilyNameValue::equals(const CSSFontFamilyNameValue& other) const
{
    return m_fontFamilyName == other.m_fontFamilyName;
}

IterationStatus CSSFontFamilyNameValue::customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
{
    return CSS::visitCSSValueChildren(func, m_fontFamilyName);
}

String CSSFontFamilyNameValue::stringValue() const
{
    return m_fontFamilyName.value;
}

} // namespace WebCore
