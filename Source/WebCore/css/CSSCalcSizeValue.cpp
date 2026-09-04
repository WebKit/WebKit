/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "CSSCalcSizeValue.h"

#include "CSSSerializationContext.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

Ref<CSSCalcSizeValue> CSSCalcSizeValue::create(CSS::CalcSize&& calcSize)
{
    return adoptRef(*new CSSCalcSizeValue(WTF::move(calcSize)));
}

CSSCalcSizeValue::CSSCalcSizeValue(CSS::CalcSize&& calcSize)
    : CSSValue(ClassType::CalcSize)
    , m_calcSize(WTF::move(calcSize))
{
}

String CSSCalcSizeValue::customCSSText(const CSS::SerializationContext& context) const
{
    StringBuilder builder;
    CSS::serializationForCSS(builder, context, m_calcSize);
    return builder.toString();
}

void CSSCalcSizeValue::collectComputedStyleDependencies(ComputedStyleDependencies& dependencies) const
{
    m_calcSize.collectComputedStyleDependencies(dependencies);
}

bool CSSCalcSizeValue::equals(const CSSCalcSizeValue& other) const
{
    return m_calcSize == other.m_calcSize;
}

} // namespace WebCore
