/*
 * Copyright (C) 2019 Igalia S.L.
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

#if ENABLE(RESIZE_OBSERVER)
#include "ResizeObserver.h"

#include "Element.h"
#include "InspectorInstrumentation.h"
#include "ResizeObserverEntry.h"

namespace WebCore {

Ref<ResizeObserver> ResizeObserver::create(Document& document, Ref<ResizeObserverCallback>&& callback)
{
    return adoptRef(*new ResizeObserver(document, WTFMove(callback)));
}

ResizeObserver::ResizeObserver(Document& document, Ref<ResizeObserverCallback>&& callback)
    : ActiveDOMObject(callback->scriptExecutionContext())
    , m_document(makeWeakPtr(document))
    , m_callback(WTFMove(callback))
{
    suspendIfNeeded();
}

ResizeObserver::~ResizeObserver()
{
    disconnect();
    if (m_document)
        m_document->removeResizeObserver(*this);
}

void ResizeObserver::observe(Element& target)
{
    if (!m_callback)
        return;

    auto position = m_observations.findMatching([&](auto& observation) {
        return observation->target() == &target;
    });

    if (position != notFound)
        return;

    auto& observerData = target.ensureResizeObserverData();
    observerData.observers.append(makeWeakPtr(this));

    m_observations.append(ResizeObservation::create(&target));
    m_pendingTargets.append(target);

    if (m_document) {
        m_document->addResizeObserver(*this);
        m_document->scheduleTimedRenderingUpdate();
    }
}

void ResizeObserver::unobserve(Element& target)
{
    if (!removeTarget(target))
        return;

    removeObservation(target);
}

void ResizeObserver::disconnect()
{
    removeAllTargets();
}

void ResizeObserver::targetDestroyed(Element& target)
{
    removeObservation(target);
}

size_t ResizeObserver::gatherObservations(size_t deeperThan)
{
    m_hasSkippedObservations = false;
    size_t minObservedDepth = maxElementDepth();
    for (const auto& observation : m_observations) {
        LayoutSize currentSize;
        if (observation->elementSizeChanged(currentSize)) {
            size_t depth = observation->targetElementDepth();
            if (depth > deeperThan) {
                observation->updateObservationSize(currentSize);
                m_activeObservations.append(observation.get());
                minObservedDepth = std::min(depth, minObservedDepth);
            } else
                m_hasSkippedObservations = true;
        }
    }
    return minObservedDepth;
}

void ResizeObserver::deliverObservations()
{
    Vector<Ref<ResizeObserverEntry>> entries;
    for (const auto& observation : m_activeObservations) {
        ASSERT(observation->target());
        entries.append(ResizeObserverEntry::create(observation->target(), observation->computeContentRect()));
    }
    m_activeObservations.clear();

    auto* context = m_callback->scriptExecutionContext();
    if (!context)
        return;

    InspectorInstrumentation::willFireObserverCallback(*context, "ResizeObserver"_s);
    m_callback->handleEvent(entries, *this);
    InspectorInstrumentation::didFireObserverCallback(*context);
}

bool ResizeObserver::removeTarget(Element& target)
{
    auto* observerData = target.resizeObserverData();
    if (!observerData)
        return false;

    auto& observers = observerData->observers;
    return observers.removeFirst(this);
}

void ResizeObserver::removeAllTargets()
{
    for (auto& observation : m_observations) {
        bool removed = removeTarget(*observation->target());
        ASSERT_UNUSED(removed, removed);
    }
    m_pendingTargets.clear();
    m_activeObservations.clear();
    m_observations.clear();
}

bool ResizeObserver::removeObservation(const Element& target)
{
    m_pendingTargets.removeFirstMatching([&target](auto& pendingTarget) {
        return pendingTarget.ptr() == &target;
    });

    m_activeObservations.removeFirstMatching([&target](auto& observation) {
        return observation->target() == &target;
    });

    return m_observations.removeFirstMatching([&target](auto& observation) {
        return observation->target() == &target;
    });
}

bool ResizeObserver::hasPendingActivity() const
{
    return (hasObservations() && m_document) || !m_activeObservations.isEmpty();
}

const char* ResizeObserver::activeDOMObjectName() const
{
    return "ResizeObserver";
}

void ResizeObserver::stop()
{
    disconnect();
    m_callback = nullptr;
}

} // namespace WebCore

#endif // ENABLE(RESIZE_OBSERVER)
