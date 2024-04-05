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
#include "ElementTargetingController.h"

#include "DOMTokenList.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementChildIteratorInlines.h"
#include "ElementInlines.h"
#include "ElementTargetingTypes.h"
#include "FloatPoint.h"
#include "FloatRect.h"
#include "HTMLBodyElement.h"
#include "HTMLNames.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "NodeList.h"
#include "Page.h"
#include "PseudoElement.h"
#include "Region.h"
#include "RenderDescendantIterator.h"
#include "RenderView.h"
#include "TextExtraction.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "VisibilityAdjustment.h"

namespace WebCore {

static constexpr auto maximumNumberOfClasses = 5;
static constexpr auto maximumAreaRatioForAbsolutelyPositionedContent = 0.75;
static constexpr auto maximumAreaRatioForInFlowContent = 0.5;
static constexpr auto maximumAreaRatioForNearbyTargets = 0.25;
static constexpr auto minimumAreaRatioForInFlowContent = 0.01;
static constexpr auto maximumAreaRatioForTrackingAdjustmentAreas = 0.25;
static constexpr auto marginForTrackingAdjustmentRects = 5;
static constexpr auto minimumDistanceToConsiderEdgesEquidistant = 2;
static constexpr auto selectorBasedVisibilityAdjustmentTimeLimit = 30_s;
static constexpr auto adjustmentClientRectCleanUpDelay = 15_s;

ElementTargetingController::ElementTargetingController(Page& page)
    : m_page { page }
    , m_recentAdjustmentClientRectsCleanUpTimer { *this, &ElementTargetingController::cleanUpAdjustmentClientRects, adjustmentClientRectCleanUpDelay }
{
}

static inline bool elementAndAncestorsAreOnlyRenderedChildren(const Element& element)
{
    CheckedPtr renderer = element.renderer();
    if (!renderer)
        return false;

    for (auto& ancestor : ancestorsOfType<RenderElement>(*renderer)) {
        unsigned numberOfRenderedChildren = 0;
        for (auto& child : childrenOfType<RenderElement>(ancestor)) {
            if (RefPtr childElement = child.element(); childElement && !childElement->visibilityAdjustment().contains(VisibilityAdjustment::Subtree))
                numberOfRenderedChildren++;
        }
        if (numberOfRenderedChildren >= 2)
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
        auto& classList = element.classList();
        Vector<String> classes;
        classes.reserveInitialCapacity(classList.length());
        for (unsigned i = 0; i < std::min<unsigned>(maximumNumberOfClasses, classList.length()); ++i)
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
    if (RefPtr pseudoElement = dynamicDowncast<PseudoElement>(element)) {
        RefPtr host = pseudoElement->hostElement();
        if (!host)
            return { };

        auto pseudoSelector = [&]() -> String {
            if (element.isBeforePseudoElement())
                return "::before"_s;

            if (element.isAfterPseudoElement())
                return "::after"_s;

            return { };
        }();

        if (pseudoSelector.isEmpty())
            return { };

        return selectorsForTarget(*host).map([&](auto hostSelector) {
            return makeString(hostSelector, pseudoSelector);
        });
    }

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

static FloatRect computeClientRect(RenderObject& renderer)
{
    FloatRect rect = renderer.absoluteBoundingBoxRect();
    renderer.document().convertAbsoluteToClientRect(rect, renderer.style());
    return rect;
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
        .boundsInClientCoordinates = computeClientRect(*renderer),
        .positionType = renderer->style().position(),
        .childFrameIdentifiers = collectChildFrameIdentifiers(element),
        .isUnderPoint = isUnderPoint == IsUnderPoint::Yes,
        .isPseudoElement = element.isPseudoElement(),
    };
}

static inline HTMLElement* findOnlyMainElement(HTMLBodyElement& bodyElement)
{
    RefPtr<HTMLElement> onlyMainElement;
    for (auto& descendant : descendantsOfType<HTMLElement>(bodyElement)) {
        if (!descendant.hasTagName(HTMLNames::mainTag))
            continue;

        if (onlyMainElement) {
            onlyMainElement = nullptr;
            break;
        }

        onlyMainElement = &descendant;
    }
    return onlyMainElement.get();
}

static bool isTargetCandidate(Element& element, const HTMLElement* onlyMainElement)
{
    if (!element.renderer())
        return false;

    if (element.isBeforePseudoElement() || element.isAfterPseudoElement()) {
        // We don't need to worry about affecting main content if we're only adjusting pseudo elements.
        return true;
    }

    if (&element == element.document().body())
        return false;

    if (&element == element.document().documentElement())
        return false;

    if (onlyMainElement && (onlyMainElement == &element || element.contains(*onlyMainElement)))
        return false;

    if (elementAndAncestorsAreOnlyRenderedChildren(element))
        return false;

    return true;
}

static inline std::optional<IntRect> inflatedClientRectForAdjustmentRegionTracking(Element& element, float viewportArea)
{
    CheckedPtr renderer = element.checkedRenderer();
    if (!renderer)
        return { };

    if (!renderer->isOutOfFlowPositioned())
        return { };

    auto clientRect = computeClientRect(*renderer);
    if (clientRect.isEmpty())
        return { };

    if (clientRect.area() / viewportArea >= maximumAreaRatioForTrackingAdjustmentAreas)
        return { };

    // Keep track of the client rects of elements we're targeting, until the client
    // triggers visibility adjustment for these elements.
    auto inflatedClientRect = enclosingIntRect(clientRect);
    inflatedClientRect.inflate(marginForTrackingAdjustmentRects);
    return { inflatedClientRect };
}

Vector<TargetedElementInfo> ElementTargetingController::findTargets(TargetedElementRequest&& request)
{
    RefPtr page = m_page.get();
    if (!page)
        return { };

    RefPtr mainFrame = dynamicDowncast<LocalFrame>(page->mainFrame());
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
    if (!viewportArea)
        return { };

    static constexpr OptionSet hitTestOptions {
        HitTestRequest::Type::ReadOnly,
        HitTestRequest::Type::DisallowUserAgentShadowContent,
        HitTestRequest::Type::IgnoreCSSPointerEventsProperty,
        HitTestRequest::Type::CollectMultipleElements,
        HitTestRequest::Type::IncludeAllElementsUnderPoint
    };

    HitTestResult result { LayoutPoint { view->rootViewToContents(request.pointInRootView) } };
    document->hitTest(hitTestOptions, result);

    RefPtr onlyMainElement = findOnlyMainElement(*bodyElement);
    auto candidates = [&] {
        auto& results = result.listBasedTestResult();
        Vector<Ref<Element>> elements;
        elements.reserveInitialCapacity(results.size());
        for (auto& node : results) {
            if (RefPtr element = dynamicDowncast<Element>(node); element && isTargetCandidate(*element, onlyMainElement.get()))
                elements.append(element.releaseNonNull());
        }
        return elements;
    }();

    auto addOutOfFlowTargetClientRectIfNeeded = [&](Element& element) {
        if (auto rect = inflatedClientRectForAdjustmentRegionTracking(element, viewportArea))
            m_recentAdjustmentClientRects.set(element.identifier(), *rect);
    };

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

    m_recentAdjustmentClientRectsCleanUpTimer.restart();

    Vector<TargetedElementInfo> results;
    results.reserveInitialCapacity(targets.size());
    for (auto iterator = targets.rbegin(); iterator != targets.rend(); ++iterator) {
        results.append(targetedElementInfo(*iterator, IsUnderPoint::Yes));
        addOutOfFlowTargetClientRectIfNeeded(*iterator);
    }

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

            if (!isTargetCandidate(*element, onlyMainElement.get()))
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

    for (auto& element : nearbyTargets) {
        results.append(targetedElementInfo(element, IsUnderPoint::No));
        addOutOfFlowTargetClientRectIfNeeded(element);
    }

    return results;
}

static inline Element& elementToAdjust(Element& element)
{
    if (RefPtr pseudoElement = dynamicDowncast<PseudoElement>(element)) {
        if (RefPtr host = pseudoElement->hostElement())
            return *host;
    }
    return element;
}

static inline VisibilityAdjustment adjustmentToApply(Element& element)
{
    if (element.isAfterPseudoElement())
        return VisibilityAdjustment::AfterPseudo;

    if (element.isBeforePseudoElement())
        return VisibilityAdjustment::BeforePseudo;

    return VisibilityAdjustment::Subtree;
}

struct VisibilityAdjustmentResult {
    RefPtr<Element> adjustedElement;
    bool invalidateSubtree { false };
};

static inline VisibilityAdjustmentResult adjustVisibilityIfNeeded(Element& element)
{
    Ref adjustedElement = elementToAdjust(element);
    auto adjustment = adjustmentToApply(element);
    auto currentAdjustment = adjustedElement->visibilityAdjustment();
    if (currentAdjustment.contains(adjustment))
        return { };

    adjustedElement->setVisibilityAdjustment(currentAdjustment | adjustment);
    return { adjustedElement.ptr(), adjustment == VisibilityAdjustment::Subtree };
}

bool ElementTargetingController::adjustVisibility(const Vector<std::pair<ElementIdentifier, ScriptExecutionContextIdentifier>>& identifiers)
{
    RefPtr page = m_page.get();
    if (!page)
        return false;

    RefPtr mainFrame = dynamicDowncast<LocalFrame>(page->mainFrame());
    if (!mainFrame)
        return false;

    RefPtr frameView = mainFrame->view();
    if (!frameView)
        return false;

    FloatSize viewportSize = frameView->baseLayoutViewportSize();
    auto viewportArea = viewportSize.area();
    if (!viewportArea)
        return false;

    Region newAdjustmentRegion;
    for (auto [elementID, documentID] : identifiers) {
        if (auto rect = m_recentAdjustmentClientRects.get(elementID); !rect.isEmpty())
            newAdjustmentRegion.unite(rect);
    }

    m_repeatedAdjustmentClientRegion.unite(intersect(m_adjustmentClientRegion, newAdjustmentRegion));
    m_adjustmentClientRegion.unite(newAdjustmentRegion);

    Vector<Ref<Element>> elements;
    elements.reserveInitialCapacity(identifiers.size());
    for (auto [elementID, documentID] : identifiers) {
        if (RefPtr element = Element::fromIdentifier(elementID); element && element->document().identifier() == documentID)
            elements.append(element.releaseNonNull());
    }

    bool changed = false;
    for (auto& element : elements) {
        CheckedPtr renderer = element->renderer();
        if (!renderer)
            continue;

        auto [adjustedElement, invalidateSubtree] = adjustVisibilityIfNeeded(element);
        if (!adjustedElement)
            continue;

        changed = true;

        if (invalidateSubtree)
            adjustedElement->invalidateStyleAndRenderersForSubtree();
        else
            adjustedElement->invalidateStyle();
        m_adjustedElements.add(element);
    }
    return changed;
}

static void adjustRegionAfterViewportSizeChange(Region& region, FloatSize oldSize, FloatSize newSize)
{
    if (region.isEmpty())
        return;

    bool shouldRebuildRegion = false;
    auto adjustedRects = region.rects().map([&](auto rect) {
        auto distanceToLeftEdge = std::max<float>(0, rect.x());
        auto distanceToTopEdge = std::max<float>(0, rect.y());
        auto distanceToRightEdge = std::max<float>(0, oldSize.width() - rect.maxX());
        auto distanceToBottomEdge = std::max<float>(0, oldSize.height() - rect.maxY());
        float widthDelta = newSize.width() - oldSize.width();
        float heightDelta = newSize.height() - oldSize.height();

        FloatRect adjustedRect = rect;
        if (widthDelta) {
            if (std::abs(distanceToLeftEdge - distanceToRightEdge) < minimumDistanceToConsiderEdgesEquidistant)
                adjustedRect.inflateX(widthDelta / 2);
            else if (distanceToRightEdge < distanceToLeftEdge)
                adjustedRect.move(widthDelta, 0);
        }

        if (heightDelta) {
            if (std::abs(distanceToTopEdge - distanceToBottomEdge) < minimumDistanceToConsiderEdgesEquidistant)
                adjustedRect.inflateY(heightDelta / 2);
            else if (distanceToBottomEdge < distanceToTopEdge)
                adjustedRect.move(heightDelta, 0);
        }

        auto enclosingAdjustedRect = enclosingIntRect(adjustedRect);
        if (enclosingAdjustedRect != rect)
            shouldRebuildRegion |= true;

        return enclosingAdjustedRect;
    });

    if (!shouldRebuildRegion)
        return;

    region = { };

    for (auto newRect : adjustedRects)
        region.unite(newRect);
}

void ElementTargetingController::adjustVisibilityInRepeatedlyTargetedRegions(Document& document)
{
    if (!document.isTopDocument())
        return;

    RefPtr frameView = document.view();
    if (!frameView)
        return;

    CheckedPtr renderView = document.renderView();
    if (!renderView)
        return;

    RefPtr bodyElement = document.body();
    if (!bodyElement)
        return;

    auto previousViewportSize = std::exchange(m_viewportSizeForVisibilityAdjustment, frameView->baseLayoutViewportSize());
    if (previousViewportSize != m_viewportSizeForVisibilityAdjustment) {
        adjustRegionAfterViewportSizeChange(m_adjustmentClientRegion, previousViewportSize, m_viewportSizeForVisibilityAdjustment);
        adjustRegionAfterViewportSizeChange(m_repeatedAdjustmentClientRegion, previousViewportSize, m_viewportSizeForVisibilityAdjustment);
    }

    applyVisibilityAdjustmentFromSelectors(document);

    if (m_repeatedAdjustmentClientRegion.isEmpty())
        return;

    RefPtr onlyMainElement = findOnlyMainElement(*bodyElement);

    auto visibleDocumentRect = frameView->windowToContents(frameView->windowClipRect());
    Vector<Ref<Element>> elementsToAdjust;
    for (auto& renderer : descendantsOfType<RenderElement>(*renderView)) {
        if (!renderer.isOutOfFlowPositioned())
            continue;

        RefPtr element = renderer.element();
        if (!element)
            continue;

        if (!renderer.isVisibleInDocumentRect(visibleDocumentRect))
            continue;

        if (!m_repeatedAdjustmentClientRegion.contains(enclosingIntRect(computeClientRect(renderer))))
            continue;

        if (!isTargetCandidate(*element, onlyMainElement.get()))
            continue;

        elementsToAdjust.append(element.releaseNonNull());
    }

    for (auto& element : elementsToAdjust) {
        auto [adjustedElement, invalidateSubtree] = adjustVisibilityIfNeeded(element);
        if (!adjustedElement)
            continue;

        if (invalidateSubtree)
            adjustedElement->invalidateStyleAndRenderersForSubtree();
        else
            adjustedElement->invalidateStyle();
        m_adjustedElements.add(element);
    }
}

void ElementTargetingController::applyVisibilityAdjustmentFromSelectors(Document& document)
{
    RefPtr loader = document.loader();
    if (!loader)
        return;

    auto currentTime = ApproximateTime::now();
    if (!m_remainingVisibilityAdjustmentSelectors) {
        m_remainingVisibilityAdjustmentSelectors = loader->visibilityAdjustmentSelectors();
        m_startTimeForSelectorBasedVisibilityAdjustment = currentTime;
    }

    if (currentTime - m_startTimeForSelectorBasedVisibilityAdjustment >= selectorBasedVisibilityAdjustmentTimeLimit) {
        m_remainingVisibilityAdjustmentSelectors->clear();
        return;
    }

    if (m_remainingVisibilityAdjustmentSelectors->isEmpty())
        return;

    auto resolveSelectorToQuery = [](const String& selectorIncludingPseudo) -> std::pair<String, VisibilityAdjustment> {
        auto components = selectorIncludingPseudo.splitAllowingEmptyEntries("::"_s);
        if (components.size() == 1)
            return { components.first(), VisibilityAdjustment::Subtree };

        if (components.size() == 2) {
            auto pseudo = components.last();
            if (equalLettersIgnoringASCIICase(pseudo, "after"_s))
                return { components.first(), VisibilityAdjustment::AfterPseudo };

            if (equalLettersIgnoringASCIICase(pseudo, "before"_s))
                return { components.first(), VisibilityAdjustment::BeforePseudo };
        }

        return { { }, VisibilityAdjustment::Subtree };
    };

    auto viewportArea = m_viewportSizeForVisibilityAdjustment.area();
    Region adjustmentRegion;
    Vector<String> selectorsToRemove;
    for (auto& selectorIncludingPseudo : *m_remainingVisibilityAdjustmentSelectors) {
        auto [selector, adjustment] = resolveSelectorToQuery(selectorIncludingPseudo);
        if (selector.isEmpty()) {
            // FIXME: Handle the case where the full selector is `::after|before`.
            continue;
        }

        auto queryResult = document.querySelector(selector);
        if (queryResult.hasException())
            continue;

        RefPtr element = queryResult.releaseReturnValue();
        if (!element)
            continue;

        if (adjustment == VisibilityAdjustment::AfterPseudo && !element->afterPseudoElement())
            continue;

        if (adjustment == VisibilityAdjustment::BeforePseudo && !element->beforePseudoElement())
            continue;

        auto currentAdjustment = element->visibilityAdjustment();
        if (currentAdjustment.contains(adjustment)) {
            selectorsToRemove.append(selectorIncludingPseudo);
            continue;
        }

        element->setVisibilityAdjustment(currentAdjustment | adjustment);
        if (adjustment == VisibilityAdjustment::Subtree)
            element->invalidateStyleAndRenderersForSubtree();
        else
            element->invalidateStyle();

        m_adjustedElements.add(*element);
        selectorsToRemove.append(selectorIncludingPseudo);

        if (auto clientRect = inflatedClientRectForAdjustmentRegionTracking(*element, viewportArea))
            adjustmentRegion.unite(*clientRect);
    }

    if (!adjustmentRegion.isEmpty())
        m_adjustmentClientRegion.unite(adjustmentRegion);

    for (auto& selector : selectorsToRemove)
        m_remainingVisibilityAdjustmentSelectors->remove(selector);
}

void ElementTargetingController::reset()
{
    m_adjustmentClientRegion = { };
    m_repeatedAdjustmentClientRegion = { };
    m_viewportSizeForVisibilityAdjustment = { };
    m_adjustedElements = { };
    m_remainingVisibilityAdjustmentSelectors = { };
    m_startTimeForSelectorBasedVisibilityAdjustment = { };
    m_recentAdjustmentClientRectsCleanUpTimer.stop();
    cleanUpAdjustmentClientRects();
}

bool ElementTargetingController::resetVisibilityAdjustments(const Vector<std::pair<ElementIdentifier, ScriptExecutionContextIdentifier>>& identifiers)
{
    RefPtr page = m_page.get();
    if (!page)
        return false;

    RefPtr mainFrame = dynamicDowncast<LocalFrame>(page->mainFrame());
    if (!mainFrame)
        return false;

    RefPtr frameView = mainFrame->view();
    if (!frameView)
        return false;

    RefPtr document = mainFrame->document();
    if (!document)
        return false;

    Vector<Ref<Element>> elementsToReset;
    if (identifiers.isEmpty()) {
        elementsToReset.reserveInitialCapacity(m_adjustedElements.computeSize());
        for (auto& element : m_adjustedElements)
            elementsToReset.append(element);
        m_adjustedElements.clear();
    } else {
        elementsToReset.reserveInitialCapacity(identifiers.size());
        for (auto [elementID, documentID] : identifiers) {
            RefPtr element = Element::fromIdentifier(elementID);
            if (!element)
                continue;

            if (element->document().identifier() != documentID)
                continue;

            if (!m_adjustedElements.remove(*element))
                continue;

            elementsToReset.append(element.releaseNonNull());
        }
    }

    if (elementsToReset.isEmpty())
        return false;

    bool changed = false;
    for (auto& element : elementsToReset) {
        Ref adjustedElement = elementToAdjust(element);
        auto adjustment = adjustmentToApply(element);
        auto currentAdjustment = adjustedElement->visibilityAdjustment();
        if (!currentAdjustment.contains(adjustment))
            continue;

        adjustedElement->setVisibilityAdjustment(currentAdjustment - adjustment);
        if (adjustment == VisibilityAdjustment::Subtree)
            adjustedElement->invalidateStyleAndRenderersForSubtree();
        else
            adjustedElement->invalidateStyle();
        changed = true;
    }

    m_viewportSizeForVisibilityAdjustment = frameView->baseLayoutViewportSize();
    m_repeatedAdjustmentClientRegion = { };
    m_adjustmentClientRegion = { };

    if (changed && !m_adjustedElements.isEmptyIgnoringNullReferences()) {
        document->updateLayoutIgnorePendingStylesheets();
        auto viewportArea = m_viewportSizeForVisibilityAdjustment.area();
        for (auto& element : m_adjustedElements) {
            if (auto rect = inflatedClientRectForAdjustmentRegionTracking(element, viewportArea))
                m_adjustmentClientRegion.unite(*rect);
        }
    }

    return changed;
}

uint64_t ElementTargetingController::numberOfVisibilityAdjustmentRects() const
{
    RefPtr page = m_page.get();
    if (!page)
        return 0;

    RefPtr mainFrame = dynamicDowncast<LocalFrame>(page->mainFrame());
    if (!mainFrame)
        return 0;

    RefPtr document = mainFrame->document();
    if (!document)
        return 0;

    document->updateLayoutIgnorePendingStylesheets();

    Vector<FloatRect> clientRects;
    clientRects.reserveInitialCapacity(m_adjustedElements.computeSize());
    for (auto& element : m_adjustedElements) {
        CheckedPtr renderer = element.renderer();
        if (!renderer)
            continue;

        auto clientRect = computeClientRect(*renderer);
        if (clientRect.isEmpty())
            continue;

        clientRects.append(clientRect);
    }

    // Sort by area in descending order so that we don't double-count fully overlapped elements.
    std::sort(clientRects.begin(), clientRects.end(), [](auto first, auto second) {
        return first.area() > second.area();
    });

    Region adjustedRegion;
    uint64_t numberOfRects = 0;

    for (auto rect : clientRects) {
        auto enclosingRect = enclosingIntRect(rect);
        if (adjustedRegion.contains(enclosingRect))
            continue;

        numberOfRects++;
        adjustedRegion.unite(enclosingRect);
    }

    return numberOfRects;
}

void ElementTargetingController::cleanUpAdjustmentClientRects()
{
    m_recentAdjustmentClientRects = { };
}

} // namespace WebCore
