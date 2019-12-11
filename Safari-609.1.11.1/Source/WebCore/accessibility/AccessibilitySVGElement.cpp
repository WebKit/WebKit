/*
 * Copyright (C) 2016 Igalia, S.L.
 * All rights reserved.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AccessibilitySVGElement.h"

#include "AXObjectCache.h"
#include "ElementIterator.h"
#include "HTMLNames.h"
#include "RenderIterator.h"
#include "RenderText.h"
#include "SVGAElement.h"
#include "SVGDescElement.h"
#include "SVGGElement.h"
#include "SVGTitleElement.h"
#include "SVGUseElement.h"
#include "XLinkNames.h"
#include <wtf/Language.h>

namespace WebCore {

AccessibilitySVGElement::AccessibilitySVGElement(RenderObject* renderer)
    : AccessibilityRenderObject(renderer)
{
}

AccessibilitySVGElement::~AccessibilitySVGElement() = default;

Ref<AccessibilitySVGElement> AccessibilitySVGElement::create(RenderObject* renderer)
{
    return adoptRef(*new AccessibilitySVGElement(renderer));
}

AccessibilityObject* AccessibilitySVGElement::targetForUseElement() const
{
    if (!is<SVGUseElement>(element()))
        return nullptr;

    SVGUseElement& use = downcast<SVGUseElement>(*element());
    String href = use.href();
    if (href.isEmpty())
        href = getAttribute(HTMLNames::hrefAttr);

    auto target = SVGURIReference::targetElementFromIRIString(href, use.treeScope());
    if (!target.element)
        return nullptr;
    return axObjectCache()->getOrCreate(target.element.get());
}

template <typename ChildrenType>
Element* AccessibilitySVGElement::childElementWithMatchingLanguage(ChildrenType& children) const
{
    String languageCode = language();
    if (languageCode.isEmpty())
        languageCode = defaultLanguage();

    // The best match for a group of child SVG2 'title' or 'desc' elements may be the one
    // which lacks a 'lang' attribute value. However, indexOfBestMatchingLanguageInList()
    // currently bases its decision on non-empty strings. Furthermore, we cannot count on
    // that child element having a given position. So we'll look for such an element while
    // building the language list and save it as our fallback.

    Element* fallback = nullptr;
    Vector<String> childLanguageCodes;
    Vector<Element*> elements;
    for (auto& child : children) {
        auto& lang = child.attributeWithoutSynchronization(SVGNames::langAttr);
        childLanguageCodes.append(lang);
        elements.append(&child);

        // The current draft of the SVG2 spec states if there are multiple equally-valid
        // matches, the first match should be used.
        if (lang.isEmpty() && !fallback)
            fallback = &child;
    }

    bool exactMatch;
    size_t index = indexOfBestMatchingLanguageInList(languageCode, childLanguageCodes, exactMatch);
    if (index < childLanguageCodes.size())
        return elements[index];

    return fallback;
}

void AccessibilitySVGElement::accessibilityText(Vector<AccessibilityText>& textOrder) const
{
    String description = accessibilityDescription();
    if (!description.isEmpty())
        textOrder.append(AccessibilityText(description, AccessibilityTextSource::Alternative));

    String helptext = helpText();
    if (!helptext.isEmpty())
        textOrder.append(AccessibilityText(helptext, AccessibilityTextSource::Help));
}

String AccessibilitySVGElement::accessibilityDescription() const
{
    // According to the SVG Accessibility API Mappings spec, the order of priority is:
    // 1. aria-labelledby
    // 2. aria-label
    // 3. a direct child title element (selected according to language)
    // 4. xlink:title attribute
    // 5. for a use element, the accessible name calculated for the re-used content
    // 6. for text container elements, the text content

    String ariaDescription = ariaAccessibilityDescription();
    if (!ariaDescription.isEmpty())
        return ariaDescription;

    auto titleElements = childrenOfType<SVGTitleElement>(*element());
    if (auto titleChild = childElementWithMatchingLanguage(titleElements))
        return titleChild->textContent();

    if (is<SVGAElement>(element())) {
        auto& xlinkTitle = element()->attributeWithoutSynchronization(XLinkNames::titleAttr);
        if (!xlinkTitle.isEmpty())
            return xlinkTitle;
    }

    if (m_renderer->isSVGText()) {
        AccessibilityTextUnderElementMode mode;
        String text = textUnderElement(mode);
        if (!text.isEmpty())
            return text;
    }

    if (is<SVGUseElement>(element())) {
        if (AccessibilityObject* target = targetForUseElement())
            return target->accessibilityDescription();
    }

    // FIXME: This is here to not break the svg-image.html test. But 'alt' is not
    // listed as a supported attribute of the 'image' element in the SVG spec:
    // https://www.w3.org/TR/SVG/struct.html#ImageElement
    if (m_renderer->isSVGImage()) {
        const AtomString& alt = getAttribute(HTMLNames::altAttr);
        if (!alt.isNull())
            return alt;
    }

    return String();
}

String AccessibilitySVGElement::helpText() const
{
    // According to the SVG Accessibility API Mappings spec, the order of priority is:
    // 1. aria-describedby
    // 2. a direct child desc element
    // 3. for a use element, the accessible description calculated for the re-used content
    // 4. for text container elements, the text content, if not used for the name
    // 5. a direct child title element that provides a tooltip, if not used for the name

    String describedBy = ariaDescribedByAttribute();
    if (!describedBy.isEmpty())
        return describedBy;

    auto descriptionElements = childrenOfType<SVGDescElement>(*element());
    if (auto descriptionChild = childElementWithMatchingLanguage(descriptionElements))
        return descriptionChild->textContent();

    if (is<SVGUseElement>(element())) {
        AccessibilityObject* target = targetForUseElement();
        if (target)
            return target->helpText();
    }

    String description = accessibilityDescription();

    if (m_renderer->isSVGText()) {
        AccessibilityTextUnderElementMode mode;
        String text = textUnderElement(mode);
        if (!text.isEmpty() && text != description)
            return text;
    }

    auto titleElements = childrenOfType<SVGTitleElement>(*element());
    if (auto titleChild = childElementWithMatchingLanguage(titleElements)) {
        if (titleChild->textContent() != description)
            return titleChild->textContent();
    }

    return String();
}

bool AccessibilitySVGElement::computeAccessibilityIsIgnored() const
{
    // According to the SVG Accessibility API Mappings spec, items should be excluded if:
    // * They would be excluded according to the Core Accessibility API Mappings.
    // * They are neither perceivable nor interactive.
    // * Their first mappable role is presentational, unless they have a global ARIA
    //   attribute (covered by Core AAM) or at least one 'title' or 'desc' child element.
    // * They have an ancestor with Children Presentational: True (covered by Core AAM)

    AccessibilityObjectInclusion decision = defaultObjectInclusion();
    if (decision == AccessibilityObjectInclusion::IgnoreObject)
        return true;

    if (m_renderer->isSVGHiddenContainer())
        return true;

    // The SVG AAM states objects with at least one 'title' or 'desc' element MUST be included.
    // At this time, the presence of a matching 'lang' attribute is not mentioned in the spec.
    for (const auto& child : childrenOfType<SVGElement>(*element())) {
        if ((is<SVGTitleElement>(child) || is<SVGDescElement>(child)))
            return false;
    }

    if (roleValue() == AccessibilityRole::Presentational || inheritsPresentationalRole())
        return true;

    if (ariaRoleAttribute() != AccessibilityRole::Unknown)
        return false;

    // The SVG AAM states text elements should also be included, if they have content.
    if (m_renderer->isSVGText() || m_renderer->isSVGTextPath()) {
        for (auto& child : childrenOfType<RenderText>(downcast<RenderElement>(*m_renderer))) {
            if (!child.isAllCollapsibleWhitespace())
                return false;
        }
    }

    // SVG shapes should not be included unless there's a concrete reason for inclusion.
    // https://rawgit.com/w3c/aria/master/svg-aam/svg-aam.html#exclude_elements
    if (m_renderer->isSVGShape()) {
        if (canSetFocusAttribute() || element()->hasEventListeners())
            return false;
        if (auto* svgParent = Accessibility::findAncestor<AccessibilityObject>(*this, true, [] (const AccessibilityObject& object) {
            return object.hasAttributesRequiredForInclusion() || object.isAccessibilitySVGRoot();
        }))
            return !svgParent->hasAttributesRequiredForInclusion();
        return true;
    }

    return AccessibilityRenderObject::computeAccessibilityIsIgnored();
}

bool AccessibilitySVGElement::inheritsPresentationalRole() const
{
    if (canSetFocusAttribute())
        return false;

    AccessibilityRole role = roleValue();
    if (role != AccessibilityRole::SVGTextPath && role != AccessibilityRole::SVGTSpan)
        return false;

    for (AccessibilityObject* parent = parentObject(); parent; parent = parent->parentObject()) {
        if (is<AccessibilityRenderObject>(*parent) && parent->element()->hasTagName(SVGNames::textTag))
            return parent->roleValue() == AccessibilityRole::Presentational;
    }

    return false;
}

AccessibilityRole AccessibilitySVGElement::determineAriaRoleAttribute() const
{
    AccessibilityRole role = AccessibilityRenderObject::determineAriaRoleAttribute();
    if (role != AccessibilityRole::Presentational)
        return role;

    // The presence of a 'title' or 'desc' child element trumps PresentationalRole.
    // https://lists.w3.org/Archives/Public/public-svg-a11y/2016Apr/0016.html
    // At this time, the presence of a matching 'lang' attribute is not mentioned.
    for (const auto& child : childrenOfType<SVGElement>(*element())) {
        if ((is<SVGTitleElement>(child) || is<SVGDescElement>(child)))
            return AccessibilityRole::Unknown;
    }

    return role;
}

AccessibilityRole AccessibilitySVGElement::determineAccessibilityRole()
{
    if ((m_ariaRole = determineAriaRoleAttribute()) != AccessibilityRole::Unknown)
        return m_ariaRole;

    Element* svgElement = element();

    if (m_renderer->isSVGShape() || m_renderer->isSVGPath() || m_renderer->isSVGImage() || is<SVGUseElement>(svgElement))
        return AccessibilityRole::Image;
    if (m_renderer->isSVGForeignObject() || is<SVGGElement>(svgElement))
        return AccessibilityRole::Group;
    if (m_renderer->isSVGText())
        return AccessibilityRole::SVGText;
    if (m_renderer->isSVGTextPath())
        return AccessibilityRole::SVGTextPath;
    if (m_renderer->isSVGTSpan())
        return AccessibilityRole::SVGTSpan;
    if (is<SVGAElement>(svgElement))
        return AccessibilityRole::WebCoreLink;

    return AccessibilityRenderObject::determineAccessibilityRole();
}

} // namespace WebCore
