/*
 * Copyright (C) 2019 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSGridIntegerRepeatValue.h"

#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSGridIntegerRepeatValue::CSSGridIntegerRepeatValue(Ref<CSSPrimitiveValue>&& repetitions, CSSValueListBuilder builder)
    : CSSValueContainingVector(ClassType::GridIntegerRepeat, SpaceSeparator, WTFMove(builder))
    , m_repetitions(WTFMove(repetitions))
{
}

Ref<CSSGridIntegerRepeatValue> CSSGridIntegerRepeatValue::create(Ref<CSSPrimitiveValue>&& repetitions, CSSValueListBuilder builder)
{
    return adoptRef(*new CSSGridIntegerRepeatValue(WTFMove(repetitions), WTFMove(builder)));
}

String CSSGridIntegerRepeatValue::customCSSText(const CSS::SerializationContext& context) const
{
    StringBuilder result;
    result.append("repeat("_s, m_repetitions->cssText(context), ", "_s);
    serializeItems(result, context);
    result.append(')');
    return result.toString();
}

bool CSSGridIntegerRepeatValue::equals(const CSSGridIntegerRepeatValue& other) const
{
    return compareCSSValue(m_repetitions, other.m_repetitions) && itemsEqual(other);
}

} // namespace WebCore
