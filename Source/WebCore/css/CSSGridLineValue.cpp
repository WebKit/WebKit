/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "CSSGridLineValue.h"

#include "CSSPrimitiveNumericTypes+Serialization.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSGridLineValue::CSSGridLineValue(std::optional<CSS::Keyword::Span> span, std::optional<CSS::Integer<>>&& numeric, std::optional<CSS::CustomIdent>&& gridLineName)
    : CSSValue(ClassType::GridLineValue)
    , m_span(span)
    , m_numeric(WTF::move(numeric))
    , m_gridLineName(WTF::move(gridLineName))
{
}

Ref<CSSGridLineValue> CSSGridLineValue::create(std::optional<CSS::Keyword::Span> span, std::optional<CSS::Integer<>>&& numeric, std::optional<CSS::CustomIdent>&& gridLineName)
{
    return adoptRef(*new CSSGridLineValue(span, WTF::move(numeric), WTF::move(gridLineName)));
}

bool CSSGridLineValue::equals(const CSSGridLineValue& other) const
{
    if (m_span != other.m_span)
        return false;
    if (m_numeric != other.m_numeric)
        return false;
    if (m_gridLineName != other.m_gridLineName)
        return false;
    return true;
}

String CSSGridLineValue::customCSSText(const CSS::SerializationContext& context) const
{
    using SerializationType = SpaceSeparatedTuple<
        std::optional<CSS::Keyword::Span>,
        std::optional<CSS::Integer<>>,
        std::optional<CSS::CustomIdent>
    >;

    // Only return the numeric value if not 1, or if it provided without a span value.
    // https://drafts.csswg.org/css-grid-2/#grid-placement-span-int

    return CSS::serializationForCSS(context, SerializationType {
        m_span,
        m_numeric && (m_numeric->raw() != 1 || !m_span || !m_gridLineName) ? m_numeric : std::nullopt,
        m_gridLineName
    });
}

} // namespace WebCore
