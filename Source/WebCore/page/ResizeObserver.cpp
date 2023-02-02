/*
 * Copyright (C) 2019 Igalia S.L.
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

#include "ResizeObserver.h"

#include "Element.h"
#include "InspectorInstrumentation.h"
#include "JSNodeCustom.h"
#include "Logging.h"
#include "ResizeObserverEntry.h"
#include "ResizeObserverOptions.h"
#include "WebCoreOpaqueRoot.h"
#include <JavaScriptCore/AbstractSlotVisitorInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<ResizeObserver> ResizeObserver::create(Document& document, Ref<ResizeObserverCallback>&& callback)
{
    return adoptRef(*new ResizeObserver(document, WTFMove(callback)));
}

ResizeObserver::ResizeObserver(Document& document, Ref<ResizeObserverCallback>&& callback)
    : m_document(document)
    , m_callback(WTFMove(callback))
{
}

ResizeObserver::~ResizeObserver()
{
    disconnect();
    if (m_document)
        m_document->removeResizeObserver(*this);
}

// https://drafts.csswg.org/resize-observer/#dom-resizeobserver-observe
void ResizeObserver::observe(Element& target, const ResizeObserverOptions& options)
{
    if (!m_callback)
        return;

    auto position = m_observations.findIf([&](auto& observation) {
        return observation->target() == &target;
    });

    if (position != notFound) {
        // The spec suggests unconditionally unobserving here, but that causes a test failure:
        // https://github.com/web-platform-tests/wpt/issues/30708
        if (m_observations[position]->observedBox() == options.box)
            return;

        unobserve(target);
    }

    auto& observerData = target.ensureResizeObserverData();
    observerData.observers.append(*this);

    m_observations.append(ResizeObservation::create(target, options.box));

    // Per the specification, we should dispatch at least one observation for the target. For this reason, we make sure to keep the
    // target alive until this first observation. This, in turn, will keep the ResizeObserver's JS wrapper alive via
    // isReachableFromOpaqueRoots(), so the callback stays alive.
    m_targetsWaitingForFirstObservation.append(target);

    if (m_document) {
        m_document->addResizeObserver(*this);
        m_document->scheduleRenderingUpdate(RenderingUpdateStep::ResizeObservations);
    }
}

// https://drafts.csswg.org/resize-observer/#dom-resizeobserver-unobserve
void ResizeObserver::unobserve(Element& target)
{
    if (!removeTarget(target))
        return;

    removeObservation(target);
}

// https://drafts.csswg.org/resize-observer/#dom-resizeobserver-disconnect
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
        if (auto currentSizes = observation->elementSizeChanged()) {
            size_t depth = observation->targetElementDepth();
            if (depth > deeperThan) {
                observation->updateObservationSize(*currentSizes);

                LOG_WITH_STREAM(ResizeObserver, stream << "ResizeObserver " << this << " gatherObservations - recording observation " << observation.get());

                m_activeObservations.append(observation.get());
                m_activeObservationTargets.append(*observation->target());
                minObservedDepth = std::min(depth, minObservedDepth);
            } else
                m_hasSkippedObservations = true;
        }
    }
    return minObservedDepth;
}

void ResizeObserver::deliverObservations()
{
    LOG_WITH_STREAM(ResizeObserver, stream << "ResizeObserver " << this << " deliverObservations");

    auto entries = m_activeObservations.map([](auto& observation) {
        ASSERT(observation->target());
        return ResizeObserverEntry::create(observation->target(), observation->computeContentRect(), observation->borderBoxSize(), observation->contentBoxSize());
    });
    m_activeObservations.clear();
    auto activeObservationTargets = std::exchange(m_activeObservationTargets, { });

    auto targetsWaitingForFirstObservation = std::exchange(m_targetsWaitingForFirstObservation, { });

    // FIXME: The JSResizeObserver wrapper should be kept alive as long as the resize observer can fire events.
    ASSERT(m_callback->hasCallback());
    if (!m_callback->hasCallback())
        return;

    auto* context = m_callback->scriptExecutionContext();
    if (!context)
        return;

    InspectorInstrumentation::willFireObserverCallback(*context, "ResizeObserver"_s);
    m_callback->handleEvent(*this, entries, *this);
    InspectorInstrumentation::didFireObserverCallback(*context);
}

bool ResizeObserver::isReachableFromOpaqueRoots(JSC::AbstractSlotVisitor& visitor) const
{
    for (auto& observation : m_observations) {
        if (auto* target = observation->target(); target && containsWebCoreOpaqueRoot(visitor, target))
            return true;
    }
    for (auto& target : m_activeObservationTargets) {
        if (containsWebCoreOpaqueRoot(visitor, target.get()))
            return true;
    }
    return !m_targetsWaitingForFirstObservation.isEmpty();
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
    m_activeObservationTargets.clear();
    m_activeObservations.clear();
    m_targetsWaitingForFirstObservation.clear();
    m_observations.clear();
}

bool ResizeObserver::removeObservation(const Element& target)
{
    m_targetsWaitingForFirstObservation.removeFirstMatching([&target](auto& pendingTarget) {
        return pendingTarget.ptr() == &target;
    });
    return m_observations.removeFirstMatching([&target](auto& observation) {
        return observation->target() == &target;
    });
}

} // namespace WebCore
