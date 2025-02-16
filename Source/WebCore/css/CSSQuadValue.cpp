/**
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "CSSQuadValue.h"
#include "CSSValue.h"

namespace WebCore {

CSSQuadValue::CSSQuadValue(Quad quad)
    : CSSValue(ClassType::Quad)
    , m_coalesceIdenticalValues(true)
    , m_quad(WTFMove(quad))
{
}

Ref<CSSQuadValue> CSSQuadValue::create(Quad quad)
{
    return adoptRef(*new CSSQuadValue(WTFMove(quad)));
}

String CSSQuadValue::customCSSText(const CSS::SerializationContext& context) const
{
    return m_quad.cssText(context);
}

bool CSSQuadValue::equals(const CSSQuadValue& other) const
{
    return m_quad.equals(other.m_quad);
}

bool CSSQuadValue::canBeCoalesced() const
{
    Ref top = m_quad.top();
    Ref right = m_quad.right();
    Ref left = m_quad.left();
    Ref bottom = m_quad.bottom();
    return m_coalesceIdenticalValues && top->equals(right) && top->equals(left) && top->equals(bottom);
}

} // namespace WebCore
