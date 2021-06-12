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
#include "StyleSharingResolver.h"

#include "ElementRuleCollector.h"
#include "FullscreenManager.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "NodeRenderStyle.h"
#include "RenderStyle.h"
#include "SVGElement.h"
#include "ShadowRoot.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "StyleScopeRuleSets.h"
#include "StyleUpdate.h"
#include "StyledElement.h"
#include "VisitedLinkState.h"
#include "WebVTTElement.h"
#include "XMLNames.h"

namespace WebCore {
namespace Style {

static const unsigned cStyleSearchThreshold = 10;

struct SharingResolver::Context {
    const Update& update;
    const StyledElement& element;
    bool elementAffectedByClassRules;
    InsideLink elementLinkState;
};

SharingResolver::SharingResolver(const Document& document, const ScopeRuleSets& ruleSets, const SelectorFilter& selectorFilter)
    : m_document(document)
    , m_ruleSets(ruleSets)
    , m_selectorFilter(selectorFilter)
{
}

static inline bool parentElementPreventsSharing(const Element& parentElement)
{
    return parentElement.hasFlagsSetDuringStylingOfChildren();
}

static inline bool elementHasDirectionAuto(const Element& element)
{
    return is<HTMLElement>(element) && downcast<HTMLElement>(element).hasDirectionAuto();
}

std::unique_ptr<RenderStyle> SharingResolver::resolve(const Styleable& searchStyleable, const Update& update)
{
    if (!is<StyledElement>(searchStyleable.element))
        return nullptr;
    auto& element = downcast<StyledElement>(searchStyleable.element);
    if (!element.parentElement())
        return nullptr;
    auto& parentElement = *element.parentElement();
    if (parentElement.shadowRoot())
        return nullptr;
    if (!update.elementStyle(parentElement))
        return nullptr;
    // If the element has inline style it is probably unique.
    if (element.inlineStyle())
        return nullptr;
    if (element.isSVGElement() && downcast<SVGElement>(element).animatedSMILStyleProperties())
        return nullptr;
    // Ids stop style sharing if they show up in the stylesheets.
    auto& id = element.idForStyleResolution();
    if (!id.isNull() && m_ruleSets.features().idsInRules.contains(id))
        return nullptr;
    if (parentElementPreventsSharing(parentElement))
        return nullptr;
    if (&element == m_document.cssTarget())
        return nullptr;
    if (elementHasDirectionAuto(element))
        return nullptr;
    if (element.shadowRoot() && !element.shadowRoot()->styleScope().resolver().ruleSets().authorStyle().hostPseudoClassRules().isEmpty())
        return nullptr;
    if (auto* keyframeEffectStack = searchStyleable.keyframeEffectStack()) {
        if (keyframeEffectStack->hasEffectWithImplicitKeyframes())
            return nullptr;
    }

    Context context {
        update,
        element,
        element.hasClass() && classNamesAffectedByRules(element.classNames()),
        m_document.visitedLinkState().determineLinkState(element)
    };

    // Check previous siblings and their cousins.
    unsigned count = 0;
    StyledElement* shareElement = nullptr;
    Node* cousinList = element.previousSibling();
    while (cousinList) {
        shareElement = findSibling(context, cousinList, count);
        if (shareElement)
            break;
        if (count >= cStyleSearchThreshold)
            break;
        cousinList = locateCousinList(cousinList->parentElement());
    }

    // If we have exhausted all our budget or our cousins.
    if (!shareElement)
        return nullptr;

    // Can't share if sibling rules apply. This is checked at the end as it should rarely fail.
    if (styleSharingCandidateMatchesRuleSet(element, m_ruleSets.sibling()))
        return nullptr;
    // Can't share if attribute rules apply.
    if (styleSharingCandidateMatchesRuleSet(element, m_ruleSets.uncommonAttribute()))
        return nullptr;
    // Tracking child index requires unique style for each node. This may get set by the sibling rule match above.
    if (parentElementPreventsSharing(parentElement))
        return nullptr;

    m_elementsSharingStyle.add(&element, shareElement);

    return RenderStyle::clonePtr(*update.elementStyle(*shareElement));
}

StyledElement* SharingResolver::findSibling(const Context& context, Node* node, unsigned& count) const
{
    for (; node; node = node->previousSibling()) {
        if (!is<StyledElement>(*node))
            continue;
        if (canShareStyleWithElement(context, downcast<StyledElement>(*node)))
            break;
        if (count++ >= cStyleSearchThreshold)
            return nullptr;
    }
    return downcast<StyledElement>(node);
}

Node* SharingResolver::locateCousinList(const Element* parent) const
{
    for (unsigned count = 0; count < cStyleSearchThreshold; ++count) {
        auto* elementSharingParentStyle = m_elementsSharingStyle.get(parent);
        if (!elementSharingParentStyle)
            return nullptr;
        if (!parentElementPreventsSharing(*elementSharingParentStyle)) {
            if (auto* cousin = elementSharingParentStyle->lastChild())
                return cousin;
        }
        parent = elementSharingParentStyle;
    }

    return nullptr;
}

static bool canShareStyleWithControl(const HTMLFormControlElement& element, const HTMLFormControlElement& formElement)
{
    if (!is<HTMLInputElement>(formElement) || !is<HTMLInputElement>(element))
        return false;

    auto& thisInputElement = downcast<HTMLInputElement>(formElement);
    auto& otherInputElement = downcast<HTMLInputElement>(element);

    if (thisInputElement.isAutoFilled() != otherInputElement.isAutoFilled())
        return false;
    if (thisInputElement.shouldAppearChecked() != otherInputElement.shouldAppearChecked())
        return false;
    if (thisInputElement.isRequired() != otherInputElement.isRequired())
        return false;

    if (formElement.isDisabledFormControl() != element.isDisabledFormControl())
        return false;

    if (formElement.isInRange() != element.isInRange())
        return false;

    if (formElement.isOutOfRange() != element.isOutOfRange())
        return false;

    return true;
}

bool SharingResolver::canShareStyleWithElement(const Context& context, const StyledElement& candidateElement) const
{
    auto& element = context.element;
    auto* style = context.update.elementStyle(candidateElement);
    if (!style)
        return false;
    if (style->unique())
        return false;
    if (style->hasUniquePseudoStyle())
        return false;
    if (candidateElement.tagQName() != element.tagQName())
        return false;
    if (candidateElement.inlineStyle())
        return false;
    if (candidateElement.needsStyleRecalc())
        return false;
    if (candidateElement.isSVGElement() && downcast<SVGElement>(candidateElement).animatedSMILStyleProperties())
        return false;
    if (candidateElement.isLink() != element.isLink())
        return false;
    if (candidateElement.hovered() != element.hovered())
        return false;
    if (candidateElement.active() != element.active())
        return false;
    if (candidateElement.focused() != element.focused())
        return false;
    if (candidateElement.hasFocusVisible() != element.hasFocusVisible())
        return false;
    if (candidateElement.hasFocusWithin() != element.hasFocusWithin())
        return false;
    if (candidateElement.isBeingDragged() != element.isBeingDragged())
        return false;
    if (candidateElement.shadowPseudoId() != element.shadowPseudoId())
        return false;
    if (element.isInShadowTree() && candidateElement.partNames() != element.partNames())
        return false;
    if (&candidateElement == m_document.cssTarget())
        return false;
    if (!sharingCandidateHasIdenticalStyleAffectingAttributes(context, candidateElement))
        return false;
    if (const_cast<StyledElement&>(candidateElement).additionalPresentationalHintStyle() != const_cast<StyledElement&>(element).additionalPresentationalHintStyle())
        return false;
    if (candidateElement.affectsNextSiblingElementStyle() || candidateElement.styleIsAffectedByPreviousSibling())
        return false;

    auto& candidateElementId = candidateElement.idForStyleResolution();
    if (!candidateElementId.isNull() && m_ruleSets.features().idsInRules.contains(candidateElementId))
        return false;

    bool isControl = is<HTMLFormControlElement>(candidateElement);

    if (isControl != is<HTMLFormControlElement>(element))
        return false;

    if (isControl && !canShareStyleWithControl(downcast<HTMLFormControlElement>(element), downcast<HTMLFormControlElement>(candidateElement)))
        return false;

    if (style->transitions() || style->animations())
        return false;

    // Turn off style sharing for elements that can gain layers for reasons outside of the style system.
    // See comments in RenderObject::setStyle().
    if (candidateElement.hasTagName(HTMLNames::iframeTag) || candidateElement.hasTagName(HTMLNames::frameTag))
        return false;

    if (candidateElement.hasTagName(HTMLNames::embedTag) || candidateElement.hasTagName(HTMLNames::objectTag) || candidateElement.hasTagName(HTMLNames::appletTag) || candidateElement.hasTagName(HTMLNames::canvasTag))
        return false;

    if (elementHasDirectionAuto(candidateElement))
        return false;

    if (candidateElement.isLink() && context.elementLinkState != style->insideLink())
        return false;

    if (candidateElement.elementData() != element.elementData()) {
        if (candidateElement.attributeWithoutSynchronization(HTMLNames::readonlyAttr) != element.attributeWithoutSynchronization(HTMLNames::readonlyAttr))
            return false;
        if (candidateElement.isSVGElement()) {
            if (candidateElement.getAttribute(HTMLNames::typeAttr) != element.getAttribute(HTMLNames::typeAttr))
                return false;
        } else {
            if (candidateElement.attributeWithoutSynchronization(HTMLNames::typeAttr) != element.attributeWithoutSynchronization(HTMLNames::typeAttr))
                return false;
        }
    }

    if (candidateElement.matchesValidPseudoClass() != element.matchesValidPseudoClass())
        return false;

    if (element.matchesInvalidPseudoClass() != element.matchesValidPseudoClass())
        return false;

    if (candidateElement.matchesIndeterminatePseudoClass() != element.matchesIndeterminatePseudoClass())
        return false;

    if (candidateElement.matchesDefaultPseudoClass() != element.matchesDefaultPseudoClass())
        return false;

    if (candidateElement.shadowRoot() && !candidateElement.shadowRoot()->styleScope().resolver().ruleSets().authorStyle().hostPseudoClassRules().isEmpty())
        return false;

#if ENABLE(WHEEL_EVENT_REGIONS)
    if (candidateElement.hasEventListeners() || element.hasEventListeners())
        return false;
#endif

#if ENABLE(FULLSCREEN_API)
    if (&candidateElement == m_document.fullscreenManager().currentFullscreenElement() || &element == m_document.fullscreenManager().currentFullscreenElement())
        return false;
#endif
    return true;
}

bool SharingResolver::styleSharingCandidateMatchesRuleSet(const StyledElement& element, const RuleSet* ruleSet) const
{
    if (!ruleSet)
        return false;

    ElementRuleCollector collector(const_cast<StyledElement&>(element), m_ruleSets, &m_selectorFilter);
    return collector.hasAnyMatchingRules(ruleSet);
}

bool SharingResolver::sharingCandidateHasIdenticalStyleAffectingAttributes(const Context& context, const StyledElement& sharingCandidate) const
{
    auto& element = context.element;
    if (element.elementData() == sharingCandidate.elementData())
        return true;
    if (element.attributeWithoutSynchronization(XMLNames::langAttr) != sharingCandidate.attributeWithoutSynchronization(XMLNames::langAttr))
        return false;
    if (element.attributeWithoutSynchronization(HTMLNames::langAttr) != sharingCandidate.attributeWithoutSynchronization(HTMLNames::langAttr))
        return false;

    if (context.elementAffectedByClassRules) {
        if (!sharingCandidate.hasClass())
            return false;
        // SVG elements require a (slow!) getAttribute comparision because "class" is an animatable attribute for SVG.
        if (element.isSVGElement()) {
            if (element.getAttribute(HTMLNames::classAttr) != sharingCandidate.getAttribute(HTMLNames::classAttr))
                return false;
        } else {
            if (element.classNames() != sharingCandidate.classNames())
                return false;
        }
    } else if (sharingCandidate.hasClass() && classNamesAffectedByRules(sharingCandidate.classNames()))
        return false;

    if (const_cast<StyledElement&>(element).presentationalHintStyle() != const_cast<StyledElement&>(sharingCandidate).presentationalHintStyle())
        return false;

    return true;
}

bool SharingResolver::classNamesAffectedByRules(const SpaceSplitString& classNames) const
{
    for (unsigned i = 0; i < classNames.size(); ++i) {
        if (m_ruleSets.features().classRules.contains(classNames[i]))
            return true;
    }
    return false;
}


}
}
