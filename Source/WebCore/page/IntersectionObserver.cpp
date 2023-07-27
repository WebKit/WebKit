/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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

#include "IntersectionObserver.h"

#include "CSSParserTokenRange.h"
#include "CSSPropertyParserHelpers.h"
#include "CSSTokenizer.h"
#include "Element.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameView.h"
#include "InspectorInstrumentation.h"
#include "IntersectionObserverCallback.h"
#include "IntersectionObserverEntry.h"
#include "JSNodeCustom.h"
#include "LocalDOMWindow.h"
#include "Logging.h"
#include "Performance.h"
#include "RenderBlock.h"
#include "RenderInline.h"
#include "RenderLineBreak.h"
#include "RenderView.h"
#include "WebCoreOpaqueRootInlines.h"
#include <JavaScriptCore/AbstractSlotVisitorInlines.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/Vector.h>

namespace WebCore {

static ExceptionOr<LengthBox> parseRootMargin(String& rootMargin)
{
    CSSTokenizer tokenizer(rootMargin);
    auto tokenRange = tokenizer.tokenRange();
    Vector<Length, 4> margins;
    while (!tokenRange.atEnd()) {
        if (margins.size() == 4)
            return Exception { SyntaxError, "Failed to construct 'IntersectionObserver': Extra text found at the end of rootMargin."_s };
        RefPtr<CSSPrimitiveValue> parsedValue = CSSPropertyParserHelpers::consumeLengthOrPercent(tokenRange, HTMLStandardMode, ValueRange::All);
        if (!parsedValue || parsedValue->isCalculated())
            return Exception { SyntaxError, "Failed to construct 'IntersectionObserver': rootMargin must be specified in pixels or percent."_s };
        if (parsedValue->isPercentage())
            margins.append(Length(parsedValue->doubleValue(), LengthType::Percent));
        else if (parsedValue->isPx())
            margins.append(Length(parsedValue->intValue(), LengthType::Fixed));
        else
            return Exception { SyntaxError, "Failed to construct 'IntersectionObserver': rootMargin must be specified in pixels or percent."_s };
    }
    switch (margins.size()) {
    case 0:
        for (unsigned i = 0; i < 4; ++i)
            margins.append(Length(0, LengthType::Fixed));
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
    RefPtr<ContainerNode> root;
    if (init.root) {
        WTF::switchOn(*init.root, [&root] (RefPtr<Element> element) {
            root = element.get();
        }, [&root] (RefPtr<Document> document) {
            root = document.get();
        });
    }

    auto rootMarginOrException = parseRootMargin(init.rootMargin);
    if (rootMarginOrException.hasException())
        return rootMarginOrException.releaseException();

    Vector<double> thresholds;
    WTF::switchOn(init.threshold, [&thresholds] (double initThreshold) {
        thresholds.append(initThreshold);
    }, [&thresholds] (Vector<double>& initThresholds) {
        thresholds = WTFMove(initThresholds);
    });

    for (auto threshold : thresholds) {
        if (!(threshold >= 0 && threshold <= 1))
            return Exception { RangeError, "Failed to construct 'IntersectionObserver': all thresholds must lie in the range [0.0, 1.0]."_s };
    }

    return adoptRef(*new IntersectionObserver(document, WTFMove(callback), root.get(), rootMarginOrException.releaseReturnValue(), WTFMove(thresholds)));
}

WTF_MAKE_ISO_ALLOCATED_IMPL(IntersectionObserver);

IntersectionObserver::IntersectionObserver(Document& document, Ref<IntersectionObserverCallback>&& callback, ContainerNode* root, LengthBox&& parsedRootMargin, Vector<double>&& thresholds)
    : m_root(root)
    , m_rootMargin(WTFMove(parsedRootMargin))
    , m_thresholds(WTFMove(thresholds))
    , m_callback(WTFMove(callback))
{
    if (is<Document>(root)) {
        auto& observerData = downcast<Document>(*root).ensureIntersectionObserverData();
        observerData.observers.append(*this);
    } else if (root) {
        auto& observerData = downcast<Element>(*root).ensureIntersectionObserverData();
        observerData.observers.append(*this);
    } else if (auto* frame = document.frame()) {
        if (auto* localFrame = dynamicDowncast<LocalFrame>(frame->mainFrame()))
            m_implicitRootDocument = localFrame->document();
    }

    std::sort(m_thresholds.begin(), m_thresholds.end());
    
    LOG_WITH_STREAM(IntersectionObserver, stream << "Created IntersectionObserver " << this << " root " << root << " root margin " << m_rootMargin << " thresholds " << m_thresholds);
}

IntersectionObserver::~IntersectionObserver()
{
    RefPtr root = m_root.get();
    if (is<Document>(root))
        downcast<Document>(*root).intersectionObserverDataIfExists()->observers.removeFirst(this);
    else if (root)
        downcast<Element>(*root).intersectionObserverDataIfExists()->observers.removeFirst(this);
    disconnect();
}

String IntersectionObserver::rootMargin() const
{
    StringBuilder stringBuilder;
    for (auto side : allBoxSides) {
        auto& length = m_rootMargin.at(side);
        stringBuilder.append(length.intValue(), length.isPercent() ? "%" : "px", side != BoxSide::Left ? " " : "");
    }
    return stringBuilder.toString();
}

bool IntersectionObserver::isObserving(const Element& element) const
{
    return m_observationTargets.findIf([&](auto& target) {
        return target.get() == &element;
    }) != notFound;
}

void IntersectionObserver::observe(Element& target)
{
    if (!trackingDocument() || !m_callback || isObserving(target))
        return;

    target.ensureIntersectionObserverData().registrations.append({ *this, std::nullopt });
    bool hadObservationTargets = hasObservationTargets();
    m_observationTargets.append(target);

    // Per the specification, we should dispatch at least one observation for the target. For this reason, we make sure to keep the
    // target alive until this first observation. This, in turn, will keep the IntersectionObserver's JS wrapper alive via
    // isReachableFromOpaqueRoots(), so the callback stays alive.
    m_targetsWaitingForFirstObservation.append(target);

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
    m_targetsWaitingForFirstObservation.removeFirstMatching([&](auto& pendingTarget) { return pendingTarget.ptr() == &target; });

    if (!hasObservationTargets()) {
        if (auto* document = trackingDocument())
            document->removeIntersectionObserver(*this);
    }
}

void IntersectionObserver::disconnect()
{
    if (!hasObservationTargets()) {
        ASSERT(m_targetsWaitingForFirstObservation.isEmpty());
        return;
    }

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
    m_targetsWaitingForFirstObservation.removeFirstMatching([&](auto& pendingTarget) { return pendingTarget.ptr() == &target; });
    if (!hasObservationTargets()) {
        if (auto* document = trackingDocument())
            document->removeIntersectionObserver(*this);
    }
}

bool IntersectionObserver::removeTargetRegistration(Element& target)
{
    auto* observerData = target.intersectionObserverDataIfExists();
    if (!observerData)
        return false;

    auto& registrations = observerData->registrations;
    return registrations.removeFirstMatching([this](auto& registration) {
        return registration.observer.get() == this;
    });
}

void IntersectionObserver::removeAllTargets()
{
    for (auto& target : m_observationTargets) {
        bool removed = removeTargetRegistration(*target);
        ASSERT_UNUSED(removed, removed);
    }
    m_observationTargets.clear();
    m_targetsWaitingForFirstObservation.clear();
}

void IntersectionObserver::rootDestroyed()
{
    ASSERT(m_root);
    disconnect();
    m_root = nullptr;
}

struct IntersectionObservationState {
    FloatRect absoluteTargetRect;
    FloatRect absoluteRootBounds;
    FloatRect absoluteIntersectionRect;
    bool isIntersecting { false };
};

static void expandRootBoundsWithRootMargin(FloatRect& localRootBounds, const LengthBox& rootMargin, float zoomFactor)
{
    auto zoomAdjustedLength = [](const Length& length, float maximumValue, float zoomFactor) {
        if (length.isPercent())
            return floatValueForLength(length, maximumValue);

        return floatValueForLength(length, maximumValue) * zoomFactor;
    };

    auto rootMarginEdges = FloatBoxExtent {
        zoomAdjustedLength(rootMargin.top(), localRootBounds.height(), zoomFactor),
        zoomAdjustedLength(rootMargin.right(), localRootBounds.width(), zoomFactor),
        zoomAdjustedLength(rootMargin.bottom(), localRootBounds.height(), zoomFactor),
        zoomAdjustedLength(rootMargin.left(), localRootBounds.width(), zoomFactor)
    };

    localRootBounds.expand(rootMarginEdges);
}

static std::optional<LayoutRect> computeClippedRectInRootContentsSpace(const LayoutRect& rect, const RenderElement* renderer)
{
    OptionSet<RenderObject::VisibleRectContextOption> visibleRectOptions = { RenderObject::VisibleRectContextOption::UseEdgeInclusiveIntersection, RenderObject::VisibleRectContextOption::ApplyCompositedClips, RenderObject::VisibleRectContextOption::ApplyCompositedContainerScrolls };
    std::optional<LayoutRect> rectInFrameAbsoluteSpace = renderer->computeVisibleRectInContainer(rect, &renderer->view(), { false /* hasPositionFixedDescendant */, false /* dirtyRectIsFlipped */, visibleRectOptions });
    if (!rectInFrameAbsoluteSpace || renderer->frame().isMainFrame())
        return rectInFrameAbsoluteSpace;

    bool intersects = rectInFrameAbsoluteSpace->edgeInclusiveIntersect(renderer->view().frameView().layoutViewportRect());
    if (!intersects)
        return std::nullopt;

    auto* ownerRenderer = renderer->frame().ownerRenderer();
    if (!ownerRenderer)
        return std::nullopt;

    LayoutRect rectInFrameViewSpace { renderer->view().frameView().contentsToView(*rectInFrameAbsoluteSpace) };

    rectInFrameViewSpace.moveBy(ownerRenderer->contentBoxLocation());
    return computeClippedRectInRootContentsSpace(rectInFrameViewSpace, ownerRenderer);
}

static std::optional<IntersectionObservationState> computeIntersectionState(LocalFrameView& frameView, const IntersectionObserver& observer, Element& target, bool applyRootMargin)
{
    auto* targetRenderer = target.renderer();
    if (!targetRenderer)
        return std::nullopt;

    FloatRect localRootBounds;
    RenderBlock* rootRenderer;
    if (observer.root()) {
        if (observer.trackingDocument() != &target.document())
            return std::nullopt;

        if (!observer.root()->renderer() || !is<RenderBlock>(observer.root()->renderer()))
            return std::nullopt;

        rootRenderer = downcast<RenderBlock>(observer.root()->renderer());
        if (!rootRenderer->isContainingBlockAncestorFor(*targetRenderer))
            return std::nullopt;

        if (observer.root() == &target.document())
            localRootBounds = frameView.layoutViewportRect();
        else if (rootRenderer->hasNonVisibleOverflow())
            localRootBounds = rootRenderer->contentBoxRect();
        else
            localRootBounds = { FloatPoint(), rootRenderer->size() };
    } else {
        ASSERT(is<LocalFrame>(frameView.frame()) && downcast<LocalFrame>(frameView.frame()).isMainFrame());
        // FIXME: Handle the case of an implicit-root observer that has a target in a different frame tree.
        if (dynamicDowncast<LocalFrame>(targetRenderer->frame().mainFrame()) != &frameView.frame())
            return std::nullopt;
        rootRenderer = frameView.renderView();
        localRootBounds = frameView.layoutViewportRect();
    }

    if (applyRootMargin)
        expandRootBoundsWithRootMargin(localRootBounds, observer.rootMarginBox(), rootRenderer->style().effectiveZoom());

    LayoutRect localTargetBounds;
    if (is<RenderBox>(*targetRenderer))
        localTargetBounds = downcast<RenderBox>(targetRenderer)->borderBoundingBox();
    else if (is<RenderInline>(targetRenderer)) {
        auto pair = target.boundingAbsoluteRectWithoutLayout();
        if (pair) {
            FloatRect absoluteTargetBounds = pair->second;
            localTargetBounds = enclosingLayoutRect(targetRenderer->absoluteToLocalQuad(absoluteTargetBounds).boundingBox());
        }
    } else if (is<RenderLineBreak>(targetRenderer))
        localTargetBounds = downcast<RenderLineBreak>(targetRenderer)->linesBoundingBox();

    std::optional<LayoutRect> rootLocalTargetRect;
    if (observer.root()) {
        OptionSet<RenderObject::VisibleRectContextOption> visibleRectOptions = { RenderObject::VisibleRectContextOption::UseEdgeInclusiveIntersection, RenderObject::VisibleRectContextOption::ApplyCompositedClips, RenderObject::VisibleRectContextOption::ApplyCompositedContainerScrolls };
        rootLocalTargetRect = targetRenderer->computeVisibleRectInContainer(localTargetBounds, rootRenderer, { false /* hasPositionFixedDescendant */, false /* dirtyRectIsFlipped */, visibleRectOptions });
    } else
        rootLocalTargetRect = computeClippedRectInRootContentsSpace(localTargetBounds, targetRenderer);

    FloatRect rootLocalIntersectionRect = localRootBounds;

    IntersectionObservationState intersectionState;
    intersectionState.isIntersecting = rootLocalTargetRect && rootLocalIntersectionRect.edgeInclusiveIntersect(*rootLocalTargetRect) && !targetRenderer->isSkippedContent();
    intersectionState.absoluteTargetRect = targetRenderer->localToAbsoluteQuad(FloatRect(localTargetBounds)).boundingBox();
    intersectionState.absoluteRootBounds = rootRenderer->localToAbsoluteQuad(localRootBounds).boundingBox();

    if (intersectionState.isIntersecting) {
        FloatRect rootAbsoluteIntersectionRect = rootRenderer->localToAbsoluteQuad(rootLocalIntersectionRect).boundingBox();
        if (&targetRenderer->frame() == &rootRenderer->frame())
            intersectionState.absoluteIntersectionRect = rootAbsoluteIntersectionRect;
        else {
            FloatRect rootViewIntersectionRect = frameView.contentsToView(rootAbsoluteIntersectionRect);
            intersectionState.absoluteIntersectionRect = targetRenderer->view().frameView().rootViewToContents(rootViewIntersectionRect);
        }
        intersectionState.isIntersecting = intersectionState.absoluteIntersectionRect.edgeInclusiveIntersect(intersectionState.absoluteTargetRect);
    }

    return intersectionState;
}

auto IntersectionObserver::updateObservations(Document& hostDocument) -> NeedNotify
{
    RefPtr frameView = hostDocument.view();
    if (!frameView)
        return NeedNotify::No;

    auto timestamp = nowTimestamp();
    if (!timestamp)
        return NeedNotify::No;

    auto needNotify = NeedNotify::No;

    for (auto& target : observationTargets()) {
        auto& targetRegistrations = target->intersectionObserverDataIfExists()->registrations;
        auto index = targetRegistrations.findIf([&](auto& registration) {
            return registration.observer.get() == this;
        });
        ASSERT(index != notFound);
        auto& registration = targetRegistrations[index];

        bool isSameOriginObservation = &target->document() == &hostDocument || target->document().securityOrigin().isSameOriginDomain(hostDocument.securityOrigin());
        auto intersectionState = computeIntersectionState(*frameView, *this, *target, isSameOriginObservation);

        float intersectionRatio = 0;
        size_t thresholdIndex = 0;
        if (intersectionState) {
            if (intersectionState->isIntersecting) {
                float absTargetArea = intersectionState->absoluteTargetRect.area();
                if (absTargetArea)
                    intersectionRatio = intersectionState->absoluteIntersectionRect.area() / absTargetArea;
                else
                    intersectionRatio = 1;

                for (auto threshold : thresholds()) {
                    if (!(threshold <= intersectionRatio || WTF::areEssentiallyEqual<float>(threshold, intersectionRatio)))
                        break;
                    ++thresholdIndex;
                }
            }
        }

        if (!registration.previousThresholdIndex || thresholdIndex != registration.previousThresholdIndex) {
            FloatRect targetBoundingClientRect;
            FloatRect clientIntersectionRect;
            FloatRect clientRootBounds;
            if (intersectionState) {
                RefPtr targetFrameView = target->document().view();
                targetBoundingClientRect = targetFrameView->absoluteToClientRect(intersectionState->absoluteTargetRect, target->renderer()->style().effectiveZoom());
                clientRootBounds = frameView->absoluteToLayoutViewportRect(intersectionState->absoluteRootBounds);
                if (intersectionState->isIntersecting)
                    clientIntersectionRect = targetFrameView->absoluteToClientRect(intersectionState->absoluteIntersectionRect, target->renderer()->style().effectiveZoom());
            }

            std::optional<DOMRectInit> reportedRootBounds;
            if (isSameOriginObservation) {
                reportedRootBounds = DOMRectInit({
                    clientRootBounds.x(),
                    clientRootBounds.y(),
                    clientRootBounds.width(),
                    clientRootBounds.height()
                });
            }

            appendQueuedEntry(IntersectionObserverEntry::create({
                timestamp->milliseconds(),
                reportedRootBounds,
                { targetBoundingClientRect.x(), targetBoundingClientRect.y(), targetBoundingClientRect.width(), targetBoundingClientRect.height() },
                { clientIntersectionRect.x(), clientIntersectionRect.y(), clientIntersectionRect.width(), clientIntersectionRect.height() },
                intersectionRatio,
                target.get(),
                thresholdIndex > 0,
            }));

            needNotify = NeedNotify::Yes;
            registration.previousThresholdIndex = thresholdIndex;
        }
    }

    return needNotify;
}

std::optional<ReducedResolutionSeconds> IntersectionObserver::nowTimestamp() const
{
    if (!m_callback)
        return std::nullopt;

    auto* context = m_callback->scriptExecutionContext();
    if (!context)
        return std::nullopt;

    ASSERT(context->isDocument());
    auto& document = downcast<Document>(*context);
    if (auto* window = document.domWindow())
        return window->frozenNowTimestamp();
    
    return std::nullopt;
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

    auto takenRecords = takeRecords();
    auto targetsWaitingForFirstObservation = std::exchange(m_targetsWaitingForFirstObservation, { });

    // FIXME: The JSIntersectionObserver wrapper should be kept alive as long as the intersection observer can fire events.
    ASSERT(m_callback->hasCallback());
    if (!m_callback->hasCallback())
        return;

    auto* context = m_callback->scriptExecutionContext();
    if (!context)
        return;

#if !LOG_DISABLED
    if (LogIntersectionObserver.state == WTFLogChannelState::On) {
        TextStream recordsStream(TextStream::LineMode::MultipleLine);
        recordsStream << takenRecords.records;
        LOG_WITH_STREAM(IntersectionObserver, stream << "IntersectionObserver " << this << " notify - records " << recordsStream.release());
    }
#endif

    InspectorInstrumentation::willFireObserverCallback(*context, "IntersectionObserver"_s);
    m_callback->handleEvent(*this, WTFMove(takenRecords.records), *this);
    InspectorInstrumentation::didFireObserverCallback(*context);
}

bool IntersectionObserver::isReachableFromOpaqueRoots(JSC::AbstractSlotVisitor& visitor) const
{
    for (auto& target : m_observationTargets) {
        if (auto* element = target.get(); containsWebCoreOpaqueRoot(visitor, element))
            return true;
    }
    for (auto& target : m_pendingTargets) {
        if (containsWebCoreOpaqueRoot(visitor, target.get()))
            return true;
    }
    return !m_targetsWaitingForFirstObservation.isEmpty();
}

} // namespace WebCore
