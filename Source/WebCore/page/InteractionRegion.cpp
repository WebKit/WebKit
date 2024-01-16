/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "InteractionRegion.h"

#include "Document.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementInlines.h"
#include "ElementRuleCollector.h"
#include "FrameSnapshotting.h"
#include "GeometryUtilities.h"
#include "HTMLAnchorElement.h"
#include "HTMLAttachmentElement.h"
#include "HTMLButtonElement.h"
#include "HTMLFieldSetElement.h"
#include "HTMLFormControlElement.h"
#include "HTMLInputElement.h"
#include "HTMLLabelElement.h"
#include "HitTestResult.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PseudoClassChangeInvalidation.h"
#include "RenderAncestorIterator.h"
#include "RenderBoxInlines.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "SimpleRange.h"
#include "SliderThumbElement.h"
#include "StyleResolver.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

InteractionRegion::~InteractionRegion() = default;

static CursorType cursorTypeForElement(Element& element)
{
    auto* renderer = element.renderer();
    auto* style = renderer ? &renderer->style() : nullptr;
    auto cursorType = style ? style->cursor() : CursorType::Auto;

    if (cursorType == CursorType::Auto && element.enclosingLinkEventParentOrSelf())
        cursorType = CursorType::Pointer;

    return cursorType;
}

static bool shouldAllowElement(const Element& element)
{
    if (is<HTMLFieldSetElement>(element))
        return false;

    if (auto* input = dynamicDowncast<HTMLInputElement>(element)) {
        if (input->isDisabledFormControl())
            return false;

        // Do not allow regions for the <input type='range'>, because we make one for the thumb.
        if (input->isRangeControl())
            return false;

        // Do not allow regions for the <input type='file'>, because we make one for the button.
        if (input->isFileUpload())
            return false;
    }

    return true;
}

static bool shouldAllowAccessibilityRoleAsPointerCursorReplacement(const Element& element)
{
    switch (AccessibilityObject::ariaRoleToWebCoreRole(element.attributeWithoutSynchronization(HTMLNames::roleAttr))) {
    case AccessibilityRole::Button:
    case AccessibilityRole::Checkbox:
    case AccessibilityRole::ImageMapLink:
    case AccessibilityRole::Link:
    case AccessibilityRole::WebCoreLink:
    case AccessibilityRole::ListBoxOption:
    case AccessibilityRole::MenuButton:
    case AccessibilityRole::MenuItem:
    case AccessibilityRole::MenuItemCheckbox:
    case AccessibilityRole::MenuItemRadio:
    case AccessibilityRole::PopUpButton:
    case AccessibilityRole::RadioButton:
    case AccessibilityRole::Switch:
    case AccessibilityRole::TextField:
    case AccessibilityRole::ToggleButton:
        return true;
    default:
        return false;
    }
}

bool elementMatchesHoverRules(Element& element)
{
    bool foundHoverRules = false;
    bool initialValue = element.isUserActionElement() && element.document().userActionElements().isHovered(element);

    for (auto key : Style::makePseudoClassInvalidationKeys(CSSSelector::PseudoClass::Hover, element)) {
        auto& ruleSets = element.styleResolver().ruleSets();
        auto* invalidationRuleSets = ruleSets.pseudoClassInvalidationRuleSets(key);
        if (!invalidationRuleSets)
            continue;

        for (auto& invalidationRuleSet : *invalidationRuleSets) {
            element.document().userActionElements().setHovered(element, invalidationRuleSet.isNegation == Style::IsNegation::No);
            Style::ElementRuleCollector ruleCollector(element, *invalidationRuleSet.ruleSet, nullptr);
            ruleCollector.setMode(SelectorChecker::Mode::CollectingRulesIgnoringVirtualPseudoElements);
            if (ruleCollector.matchesAnyAuthorRules()) {
                foundHoverRules = true;
                break;
            }
        }

        if (foundHoverRules)
            break;
    }

    element.document().userActionElements().setHovered(element, initialValue);
    return foundHoverRules;
}

static bool shouldAllowNonPointerCursorForElement(const Element& element)
{
#if ENABLE(ATTACHMENT_ELEMENT)
    if (is<HTMLAttachmentElement>(element))
        return true;
#endif

    if (is<HTMLTextFormControlElement>(element))
        return !element.focused();

    if (is<HTMLFormControlElement>(element))
        return true;

    if (is<SliderThumbElement>(element))
        return true;

    if (is<HTMLAnchorElement>(element))
        return true;

    if (shouldAllowAccessibilityRoleAsPointerCursorReplacement(element))
        return true;

    return false;
}

static bool shouldGetOcclusion(const RenderElement& renderer)
{
    if (auto* renderLayerModelObject = dynamicDowncast<RenderBox>(renderer)) {
        if (renderLayerModelObject->hasLayer() && renderLayerModelObject->layer()->isComposited())
            return false;
    }

    if (renderer.style().specifiedZIndex() > 0)
        return true;

    if (renderer.isFixedPositioned())
        return true;

    return false;
}

std::optional<InteractionRegion> interactionRegionForRenderedRegion(RenderObject& regionRenderer, const FloatRect& bounds)
{
    if (bounds.isEmpty())
        return std::nullopt;

    if (!regionRenderer.node())
        return std::nullopt;

    Ref mainFrameView = *regionRenderer.document().frame()->mainFrame().virtualView();

    FloatSize frameViewSize = mainFrameView->size();
    auto scale = 1 / mainFrameView->visibleContentScaleFactor();
    frameViewSize.scale(scale, scale);
    auto frameViewArea = frameViewSize.area();

    auto originalElement = dynamicDowncast<Element>(regionRenderer.node());
    if (originalElement && originalElement->isPseudoElement())
        return std::nullopt;

    auto matchedElement = originalElement;
    if (!matchedElement)
        matchedElement = regionRenderer.node()->parentElement();
    if (!matchedElement)
        return std::nullopt;

    bool isLabelable = is<HTMLElement>(matchedElement) && downcast<HTMLElement>(matchedElement)->isLabelable();
    for (Node* node = matchedElement; node; node = node->parentInComposedTree()) {
        auto* element = dynamicDowncast<Element>(node);
        if (!element)
            continue;
        bool matchedButton = is<HTMLButtonElement>(*element);
        bool matchedLabel = isLabelable && is<HTMLLabelElement>(*element);
        bool matchedLink = element->isLink();
        if (matchedButton || matchedLabel || matchedLink) {
            matchedElement = element;
            break;
        }
    }

    if (!shouldAllowElement(*matchedElement))
        return std::nullopt;

    if (!matchedElement->renderer())
        return std::nullopt;
    auto& renderer = *matchedElement->renderer();

    if (renderer.style().effectivePointerEvents() == PointerEvents::None)
        return std::nullopt;

    bool isOriginalMatch = matchedElement == originalElement;

    // FIXME: Consider also allowing elements that only receive touch events.
    bool hasListener = renderer.style().eventListenerRegionTypes().contains(EventListenerRegionType::MouseClick);
    bool hasPointer = cursorTypeForElement(*matchedElement) == CursorType::Pointer || shouldAllowNonPointerCursorForElement(*matchedElement);
    bool isTooBigForInteraction = bounds.area() > frameViewArea / 3;
    bool isTooBigForOcclusion = bounds.area() > frameViewArea * 3;

    auto elementIdentifier = matchedElement->identifier();

    if (!hasPointer && is<HTMLLabelElement>(matchedElement)) {
        // Could be a `<label for="...">` or a label with a descendant.
        // In cases where both elements get a region we want to group them by the same `elementIdentifier`.
        auto associatedElement = downcast<HTMLLabelElement>(matchedElement)->control();
        if (associatedElement && !associatedElement->isDisabledFormControl()) {
            hasPointer = true;
            elementIdentifier = associatedElement->identifier();
        }
    }

    bool detectedHoverRules = false;
    if (!hasPointer) {
        // The hover check can be expensive (it may end up doing selector matching), so we only run it on some elements.
        bool hasVisibleBoxDecorations = renderer.hasVisibleBoxDecorations();
        bool nonScrollable = !is<RenderBox>(renderer) || (!downcast<RenderBox>(renderer).hasScrollableOverflowX() && !downcast<RenderBox>(renderer).hasScrollableOverflowY());
        if (hasVisibleBoxDecorations && nonScrollable)
            detectedHoverRules = elementMatchesHoverRules(*matchedElement);
    }

    if (!hasListener || !(hasPointer || detectedHoverRules) || isTooBigForInteraction) {
        if (isOriginalMatch && shouldGetOcclusion(renderer) && !isTooBigForOcclusion) {
            return { {
                InteractionRegion::Type::Occlusion,
                elementIdentifier,
                bounds
            } };
        }

        return std::nullopt;
    }

    bool isInlineNonBlock = renderer.isInline() && !renderer.isReplacedOrInlineBlock();

    // The parent will get its own InteractionRegion.
    if (!isOriginalMatch && !isInlineNonBlock && !renderer.style().isDisplayTableOrTablePart())
        return std::nullopt;

    auto rect = bounds;
    float cornerRadius = 0;
    OptionSet<InteractionRegion::CornerMask> maskedCorners { };
    std::optional<Path> clipPath = std::nullopt;

    if (const auto& renderBox = dynamicDowncast<RenderBox>(regionRenderer)) {
        auto roundedRect = renderBox->borderRoundedRect();
        auto borderRadii = roundedRect.radii();
        auto minRadius = borderRadii.minimumRadius();
        auto maxRadius = borderRadii.maximumRadius();

        bool needsClipPath = false;
        auto checkIfClipPathNeeded = [&](auto radius) {
            if (radius != minRadius && radius != maxRadius)
                needsClipPath = true;
        };
        checkIfClipPathNeeded(borderRadii.topLeft().minDimension());
        checkIfClipPathNeeded(borderRadii.topRight().minDimension());
        checkIfClipPathNeeded(borderRadii.bottomLeft().minDimension());
        checkIfClipPathNeeded(borderRadii.bottomRight().minDimension());

        if (minRadius == maxRadius)
            cornerRadius = minRadius;
        else if (!minRadius && !needsClipPath) {
            cornerRadius = maxRadius;
            if (borderRadii.topLeft().minDimension() == maxRadius)
                maskedCorners.add(InteractionRegion::CornerMask::MinXMinYCorner);
            if (borderRadii.topRight().minDimension() == maxRadius)
                maskedCorners.add(InteractionRegion::CornerMask::MaxXMinYCorner);
            if (borderRadii.bottomLeft().minDimension() == maxRadius)
                maskedCorners.add(InteractionRegion::CornerMask::MinXMaxYCorner);
            if (borderRadii.bottomRight().minDimension() == maxRadius)
                maskedCorners.add(InteractionRegion::CornerMask::MaxXMaxYCorner);
        } else {
            Path path;
            path.addRoundedRect(roundedRect);
            clipPath = path;
        }
    }

    auto& style = regionRenderer.style();
    bool canTweakShape = !style.hasBackground()
        && !style.hasOutline()
        && !style.boxShadow()
        && !style.hasExplicitlySetBorderRadius()
        // No visible borders or borders that do not create a complete box.
        && (!style.hasVisibleBorder()
            || !(style.borderTopWidth() && style.borderRightWidth() && style.borderBottomWidth() && style.borderLeftWidth()));

    if (canTweakShape) {
        // We can safely tweak the bounds and radius without causing visual mismatch.
        cornerRadius = std::max<float>(cornerRadius, regionRenderer.document().settings().interactionRegionMinimumCornerRadius());
        if (isInlineNonBlock)
            rect.inflate(regionRenderer.document().settings().interactionRegionInlinePadding());
    }

    return { {
        InteractionRegion::Type::Interaction,
        elementIdentifier,
        rect,
        cornerRadius,
        maskedCorners,
        clipPath
    } };
}

TextStream& operator<<(TextStream& ts, const InteractionRegion& interactionRegion)
{
    auto regionName = interactionRegion.type == InteractionRegion::Type::Interaction
        ? "interaction"
        : (interactionRegion.type == InteractionRegion::Type::Occlusion ? "occlusion" : "guard");
    ts.dumpProperty(regionName, interactionRegion.rectInLayerCoordinates);
    auto radius = interactionRegion.cornerRadius;
    if (radius > 0) {
        if (interactionRegion.maskedCorners.isEmpty())
            ts.dumpProperty("cornerRadius", radius);
        else  {
            auto mask = interactionRegion.maskedCorners;
            ts.dumpProperty("cornerRadius", makeString(
                mask.contains(InteractionRegion::CornerMask::MinXMinYCorner) ? radius : 0, ' ',
                mask.contains(InteractionRegion::CornerMask::MaxXMinYCorner) ? radius : 0, ' ',
                mask.contains(InteractionRegion::CornerMask::MaxXMaxYCorner) ? radius : 0, ' ',
                mask.contains(InteractionRegion::CornerMask::MinXMaxYCorner) ? radius : 0
            ));
        }
    }
    if (interactionRegion.clipPath)
        ts.dumpProperty("clipPath", interactionRegion.clipPath.value());

    return ts;
}

}
