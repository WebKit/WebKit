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
#include "Language.h"
#include "RenderIterator.h"
#include "RenderText.h"
#include "SVGDescElement.h"
#include "SVGTitleElement.h"
#include "SVGUseElement.h"
#include "XLinkNames.h"

namespace WebCore {

AccessibilitySVGElement::AccessibilitySVGElement(RenderObject* renderer)
    : AccessibilityRenderObject(renderer)
{
}

AccessibilitySVGElement::~AccessibilitySVGElement()
{
}

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

    Element* target = SVGURIReference::targetElementFromIRIString(href, use.document());
    if (target)
        return axObjectCache()->getOrCreate(target);

    return nullptr;
}

void AccessibilitySVGElement::accessibilityText(Vector<AccessibilityText>& textOrder)
{
    String description = accessibilityDescription();
    if (!description.isEmpty())
        textOrder.append(AccessibilityText(description, AlternativeText));

    String helptext = helpText();
    if (!helptext.isEmpty())
        textOrder.append(AccessibilityText(helptext, HelpText));
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

    String lang = language();
    if (lang.isEmpty())
        lang = defaultLanguage().substring(0, 2);

    String childLang;
    for (const auto& child : childrenOfType<SVGTitleElement>(*element())) {
        childLang = child.getAttribute(SVGNames::langAttr);
        if (childLang == lang || childLang.isEmpty())
            return child.textContent();
    }

    if (is<SVGAElement>(element())) {
        String xlinkTitle = element()->fastGetAttribute(XLinkNames::titleAttr);
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
        const AtomicString& alt = getAttribute(HTMLNames::altAttr);
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

    String lang = language();
    if (lang.isEmpty())
        lang = defaultLanguage().substring(0, 2);

    String childLang;
    for (const auto& child : childrenOfType<SVGDescElement>(*element())) {
        childLang = child.getAttribute(SVGNames::langAttr);
        if (childLang == lang || childLang.isEmpty())
            return child.textContent();
    }

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

    for (const auto& child : childrenOfType<SVGTitleElement>(*element())) {
        childLang = child.getAttribute(SVGNames::langAttr);
        if ((childLang == lang || childLang.isEmpty()) && child.textContent() != description)
            return child.textContent();
    }

    return String();
}

bool AccessibilitySVGElement::computeAccessibilityIsIgnored() const
{
    // According to the SVG Accessibility API Mappings spec, items should be excluded if:
    // * They would be excluded according to the Core Accessibility API Mappings.
    // * They are neither perceivable nor interactive.
    // * Their first mappable role is presentational, regardless of other ARIA attributes.
    // * They have an ancestor with Children Presentational: True (covered by Core AAM)

    AccessibilityObjectInclusion decision = defaultObjectInclusion();
    if (decision == IgnoreObject)
        return true;

    if (m_renderer->isSVGHiddenContainer())
        return true;

    if (roleValue() == PresentationalRole || inheritsPresentationalRole())
        return true;

    if (ariaRoleAttribute() != UnknownRole)
        return false;

    // The SVG AAM states objects with at least one 'title' or 'desc' element MUST be included.
    for (const auto& child : childrenOfType<SVGElement>(*element())) {
        if ((is<SVGTitleElement>(child) || is<SVGDescElement>(child)))
            return false;
    }

    // The SVG AAM states text elements should also be included, if they have content.
    if (m_renderer->isSVGText() || m_renderer->isSVGTextPath()) {
        for (auto& child : childrenOfType<RenderText>(downcast<RenderElement>(*m_renderer))) {
            if (!child.isAllCollapsibleWhitespace())
                return false;
        }
    }

    // SVG shapes should not be included unless there's a concrete reason for inclusion.
    // https://rawgit.com/w3c/aria/master/svg-aam/svg-aam.html#exclude_elements
    if (m_renderer->isSVGShape())
        return !(hasAttributesRequiredForInclusion() || canSetFocusAttribute() || element()->hasEventListeners());

    return AccessibilityRenderObject::computeAccessibilityIsIgnored();
}

bool AccessibilitySVGElement::inheritsPresentationalRole() const
{
    if (canSetFocusAttribute())
        return false;

    AccessibilityRole role = roleValue();
    if (role != SVGTextPathRole && role != SVGTSpanRole)
        return false;

    for (AccessibilityObject* parent = parentObject(); parent; parent = parent->parentObject()) {
        if (is<AccessibilityRenderObject>(*parent) && parent->element()->hasTagName(SVGNames::textTag))
            return parent->roleValue() == PresentationalRole;
    }

    return false;
}

AccessibilityRole AccessibilitySVGElement::determineAriaRoleAttribute() const
{
    // Presentational roles are normally invalidated by the presence of ARIA attributes
    // if the element is focusable. As a result, an UnknownRole might be an invalidated
    // PresentationalRole. We need to check because in SVG the PresentationalRole is
    // expected to trump ARIA attributes.
    // See https://github.com/w3c/aria/issues/136#issuecomment-170557956.

    AccessibilityRole role = AccessibilityRenderObject::determineAriaRoleAttribute();
    if (role != UnknownRole || canSetFocusAttribute())
        return role;

    const AtomicString& ariaRole = getAttribute(HTMLNames::roleAttr);
    if (ariaRole.isNull() || ariaRole.isEmpty())
        return UnknownRole;

    return ariaRoleToWebCoreRole(ariaRole);
}

AccessibilityRole AccessibilitySVGElement::determineAccessibilityRole()
{
    if ((m_ariaRole = determineAriaRoleAttribute()) != UnknownRole)
        return m_ariaRole;

    Element* svgElement = element();

    if (m_renderer->isSVGShape() || m_renderer->isSVGPath() || m_renderer->isSVGImage() || is<SVGUseElement>(svgElement))
        return ImageRole;
    if (m_renderer->isSVGForeignObject() || is<SVGGElement>(svgElement))
        return GroupRole;
    if (m_renderer->isSVGText())
        return SVGTextRole;
    if (m_renderer->isSVGTextPath())
        return SVGTextPathRole;
    if (m_renderer->isSVGTSpan())
        return SVGTSpanRole;
    if (is<SVGAElement>(svgElement))
        return WebCoreLinkRole;

    return AccessibilityRenderObject::determineAccessibilityRole();
}

} // namespace WebCore
