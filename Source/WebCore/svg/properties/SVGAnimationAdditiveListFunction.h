/*
 * Copyright (C) 2019 Apple Inc.  All rights reserved.
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

#include "SVGAnimationAdditiveFunction.h"

namespace WebCore {

template<typename ListType>
class SVGAnimationAdditiveListFunction : public SVGAnimationAdditiveFunction {
public:
    template<typename... Arguments>
    SVGAnimationAdditiveListFunction(AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive, Arguments&&... arguments)
        : SVGAnimationAdditiveFunction(animationMode, calcMode, isAccumulated, isAdditive)
        , m_from(ListType::create(std::forward<Arguments>(arguments)...))
        , m_to(ListType::create(std::forward<Arguments>(arguments)...))
        , m_toAtEndOfDuration(ListType::create(std::forward<Arguments>(arguments)...))
    {
    }

protected:
    const Ref<ListType>& toAtEndOfDuration() const { return !m_toAtEndOfDuration->isEmpty() ? m_toAtEndOfDuration : m_to; }

    bool adjustAnimatedList(AnimationMode animationMode, float percentage, RefPtr<ListType>& animated, bool resizeAnimatedIfNeeded = true)
    {
        if (!m_to->numberOfItems())
            return false;

        if (m_from->numberOfItems() && m_from->size() != m_to->size()) {
            if (percentage >= 0.5)
                *animated = m_to;
            else if (animationMode != AnimationMode::To)
                *animated = m_from;
            return false;
        }

        if (resizeAnimatedIfNeeded && animated->size() < m_to->size())
            animated->resize(m_to->size());
        return true;
    }

    Ref<ListType> m_from;
    Ref<ListType> m_to;
    Ref<ListType> m_toAtEndOfDuration;
};

}
