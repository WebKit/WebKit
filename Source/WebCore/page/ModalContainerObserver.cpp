/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "ModalContainerTypes.h"
#include "Page.h"
#include "RenderDescendantIterator.h"
#include "RenderText.h"
#include "RenderView.h"
#include "SimulatedClickOptions.h"
#include "Text.h"
#include <wtf/Scope.h>
#include <wtf/URL.h>

namespace WebCore {

static constexpr size_t maxLengthForClickableElementText = 100;

bool ModalContainerObserver::isNeededFor(const Document& document)
{
    RefPtr documentLoader = document.loader();
    if (!documentLoader || documentLoader->modalContainerObservationPolicy() == ModalContainerObservationPolicy::Disabled)
        return false;

    if (!document.url().protocolIsInHTTPFamily())
        return false;

    if (!document.isTopDocument()) {
        // FIXME (234446): Need to properly support subframes.
        return false;
    }

    if (document.inDesignMode() || !is<HTMLDocument>(document))
        return false;

    auto* frame = document.frame();
    if (!frame)
        return false;

    auto* page = frame->page();
    if (!page || page->isEditable())
        return false;

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
    bool shouldSearchNode = ([&] {
        if (!is<HTMLElement>(parent.get()))
            return false;

        return parent->hasTagName(HTMLNames::aTag) || parent->hasTagName(HTMLNames::divTag) || parent->hasTagName(HTMLNames::pTag) || parent->hasTagName(HTMLNames::spanTag) || parent->hasTagName(HTMLNames::sectionTag);
    })();

    if (!shouldSearchNode)
        return false;

    if (LIKELY(!textNode.data().containsIgnoringASCIICase(searchTerm)))
        return false;

    return true;
}

void ModalContainerObserver::updateModalContainerIfNeeded(const FrameView& view)
{
    if (m_container)
        return;

    if (m_hasAttemptedToFulfillPolicy)
        return;

    if (!view.hasViewportConstrainedObjects())
        return;

    auto* page = view.frame().page();
    if (!page)
        return;

    auto& searchTerm = page->chrome().client().searchStringForModalContainerObserver();
    if (searchTerm.isNull())
        return;

    for (auto& renderer : *view.viewportConstrainedObjects()) {
        RefPtr element = renderer.element();
        if (!element || is<HTMLBodyElement>(*element) || element->isDocumentNode())
            continue;

        if (!m_elementsToIgnoreWhenSearching.add(*element).isNewEntry)
            continue;

        for (auto& textRenderer : descendantsOfType<RenderText>(renderer)) {
            if (RefPtr textNode = textRenderer.textNode(); textNode && matchesSearchTerm(*textNode, searchTerm)) {
                m_container = element.get();
                element->invalidateStyle();
                scheduleClickableElementCollection();
                return;
            }
        }
    }
}

static AccessibilityRole accessibilityRole(const HTMLElement& element)
{
    return AccessibilityObject::ariaRoleToWebCoreRole(element.attributeWithoutSynchronization(HTMLNames::roleAttr));
}

static bool isClickableControl(const HTMLElement& element)
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

    return element.hasEventListeners(eventNames().clickEvent) || element.hasEventListeners(eventNames().mousedownEvent) || element.hasEventListeners(eventNames().mouseupEvent)
        || element.hasEventListeners(eventNames().touchstartEvent) || element.hasEventListeners(eventNames().touchendEvent)
        || element.hasEventListeners(eventNames().pointerdownEvent) || element.hasEventListeners(eventNames().pointerupEvent);
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

void ModalContainerObserver::collectClickableElementsTimerFired()
{
    if (!m_container)
        return;

    m_container->document().eventLoop().queueTask(TaskSource::InternalAsyncTask, [observer = this, weakDocument = WeakPtr { m_container->document() }]() mutable {
        RefPtr document = weakDocument.get();
        if (!document)
            return;

        if (observer != document->modalContainerObserverIfExists() || !observer->m_container.get()) {
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

        page->chrome().client().classifyModalContainerControls(WTFMove(controlTextsToClassify), [weakDocument = WTFMove(weakDocument), observer, controls = WTFMove(classifiableControls)] (auto&& types) mutable {
            if (types.size() != controls.size())
                return;

            RefPtr document = weakDocument.get();
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

            struct ClassifiedControls {
                Vector<WeakPtr<HTMLElement>> positive;
                Vector<WeakPtr<HTMLElement>> neutral;
                Vector<WeakPtr<HTMLElement>> negative;

                HTMLElement* controlToClick(ModalContainerDecision decision) const
                {
                    auto matchNonNull = [&](const WeakPtr<HTMLElement>& element) {
                        return !!element;
                    };

                    switch (decision) {
                    case ModalContainerDecision::Show:
                    case ModalContainerDecision::HideAndIgnore:
                        break;
                    case ModalContainerDecision::HideAndAllow:
                        if (auto index = positive.findMatching(matchNonNull); index != notFound)
                            return positive[index].get();
                        if (auto index = neutral.findMatching(matchNonNull); index != notFound)
                            return neutral[index].get();
                        break;
                    case ModalContainerDecision::HideAndDisallow:
                        if (auto index = negative.findMatching(matchNonNull); index != notFound)
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
            if (clickableControlTypes.isEmpty()) {
                observer->revealModalContainer();
                return;
            }

            page->chrome().client().decidePolicyForModalContainer(clickableControlTypes, [weakDocument = WTFMove(weakDocument), observer, classifiedControls = WTFMove(classifiedControls)](auto decision) mutable {
                RefPtr document = weakDocument.get();
                if (!document)
                    return;

                if (observer != document->modalContainerObserverIfExists())
                    return;

                if (decision == ModalContainerDecision::Show) {
                    observer->revealModalContainer();
                    return;
                }

                if (RefPtr controlToClick = classifiedControls.controlToClick(decision))
                    controlToClick->dispatchSimulatedClick(nullptr, SendMouseUpDownEvents, DoNotShowPressedLook);
            });
        });
    });
}

void ModalContainerObserver::revealModalContainer()
{
    if (auto container = std::exchange(m_container, { }))
        container->invalidateStyle();
}

std::pair<Vector<WeakPtr<HTMLElement>>, Vector<String>> ModalContainerObserver::collectClickableElements()
{
    Ref container = *m_container;
    m_collectingClickableElements = true;
    auto exitCollectClickableElementsScope = makeScopeExit([&] {
        m_collectingClickableElements = false;
        container->invalidateStyle();
    });

    container->invalidateStyle();
    container->document().updateLayoutIgnorePendingStylesheets();

    Vector<Ref<HTMLElement>> clickableControls;
    for (auto& child : descendantsOfType<HTMLElement>(container)) {
        if (isClickableControl(child))
            clickableControls.append(child);
    }

    removeElementsWithEmptyBounds(clickableControls);
    removeParentOrChildElements(clickableControls);

    Vector<WeakPtr<HTMLElement>> classifiableControls;
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

bool ModalContainerObserver::shouldHide(const Element& element)
{
    return &element == m_container && !m_collectingClickableElements;
}

} // namespace WebCore
