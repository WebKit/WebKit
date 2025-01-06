/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
#include "DeprecatedCSSOMBoxShadowValue.h"

#include "CSSPrimitiveNumericTypes+Serialization.h"

namespace WebCore {

Ref<DeprecatedCSSOMBoxShadowValue> DeprecatedCSSOMBoxShadowValue::create(CSS::BoxShadow shadow, CSSStyleDeclaration& owner)
{
    return adoptRef(*new DeprecatedCSSOMBoxShadowValue(WTFMove(shadow), owner));
}

DeprecatedCSSOMBoxShadowValue::DeprecatedCSSOMBoxShadowValue(CSS::BoxShadow&& shadow, CSSStyleDeclaration& owner)
    : DeprecatedCSSOMValue(ClassType::BoxShadow, owner)
    , m_shadow(WTFMove(shadow))
{
}

String DeprecatedCSSOMBoxShadowValue::cssText() const
{
    return CSS::serializationForCSS(m_shadow);
}

} // namespace WebCore
