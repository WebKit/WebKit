/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Google Inc. All rights reserved.
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

#include "ContainerNodeInlines.h"
#include "ContextDestructionObserverInlines.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserConsumer+Background.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+LengthPercentageDefinitions.h"
#include "CSSTokenizer.h"
#include "DocumentSecurityOrigin.h"
#include "DocumentView.h"
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
#include "RenderBoxInlines.h"
#include "RenderInline.h"
#include "RenderLineBreak.h"
#include "RenderObjectInlines.h"
#include "RenderView.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include "VisibleRectContext.h"
#include "WebCoreOpaqueRootInlines.h"
#include <JavaScriptCore/AbstractSlotVisitorInlines.h>
#include <ranges>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>

namespace WebCore {

static ExceptionOr<IntersectionObserverMarginBox> parseMargin(String& margin, const String& marginName)
{
    using namespace CSSPropertyParserHelpers;

    auto parserContext = CSSParserContext { HTMLStandardMode };
    auto parserState = CSS::PropertyParserState {
        .context = parserContext,
    };

    CSSTokenizer tokenizer(margin);
    auto tokenRange = tokenizer.tokenRange();
    tokenRange.consumeWhitespace();

    if (tokenRange.atEnd())
        return IntersectionObserverMarginBox { IntersectionObserverMarginEdge::Fixed { 0 } };

    auto consumeEdge = [&] -> ExceptionOr<IntersectionObserverMarginEdge> {
        auto parsedValue = CSSPrimitiveValueResolver<CSS::LengthPercentage<>>::consumeAndResolve(tokenRange, parserState);

        if (!parsedValue || parsedValue->isCalculated())
            return Exception { ExceptionCode::SyntaxError, makeString("Failed to construct 'IntersectionObserver': "_s, marginName, " must be specified in pixels or percent."_s) };

        if (parsedValue->isPercentage())
            return { IntersectionObserverMarginEdge::Percentage { parsedValue->resolveAsPercentageNoConversionDataRequired<float>() } };

        // FIXME: This should support all absolute length units, not just px.
        // Spec states: "Similar to the CSS margin property, this is a string of 1-4 components, each either an *absolute length* or a percentage."
        // https://w3c.github.io/IntersectionObserver/#dom-intersectionobserverinit-rootmargin
        if (parsedValue->isPx())
            return { IntersectionObserverMarginEdge::Fixed { parsedValue->resolveAsLengthNoConversionDataRequired<float>() } };

        return Exception { ExceptionCode::SyntaxError, makeString("Failed to construct 'IntersectionObserver': "_s, marginName, " must be specified in pixels or percent."_s) };
    };

    auto edge1 = consumeEdge();
    if (edge1.hasException())
        return edge1.releaseException();

    if (tokenRange.atEnd())
        return completeQuad<IntersectionObserverMarginBox>(edge1.releaseReturnValue());

    auto edge2 = consumeEdge();
    if (edge2.hasException())
        return edge2.releaseException();

    if (tokenRange.atEnd())
        return completeQuad<IntersectionObserverMarginBox>(edge1.releaseReturnValue(), edge2.releaseReturnValue());

    auto edge3 = consumeEdge();
    if (edge3.hasException())
        return edge3.releaseException();

    if (tokenRange.atEnd())
        return completeQuad<IntersectionObserverMarginBox>(edge1.releaseReturnValue(), edge2.releaseReturnValue(), edge3.releaseReturnValue());

    auto edge4 = consumeEdge();
    if (edge4.hasException())
        return edge4.releaseException();

    if (!tokenRange.atEnd())
        return Exception { ExceptionCode::SyntaxError, makeString("Failed to construct 'IntersectionObserver': Extra text found at the end of "_s, marginName, "."_s) };

    return IntersectionObserverMarginBox { edge1.releaseReturnValue(), edge2.releaseReturnValue(), edge3.releaseReturnValue(), edge4.releaseReturnValue() };
}

ExceptionOr<Ref<IntersectionObserver>> IntersectionObserver::create(Document& document, Ref<IntersectionObserverCallback>&& callback, IntersectionObserver::Init&& init, IncludeObscuredInsets includeObscuredInsets)
{
    RefPtr<ContainerNode> root;
    if (init.root) {
        root = WTF::switchOn(*init.root,
            [](auto elementOrDocument) -> RefPtr<ContainerNode> {
                return elementOrDocument.get();
            }
        );
    }

    auto rootMarginOrException = parseMargin(init.rootMargin, "rootMargin"_s);
    if (rootMarginOrException.hasException())
        return rootMarginOrException.releaseException();

    auto scrollMarginOrException = parseMargin(init.scrollMargin, "scrollMargin"_s);
    if (scrollMarginOrException.hasException())
        return scrollMarginOrException.releaseException();

    Vector<double> thresholds;
    WTF::switchOn(init.threshold,
        [&thresholds](double initThreshold) {
            thresholds.append(initThreshold);
        },
        [&thresholds](Vector<double>& initThresholds) {
            thresholds = WTFMove(initThresholds);
        }
    );

    if (thresholds.isEmpty())
        thresholds.append(0.f);

    for (auto threshold : thresholds) {
        if (!(threshold >= 0 && threshold <= 1))
            return Exception { ExceptionCode::RangeError, "Failed to construct 'IntersectionObserver': all thresholds must lie in the range [0.0, 1.0]."_s };
    }

    return adoptRef(*new IntersectionObserver(document, WTFMove(callback), root.get(), rootMarginOrException.releaseReturnValue(), scrollMarginOrException.releaseReturnValue(), WTFMove(thresholds), includeObscuredInsets));
}

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(IntersectionObserver);

IntersectionObserver::IntersectionObserver(Document& document, Ref<IntersectionObserverCallback>&& callback, ContainerNode* root, IntersectionObserverMarginBox&& parsedRootMargin, IntersectionObserverMarginBox&& parsedScrollMargin, Vector<double>&& thresholds, IncludeObscuredInsets includeObscuredInsets)
    : m_root(root)
    , m_rootMargin(WTFMove(parsedRootMargin))
    , m_scrollMargin(WTFMove(parsedScrollMargin))
    , m_thresholds(WTFMove(thresholds))
    , m_callback(WTFMove(callback))
    , m_includeObscuredInsets(includeObscuredInsets)
{
    if (RefPtr rootDocument = dynamicDowncast<Document>(root)) {
        auto& observerData = rootDocument->ensureIntersectionObserverData();
        observerData.observers.append(*this);
    } else if (root) {
        auto& observerData = downcast<Element>(*root).ensureIntersectionObserverData();
        observerData.observers.append(*this);
    } else if (auto* frame = document.frame()) {
        if (auto* localFrame = dynamicDowncast<LocalFrame>(frame->mainFrame()))
            m_implicitRootDocument = localFrame->document();
    }

    std::ranges::sort(m_thresholds);

    LOG_WITH_STREAM(IntersectionObserver, stream << "Created IntersectionObserver " << this << " root " << root << " root margin " << m_rootMargin << " scroll margin " << m_scrollMargin << " thresholds " << m_thresholds);
}

IntersectionObserver::~IntersectionObserver()
{
    RefPtr root = m_root.get();
    if (RefPtr document = dynamicDowncast<Document>(root))
        document->intersectionObserverDataIfExists()->observers.removeFirst(this);
    else if (root)
        downcast<Element>(*root).intersectionObserverDataIfExists()->observers.removeFirst(this);
    disconnect();
}

Document* IntersectionObserver::trackingDocument() const
{
    return m_root ? &m_root->document() : m_implicitRootDocument.get();
}

static String marginBoxToString(const IntersectionObserverMarginBox& marginBox)
{
    StringBuilder stringBuilder;
    for (auto side : allBoxSides) {
        auto& edge = marginBox.at(side);
        if (auto percentage = edge.tryPercentage())
            stringBuilder.append(static_cast<int>(percentage->value), "%"_s, side != BoxSide::Left ? " "_s : ""_s);
        else
            stringBuilder.append(static_cast<int>(edge.tryFixed()->resolveZoom(Style::ZoomNeeded { })), "px"_s, side != BoxSide::Left ? " "_s : ""_s);
    }
    return stringBuilder.toString();
}

String IntersectionObserver::rootMargin() const
{
    return marginBoxToString(m_rootMargin);
}

String IntersectionObserver::scrollMargin() const
{
    return marginBoxToString(m_scrollMargin);
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

    RefPtr document = trackingDocument();
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
        if (RefPtr document = trackingDocument())
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
    if (RefPtr document = trackingDocument())
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
        if (RefPtr document = trackingDocument())
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

static void expandRootBoundsWithRootMargin(FloatRect& rootBounds, const IntersectionObserverMarginBox& rootMargin, float zoomFactor)
{
    auto zoomAdjustedLength = [](const IntersectionObserverMarginEdge& edge, float maximumValue, float zoomFactor) {
        if (auto percentage = edge.tryPercentage())
            return Style::evaluate<float>(*percentage, maximumValue);
        return Style::evaluate<float>(*edge.tryFixed(), Style::ZoomNeeded { }) * zoomFactor;
    };

    auto rootMarginEdges = FloatBoxExtent {
        zoomAdjustedLength(rootMargin.top(), rootBounds.height(), zoomFactor),
        zoomAdjustedLength(rootMargin.right(), rootBounds.width(), zoomFactor),
        zoomAdjustedLength(rootMargin.bottom(), rootBounds.height(), zoomFactor),
        zoomAdjustedLength(rootMargin.left(), rootBounds.width(), zoomFactor)
    };

    rootBounds.expand(rootMarginEdges);
}

static std::optional<LayoutRect> computeClippedRectInRootContentsSpace(const LayoutRect& rect, const SecurityOrigin& targetSecurityOrigin, const RenderElement* renderer, std::optional<IntersectionObserverMarginBox> scrollMargin)
{
    // targetSecurityOrigin is the security origin of the target (the element that originates the very first rect)
    // Scroll margin should not propagate past the first cross-origin frame in the chain leading to the main frame.
    // e.g given the chain: main frame <- cross-origin frame <- same-origin frame 2 <- same-origin frame 1 <- target
    // then scroll margin is applied to same-origin frame 1/2 but not to cross-origin and main frames.
    // Hence, clear out the scroll margin when we see a cross-origin frame.
    bool shouldApplyScrollMargin = Ref<const Frame>(renderer->frame())->frameDocumentSecurityOrigin()->isSameOriginDomain(targetSecurityOrigin);
    if (!shouldApplyScrollMargin)
        scrollMargin.reset();

    auto absoluteRects = renderer->computeVisibleRectsInContainer(
        { rect },
        &renderer->view(),
        {
            .hasPositionFixedDescendant = false,
            .dirtyRectIsFlipped = false,
            .descendantNeedsEnclosingIntRect = false,
            .options = {
                VisibleRectContext::Option::UseEdgeInclusiveIntersection,
                VisibleRectContext::Option::ApplyCompositedClips,
                VisibleRectContext::Option::ApplyCompositedContainerScrolls
            },
            .scrollMargin = scrollMargin
        }
    );
    if (!absoluteRects)
        return std::nullopt;

    auto absoluteClippedRect = absoluteRects->clippedOverflowRect;
    if (renderer->frame().isMainFrame())
        return absoluteClippedRect;

    auto frameRect = renderer->view().frameView().layoutViewportRect();
    if (scrollMargin) {
        auto scrollMarginEdges = LayoutBoxExtent {
            LayoutUnit(Style::evaluate<int>(scrollMargin->top(), frameRect.height(), Style::ZoomNeeded { })),
            LayoutUnit(Style::evaluate<int>(scrollMargin->right(), frameRect.width(), Style::ZoomNeeded { })),
            LayoutUnit(Style::evaluate<int>(scrollMargin->bottom(), frameRect.height(), Style::ZoomNeeded { })),
            LayoutUnit(Style::evaluate<int>(scrollMargin->left(), frameRect.width(), Style::ZoomNeeded { })),
        };
        frameRect.expand(scrollMarginEdges);
    }

    bool intersects = absoluteClippedRect.edgeInclusiveIntersect(frameRect);
    if (!intersects)
        return std::nullopt;

    RefPtr ownerRenderer = renderer->frame().ownerRenderer();
    if (!ownerRenderer)
        return std::nullopt;

    LayoutRect rectInFrameViewSpace { renderer->view().frameView().contentsToView(absoluteClippedRect) };

    rectInFrameViewSpace.moveBy(ownerRenderer->contentBoxLocation());
    return computeClippedRectInRootContentsSpace(rectInFrameViewSpace, targetSecurityOrigin, ownerRenderer.get(), WTFMove(scrollMargin));
}

auto IntersectionObserver::computeIntersectionState(const IntersectionObserverRegistration& registration, LocalFrameView& frameView, Element& target, ApplyRootMargin applyRootMargin) const -> IntersectionObservationState
{
    bool isFirstObservation = !registration.previousThresholdIndex;

    RenderBlock* rootRenderer = nullptr;
    RenderElement* targetRenderer = nullptr;
    IntersectionObservationState intersectionState;

    auto layoutViewportRectForIntersection = [&] {
        if (m_includeObscuredInsets == IncludeObscuredInsets::Yes)
            return frameView.layoutViewportRectIncludingObscuredInsets();

        return frameView.layoutViewportRect();
    };

    auto computeRootBounds = [&]() {
        targetRenderer = target.renderer();
        if (!targetRenderer)
            return;

        if (root()) {
            if (trackingDocument() != &target.document())
                return;

            if (!root()->renderer())
                return;

            rootRenderer = dynamicDowncast<RenderBlock>(root()->renderer());
            if (!rootRenderer || !rootRenderer->isContainingBlockAncestorFor(*targetRenderer))
                return;

            intersectionState.canComputeIntersection = true;
            if (root() == &target.document())
                intersectionState.rootBounds = layoutViewportRectForIntersection();
            else if (rootRenderer->hasNonVisibleOverflow())
                intersectionState.rootBounds = rootRenderer->contentBoxRect();
            else
                intersectionState.rootBounds = { FloatPoint(), rootRenderer->size() };

            return;
        }

        ASSERT(frameView.frame().isMainFrame());
        // FIXME: Handle the case of an implicit-root observer that has a target in a different frame tree.
        if (&targetRenderer->frame().mainFrame() != &frameView.frame())
            return;

        intersectionState.canComputeIntersection = true;
        rootRenderer = frameView.renderView();
        intersectionState.rootBounds = layoutViewportRectForIntersection();
    };

    computeRootBounds();
    if (!intersectionState.canComputeIntersection) {
        intersectionState.observationChanged = isFirstObservation || *registration.previousThresholdIndex != 0;
        return intersectionState;
    }

    if (applyRootMargin == ApplyRootMargin::Yes) {
        expandRootBoundsWithRootMargin(intersectionState.rootBounds, scrollMarginBox(), rootRenderer->style().usedZoom());
        expandRootBoundsWithRootMargin(intersectionState.rootBounds, rootMarginBox(), rootRenderer->style().usedZoom());
    }

    auto localTargetBounds = [&]() -> LayoutRect {
        if (CheckedPtr renderBox = dynamicDowncast<RenderBox>(*targetRenderer))
            return renderBox->borderBoundingBox();

        if (is<RenderInline>(targetRenderer)) {
            Vector<LayoutRect> rects;
            targetRenderer->boundingRects(rects, { });
            return unionRect(rects);
        }

        if (CheckedPtr renderLineBreak = dynamicDowncast<RenderLineBreak>(targetRenderer))
            return renderLineBreak->linesBoundingBox();

        // FIXME: Implement for SVG etc.
        return { };
    }();

    auto rootRelativeTargetRect = [&]() -> std::optional<LayoutRect> {
        if (targetRenderer->isSkippedContent())
            return std::nullopt;

        if (root()) {
            auto result = targetRenderer->computeVisibleRectsInContainer(
                { localTargetBounds },
                rootRenderer,
                {
                    .hasPositionFixedDescendant = false,
                    .dirtyRectIsFlipped = false,
                    .descendantNeedsEnclosingIntRect = false,
                    .options = {
                        VisibleRectContext::Option::UseEdgeInclusiveIntersection,
                        VisibleRectContext::Option::ApplyCompositedClips,
                        VisibleRectContext::Option::ApplyCompositedContainerScrolls
                    },
                    .scrollMargin = { }
                }
            );
            if (!result)
                return std::nullopt;
            return result->clippedOverflowRect;
        }

        return computeClippedRectInRootContentsSpace(localTargetBounds, target.document().securityOrigin(), targetRenderer, scrollMarginBox());
    }();

    auto rootLocalIntersectionRect = intersectionState.rootBounds;
    intersectionState.isIntersecting = rootRelativeTargetRect && rootLocalIntersectionRect.edgeInclusiveIntersect(*rootRelativeTargetRect);

    if (isFirstObservation || intersectionState.isIntersecting)
        intersectionState.absoluteTargetRect = targetRenderer->localToAbsoluteQuad(FloatRect(localTargetBounds)).boundingBox();

    if (intersectionState.isIntersecting) {
        // If implicit root, rootLocalIntersectionRect is already in absolute coordinates.
        auto rootAbsoluteIntersectionRect = root() ? rootRenderer->localToAbsoluteQuad(rootLocalIntersectionRect).boundingBox() : rootLocalIntersectionRect;

        if (&targetRenderer->frame() == &rootRenderer->frame())
            intersectionState.absoluteIntersectionRect = rootAbsoluteIntersectionRect;
        else {
            auto rootViewIntersectionRect = frameView.contentsToView(rootAbsoluteIntersectionRect);
            intersectionState.absoluteIntersectionRect = targetRenderer->view().frameView().rootViewToContents(rootViewIntersectionRect);
        }

        intersectionState.isIntersecting = intersectionState.absoluteIntersectionRect->edgeInclusiveIntersect(*intersectionState.absoluteTargetRect);
    }

    if (intersectionState.isIntersecting) {
        float absTargetArea = intersectionState.absoluteTargetRect->area();
        if (absTargetArea)
            intersectionState.intersectionRatio = intersectionState.absoluteIntersectionRect->area() / absTargetArea;
        else
            intersectionState.intersectionRatio = 1;

        size_t thresholdIndex = 0;
        for (auto threshold : thresholds()) {
            if (!(threshold <= intersectionState.intersectionRatio || WTF::areEssentiallyEqual<float>(threshold, intersectionState.intersectionRatio)))
                break;
            ++thresholdIndex;
        }

        intersectionState.thresholdIndex = thresholdIndex;
    }

    intersectionState.observationChanged = isFirstObservation || intersectionState.thresholdIndex != registration.previousThresholdIndex;
    if (intersectionState.observationChanged) {
        if (root()) {
            // Explicit root in the same process, localToAbsoluteQuad can be used.
            intersectionState.absoluteRootBounds = rootRenderer->localToAbsoluteQuad(intersectionState.rootBounds).boundingBox();
        } else {
            // Implicit root (the main frame's document), might be out of process due to site isolation.
            // Regardless, intersectionState.rootBounds is already in absolute coordinate.
            intersectionState.absoluteRootBounds = intersectionState.rootBounds;
        }

        if (!intersectionState.absoluteTargetRect)
            intersectionState.absoluteTargetRect = targetRenderer->localToAbsoluteQuad(FloatRect(localTargetBounds)).boundingBox();
    }

    return intersectionState;
}

auto IntersectionObserver::updateObservations(const Frame& hostFrame) -> NeedNotify
{
    RefPtr frameView = dynamicDowncast<LocalFrameView>(hostFrame.virtualView());
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

        bool isSameOriginObservation = [&] () {
            if (RefPtr hostFrameSecurityOrigin = hostFrame.frameDocumentSecurityOrigin())
                return target->document().protectedSecurityOrigin()->isSameOriginDomain(*hostFrameSecurityOrigin);

            return false;
        }();
        auto applyRootMargin = isSameOriginObservation ? ApplyRootMargin::Yes : ApplyRootMargin::No;
        auto intersectionState = computeIntersectionState(registration, *frameView, *target, applyRootMargin);

        if (intersectionState.observationChanged) {
            FloatRect targetBoundingClientRect;
            FloatRect clientIntersectionRect;
            FloatRect clientRootBounds;
            if (intersectionState.canComputeIntersection) {
                ASSERT(intersectionState.absoluteTargetRect);
                ASSERT(intersectionState.absoluteRootBounds);

                RefPtr targetFrameView = target->document().view();
                targetBoundingClientRect = targetFrameView->absoluteToClientRect(*intersectionState.absoluteTargetRect, target->renderer()->style().usedZoom());
                clientRootBounds = frameView->absoluteToLayoutViewportRect(*intersectionState.absoluteRootBounds);
                if (intersectionState.isIntersecting) {
                    ASSERT(intersectionState.absoluteIntersectionRect);
                    clientIntersectionRect = targetFrameView->absoluteToClientRect(*intersectionState.absoluteIntersectionRect, target->renderer()->style().usedZoom());
                }
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
                intersectionState.intersectionRatio,
                target.get(),
                intersectionState.thresholdIndex > 0,
            }));

            needNotify = NeedNotify::Yes;
            registration.previousThresholdIndex = intersectionState.thresholdIndex;
        }
    }

    return needNotify;
}

std::optional<ReducedResolutionSeconds> IntersectionObserver::nowTimestamp() const
{
    if (!m_callback)
        return std::nullopt;

    RefPtr<LocalDOMWindow> window;
    {
        auto* context = m_callback->scriptExecutionContext();
        if (!context)
            return std::nullopt;
        auto& document = downcast<Document>(*context);
        window = document.window();
        if (!window)
            return std::nullopt;
    }
    return window->frozenNowTimestamp();
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

    RefPtr context = m_callback->scriptExecutionContext();
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
    m_callback->invoke(*this, WTFMove(takenRecords.records), *this);
    InspectorInstrumentation::didFireObserverCallback(*context);
}

bool IntersectionObserver::isReachableFromOpaqueRoots(JSC::AbstractSlotVisitor& visitor) const
{
    for (auto& target : m_observationTargets) {
        SUPPRESS_UNCOUNTED_LOCAL auto* element = target.get();
        if (containsWebCoreOpaqueRoot(visitor, element))
            return true;
    }
    for (auto& target : m_pendingTargets) {
        if (containsWebCoreOpaqueRoot(visitor, target.get()))
            return true;
    }
    return !m_targetsWaitingForFirstObservation.isEmpty();
}

} // namespace WebCore
