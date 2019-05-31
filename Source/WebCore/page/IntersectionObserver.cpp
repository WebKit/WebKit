/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if ENABLE(INTERSECTION_OBSERVER)
#include "IntersectionObserver.h"

#include "CSSParserTokenRange.h"
#include "CSSPropertyParserHelpers.h"
#include "CSSTokenizer.h"
#include "DOMWindow.h"
#include "Element.h"
#include "InspectorInstrumentation.h"
#include "IntersectionObserverCallback.h"
#include "IntersectionObserverEntry.h"
#include "Performance.h"
#include <wtf/Vector.h>

namespace WebCore {

static ExceptionOr<LengthBox> parseRootMargin(String& rootMargin)
{
    CSSTokenizer tokenizer(rootMargin);
    auto tokenRange = tokenizer.tokenRange();
    Vector<Length, 4> margins;
    while (!tokenRange.atEnd()) {
        if (margins.size() == 4)
            return Exception { SyntaxError, "Failed to construct 'IntersectionObserver': Extra text found at the end of rootMargin." };
        RefPtr<CSSPrimitiveValue> parsedValue = CSSPropertyParserHelpers::consumeLengthOrPercent(tokenRange, HTMLStandardMode, ValueRangeAll);
        if (!parsedValue || parsedValue->isCalculated())
            return Exception { SyntaxError, "Failed to construct 'IntersectionObserver': rootMargin must be specified in pixels or percent." };
        if (parsedValue->isPercentage())
            margins.append(Length(parsedValue->doubleValue(), Percent));
        else if (parsedValue->isPx())
            margins.append(Length(parsedValue->intValue(), Fixed));
        else
            return Exception { SyntaxError, "Failed to construct 'IntersectionObserver': rootMargin must be specified in pixels or percent." };
    }
    switch (margins.size()) {
    case 0:
        for (unsigned i = 0; i < 4; ++i)
            margins.append(Length(0, Fixed));
        break;
    case 1:
        for (unsigned i = 0; i < 3; ++i)
            margins.append(margins[0]);
        break;
    case 2:
        margins.append(margins[0]);
        margins.append(margins[1]);
        break;
    case 3:
        margins.append(margins[1]);
        break;
    case 4:
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    return LengthBox(WTFMove(margins[0]), WTFMove(margins[1]), WTFMove(margins[2]), WTFMove(margins[3]));
}

ExceptionOr<Ref<IntersectionObserver>> IntersectionObserver::create(Document& document, Ref<IntersectionObserverCallback>&& callback, IntersectionObserver::Init&& init)
{
    auto rootMarginOrException = parseRootMargin(init.rootMargin);
    if (rootMarginOrException.hasException())
        return rootMarginOrException.releaseException();

    Vector<double> thresholds;
    WTF::switchOn(init.threshold, [&thresholds] (double initThreshold) {
        thresholds.reserveInitialCapacity(1);
        thresholds.uncheckedAppend(initThreshold);
    }, [&thresholds] (Vector<double>& initThresholds) {
        thresholds = WTFMove(initThresholds);
    });

    for (auto threshold : thresholds) {
        if (!(threshold >= 0 && threshold <= 1))
            return Exception { RangeError, "Failed to construct 'IntersectionObserver': all thresholds must lie in the range [0.0, 1.0]." };
    }

    return adoptRef(*new IntersectionObserver(document, WTFMove(callback), init.root, rootMarginOrException.releaseReturnValue(), WTFMove(thresholds)));
}

IntersectionObserver::IntersectionObserver(Document& document, Ref<IntersectionObserverCallback>&& callback, Element* root, LengthBox&& parsedRootMargin, Vector<double>&& thresholds)
    : ActiveDOMObject(callback->scriptExecutionContext())
    , m_root(root)
    , m_rootMargin(WTFMove(parsedRootMargin))
    , m_thresholds(WTFMove(thresholds))
    , m_callback(WTFMove(callback))
{
    if (m_root) {
        auto& observerData = m_root->ensureIntersectionObserverData();
        observerData.observers.append(makeWeakPtr(this));
    } else if (auto* frame = document.frame())
        m_implicitRootDocument = makeWeakPtr(frame->mainFrame().document());

    std::sort(m_thresholds.begin(), m_thresholds.end());
    suspendIfNeeded();
}

IntersectionObserver::~IntersectionObserver()
{
    if (m_root)
        m_root->intersectionObserverData()->observers.removeFirst(this);
    disconnect();
}

String IntersectionObserver::rootMargin() const
{
    StringBuilder stringBuilder;
    PhysicalBoxSide sides[4] = { PhysicalBoxSide::Top, PhysicalBoxSide::Right, PhysicalBoxSide::Bottom, PhysicalBoxSide::Left };
    for (auto side : sides) {
        auto& length = m_rootMargin.at(side);
        stringBuilder.appendNumber(length.intValue());
        if (length.type() == Percent)
            stringBuilder.append('%');
        else
            stringBuilder.appendLiteral("px");
        if (side != PhysicalBoxSide::Left)
            stringBuilder.append(' ');
    }
    return stringBuilder.toString();
}

void IntersectionObserver::observe(Element& target)
{
    if (!trackingDocument() || !m_callback || m_observationTargets.contains(&target))
        return;

    target.ensureIntersectionObserverData().registrations.append({ makeWeakPtr(this), WTF::nullopt });
    bool hadObservationTargets = hasObservationTargets();
    m_observationTargets.append(&target);
    auto* document = trackingDocument();
    if (!hadObservationTargets)
        document->addIntersectionObserver(*this);
    document->scheduleInitialIntersectionObservationUpdate();
}

void IntersectionObserver::unobserve(Element& target)
{
    if (!removeTargetRegistration(target))
        return;

    bool removed = m_observationTargets.removeFirst(&target);
    ASSERT_UNUSED(removed, removed);

    if (!hasObservationTargets()) {
        if (auto* document = trackingDocument())
            document->removeIntersectionObserver(*this);
    }
}

void IntersectionObserver::disconnect()
{
    if (!hasObservationTargets())
        return;

    removeAllTargets();
    if (auto* document = trackingDocument())
        document->removeIntersectionObserver(*this);
}

auto IntersectionObserver::takeRecords() -> TakenRecords
{
    return { WTFMove(m_queuedEntries), WTFMove(m_pendingTargets) };
}

void IntersectionObserver::targetDestroyed(Element& target)
{
    m_observationTargets.removeFirst(&target);
    if (!hasObservationTargets()) {
        if (auto* document = trackingDocument())
            document->removeIntersectionObserver(*this);
    }
}

bool IntersectionObserver::removeTargetRegistration(Element& target)
{
    auto* observerData = target.intersectionObserverData();
    if (!observerData)
        return false;

    auto& registrations = observerData->registrations;
    return registrations.removeFirstMatching([this](auto& registration) {
        return registration.observer.get() == this;
    });
}

void IntersectionObserver::removeAllTargets()
{
    for (auto* target : m_observationTargets) {
        bool removed = removeTargetRegistration(*target);
        ASSERT_UNUSED(removed, removed);
    }
    m_observationTargets.clear();
}

void IntersectionObserver::rootDestroyed()
{
    ASSERT(m_root);
    disconnect();
    m_root = nullptr;
}

bool IntersectionObserver::createTimestamp(DOMHighResTimeStamp& timestamp) const
{
    if (!m_callback)
        return false;

    auto* context = m_callback->scriptExecutionContext();
    if (!context)
        return false;
    ASSERT(context->isDocument());
    auto& document = downcast<Document>(*context);
    if (auto* window = document.domWindow()) {
        timestamp =  window->performance().now();
        return true;
    }
    return false;
}

void IntersectionObserver::appendQueuedEntry(Ref<IntersectionObserverEntry>&& entry)
{
    ASSERT(entry->target());
    m_pendingTargets.append(*entry->target());
    m_queuedEntries.append(WTFMove(entry));
}

void IntersectionObserver::notify()
{
    if (m_queuedEntries.isEmpty()) {
        ASSERT(m_pendingTargets.isEmpty());
        return;
    }

    auto* context = m_callback->scriptExecutionContext();
    if (!context)
        return;

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willFireObserverCallback(*context, "IntersectionObserver"_s);

    auto takenRecords = takeRecords();
    m_callback->handleEvent(WTFMove(takenRecords.records), *this);

    InspectorInstrumentation::didFireObserverCallback(cookie);
}

bool IntersectionObserver::hasPendingActivity() const
{
    return (hasObservationTargets() && trackingDocument()) || !m_queuedEntries.isEmpty();
}

const char* IntersectionObserver::activeDOMObjectName() const
{
    return "IntersectionObserver";
}

bool IntersectionObserver::canSuspendForDocumentSuspension() const
{
    return true;
}

void IntersectionObserver::stop()
{
    disconnect();
    m_callback = nullptr;
    m_queuedEntries.clear();
    m_pendingTargets.clear();
}

} // namespace WebCore

#endif // ENABLE(INTERSECTION_OBSERVER)
