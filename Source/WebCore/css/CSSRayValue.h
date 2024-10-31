/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSRayFunction.h"
#include "RenderStyleConstants.h"

namespace WebCore {

// Class containing the value of a ray() function, as used in offset-path:
// https://drafts.fxtf.org/motion-1/#funcdef-offset-path-ray.
class CSSRayValue final : public CSSValue {
public:
    static Ref<CSSRayValue> create(CSS::RayFunction ray, CSSBoxType coordinateBox = CSSBoxType::BoxMissing)
    {
        return adoptRef(*new CSSRayValue(WTFMove(ray), coordinateBox));
    }


    const CSS::RayFunction& ray() const { return m_ray; }
    CSSBoxType coordinateBox() const { return m_coordinateBox; }

    String customCSSText() const;
    bool equals(const CSSRayValue&) const;
    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>&) const;

private:
    CSSRayValue(CSS::RayFunction ray, CSSBoxType coordinateBox)
        : CSSValue(ClassType::Ray)
        , m_ray(WTFMove(ray))
        , m_coordinateBox(coordinateBox)
    {
    }

    CSS::RayFunction m_ray;
    CSSBoxType m_coordinateBox;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSRayValue, isRayValue())
