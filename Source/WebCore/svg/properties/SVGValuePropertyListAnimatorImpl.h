/*
 * Copyright (C) 2018-2021 Apple Inc.  All rights reserved.
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

#pragma once

#include "SVGAnimationFunction.h"
#include "SVGValuePropertyListAnimator.h"

namespace WebCore {

class SVGLengthListAnimator final : public SVGValuePropertyListAnimator<SVGLengthList, SVGAnimationLengthListFunction> {
    using Base = SVGValuePropertyListAnimator<SVGLengthList, SVGAnimationLengthListFunction>;
    using Base::Base;
    using Base::m_attributeName;
    using Base::m_list;

public:
    static auto create(const QualifiedName& attributeName, Ref<SVGProperty>&& property, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        return adoptRef(*new SVGLengthListAnimator(attributeName, WTFMove(property), animationMode, calcMode, isAccumulated, isAdditive, SVGLengthMode::Other));
    }

    void start(SVGElement& targetElement) final
    {
        String baseValue = computeCSSPropertyValue(targetElement, cssPropertyID(m_attributeName.localName()));
        if (!m_list->parse(baseValue))
            m_list->clearItems();
    }
};

} // namespace WebCore
