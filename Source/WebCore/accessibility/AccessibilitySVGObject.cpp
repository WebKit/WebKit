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
#include "AccessibilitySVGObject.h"

#include "AccessibilityNodeObjectInlines.h"
#include "AXObjectCacheInlines.h"
#include "AXUtilities.h"
#include "AccessibilityObjectInlines.h"
#include "ElementChildIteratorInlines.h"
#include "ElementInlines.h"
#include "EventTargetInlines.h"
#include "HTMLNames.h"
#include "RenderIterator.h"
#include "RenderObject.h"
#include "RenderObjectInlines.h"
#include "RenderText.h"
#include "SVGAElement.h"
#include "SVGDescElement.h"
#include "SVGElementTypeHelpers.h"
#include "SVGGElement.h"
#include "SVGNames.h"
#include "SVGTitleElement.h"
#include "SVGUseElement.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "XLinkNames.h"
#include <wtf/Language.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

AccessibilitySVGObject::AccessibilitySVGObject(AXID axID, RenderObject& renderer, AXObjectCache& cache, bool isSVGRoot)
    : AccessibilityRenderObject(axID, renderer, cache)
{
    m_isSVGRoot = isSVGRoot;
}

AccessibilitySVGObject::AccessibilitySVGObject(AXID axID, Element& element, AXObjectCache& cache, bool isSVGRoot)
    : AccessibilityRenderObject(axID, element, cache)
{
    m_isSVGRoot = isSVGRoot;
}

AccessibilitySVGObject::~AccessibilitySVGObject() = default;

Ref<AccessibilitySVGObject> AccessibilitySVGObject::create(AXID axID, RenderObject& renderer, AXObjectCache& cache, bool isSVGRoot)
{
    return adoptRef(*new AccessibilitySVGObject(axID, renderer, cache, isSVGRoot));
}

Ref<AccessibilitySVGObject> AccessibilitySVGObject::create(AXID axID, Element& element, AXObjectCache& cache, bool isSVGRoot)
{
    return adoptRef(*new AccessibilitySVGObject(axID, element, cache, isSVGRoot));
}

AccessibilityObject* AccessibilitySVGObject::targetForUseElement() const
{
    RefPtr use = dynamicDowncast<SVGUseElement>(element());
    if (!use)
        return nullptr;

    auto href = use->href();
    if (href.isEmpty())
        href = getAttribute(HTMLNames::hrefAttr);

    auto target = SVGURIReference::targetElementFromIRIString(href, Ref { use->treeScopeForSVGReferences() });
    CheckedPtr cache = axObjectCache();
    return cache ? cache->getOrCreate(target.element.get()) : nullptr;
}

void AccessibilitySVGObject::accessibilityText(Vector<AccessibilityText>& textOrder) const
{
    // Compute both description and help text together to avoid redundant language
    // matching, which showed up on samples taken on Speedometer 3.1.
    auto [titleChild, descChild] = matchingTitleAndDescChildren();

    String descriptionText = descriptionFromTitleChild(titleChild.get());
    if (!descriptionText.isEmpty())
        textOrder.append(AccessibilityText(descriptionText, AccessibilityTextSource::Alternative));

    String helptext = helpTextFromChildren(titleChild.get(), descChild.get(), descriptionText);
    if (!helptext.isEmpty())
        textOrder.append(AccessibilityText(WTF::move(helptext), AccessibilityTextSource::Help));
}

auto AccessibilitySVGObject::matchingTitleAndDescChildren() const -> MatchingLanguageChildren
{
    RefPtr element = this->element();
    if (!element)
        return { };

    // Iterate over SVG children once, bucketing <title> and <desc> elements,
    // then do language matching with a single languageIncludingAncestors() call.
    // This is an optimization based on a profile taken on Speedometer.
    Vector<String> titleLangs;
    Vector<Element*> titleElements;
    RefPtr<Element> titleFallback;
    bool hasTitleWithLang = false;
    Vector<String> descLangs;
    Vector<Element*> descElements;
    RefPtr<Element> descFallback;
    bool hasDescWithLang = false;

    for (Ref child : childrenOfType<SVGElement>(*element)) {
        const auto& lang = child->attributeWithoutSynchronization(SVGNames::langAttr);
        if (is<SVGTitleElement>(child.get())) {
            titleLangs.append(lang);
            titleElements.append(const_cast<Element*>(static_cast<const Element*>(child.ptr())));
            if (lang.isEmpty()) {
                if (!titleFallback)
                    titleFallback = child.ptr();
            } else
                hasTitleWithLang = true;
        } else if (is<SVGDescElement>(child.get())) {
            descLangs.append(lang);
            descElements.append(const_cast<Element*>(static_cast<const Element*>(child.ptr())));
            if (lang.isEmpty()) {
                if (!descFallback)
                    descFallback = child.ptr();
            } else
                hasDescWithLang = true;
        }
    }

    // If no child has a lang attribute, language matching can't improve on the
    // fallback. Skip the expensive languageIncludingAncestors() and NSLocale calls.
    if (!hasTitleWithLang && !hasDescWithLang)
        return { WTF::move(titleFallback), WTF::move(descFallback) };

    String languageCode = languageIncludingAncestors();
    if (languageCode.isEmpty())
        languageCode = defaultLanguage();

    auto matchInList = [&](Vector<String>& langs, Vector<Element*>& elements, RefPtr<Element>& fallback) -> RefPtr<Element> {
        bool exactMatch;
        size_t index = indexOfBestMatchingLanguageInList(languageCode, langs, exactMatch);
        if (index < langs.size())
            return elements[index];
        return fallback;
    };

    RefPtr titleChild = hasTitleWithLang ? matchInList(titleLangs, titleElements, titleFallback) : WTF::move(titleFallback);
    RefPtr descChild = hasDescWithLang ? matchInList(descLangs, descElements, descFallback) : WTF::move(descFallback);
    return { WTF::move(titleChild), WTF::move(descChild) };
}

String AccessibilitySVGObject::descriptionFromTitleChild(Element* titleChild) const
{
    // Priority per SVG AAM: aria-labelledby/label, title child, xlink:title, use target, alt.
    String result = ariaAccessibilityDescription();
    if (!result.isEmpty())
        return result;

    if (titleChild) {
        result = titleChild->textContent();
        if (!result.isEmpty())
            return result;
    }

    RefPtr element = this->element();
    if (is<SVGAElement>(element.get())) {
        const auto& xlinkTitle = element->attributeWithoutSynchronization(XLinkNames::titleAttr);
        if (!xlinkTitle.isEmpty())
            return xlinkTitle;
    }

    if (RefPtr target = targetForUseElement()) {
        // Avoid infinite recursion from circular <use> references by tracking ones we're currently resolving.
        static NeverDestroyed<HashSet<Element*>> elementsResolvingDescription;
        if (elementsResolvingDescription->add(element.get()).isNewEntry) {
            auto result = target->description();
            elementsResolvingDescription->remove(element.get());
            return result;
        }
    }

    if (m_renderer && m_renderer->isRenderOrLegacyRenderSVGImage()) {
        const auto& alt = getAttribute(HTMLNames::altAttr);
        if (!alt.isNull())
            return alt;
    }

    return { };
}

String AccessibilitySVGObject::helpTextFromChildren(Element* titleChild, Element* descChild, const String& descriptionText) const
{
    // Priority per SVG AAM: aria-describedby, desc child, use target, title child (if != description).
    RefPtr element = this->element();
    if (!element)
        return { };

    String result = ariaDescribedByAttribute();
    if (!result.isEmpty())
        return result;

    if (descChild) {
        result = descChild->textContent();
        if (!result.isEmpty())
            return result;
    }

    if (RefPtr target = targetForUseElement()) {
        // Avoid infinite recursion from circular <use> references by tracking ones we're currently resolving.
        static NeverDestroyed<HashSet<Element*>> elementsResolvingHelpText;
        if (elementsResolvingHelpText->add(element.get()).isNewEntry) {
            auto result = target->helpText();
            elementsResolvingHelpText->remove(element.get());
            return result;
        }
    }

    if (titleChild) {
        auto titleText = titleChild->textContent();
        if (titleText != descriptionText)
            return titleText;
    }

    return { };
}

String AccessibilitySVGObject::description() const
{
    auto [titleChild, descChild] = matchingTitleAndDescChildren();
    return descriptionFromTitleChild(titleChild.get());
}

String AccessibilitySVGObject::helpText() const
{
    auto [titleChild, descChild] = matchingTitleAndDescChildren();
    String descriptionText = descriptionFromTitleChild(titleChild.get());
    return helpTextFromChildren(titleChild.get(), descChild.get(), descriptionText);
}

bool AccessibilitySVGObject::hasTitleOrDescriptionChild() const
{
    RefPtr element = this->element();
    if (!element)
        return false;

    for (const auto& child : childrenOfType<SVGElement>(*element)) {
        if (is<SVGTitleElement>(child) || is<SVGDescElement>(child))
            return true;
    }
    return false;
}

bool AccessibilitySVGObject::computeIsIgnored() const
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

    if (!m_renderer || m_renderer->isLegacyRenderSVGHiddenContainer() || m_renderer->isRenderSVGHiddenContainer())
        return true;

    // The SVG AAM states objects with at least one 'title' or 'desc' element MUST be included.
    // At this time, the presence of a matching 'lang' attribute is not mentioned in the spec.
    if (hasTitleOrDescriptionChild())
        return false;

    if (ignoredFromPresentationalRole())
        return true;

    if (ariaRoleAttribute() != AccessibilityRole::Unknown)
        return false;

    // The SVG AAM states text elements should also be included, if they have content.
    if (m_renderer->isRenderSVGText() || m_renderer->isRenderSVGTextPath()) {
        for (CheckedRef child : childrenOfType<RenderText>(downcast<RenderElement>(*m_renderer))) {
            if (!child->containsOnlyCollapsibleWhitespace())
                return false;
        }
    }

    // SVG shapes should not be included unless there's a concrete reason for inclusion.
    // https://rawgit.com/w3c/aria/master/svg-aam/svg-aam.html#exclude_elements
    if (m_renderer->isRenderOrLegacyRenderSVGShape()) {
        if (canSetFocusAttribute() || element()->hasEventListeners())
            return false;
        if (RefPtr svgParent = Accessibility::findAncestor<AccessibilityObject>(*this, true, [] (const AccessibilityObject& object) {
            return object.hasAttributesRequiredForInclusion() || object.isAccessibilitySVGRoot();
        }))
            return !svgParent->hasAttributesRequiredForInclusion();
        return true;
    }

    return AccessibilityRenderObject::computeIsIgnored();
}

bool AccessibilitySVGObject::inheritsPresentationalRole() const
{
    if (canSetFocusAttribute())
        return false;

    auto role = this->role();
    if (role != AccessibilityRole::SVGTextPath && role != AccessibilityRole::SVGTSpan)
        return false;

    for (RefPtr parent = parentObject(); parent; parent = parent->parentObject()) {
        if (is<AccessibilityRenderObject>(*parent) && parent->hasElementName(ElementName::SVG_text))
            return parent->role() == AccessibilityRole::Presentational;
    }

    return false;
}

AccessibilityRole AccessibilitySVGObject::determineAriaRoleAttribute() const
{
    auto role = AccessibilityRenderObject::determineAriaRoleAttribute();
    if (role != AccessibilityRole::Presentational)
        return role;

    // The presence of a 'title' or 'desc' child element trumps PresentationalRole.
    // https://lists.w3.org/Archives/Public/public-svg-a11y/2016Apr/0016.html
    // At this time, the presence of a matching 'lang' attribute is not mentioned.
    return hasTitleOrDescriptionChild() ? AccessibilityRole::Unknown : role;
}

AccessibilityRole AccessibilitySVGObject::determineAccessibilityRole()
{
    if (m_ariaRole != AccessibilityRole::Unknown)
        return m_ariaRole;

    if (!m_renderer)
        return AccessibilityRole::Unknown;

    if (isAccessibilitySVGRoot())
        return AccessibilityRole::Generic;

    RefPtr element = this->element();
    if (m_renderer->isRenderOrLegacyRenderSVGShape() || m_renderer->isRenderOrLegacyRenderSVGPath() || m_renderer->isRenderOrLegacyRenderSVGImage() || is<SVGUseElement>(element))
        return AccessibilityRole::Image;
    if (m_renderer->isRenderOrLegacyRenderSVGForeignObject())
        return AccessibilityRole::Generic;
    if (is<SVGGElement>(element)) {
        // https://w3c.github.io/svg-aam/#include_elements
        // g elements are generic (like a div) unless they have a name or is focusable.
        if (WebCore::hasAccNameAttribute(*element) || hasTitleOrDescriptionChild() || canSetFocusAttribute())
            return AccessibilityRole::Group;
        return AccessibilityRole::Generic;
    }
    if (m_renderer->isRenderSVGInlineText())
        return AccessibilityRole::StaticText;
    if (m_renderer->isRenderSVGText())
        return AccessibilityRole::SVGText;
    if (m_renderer->isRenderSVGTextPath())
        return AccessibilityRole::SVGTextPath;
    if (m_renderer->isRenderSVGTSpan())
        return AccessibilityRole::SVGTSpan;
    if (is<SVGAElement>(element))
        return AccessibilityRole::Link;

    return AccessibilityRenderObject::determineAccessibilityRole();
}

AccessibilityObject* AccessibilitySVGObject::parentObject() const
{
    if (m_parent) {
        // If a parent was set because this is a remote SVG resource, use that.
        AX_ASSERT(m_isSVGRoot);
        return m_parent.get();
    }

    // Otherwise, we should rely on the standard render tree for the parent.
    return AccessibilityRenderObject::parentObject();
}

bool AccessibilitySVGObject::isRootWithAccessibleContent() const
{
    if (!isAccessibilitySVGRoot())
        return false;

    RefPtr rootElement = this->element();
    if (!rootElement)
        return false;

    auto isAccessibleSVGElement = [] (const SVGElement& element) -> bool {
        // The presence of an SVGTitle or SVGDesc element is enough to deem the SVG hierarchy as accessible.
        if (is<SVGTitleElement>(element)
            || is<SVGDescElement>(element))
            return true;

        // Text content is accessible.
        if (element.isTextContent())
            return true;

        // If the role or aria-label attributes are specified, this is accessible.
        if (!element.attributeWithoutSynchronization(HTMLNames::roleAttr).isEmpty()
            || !element.attributeWithoutSynchronization(HTMLNames::aria_labelAttr).isEmpty())
            return true;

        return false;
    };

    RefPtr svgRootElement = dynamicDowncast<SVGElement>(*rootElement);
    if (svgRootElement && isAccessibleSVGElement(*svgRootElement))
        return true;

    // This SVG hierarchy is accessible if any of its descendants is accessible.
    for (const Ref descendant : descendantsOfType<SVGElement>(*rootElement)) {
        if (isAccessibleSVGElement(descendant.get()))
            return true;
    }

    return false;
}

} // namespace WebCore
