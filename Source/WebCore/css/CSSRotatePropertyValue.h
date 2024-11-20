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

#pragma once

#include "CSSRotateProperty.h"
#include "CSSValue.h"

namespace WebCore {

class CSSRotatePropertyValue final : public CSSValue {
public:
    static Ref<CSSRotatePropertyValue> create(CSS::RotateProperty rotate)
    {
        return adoptRef(*new CSSRotatePropertyValue(WTFMove(rotate)));
    }

    const CSS::RotateProperty& rotate() const { return m_rotate; }

    String customCSSText() const;
    bool equals(const CSSRotatePropertyValue&) const;

    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>&) const;
    void customCollectComputedStyleDependencies(ComputedStyleDependencies&) const;
    bool addDerivedHash(Hasher&) const;

private:
    CSSRotatePropertyValue(CSS::RotateProperty&& rotate)
        : CSSValue(ClassType::RotateProperty)
        , m_rotate { WTFMove(rotate) }
    {
    }

    CSS::RotateProperty m_rotate;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSRotatePropertyValue, isRotatePropertyValue())
