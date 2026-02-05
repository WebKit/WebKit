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
            thresholds = WTF::move(initThresholds);
        }
    );

    if (thresholds.isEmpty())
        thresholds.append(0.f);

    for (auto threshold : thresholds) {
        if (!(threshold >= 0 && threshold <= 1))
            return Exception { ExceptionCode::RangeError, "Failed to construct 'IntersectionObserver': all thresholds must lie in the range [0.0, 1.0]."_s };
    }

    return adoptRef(*new IntersectionObserver(document, WTF::move(callback), root.get(), rootMarginOrException.releaseReturnValue(), scrollMarginOrException.releaseReturnValue(), WTF::move(thresholds), includeObscuredInsets));
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(IntersectionObserver);

IntersectionObserver::IntersectionObserver(Document& document, Ref<IntersectionObserverCallback>&& callback, ContainerNode* root, IntersectionObserverMarginBox&& parsedRootMargin, IntersectionObserverMarginBox&& parsedScrollMargin, Vector<double>&& thresholds, IncludeObscuredInsets includeObscuredInsets)
    : m_root(root)
    , m_rootMargin(WTF::move(parsedRootMargin))
    , m_scrollMargin(WTF::move(parsedScrollMargin))
    , m_thresholds(WTF::move(thresholds))
    , m_callback(WTF::move(callback))
    , m_includeObscuredInsets(includeObscuredInsets)
{
    if (RefPtr rootDocument = dynamicDowncast<Document>(root)) {
        auto& observerData = rootDocument->ensureIntersectionObserverData();
        observerData.observers.append(*this);
    } else if (root) {
        auto& observerData = downcast<Element>(*root).ensureIntersectionObserverData();
        observerData.observers.append(*this);
    } else if (RefPtr frame = document.frame()) {
        if (RefPtr localFrame = dynamicDowncast<LocalFrame>(frame->mainFrame()))
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
    return { WTF::move(m_queuedEntries), WTF::move(m_pendingTargets) };
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

static std::optional<LayoutRect> computeClippedRectInRootContentsSpace(const LayoutRect& rect, const SecurityOrigin& targetSecurityOrigin, Variant<const RenderElement*, const Frame*> rendererOrFrame, std::optional<IntersectionObserverMarginBox> scrollMargin)
{
    RefPtr rendererOrFrameSecurityOrigin = WTF::visit(WTF::makeVisitor(
        [&] (const RenderElement* renderer) { return Ref<const Frame>(renderer->frame())->frameDocumentSecurityOrigin(); },
        [&] (const Frame* frame) { return frame->frameDocumentSecurityOrigin(); }
    ), rendererOrFrame);

    // targetSecurityOrigin is the security origin of the target (the element that originates the very first rect)
    // Scroll margin should not propagate past the first cross-origin frame in the chain leading to the main frame.
    // e.g given the chain: main frame <- cross-origin frame <- same-origin frame 2 <- same-origin frame 1 <- target
    // then scroll margin is applied to same-origin frame 1/2 but not to cross-origin and main frames.
    // Hence, clear out the scroll margin when we see a cross-origin frame.
    bool isSameOriginDomain = [&] () {
        if (rendererOrFrameSecurityOrigin)
            return rendererOrFrameSecurityOrigin->isSameOriginDomain(targetSecurityOrigin);

        return false;
    }();
    if (!isSameOriginDomain)
        scrollMargin.reset();

    RefPtr<const Frame> enclosingFrame = WTF::visit(WTF::makeVisitor(
        [&] (const RenderElement* renderer) { return static_cast<const Frame*>(&renderer->frame()); },
        [&] (const Frame* frame) { return static_cast<const Frame*>(frame->tree().parent()); }
    ), rendererOrFrame);

    if (!enclosingFrame)
        return std::nullopt;

    auto absoluteClippedRect = WTF::visit(WTF::makeVisitor(
        [&] (const RenderElement* renderer) {
            auto visibleRects = renderer->computeVisibleRectsInContainer(
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

            return visibleRects.transform([] (auto&& repaintRects) { return repaintRects.clippedOverflowRect; } );
        },
        [&] (const Frame* frame) -> std::optional<LayoutRect> {
            auto visibleRectInParentFrame = enclosingFrame->virtualView()->visibleRectOfChild(*frame);
            if (!visibleRectInParentFrame)
                return std::nullopt;

            auto clippedRect = rect;
            if (!clippedRect.edgeInclusiveIntersect(*visibleRectInParentFrame))
                return std::nullopt;

            return std::make_optional(clippedRect);
    }), rendererOrFrame);

    if (!absoluteClippedRect)
        return std::nullopt;

    // If the renderer is in the main frame, there are no more frames to traverse to, so stop here.
    if (enclosingFrame->isMainFrame())
        return absoluteClippedRect;

    // The computed visible rect is in the coordinate space of the document content box,
    // and is what's visible in the iframe's content area (aka the iframe document content box)
    // But only the iframe's viewport is visible, so clip by the iframe's viewport.

    // Compute the frame's viewport (this is in the coordinate space of the document content box)
    RefPtr<const FrameView> enclosingFrameView = enclosingFrame->virtualView();
    ASSERT(enclosingFrameView);

    auto frameRect = enclosingFrameView->layoutViewportRect();
    if (scrollMargin) {
        auto scrollMarginEdges = LayoutBoxExtent {
            LayoutUnit(Style::evaluate<int>(scrollMargin->top(), frameRect.height(), Style::ZoomNeeded { })),
            LayoutUnit(Style::evaluate<int>(scrollMargin->right(), frameRect.width(), Style::ZoomNeeded { })),
            LayoutUnit(Style::evaluate<int>(scrollMargin->bottom(), frameRect.height(), Style::ZoomNeeded { })),
            LayoutUnit(Style::evaluate<int>(scrollMargin->left(), frameRect.width(), Style::ZoomNeeded { })),
        };
        frameRect.expand(scrollMarginEdges);
    }

    if (!absoluteClippedRect->edgeInclusiveIntersect(frameRect))
        return std::nullopt;

    absoluteClippedRect = LayoutRect { enclosingFrameView->contentsToView(*absoluteClippedRect) };

    if (RefPtr ownerRenderer = enclosingFrame->ownerRenderer()) {
        absoluteClippedRect->moveBy(ownerRenderer->contentBoxLocation());
        return computeClippedRectInRootContentsSpace(*absoluteClippedRect, targetSecurityOrigin, ownerRenderer.get(), scrollMargin);
    }

    absoluteClippedRect->moveBy(enclosingFrameView->location());
    return computeClippedRectInRootContentsSpace(*absoluteClippedRect, targetSecurityOrigin, enclosingFrame.get(), WTF::move(scrollMargin));
}

auto IntersectionObserver::computeIntersectionState(const IntersectionObserverRegistration& registration, FrameView& hostFrameView, Element& target, ApplyRootMargin applyRootMargin) const -> IntersectionObservationState
{
    bool isFirstObservation = !registration.previousThresholdIndex;

    float rootUsedZoom = 1.0;
    RenderBlock* rootRenderer = nullptr;
    RenderElement* targetRenderer = nullptr;
    IntersectionObservationState intersectionState;

    auto layoutViewportRectForIntersection = [&] {
        if (m_includeObscuredInsets == IncludeObscuredInsets::Yes) {
            // IncludeObscuredInsets::Yes is only used by ContentVisibilityDocumentState, which
            // tracks the visibility of an element wrt. its document.
            // Therefore the intersection observer is guaranteed to be a local and explicit root
            // observer, so frameView must be local too.
            return downcast<LocalFrameView>(hostFrameView).layoutViewportRectIncludingObscuredInsets();
        }

        return hostFrameView.layoutViewportRect();
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
                intersectionState.rootBounds = rootRenderer->paddingBoxRect();
            else
                intersectionState.rootBounds = { FloatPoint(), rootRenderer->size() };

            rootUsedZoom = rootRenderer->style().usedZoom();

            return;
        }

        ASSERT(hostFrameView.frame().isMainFrame());
        // FIXME: Handle the case of an implicit-root observer that has a target in a different frame tree.
        if (&targetRenderer->frame().mainFrame() != &hostFrameView.frame())
            return;

        intersectionState.canComputeIntersection = true;
        // FIXME: provide these information in some way if the host frame is remote.
        rootRenderer = downcast<LocalFrameView>(hostFrameView).renderView();
        rootUsedZoom = downcast<LocalFrameView>(hostFrameView).renderView()->style().usedZoom();
        intersectionState.rootBounds = layoutViewportRectForIntersection();
    };

    computeRootBounds();
    if (!intersectionState.canComputeIntersection) {
        intersectionState.observationChanged = isFirstObservation || *registration.previousThresholdIndex != 0;
        return intersectionState;
    }

    if (applyRootMargin == ApplyRootMargin::Yes) {
        expandRootBoundsWithRootMargin(intersectionState.rootBounds, scrollMarginBox(), rootUsedZoom);
        expandRootBoundsWithRootMargin(intersectionState.rootBounds, rootMarginBox(), rootUsedZoom);
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
        auto rootAbsoluteIntersectionRect = rootRenderer->localToAbsoluteQuad(rootLocalIntersectionRect).boundingBox();

        if (root() && &targetRenderer->frame() == &rootRenderer->frame())
            intersectionState.absoluteIntersectionRect = rootAbsoluteIntersectionRect;
        else {
            auto rootViewIntersectionRect = hostFrameView.contentsToView(rootAbsoluteIntersectionRect);
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
        intersectionState.absoluteRootBounds = rootRenderer->localToAbsoluteQuad(intersectionState.rootBounds).boundingBox();

        if (!intersectionState.absoluteTargetRect)
            intersectionState.absoluteTargetRect = targetRenderer->localToAbsoluteQuad(FloatRect(localTargetBounds)).boundingBox();
    }

    return intersectionState;
}

auto IntersectionObserver::updateObservations(const Frame& hostFrame) -> NeedNotify
{
    RefPtr hostFrameView = hostFrame.virtualView();
    if (!hostFrameView)
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
                return protect(target->document().securityOrigin())->isSameOriginDomain(*hostFrameSecurityOrigin);

            return false;
        }();
        auto applyRootMargin = isSameOriginObservation ? ApplyRootMargin::Yes : ApplyRootMargin::No;
        auto intersectionState = computeIntersectionState(registration, *hostFrameView, *target, applyRootMargin);

        if (intersectionState.observationChanged) {
            FloatRect targetBoundingClientRect;
            FloatRect clientIntersectionRect;
            FloatRect clientRootBounds;
            if (intersectionState.canComputeIntersection) {
                ASSERT(intersectionState.absoluteTargetRect);
                ASSERT(intersectionState.absoluteRootBounds);

                RefPtr targetFrameView = target->document().view();
                targetBoundingClientRect = targetFrameView->absoluteToClientRect(*intersectionState.absoluteTargetRect, target->renderer()->style().usedZoom());
                clientRootBounds = hostFrameView->absoluteToLayoutViewportRect(*intersectionState.absoluteRootBounds);

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
                intersectionState.thresholdIndex > 0,
                intersectionState.intersectionRatio,
                *target,
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
    m_pendingTargets.append(entry->target());
    m_queuedEntries.append(WTF::move(entry));
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
    m_callback->invoke(*this, WTF::move(takenRecords.records), *this);
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
