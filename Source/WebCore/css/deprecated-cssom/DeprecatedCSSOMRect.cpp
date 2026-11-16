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
#include "DeprecatedCSSOMRect.h"

#include "CSSClip.h"
#include "CSSPrimitiveNumericTypes+DeprecatedCSSOMValueCreation.h"

namespace WebCore {

static Ref<DeprecatedCSSOMPrimitiveValue> makeDeprecatedCSSOMPrimitiveValueForClipEdge(const CSS::ClipEdge& edge, CSSStyleDeclaration& owner)
{
    return WTF::switchOn(edge,
        [&](CSS::Keyword::Auto keyword) {
            return DeprecatedCSSOMPrimitiveValue::create(CSS::Keyword { keyword.value }, owner);
        },
        [&](const CSS::Length<>& length) {
            return CSS::makeDeprecatedCSSOMPrimitiveValueForNumeric(length, owner);
        }
    );
}

Ref<DeprecatedCSSOMRect> DeprecatedCSSOMRect::create(const CSS::ClipRect& rect, CSSStyleDeclaration& owner)
{
    return adoptRef(*new DeprecatedCSSOMRect(rect, owner));
}

DeprecatedCSSOMRect::DeprecatedCSSOMRect(const CSS::ClipRect& rect, CSSStyleDeclaration& owner)
    : m_top(makeDeprecatedCSSOMPrimitiveValueForClipEdge(rect.value->top(), owner))
    , m_right(makeDeprecatedCSSOMPrimitiveValueForClipEdge(rect.value->right(), owner))
    , m_bottom(makeDeprecatedCSSOMPrimitiveValueForClipEdge(rect.value->bottom(), owner))
    , m_left(makeDeprecatedCSSOMPrimitiveValueForClipEdge(rect.value->left(), owner))
{
}

} // namespace WebCore
