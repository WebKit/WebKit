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

#include "DocumentRuleSets.h"
#include "ElementRuleCollector.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "NodeRenderStyle.h"
#include "RenderStyle.h"
#include "SVGElement.h"
#include "StyledElement.h"
#include "VisitedLinkState.h"
#include "WebVTTElement.h"
#include "XMLNames.h"

namespace WebCore {
namespace Style {

static const unsigned cStyleSearchThreshold = 10;
static const unsigned cStyleSearchLevelThreshold = 10;

struct SharingResolver::Context {
    const StyledElement& element;
    bool elementAffectedByClassRules;
    EInsideLink elementLinkState;
};

SharingResolver::SharingResolver(const Document& document, const DocumentRuleSets& ruleSets, const SelectorFilter& selectorFilter)
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
    // FIXME: This line is surprisingly hot, we may wish to inline hasDirectionAuto into StyleResolver.
    return is<HTMLElement>(element) && downcast<HTMLElement>(element).hasDirectionAuto();
}

const Element* SharingResolver::resolve(const Element& searchElement) const
{
    if (!is<StyledElement>(searchElement))
        return nullptr;
    auto& element = downcast<StyledElement>(searchElement);
    if (!element.parentElement())
        return nullptr;
    auto& parentElement = *element.parentElement();
    if (parentElement.shadowRoot())
        return nullptr;
    if (!parentElement.renderStyle())
        return nullptr;
    // If the element has inline style it is probably unique.
    if (element.inlineStyle())
        return nullptr;
    if (element.isSVGElement() && downcast<SVGElement>(element).animatedSMILStyleProperties())
        return nullptr;
    // Ids stop style sharing if they show up in the stylesheets.
    if (element.hasID() && m_ruleSets.features().idsInRules.contains(element.idForStyleResolution().impl()))
        return nullptr;
    if (parentElementPreventsSharing(parentElement))
        return nullptr;
    if (&element == m_document.cssTarget())
        return nullptr;
    if (elementHasDirectionAuto(element))
        return nullptr;

    Context context {
        element,
        element.hasClass() && classNamesAffectedByRules(element.classNames()),
        m_document.visitedLinkState().determineLinkState(element)
    };

    // Check previous siblings and their cousins.
    unsigned count = 0;
    unsigned visitedNodeCount = 0;
    StyledElement* shareElement = nullptr;
    Node* cousinList = element.previousSibling();
    while (cousinList) {
        shareElement = findSibling(context, cousinList, count);
        if (shareElement)
            break;
        cousinList = locateCousinList(cousinList->parentElement(), visitedNodeCount);
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

    return shareElement;
}

StyledElement* SharingResolver::findSibling(const Context& context, Node* node, unsigned& count) const
{
    for (; node; node = node->previousSibling()) {
        if (!is<StyledElement>(*node))
            continue;
        if (canShareStyleWithElement(context, downcast<StyledElement>(*node)))
            break;
        if (count++ == cStyleSearchThreshold)
            return nullptr;
    }
    return downcast<StyledElement>(node);
}

Node* SharingResolver::locateCousinList(Element* parent, unsigned& visitedNodeCount) const
{
    if (visitedNodeCount >= cStyleSearchThreshold * cStyleSearchLevelThreshold)
        return nullptr;
    if (!is<StyledElement>(parent))
        return nullptr;
    auto& styledParent = downcast<StyledElement>(*parent);
    if (styledParent.inlineStyle())
        return nullptr;
    if (is<SVGElement>(styledParent) && downcast<SVGElement>(styledParent).animatedSMILStyleProperties())
        return nullptr;
    if (styledParent.hasID() && m_ruleSets.features().idsInRules.contains(styledParent.idForStyleResolution().impl()))
        return nullptr;

    RenderStyle* parentStyle = styledParent.renderStyle();
    unsigned subcount = 0;
    Node* thisCousin = &styledParent;
    Node* currentNode = styledParent.previousSibling();

    // Reserve the tries for this level. This effectively makes sure that the algorithm
    // will never go deeper than cStyleSearchLevelThreshold levels into recursion.
    visitedNodeCount += cStyleSearchThreshold;
    while (thisCousin) {
        for (; currentNode; currentNode = currentNode->previousSibling()) {
            if (++subcount > cStyleSearchThreshold)
                return nullptr;
            if (!is<Element>(*currentNode))
                continue;
            auto& currentElement = downcast<Element>(*currentNode);
            if (currentElement.renderStyle() != parentStyle)
                continue;
            if (!currentElement.lastChild())
                continue;
            if (!parentElementPreventsSharing(currentElement)) {
                // Adjust for unused reserved tries.
                visitedNodeCount -= cStyleSearchThreshold - subcount;
                return currentNode->lastChild();
            }
        }
        currentNode = locateCousinList(thisCousin->parentElement(), visitedNodeCount);
        thisCousin = currentNode;
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
    if (thisInputElement.shouldAppearIndeterminate() != otherInputElement.shouldAppearIndeterminate())
        return false;
    if (thisInputElement.isRequired() != otherInputElement.isRequired())
        return false;

    if (formElement.isDisabledFormControl() != element.isDisabledFormControl())
        return false;

    if (formElement.isDefaultButtonForForm() != element.isDefaultButtonForForm())
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
    auto* style = candidateElement.renderStyle();
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
    if (candidateElement.shadowPseudoId() != element.shadowPseudoId())
        return false;
    if (&candidateElement == m_document.cssTarget())
        return false;
    if (!sharingCandidateHasIdenticalStyleAffectingAttributes(context, candidateElement))
        return false;
    if (const_cast<StyledElement&>(candidateElement).additionalPresentationAttributeStyle() != const_cast<StyledElement&>(element).additionalPresentationAttributeStyle())
        return false;
    if (candidateElement.affectsNextSiblingElementStyle() || candidateElement.styleIsAffectedByPreviousSibling())
        return false;

    if (candidateElement.hasID() && m_ruleSets.features().idsInRules.contains(candidateElement.idForStyleResolution().impl()))
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
        if (candidateElement.fastGetAttribute(HTMLNames::readonlyAttr) != element.fastGetAttribute(HTMLNames::readonlyAttr))
            return false;
        if (candidateElement.isSVGElement()) {
            if (candidateElement.getAttribute(HTMLNames::typeAttr) != element.getAttribute(HTMLNames::typeAttr))
                return false;
        } else {
            if (candidateElement.fastGetAttribute(HTMLNames::typeAttr) != element.fastGetAttribute(HTMLNames::typeAttr))
                return false;
        }
    }

    if (candidateElement.matchesValidPseudoClass() != element.matchesValidPseudoClass())
        return false;

    if (element.matchesInvalidPseudoClass() != element.matchesValidPseudoClass())
        return false;

#if ENABLE(VIDEO_TRACK)
    // Deny sharing styles between WebVTT and non-WebVTT nodes.
    if (is<WebVTTElement>(element))
        return false;
#endif

#if ENABLE(FULLSCREEN_API)
    if (&element == m_document.webkitCurrentFullScreenElement() || &element == m_document.webkitCurrentFullScreenElement())
        return false;
#endif
    return true;
}

bool SharingResolver::styleSharingCandidateMatchesRuleSet(const StyledElement& element, const RuleSet* ruleSet) const
{
    if (!ruleSet)
        return false;

    ElementRuleCollector collector(const_cast<StyledElement&>(element), nullptr, m_ruleSets, &m_selectorFilter);
    return collector.hasAnyMatchingRules(ruleSet);
}

bool SharingResolver::sharingCandidateHasIdenticalStyleAffectingAttributes(const Context& context, const StyledElement& sharingCandidate) const
{
    auto& element = context.element;
    if (element.elementData() == sharingCandidate.elementData())
        return true;
    if (element.fastGetAttribute(XMLNames::langAttr) != sharingCandidate.fastGetAttribute(XMLNames::langAttr))
        return false;
    if (element.fastGetAttribute(HTMLNames::langAttr) != sharingCandidate.fastGetAttribute(HTMLNames::langAttr))
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

    if (const_cast<StyledElement&>(element).presentationAttributeStyle() != const_cast<StyledElement&>(sharingCandidate).presentationAttributeStyle())
        return false;

    if (element.hasTagName(HTMLNames::progressTag)) {
        if (element.shouldAppearIndeterminate() != sharingCandidate.shouldAppearIndeterminate())
            return false;
    }

    return true;
}

bool SharingResolver::classNamesAffectedByRules(const SpaceSplitString& classNames) const
{
    for (unsigned i = 0; i < classNames.size(); ++i) {
        if (m_ruleSets.features().classesInRules.contains(classNames[i].impl()))
            return true;
    }
    return false;
}


}
}
