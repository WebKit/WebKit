/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ContainerQuery.h"
#include "GenericMediaQueryEvaluator.h"
#include "SelectorMatchingState.h"
#include "StyleScopeOrdinal.h"
#include <wtf/Ref.h>

namespace WebCore {

class Element;

namespace Style {

class ContainerQueryEvaluator : public MQ::GenericMediaQueryEvaluator<ContainerQueryEvaluator> {
public:
    enum class SelectionMode : bool { Element, PseudoElement };
    ContainerQueryEvaluator(const Element&, SelectionMode, ScopeOrdinal, SelectorMatchingState*);

    bool evaluate(const CQ::ContainerQuery&) const;

    static const Element* selectContainer(OptionSet<CQ::Axis>, const String& name, const Element&, SelectionMode = SelectionMode::Element, ScopeOrdinal = ScopeOrdinal::Element, const CachedQueryContainers* = nullptr);

private:
    std::optional<MQ::FeatureEvaluationContext> featureEvaluationContextForQuery(const CQ::ContainerQuery&) const;

    const Ref<const Element> m_element;
    const SelectionMode m_selectionMode;
    const ScopeOrdinal m_scopeOrdinal;
    SelectorMatchingState* m_selectorMatchingState;
};

}
}
