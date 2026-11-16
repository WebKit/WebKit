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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DeprecatedCSSOMCustomValue.h"

#include "CSSSerializationContext.h"

namespace WebCore {

Ref<DeprecatedCSSOMCustomValue> DeprecatedCSSOMCustomValue::create(SerializationFunctor&& functor, CSSStyleDeclaration& owner)
{
    return adoptRef(*new DeprecatedCSSOMCustomValue(WTF::move(functor), owner));
}

DeprecatedCSSOMCustomValue::DeprecatedCSSOMCustomValue(SerializationFunctor&& functor, CSSStyleDeclaration& owner)
    : DeprecatedCSSOMValue(owner)
    , m_serializationFunctor(WTF::move(functor))
{
}

String DeprecatedCSSOMCustomValue::cssText() const
{
    return m_serializationFunctor(CSS::defaultSerializationContext());
}

unsigned short DeprecatedCSSOMCustomValue::cssValueType() const
{
    return CSS_CUSTOM;
}

} // namespace WebCore
