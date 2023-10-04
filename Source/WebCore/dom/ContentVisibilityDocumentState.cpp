/*
 * Copyright (C) 2023 Igalia S.L. All rights reserved.
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
#include "ContentVisibilityDocumentState.h"

#include "ContentVisibilityAutoStateChangeEvent.h"
#include "DeclarativeAnimation.h"
#include "DocumentTimeline.h"
#include "Element.h"
#include "EventNames.h"
#include "FrameSelection.h"
#include "IntersectionObserverCallback.h"
#include "IntersectionObserverEntry.h"
#include "NodeRenderStyle.h"
#include "RenderElement.h"
#include "RenderStyleInlines.h"
#include "SimpleRange.h"
#include "VisibleSelection.h"

namespace WebCore {

class ContentVisibilityIntersectionObserverCallback final : public IntersectionObserverCallback {
public:
    static Ref<ContentVisibilityIntersectionObserverCallback> create(Document& document)
    {
        return adoptRef(*new ContentVisibilityIntersectionObserverCallback(document));
    }

private:
    bool hasCallback() const final { return true; }

    CallbackResult<void> handleEvent(IntersectionObserver&, const Vector<Ref<IntersectionObserverEntry>>& entries, IntersectionObserver&) final
    {
        ASSERT(!entries.isEmpty());

        for (auto& entry : entries) {
            if (auto* element = entry->target())
                element->document().contentVisibilityDocumentState().updateViewportProximity(*element, entry->isIntersecting() ? ViewportProximity::Near : ViewportProximity::Far);
        }
        return { };
    }

    ContentVisibilityIntersectionObserverCallback(Document& document)
        : IntersectionObserverCallback(&document)
    {
    }
};

void ContentVisibilityDocumentState::observe(Element& element)
{
    Ref document = element.document();
    auto& state = document->contentVisibilityDocumentState();
    if (auto* intersectionObserver = state.intersectionObserver(document))
        intersectionObserver->observe(element);
}

void ContentVisibilityDocumentState::unobserve(Element& element)
{
    Ref document = element.document();
    auto& state = document->contentVisibilityDocumentState();
    if (auto& intersectionObserver = state.m_observer) {
        intersectionObserver->unobserve(element);
        state.removeViewportProximity(element);
    }
    element.setContentRelevancy({ });
}

IntersectionObserver* ContentVisibilityDocumentState::intersectionObserver(Document& document)
{
    if (!m_observer) {
        auto callback = ContentVisibilityIntersectionObserverCallback::create(document);
        IntersectionObserver::Init options { &document, { }, { } };
        auto observer = IntersectionObserver::create(document, WTFMove(callback), WTFMove(options));
        if (observer.hasException())
            return nullptr;
        m_observer = observer.returnValue().ptr();
    }
    return m_observer.get();
}

bool ContentVisibilityDocumentState::checkRelevancyOfContentVisibilityElement(Element& target, OptionSet<ContentRelevancy> relevancyToCheck) const
{
    auto oldRelevancy = target.contentRelevancy();
    OptionSet<ContentRelevancy> newRelevancy;
    if (oldRelevancy)
        newRelevancy = *oldRelevancy;
    auto setRelevancyValue = [&](ContentRelevancy reason, bool value) {
        if (value)
            newRelevancy.add(reason);
        else
            newRelevancy.remove(reason);
    };
    if (relevancyToCheck.contains(ContentRelevancy::OnScreen)) {
        auto viewportProximityIterator = m_elementViewportProximities.find(target);
        setRelevancyValue(ContentRelevancy::OnScreen, viewportProximityIterator->value == ViewportProximity::Near);
    }

    if (relevancyToCheck.contains(ContentRelevancy::Focused))
        setRelevancyValue(ContentRelevancy::Focused, target.hasFocusWithin());

    auto targetContainsSelection = [](Element& target) {
        auto selectionRange = target.document().selection().selection().range();
        return selectionRange && intersects<ComposedTree>(*selectionRange, target);
    };

    if (relevancyToCheck.contains(ContentRelevancy::Selected))
        setRelevancyValue(ContentRelevancy::Selected, targetContainsSelection(target));

    auto hasTopLayerinSubtree = [](const Element& target) {
        for (auto& element : target.document().topLayerElements()) {
            if (element->isDescendantOf(target))
                return true;
        }
        return false;
    };
    if (relevancyToCheck.contains(ContentRelevancy::IsInTopLayer))
        setRelevancyValue(ContentRelevancy::IsInTopLayer, hasTopLayerinSubtree(target));

    if (oldRelevancy && oldRelevancy == newRelevancy)
        return false;

    auto wasSkippedContent = target.isRelevantToUser() ? IsSkippedContent::No : IsSkippedContent::Yes;
    target.setContentRelevancy(newRelevancy);
    auto isSkippedContent = target.isRelevantToUser() ? IsSkippedContent::No : IsSkippedContent::Yes;
    target.invalidateStyle();
    updateAnimations(target, wasSkippedContent, isSkippedContent);
    if (target.isConnected()) {
        ContentVisibilityAutoStateChangeEvent::Init init;
        init.skipped = isSkippedContent == IsSkippedContent::Yes;
        target.queueTaskToDispatchEvent(TaskSource::DOMManipulation, ContentVisibilityAutoStateChangeEvent::create(eventNames().contentvisibilityautostatechangeEvent, init));
    }
    return true;
}

DidUpdateAnyContentRelevancy ContentVisibilityDocumentState::updateRelevancyOfContentVisibilityElements(OptionSet<ContentRelevancy> relevancyToCheck) const
{
    auto didUpdateAnyContentRelevancy = DidUpdateAnyContentRelevancy::No;
    for (auto target : m_observer->observationTargets()) {
        if (target) {
            if (checkRelevancyOfContentVisibilityElement(*target, relevancyToCheck))
                didUpdateAnyContentRelevancy = DidUpdateAnyContentRelevancy::Yes;
        }
    }
    return didUpdateAnyContentRelevancy;
}

HadInitialVisibleContentVisibilityDetermination ContentVisibilityDocumentState::determineInitialVisibleContentVisibility() const
{
    if (!m_observer)
        return HadInitialVisibleContentVisibilityDetermination::No;
    Vector<Ref<Element>> elementsToCheck;
    for (auto target : m_observer->observationTargets()) {
        if (target) {
            bool checkForInitialDetermination = !m_elementViewportProximities.contains(*target) && !target->isRelevantToUser();
            if (checkForInitialDetermination)
                elementsToCheck.append(*target);
        }
    }
    auto hadInitialVisibleContentVisibilityDetermination = HadInitialVisibleContentVisibilityDetermination::No;
    if (!elementsToCheck.isEmpty()) {
        elementsToCheck.first()->document().updateIntersectionObservations({ m_observer });
        for (auto& element : elementsToCheck) {
            checkRelevancyOfContentVisibilityElement(element, { ContentRelevancy::OnScreen });
            if (element->isRelevantToUser())
                hadInitialVisibleContentVisibilityDetermination = HadInitialVisibleContentVisibilityDetermination::Yes;
        }
    }
    return hadInitialVisibleContentVisibilityDetermination;
}

// Workaround for lack of support for scroll anchoring. We make sure any content-visibility: auto elements
// above the one to be scrolled to are already hidden, so the scroll position will not need to be adjusted
// later.
// FIXME: remove when scroll anchoring is implemented (https://bugs.webkit.org/show_bug.cgi?id=259269).
void ContentVisibilityDocumentState::updateContentRelevancyForScrollIfNeeded(const Element& scrollAnchor)
{
    if (!m_observer)
        return;
    auto findSkippedContentRoot = [](const Element& element) -> const Element* {
        const Element* found = nullptr;
        if (element.renderer() && element.renderer()->isSkippedContent()) {
            for (auto candidate = &element; candidate; candidate = candidate->parentElementInComposedTree()) {
                if (candidate->renderer() && candidate->renderStyle()->contentVisibility() == ContentVisibility::Auto)
                    found = candidate;
            }
        }
        return found;
    };

    if (auto* scrollAnchorRoot = findSkippedContentRoot(scrollAnchor)) {
        for (auto target : m_observer->observationTargets()) {
            if (target) {
                ASSERT(target->renderer() && target->renderStyle()->contentVisibility() == ContentVisibility::Auto);
                updateViewportProximity(*target, ViewportProximity::Far);
            }
        }
        updateViewportProximity(*scrollAnchorRoot, ViewportProximity::Near);
        scrollAnchorRoot->document().updateRelevancyOfContentVisibilityElements();
    }
}

void ContentVisibilityDocumentState::updateViewportProximity(const Element& element, ViewportProximity viewportProximity)
{
    // No need to schedule content relevancy update for first time call, since
    // that will be handled by determineInitialVisibleContentVisibility.
    if (m_elementViewportProximities.contains(element))
        element.document().scheduleContentRelevancyUpdate(ContentRelevancy::OnScreen);
    m_elementViewportProximities.ensure(element, [] {
        return ViewportProximity::Far;
    }).iterator->value = viewportProximity;
}

void ContentVisibilityDocumentState::removeViewportProximity(const Element& element)
{
    m_elementViewportProximities.remove(element);
}

void ContentVisibilityDocumentState::updateAnimations(const Element& element, IsSkippedContent wasSkipped, IsSkippedContent becomesSkipped)
{
    if (wasSkipped == IsSkippedContent::No || becomesSkipped == IsSkippedContent::Yes)
        return;
    for (auto* animation : WebAnimation::instances()) {
        if (!animation->isDeclarativeAnimation())
            continue;

        auto& declarativeAnimation = downcast<DeclarativeAnimation>(*animation);
        auto owningElement = declarativeAnimation.owningElement();
        if (!owningElement || !owningElement->element.isDescendantOrShadowDescendantOf(&element))
            continue;

        if (auto* timeline = declarativeAnimation.timeline())
            timeline->animationTimingDidChange(declarativeAnimation);
    }
}

}
