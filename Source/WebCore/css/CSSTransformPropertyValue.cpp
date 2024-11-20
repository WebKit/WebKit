/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "CSSTransformPropertyValue.h"

#include "CSSPrimitiveNumericTypes+CSSValueVisitation.h"
#include "CSSPrimitiveNumericTypes+ComputedStyleDependencies.h"
#include "CSSPrimitiveNumericTypes+Hashing.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "CSSPrimitiveNumericTypes.h"
#include "CSSTransformFunctionValue.h"
#include "DeprecatedCSSOMValueList.h"

namespace WebCore {

String CSSTransformPropertyValue::customCSSText() const
{
    return CSS::serializationForCSS(m_transform);
}

bool CSSTransformPropertyValue::equals(const CSSTransformPropertyValue& other) const
{
    return m_transform == other.m_transform;
}

IterationStatus CSSTransformPropertyValue::customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
{
    return CSS::visitCSSValueChildren(func, m_transform);
}

void CSSTransformPropertyValue::customCollectComputedStyleDependencies(ComputedStyleDependencies& dependencies) const
{
    CSS::collectComputedStyleDependencies(dependencies, m_transform);
}

bool CSSTransformPropertyValue::addDerivedHash(Hasher& hasher) const
{
    CSS::hash(hasher, m_transform);
    return true;
}

Ref<DeprecatedCSSOMValue> CSSTransformPropertyValue::createDeprecatedCSSOMWrapper(CSSStyleDeclaration& styleDeclaration) const
{
    // We expose this as a DeprecatedCSSOMValueList in CSSOM to maintain old behavior.

    DeprecatedCSSOMValueList::StorageVector resultVector;
    for (auto function : m_transform.list)
        resultVector.append(DeprecatedCSSOMComplexValue::create(CSSTransformFunctionValue::create(function.asVariant()), styleDeclaration));

    return DeprecatedCSSOMValueList::create(
        WTFMove(resultVector),
        CSSValue::SpaceSeparator,
        styleDeclaration
    );
}

} // namespace WebCore
