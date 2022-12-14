/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
#include "ModalContainerObserver.h"

#include "AccessibilityObject.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "ElementIterator.h"
#include "EventHandler.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLAnchorElement.h"
#include "HTMLBodyElement.h"
#include "HTMLDocument.h"
#include "HTMLElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HitTestResult.h"
#include "ModalContainerTypes.h"
#include "Page.h"
#include "RenderDescendantIterator.h"
#include "RenderText.h"
#include "RenderView.h"
#include "SimulatedClickOptions.h"
#include "Text.h"
#include <wtf/Noncopyable.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/Scope.h>
#include <wtf/URL.h>

namespace WebCore {

static constexpr size_t maxLengthForClickableElementText = 100;
static constexpr double maxWidthForElementsThatLookClickable = 200;
static constexpr double maxHeightForElementsThatLookClickable = 100;

bool ModalContainerObserver::isNeededFor(const Document& document)
{
    RefPtr topDocumentLoader = document.topDocument().loader();
    if (!topDocumentLoader || topDocumentLoader->modalContainerObservationPolicy() == ModalContainerObservationPolicy::Disabled)
        return false;

    if (!document.topDocument().url().protocolIsInHTTPFamily())
        return false;

    if (document.inDesignMode() || !is<HTMLDocument>(document))
        return false;

    auto* frame = document.frame();
    if (!frame)
        return false;

    auto* page = frame->page();
    if (!page || page->isEditable())
        return false;

    if (RefPtr owner = document.ownerElement()) {
        auto observer = owner->document().modalContainerObserverIfExists();
        return observer && observer->m_frameOwnersAndContainersToSearchAgain.contains(*owner);
    }

    return true;
}

ModalContainerObserver::ModalContainerObserver()
    : m_collectClickableElementsTimer(*this, &ModalContainerObserver::collectClickableElementsTimerFired)
{
}

ModalContainerObserver::~ModalContainerObserver() = default;

static bool matchesSearchTerm(const Text& textNode, const AtomString& searchTerm)
{
    RefPtr parent = textNode.parentElementInComposedTree();

    static NeverDestroyed tagNamesToSearch = [] {
        static constexpr std::array tags {
            &HTMLNames::aTag,
            &HTMLNames::divTag,
            &HTMLNames::pTag,
            &HTMLNames::spanTag,
            &HTMLNames::sectionTag,
            &HTMLNames::bTag,
            &HTMLNames::iTag,
            &HTMLNames::uTag,
            &HTMLNames::liTag,
            &HTMLNames::h1Tag,
            &HTMLNames::h2Tag,
            &HTMLNames::h3Tag,
            &HTMLNames::h4Tag,
            &HTMLNames::h5Tag,
            &HTMLNames::h6Tag,
        };
        MemoryCompactLookupOnlyRobinHoodHashSet<AtomString> set;
        set.reserveInitialCapacity(std::size(tags));
        for (auto& tag : tags)
            set.add(tag->get().localName());
        return set;
    }();

    if (!is<HTMLElement>(parent.get()) || !tagNamesToSearch.get().contains(downcast<HTMLElement>(*parent).localName()))
        return false;

    if (LIKELY(!textNode.data().containsIgnoringASCIICase(searchTerm)))
        return false;

    return true;
}

static AccessibilityRole accessibilityRole(const HTMLElement& element)
{
    return AccessibilityObject::ariaRoleToWebCoreRole(element.attributeWithoutSynchronization(HTMLNames::roleAttr));
}

static bool isInsideNavigationElement(const Text& textNode)
{
    for (auto& parent : ancestorsOfType<HTMLElement>(textNode)) {
        if (parent.hasTagName(HTMLNames::navTag) || accessibilityRole(parent) == AccessibilityRole::LandmarkNavigation)
            return true;
    }
    return false;
}

struct TextSearchResult {
    bool foundMatch { false };
    bool containsAnyText { false };
};

static TextSearchResult searchForMatch(RenderLayerModelObject& renderer, const AtomString& searchTerm)
{
    TextSearchResult result;
    for (auto& textRenderer : descendantsOfType<RenderText>(renderer)) {
        result.containsAnyText = true;
        if (RefPtr textNode = textRenderer.textNode(); textNode && matchesSearchTerm(*textNode, searchTerm)) {
            result.foundMatch = !isInsideNavigationElement(*textNode);
            return result;
        }
    }
    return result;
}

void ModalContainerObserver::searchForModalContainerOnBehalfOfFrameOwnerIfNeeded(HTMLFrameOwnerElement& owner)
{
    auto containerToSearchAgain = m_frameOwnersAndContainersToSearchAgain.take(owner);
    if (!containerToSearchAgain)
        return;

    if (!m_elementsToIgnoreWhenSearching.remove(*containerToSearchAgain))
        return;

    if (RefPtr view = owner.document().view())
        updateModalContainerIfNeeded(*view);
}

void ModalContainerObserver::updateModalContainerIfNeeded(const FrameView& view)
{
    if (container())
        return;

    if (m_hasAttemptedToFulfillPolicy)
        return;

    if (RefPtr owner = view.frame().ownerElement()) {
        if (auto parentObserver = owner->document().modalContainerObserverIfExists())
            parentObserver->searchForModalContainerOnBehalfOfFrameOwnerIfNeeded(*owner);
        return;
    }

    if (!view.frame().isMainFrame())
        return;

    if (!view.hasViewportConstrainedObjects())
        return;

    auto searchTerm = ([&]() -> AtomString {
        if (UNLIKELY(!m_overrideSearchTermForTesting.isNull()))
            return m_overrideSearchTermForTesting;

        if (auto* page = view.frame().page())
            return page->chrome().client().searchStringForModalContainerObserver();

        return nullAtom();
    })();

    if (searchTerm.isNull())
        return;

    for (auto& renderer : *view.viewportConstrainedObjects()) {
        if (renderer.isDocumentElementRenderer())
            continue;

        if (renderer.style().visibility() == Visibility::Hidden)
            continue;

        RefPtr element = renderer.element();
        if (!element || is<HTMLBodyElement>(*element) || element->isDocumentNode())
            continue;

        if (m_elementsToIgnoreWhenSearching.contains(*element))
            continue;

        auto [foundMatch, containsAnyText] = searchForMatch(renderer, searchTerm);

        if (containsAnyText)
            m_elementsToIgnoreWhenSearching.add(*element);

        if (foundMatch) {
            setContainer(*element);
            return;
        }

        for (auto& frameOwner : descendantsOfType<HTMLFrameOwnerElement>(*element)) {
            RefPtr contentFrame = dynamicDowncast<LocalFrame>(frameOwner.contentFrame());
            if (!contentFrame)
                continue;

            auto renderView = contentFrame->contentRenderer();
            if (!renderView)
                continue;

            if (searchForMatch(*renderView, searchTerm).foundMatch) {
                setContainer(*element, &frameOwner);
                return;
            }

            if (auto frameView = contentFrame->view(); frameView && !frameView->isVisuallyNonEmpty()) {
                // If the subframe content has not become visually non-empty yet, search the subframe again later.
                m_frameOwnersAndContainersToSearchAgain.add(frameOwner, *element);
            }
        }
    }
}

void ModalContainerObserver::setContainer(Element& newContainer, HTMLFrameOwnerElement* frameOwner)
{
    if (container())
        container()->invalidateStyle();

    if (m_userInteractionBlockingElement)
        m_userInteractionBlockingElement->invalidateStyle();

    m_userInteractionBlockingElement = { };
    m_containerAndFrameOwnerForControls = { { newContainer }, { frameOwner } };

    newContainer.invalidateStyle();
    scheduleClickableElementCollection();

    newContainer.document().eventLoop().queueTask(TaskSource::InternalAsyncTask, [weakContainer = WeakPtr { newContainer }]() mutable {
        RefPtr container = weakContainer.get();
        if (!container)
            return;

        auto observer = container->document().modalContainerObserverIfExists();
        if (!observer || container != observer->container())
            return;

        observer->hideUserInteractionBlockingElementIfNeeded();
        observer->makeBodyAndDocumentElementScrollableIfNeeded();
    });
}

Element* ModalContainerObserver::container() const
{
    return m_containerAndFrameOwnerForControls.first.get();
}

HTMLFrameOwnerElement* ModalContainerObserver::frameOwnerForControls() const
{
    return m_containerAndFrameOwnerForControls.second.get();
}

static bool listensForUserActivation(const Element& element)
{
    return element.hasEventListeners(eventNames().clickEvent) || element.hasEventListeners(eventNames().mousedownEvent) || element.hasEventListeners(eventNames().mouseupEvent)
        || element.hasEventListeners(eventNames().touchstartEvent) || element.hasEventListeners(eventNames().touchendEvent)
        || element.hasEventListeners(eventNames().pointerdownEvent) || element.hasEventListeners(eventNames().pointerupEvent);
}

enum class ContainerListensForUserActivation : bool { No, Yes };
static bool isClickableControl(const HTMLElement& element, ContainerListensForUserActivation containerListensForUserActivation)
{
    if (element.isActuallyDisabled())
        return false;

    if (!element.renderer())
        return false;

    if (element.hasTagName(HTMLNames::buttonTag))
        return true;

    if (is<HTMLInputElement>(element) && downcast<HTMLInputElement>(element).isTextButton())
        return true;

    switch (accessibilityRole(element)) {
    case AccessibilityRole::Button:
        return true;
    case AccessibilityRole::CheckBox:
    case AccessibilityRole::Switch:
        return false;
    default:
        break;
    }

    if (is<HTMLAnchorElement>(element)) {
        // FIXME: We might need a more comprehensive policy here that attempts to click the link to detect navigation,
        // but then immediately cancels the navigation.
        auto href = downcast<HTMLAnchorElement>(element).href();
        return equalIgnoringFragmentIdentifier(element.document().url(), href) || !href.protocolIsInHTTPFamily();
    }

    if (listensForUserActivation(element))
        return true;

    if (containerListensForUserActivation == ContainerListensForUserActivation::No)
        return false;

    auto rendererAndRect = element.boundingAbsoluteRectWithoutLayout();
    if (!rendererAndRect)
        return false;

    auto [renderer, rect] = *rendererAndRect;
    if (!renderer || rect.isEmpty())
        return false;

    // If the modal container itself has event listeners for user activation, continue looking for elements that look like
    // clickable elements (e.g. small nodes with pointer-style cursor).
    if (renderer->style().cursor() == CursorType::Pointer) {
        if (rect.width() <= maxWidthForElementsThatLookClickable && rect.height() <= maxHeightForElementsThatLookClickable)
            return true;
    }

    return false;
}

static void removeParentOrChildElements(Vector<Ref<HTMLElement>>& elements)
{
    HashSet<Ref<HTMLElement>> elementsToRemove;
    for (auto& outer : elements) {
        if (elementsToRemove.contains(outer))
            continue;

        for (auto& inner : elements) {
            if (elementsToRemove.contains(inner) || outer.ptr() == inner.ptr() || !outer->contains(inner))
                continue;

            if (accessibilityRole(outer) == AccessibilityRole::Button) {
                elementsToRemove.add(inner);
                continue;
            }

            if (accessibilityRole(inner) == AccessibilityRole::Button) {
                elementsToRemove.add(outer);
                continue;
            }

            if (outer->hasTagName(HTMLNames::divTag) || outer->hasTagName(HTMLNames::spanTag) || outer->hasTagName(HTMLNames::pTag) || outer->hasTagName(HTMLNames::sectionTag))
                elementsToRemove.add(outer);
            else
                elementsToRemove.add(inner);
        }
    }

    elements.removeAllMatching([&] (auto& control) {
        return elementsToRemove.contains(control);
    });
}

static void removeElementsWithEmptyBounds(Vector<Ref<HTMLElement>>& elements)
{
    elements.removeAllMatching([&] (auto& element) {
        return element->boundingClientRect().isEmpty();
    });
}

static String textForControl(HTMLElement& control)
{
    auto ariaLabel = control.attributeWithoutSynchronization(HTMLNames::aria_labelAttr);
    if (!ariaLabel.isEmpty())
        return ariaLabel;

    if (is<HTMLInputElement>(control))
        return downcast<HTMLInputElement>(control).value();

    auto title = control.title();
    if (!title.isEmpty())
        return title;

    if (is<HTMLImageElement>(control)) {
        auto altText = downcast<HTMLImageElement>(control).altText();
        if (!altText.isEmpty())
            return altText;
    }

    return control.outerText();
}

void ModalContainerObserver::scheduleClickableElementCollection()
{
    m_collectClickableElementsTimer.startOneShot(200_ms);
}

class ModalContainerPolicyDecisionScope {
    WTF_MAKE_NONCOPYABLE(ModalContainerPolicyDecisionScope);
    WTF_MAKE_FAST_ALLOCATED;
public:
    ModalContainerPolicyDecisionScope(Document& document)
        : m_document { document }
    {
    }

    ModalContainerPolicyDecisionScope(ModalContainerPolicyDecisionScope&&) = default;

    ~ModalContainerPolicyDecisionScope()
    {
        if (m_continueHidingModalContainerAfterScope || !m_document)
            return;

        if (auto observer = m_document->modalContainerObserverIfExists())
            observer->revealModalContainer();
    }

    void continueHidingModalContainerAfterScope() { m_continueHidingModalContainerAfterScope = true; }
    Document* document() const { return m_document.get(); }

private:
    WeakPtr<Document, WeakPtrImplWithEventTargetData> m_document;
    bool m_continueHidingModalContainerAfterScope { false };
};

void ModalContainerObserver::collectClickableElementsTimerFired()
{
    if (!container())
        return;

    container()->document().eventLoop().queueTask(TaskSource::InternalAsyncTask, [observer = this, decisionScope = ModalContainerPolicyDecisionScope { container()->document() }]() mutable {
        RefPtr document = decisionScope.document();
        if (!document)
            return;

        if (observer != document->modalContainerObserverIfExists() || !observer->container()) {
            ASSERT_NOT_REACHED();
            return;
        }

        auto [classifiableControls, controlTextsToClassify] = observer->collectClickableElements();
        if (classifiableControls.isEmpty())
            return;

        auto* page = document->page();
        if (!page) {
            ASSERT_NOT_REACHED();
            return;
        }

        page->chrome().client().classifyModalContainerControls(WTFMove(controlTextsToClassify), [decisionScope = WTFMove(decisionScope), observer, controls = WTFMove(classifiableControls)] (auto&& types) mutable {
            RefPtr document = decisionScope.document();
            if (!document)
                return;

            RefPtr documentLoader = document->loader();
            if (!documentLoader) {
                ASSERT_NOT_REACHED();
                return;
            }

            if (observer != document->modalContainerObserverIfExists()) {
                ASSERT_NOT_REACHED();
                return;
            }

            if (types.size() != controls.size())
                return;

            struct ClassifiedControls {
                Vector<WeakPtr<HTMLElement, WeakPtrImplWithEventTargetData>> positive;
                Vector<WeakPtr<HTMLElement, WeakPtrImplWithEventTargetData>> neutral;
                Vector<WeakPtr<HTMLElement, WeakPtrImplWithEventTargetData>> negative;

                HTMLElement* controlToClick(ModalContainerDecision decision) const
                {
                    auto matchNonNull = [&](const WeakPtr<HTMLElement, WeakPtrImplWithEventTargetData>& element) {
                        return !!element;
                    };

                    switch (decision) {
                    case ModalContainerDecision::Show:
                    case ModalContainerDecision::HideAndIgnore:
                        break;
                    case ModalContainerDecision::HideAndAllow:
                        if (auto index = positive.findIf(matchNonNull); index != notFound)
                            return positive[index].get();
                        if (auto index = neutral.findIf(matchNonNull); index != notFound)
                            return neutral[index].get();
                        break;
                    case ModalContainerDecision::HideAndDisallow:
                        if (auto index = negative.findIf(matchNonNull); index != notFound)
                            return negative[index].get();
                        break;
                    }
                    return nullptr;
                }

                OptionSet<ModalContainerControlType> types() const
                {
                    OptionSet<ModalContainerControlType> availableTypesIgnoringOther;
                    if (!positive.isEmpty())
                        availableTypesIgnoringOther.add(ModalContainerControlType::Positive);
                    if (!negative.isEmpty())
                        availableTypesIgnoringOther.add(ModalContainerControlType::Negative);
                    if (!neutral.isEmpty())
                        availableTypesIgnoringOther.add(ModalContainerControlType::Neutral);
                    return availableTypesIgnoringOther;
                }
            };

            ClassifiedControls classifiedControls;
            for (size_t index = 0; index < types.size(); ++index) {
                auto control = controls[index];
                if (!control)
                    continue;

                switch (types[index]) {
                case ModalContainerControlType::Positive:
                    classifiedControls.positive.append(control);
                    break;
                case ModalContainerControlType::Negative:
                    classifiedControls.negative.append(control);
                    break;
                case ModalContainerControlType::Neutral:
                    classifiedControls.neutral.append(control);
                    break;
                case ModalContainerControlType::Other:
                    break;
                }
            }

            observer->m_hasAttemptedToFulfillPolicy = true;

            auto* page = document->page();
            if (!page)
                return;

            auto clickableControlTypes = classifiedControls.types();
            if (clickableControlTypes.isEmpty())
                return;

            page->chrome().client().decidePolicyForModalContainer(clickableControlTypes, [decisionScope = WTFMove(decisionScope), observer, classifiedControls = WTFMove(classifiedControls)](auto decision) mutable {
                RefPtr document = decisionScope.document();
                if (!document)
                    return;

                if (observer != document->modalContainerObserverIfExists()) {
                    ASSERT_NOT_REACHED();
                    return;
                }

                if (decision == ModalContainerDecision::Show)
                    return;

                if (RefPtr controlToClick = classifiedControls.controlToClick(decision)) {
                    observer->clearScrollabilityOverrides(*document);
                    controlToClick->dispatchSimulatedClick(nullptr, SendMouseUpDownEvents, DoNotShowPressedLook);
                }

                decisionScope.continueHidingModalContainerAfterScope();
            });
        });
    });
}

void ModalContainerObserver::makeBodyAndDocumentElementScrollableIfNeeded()
{
    if (!container())
        return;

    Ref document = container()->document();
    RefPtr view = document->view();
    if (!view || view->isScrollable())
        return;

    document->updateLayoutIgnorePendingStylesheets();

    auto visibleHeight = view->visibleSize().height();
    auto shouldMakeElementScrollable = [visibleHeight] (Element* element) {
        if (!element)
            return false;

        auto renderer = element->renderer();
        if (!renderer || renderer->style().overflowY() != Overflow::Hidden)
            return false;

        return element->boundingClientRect().height() > visibleHeight;
    };

    if (!m_makeBodyElementScrollable) {
        if (RefPtr body = document->body(); shouldMakeElementScrollable(body.get())) {
            m_makeBodyElementScrollable = true;
            body->invalidateStyle();
        }
    }

    if (!m_makeDocumentElementScrollable) {
        if (RefPtr documentElement = document->documentElement(); shouldMakeElementScrollable(documentElement.get())) {
            m_makeDocumentElementScrollable = true;
            documentElement->invalidateStyle();
        }
    }
}

void ModalContainerObserver::clearScrollabilityOverrides(Document& document)
{
    if (std::exchange(m_makeBodyElementScrollable, false)) {
        if (auto element = document.body())
            element->invalidateStyle();
    }

    if (std::exchange(m_makeDocumentElementScrollable, false)) {
        if (auto element = document.documentElement())
            element->invalidateStyle();
    }
}

void ModalContainerObserver::hideUserInteractionBlockingElementIfNeeded()
{
    if (m_userInteractionBlockingElement)
        return;

    RefPtr container = this->container();
    if (!container) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr view = container->document().view();
    if (!view)
        return;

    auto fixedPositionRect = view->rectForFixedPositionLayout();
    if (fixedPositionRect.isEmpty())
        return;

    FixedVector locationsToHitTest {
        fixedPositionRect.center(),
        fixedPositionRect.minXMinYCorner() + LayoutSize { 1, 1 },
        fixedPositionRect.maxXMinYCorner() + LayoutSize { -1, 1 },
        fixedPositionRect.minXMaxYCorner() + LayoutSize { 1, -1 },
        fixedPositionRect.maxXMaxYCorner() + LayoutSize { -1, -1 }
    };

    constexpr OptionSet hitTestTypes { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::DisallowUserAgentShadowContent };

    RefPtr<Element> foundElement;
    for (auto& location : locationsToHitTest) {
        auto hitTestResult = view->frame().eventHandler().hitTestResultAtPoint(location, hitTestTypes);
        auto target = hitTestResult.targetElement();
        if (!target || is<HTMLBodyElement>(*target) || target->isDocumentNode())
            return;

        if (foundElement && foundElement != target)
            return;

        auto renderer = target->renderer();
        if (!renderer || renderer->firstChild() || !renderer->style().hasViewportConstrainedPosition() || renderer->isDocumentElementRenderer())
            return;

        if (!foundElement)
            foundElement = target;
    }

    m_userInteractionBlockingElement = foundElement.get();
    foundElement->invalidateStyle();
}

void ModalContainerObserver::revealModalContainer()
{
    auto [container, frameOwner] = std::exchange(m_containerAndFrameOwnerForControls, { });
    if (container) {
        container->invalidateStyle();
        clearScrollabilityOverrides(container->document());
    }

    if (auto element = std::exchange(m_userInteractionBlockingElement, { }))
        element->invalidateStyle();
}

std::pair<Vector<WeakPtr<HTMLElement, WeakPtrImplWithEventTargetData>>, Vector<String>> ModalContainerObserver::collectClickableElements()
{
    Ref container = *this->container();
    m_collectingClickableElements = true;
    auto exitCollectClickableElementsScope = makeScopeExit([&] {
        m_collectingClickableElements = false;
        container->invalidateStyle();
    });

    container->invalidateStyle();
    container->document().updateLayoutIgnorePendingStylesheets();

    auto containerForControls = ([&]() -> RefPtr<Element> {
        auto frameOwner = frameOwnerForControls();
        if (!frameOwner)
            return container.ptr();

        auto contentDocument = frameOwner->contentDocument();
        if (!contentDocument)
            return { };

        return contentDocument->documentElement();
    })();

    if (!containerForControls)
        return { };

    auto containerListensForUserActivation = listensForUserActivation(*containerForControls) ? ContainerListensForUserActivation::Yes : ContainerListensForUserActivation::No;
    Vector<Ref<HTMLElement>> clickableControls;
    for (auto& child : descendantsOfType<HTMLElement>(*containerForControls)) {
        if (isClickableControl(child, containerListensForUserActivation))
            clickableControls.append(child);
    }

    removeElementsWithEmptyBounds(clickableControls);
    removeParentOrChildElements(clickableControls);

    Vector<WeakPtr<HTMLElement, WeakPtrImplWithEventTargetData>> classifiableControls;
    Vector<String> controlTextsToClassify;
    classifiableControls.reserveInitialCapacity(clickableControls.size());
    controlTextsToClassify.reserveInitialCapacity(clickableControls.size());
    for (auto& control : clickableControls) {
        auto text = textForControl(control).stripWhiteSpace();
        if (!text.isEmpty() && text.length() < maxLengthForClickableElementText) {
            classifiableControls.uncheckedAppend({ control });
            controlTextsToClassify.uncheckedAppend(WTFMove(text));
        }
    }
    return { WTFMove(classifiableControls), WTFMove(controlTextsToClassify) };
}

bool ModalContainerObserver::shouldMakeVerticallyScrollable(const Element& element) const
{
    if (m_makeBodyElementScrollable && element.document().body() == &element)
        return true;

    if (m_makeDocumentElementScrollable && element.document().documentElement() == &element)
        return true;

    return false;
}

} // namespace WebCore
