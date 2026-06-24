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
#include "CSSBorderImageSourceValue.h"

#include "CSSValuePool.h"
#include "CSSValueTypes+DeprecatedCSSOMValueCreation.h"

namespace WebCore {

CSSBorderImageSourceValue::CSSBorderImageSourceValue(CSS::BorderImageSource&& source)
    : CSSValue(ClassType::BorderImageSource)
    , m_source(WTF::move(source))
{
}

CSSBorderImageSourceValue::~CSSBorderImageSourceValue() = default;

Ref<CSSBorderImageSourceValue> CSSBorderImageSourceValue::create(CSS::BorderImageSource&& source)
{
    return adoptRef(*new CSSBorderImageSourceValue(WTF::move(source)));
}

String CSSBorderImageSourceValue::customCSSText(const CSS::SerializationContext& context) const
{
    return CSS::serializationForCSS(context, m_source);
}

bool CSSBorderImageSourceValue::equals(const CSSBorderImageSourceValue& other) const
{
    return m_source == other.m_source;
}

bool CSSBorderImageSourceValue::customTraverseSubresources(NOESCAPE const Function<bool(const CachedResource&)>& handler) const
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

IterationStatus CSSBorderImageSourceValue::customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>& function) const
{
    return CSS::visitCSSValueChildren(function, m_source);
}

Ref<DeprecatedCSSOMValue> CSSBorderImageSourceValue::customCreateDeprecatedCSSOMWrapper(CSSStyleDeclaration& owner) const
{
    return CSS::createDeprecatedCSSOMValue(CSSValuePool::singleton(), owner, m_source);
}

} // namespace WebCore
