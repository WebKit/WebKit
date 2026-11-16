/*
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
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
#include "CSSCrossfadeValue.h"

#include "CSSPrimitiveNumericTypes+CSSValueVisitation.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "StyleBuilderState.h"
#include "StyleCrossfadeImage.h"
#include "StylePrimitiveNumericTypes+Conversions.h"

namespace WebCore {

inline CSSCrossfadeValue::CSSCrossfadeValue(CSS::CrossfadeFunction&& function)
    : CSSValue { ClassType::Crossfade }
    , m_function { WTF::move(function) }
{
}

inline CSSCrossfadeValue::CSSCrossfadeValue(CSS::WebkitCrossfadeFunction&& function)
    : CSSValue { ClassType::Crossfade }
    , m_function { WTF::move(function) }
{
}

Ref<CSSCrossfadeValue> CSSCrossfadeValue::create(CSS::CrossfadeFunction&& function)
{
    return adoptRef(*new CSSCrossfadeValue(WTF::move(function)));
}

Ref<CSSCrossfadeValue> CSSCrossfadeValue::create(CSS::WebkitCrossfadeFunction&& function)
{
    return adoptRef(*new CSSCrossfadeValue(WTF::move(function)));
}

CSSCrossfadeValue::~CSSCrossfadeValue() = default;

bool CSSCrossfadeValue::equals(const CSSCrossfadeValue& other) const
{
    return m_function == other.m_function;
}

String CSSCrossfadeValue::customCSSText(const CSS::SerializationContext& context) const
{
    return WTF::switchOn(m_function,
        [&](const auto& function) {
            return CSS::serializationForCSS(context, function);
        }
    );
}

IterationStatus CSSCrossfadeValue::customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>& func) const
{
    return WTF::switchOn(m_function,
        [&](const auto& function) {
            return CSS::visitCSSValueChildren(func, function);
        }
    );
}

RefPtr<Style::Image> CSSCrossfadeValue::createStyleImage(const Style::BuilderState& state) const
{
    return WTF::switchOn(m_function,
        [&](const auto& function) -> RefPtr<Style::Image> {
            return Style::CrossfadeImage::create(Style::toStyle(function, state));
        }
    );
}

} // namespace WebCore
