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

#include "Element.h"
#include "IntersectionObserverCallback.h"
#include "IntersectionObserverEntry.h"
#include "NodeRenderStyle.h"
#include "RenderElement.h"
#include "RenderStyleInlines.h"

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
            if (auto* element = entry->target()) {
                element->contentVisibilityViewportChange(entry->isIntersecting());
                element->document().contentVisibilityDocumentState().updateOnScreenObservationTarget(*element, entry->isIntersecting());
            }
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
    auto& state = element.document().contentVisibilityDocumentState();
    if (auto* intersectionObserver = state.intersectionObserver(element.document()))
        intersectionObserver->observe(element);
}

void ContentVisibilityDocumentState::unobserve(Element& element)
{
    auto& state = element.document().contentVisibilityDocumentState();
    if (auto& intersectionObserver = state.m_observer) {
        intersectionObserver->unobserve(element);
        state.updateOnScreenObservationTarget(element, false);
    }
    element.setContentRelevancyStatus({ });
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

bool ContentVisibilityDocumentState::updateRelevancyOfContentVisibilityElements(const OptionSet<ContentRelevancyStatus>& relevancyToCheck)
{
    bool didUpdateAnyContentRelevancy = false;
    for (auto target : m_observer->observationTargets()) {
        if (target) {
            auto oldRelevancy = target->contentRelevancyStatus();
            auto newRelevancy = oldRelevancy;
            auto setRelevancyValue = [&](ContentRelevancyStatus reason, bool value) {
                if (value)
                    newRelevancy.add(reason);
                else
                    newRelevancy.remove(reason);
            };
            if (relevancyToCheck.contains(ContentRelevancyStatus::OnScreen))
                setRelevancyValue(ContentRelevancyStatus::OnScreen, m_onScreenObservationTargets.contains(*target));

            if (relevancyToCheck.contains(ContentRelevancyStatus::Focused))
                setRelevancyValue(ContentRelevancyStatus::Focused, target->hasFocusWithin());

            auto hasTopLayerinSubtree = [](const Element& target) {
                for (auto& element : target.document().topLayerElements()) {
                    if (element->isDescendantOf(target))
                        return true;
                }
                return false;
            };
            if (relevancyToCheck.contains(ContentRelevancyStatus::IsInTopLayer))
                setRelevancyValue(ContentRelevancyStatus::IsInTopLayer, hasTopLayerinSubtree(*target));

            if (oldRelevancy == newRelevancy)
                continue;
            target->setContentRelevancyStatus(newRelevancy);
            target->invalidateStyle();
            didUpdateAnyContentRelevancy = true;
        }
    }
    return didUpdateAnyContentRelevancy;
}

// Workaround for lack of support for scroll anchoring. We make sure any content-visibility: auto elements
// above the one to be scrolled to are already hidden, so the scroll position will not need to be adjusted
// later.
// FIXME: remove when scroll anchoring is implemented.
void ContentVisibilityDocumentState::updateContentRelevancyStatusForScrollIfNeeded(const Element& scrollAnchor)
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
                updateOnScreenObservationTarget(*target, false);
            }
        }
        updateOnScreenObservationTarget(*scrollAnchorRoot, true);
        scrollAnchorRoot->document().scheduleContentRelevancyUpdate(ContentRelevancyStatus::OnScreen);
        scrollAnchorRoot->document().updateRelevancyOfContentVisibilityElements();
    }
}

void ContentVisibilityDocumentState::updateOnScreenObservationTarget(const Element& element, bool onScreen)
{
    if (onScreen)
        m_onScreenObservationTargets.add(element);
    else
        m_onScreenObservationTargets.remove(element);
}

}
