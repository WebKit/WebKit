/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSMaskBorderSourceValue.h"

#include "CSSValuePool.h"
#include "CSSValueTypes+DeprecatedCSSOMValueCreation.h"

namespace WebCore {

CSSMaskBorderSourceValue::CSSMaskBorderSourceValue(CSS::MaskBorderSource&& source)
    : CSSValue(ClassType::MaskBorderSource)
    , m_source(WTF::move(source))
{
}

CSSMaskBorderSourceValue::~CSSMaskBorderSourceValue() = default;

Ref<CSSMaskBorderSourceValue> CSSMaskBorderSourceValue::create(CSS::MaskBorderSource&& source)
{
    return adoptRef(*new CSSMaskBorderSourceValue(WTF::move(source)));
}

String CSSMaskBorderSourceValue::customCSSText(const CSS::SerializationContext& context) const
{
    return CSS::serializationForCSS(context, m_source);
}

bool CSSMaskBorderSourceValue::equals(const CSSMaskBorderSourceValue& other) const
{
    return m_source == other.m_source;
}

bool CSSMaskBorderSourceValue::customTraverseSubresources(NOESCAPE const Function<bool(const CachedResource&)>& handler) const
{
    return WTF::switchOn(m_source,
        [&](CSS::Keyword::None) {
            return false;
        },
        [&](const CSS::ImageWrapper& imageWrapper) {
            return protect(imageWrapper.value)->traverseSubresources(handler);
        }
    );
}

IterationStatus CSSMaskBorderSourceValue::customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>& function) const
{
    return CSS::visitCSSValueChildren(function, m_source);
}

Ref<DeprecatedCSSOMValue> CSSMaskBorderSourceValue::customCreateDeprecatedCSSOMWrapper(CSSStyleDeclaration& owner) const
{
    return CSS::createDeprecatedCSSOMValue(CSSValuePool::singleton(), owner, m_source);
}

} // namespace WebCore
