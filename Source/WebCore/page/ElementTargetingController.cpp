/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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
#include "BitmapImage.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ContainerNodeInlines.h"
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
#include "FrameDestructionObserverInlines.h"
#include "FrameSnapshotting.h"
#include "HTMLAnchorElement.h"
#include "HTMLBodyElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLImageElement.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "LocalFrameInlines.h"
#include "LocalFrameView.h"
#include "NamedNodeMap.h"
#include "NodeInlines.h"
#include "NodeList.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "PseudoElement.h"
#include "Region.h"
#include "RenderBoxInlines.h"
#include "RenderDescendantIterator.h"
#include "RenderView.h"
#include "ShadowRoot.h"
#include "SimpleRange.h"
#include "StyleImage.h"
#include "StyleURL.h"
#include "TextExtraction.h"
#include "TextIterator.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "VisibilityAdjustment.h"
#include <ranges>
#include <wtf/HashMap.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ElementTargetingController);

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

class ClearVisibilityAdjustmentForScope {
    WTF_MAKE_NONCOPYABLE(ClearVisibilityAdjustmentForScope);
    WTF_MAKE_TZONE_ALLOCATED(ClearVisibilityAdjustmentForScope);
public:
    ClearVisibilityAdjustmentForScope(Element& element)
        : m_element(element)
        , m_adjustmentToRestore(element.visibilityAdjustment())
    {
        if (m_adjustmentToRestore.isEmpty())
            return;

        element.setVisibilityAdjustment({ });
        element.invalidateStyleAndRenderersForSubtree();
    }

    ClearVisibilityAdjustmentForScope(ClearVisibilityAdjustmentForScope&& other)
        : m_element(WTF::move(other.m_element))
        , m_adjustmentToRestore(std::exchange(other.m_adjustmentToRestore, { }))
    {
    }

    ~ClearVisibilityAdjustmentForScope()
    {
        if (m_adjustmentToRestore.isEmpty())
            return;

        m_element->setVisibilityAdjustment(m_adjustmentToRestore);
        m_element->invalidateStyleAndRenderersForSubtree();
    }

private:
    Ref<Element> m_element;
    OptionSet<VisibilityAdjustment> m_adjustmentToRestore;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(ClearVisibilityAdjustmentForScope);

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
        if (ancestor.style().usedVisibility() == Visibility::Hidden)
            continue;

        unsigned numberOfVisibleChildren = 0;
        for (auto& child : childrenOfType<RenderObject>(ancestor)) {
            if (CheckedPtr renderElement = dynamicDowncast<RenderElement>(child); renderElement && renderElement->style().usedVisibility() == Visibility::Hidden)
                continue;

            if (++numberOfVisibleChildren >= 2)
                return false;
        }
    }
    return true;
}

static inline bool querySelectorMatchesOneElement(const Element& element, const String& selector)
{
    Ref container = [&]() -> Ref<ContainerNode> {
        if (RefPtr shadowRoot = element.containingShadowRoot())
            return shadowRoot.releaseNonNull();
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
    for (Ref child : childrenOfType<Element>(parent)) {
        if (child.ptr() == &element)
            index = currentChildIndex;

        if (child->tagName() == elementTagName) {
            if (!firstOfType)
                firstOfType = child.ptr();
            lastOfType = child.ptr();
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
    Ref attributes = element.attributesMap();
    attributesToCheck.reserveInitialCapacity(attributes->length());
    for (unsigned i = 0; i < attributes->length(); ++i) {
        RefPtr attribute = attributes->item(i);
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

        attributesToCheck.append({ WTF::move(name), value.string() });
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

    Ref classList = element.classList();
    Vector<String> classes;
    classes.reserveInitialCapacity(classList->length());
    for (unsigned i = 0; i < std::min<unsigned>(maximumNumberOfClasses, classList->length()); ++i)
        classes.append(classList->item(i));

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
        selectors.append(WTF::move(selector));

    if (querySelectorMatchesOneElement(element, element.tagName()))
        selectors.append(element.tagName());
    else if (auto selector = computeTagAndClassSelector(element); !selector.isEmpty())
        selectors.append(WTF::move(selector));

    if (auto selector = computeTagAndAttributeSelector(element); !selector.isEmpty())
        selectors.append(WTF::move(selector));

    if (auto selector = shortestSelector(selectors); !selector.isEmpty()) {
        cache.add(element, selector);
        return selector;
    }

    if (auto selector = parentRelativeSelectorRecursive(element, cache); !selector.isEmpty())
        selectors.append(WTF::move(selector));

    if (auto selector = siblingRelativeSelectorRecursive(element, cache); !selector.isEmpty())
        selectors.append(WTF::move(selector));

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
        return makeString(WTF::move(selector), " + "_s, element.tagName());

    return emptyString();
}

static String parentRelativeSelectorRecursive(Element& element, ElementSelectorCache& cache)
{
    RefPtr parent = element.parentElement();
    if (!parent)
        return emptyString();

    if (auto selector = selectorForElementRecursive(*parent, cache); !selector.isEmpty()) {
        auto selectorPrefix = makeString(WTF::move(selector), " > "_s, element.tagName());
        auto [childIndex, firstOfType, lastOfType] = findChild(element, *parent);
        if (childIndex == notFound)
            return emptyString();

        if (firstOfType && lastOfType)
            return selectorPrefix;

        if (firstOfType)
            return makeString(WTF::move(selectorPrefix), ":first-of-type"_s);

        if (lastOfType)
            return makeString(WTF::move(selectorPrefix), ":last-of-type"_s);

        return makeString(WTF::move(selectorPrefix), ":nth-child("_s, childIndex + 1, ')');
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
    for (Ref child : descendantsOfType<HTMLElement>(element)) {
        if (!tagsToCheckForUniqueAttributes->contains(child->tagQName()))
            continue;

        auto selector = computeTagAndAttributeSelector(child);
        if (selector.isEmpty())
            continue;

        selectorSuffix = makeString(":has("_s, WTF::move(selector), ')');
        break;
    }

    if (selectorSuffix.isEmpty())
        return emptyString();

    for (Ref ancestor : lineageOfType<HTMLElement>(element)) {
        auto selectorWithTag = makeString(ancestor->tagName(), selectorSuffix);
        if (querySelectorMatchesOneElement(element, selectorWithTag))
            return selectorWithTag;

        if (auto selector = computeTagAndAttributeSelector(ancestor, selectorSuffix); !selector.isEmpty())
            return selector;

        selectorSuffix = makeString(" > "_s, WTF::move(selectorWithTag));
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
        selectors.append(WTF::move(selector));

    if (querySelectorMatchesOneElement(element, element.tagName()))
        selectors.append(element.tagName());
    else {
        if (auto selector = computeTagAndClassSelector(element); !selector.isEmpty())
            selectors.append(WTF::move(selector));

        if (auto selector = computeTagAndAttributeSelector(element); !selector.isEmpty())
            selectors.append(WTF::move(selector));
    }

    if (selectors.isEmpty()) {
        // Next, fall back to using :has(), with a child that can be uniquely identified.
        if (auto selector = computeHasChildSelector(element); !selector.isEmpty())
            selectors.append(WTF::move(selector));
    }

    if (selectors.isEmpty()) {
        // Finally, fall back on nth-child or sibling selectors.
        if (auto selector = parentRelativeSelectorRecursive(element, cache); !selector.isEmpty())
            selectors.append(WTF::move(selector));

        if (auto selector = siblingRelativeSelectorRecursive(element, cache); !selector.isEmpty())
            selectors.append(WTF::move(selector));
    }

    std::ranges::sort(selectors, { }, &String::length);

    if (!selectors.isEmpty())
        cache.add(element, selectors.first());

    selectorsIncludingShadowHost.append(WTF::move(selectors));
    return selectorsIncludingShadowHost;
}

RefPtr<Element> ElementTargetingController::elementFromSelectors(Document& document, const TargetedElementSelectors& selectors)
{
    return findElementFromSelectors(document, selectors).element;
}

TargetedElementSelectors ElementTargetingController::selectorsForElement(Element& element)
{
    ElementSelectorCache cache;
    return WTF::map(selectorsForTarget(element, cache), [](auto&& selectors) {
        return HashSet<String> { WTF::move(selectors) };
    });
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
    for (Ref owner : descendantsOfType<HTMLFrameOwnerElement>(element)) {
        if (RefPtr frame = owner->contentFrame())
            identifiers.append(frame->frameID());
    }
    return identifiers;
}

static FloatRect computeClientRect(const RenderElement& renderer)
{
    auto rect = FloatRect { renderer.absoluteBoundingBoxRect() };
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

    for (Ref descendant : descendantsOfType<HTMLFrameOwnerElement>(container))
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
        longestText = WTF::move(text);
    }

    auto documentElements = collectDocumentElementsFromChildFrames(target);
    for (auto& documentElement : documentElements) {
        if (auto text = searchableTextForTarget(documentElement); text.length() > longestLength) {
            longestLength = text.length();
            longestText = WTF::move(text);
        }
    }

    if (longestLength >= minimumLengthForSearchableText)
        return longestText.left(maximumLengthForSearchableText);

    return emptyString();
}

static bool hasAudibleMedia(const Element& element)
{
#if ENABLE(VIDEO)
    if (RefPtr media = dynamicDowncast<HTMLMediaElement>(element))
        return media->isAudible();

    for (Ref media : descendantsOfType<HTMLMediaElement>(element)) {
        if (media->isAudible())
            return true;
    }

    for (auto& documentElement : collectDocumentElementsFromChildFrames(element)) {
        if (hasAudibleMedia(documentElement))
            return true;
    }
#else
    UNUSED_PARAM(element);
#endif

    return false;
}

static URL urlForElement(const Element& element)
{
    if (RefPtr anchor = dynamicDowncast<HTMLAnchorElement>(element))
        return anchor->href();

    if (RefPtr image = dynamicDowncast<HTMLImageElement>(element))
        return image->currentURL();

#if ENABLE(VIDEO)
    if (RefPtr media = dynamicDowncast<HTMLMediaElement>(element))
        return media->currentSrc();
#endif

    if (CheckedPtr renderer = element.renderer()) {
        if (auto& style = renderer->style(); style.hasBackgroundImage()) {
            if (RefPtr image = style.backgroundLayers().usedFirst().image().tryStyleImage())
                return image->url().resolved;
        }
    }

    return { };
}

static void collectMediaAndLinkURLsRecursive(const Element& element, HashSet<URL>& urls)
{
    auto addURLForElement = [&urls](const Element& element) {
        if (auto url = urlForElement(element); !url.isEmpty() && !url.protocolIsData() && !url.protocolIsBlob())
            urls.add(WTF::move(url));
    };

    addURLForElement(element);

    for (Ref descendant : descendantsOfType<Element>(element)) {
        addURLForElement(descendant);

        RefPtr frameOwner = dynamicDowncast<HTMLFrameOwnerElement>(descendant.ptr());
        if (!frameOwner)
            continue;

        RefPtr contentDocument = frameOwner->contentDocument();
        if (!contentDocument)
            continue;

        RefPtr documentElement = contentDocument->documentElement();
        if (!documentElement)
            continue;

        collectMediaAndLinkURLsRecursive(*documentElement, urls);
    }
}

static HashSet<URL> collectMediaAndLinkURLs(const Element& element)
{
    HashSet<URL> urls;
    collectMediaAndLinkURLsRecursive(element, urls);
    return urls;
}

enum class IsNearbyTarget : bool { No, Yes };
static std::optional<TargetedElementInfo> targetedElementInfo(Element& element, IsNearbyTarget isNearbyTarget, ElementSelectorCache& cache, const WeakHashSet<Element, WeakPtrImplWithEventTargetData>& adjustedElements)
{
    protect(element.document())->updateLayoutIgnorePendingStylesheets();

    FloatRect boundsInClientCoordinates;
    RectEdges<bool> offsetEdges;
    PositionType positionType = PositionType::Static;
    {
        WeakPtr renderer = element.renderer();
        if (!renderer)
            return { };

        offsetEdges = computeOffsetEdges(renderer->style());
        positionType = renderer->style().position();
        boundsInClientCoordinates = computeClientRect(*renderer);
    }

    bool isInVisibilityAdjustmentSubtree = [&] {
        for (RefPtr ancestor = element; ancestor; ancestor = ancestor->parentElementInComposedTree()) {
            if (adjustedElements.contains(*ancestor))
                return true;
        }
        return false;
    }();

    auto [renderedText, screenReaderText, hasLargeReplacedDescendant] = TextExtraction::extractRenderedText(element);
    return { {
        .nodeIdentifier = element.nodeIdentifier(),
        .documentIdentifier = element.document().identifier(),
        .offsetEdges = offsetEdges,
        .renderedText = WTF::move(renderedText),
        .searchableText = searchableTextForTarget(element),
        .screenReaderText = WTF::move(screenReaderText),
        .selectors = selectorsForTarget(element, cache),
        .boundsInRootView = element.boundingBoxInRootViewCoordinates(),
        .boundsInClientCoordinates = WTF::move(boundsInClientCoordinates),
        .positionType = positionType,
        .childFrameIdentifiers = collectChildFrameIdentifiers(element),
        .mediaAndLinkURLs = collectMediaAndLinkURLs(element),
        .isNearbyTarget = isNearbyTarget == IsNearbyTarget::Yes,
        .isPseudoElement = element.isPseudoElement(),
        .isInShadowTree = element.isInShadowTree(),
        .isInVisibilityAdjustmentSubtree = isInVisibilityAdjustmentSubtree,
        .hasLargeReplacedDescendant = hasLargeReplacedDescendant,
        .hasAudibleMedia = hasAudibleMedia(element)
    } };
}

static RefPtr<const HTMLElement> findOnlyMainElement(const HTMLBodyElement& bodyElement)
{
    RefPtr<const HTMLElement> onlyMainElement;
    for (Ref descendant : descendantsOfType<HTMLElement>(bodyElement)) {
        if (!descendant->hasTagName(HTMLNames::mainTag))
            continue;

        if (onlyMainElement) {
            onlyMainElement = nullptr;
            break;
        }

        onlyMainElement = descendant.ptr();
    }
    return onlyMainElement;
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

    for (Ref descendant : descendantsOfType<HTMLElement>(element)) {
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
    CheckedPtr renderer = element.renderer();
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

static bool shouldIgnoreExistingVisibilityAdjustments(const TargetedElementRequest& request)
{
    return std::holds_alternative<String>(request.data) || std::holds_alternative<TargetedElementSelectors>(request.data);
}

Vector<TargetedElementInfo> ElementTargetingController::findTargets(TargetedElementRequest&& request)
{
    Vector<ClearVisibilityAdjustmentForScope> clearVisibilityAdjustmentScopes;
    if (shouldIgnoreExistingVisibilityAdjustments(request) && m_adjustedElements.computeSize()) {
        for (Ref element : m_adjustedElements)
            clearVisibilityAdjustmentScopes.append({ element });

        if (RefPtr document = mainDocument())
            document->updateLayoutIgnorePendingStylesheets();
    }

    auto checkViewportAreaRatio = CheckViewportAreaRatio::Yes;
    auto [nodes, innerElement] = switchOn(request.data, [this](const String& searchText) {
        return findNodes(searchText);
    }, [this, &request](const FloatPoint& point) {
        return findNodes(point, request.shouldIgnorePointerEventsNone);
    }, [this, &checkViewportAreaRatio](const TargetedElementSelectors& selectors) {
        checkViewportAreaRatio = CheckViewportAreaRatio::No;
        return findNodes(selectors);
    });

    if (nodes.isEmpty())
        return { };

    auto includeNearbyElements = request.canIncludeNearbyElements ? IncludeNearbyElements::Yes : IncludeNearbyElements::No;
    return extractTargets(WTF::move(nodes), WTF::move(innerElement), checkViewportAreaRatio, includeNearbyElements);
}

void ElementTargetingController::topologicallySortElementsHelper(NodeIdentifier currentElementID, Vector<NodeIdentifier>& depthSortedIDs, HashSet<NodeIdentifier>& processingIDs, HashSet<NodeIdentifier>& unprocessedIDs, const HashMap<NodeIdentifier, HashSet<NodeIdentifier>>& nodeIDToOccludedElementIDs)
{
    if (processingIDs.contains(currentElementID)) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (!unprocessedIDs.contains(currentElementID))
        return;

    unprocessedIDs.remove(currentElementID);
    processingIDs.add(currentElementID);

    for (auto& occludedElementID : nodeIDToOccludedElementIDs.get(currentElementID))
        topologicallySortElementsHelper(occludedElementID, depthSortedIDs, processingIDs, unprocessedIDs, nodeIDToOccludedElementIDs);

    processingIDs.remove(currentElementID);
    depthSortedIDs.append(currentElementID);
}

Vector<NodeIdentifier> ElementTargetingController::topologicallySortElements(const HashMap<NodeIdentifier, HashSet<NodeIdentifier>>& nodeIDToOccludedElementIDs)
{
    Vector<NodeIdentifier> depthSortedIDs;
    HashSet<NodeIdentifier> processingIDs;
    HashSet<NodeIdentifier> unprocessedIDs;

    unprocessedIDs.addAll(nodeIDToOccludedElementIDs.keys());

    while (!unprocessedIDs.isEmpty() || !processingIDs.isEmpty()) {
        if (unprocessedIDs.isEmpty()) {
            ASSERT_NOT_REACHED();
            break;
        }

        topologicallySortElementsHelper(*unprocessedIDs.begin(), depthSortedIDs, processingIDs, unprocessedIDs, nodeIDToOccludedElementIDs);
    }

    depthSortedIDs.reverse();
    return depthSortedIDs;
}

Vector<Vector<TargetedElementInfo>> ElementTargetingController::findAllTargets(float hitTestInterval)
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

    RefPtr view = mainFrame->view();
    if (!view) {
        ASSERT_NOT_REACHED();
        return { };
    }

    const auto viewportRect = view->unobscuredContentRect();
    const auto halfHitTestInterval = std::floor(hitTestInterval / 2);

    Vector<Vector<TargetedElementInfo>> targetsList;
    for (auto x = viewportRect.x() + halfHitTestInterval; x < viewportRect.maxX(); x += hitTestInterval) {
        for (auto y = viewportRect.y() + halfHitTestInterval; y < viewportRect.maxY(); y += hitTestInterval) {
            auto [nodes, innerElement] = findNodes({ x, y }, true);
            if (nodes.isEmpty())
                continue;

            targetsList.append(extractTargets(WTF::move(nodes), WTF::move(innerElement), CheckViewportAreaRatio::Yes, IncludeNearbyElements::No));
        }
    }

    HashMap<NodeIdentifier, HashSet<NodeIdentifier>> nodeIDToOccludedElementIDs;
    HashMap<NodeIdentifier, Vector<TargetedElementInfo>> nodeIDToTargets;
    for (auto& targets : targetsList) {
        if (targets.isEmpty())
            continue;

        const auto topElementID = targets.first().nodeIdentifier;
        HashSet<NodeIdentifier> occludedElementIDsToInsert;
        for (unsigned index = 1; index < targets.size(); ++index)
            occludedElementIDsToInsert.add(targets[index].nodeIdentifier);

        auto storedTargets = nodeIDToTargets.getOptional(topElementID);
        auto storedIDsSet = nodeIDToOccludedElementIDs.getOptional(topElementID);
        if (storedTargets && storedIDsSet) {
            for (auto& target : targets) {
                if (target.nodeIdentifier != topElementID && !storedIDsSet->contains(target.nodeIdentifier))
                    storedTargets->append(target);
            }

            nodeIDToTargets.set(topElementID, *storedTargets);
            nodeIDToOccludedElementIDs.set(topElementID, storedIDsSet->unionWith(occludedElementIDsToInsert));
        } else {
            nodeIDToTargets.set(topElementID, targets);
            nodeIDToOccludedElementIDs.set(topElementID, occludedElementIDsToInsert);
        }
    }

    return topologicallySortElements(nodeIDToOccludedElementIDs).map([& nodeIDToTargets](const auto& nodeID) {
        return nodeIDToTargets.get(nodeID);
    });
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

static RefPtr<Element> searchForElementContainingText(ContainerNode& container, const String& searchText)
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
        if (!renderer || renderer->style().isForceHidden()) {
            remainingRange.start = foundRange.end;
            continue;
        }

        return ancestorsOfType<Element>(*target).first();
    }

    auto documentElements = collectDocumentElementsFromChildFrames(container);
    for (auto& documentElement : documentElements) {
        if (RefPtr target = searchForElementContainingText(documentElement, searchText))
            return target;
    }

    return nullptr;
}

std::pair<Vector<Ref<Node>>, RefPtr<Element>> ElementTargetingController::findNodes(const String& searchText)
{
    RefPtr document = mainDocument();
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
    for (Ref ancestor : ancestorsOfType<Element>(*foundElement))
        potentialCandidates.append(ancestor);
    return { WTF::move(potentialCandidates), WTF::move(foundElement) };
}

std::pair<Vector<Ref<Node>>, RefPtr<Element>> ElementTargetingController::findNodes(const TargetedElementSelectors& selectors)
{
    auto [foundElement, selectorIncludingPseudo] = findElementFromSelectors(selectors);
    if (!foundElement)
        return { };

    return { { *foundElement }, foundElement };
}

static Vector<Ref<Element>> filterRedundantNearbyTargets(HashSet<Ref<Element>>&& unfilteredNearbyTargets)
{
    HashMap<Ref<Element>, bool> shouldKeepCache;
    Vector<Ref<Element>> filteredResults;

    for (auto& originalTarget : unfilteredNearbyTargets) {
        Vector<Ref<Element>> ancestorsOfTarget;
        bool shouldKeep = true;
        for (Ref ancestor : ancestorsOfType<Element>(originalTarget)) {
            if (unfilteredNearbyTargets.contains(ancestor)) {
                shouldKeep = false;
                break;
            }

            if (auto entry = shouldKeepCache.find(ancestor.ptr()); entry != shouldKeepCache.end()) {
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

static IntRect absoluteBoundsForTargetAreaRatio(const Element& element, WeakHashMap<const Element, IntRect, WeakPtrImplWithEventTargetData>& cache)
{
    auto absoluteBoundingBoxRect = [&cache](const Element& element) {
        auto entry = cache.find(element);
        if (entry != cache.end())
            return entry->value;

        CheckedPtr renderer = element.renderer();
        if (!renderer)
            return IntRect { };

        auto bounds = renderer->absoluteBoundingBoxRect();
        cache.set(element, bounds);
        return bounds;
    };

    auto bounds = absoluteBoundingBoxRect(element);
    bool hasVisualOverflowX = false;
    bool hasVisualOverflowY = false;
    if (CheckedPtr style = element.renderStyle()) {
        hasVisualOverflowX = style->overflowX() == Overflow::Visible;
        hasVisualOverflowY = style->overflowY() == Overflow::Visible;
    }

    if (!hasVisualOverflowX && !hasVisualOverflowY)
        return bounds;

    IntRect absoluteBoundsOfChildren;
    for (Ref child : childrenOfType<Element>(element))
        absoluteBoundsOfChildren.uniteIfNonZero(absoluteBoundingBoxRect(child));

    if (absoluteBoundsOfChildren.isEmpty())
        return bounds;

    auto boundsIncludingChildren = unionRect(bounds, absoluteBoundsOfChildren);
    if (hasVisualOverflowX) {
        bounds.shiftXEdgeTo(boundsIncludingChildren.x());
        bounds.shiftMaxXEdgeTo(boundsIncludingChildren.maxX());
    }

    if (hasVisualOverflowY) {
        bounds.shiftYEdgeTo(boundsIncludingChildren.y());
        bounds.shiftMaxYEdgeTo(boundsIncludingChildren.maxY());
    }

    return bounds;
}

Vector<TargetedElementInfo> ElementTargetingController::extractTargets(Vector<Ref<Node>>&& nodes, RefPtr<Element>&& innerElement, CheckViewportAreaRatio checkViewportAreaRatio, IncludeNearbyElements includeNearbyElements)
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
            m_recentAdjustmentClientRects.set(element.nodeIdentifier(), *rect);
    };

    auto computeViewportAreaRatio = [&](IntRect boundingBox) {
        auto area = boundingBox.area<RecordOverflow>();
        return area.hasOverflowed() ? std::numeric_limits<float>::max() : area.value() / viewportArea;
    };

    Vector<Ref<Element>> targets; // The front-most target is last in this list.
    Region additionalRegionForNearbyElements;
    WeakHashMap<const Element, IntRect, WeakPtrImplWithEventTargetData> absoluteBoundsCache;

    // Prioritize parent elements over their children by traversing backwards over the candidates.
    // This allows us to target only the top-most container elements that satisfy the criteria.
    // While adding targets, we also accumulate additional regions, wherein we should report any
    // nearby targets.
    while (!candidates.isEmpty()) {
        Ref target = candidates.takeLast();
        CheckedPtr targetRenderer = target->renderer();
        auto targetBoundingBox = view->contentsToRootView(absoluteBoundsForTargetAreaRatio(target, absoluteBoundsCache));
        auto targetAreaRatio = computeViewportAreaRatio(targetBoundingBox);

        auto hasOneRenderedChild = [](const Element& target) {
            CheckedPtr renderer = target.renderer();
            if (!renderer)
                return false;

            CheckedPtr firstChild = renderer->firstChild();
            return firstChild && firstChild == renderer->lastChild();
        };

        bool shouldSkipIrrelevantTarget = [&] {
            if (targetAreaRatio < minimumAreaRatioForElementToCoverViewport && !hasOneRenderedChild(target))
                return false;

            auto& style = targetRenderer->style();
            if (auto specifiedZIndexValue = style.specifiedZIndex().tryValue(); specifiedZIndexValue && *specifiedZIndexValue < 0)
                return true;

            return targetRenderer->isOutOfFlowPositioned()
                && (!style.hasBackground() || style.opacity().isTransparent())
                && targetRenderer->usedPointerEvents() == PointerEvents::None;
        }();

        if (shouldSkipIrrelevantTarget)
            continue;

        bool shouldAddTarget = [&] {
            if (targetAreaRatio <= 0)
                return false;

            if (targetRenderer->isFixedPositioned())
                return true;

            if (targetRenderer->isStickilyPositioned())
                return true;

            if (!target->firstElementChild())
                return true;

            return checkViewportAreaRatio == CheckViewportAreaRatio::No
                || (targetRenderer->isAbsolutelyPositioned() && targetAreaRatio < maximumAreaRatioForAbsolutelyPositionedContent(viewportArea))
                || (minimumAreaRatioForInFlowContent(viewportArea) < targetAreaRatio && targetAreaRatio < maximumAreaRatioForInFlowContent(viewportArea));
        }();

        if (!shouldAddTarget)
            continue;

        bool checkForNearbyTargets = includeNearbyElements == IncludeNearbyElements::Yes
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
                candidateOrHost = candidate;
            return candidateOrHost && target.isShadowIncludingInclusiveAncestorOf(candidateOrHost.get());
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

        targets.append(WTF::move(target));
    }

    if (targets.isEmpty())
        return { };

    m_recentAdjustmentClientRectsCleanUpTimer.restart();

    ElementSelectorCache cache;
    Vector<TargetedElementInfo> results;
    results.reserveInitialCapacity(targets.size());
    for (auto iterator = targets.rbegin(); iterator != targets.rend(); ++iterator) {
        if (auto info = targetedElementInfo(*iterator, IsNearbyTarget::No, cache, m_adjustedElements)) {
            results.append(WTF::move(*info));
            addOutOfFlowTargetClientRectIfNeeded(*iterator);
        }
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

            RefPtr element = renderer.element();
            if (!element)
                continue;

            bool elementIsAlreadyTargeted = targets.containsIf([&element](auto& target) {
                return target->isShadowIncludingInclusiveAncestorOf(element.get());
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

        return filterRedundantNearbyTargets(WTF::move(results));
    }();

    for (auto& element : nearbyTargets) {
        if (auto info = targetedElementInfo(element, IsNearbyTarget::Yes, cache, m_adjustedElements)) {
            results.append(WTF::move(*info));
            addOutOfFlowTargetClientRectIfNeeded(element);
        }
    }

    return results;
}

static inline Ref<Element> elementToAdjust(Element& element)
{
    if (RefPtr pseudoElement = dynamicDowncast<PseudoElement>(element)) {
        if (RefPtr host = pseudoElement->hostElement())
            return host.releaseNonNull();
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
        auto [nodeID, documentID] = identifiers;
        auto rect = m_recentAdjustmentClientRects.get(nodeID);
        if (rect.isEmpty())
            continue;

        if (RefPtr target = dynamicDowncast<Element>(Node::fromIdentifier(identifiers.first)); target && target->isInVisibilityAdjustmentSubtree()) {
            // This target's visibility has already been adjusted; avoid treating it as a new region.
            continue;
        }

        newAdjustmentRegion.unite(rect);
    }

    m_repeatedAdjustmentClientRegion.unite(intersect(m_adjustmentClientRegion, newAdjustmentRegion));
    m_adjustmentClientRegion.unite(newAdjustmentRegion);

    Vector<Ref<Element>> elements;
    elements.reserveInitialCapacity(adjustments.size());
    for (auto& [identifiers, selectors] : adjustments) {
        auto [nodeID, documentID] = identifiers;
        RefPtr element = dynamicDowncast<Element>(Node::fromIdentifier(nodeID));
        if (!element)
            continue;

        if (element->document().identifier() != documentID)
            continue;

        elements.append(element.releaseNonNull());
        if (m_additionalAdjustmentCount < maximumNumberOfAdditionalAdjustments) {
            m_visibilityAdjustmentSelectors.append({ nodeID, WTF::move(selectors) });
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
        m_documentsAffectedByVisibilityAdjustment.add(element->document());
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
    if (RefPtr frame = document.frame(); !frame || !frame->isMainFrame())
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
        m_initialVisibilityAdjustmentSelectors = loader->visibilityAdjustmentSelectors();
        m_visibilityAdjustmentSelectors.appendVector(m_initialVisibilityAdjustmentSelectors.map([](auto& selectors) -> std::pair<Markable<NodeIdentifier>, TargetedElementSelectors> {
            return { std::nullopt, selectors };
        }));
        m_startTimeForSelectorBasedVisibilityAdjustment = ApproximateTime::now();
        m_didCollectInitialAdjustments = true;
    }

    if (!m_visibilityAdjustmentSelectors.isEmpty()) {
        if (ApproximateTime::now() - m_startTimeForSelectorBasedVisibilityAdjustment <= selectorBasedVisibilityAdjustmentThrottlingTimeLimit)
            applyVisibilityAdjustmentFromSelectors();
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
        m_documentsAffectedByVisibilityAdjustment.add(element->document());
    }

    dispatchVisibilityAdjustmentStateDidChange();
}

static std::pair<String, VisibilityAdjustment> resolveSelectorToQuery(const String& selectorIncludingPseudo)
{
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
}

void ElementTargetingController::applyVisibilityAdjustmentFromSelectors()
{
    if (m_visibilityAdjustmentSelectors.isEmpty())
        return;

    RefPtr page = m_page.get();
    if (!page)
        return;

    RefPtr document = mainDocument();
    if (!document)
        return;

    document->updateLayoutIgnorePendingStylesheets();

    auto viewportArea = m_viewportSizeForVisibilityAdjustment.area();
    Region adjustmentRegion;
    Vector<String> matchingSelectors;
    for (auto& [identifier, selectorsForElementIncludingShadowHosts] : m_visibilityAdjustmentSelectors) {
        auto [element, selectorIncludingPseudo] = findElementFromSelectors(selectorsForElementIncludingShadowHosts);
        if (!element)
            continue;

        auto [selector, adjustment] = resolveSelectorToQuery(selectorIncludingPseudo);
        auto currentAdjustment = element->visibilityAdjustment();
        if (currentAdjustment.contains(adjustment))
            continue;

        element->setVisibilityAdjustment(currentAdjustment | adjustment);

        if (adjustment == VisibilityAdjustment::Subtree)
            element->invalidateStyleAndRenderersForSubtree();
        else
            element->invalidateStyle();

        m_adjustedElements.add(*element);
        m_documentsAffectedByVisibilityAdjustment.add(element->document());

        if (auto clientRect = inflatedClientRectForAdjustmentRegionTracking(*element, viewportArea))
            adjustmentRegion.unite(*clientRect);

        matchingSelectors.append(WTF::move(selectorIncludingPseudo));
    }

    if (!adjustmentRegion.isEmpty())
        m_adjustmentClientRegion.unite(adjustmentRegion);

    if (matchingSelectors.isEmpty())
        return;

    dispatchVisibilityAdjustmentStateDidChange();
    page->chrome().client().didAdjustVisibilityWithSelectors(WTF::move(matchingSelectors));
}

ElementTargetingController::FindElementFromSelectorsResult ElementTargetingController::findElementFromSelectors(const TargetedElementSelectors& selectorsForElementIncludingShadowHosts)
{
    if (selectorsForElementIncludingShadowHosts.isEmpty())
        return { };

    RefPtr document = mainDocument();
    if (!document)
        return { };

    return findElementFromSelectors(*document, selectorsForElementIncludingShadowHosts);
}

ElementTargetingController::FindElementFromSelectorsResult ElementTargetingController::findElementFromSelectors(Document& document, const TargetedElementSelectors& selectorsForElementIncludingShadowHosts)
{
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
                    return { };

                return { WTF::move(element), selectorIncludingPseudo };
            }

            currentTarget = WTF::move(element);
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

    return { };
}

void ElementTargetingController::reset()
{
    m_adjustmentClientRegion = { };
    m_repeatedAdjustmentClientRegion = { };
    m_viewportSizeForVisibilityAdjustment = { };
    m_adjustedElements = { };
    m_visibilityAdjustmentSelectors = { };
    m_initialVisibilityAdjustmentSelectors = { };
    m_didCollectInitialAdjustments = false;
    m_additionalAdjustmentCount = 0;
    m_selectorBasedVisibilityAdjustmentTimer.stop();
    m_startTimeForSelectorBasedVisibilityAdjustment = { };
    m_recentAdjustmentClientRectsCleanUpTimer.stop();
    cleanUpAdjustmentClientRects();
}

void ElementTargetingController::didChangeMainDocument(Document* newDocument)
{
    m_shouldRecomputeAdjustedElements = newDocument && m_documentsAffectedByVisibilityAdjustment.contains(*newDocument);
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

    document->updateLayoutIgnorePendingStylesheets();

    HashSet<Ref<Element>> elementsToReset;
    if (identifiers.isEmpty()) {
        elementsToReset.reserveInitialCapacity(m_adjustedElements.computeSize());
        for (Ref element : m_adjustedElements)
            elementsToReset.add(element);
        m_adjustedElements.clear();
    } else {
        elementsToReset.reserveInitialCapacity(identifiers.size());
        for (auto [nodeID, documentID] : identifiers) {
            RefPtr element = dynamicDowncast<Element>(Node::fromIdentifier(nodeID));
            if (!element)
                continue;

            if (element->document().identifier() != documentID)
                continue;

            if (!m_adjustedElements.remove(*element))
                continue;

            elementsToReset.add(element.releaseNonNull());
        }
    }

    if (RefPtr loader = document->loader(); loader && !identifiers.isEmpty()) {
        m_initialVisibilityAdjustmentSelectors.removeAllMatching([&](auto& selectors) {
            auto foundElement = findElementFromSelectors(selectors).element;
            return foundElement && elementsToReset.contains(*foundElement);
        });
        m_visibilityAdjustmentSelectors = m_initialVisibilityAdjustmentSelectors.map([](auto& selectors) -> std::pair<Markable<NodeIdentifier>, TargetedElementSelectors> {
            return { std::nullopt, selectors };
        });
    } else {
        // There are no initial adjustments after resetting.
        m_visibilityAdjustmentSelectors = { };
        m_initialVisibilityAdjustmentSelectors = { };
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
        for (Ref element : m_adjustedElements) {
            if (auto rect = inflatedClientRectForAdjustmentRegionTracking(element, viewportArea))
                m_adjustmentClientRegion.unite(*rect);
        }
    }

    if (changed)
        dispatchVisibilityAdjustmentStateDidChange();

    return changed;
}

uint64_t ElementTargetingController::numberOfVisibilityAdjustmentRects()
{
    RefPtr page = m_page.get();
    if (!page)
        return 0;

    if (!page->hasEverSetVisibilityAdjustment() && !m_shouldRecomputeAdjustedElements)
        return 0;

    RefPtr mainFrame = dynamicDowncast<LocalFrame>(page->mainFrame());
    if (!mainFrame)
        return 0;

    RefPtr document = mainFrame->document();
    if (!document)
        return 0;

    document->updateLayoutIgnorePendingStylesheets();

    recomputeAdjustedElementsIfNeeded();

    Vector<FloatRect> clientRects;
    clientRects.reserveInitialCapacity(m_adjustedElements.computeSize());

    unsigned numberOfParentedEmptyOrNonRenderedElements = 0;
    for (Ref element : m_adjustedElements) {
        if (!element->isConnected())
            continue;

        CheckedPtr renderer = element->renderer();
        if (!renderer) {
            numberOfParentedEmptyOrNonRenderedElements++;
            continue;
        }

        auto clientRect = computeClientRect(*renderer);
        if (clientRect.isEmpty()) {
            numberOfParentedEmptyOrNonRenderedElements++;
            continue;
        }

        clientRects.append(clientRect);
    }

    // Sort by area in descending order so that we don't double-count fully overlapped elements.
    std::ranges::sort(clientRects, std::ranges::greater { }, &FloatRect::area);

    Region adjustedRegion;
    uint64_t numberOfRects = 0;

    for (auto rect : clientRects) {
        auto enclosingRect = enclosingIntRect(rect);
        if (adjustedRegion.contains(enclosingRect))
            continue;

        numberOfRects++;
        adjustedRegion.unite(enclosingRect);
    }

    return numberOfParentedEmptyOrNonRenderedElements + numberOfRects;
}

void ElementTargetingController::recomputeAdjustedElementsIfNeeded()
{
    if (!m_shouldRecomputeAdjustedElements)
        return;

    m_shouldRecomputeAdjustedElements = false;

    RefPtr mainDocument = this->mainDocument();
    if (!mainDocument)
        return;

    RefPtr documentElement = mainDocument->documentElement();
    if (!documentElement)
        return;

    for (Ref element : descendantsOfType<Element>(*documentElement)) {
        auto adjustment = element->visibilityAdjustment();
        if (adjustment.isEmpty())
            continue;

        if (adjustment.contains(VisibilityAdjustment::Subtree))
            m_adjustedElements.add(element);

        if (adjustment.contains(VisibilityAdjustment::AfterPseudo)) {
            if (RefPtr afterPseudo = element->afterPseudoElement())
                m_adjustedElements.add(*afterPseudo);
        }

        if (adjustment.contains(VisibilityAdjustment::BeforePseudo)) {
            if (RefPtr beforePseudo = element->beforePseudoElement())
                m_adjustedElements.add(*beforePseudo);
        }
    }
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

RefPtr<Document> ElementTargetingController::mainDocument() const
{
    RefPtr page = m_page.get();
    if (!page)
        return { };

    RefPtr mainFrame = dynamicDowncast<LocalFrame>(page->mainFrame());
    if (!mainFrame)
        return { };

    return mainFrame->document();
}

void ElementTargetingController::selectorBasedVisibilityAdjustmentTimerFired()
{
    applyVisibilityAdjustmentFromSelectors();
}

RefPtr<Image> ElementTargetingController::snapshotIgnoringVisibilityAdjustment(NodeIdentifier nodeID, ScriptExecutionContextIdentifier documentID)
{
    RefPtr page = m_page.get();
    if (!page)
        return { };

    RefPtr mainFrame = dynamicDowncast<LocalFrame>(page->mainFrame());
    if (!mainFrame)
        return { };

    RefPtr element = dynamicDowncast<Element>(Node::fromIdentifier(nodeID));
    if (!element)
        return { };

    RefPtr frameView = mainFrame->view();
    if (!frameView)
        return { };

    if (element->document().identifier() != documentID)
        return { };

    ClearVisibilityAdjustmentForScope clearAdjustmentScope { *element };
    protect(element->document())->updateLayoutIgnorePendingStylesheets();

    CheckedPtr renderer = element->renderer();
    if (!renderer)
        return { };

    if (!renderer->isRenderReplaced() && !renderer->firstChild() && !renderer->style().hasBackgroundImage())
        return { };

    auto backgroundColor = frameView->baseBackgroundColor();
    frameView->setBaseBackgroundColor(Color::transparentBlack);
    frameView->setNodeToDraw(element.get());
    auto resetPaintingState = makeScopeExit([frameView, backgroundColor]() mutable {
        frameView->setBaseBackgroundColor(WTF::move(backgroundColor));
        frameView->setNodeToDraw(nullptr);
    });

    auto snapshotRect = renderer->absoluteBoundingBoxRect();
    if (snapshotRect.isEmpty())
        return { };

    auto buffer = snapshotFrameRect(*mainFrame, snapshotRect, { { }, PixelFormat::BGRA8, DestinationColorSpace::SRGB() });
    return BitmapImage::create(ImageBuffer::sinkIntoNativeImage(WTF::move(buffer)));
}

} // namespace WebCore
