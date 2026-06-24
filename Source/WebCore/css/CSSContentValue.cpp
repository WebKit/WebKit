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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSContentValue.h"

#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "CSSValuePool.h"
#include "CSSValueTypes+DeprecatedCSSOMValueCreation.h"
#include "DeprecatedCSSOMValue.h"

namespace WebCore {

CSSContentValue::CSSContentValue(CSS::Content&& content)
    : CSSValue(ClassType::Content)
    , m_content(WTF::move(content))
{
}

CSSContentValue::~CSSContentValue() = default;

Ref<CSSContentValue> CSSContentValue::create(CSS::Content&& content)
{
    return adoptRef(*new CSSContentValue(WTF::move(content)));
}

String CSSContentValue::customCSSText(const CSS::SerializationContext& context) const
{
    return CSS::serializationForCSS(context, m_content);
}

bool CSSContentValue::equals(const CSSContentValue& other) const
{
    return m_content == other.m_content;
}

Ref<DeprecatedCSSOMValue> CSSContentValue::customCreateDeprecatedCSSOMWrapper(CSSStyleDeclaration& owner) const
{
    return CSS::createDeprecatedCSSOMValue(CSSValuePool::singleton(), owner, m_content);
}

} // namespace WebCore
