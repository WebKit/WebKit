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

#include "AccessibilityObject.h"
#include "Attr.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "DOMTokenList.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementChildIteratorInlines.h"
#include "ElementInlines.h"
#include "ElementRareData.h"
#include "ElementTargetingTypes.h"
#include "FloatPoint.h"
#include "FloatRect.h"
#include "HTMLBodyElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "NamedNodeMap.h"
#include "NodeList.h"
#include "Page.h"
#include "PseudoElement.h"
#include "Region.h"
#include "RenderDescendantIterator.h"
#include "RenderView.h"
#include "ShadowRoot.h"
#include "SimpleRange.h"
#include "TextExtraction.h"
#include "TextIterator.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "VisibilityAdjustment.h"

namespace WebCore {

static constexpr auto maximumNumberOfClasses = 5;
static constexpr auto marginForTrackingAdjustmentRects = 5;
static constexpr auto minimumDistanceToConsiderEdgesEquidistant = 2;
static constexpr auto minimumWidthForNearbyTarget = 2;
static constexpr auto minimumHeightForNearbyTarget = 2;
static constexpr auto minimumLengthForSearchableText = 25;
static constexpr auto maximumLengthForSearchableText = 100;
static constexpr auto selectorBasedVisibilityAdjustmentThrottlingTimeLimit = 10_s;
static constexpr auto selectorBasedVisibilityAdjustmentInterval = 1_s;
static constexpr auto maximumNumberOfAdditionalAdjustments = 20;
static constexpr auto adjustmentClientRectCleanUpDelay = 15_s;
static constexpr auto minimumAreaRatioForElementToCoverViewport = 0.95;
static constexpr auto minimumAreaForInterpolation = 200000;
static constexpr auto maximumAreaForInterpolation = 800000;

static float linearlyInterpolatedViewportRatio(float viewportArea, float minimumValue, float maximumValue)
{
    auto areaRatio = (viewportArea - minimumAreaForInterpolation) / (maximumAreaForInterpolation - minimumAreaForInterpolation);
    return clampTo(maximumValue - areaRatio * (maximumValue - minimumValue), minimumValue, maximumValue);
}

static float maximumAreaRatioForAbsolutelyPositionedContent(float viewportArea)
{
    return linearlyInterpolatedViewportRatio(viewportArea, 0.75, 1);
}

static float maximumAreaRatioForInFlowContent(float viewportArea)
{
    return linearlyInterpolatedViewportRatio(viewportArea, 0.5, 1);
}

static float maximumAreaRatioForNearbyTargets(float viewportArea)
{
    return linearlyInterpolatedViewportRatio(viewportArea, 0.25, 0.5);
}

static float minimumAreaRatioForInFlowContent(float viewportArea)
{
    return linearlyInterpolatedViewportRatio(viewportArea, 0.005, 0.01);
}

static float maximumAreaRatioForTrackingAdjustmentAreas(float viewportArea)
{
    return linearlyInterpolatedViewportRatio(viewportArea, 0.25, 0.3);
}

using ElementSelectorCache = HashMap<Ref<Element>, std::optional<String>>;

ElementTargetingController::ElementTargetingController(Page& page)
    : m_page { page }
    , m_recentAdjustmentClientRectsCleanUpTimer { *this, &ElementTargetingController::cleanUpAdjustmentClientRects, adjustmentClientRectCleanUpDelay }
    , m_selectorBasedVisibilityAdjustmentTimer { *this, &ElementTargetingController::selectorBasedVisibilityAdjustmentTimerFired }
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

static inline bool querySelectorMatchesOneElement(const Element& element, const String& selector)
{
    Ref container = [&]() -> ContainerNode& {
        if (RefPtr shadowRoot = element.containingShadowRoot())
            return *shadowRoot;
        return element.document();
    }();

    auto result = container->querySelectorAll(selector);
    if (result.hasException())
        return false;
    return result.returnValue()->length() == 1 && result.returnValue()->item(0) == &element;
}

struct ChildElementPosition {
    size_t index { notFound };
    bool firstOfType { false };
    bool lastOfType { false };
};

static inline ChildElementPosition findChild(const Element& element, const Element& parent)
{
    auto elementTagName = element.tagName();
    RefPtr<const Element> firstOfType;
    RefPtr<const Element> lastOfType;
    size_t index = notFound;
    size_t currentChildIndex = 0;
    for (auto& child : childrenOfType<Element>(parent)) {
        if (&child == &element)
            index = currentChildIndex;

        if (child.tagName() == elementTagName) {
            if (!firstOfType)
                firstOfType = &child;
            lastOfType = &child;
        }
        currentChildIndex++;
    }
    return { index, &element == firstOfType, &element == lastOfType };
}

static inline String computeIDSelector(const Element& element)
{
    if (element.hasID()) {
        auto elementID = element.getIdAttribute();
        if (auto* matches = element.treeScope().getAllElementsById(elementID); matches && matches->size() == 1)
            return makeString('#', elementID);
    }
    return emptyString();
}

static inline String computeTagAndAttributeSelector(const Element& element, const String& suffix = emptyString())
{
    if (!element.hasAttributes())
        return emptyString();

    static NeverDestroyed<MemoryCompactLookupOnlyRobinHoodHashSet<QualifiedName>> attributesToExclude { std::initializer_list<QualifiedName> {
        HTMLNames::classAttr,
        HTMLNames::idAttr,
        HTMLNames::styleAttr,
        HTMLNames::widthAttr,
        HTMLNames::heightAttr,
        HTMLNames::forAttr,
        HTMLNames::aria_labeledbyAttr,
        HTMLNames::aria_labelledbyAttr,
        HTMLNames::aria_describedbyAttr
    } };

    static constexpr auto maximumNameLength = 16;
    static constexpr auto maximumValueLength = 150;
    static constexpr auto maximumValueLengthForExactMatch = 60;

    Vector<std::pair<String, String>> attributesToCheck;
    auto& attributes = element.attributes();
    attributesToCheck.reserveInitialCapacity(attributes.length());
    for (unsigned i = 0; i < attributes.length(); ++i) {
        RefPtr attribute = attributes.item(i);
        auto qualifiedName = attribute->qualifiedName();
        if (attributesToExclude->contains(qualifiedName))
            continue;

        auto name = qualifiedName.toString();
        if (name.length() > maximumNameLength)
            continue;

        if (name.startsWith("on"_s))
            continue;

        auto value = attribute->value();
        if (value.length() > maximumValueLength)
            continue;

        attributesToCheck.append({ WTFMove(name), value.string() });
    }

    if (attributesToCheck.isEmpty())
        return emptyString();

    auto tagName = element.tagName();
    for (auto [name, value] : attributesToCheck) {
        String selector;
        if (value.length() > maximumValueLengthForExactMatch) {
            value = value.left(maximumValueLengthForExactMatch);
            selector = makeString(tagName, '[', name, "^='"_s, value, "']"_s, suffix);
        } else if (value.isEmpty())
            selector = makeString(tagName, '[', name, ']', suffix);
        else
            selector = makeString(tagName, '[', name, "='"_s, value, "']"_s, suffix);

        if (querySelectorMatchesOneElement(element, selector))
            return selector;
    }

    return emptyString();
}

static inline String computeTagAndClassSelector(Element& element)
{
    if (!element.hasClass())
        return emptyString();

    auto& classList = element.classList();
    Vector<String> classes;
    classes.reserveInitialCapacity(classList.length());
    for (unsigned i = 0; i < std::min<unsigned>(maximumNumberOfClasses, classList.length()); ++i)
        classes.append(classList.item(i));

    auto selector = makeString(element.tagName(), '.', makeStringByJoining(classes, "."_s));
    if (querySelectorMatchesOneElement(element, selector))
        return selector;

    return emptyString();
}

static String siblingRelativeSelectorRecursive(Element&, ElementSelectorCache&);
static String parentRelativeSelectorRecursive(Element&, ElementSelectorCache&);

static String shortestSelector(const Vector<String>& selectors)
{
    auto minLength = std::numeric_limits<size_t>::max();
    String shortestSelector;
    for (auto& selector : selectors) {
        if (selector.length() >= minLength)
            continue;

        minLength = selector.length();
        shortestSelector = selector;
    }
    return shortestSelector;
}

static String selectorForElementRecursive(Element& element, ElementSelectorCache& cache)
{
    if (auto selector = cache.get(element))
        return *selector;

    Vector<String> selectors;
    selectors.reserveInitialCapacity(5);
    if (auto selector = computeIDSelector(element); !selector.isEmpty())
        selectors.append(WTFMove(selector));

    if (querySelectorMatchesOneElement(element, element.tagName()))
        selectors.append(element.tagName());
    else if (auto selector = computeTagAndClassSelector(element); !selector.isEmpty())
        selectors.append(WTFMove(selector));

    if (auto selector = computeTagAndAttributeSelector(element); !selector.isEmpty())
        selectors.append(WTFMove(selector));

    if (auto selector = shortestSelector(selectors); !selector.isEmpty()) {
        cache.add(element, selector);
        return selector;
    }

    if (auto selector = parentRelativeSelectorRecursive(element, cache); !selector.isEmpty())
        selectors.append(WTFMove(selector));

    if (auto selector = siblingRelativeSelectorRecursive(element, cache); !selector.isEmpty())
        selectors.append(WTFMove(selector));

    auto selector = shortestSelector(selectors);
    cache.add(element, selector);
    return selector;
}

static String siblingRelativeSelectorRecursive(Element& element, ElementSelectorCache& cache)
{
    RefPtr<Element> siblingElement;
    for (RefPtr sibling = element.previousSibling(); sibling; sibling = sibling->previousSibling()) {
        siblingElement = dynamicDowncast<Element>(sibling);
        if (siblingElement)
            break;
    }

    if (!siblingElement)
        return emptyString();

    if (auto selector = selectorForElementRecursive(*siblingElement, cache); !selector.isEmpty())
        return makeString(WTFMove(selector), " + "_s, element.tagName());

    return emptyString();
}

static String parentRelativeSelectorRecursive(Element& element, ElementSelectorCache& cache)
{
    RefPtr parent = element.parentElement();
    if (!parent)
        return emptyString();

    if (auto selector = selectorForElementRecursive(*parent, cache); !selector.isEmpty()) {
        auto selectorPrefix = makeString(WTFMove(selector), " > "_s, element.tagName());
        auto [childIndex, firstOfType, lastOfType] = findChild(element, *parent);
        if (childIndex == notFound)
            return emptyString();

        if (firstOfType && lastOfType)
            return selectorPrefix;

        if (firstOfType)
            return makeString(WTFMove(selectorPrefix), ":first-of-type"_s);

        if (lastOfType)
            return makeString(WTFMove(selectorPrefix), ":last-of-type"_s);

        return makeString(WTFMove(selectorPrefix), ":nth-child("_s, childIndex + 1, ')');
    }

    return emptyString();
}

static String computeHasChildSelector(Element& element)
{
    static NeverDestroyed<MemoryCompactLookupOnlyRobinHoodHashSet<QualifiedName>> tagsToCheckForUniqueAttributes { std::initializer_list<QualifiedName> {
        HTMLNames::aTag,
        HTMLNames::imgTag,
        HTMLNames::timeTag,
        HTMLNames::pictureTag,
        HTMLNames::videoTag,
        HTMLNames::articleTag,
        HTMLNames::audioTag,
        HTMLNames::iframeTag,
        HTMLNames::embedTag,
        HTMLNames::sourceTag,
        HTMLNames::formTag,
        HTMLNames::inputTag,
        HTMLNames::selectTag,
        HTMLNames::buttonTag
    } };

    String selectorSuffix;
    for (auto& child : descendantsOfType<HTMLElement>(element)) {
        if (!tagsToCheckForUniqueAttributes->contains(child.tagQName()))
            continue;

        auto selector = computeTagAndAttributeSelector(child);
        if (selector.isEmpty())
            continue;

        selectorSuffix = makeString(":has("_s, WTFMove(selector), ')');
        break;
    }

    if (selectorSuffix.isEmpty())
        return emptyString();

    for (auto& ancestor : lineageOfType<HTMLElement>(element)) {
        auto selectorWithTag = makeString(ancestor.tagName(), selectorSuffix);
        if (querySelectorMatchesOneElement(element, selectorWithTag))
            return selectorWithTag;

        if (auto selector = computeTagAndAttributeSelector(ancestor, selectorSuffix); !selector.isEmpty())
            return selector;

        selectorSuffix = makeString(" > "_s, WTFMove(selectorWithTag));
    }

    return emptyString();
}

// Returns multiple CSS selectors that uniquely match the target element.
static Vector<Vector<String>> selectorsForTarget(Element& element, ElementSelectorCache& cache)
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

        auto selectors = selectorsForTarget(*host, cache);
        if (selectors.isEmpty())
            return { };

        for (auto& selector : selectors.last())
            selector = makeString(selector, pseudoSelector);

        return selectors;
    }

    Vector<Vector<String>> selectorsIncludingShadowHost;
    if (RefPtr shadowHost = element.shadowHost()) {
        selectorsIncludingShadowHost = selectorsForTarget(*shadowHost, cache);
        if (selectorsIncludingShadowHost.isEmpty())
            return { };
    }

    Vector<String> selectors;
    selectors.reserveInitialCapacity(5);

    // First, try to compute a selector using only the target element and its attributes.
    if (auto selector = computeIDSelector(element); !selector.isEmpty())
        selectors.append(WTFMove(selector));

    if (querySelectorMatchesOneElement(element, element.tagName()))
        selectors.append(element.tagName());
    else {
        if (auto selector = computeTagAndClassSelector(element); !selector.isEmpty())
            selectors.append(WTFMove(selector));

        if (auto selector = computeTagAndAttributeSelector(element); !selector.isEmpty())
            selectors.append(WTFMove(selector));
    }

    if (selectors.isEmpty()) {
        // Next, fall back to using :has(), with a child that can be uniquely identified.
        if (auto selector = computeHasChildSelector(element); !selector.isEmpty())
            selectors.append(WTFMove(selector));
    }

    if (selectors.isEmpty()) {
        // Finally, fall back on nth-child or sibling selectors.
        if (auto selector = parentRelativeSelectorRecursive(element, cache); !selector.isEmpty())
            selectors.append(WTFMove(selector));

        if (auto selector = siblingRelativeSelectorRecursive(element, cache); !selector.isEmpty())
            selectors.append(WTFMove(selector));
    }

    std::sort(selectors.begin(), selectors.end(), [](auto& first, auto& second) {
        return first.length() < second.length();
    });

    if (!selectors.isEmpty())
        cache.add(element, selectors.first());

    selectorsIncludingShadowHost.append(WTFMove(selectors));
    return selectorsIncludingShadowHost;
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

static inline Vector<FrameIdentifier> collectChildFrameIdentifiers(const Element& element)
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

static Vector<Ref<Element>> collectDocumentElementsFromChildFrames(const ContainerNode& container)
{
    Vector<Ref<Element>> documentElements;
    auto appendElement = [&](const HTMLFrameOwnerElement& owner) {
        if (RefPtr contentDocument = owner.contentDocument()) {
            if (RefPtr documentElement = contentDocument->documentElement())
                documentElements.append(documentElement.releaseNonNull());
        }
    };

    if (RefPtr containerAsFrameOwner = dynamicDowncast<HTMLFrameOwnerElement>(container))
        appendElement(*containerAsFrameOwner);

    for (auto& descendant : descendantsOfType<HTMLFrameOwnerElement>(container))
        appendElement(descendant);

    return documentElements;
}

static String searchableTextForTarget(Element& target)
{
    auto longestText = emptyString();
    size_t longestLength = 0;
    TextIterator iterator { makeRangeSelectingNodeContents(target), { TextIteratorBehavior::EmitsTextsWithoutTranscoding } };
    for (; !iterator.atEnd(); iterator.advance()) {
        auto text = iterator.copyableText().text().toString().trim(isASCIIWhitespace);
        if (text.length() <= longestLength)
            continue;

        longestLength = text.length();
        longestText = WTFMove(text);
    }

    auto documentElements = collectDocumentElementsFromChildFrames(target);
    for (auto& documentElement : documentElements) {
        if (auto text = searchableTextForTarget(documentElement); text.length() > longestLength) {
            longestLength = text.length();
            longestText = WTFMove(text);
        }
    }

    if (longestLength >= minimumLengthForSearchableText)
        return longestText.left(maximumLengthForSearchableText);

    return emptyString();
}

static bool hasAudibleMedia(const Element& element)
{
    if (RefPtr media = dynamicDowncast<HTMLMediaElement>(element))
        return media->isAudible();

    for (auto& media : descendantsOfType<HTMLMediaElement>(element)) {
        if (media.isAudible())
            return true;
    }

    for (auto& documentElement : collectDocumentElementsFromChildFrames(element)) {
        if (hasAudibleMedia(documentElement))
            return true;
    }

    return false;
}

enum class IsNearbyTarget : bool { No, Yes };
static TargetedElementInfo targetedElementInfo(Element& element, IsNearbyTarget isNearbyTarget, ElementSelectorCache& cache)
{
    CheckedPtr renderer = element.renderer();
    return {
        .elementIdentifier = element.identifier(),
        .documentIdentifier = element.document().identifier(),
        .offsetEdges = computeOffsetEdges(renderer->style()),
        .renderedText = TextExtraction::extractRenderedText(element),
        .searchableText = searchableTextForTarget(element),
        .selectors = selectorsForTarget(element, cache),
        .boundsInRootView = element.boundingBoxInRootViewCoordinates(),
        .boundsInClientCoordinates = computeClientRect(*renderer),
        .positionType = renderer->style().position(),
        .childFrameIdentifiers = collectChildFrameIdentifiers(element),
        .isNearbyTarget = isNearbyTarget == IsNearbyTarget::Yes,
        .isPseudoElement = element.isPseudoElement(),
        .isInShadowTree = element.isInShadowTree(),
        .hasAudibleMedia = hasAudibleMedia(element)
    };
}

static const HTMLElement* findOnlyMainElement(const HTMLBodyElement& bodyElement)
{
    RefPtr<const HTMLElement> onlyMainElement;
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

static bool isNavigationalElement(const Element& element)
{
    if (element.hasTagName(HTMLNames::navTag))
        return true;

    auto roleValue = element.attributeWithoutSynchronization(HTMLNames::roleAttr);
    return AccessibilityObject::ariaRoleToWebCoreRole(roleValue) == AccessibilityRole::LandmarkNavigation;
}

static bool containsNavigationalElement(const Element& element)
{
    if (isNavigationalElement(element))
        return true;

    for (auto& descendant : descendantsOfType<HTMLElement>(element)) {
        if (isNavigationalElement(descendant))
            return true;
    }

    return false;
}

static bool isTargetCandidate(Element& element, const HTMLElement* onlyMainElement, const Element* hitTestedElement = nullptr)
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

    if (is<HTMLFrameOwnerElement>(hitTestedElement) && containsNavigationalElement(element))
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

    if (clientRect.area() / viewportArea >= maximumAreaRatioForTrackingAdjustmentAreas(viewportArea))
        return { };

    // Keep track of the client rects of elements we're targeting, until the client
    // triggers visibility adjustment for these elements.
    auto inflatedClientRect = enclosingIntRect(clientRect);
    inflatedClientRect.inflate(marginForTrackingAdjustmentRects);
    return { inflatedClientRect };
}

Vector<TargetedElementInfo> ElementTargetingController::findTargets(TargetedElementRequest&& request)
{
    auto [nodes, innerElement] = std::visit(WTF::makeVisitor([this](const String& searchText) {
        return findNodes(searchText);
    }, [this, &request](const FloatPoint& point) {
        return findNodes(point, request.shouldIgnorePointerEventsNone);
    }), request.data);

    if (nodes.isEmpty())
        return { };

    return extractTargets(WTFMove(nodes), WTFMove(innerElement), request.canIncludeNearbyElements);
}

std::pair<Vector<Ref<Node>>, RefPtr<Element>> ElementTargetingController::findNodes(FloatPoint pointInRootView, bool shouldIgnorePointerEventsNone)
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

    static constexpr OptionSet defaultHitTestOptions {
        HitTestRequest::Type::ReadOnly,
        HitTestRequest::Type::DisallowUserAgentShadowContent,
        HitTestRequest::Type::CollectMultipleElements,
        HitTestRequest::Type::IncludeAllElementsUnderPoint
    };

    auto hitTestOptions = defaultHitTestOptions;
    if (shouldIgnorePointerEventsNone)
        hitTestOptions.add(HitTestRequest::Type::IgnoreCSSPointerEventsProperty);

    HitTestResult result { LayoutPoint { view->rootViewToContents(pointInRootView) } };
    document->hitTest(hitTestOptions, result);

    return { copyToVector(result.listBasedTestResult()), result.innerNonSharedElement() };
}

static Element* searchForElementContainingText(ContainerNode& container, const String& searchText)
{
    auto remainingRange = makeRangeSelectingNodeContents(container);
    while (is_lt(treeOrder(remainingRange.start, remainingRange.end))) {
        auto foundRange = findPlainText(remainingRange, searchText, {
            FindOption::DoNotRevealSelection,
            FindOption::DoNotSetSelection,
        });

        if (foundRange.collapsed())
            break;

        RefPtr target = commonInclusiveAncestor<ComposedTree>(foundRange);
        if (!target) {
            remainingRange.start = foundRange.end;
            continue;
        }

        CheckedPtr renderer = target->renderer();
        if (!renderer || renderer->style().isInVisibilityAdjustmentSubtree()) {
            remainingRange.start = foundRange.end;
            continue;
        }

        return ancestorsOfType<Element>(*target).first();
    }

    auto documentElements = collectDocumentElementsFromChildFrames(container);
    for (auto& documentElement : documentElements) {
        if (RefPtr target = searchForElementContainingText(documentElement, searchText))
            return target.get();
    }

    return nullptr;
}

std::pair<Vector<Ref<Node>>, RefPtr<Element>> ElementTargetingController::findNodes(const String& searchText)
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

    RefPtr documentElement = document->documentElement();
    if (!documentElement)
        return { };

    RefPtr foundElement = searchForElementContainingText(*documentElement, searchText);
    if (!foundElement)
        return { };

    while (!foundElement->document().isTopDocument())
        foundElement = foundElement->document().ownerElement();

    if (!foundElement) {
        ASSERT_NOT_REACHED();
        return { };
    }

    Vector<Ref<Node>> potentialCandidates;
    potentialCandidates.append(*foundElement);
    for (auto& ancestor : ancestorsOfType<Element>(*foundElement))
        potentialCandidates.append(ancestor);

    return { WTFMove(potentialCandidates), WTFMove(foundElement) };
}

static Vector<Ref<Element>> filterRedundantNearbyTargets(HashSet<Ref<Element>>&& unfilteredNearbyTargets)
{
    HashMap<Ref<Element>, bool> shouldKeepCache;
    Vector<Ref<Element>> filteredResults;

    for (auto& originalTarget : unfilteredNearbyTargets) {
        Vector<Ref<Element>> ancestorsOfTarget;
        bool shouldKeep = true;
        for (auto& ancestor : ancestorsOfType<Element>(originalTarget)) {
            if (unfilteredNearbyTargets.contains(ancestor)) {
                shouldKeep = false;
                break;
            }

            if (auto entry = shouldKeepCache.find(ancestor); entry != shouldKeepCache.end()) {
                shouldKeep = entry->value;
                break;
            }

            ancestorsOfTarget.append(ancestor);
        }

        for (auto& ancestor : ancestorsOfTarget)
            shouldKeepCache.add(ancestor, shouldKeep);

        if (shouldKeep)
            filteredResults.append(originalTarget);
    }

    return filteredResults;
}

Vector<TargetedElementInfo> ElementTargetingController::extractTargets(Vector<Ref<Node>>&& nodes, RefPtr<Element>&& innerElement, bool canIncludeNearbyElements)
{
    RefPtr page = m_page.get();
    if (!page) {
        ASSERT_NOT_REACHED();
        return { };
    }

    RefPtr mainFrame = dynamicDowncast<LocalFrame>(page->mainFrame());
    if (!mainFrame) {
        ASSERT_NOT_REACHED();
        return { };
    }

    RefPtr document = mainFrame->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return { };
    }

    RefPtr view = mainFrame->view();
    if (!view) {
        ASSERT_NOT_REACHED();
        return { };
    }

    RefPtr bodyElement = document->body();
    if (!bodyElement) {
        ASSERT_NOT_REACHED();
        return { };
    }

    FloatSize viewportSize = view->baseLayoutViewportSize();
    auto viewportArea = viewportSize.area();
    if (!viewportArea)
        return { };

    RefPtr onlyMainElement = findOnlyMainElement(*bodyElement);
    auto candidates = [&] {
        Vector<Ref<Element>> elements;
        elements.reserveInitialCapacity(nodes.size());
        for (auto& node : nodes) {
            if (RefPtr element = dynamicDowncast<Element>(node); element && isTargetCandidate(*element, onlyMainElement.get(), innerElement.get()))
                elements.append(element.releaseNonNull());
        }
        return elements;
    }();

    auto nearbyTargetAreaRatio = maximumAreaRatioForNearbyTargets(viewportArea);
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

        bool shouldSkipTargetThatCoversViewport = [&] {
            if (targetAreaRatio < minimumAreaRatioForElementToCoverViewport)
                return false;

            auto& style = targetRenderer->style();
            if (style.specifiedZIndex() < 0)
                return true;

            return targetRenderer->isOutOfFlowPositioned()
                && (!style.hasBackground() || !style.opacity())
                && style.usedPointerEvents() == PointerEvents::None;
        }();

        if (shouldSkipTargetThatCoversViewport)
            continue;

        bool shouldAddTarget = targetRenderer->isFixedPositioned()
            || targetRenderer->isStickilyPositioned()
            || (targetRenderer->isAbsolutelyPositioned() && targetAreaRatio < maximumAreaRatioForAbsolutelyPositionedContent(viewportArea))
            || (minimumAreaRatioForInFlowContent(viewportArea) < targetAreaRatio && targetAreaRatio < maximumAreaRatioForInFlowContent(viewportArea))
            || !target->firstElementChild();

        if (!shouldAddTarget)
            continue;

        bool checkForNearbyTargets = canIncludeNearbyElements
            && targetRenderer->isOutOfFlowPositioned()
            && targetAreaRatio < nearbyTargetAreaRatio;

        if (checkForNearbyTargets && computeViewportAreaRatio(targetBoundingBox) < nearbyTargetAreaRatio)
            additionalRegionForNearbyElements.unite(targetBoundingBox);

        auto targetEncompassesOtherCandidate = [](Element& target, Element& candidate) {
            if (&target == &candidate)
                return true;

            RefPtr<Element> candidateOrHost;
            if (RefPtr pseudo = dynamicDowncast<PseudoElement>(candidate))
                candidateOrHost = pseudo->hostElement();
            else
                candidateOrHost = &candidate;
            return candidateOrHost && target.containsIncludingShadowDOM(candidateOrHost.get());
        };

        candidates.removeAllMatching([&](auto& candidate) {
            if (!targetEncompassesOtherCandidate(target, candidate))
                return false;

            if (checkForNearbyTargets) {
                auto boundingBox = candidate->boundingBoxInRootViewCoordinates();
                if (computeViewportAreaRatio(boundingBox) < nearbyTargetAreaRatio)
                    additionalRegionForNearbyElements.unite(boundingBox);
            }

            return true;
        });

        targets.append(WTFMove(target));
    }

    if (targets.isEmpty())
        return { };

    m_recentAdjustmentClientRectsCleanUpTimer.restart();

    ElementSelectorCache cache;
    Vector<TargetedElementInfo> results;
    results.reserveInitialCapacity(targets.size());
    for (auto iterator = targets.rbegin(); iterator != targets.rend(); ++iterator) {
        results.append(targetedElementInfo(*iterator, IsNearbyTarget::No, cache));
        addOutOfFlowTargetClientRectIfNeeded(*iterator);
    }

    if (additionalRegionForNearbyElements.isEmpty())
        return results;

    auto nearbyTargets = [&]() -> Vector<Ref<Element>> {
        HashSet<Ref<Element>> results;
        CheckedPtr bodyRenderer = bodyElement->renderer();
        if (!bodyRenderer)
            return { };

        for (auto& renderer : descendantsOfType<RenderElement>(*bodyRenderer)) {
            if (!renderer.isOutOfFlowPositioned())
                continue;

            RefPtr element = renderer.protectedElement();
            if (!element)
                continue;

            bool elementIsAlreadyTargeted = targets.containsIf([&element](auto& target) {
                return target->containsIncludingShadowDOM(element.get());
            });

            if (elementIsAlreadyTargeted)
                continue;

            if (results.contains(*element))
                continue;

            if (nodes.containsIf([&](auto& node) { return node.ptr() == element; }))
                continue;

            if (!isTargetCandidate(*element, onlyMainElement.get(), innerElement.get()))
                continue;

            auto boundingBox = element->boundingBoxInRootViewCoordinates();
            if (boundingBox.width() <= minimumWidthForNearbyTarget)
                continue;

            if (boundingBox.height() <= minimumHeightForNearbyTarget)
                continue;

            if (!additionalRegionForNearbyElements.contains(boundingBox))
                continue;

            if (computeViewportAreaRatio(boundingBox) > nearbyTargetAreaRatio)
                continue;

            results.add(element.releaseNonNull());
        }

        return filterRedundantNearbyTargets(WTFMove(results));
    }();

    for (auto& element : nearbyTargets) {
        results.append(targetedElementInfo(element, IsNearbyTarget::Yes, cache));
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

bool ElementTargetingController::adjustVisibility(Vector<TargetedElementAdjustment>&& adjustments)
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
    for (auto& [identifiers, selectors] : adjustments) {
        auto [elementID, documentID] = identifiers;
        if (auto rect = m_recentAdjustmentClientRects.get(elementID); !rect.isEmpty())
            newAdjustmentRegion.unite(rect);
    }

    m_repeatedAdjustmentClientRegion.unite(intersect(m_adjustmentClientRegion, newAdjustmentRegion));
    m_adjustmentClientRegion.unite(newAdjustmentRegion);

    Vector<Ref<Element>> elements;
    elements.reserveInitialCapacity(adjustments.size());
    for (auto& [identifiers, selectors] : adjustments) {
        auto [elementID, documentID] = identifiers;
        RefPtr element = Element::fromIdentifier(elementID);
        if (!element)
            continue;

        if (element->document().identifier() != documentID)
            continue;

        elements.append(element.releaseNonNull());
        if (m_additionalAdjustmentCount < maximumNumberOfAdditionalAdjustments) {
            m_visibilityAdjustmentSelectors.append({ elementID, WTFMove(selectors) });
            m_additionalAdjustmentCount++;
        }
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

    if (changed)
        dispatchVisibilityAdjustmentStateDidChange();

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

    if (RefPtr loader = document.loader(); loader && !m_didCollectInitialAdjustments) {
        m_visibilityAdjustmentSelectors.appendVector(loader->visibilityAdjustmentSelectors().map([](auto& selectors) -> std::pair<ElementIdentifier, TargetedElementSelectors> {
            return { { }, selectors };
        }));
        m_startTimeForSelectorBasedVisibilityAdjustment = ApproximateTime::now();
        m_didCollectInitialAdjustments = true;
    }

    if (!m_visibilityAdjustmentSelectors.isEmpty()) {
        if (ApproximateTime::now() - m_startTimeForSelectorBasedVisibilityAdjustment <= selectorBasedVisibilityAdjustmentThrottlingTimeLimit)
            applyVisibilityAdjustmentFromSelectors(document);
        else if (!m_selectorBasedVisibilityAdjustmentTimer.isActive())
            m_selectorBasedVisibilityAdjustmentTimer.startOneShot(selectorBasedVisibilityAdjustmentInterval);
    }

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

    if (elementsToAdjust.isEmpty())
        return;

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

    dispatchVisibilityAdjustmentStateDidChange();
}

void ElementTargetingController::applyVisibilityAdjustmentFromSelectors(Document& document)
{
    if (m_visibilityAdjustmentSelectors.isEmpty())
        return;

    RefPtr page = m_page.get();
    if (!page)
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

    document.updateLayoutIgnorePendingStylesheets();

    auto viewportArea = m_viewportSizeForVisibilityAdjustment.area();
    Region adjustmentRegion;
    Vector<String> matchingSelectors;
    for (auto& [identifier, selectorsForElementIncludingShadowHosts] : m_visibilityAdjustmentSelectors) {
        if (selectorsForElementIncludingShadowHosts.isEmpty())
            continue;

        Ref<ContainerNode> containerToQuery = document;
        size_t indexOfSelectorToQuery = 0;
        for (auto& selectorsToQuery : selectorsForElementIncludingShadowHosts) {
            bool isLastTarget = ++indexOfSelectorToQuery == selectorsForElementIncludingShadowHosts.size();
            RefPtr<Element> currentTarget;
            for (auto& selectorIncludingPseudo : selectorsToQuery) {
                auto [selector, adjustment] = resolveSelectorToQuery(selectorIncludingPseudo);
                if (selector.isEmpty()) {
                    // FIXME: Handle the case where the full selector is `::after|before`.
                    continue;
                }

                auto queryResult = containerToQuery->querySelector(selector);
                if (queryResult.hasException())
                    continue;

                RefPtr element = queryResult.releaseReturnValue();
                if (!element)
                    continue;

                CheckedPtr renderer = element->renderer();
                if (!renderer)
                    continue;

                if (adjustment == VisibilityAdjustment::AfterPseudo && !element->afterPseudoElement())
                    continue;

                if (adjustment == VisibilityAdjustment::BeforePseudo && !element->beforePseudoElement())
                    continue;

                if (isLastTarget) {
                    if (computeClientRect(*renderer).isEmpty())
                        continue;

                    auto currentAdjustment = element->visibilityAdjustment();
                    if (!currentAdjustment.contains(adjustment)) {
                        element->setVisibilityAdjustment(currentAdjustment | adjustment);

                        if (adjustment == VisibilityAdjustment::Subtree)
                            element->invalidateStyleAndRenderersForSubtree();
                        else
                            element->invalidateStyle();

                        m_adjustedElements.add(*element);

                        if (auto clientRect = inflatedClientRectForAdjustmentRegionTracking(*element, viewportArea))
                            adjustmentRegion.unite(*clientRect);

                        matchingSelectors.append(selectorIncludingPseudo);
                    }
                }

                currentTarget = WTFMove(element);
                break;
            }

            if (!currentTarget) {
                // We failed to resolve the targeted element, or one of its shadow hosts.
                break;
            }

            if (isLastTarget) {
                // We resolved the final targeted element.
                break;
            }

            RefPtr nextShadowRoot = currentTarget->shadowRoot();
            if (!nextShadowRoot)
                break;

            // Continue the search underneath the next shadow root.
            containerToQuery = nextShadowRoot.releaseNonNull();
        }
    }

    if (!adjustmentRegion.isEmpty())
        m_adjustmentClientRegion.unite(adjustmentRegion);

    if (matchingSelectors.isEmpty())
        return;

    dispatchVisibilityAdjustmentStateDidChange();
    page->chrome().client().didAdjustVisibilityWithSelectors(WTFMove(matchingSelectors));
}

void ElementTargetingController::reset()
{
    m_adjustmentClientRegion = { };
    m_repeatedAdjustmentClientRegion = { };
    m_viewportSizeForVisibilityAdjustment = { };
    m_adjustedElements = { };
    m_visibilityAdjustmentSelectors = { };
    m_didCollectInitialAdjustments = false;
    m_additionalAdjustmentCount = 0;
    m_selectorBasedVisibilityAdjustmentTimer.stop();
    m_startTimeForSelectorBasedVisibilityAdjustment = { };
    m_recentAdjustmentClientRectsCleanUpTimer.stop();
    cleanUpAdjustmentClientRects();
}

bool ElementTargetingController::resetVisibilityAdjustments(const Vector<TargetedElementIdentifiers>& identifiers)
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

    if (RefPtr loader = document->loader(); loader && !identifiers.isEmpty()) {
        m_visibilityAdjustmentSelectors = loader->visibilityAdjustmentSelectors().map([](auto& selectors) -> std::pair<ElementIdentifier, TargetedElementSelectors> {
            return { { }, selectors };
        });
    } else {
        // There are no initial adjustments after resetting.
        m_visibilityAdjustmentSelectors = { };
    }
    m_additionalAdjustmentCount = 0;
    m_didCollectInitialAdjustments = true;

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

    if (changed)
        dispatchVisibilityAdjustmentStateDidChange();

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

void ElementTargetingController::dispatchVisibilityAdjustmentStateDidChange()
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    page->forEachDocument([](auto& document) {
        document.visibilityAdjustmentStateDidChange();
    });
}

void ElementTargetingController::selectorBasedVisibilityAdjustmentTimerFired()
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    RefPtr mainFrame = dynamicDowncast<LocalFrame>(page->mainFrame());
    if (!mainFrame)
        return;

    RefPtr document = mainFrame->document();
    if (!document)
        return;

    applyVisibilityAdjustmentFromSelectors(*document);
}

} // namespace WebCore
