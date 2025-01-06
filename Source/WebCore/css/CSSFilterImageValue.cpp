/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2021 Apple Inc. All right reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
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
#include "CSSFilterImageValue.h"

#include "CSSPrimitiveNumericTypes+CSSValueVisitation.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "StyleBuilderState.h"
#include "StyleFilterImage.h"
#include <wtf/text/MakeString.h>

namespace WebCore {

CSSFilterImageValue::CSSFilterImageValue(Ref<CSSValue>&& imageValueOrNone, CSS::FilterProperty&& filter)
    : CSSValue { ClassType::FilterImage }
    , m_imageValueOrNone { WTFMove(imageValueOrNone) }
    , m_filter { WTFMove(filter) }
{
}

CSSFilterImageValue::~CSSFilterImageValue() = default;

bool CSSFilterImageValue::equals(const CSSFilterImageValue& other) const
{
    return equalInputImages(other) && m_filter == other.m_filter;
}

bool CSSFilterImageValue::equalInputImages(const CSSFilterImageValue& other) const
{
    return compareCSSValue(m_imageValueOrNone, other.m_imageValueOrNone);
}

String CSSFilterImageValue::customCSSText() const
{
    return makeString("filter("_s, m_imageValueOrNone->cssText(), ", "_s, CSS::serializationForCSS(m_filter), ')');
}

IterationStatus CSSFilterImageValue::customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
{
    if (func(m_imageValueOrNone.get()) == IterationStatus::Done)
        return IterationStatus::Done;
    if (CSS::visitCSSValueChildren(func, m_filter) == IterationStatus::Done)
        return IterationStatus::Done;
    return IterationStatus::Continue;
}

RefPtr<StyleImage> CSSFilterImageValue::createStyleImage(const Style::BuilderState& state) const
{
    return StyleFilterImage::create(state.createStyleImage(m_imageValueOrNone), state.createFilterOperations(m_filter));
}

} // namespace WebCore
