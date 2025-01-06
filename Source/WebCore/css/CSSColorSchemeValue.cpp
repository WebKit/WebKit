/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
#include "CSSColorSchemeValue.h"

#if ENABLE(DARK_MODE_CSS)

namespace WebCore {

Ref<CSSColorSchemeValue> CSSColorSchemeValue::create(CSS::ColorScheme colorScheme)
{
    return adoptRef(*new CSSColorSchemeValue(WTFMove(colorScheme)));
}

CSSColorSchemeValue::CSSColorSchemeValue(CSS::ColorScheme colorScheme)
    : CSSValue(ClassType::ColorScheme)
    , m_colorScheme(WTFMove(colorScheme))
{
}

String CSSColorSchemeValue::customCSSText() const
{
    return CSS::serializationForCSS(m_colorScheme);
}

bool CSSColorSchemeValue::equals(const CSSColorSchemeValue& other) const
{
    return m_colorScheme == other.m_colorScheme;
}

IterationStatus CSSColorSchemeValue::customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
{
    return CSS::visitCSSValueChildren(func, m_colorScheme);
}

} // namespace WebCore

#endif
