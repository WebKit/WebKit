/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Element.h"
#include "StyleInvalidator.h"
#include "StyleScope.h"
#include <wtf/HashSet.h>

namespace WebCore {
namespace Style {

class ChildChangeInvalidation {
public:
    ChildChangeInvalidation(ContainerNode&, const ContainerNode::ChildChange&);
    ~ChildChangeInvalidation();

    static void invalidateAfterFinishedParsingChildren(Element&);

private:
    void invalidateForHasBeforeMutation();
    void invalidateForHasAfterMutation();
    void invalidateAfterChange();
    void checkForSiblingStyleChanges();
    using MatchingHasSelectors = HashSet<const CSSSelector*>;
    enum class ChangedElementRelation : uint8_t { SelfOrDescendant, Sibling };
    void invalidateForChangedElement(Element&, MatchingHasSelectors&, ChangedElementRelation);
    void invalidateForChangeOutsideHasScope();

    template<typename Function> void traverseRemovedElements(Function&&);
    template<typename Function> void traverseAddedElements(Function&&);
    template<typename Function> void traverseRemainingExistingSiblings(Function&&);

    Element& parentElement() { return *m_parentElement; }

    Element* m_parentElement { nullptr };
    const ContainerNode::ChildChange& m_childChange;

    const bool m_isEnabled;
    const bool m_needsHasInvalidation;
    const bool m_wasEmpty;
};

inline ChildChangeInvalidation::ChildChangeInvalidation(ContainerNode& container, const ContainerNode::ChildChange& childChange)
    : m_parentElement(dynamicDowncast<Element>(container))
    , m_childChange(childChange)
    , m_isEnabled(m_parentElement && m_parentElement->needsStyleInvalidation())
    , m_needsHasInvalidation(m_isEnabled && Scope::forNode(*m_parentElement).usesHasPseudoClass())
    , m_wasEmpty(!container.firstChild())
{
    if (!m_isEnabled)
        return;

    if (m_needsHasInvalidation)
        invalidateForHasBeforeMutation();
}

inline ChildChangeInvalidation::~ChildChangeInvalidation()
{
    if (!m_isEnabled)
        return;

    if (m_needsHasInvalidation)
        invalidateForHasAfterMutation();

    invalidateAfterChange();
}

}
}
