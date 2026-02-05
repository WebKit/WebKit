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

#include "ContextDestructionObserverInlines.h"
#include "Element.h"
#include "InspectorInstrumentation.h"
#include "JSNodeCustom.h"
#include "Logging.h"
#include "ResizeObserverEntry.h"
#include "ResizeObserverOptions.h"
#include "WebCoreOpaqueRootInlines.h"
#include <JavaScriptCore/AbstractSlotVisitorInlines.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<ResizeObserver> ResizeObserver::create(Document& document, Ref<ResizeObserverCallback>&& callback)
{
    return adoptRef(*new ResizeObserver(document, { RefPtr<ResizeObserverCallback> { WTF::move(callback) } }));
}

Ref<ResizeObserver> ResizeObserver::createNativeObserver(Document& document, NativeResizeObserverCallback&& nativeCallback)
{
    return adoptRef(*new ResizeObserver(document, { WTF::move(nativeCallback) }));
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(ResizeObserver);

ResizeObserver::ResizeObserver(Document& document, JSOrNativeResizeObserverCallback&& callback)
    : m_document(document)
    , m_JSOrNativeCallback(WTF::move(callback))
{
}

ResizeObserver::~ResizeObserver()
{
    disconnect();
    if (m_document)
        m_document->removeResizeObserver(*this);
}

void ResizeObserver::observeInternal(Element& target, const ResizeObserverBoxOptions boxOptions)
{
    ASSERT(!m_JSOrNativeCallback.valueless_by_exception());

    auto position = m_observations.findIf([&](auto& observation) {
        return observation->target() == &target;
    });

    if (position != notFound) {
        // The spec suggests unconditionally unobserving here, but that causes a test failure:
        // https://github.com/web-platform-tests/wpt/issues/30708
        if (m_observations[position]->observedBox() == boxOptions)
            return;

        unobserve(target);
    }

    auto& observerData = target.ensureResizeObserverData();
    observerData.observers.append(*this);

    m_observations.append(ResizeObservation::create(target, boxOptions));

    // Per the specification, we should dispatch at least one observation for the target. For this reason, we make sure to keep the
    // target alive until this first observation. This, in turn, will keep the ResizeObserver's JS wrapper alive via
    // isReachableFromOpaqueRoots(), so the callback stays alive.
    {
        Locker locker { m_observationTargetsLock };
        m_targetsWaitingForFirstObservation.append(target);
    }

    if (m_document && isJSCallback()) {
        m_document->addResizeObserver(*this);
        m_document->scheduleRenderingUpdate(RenderingUpdateStep::ResizeObservations);
    }
}

// https://drafts.csswg.org/resize-observer/#dom-resizeobserver-observe
void ResizeObserver::observe(Element& target, const ResizeObserverOptions& options)
{
    observeInternal(target, options.box);
}

void ResizeObserver::observe(Element& target)
{
    observeInternal(target, ResizeObserverBoxOptions::ContentBox);
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
                {
                    Locker locker { m_observationTargetsLock };
                    m_activeObservationTargets.append(*observation->protectedTarget());
                }
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

    auto entries = WTF::compactMap(m_activeObservations, [](auto& observation) -> RefPtr<ResizeObserverEntry> {
        RefPtr target = observation->target();
        ASSERT(target); // The target is supposed to be kept alive via `m_activeObservationTargets` and JSResizeObserver::visitAdditionalChildren().
        if (!target)
            return nullptr;
        return ResizeObserverEntry::create(target.releaseNonNull(), observation->computeContentRect(), observation->borderBoxSize(), observation->contentBoxSize());
    });
    m_activeObservations.clear();

    // Use GCReachableRef here to make sure the targets and their JS wrappers are kept alive while we deliver.
    // It is important since m_activeObservationTargets / m_targetsWaitingForFirstObservation will get cleared and
    // thus JSResizeObserver::visitAdditionalChildren() won't be able to visit them on the GC thread.
    Vector<GCReachableRef<Element>> activeObservationTargets;
    Vector<GCReachableRef<Element>> targetsWaitingForFirstObservation;
    {
        Locker locker { m_observationTargetsLock };
        activeObservationTargets = WTF::compactMap(m_activeObservationTargets, [](auto& weakTarget) -> std::optional<GCReachableRef<Element>> {
            if (weakTarget)
                return GCReachableRef<Element> { *weakTarget };
            ASSERT_NOT_REACHED(); // Targets are supposed to be kept alive via JSResizeObserver::visitAdditionalChildren().
            return std::nullopt;
        });
        m_activeObservationTargets = { };
        targetsWaitingForFirstObservation = WTF::compactMap(m_targetsWaitingForFirstObservation, [](auto& weakTarget) -> std::optional<GCReachableRef<Element>> {
            if (weakTarget)
                return GCReachableRef<Element> { *weakTarget };
            ASSERT_NOT_REACHED(); // Targets are supposed to be kept alive via JSResizeObserver::visitAdditionalChildren().
            return std::nullopt;
        });
        m_targetsWaitingForFirstObservation = { };
    }

    if (isNativeCallback()) {
        std::get<NativeResizeObserverCallback>(m_JSOrNativeCallback)(entries, *this);
        return;
    }

    // FIXME: The JSResizeObserver wrapper should be kept alive as long as the resize observer can fire events.
    ASSERT(isJSCallback());
    auto jsCallback = std::get<RefPtr<ResizeObserverCallback>>(m_JSOrNativeCallback);
    ASSERT(jsCallback->hasCallback());
    if (!jsCallback->hasCallback())
        return;

    RefPtr context = jsCallback->scriptExecutionContext();
    if (!context)
        return;

    InspectorInstrumentation::willFireObserverCallback(*context, "ResizeObserver"_s);
    jsCallback->invoke(*this, entries, *this);
    InspectorInstrumentation::didFireObserverCallback(*context);
}

bool ResizeObserver::isReachableFromOpaqueRoots(JSC::AbstractSlotVisitor& visitor) const
{
    for (auto& observation : m_observations) {
        if (SUPPRESS_UNCOUNTED_LOCAL auto* target = observation->target(); target && containsWebCoreOpaqueRoot(visitor, target))
            return true;
    }

    Locker locker { m_observationTargetsLock };

    for (const auto& weakTarget : m_activeObservationTargets) {
        RefPtr target = weakTarget.get();
        if (target && containsWebCoreOpaqueRoot(visitor, target.get()))
            return true;
    }
    for (const auto& weakTarget : m_targetsWaitingForFirstObservation) {
        if (SUPPRESS_UNCOUNTED_LOCAL auto* element = weakTarget.get(); element && containsWebCoreOpaqueRoot(visitor, element))
            return true;
    }
    return false;
}

bool ResizeObserver::removeTarget(Element& target)
{
    auto* observerData = target.resizeObserverDataIfExists();
    if (!observerData)
        return false;

    auto& observers = observerData->observers;
    return observers.removeFirst(this);
}

void ResizeObserver::removeAllTargets()
{
    for (auto& observation : m_observations) {
        bool removed = removeTarget(*observation->protectedTarget());
        ASSERT_UNUSED(removed, removed);
    }
    {
        Locker locker { m_observationTargetsLock };
        m_activeObservationTargets.clear();
        m_targetsWaitingForFirstObservation.clear();
    }
    m_activeObservations.clear();
    m_observations.clear();
}

bool ResizeObserver::removeObservation(const Element& target)
{
    {
        Locker locker { m_observationTargetsLock };
        m_targetsWaitingForFirstObservation.removeFirstMatching([&target](auto& pendingTarget) {
            return pendingTarget.get() == &target;
        });
    }
    return m_observations.removeFirstMatching([&target](auto& observation) {
        return observation->target() == &target;
    });
}

bool ResizeObserver::isJSCallback()
{
    return std::holds_alternative<RefPtr<ResizeObserverCallback>>(m_JSOrNativeCallback);
}

bool ResizeObserver::isNativeCallback()
{
    return std::holds_alternative<NativeResizeObserverCallback>(m_JSOrNativeCallback);
}

ResizeObserverCallback* ResizeObserver::callbackConcurrently()
{
    return WTF::switchOn(m_JSOrNativeCallback,
    [] (const RefPtr<ResizeObserverCallback>& jsCallback) -> ResizeObserverCallback* {
        return jsCallback.get();
    },
    [] (const NativeResizeObserverCallback&) -> ResizeObserverCallback* {
        return nullptr;
    });
}

void ResizeObserver::resetObservationSize(Element& target)
{
    auto position = m_observations.findIf([&](auto& observation) {
        return observation->target() == &target;
    });

    if (position != notFound)
        m_observations[position]->resetObservationSize();
}

} // namespace WebCore
