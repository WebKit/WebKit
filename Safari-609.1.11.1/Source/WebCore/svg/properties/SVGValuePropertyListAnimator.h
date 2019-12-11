/*
 * Copyright (C) 2018-2019 Apple Inc.  All rights reserved.
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

#include "SVGPropertyAnimator.h"

namespace WebCore {

template<typename ListType, typename AnimationFunction>
class SVGValuePropertyListAnimator : public SVGPropertyAnimator<AnimationFunction> {
    using Base = SVGPropertyAnimator<AnimationFunction>;
    using Base::Base;
    using Base::applyAnimatedStylePropertyChange;
    using Base::m_function;

public:
    template<typename... Arguments>
    SVGValuePropertyListAnimator(const QualifiedName& attributeName, Ref<SVGProperty>&& property, Arguments&&... arguments)
        : Base(attributeName, std::forward<Arguments>(arguments)...)
        , m_list(static_reference_cast<ListType>(WTFMove(property)))
    {
    }

    void animate(SVGElement* targetElement, float progress, unsigned repeatCount) override
    {
        m_function.animate(targetElement, progress, repeatCount, m_list);
    }

    void apply(SVGElement* targetElement) override
    {
        applyAnimatedStylePropertyChange(targetElement, m_list->valueAsString());
    }

protected:
    using Base::computeCSSPropertyValue;
    using Base::m_attributeName;

    RefPtr<ListType> m_list;
};

}
