/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "ElementTargeting.h"

#include "DOMTokenList.h"
#include "Document.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementChildIteratorInlines.h"
#include "ElementInlines.h"
#include "ElementTargetingTypes.h"
#include "FloatPoint.h"
#include "HTMLBodyElement.h"
#include "HTMLNames.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "NodeList.h"
#include "Page.h"
#include "Region.h"
#include "RenderDescendantIterator.h"
#include "RenderIFrame.h"
#include "RenderView.h"
#include "TextExtraction.h"
#include "TypedElementDescendantIteratorInlines.h"

namespace WebCore {

static inline bool elementAndAncestorsAreOnlyChildren(const Element& element)
{
    for (auto& ancestor : ancestorsOfType<Element>(element)) {
        if (!ancestor.hasOneChild())
            return false;
    }
    return true;
}

static inline bool querySelectorMatchesOneElement(Document& document, const String& selector)
{
    auto result = document.querySelectorAll(selector);
    return !result.hasException() && result.returnValue()->length() == 1;
}

struct ChildElementPosition {
    size_t index { notFound };
    size_t childCountOfType { 0 };
};

static inline ChildElementPosition childIndexByType(Element& element, Element& parent)
{
    ChildElementPosition result;
    auto elementTagName = element.tagName();
    for (auto& child : childrenOfType<Element>(parent)) {
        if (child.tagName() != elementTagName)
            continue;

        if (&child == &element)
            result.index = result.childCountOfType;

        result.childCountOfType++;
    }

    return result;
}

static inline String computeIDSelector(Element& element)
{
    if (element.hasID()) {
        auto elementID = element.getIdAttribute();
        if (auto* matches = element.document().getAllElementsById(elementID); matches && matches->size() == 1)
            return makeString('#', elementID);
    }
    return { };
}

static inline String computeClassSelector(Element& element)
{
    if (element.hasClass()) {
        static constexpr auto maxNumberOfClasses = 5;
        auto& classList = element.classList();
        Vector<String> classes;
        classes.reserveInitialCapacity(classList.length());
        for (unsigned i = 0; i < std::min<unsigned>(maxNumberOfClasses, classList.length()); ++i)
            classes.append(classList.item(i));
        auto selector = makeString('.', makeStringByJoining(classes, "."_s));
        if (querySelectorMatchesOneElement(element.document(), selector))
            return selector;
    }
    return { };
}

static String parentRelativeSelectorRecursive(Element&);
static String selectorForElementRecursive(Element& element)
{
    if (auto selector = computeIDSelector(element); !selector.isEmpty())
        return selector;

    if (auto selector = computeClassSelector(element); !selector.isEmpty())
        return selector;

    if (querySelectorMatchesOneElement(element.document(), element.tagName()))
        return element.tagName();

    return parentRelativeSelectorRecursive(element);
}

static String parentRelativeSelectorRecursive(Element& element)
{
    RefPtr parent = element.parentElement();
    if (!parent)
        return { };

    if (auto selector = selectorForElementRecursive(*parent); !selector.isEmpty()) {
        auto selectorPrefix = makeString(WTFMove(selector), " > "_s, element.tagName());
        auto [childIndex, childCountOfType] = childIndexByType(element, *parent);
        if (childIndex == notFound)
            return { };

        if (childCountOfType == 1)
            return selectorPrefix;

        if (!childIndex)
            return makeString(WTFMove(selectorPrefix), ":first-of-type"_s);

        if (childIndex == childCountOfType - 1)
            return makeString(WTFMove(selectorPrefix), ":last-of-type"_s);

        return makeString(WTFMove(selectorPrefix), ":nth-child("_s, childIndex + 1, ')');
    }

    return { };
}

// Returns multiple CSS selectors that uniquely match the target element.
static Vector<String> selectorsForTarget(Element& element)
{
    Vector<String> selectors;
    if (auto selector = computeIDSelector(element); !selector.isEmpty())
        selectors.append(WTFMove(selector));

    if (auto selector = computeClassSelector(element); !selector.isEmpty())
        selectors.append(WTFMove(selector));

    if (querySelectorMatchesOneElement(element.document(), element.tagName()))
        selectors.append(element.tagName());

    if (selectors.isEmpty()) {
        // Only fall back on the parent relative selector as a last resort.
        if (auto selector = parentRelativeSelectorRecursive(element); !selector.isEmpty())
            selectors.append(WTFMove(selector));
    }

    return selectors;
}

static inline RectEdges<bool> computeOffsetEdges(const RenderStyle& style)
{
    return {
        style.top().isSpecified(),
        style.right().isSpecified(),
        style.bottom().isSpecified(),
        style.left().isSpecified()
    };
}

static inline Vector<FrameIdentifier> collectChildFrameIdentifiers(Element& element)
{
    Vector<FrameIdentifier> identifiers;
    for (auto& owner : descendantsOfType<HTMLFrameOwnerElement>(element)) {
        if (RefPtr frame = owner.protectedContentFrame())
            identifiers.append(frame->frameID());
    }
    return identifiers;
}

enum class IsUnderPoint : bool { No, Yes };
static TargetedElementInfo targetedElementInfo(Element& element, IsUnderPoint isUnderPoint)
{
    CheckedPtr renderer = element.renderer();
    return {
        .elementIdentifier = element.identifier(),
        .documentIdentifier = element.document().identifier(),
        .offsetEdges = computeOffsetEdges(renderer->style()),
        .renderedText = TextExtraction::extractRenderedText(element),
        .selectors = selectorsForTarget(element),
        .boundsInRootView = element.boundingBoxInRootViewCoordinates(),
        .positionType = renderer->style().position(),
        .childFrameIdentifiers = collectChildFrameIdentifiers(element),
        .isUnderPoint = isUnderPoint == IsUnderPoint::Yes,
    };
}

Vector<TargetedElementInfo> findTargetedElements(Page& page, TargetedElementRequest&& request)
{
    RefPtr mainFrame = dynamicDowncast<LocalFrame>(page.mainFrame());
    if (!mainFrame)
        return { };

    RefPtr document = mainFrame->document();
    if (!document)
        return { };

    RefPtr view = mainFrame->view();
    if (!view)
        return { };

    RefPtr bodyElement = document->body();
    if (!bodyElement)
        return { };

    RefPtr documentElement = document->documentElement();
    if (!documentElement)
        return { };

    FloatSize viewportSize = view->baseLayoutViewportSize();
    auto viewportArea = viewportSize.area();

    static constexpr OptionSet hitTestOptions {
        HitTestRequest::Type::ReadOnly,
        HitTestRequest::Type::DisallowUserAgentShadowContent,
        HitTestRequest::Type::IgnoreCSSPointerEventsProperty,
        HitTestRequest::Type::CollectMultipleElements,
        HitTestRequest::Type::IncludeAllElementsUnderPoint
    };

    HitTestResult result { LayoutPoint { view->rootViewToContents(request.pointInRootView) } };
    document->hitTest(hitTestOptions, result);

    RefPtr<Element> onlyMainElement;
    for (auto& descendant : descendantsOfType<HTMLElement>(*bodyElement)) {
        if (!descendant.hasTagName(HTMLNames::mainTag))
            continue;

        if (onlyMainElement) {
            onlyMainElement = nullptr;
            break;
        }

        onlyMainElement = &descendant;
    }

    auto isCandidate = [&](Element& element) {
        if (!element.renderer())
            return false;

        if (&element == document->body())
            return false;

        if (&element == document->documentElement())
            return false;

        if (onlyMainElement && (onlyMainElement == &element || element.contains(*onlyMainElement)))
            return false;

        if (elementAndAncestorsAreOnlyChildren(element))
            return false;

        return true;
    };

    auto candidates = [&] {
        auto& results = result.listBasedTestResult();
        Vector<Ref<Element>> elements;
        elements.reserveInitialCapacity(results.size());
        for (auto& node : results) {
            if (RefPtr element = dynamicDowncast<Element>(node); element && isCandidate(*element))
                elements.append(element.releaseNonNull());
        }
        return elements;
    }();

    static constexpr auto maximumAreaRatioForAbsolutelyPositionedContent = 0.75;
    static constexpr auto maximumAreaRatioForInFlowContent = 0.5;
    static constexpr auto maximumAreaRatioForNearbyTargets = 0.25;
    static constexpr auto minimumAreaRatioForInFlowContent = 0.01;

    auto computeViewportAreaRatio = [&](IntRect boundingBox) {
        auto area = boundingBox.area<RecordOverflow>();
        return area.hasOverflowed() ? std::numeric_limits<float>::max() : area.value() / viewportArea;
    };

    Vector<Ref<Element>> targets; // The front-most target is last in this list.
    Region additionalRegionForNearbyElements;

    // Prioritize parent elements over their children by traversing backwards over the candidates.
    // This allows us to target only the top-most container elements that satisfy the criteria.
    // While adding targets, we also accumulate additional regions, wherein we should report any
    // nearby targets.
    while (!candidates.isEmpty()) {
        Ref target = candidates.takeLast();
        CheckedPtr targetRenderer = target->renderer();
        auto targetBoundingBox = target->boundingBoxInRootViewCoordinates();
        auto targetAreaRatio = computeViewportAreaRatio(targetBoundingBox);
        bool shouldAddTarget = targetRenderer->isFixedPositioned()
            || targetRenderer->isStickilyPositioned()
            || (targetRenderer->isAbsolutelyPositioned() && targetAreaRatio < maximumAreaRatioForAbsolutelyPositionedContent)
            || (minimumAreaRatioForInFlowContent < targetAreaRatio && targetAreaRatio < maximumAreaRatioForInFlowContent);

        if (!shouldAddTarget)
            continue;

        bool checkForNearbyTargets = request.canIncludeNearbyElements
            && targetRenderer->isOutOfFlowPositioned()
            && targetAreaRatio < maximumAreaRatioForNearbyTargets;

        if (checkForNearbyTargets && computeViewportAreaRatio(targetBoundingBox) < maximumAreaRatioForNearbyTargets)
            additionalRegionForNearbyElements.unite(targetBoundingBox);

        candidates.removeAllMatching([&](auto& candidate) {
            if (target.ptr() != candidate.ptr() && !target->contains(candidate))
                return false;

            if (checkForNearbyTargets) {
                auto boundingBox = candidate->boundingBoxInRootViewCoordinates();
                if (computeViewportAreaRatio(boundingBox) < maximumAreaRatioForNearbyTargets)
                    additionalRegionForNearbyElements.unite(boundingBox);
            }

            return true;
        });

        targets.append(WTFMove(target));
    }

    if (targets.isEmpty())
        return { };

    Vector<TargetedElementInfo> results;
    results.reserveInitialCapacity(targets.size());
    for (auto iterator = targets.rbegin(); iterator != targets.rend(); ++iterator)
        results.append(targetedElementInfo(*iterator, IsUnderPoint::Yes));

    if (additionalRegionForNearbyElements.isEmpty())
        return results;

    auto nearbyTargets = [&] {
        HashSet<Ref<Element>> targets;
        CheckedPtr bodyRenderer = bodyElement->renderer();
        if (!bodyRenderer)
            return targets;

        for (auto& renderer : descendantsOfType<RenderElement>(*bodyRenderer)) {
            if (!renderer.isOutOfFlowPositioned())
                continue;

            RefPtr element = renderer.protectedElement();
            if (!element)
                continue;

            if (targets.contains(*element))
                continue;

            if (result.listBasedTestResult().contains(*element))
                continue;

            if (!isCandidate(*element))
                continue;

            auto boundingBox = element->boundingBoxInRootViewCoordinates();
            if (!additionalRegionForNearbyElements.contains(boundingBox))
                continue;

            if (computeViewportAreaRatio(boundingBox) > maximumAreaRatioForNearbyTargets)
                continue;

            targets.add(element.releaseNonNull());
        }
        return targets;
    }();

    for (auto& element : nearbyTargets)
        results.append(targetedElementInfo(element, IsUnderPoint::No));

    return results;
}

static bool setNeedsVisibilityAdjustmentRecursive(Element& element)
{
    if (element.isVisibilityAdjustmentRoot())
        return false;

    element.setIsVisibilityAdjustmentRoot();

    CheckedPtr renderer = element.renderer();
    if (!renderer)
        return true;

    for (auto& frameRenderer : descendantsOfType<RenderIFrame>(*renderer)) {
        RefPtr childView = frameRenderer.childView();
        if (!childView)
            continue;

        CheckedPtr childRenderView = childView->checkedRenderView();
        if (!childRenderView)
            continue;

        RefPtr childDocumentElement = childRenderView->document().documentElement();
        if (!childDocumentElement)
            continue;

        setNeedsVisibilityAdjustmentRecursive(*childDocumentElement);
    }

    element.invalidateStyleAndRenderersForSubtree();
    return true;
}

bool adjustVisibilityForTargetedElements(const Vector<Ref<Element>>& elements)
{
    bool changed = false;
    for (auto& element : elements) {
        if (setNeedsVisibilityAdjustmentRecursive(element))
            changed = true;
    }
    return changed;
}

} // namespace WebCore
