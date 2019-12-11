/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "config.h"
#include "StyleRelations.h"

#include "Element.h"
#include "NodeRenderStyle.h"
#include "RenderStyle.h"
#include "StyleUpdate.h"

namespace WebCore {
namespace Style {

std::unique_ptr<Relations> commitRelationsToRenderStyle(RenderStyle& style, const Element& element, const Relations& relations)
{
    std::unique_ptr<Relations> remainingRelations;

    auto appendStyleRelation = [&remainingRelations] (const Relation& relation) {
        if (!remainingRelations)
            remainingRelations = makeUnique<Relations>();
        remainingRelations->append(relation);
    };

    for (auto& relation : relations) {
        if (relation.element != &element) {
            appendStyleRelation(relation);
            continue;
        }
        switch (relation.type) {
        case Relation::AffectedByActive:
            style.setAffectedByActive();
            appendStyleRelation(relation);
            break;
        case Relation::AffectedByDrag:
            style.setAffectedByDrag();
            break;
        case Relation::AffectedByEmpty:
            style.setEmptyState(relation.value);
            appendStyleRelation(relation);
            break;
        case Relation::AffectedByHover:
            style.setAffectedByHover();
            break;
        case Relation::FirstChild:
            style.setFirstChildState();
            break;
        case Relation::LastChild:
            style.setLastChildState();
            break;
        case Relation::Unique:
            style.setUnique();
            break;
        case Relation::AffectedByFocusWithin:
        case Relation::AffectedByPreviousSibling:
        case Relation::DescendantsAffectedByPreviousSibling:
        case Relation::AffectsNextSibling:
        case Relation::ChildrenAffectedByForwardPositionalRules:
        case Relation::DescendantsAffectedByForwardPositionalRules:
        case Relation::ChildrenAffectedByBackwardPositionalRules:
        case Relation::DescendantsAffectedByBackwardPositionalRules:
        case Relation::ChildrenAffectedByFirstChildRules:
        case Relation::ChildrenAffectedByPropertyBasedBackwardPositionalRules:
        case Relation::ChildrenAffectedByLastChildRules:
        case Relation::NthChildIndex:
            appendStyleRelation(relation);
            break;
        }
    }
    return remainingRelations;
}

void commitRelations(std::unique_ptr<Relations> relations, Update& update)
{
    if (!relations)
        return;
    for (auto& relation : *relations) {
        auto& element = const_cast<Element&>(*relation.element);
        switch (relation.type) {
        case Relation::AffectedByActive:
            element.setStyleAffectedByActive();
            break;
        case Relation::AffectedByDrag:
            element.setChildrenAffectedByDrag();
            break;
        case Relation::AffectedByEmpty:
            element.setStyleAffectedByEmpty();
            break;
        case Relation::AffectedByFocusWithin:
            element.setStyleAffectedByFocusWithin();
            break;
        case Relation::AffectedByHover:
            element.setChildrenAffectedByHover();
            break;
        case Relation::AffectedByPreviousSibling:
            element.setStyleIsAffectedByPreviousSibling();
            break;
        case Relation::DescendantsAffectedByPreviousSibling:
            element.setDescendantsAffectedByPreviousSibling();
            break;
        case Relation::AffectsNextSibling: {
            auto* sibling = &element;
            for (unsigned i = 0; i < relation.value && sibling; ++i, sibling = sibling->nextElementSibling())
                sibling->setAffectsNextSiblingElementStyle();
            break;
        }
        case Relation::ChildrenAffectedByForwardPositionalRules:
            element.setChildrenAffectedByForwardPositionalRules();
            break;
        case Relation::DescendantsAffectedByForwardPositionalRules:
            element.setDescendantsAffectedByForwardPositionalRules();
            break;
        case Relation::ChildrenAffectedByBackwardPositionalRules:
            element.setChildrenAffectedByBackwardPositionalRules();
            break;
        case Relation::DescendantsAffectedByBackwardPositionalRules:
            element.setDescendantsAffectedByBackwardPositionalRules();
            break;
        case Relation::ChildrenAffectedByFirstChildRules:
            element.setChildrenAffectedByFirstChildRules();
            break;
        case Relation::ChildrenAffectedByPropertyBasedBackwardPositionalRules:
            element.setChildrenAffectedByBackwardPositionalRules();
            element.setChildrenAffectedByPropertyBasedBackwardPositionalRules();
            break;
        case Relation::ChildrenAffectedByLastChildRules:
            element.setChildrenAffectedByLastChildRules();
            break;
        case Relation::FirstChild:
            if (auto* style = update.elementStyle(element))
                style->setFirstChildState();
            break;
        case Relation::LastChild:
            if (auto* style = update.elementStyle(element))
                style->setLastChildState();
            break;
        case Relation::NthChildIndex:
            if (auto* style = update.elementStyle(element))
                style->setUnique();
            element.setChildIndex(relation.value);
            break;
        case Relation::Unique:
            if (auto* style = update.elementStyle(element))
                style->setUnique();
            break;
        }
    }
}

}
}
