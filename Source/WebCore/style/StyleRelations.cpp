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
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle+SettersInlines.h"
#include "StyleUpdate.h"

namespace WebCore::Style {

std::unique_ptr<Relations> commitRelationsToRenderStyle(Style::ComputedStyle& style, const Element& element, const Relations& relations)
{
    std::unique_ptr<Relations> remainingRelations;

    auto appendStyleRelation = [&remainingRelations] (const Relation& relation) {
        if (!remainingRelations)
            remainingRelations = makeUnique<Relations>();
        remainingRelations->append(relation);
    };

    for (auto& relation : relations) {
        if (relation.node != &element) {
            appendStyleRelation(relation);
            continue;
        }
        switch (relation.type) {
        case Relation::AffectedByEmpty:
            appendStyleRelation(relation);
            break;
        case Relation::FirstChild:
            style.setFirstChildState();
            break;
        case Relation::LastChild:
            style.setLastChildState();
            break;
        case Relation::AffectedByPreviousSibling:
        case Relation::DescendantsAffectedByPreviousSibling:
        case Relation::AffectsNextSibling:
        case Relation::ChildrenAffectedByForwardPositionalRules:
        case Relation::DescendantsAffectedByForwardPositionalRules:
        case Relation::ChildrenAffectedByBackwardPositionalRules:
        case Relation::DescendantsAffectedByBackwardPositionalRules:
        case Relation::ChildrenAffectedByFirstChildRules:
        case Relation::ChildrenAffectedByLastChildRules:
        case Relation::NthChildIndex:
        case Relation::AffectedByHasWithBackwardSiblingRelationship:
        case Relation::AffectedByHasWithForwardSiblingRelationship:
        case Relation::AffectedByHasWithAdjacentSiblingRelationship:
            appendStyleRelation(relation);
            break;
        }
    }
    return remainingRelations;
}

void commitRelations(std::unique_ptr<Relations> relations, Update* update)
{
    if (!relations)
        return;
    for (auto& relation : *relations) {
        Ref node = const_cast<ContainerNode&>(*relation.node);
        switch (relation.type) {
        case Relation::AffectedByEmpty:
            downcast<Element>(node).setStyleAffectedByEmpty();
            break;
        case Relation::AffectedByPreviousSibling:
            downcast<Element>(node).setStyleIsAffectedByPreviousSibling();
            break;
        case Relation::DescendantsAffectedByPreviousSibling:
            downcast<Element>(node).setDescendantsAffectedByPreviousSibling();
            break;
        case Relation::AffectsNextSibling: {
            auto* sibling = &downcast<Element>(node);
            for (unsigned i = 0; i < relation.value && sibling; ++i, sibling = sibling->nextElementSibling())
                sibling->setAffectsNextSiblingElementStyle();
            break;
        }
        case Relation::ChildrenAffectedByForwardPositionalRules:
            node->setChildrenAffectedByForwardPositionalRules();
            break;
        case Relation::DescendantsAffectedByForwardPositionalRules:
            node->setDescendantsAffectedByForwardPositionalRules();
            break;
        case Relation::ChildrenAffectedByBackwardPositionalRules:
            node->setChildrenAffectedByBackwardPositionalRules();
            break;
        case Relation::DescendantsAffectedByBackwardPositionalRules:
            node->setDescendantsAffectedByBackwardPositionalRules();
            break;
        case Relation::ChildrenAffectedByFirstChildRules:
            node->setChildrenAffectedByFirstChildRules();
            break;
        case Relation::ChildrenAffectedByLastChildRules:
            node->setChildrenAffectedByLastChildRules();
            break;
        case Relation::AffectedByHasWithBackwardSiblingRelationship:
            node->setAffectedByHasWithBackwardSiblingRelationship();
            break;
        case Relation::AffectedByHasWithForwardSiblingRelationship:
            node->setAffectedByHasWithForwardSiblingRelationship();
            break;
        case Relation::AffectedByHasWithAdjacentSiblingRelationship:
            node->setAffectedByHasWithAdjacentSiblingRelationship();
            break;
        case Relation::FirstChild:
            if (auto* style = update ? update->elementStyle(downcast<Element>(node)) : nullptr)
                style->setFirstChildState();
            break;
        case Relation::LastChild:
            if (auto* style = update ? update->elementStyle(downcast<Element>(node)) : nullptr)
                style->setLastChildState();
            break;
        case Relation::NthChildIndex:
            downcast<Element>(node).setChildIndex(relation.value);
            break;
        }
    }
}

void copyRelations(Style::ComputedStyle& to, const Style::ComputedStyle& from)
{
    if (from.firstChildState())
        to.setFirstChildState();
    if (from.lastChildState())
        to.setLastChildState();
}

}
