/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "PDFDiscretePresentationController.h"

#if ENABLE(UNIFIED_PDF)

#include "Logging.h"
#include "UnifiedPDFPlugin.h"
#include "WebEventConversion.h"
#include "WebKeyboardEvent.h"
#include "WebWheelEvent.h"
#include <WebCore/Animation.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/Length.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PlatformWheelEvent.h>
#include <WebCore/TiledBacking.h>
#include <WebCore/TimingFunction.h>
#include <WebCore/TransformOperations.h>
#include <WebCore/TransformationMatrix.h>
#include <pal/spi/mac/NSScrollViewSPI.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

static constexpr auto pageSwapDistanceThreshold = 200.0f;
static constexpr auto animationFrameInterval = 16.667_ms;

static float elasticDeltaForTimeDelta(float initialPosition, float initialVelocity, Seconds elapsedTime)
{
#if PLATFORM(MAC)
    return _NSElasticDeltaForTimeDelta(initialPosition, initialVelocity, elapsedTime.seconds());
#else
    notImplemented();
    return 0;
#endif
}

#if !LOG_DISABLED
static TextStream& operator<<(TextStream& ts, PageTransitionState state)
{
    switch (state) {
    case PageTransitionState::Idle: ts << "Idle"; break;
    case PageTransitionState::DeterminingStretchAxis: ts << "DeterminingStretchAxis"; break;
    case PageTransitionState::Stretching: ts << "Stretching"; break;
    case PageTransitionState::Settling: ts << "Settling"; break;
    case PageTransitionState::StartingAnimationFromStationary: ts << "StartingAnimationFromStationary"; break;
    case PageTransitionState::StartingAnimationFromMomentum: ts << "StartingAnimationFromMomentum"; break;
    case PageTransitionState::Animating: ts << "Animating"; break;
    }
    return ts;
}
#endif

WTF_MAKE_TZONE_ALLOCATED_IMPL(PDFDiscretePresentationController);

PDFDiscretePresentationController::PDFDiscretePresentationController(UnifiedPDFPlugin& plugin)
    : PDFPresentationController(plugin)
{

}

void PDFDiscretePresentationController::teardown()
{
    PDFPresentationController::teardown();

    GraphicsLayer::unparentAndClear(m_rowsContainerLayer);
    m_rows.clear();
}

bool PDFDiscretePresentationController::supportsDisplayMode(PDFDocumentLayout::DisplayMode mode) const
{
    return PDFDocumentLayout::isDiscreteDisplayMode(mode);
}

void PDFDiscretePresentationController::willChangeDisplayMode(PDFDocumentLayout::DisplayMode newMode)
{
    ASSERT(supportsDisplayMode(newMode));
    m_visibleRowIndex = 0;
}

#pragma mark -

bool PDFDiscretePresentationController::handleKeyboardEvent(const WebKeyboardEvent& event)
{
#if PLATFORM(MAC)
    if (handleKeyboardCommand(event))
        return true;

    if (handleKeyboardEventForPageNavigation(event))
        return true;
#endif

    return false;
}

// FIXME: <https://webkit.org/b/276981> Do we need this?
bool PDFDiscretePresentationController::handleKeyboardCommand(const WebKeyboardEvent& event)
{
    return false;
}

bool PDFDiscretePresentationController::handleKeyboardEventForPageNavigation(const WebKeyboardEvent& event)
{
    if (event.type() == WebEventType::KeyUp)
        return false;

    auto key = event.key();

    if (key == "Home"_s) {
        if (!m_visibleRowIndex)
            return false;

        goToRowIndex(0, Animated::No);
        return true;
    }

    if (key == "End"_s) {
        auto lastRowIndex = m_rows.size() - 1;
        if (m_visibleRowIndex == lastRowIndex)
            return false;

        goToRowIndex(lastRowIndex, Animated::No);
        return true;
    }

    auto maximumScrollPosition = m_plugin->maximumScrollPosition();

    bool isHorizontallyScrollable = !!maximumScrollPosition.x();
    bool isVerticallyScrollable = !!maximumScrollPosition.y();

    if (key == "ArrowLeft"_s || key == "ArrowUp"_s || key == "PageUp"_s) {
        if (key == "ArrowLeft"_s) {
            if (isHorizontallyScrollable)
                return false;
        } else if (isVerticallyScrollable)
            return false;

        if (!canGoToPreviousRow())
            return false;

        goToPreviousRow(Animated::No);
        return true;
    }

    if (key == "ArrowRight"_s || key == "ArrowDown"_s || key == "PageDown"_s) {
        if (key == "ArrowRight"_s) {
            if (isHorizontallyScrollable)
                return false;
        } else if (isVerticallyScrollable)
            return false;

        if (!canGoToNextRow())
            return false;

        goToNextRow(Animated::No);
        return true;
    }

    return false;
}

#pragma mark -

bool PDFDiscretePresentationController::canTransitionOnSide(BoxSide side) const
{
    switch (side) {
    case BoxSide::Left:
    case BoxSide::Top:
        return canGoToPreviousRow();
    case BoxSide::Right:
    case BoxSide::Bottom:
        return canGoToNextRow();
    }
    return false;
}

bool PDFDiscretePresentationController::canTransitionInDirection(TransitionDirection direction) const
{
    if (isPreviousDirection(direction))
        return canGoToPreviousRow();

    return canGoToNextRow();
}

// FIXME: <https://webkit.org/b/276981> Still an issue with stretching.
bool PDFDiscretePresentationController::handleWheelEvent(const WebWheelEvent& event)
{
    // This code has to handle wheel events for swiping in a way that's compatible with
    // normal scrolling when zoomed in, and allowing history swipes. It does so as follows:
    // * For begin events and non-gesture events, check for scrollability first.
    // * For all other events, return `false` if our state is idle.

    if (m_transitionState == PageTransitionState::Animating)
        return true; // Consume the events. Ideally we'd allow this to start another animation.

    auto wheelEvent = platform(event);

    LOG_WITH_STREAM(PDF, stream << "PDFDiscretePresentationController::handleWheelEvent " << wheelEvent << " state " << m_transitionState);

    if (wheelEvent.isNonGestureEvent())
        return handleDiscreteWheelEvent(wheelEvent);

    if (wheelEvent.phase() == PlatformWheelEventPhase::Began)
        return handleBeginEvent(wheelEvent);

    // Only Begin and non-gesture events take us out of the Idle state.
    if (m_transitionState == PageTransitionState::Idle)
        return false;

    // When we receive the last non-momentum event, we don't know if momentum events are coming (but see rdar://85308435).
    if (wheelEvent.isEndOfNonMomentumScroll()) {
        if (m_transitionState == PageTransitionState::Idle)
            return false;

        maybeEndGesture();
        return true;
    }

    if (wheelEvent.isTransitioningToMomentumScroll())
        m_gestureEndTimer.stop();

    if (wheelEvent.isEndOfMomentumScroll())
        return handleEndedEvent(wheelEvent);

    if (wheelEvent.isGestureCancel())
        return handleCancelledEvent(wheelEvent);

    if (wheelEvent.phase() == PlatformWheelEventPhase::Changed || wheelEvent.momentumPhase() == PlatformWheelEventPhase::Changed)
        return handleChangedEvent(wheelEvent);

    return false;
}

bool PDFDiscretePresentationController::eventCanStartPageTransition(const PlatformWheelEvent& wheelEvent) const
{
    auto wheelDelta = -wheelEvent.delta();

    auto shouldTransitionOnSize = [&](BoxSide side) {
        if (!m_plugin->isPinnedOnSide(side))
            return false;

        return canTransitionOnSide(side);
    };

    // Trying to determine which axis a gesture is for based on the small deltas in the begin event is unreliable,
    // but if we unconditionally handle the begin event, history swiping doesn't work.
    auto horizontalSide = ScrollableArea::targetSideForScrollDelta(wheelDelta, ScrollEventAxis::Horizontal);
    if (horizontalSide && !shouldTransitionOnSize(*horizontalSide))
        return false;

    auto verticalSide = ScrollableArea::targetSideForScrollDelta(wheelDelta, ScrollEventAxis::Vertical);
    if (verticalSide && !shouldTransitionOnSize(*verticalSide))
        return false;

    return true;
}

bool PDFDiscretePresentationController::handleBeginEvent(const PlatformWheelEvent& wheelEvent)
{
    if (!eventCanStartPageTransition(wheelEvent))
        return false;

    updateState(PageTransitionState::DeterminingStretchAxis);

    auto wheelDelta = -wheelEvent.delta();
    applyWheelEventDelta(wheelDelta);
    return true;
}

bool PDFDiscretePresentationController::handleChangedEvent(const PlatformWheelEvent& wheelEvent)
{
    LOG_WITH_STREAM(PDF, stream << "PDFDiscretePresentationController::handleChangedEvent - state " << m_transitionState);

    if (m_transitionState == PageTransitionState::Idle)
        return false;

    if (m_transitionState != PageTransitionState::DeterminingStretchAxis && m_transitionState != PageTransitionState::Stretching)
        return true;

    auto delta = -wheelEvent.delta();
    applyWheelEventDelta(delta);

    auto stetchOffset = relevantAxisForDirection(m_transitionDirection.value_or(TransitionDirection::NextVertical), m_stretchDistance);
    if (std::abs(stetchOffset) >= pageSwapDistanceThreshold && wheelEvent.momentumPhase() == PlatformWheelEventPhase::Changed) {
        auto eventWithVelocity = m_plugin->wheelEventCopyWithVelocity(wheelEvent);
        auto velocity = eventWithVelocity.scrollingVelocity();
        if (!velocity.isZero())
            m_momentumVelocity = velocity;

        updateState(PageTransitionState::StartingAnimationFromMomentum);
        return true;
    }

    updateLayersForTransitionState();
    return true;
}

bool PDFDiscretePresentationController::handleEndedEvent(const PlatformWheelEvent&)
{
    startPageTransitionOrSettle();
    return true;
}

bool PDFDiscretePresentationController::handleCancelledEvent(const PlatformWheelEvent&)
{
    updateState(PageTransitionState::Idle);
    return true;
}

bool PDFDiscretePresentationController::handleDiscreteWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    if (!eventCanStartPageTransition(wheelEvent))
        return false;

    if ((wheelEvent.deltaY() > 0 || wheelEvent.deltaX() > 0) && canGoToPreviousRow()) {
        goToPreviousRow(Animated::No);
        return true;
    }

    if ((wheelEvent.deltaY() < 0 || wheelEvent.deltaX() < 0) && canGoToNextRow()) {
        goToNextRow(Animated::No);
        return true;
    }

    return false;
}

#pragma mark -

void PDFDiscretePresentationController::maybeEndGesture()
{
    static constexpr auto gestureEndTimerDelay = 32_ms;
    // Wait for two frames to see if momentum will start.
    m_gestureEndTimer.startOneShot(gestureEndTimerDelay);
}

void PDFDiscretePresentationController::gestureEndTimerFired()
{
    startPageTransitionOrSettle();
}

void PDFDiscretePresentationController::startPageTransitionOrSettle()
{
    if (m_transitionState == PageTransitionState::DeterminingStretchAxis) {
        updateState(PageTransitionState::Idle);
        return;
    }

    if (m_transitionState != PageTransitionState::Stretching)
        return;

    auto stetchOffset = relevantAxisForDirection(m_transitionDirection.value_or(TransitionDirection::NextVertical), m_stretchDistance);
    if (std::abs(stetchOffset) < pageSwapDistanceThreshold) {
        updateState(PageTransitionState::Settling);
        return;
    }

    if (stetchOffset < 0) {
        updateState(PageTransitionState::StartingAnimationFromStationary);
        return;
    }

    if (stetchOffset > 0) {
        updateState(PageTransitionState::StartingAnimationFromStationary);
        return;
    }
}

void PDFDiscretePresentationController::updateState(PageTransitionState newState)
{
    if (newState == m_transitionState)
        return;

    LOG_WITH_STREAM(PDF, stream << "PDFDiscretePresentationController::updateState from " << m_transitionState << " to " << newState);
    auto oldState = std::exchange(m_transitionState, newState);

    switch (m_transitionState) {
    case PageTransitionState::Idle:
    case PageTransitionState::DeterminingStretchAxis:
        m_unappliedStretchDelta = { };
        m_stretchDistance = { };
        m_transitionDirection = { };
        m_momentumVelocity = { };
        break;
    case PageTransitionState::Stretching:
        break;
    case PageTransitionState::Settling:
        break;
    case PageTransitionState::StartingAnimationFromStationary:
    case PageTransitionState::StartingAnimationFromMomentum:
        startTransitionAnimation(m_transitionState);
        m_transitionState = PageTransitionState::Animating;
        break;
    case PageTransitionState::Animating:
        break;
    }

    startOrStopAnimationTimerIfNecessary();
    updateLayersForTransitionState();
    updateLayerVisibilityForTransitionState(oldState);
}

void PDFDiscretePresentationController::startOrStopAnimationTimerIfNecessary()
{
    switch (m_transitionState) {
    case PageTransitionState::Idle:
    case PageTransitionState::Stretching:
    case PageTransitionState::DeterminingStretchAxis:
    case PageTransitionState::StartingAnimationFromStationary:
    case PageTransitionState::StartingAnimationFromMomentum:
    case PageTransitionState::Animating:
        m_animationTimer.stop();
        break;
    case PageTransitionState::Settling:
        m_animationStartTime = MonotonicTime::now();
        m_animationStartDistance = relevantAxisForDirection(m_transitionDirection.value_or(TransitionDirection::NextVertical), m_stretchDistance);
        if (!m_animationTimer.isActive())
            m_animationTimer.startRepeating(animationFrameInterval);
        break;
    }
}

void PDFDiscretePresentationController::animationTimerFired()
{
    switch (m_transitionState) {
    case PageTransitionState::Idle:
    case PageTransitionState::Stretching:
    case PageTransitionState::DeterminingStretchAxis:
    case PageTransitionState::StartingAnimationFromStationary:
    case PageTransitionState::StartingAnimationFromMomentum:
    case PageTransitionState::Animating:
        break;
    case PageTransitionState::Settling:
        animateRubberBand(MonotonicTime::now());
        break;
    }
}

void PDFDiscretePresentationController::animateRubberBand(MonotonicTime now)
{
    static constexpr auto initialVelocity = 0.0f;

    auto stretch = elasticDeltaForTimeDelta(m_animationStartDistance, initialVelocity, now - m_animationStartTime);
    setRelevantAxisForDirection(m_transitionDirection.value_or(TransitionDirection::NextVertical), m_stretchDistance, stretch);
    updateLayersForTransitionState();

    if (std::abs(stretch) < 0.5)
        updateState(PageTransitionState::Idle);
}

void PDFDiscretePresentationController::startTransitionAnimation(PageTransitionState animationStartState)
{
    if (!m_transitionDirection)
        return;

    static constexpr auto defaultTransitionDuration = 0.5_s;
    static constexpr auto maximumTransitionDuration = 0.75_s;
    static constexpr auto minimumTransitionDuration = 0.20_s;

    auto transitionDuration = defaultTransitionDuration;

    auto transformAnimationValueForTranslation = [](double keyTime, FloatSize offset) {
        auto xLength = Length(offset.width(), LengthType::Fixed);
        auto yLength = Length(offset.height(), LengthType::Fixed);

        Vector<Ref<TransformOperation>> operations;
        operations.reserveInitialCapacity(1);
        operations.append(TranslateTransformOperation::create(xLength, yLength, Length(0, LengthType::Fixed), TransformOperationType::Translate));

        return makeUnique<TransformAnimationValue>(keyTime, TransformOperations { WTFMove(operations) }, nullptr);
    };

    auto createPositionKeyframesForAnimation = [&](TransitionDirection direction, FloatSize initialOffset, FloatSize finalOffset) {
        auto keyframes = KeyframeValueList { AnimatedProperty::Translate };
        auto initialValue = transformAnimationValueForTranslation(0, initialOffset);
        auto finalValue = transformAnimationValueForTranslation(1, finalOffset);
        keyframes.insert(WTFMove(initialValue));
        keyframes.insert(WTFMove(finalValue));
        return keyframes;
    };

    auto createOpacityKeyframesForAnimation = [](TransitionDirection direction, std::array<float, 2> startEndOpacities) {
        auto keyframes = KeyframeValueList { AnimatedProperty::Opacity };
        keyframes.insert(makeUnique<FloatAnimationValue>(0, startEndOpacities[startIndex]));
        keyframes.insert(makeUnique<FloatAnimationValue>(1, startEndOpacities[endIndex]));
        return keyframes;
    };

    auto addAnimationsToRowContainer = [&](const RowData& animatingRow, const RowData& stationaryRow, TransitionDirection direction, FloatSize startOffset, FloatSize endOffset, const std::array<std::array<float, 2>, 2>& layerEndOpacities) {
        auto transitionDuration = defaultTransitionDuration;

        RefPtr<TimingFunction> moveTimingFunction;
        RefPtr<TimingFunction> fadeTimingFunction;

        if (animationStartState == PageTransitionState::StartingAnimationFromMomentum) {
            moveTimingFunction = LinearTimingFunction::create();
            fadeTimingFunction = LinearTimingFunction::create();

            if (m_momentumVelocity) {
                float velocity = std::abs(relevantAxisForDirection(direction, *m_momentumVelocity));
                float distance = std::abs(relevantAxisForDirection(direction, endOffset - startOffset));

                transitionDuration = Seconds { distance / velocity };
                transitionDuration = std::min(std::max(transitionDuration, minimumTransitionDuration), maximumTransitionDuration);
            }
        } else {
            auto timingFunctionPreset = isNextDirection(direction) ? CubicBezierTimingFunction::TimingFunctionPreset::EaseIn : CubicBezierTimingFunction::TimingFunctionPreset::EaseOut;
            moveTimingFunction = CubicBezierTimingFunction::create(timingFunctionPreset);
            fadeTimingFunction = CubicBezierTimingFunction::create(timingFunctionPreset);
        }

        auto moveFrames = createPositionKeyframesForAnimation(direction, startOffset, endOffset);
        Ref moveAnimation = Animation::create();
        moveAnimation->setDuration(transitionDuration.seconds());
        moveAnimation->setTimingFunction(WTFMove(moveTimingFunction));
        animatingRow.containerLayer->addAnimation(moveFrames, { }, moveAnimation.ptr(), "move"_s, 0);

        auto fadeKeyframes = createOpacityKeyframesForAnimation(direction, layerEndOpacities[topLayerIndex]);
        Ref fadeAnimation = Animation::create();
        fadeAnimation->setDuration(transitionDuration.seconds());
        fadeAnimation->setTimingFunction(WTFMove(fadeTimingFunction));
        animatingRow.containerLayer->addAnimation(fadeKeyframes, { }, fadeAnimation.ptr(), "fade"_s, 0);

        auto stationaryLayerFadeKeyframes = createOpacityKeyframesForAnimation(direction, layerEndOpacities[bottomLayerIndex]);
        stationaryRow.containerLayer->addAnimation(stationaryLayerFadeKeyframes, { }, fadeAnimation.ptr(), "fade"_s, 0);

        return transitionDuration;
    };

    auto setupAnimations = [&](const RowData& animatingRow, const RowData& stationaryRow, TransitionDirection direction, FloatSize stretchDistance, FloatSize endOffset, FloatSize rowSize) {
        auto startOffset = layerOffsetForStretch(direction, stretchDistance, rowSize);

        auto layerOpacities = layerOpacitiesForStretchOffset(direction, startOffset, rowSize);
        transitionDuration = addAnimationsToRowContainer(animatingRow, stationaryRow, direction, startOffset, endOffset, layerOpacities);

        // The animation runs on top of the non-stretched layer position and opacity.
        auto layerPosition = positionForRowContainerLayer(animatingRow.pages);
        animatingRow.containerLayer->setPosition(layerPosition);
        animatingRow.protectedContainerLayer()->setOpacity(1);
        stationaryRow.protectedContainerLayer()->setOpacity(1);

        return transitionDuration;
    };

    switch (*m_transitionDirection) {
    case TransitionDirection::PreviousHorizontal:
    case TransitionDirection::PreviousVertical: {
        // Top page animates down, fading in. Bottom page fades out.
        auto animatingRowIndex = additionalVisibleRowIndexForDirection(*m_transitionDirection);
        if (!animatingRowIndex)
            return;

        auto& animatingRow = m_rows[*animatingRowIndex];
        auto& stationaryRow = m_rows[m_visibleRowIndex];
        auto rowSize = rowContainerSize(animatingRow.pages);
        auto endOffset = FloatSize { };
        transitionDuration = setupAnimations(animatingRow, stationaryRow, *m_transitionDirection, m_stretchDistance, endOffset, rowSize);
        break;
    }

    case TransitionDirection::NextHorizontal:
    case TransitionDirection::NextVertical: {
        // Top page animates up, fading out. Botom page fades in.
        auto additionalVisibleRowIndex = additionalVisibleRowIndexForDirection(*m_transitionDirection);
        if (!additionalVisibleRowIndex)
            return;

        auto& animatingRow = m_rows[m_visibleRowIndex];
        auto& stationaryRow = m_rows[*additionalVisibleRowIndex];
        auto rowSize = rowContainerSize(animatingRow.pages);
        auto endOffset = layerOffsetForStretch(*m_transitionDirection, rowSize, rowSize);
        transitionDuration = setupAnimations(animatingRow, stationaryRow, *m_transitionDirection, m_stretchDistance, endOffset, rowSize);
        break;
    }
    }

    m_transitionEndTimer.startOneShot(transitionDuration);
}

void PDFDiscretePresentationController::transitionEndTimerFired()
{
    if (!m_transitionDirection)
        return;

    switch (*m_transitionDirection) {
    case TransitionDirection::PreviousHorizontal:
    case TransitionDirection::PreviousVertical:
        goToPreviousRow(Animated::No);
        break;
    case TransitionDirection::NextHorizontal:
    case TransitionDirection::NextVertical:
        goToNextRow(Animated::No);
        break;
    }

    updateState(PageTransitionState::Idle);
}

std::optional<unsigned> PDFDiscretePresentationController::additionalVisibleRowIndexForDirection(TransitionDirection direction) const
{
    std::optional<unsigned> additionalVisibleRow;
    if (m_transitionDirection) {
        if (isPreviousDirection(*m_transitionDirection) && m_visibleRowIndex > 0)
            additionalVisibleRow = m_visibleRowIndex - 1;
        else if (isNextDirection(*m_transitionDirection) && m_visibleRowIndex < m_rows.size() - 1)
            additionalVisibleRow = m_visibleRowIndex + 1;
    }

    return additionalVisibleRow;
}

void PDFDiscretePresentationController::updateLayerVisibilityForTransitionState(PageTransitionState previousState)
{
    switch (m_transitionState) {
    case PageTransitionState::Idle:
        updateLayersAfterChangeInVisibleRow(m_visibleRowIndex);
        break;

    case PageTransitionState::DeterminingStretchAxis:
    case PageTransitionState::Settling:
    case PageTransitionState::Animating:
        break;

    case PageTransitionState::Stretching:
    case PageTransitionState::StartingAnimationFromStationary:
    case PageTransitionState::StartingAnimationFromMomentum: {
        std::optional<unsigned> additionalVisibleRow;
        if (m_transitionDirection)
            additionalVisibleRow = additionalVisibleRowIndexForDirection(*m_transitionDirection);

        updateLayersAfterChangeInVisibleRow(additionalVisibleRow);
        break;
    }
    }
}

void PDFDiscretePresentationController::applyWheelEventDelta(FloatSize delta)
{
    auto directionFromDelta = [](FloatSize accumulatedDelta) -> std::optional<TransitionDirection> {
        // 3x the distance on one axis than the other, and at least 3px of movement.
        auto horizontalDelta = std::abs(accumulatedDelta.width());
        auto verticalDelta = std::abs(accumulatedDelta.height());

        static constexpr auto minimumAxisMovement = 3.0f;
        if (std::max(horizontalDelta, verticalDelta) < minimumAxisMovement)
            return { };

        static constexpr auto minimumAxisRatio = 1.5f;

        if (horizontalDelta > verticalDelta && (!verticalDelta || horizontalDelta / verticalDelta >= minimumAxisRatio))
            return accumulatedDelta.width() < 0 ? TransitionDirection::PreviousHorizontal : TransitionDirection::NextHorizontal;

        if (verticalDelta > horizontalDelta && (!horizontalDelta || verticalDelta / horizontalDelta >= minimumAxisRatio))
            return accumulatedDelta.height() < 0 ? TransitionDirection::PreviousVertical : TransitionDirection::NextVertical;

        return { };
    };

    auto stretchDeltaConstrainedForTransitionDirection = [](FloatSize delta, TransitionDirection direction) {
        switch (direction) {
        case TransitionDirection::PreviousHorizontal:
            if (delta.width() > 0)
                delta.setWidth(0);
            delta.setHeight(0);
            break;
        case TransitionDirection::PreviousVertical:
            delta.setWidth(0);
            if (delta.height() > 0)
                delta.setHeight(0);
            break;
        case TransitionDirection::NextHorizontal:
            delta.setHeight(0);
            break;
        case TransitionDirection::NextVertical:
            delta.setWidth(0);
            if (delta.height() < 0)
                delta.setHeight(0);
            break;
        }
        return delta;
    };

    if (!m_transitionDirection) {
        ASSERT(m_transitionState == PageTransitionState::DeterminingStretchAxis);
        m_unappliedStretchDelta += delta;

        if (auto direction = directionFromDelta(m_unappliedStretchDelta)) {
            if (!canTransitionInDirection(*direction)) {
                updateState(PageTransitionState::Idle);
                return;
            }

            m_transitionDirection = *direction;
            m_stretchDistance = stretchDeltaConstrainedForTransitionDirection(std::exchange(m_unappliedStretchDelta, { }), *m_transitionDirection);
            updateState(PageTransitionState::Stretching);
            // FIXME: <https://webkit.org/b/276981> Need to check if we're allowed in this direction.
        }
        return;
    }

    ASSERT(m_transitionDirection);
    // FIXME: <https://webkit.org/b/276981> Should we consult NSScrollWheelMultiplier like ScrollingEffectsController does?
    m_stretchDistance = stretchDeltaConstrainedForTransitionDirection(m_stretchDistance + delta, *m_transitionDirection);

    LOG_WITH_STREAM(PDF, stream << "PDFDiscretePresentationController::applyWheelEventDelta " << delta << " - stretch " << m_stretchDistance);

}

float PDFDiscretePresentationController::relevantAxisForDirection(TransitionDirection direction, FloatSize size)
{
    switch (direction) {
    case TransitionDirection::PreviousHorizontal:
    case TransitionDirection::NextHorizontal:
        return size.width();
    case TransitionDirection::PreviousVertical:
    case TransitionDirection::NextVertical:
        return size.height();
    }
    return 0;
}

void PDFDiscretePresentationController::setRelevantAxisForDirection(TransitionDirection direction, FloatSize& size, float value)
{
    switch (direction) {
    case TransitionDirection::PreviousHorizontal:
    case TransitionDirection::NextHorizontal:
        size.setWidth(value);
        break;
    case TransitionDirection::PreviousVertical:
    case TransitionDirection::NextVertical:
        size.setHeight(value);
        break;
    }
}

std::array<std::array<float, 2>, 2> PDFDiscretePresentationController::layerOpacitiesForStretchOffset(TransitionDirection direction, FloatSize layerOffset, FloatSize rowSize) const
{
    auto topLayerThresholdDistance = relevantAxisForDirection(direction, rowSize) / 2;
    switch (direction) {
    case TransitionDirection::PreviousHorizontal:
    case TransitionDirection::PreviousVertical: {
        auto topLayerStartOpacity = std::min(std::abs(relevantAxisForDirection(direction, layerOffset)), topLayerThresholdDistance) / topLayerThresholdDistance;
        auto bottomStartOpacity = std::min(std::abs(relevantAxisForDirection(direction, layerOffset)), pageSwapDistanceThreshold) / pageSwapDistanceThreshold;
        return { {
            { 1 - topLayerStartOpacity, 1.0f },
            { bottomStartOpacity, 0.0f }
        } };
    }
    case TransitionDirection::NextHorizontal:
    case TransitionDirection::NextVertical: {
        auto topLayerStartOpacity = std::min(std::abs(relevantAxisForDirection(direction, layerOffset)), topLayerThresholdDistance) / topLayerThresholdDistance;
        auto bottomStartOpacity = std::min(std::abs(relevantAxisForDirection(direction, layerOffset)), pageSwapDistanceThreshold) / pageSwapDistanceThreshold;
        return { {
            { 1 - topLayerStartOpacity, 0.0f },
            { bottomStartOpacity, 1.0f }
        } };
    }
    }
    return { { { 1, 1 }, { 1, 1 } } };
}

// FIXME: <https://webkit.org/b/276981> The gestures should be zoom-indpendent.
FloatSize PDFDiscretePresentationController::layerOffsetForStretch(TransitionDirection direction, FloatSize stretchDistance, FloatSize rowSize) const
{
    auto constrainedForDirection = [](FloatSize size, TransitionDirection direction) {
        switch (direction) {
        case TransitionDirection::PreviousHorizontal:
        case TransitionDirection::NextHorizontal:
            return FloatSize { size.width(), 0 };
        case TransitionDirection::PreviousVertical:
        case TransitionDirection::NextVertical:
            return FloatSize { 0, size.height() };
        }
    };

    switch (direction) {
    // Pulling from the left and from the top, respectively.
    case TransitionDirection::PreviousHorizontal:
    case TransitionDirection::PreviousVertical: {
        // The previous page starts showing about 1/2 of the way in.
        static constexpr float previousPageStartProportion = 0.5;

        auto startOffset = constrainedForDirection(rowSize, direction);
        startOffset.scale(previousPageStartProportion);
        return -startOffset - stretchDistance;
    }

    case TransitionDirection::NextHorizontal: // Pushing right.
    case TransitionDirection::NextVertical: // Pushing up.
        return constrainedForDirection(-stretchDistance, direction);
    }
    return { };
}

void PDFDiscretePresentationController::updateLayersForTransitionState()
{
    switch (m_transitionState) {
    case PageTransitionState::Idle: {
        // We could optimize by only touching rows we know were animating.
        for (auto& row : m_rows) {
            auto layerPosition = positionForRowContainerLayer(row.pages);
            row.containerLayer->setPosition(layerPosition);
            row.protectedContainerLayer()->setOpacity(1);
            row.containerLayer->removeAnimation("move"_s, { });
            row.containerLayer->removeAnimation("fade"_s, { });
        }
        break;
    }
    case PageTransitionState::DeterminingStretchAxis:
        break;
    case PageTransitionState::Stretching:
    case PageTransitionState::Settling:
    case PageTransitionState::StartingAnimationFromStationary:
    case PageTransitionState::StartingAnimationFromMomentum: {
        if (!m_transitionDirection)
            return;

        auto additionalVisibleRowIndex = additionalVisibleRowIndexForDirection(*m_transitionDirection);

        // The lower index (which is on top) is the one that moves.
        unsigned topLayerRowIndex = std::min(m_visibleRowIndex, additionalVisibleRowIndex.value_or(m_visibleRowIndex));
        auto& topLayerRow = m_rows[topLayerRowIndex];
        auto rowSize = rowContainerSize(topLayerRow.pages);
        auto stretchOffset = layerOffsetForStretch(*m_transitionDirection, m_stretchDistance, rowSize);
        auto layerPosition = positionForRowContainerLayer(topLayerRow.pages);
        layerPosition += stretchOffset;
        auto layerOpacities = layerOpacitiesForStretchOffset(*m_transitionDirection, stretchOffset, rowSize);

        switch (*m_transitionDirection) {
        case TransitionDirection::PreviousHorizontal:
        case TransitionDirection::PreviousVertical: {
            // Previous page pulls down up from the top or right.
            topLayerRow.containerLayer->setPosition(layerPosition);
            topLayerRow.protectedContainerLayer()->setOpacity(layerOpacities[topLayerIndex][startIndex]);
            if (additionalVisibleRowIndex) {
                auto& bottomRow = m_rows[topLayerRowIndex + 1];
                bottomRow.protectedContainerLayer()->setOpacity(layerOpacities[bottomLayerIndex][startIndex]);
            }
            break;
        }
        case TransitionDirection::NextHorizontal:
        case TransitionDirection::NextVertical: {
            // Current page pushes up from the bottom, revealing the next page.
            topLayerRow.containerLayer->setPosition(layerPosition);
            topLayerRow.containerLayer->setOpacity(layerOpacities[topLayerIndex][startIndex]);
            if (additionalVisibleRowIndex) {
                auto& bottomRow = m_rows[topLayerRowIndex + 1];
                bottomRow.containerLayer->setOpacity(layerOpacities[bottomLayerIndex][startIndex]);
            }
            break;
        }
        }
        break;
    }

    case PageTransitionState::Animating:
        break;
    }
}

#pragma mark -

bool PDFDiscretePresentationController::canGoToNextRow() const
{
    if (m_rows.isEmpty())
        return false;

    return m_visibleRowIndex < m_rows.size() - 1;
}

bool PDFDiscretePresentationController::canGoToPreviousRow() const
{
    return m_visibleRowIndex > 0;
}

void PDFDiscretePresentationController::goToNextRow(Animated)
{
    if (!canGoToNextRow())
        return;

    setVisibleRow(m_visibleRowIndex + 1);
}

void PDFDiscretePresentationController::goToPreviousRow(Animated)
{
    if (!canGoToPreviousRow())
        return;

    setVisibleRow(m_visibleRowIndex - 1);
}

void PDFDiscretePresentationController::goToRowIndex(unsigned rowIndex, Animated)
{
    if (rowIndex >= m_rows.size())
        return;

    setVisibleRow(rowIndex);
}

void PDFDiscretePresentationController::setVisibleRow(unsigned rowIndex)
{
    ASSERT(rowIndex < m_rows.size());

    if (rowIndex == m_visibleRowIndex)
        return;

    m_plugin->willChangeVisibleRow();

    m_visibleRowIndex = rowIndex;
    updateLayersAfterChangeInVisibleRow();

    m_plugin->didChangeVisibleRow();
}

#pragma mark -

PDFPageCoverage PDFDiscretePresentationController::pageCoverageForContentsRect(const FloatRect& paintingRect, std::optional<PDFLayoutRow> row) const
{
    if (m_plugin->visibleOrDocumentSizeIsEmpty())
        return { };

    if (!row) {
        // PDFDiscretePresentationController layout is row-based.
        ASSERT_NOT_REACHED();
        return { };
    }

    auto contentsRect = convertFromPaintingToContents(paintingRect, row->pages[0]);
    auto paintRectInPDFLayoutCoordinates = m_plugin->convertDown(UnifiedPDFPlugin::CoordinateSpace::Contents, UnifiedPDFPlugin::CoordinateSpace::PDFDocumentLayout, contentsRect);

    auto pageCoverage = PDFPageCoverage { };

    auto addPageToCoverage = [&](PDFDocumentLayout::PageIndex pageIndex) {
        auto pageBounds = layoutBoundsForPageAtIndex(pageIndex);
        if (!pageBounds.intersects(paintRectInPDFLayoutCoordinates))
            return;

        pageCoverage.append(PerPageInfo { pageIndex, pageBounds, paintRectInPDFLayoutCoordinates });
    };

    for (auto pageIndex : row->pages)
        addPageToCoverage(pageIndex);

    return pageCoverage;
}

PDFPageCoverageAndScales PDFDiscretePresentationController::pageCoverageAndScalesForContentsRect(const FloatRect& clipRect, std::optional<PDFLayoutRow> row, float tilingScaleFactor) const
{
    if (m_plugin->visibleOrDocumentSizeIsEmpty())
        return { { }, { }, 1, 1, 1 };

    if (!row) {
        // PDFDiscretePresentationController layout is row-based.
        ASSERT_NOT_REACHED();
        return { };
    }

    auto pageCoverageAndScales = PDFPageCoverageAndScales { pageCoverageForContentsRect(clipRect, row) };

    pageCoverageAndScales.deviceScaleFactor = m_plugin->deviceScaleFactor();
    pageCoverageAndScales.pdfDocumentScale = m_plugin->documentLayout().scale();
    pageCoverageAndScales.tilingScaleFactor = tilingScaleFactor;
    pageCoverageAndScales.contentsOffset = contentsOffsetForPage(row->pages[0]);

    return pageCoverageAndScales;
}

FloatSize PDFDiscretePresentationController::contentsOffsetForPage(PDFDocumentLayout::PageIndex pageIndex) const
{
    auto rowIndex = m_plugin->documentLayout().rowIndexForPageIndex(pageIndex);
    if (rowIndex >= m_rows.size())
        return { };

    return m_rows[rowIndex].contentsOffset;
}

FloatRect PDFDiscretePresentationController::convertFromContentsToPainting(const FloatRect& rect, std::optional<PDFDocumentLayout::PageIndex> pageIndex) const
{
    if (!pageIndex) {
        ASSERT_NOT_REACHED();
        return { };
    }

    auto rowOffset = contentsOffsetForPage(*pageIndex);
    auto adjustedRect = rect;
    adjustedRect.move(-rowOffset);
    return adjustedRect;
}

FloatRect PDFDiscretePresentationController::convertFromPaintingToContents(const FloatRect& rect, std::optional<PDFDocumentLayout::PageIndex> pageIndex) const
{
    if (!pageIndex) {
        ASSERT_NOT_REACHED();
        return { };
    }

    auto rowOffset = contentsOffsetForPage(*pageIndex);
    auto adjustedRect = rect;
    adjustedRect.move(rowOffset);
    return adjustedRect;
}

void PDFDiscretePresentationController::deviceOrPageScaleFactorChanged()
{
    for (auto& row : m_rows) {
        // We need to manually propagate noteDeviceOrPageScaleFactorChangedIncludingDescendants to the layers of unparented rows.
        if (!row.containerLayer->parent())
            row.protectedContainerLayer()->noteDeviceOrPageScaleFactorChangedIncludingDescendants();
    }
}

void PDFDiscretePresentationController::setupLayers(GraphicsLayer& scrolledContentsLayer)
{
    if (!m_rowsContainerLayer) {
        m_rowsContainerLayer = createGraphicsLayer("Rows container"_s, GraphicsLayer::Type::Normal);
        m_rowsContainerLayer->setAnchorPoint({ });
        scrolledContentsLayer.addChild(*m_rowsContainerLayer);
    }

    bool displayModeChanged = !m_displayModeAtLastLayerSetup || m_displayModeAtLastLayerSetup != m_plugin->documentLayout().displayMode();
    m_displayModeAtLastLayerSetup = m_plugin->documentLayout().displayMode();

    if (displayModeChanged) {
        clearAsyncRenderer();
        m_rows.clear();
    }

    buildRows();
}

void PDFDiscretePresentationController::buildRows()
{
    // For incrementally loading PDFs we may have some rows already. This has to handle incremental changes.
    auto layoutRows = m_plugin->documentLayout().rows();

    // FIXME: <https://webkit.org/b/276981> Need to unregister for removed layers.
    m_rows.resize(layoutRows.size());

    auto createRowPageBackgroundContainerLayers = [&](size_t rowIndex, PDFLayoutRow& layoutRow, RowData& row) {
        ASSERT(!row.leftPageContainerLayer);

        auto leftPageIndex = layoutRow.pages[0];

        row.leftPageContainerLayer = makePageContainerLayer(leftPageIndex);
        RefPtr pageBackgroundLayer = pageBackgroundLayerForPageContainerLayer(*row.protectedLeftPageContainerLayer());
        m_layerIDToRowIndexMap.add(*pageBackgroundLayer->primaryLayerID(), rowIndex);

        if (row.pages.numPages() == 1) {
            ASSERT(!row.rightPageContainerLayer);
            return;
        }

        auto rightPageIndex = layoutRow.pages[1];
        row.rightPageContainerLayer = makePageContainerLayer(rightPageIndex);
        RefPtr rightPageBackgroundLayer = pageBackgroundLayerForPageContainerLayer(*row.protectedRightPageContainerLayer());
        m_layerIDToRowIndexMap.add(*rightPageBackgroundLayer->primaryLayerID(), rowIndex);
    };

    auto parentRowLayers = [](RowData& row) {
        ASSERT(row.containerLayer->children().isEmpty());
        row.containerLayer->addChild(*row.protectedLeftPageContainerLayer());
        if (row.rightPageContainerLayer)
            row.containerLayer->addChild(*row.protectedRightPageContainerLayer());

        row.containerLayer->addChild(*row.protectedContentsLayer());
#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
        row.containerLayer->addChild(*row.protectedSelectionLayer());
#endif
    };

    auto ensureLayersForRow = [&](size_t rowIndex, PDFLayoutRow& layoutRow, RowData& row) {
        if (row.containerLayer)
            return;

        row.containerLayer = createGraphicsLayer(makeString("Row container "_s, rowIndex), GraphicsLayer::Type::Normal);
        row.containerLayer->setAnchorPoint({ });

        createRowPageBackgroundContainerLayers(rowIndex, layoutRow, row);

        // This contents layer is used to paint both pages in two-up; it spans across both backgrounds.
        RefPtr rowContentsLayer = row.contentsLayer = createGraphicsLayer(makeString("Row contents "_s, rowIndex), GraphicsLayer::Type::TiledBacking);
        rowContentsLayer->setAnchorPoint({ });
        rowContentsLayer->setDrawsContent(true);
        rowContentsLayer->setAcceleratesDrawing(m_plugin->canPaintSelectionIntoOwnedLayer());

        // This is the call that enables async rendering.
        asyncRenderer()->startTrackingLayer(*rowContentsLayer);

        m_layerIDToRowIndexMap.set(*rowContentsLayer->primaryLayerID(), rowIndex);

#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
        RefPtr rowSelectionLayer = row.selectionLayer = createGraphicsLayer(makeString("Row selection "_s, rowIndex), GraphicsLayer::Type::TiledBacking);
        rowSelectionLayer->setAnchorPoint({ });
        rowSelectionLayer->setDrawsContent(true);
        rowSelectionLayer->setAcceleratesDrawing(true);
        rowSelectionLayer->setBlendMode(BlendMode::Multiply);
        m_layerIDToRowIndexMap.set(*rowSelectionLayer->primaryLayerID(), rowIndex);
#endif

        parentRowLayers(row);
    };

    for (size_t rowIndex = 0; rowIndex < layoutRows.size(); ++rowIndex) {
        auto& layoutRow = layoutRows[rowIndex];
        auto& row = m_rows[rowIndex];

        row.pages = layoutRow;

        ensureLayersForRow(rowIndex, layoutRow, row);
    }
}

FloatPoint PDFDiscretePresentationController::positionForRowContainerLayer(const PDFLayoutRow& row) const
{
    auto& documentLayout = m_plugin->documentLayout();

    auto rowPageBounds = documentLayout.layoutBoundsForRow(row);
    auto scaledRowBounds = rowPageBounds;
    scaledRowBounds.scale(documentLayout.scale());

    return scaledRowBounds.location();
}

FloatSize PDFDiscretePresentationController::rowContainerSize(const PDFLayoutRow& row) const
{
    auto& documentLayout = m_plugin->documentLayout();

    auto rowPageBounds = documentLayout.layoutBoundsForRow(row);
    auto scaledRowBounds = rowPageBounds;
    scaledRowBounds.scale(documentLayout.scale());

    return scaledRowBounds.size();
}

void PDFDiscretePresentationController::updateLayersOnLayoutChange(FloatSize documentSize, FloatSize centeringOffset, double scaleFactor)
{
    LOG_WITH_STREAM(PDF, stream << "PDFDiscretePresentationController::updateLayersOnLayoutChange - documentSize " << documentSize << " centeringOffset " << centeringOffset);

    auto& documentLayout = m_plugin->documentLayout();

    TransformationMatrix documentScaleTransform;
    documentScaleTransform.scale(documentLayout.scale());

    auto updatePageContainerLayerBounds = [&](GraphicsLayer* pageContainerLayer, PDFDocumentLayout::PageIndex pageIndex, const FloatRect& rowBounds) {
        auto pageBounds = layoutBoundsForPageAtIndex(pageIndex);
        // Account for row.containerLayer already being positioned by the origin of rowBounds.
        pageBounds.moveBy(-rowBounds.location());

        auto destinationRect = pageBounds;
        destinationRect.scale(documentLayout.scale());

        pageContainerLayer->setPosition(destinationRect.location());
        pageContainerLayer->setSize(destinationRect.size());

        RefPtr pageBackgroundLayer = pageBackgroundLayerForPageContainerLayer(*pageContainerLayer);
        pageBackgroundLayer->setSize(pageBounds.size());
        pageBackgroundLayer->setTransform(documentScaleTransform);
    };

    auto updateRowPageContainerLayers = [&](const RowData& row, const FloatRect& rowBounds) {
        auto leftPageIndex = row.pages.pages[0];
        updatePageContainerLayerBounds(row.leftPageContainerLayer.get(), leftPageIndex, rowBounds);

        if (row.pages.numPages() == 1)
            return;

        auto rightPageIndex = row.pages.pages[1];
        updatePageContainerLayerBounds(row.rightPageContainerLayer.get(), rightPageIndex, rowBounds);
    };

    TransformationMatrix transform;
    transform.scale(scaleFactor);
    transform.translate(centeringOffset.width(), centeringOffset.height());

    protectedRowsContainerLayer()->setTransform(transform);

    for (auto& row : m_rows) {
        // Same as positionForRowContainerLayer().
        auto rowPageBounds = documentLayout.layoutBoundsForRow(row.pages);
        auto scaledRowBounds = rowPageBounds;
        scaledRowBounds.scale(documentLayout.scale());

        RefPtr rowContainerLayer = row.containerLayer;
        rowContainerLayer->setPosition(scaledRowBounds.location());
        rowContainerLayer->setSize(scaledRowBounds.size());

        updateRowPageContainerLayers(row, rowPageBounds);

        RefPtr rowContentsLayer = row.contentsLayer;
        rowContentsLayer->setPosition({ });

        bool needsRepaint = false;
        // This contents offset accounts for containerLayer being positioned with an offset from the edge of the
        // plugin's "contents" area. When painting into row.contentsLayer, we need to take this into account.
        // This is done via convertFromContentsToPainting()/convertFromPaintingToContents().
        auto newContentsOffset = toFloatSize(scaledRowBounds.location());
        if (row.contentsOffset != newContentsOffset) {
            row.contentsOffset = newContentsOffset;
            needsRepaint = true;
        }

        if (rowContentsLayer->size() != scaledRowBounds.size()) {
            rowContentsLayer->setSize(scaledRowBounds.size());
            needsRepaint = true;
        }

        if (needsRepaint)
            rowContentsLayer->setNeedsDisplay();

#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
        RefPtr rowSelectionLayer = row.selectionLayer;
        rowSelectionLayer->setPosition({ });
        rowSelectionLayer->setSize(scaledRowBounds.size());
        if (needsRepaint)
            rowSelectionLayer->setNeedsDisplay();
#endif
    }

    updateLayersAfterChangeInVisibleRow();
}

void PDFDiscretePresentationController::updateLayersAfterChangeInVisibleRow(std::optional<unsigned> additionalVisibleRowIndex)
{
    if (m_visibleRowIndex >= m_rows.size())
        return;

    if (additionalVisibleRowIndex && *additionalVisibleRowIndex >= m_rows.size())
        additionalVisibleRowIndex = { };

    RefPtr rowsContainerLayer = m_rowsContainerLayer;
    rowsContainerLayer->removeAllChildren();

    auto& visibleRow = m_rows[m_visibleRowIndex];

    auto updateRowTiledLayers = [](RowData& row, bool isInWindow) {
        row.contentsLayer->setIsInWindow(isInWindow);
#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
        row.selectionLayer->setIsInWindow(isInWindow);
#endif
    };

    bool isInWindow = m_plugin->isInWindow();
    updateRowTiledLayers(visibleRow, isInWindow);

    RefPtr rowContainer = visibleRow.containerLayer;
    rowsContainerLayer->addChild(rowContainer.releaseNonNull());

    if (additionalVisibleRowIndex) {
        auto& additionalVisibleRow = m_rows[*additionalVisibleRowIndex];
        updateRowTiledLayers(additionalVisibleRow, isInWindow);

        RefPtr rowContainer = additionalVisibleRow.containerLayer;
        if (*additionalVisibleRowIndex < m_visibleRowIndex)
            rowsContainerLayer->addChild(rowContainer.releaseNonNull());
        else
            rowsContainerLayer->addChildAtIndex(rowContainer.releaseNonNull(), 0);
    }
}

void PDFDiscretePresentationController::updateIsInWindow(bool isInWindow)
{
    for (auto& row : m_rows) {
        row.contentsLayer->setIsInWindow(isInWindow);
#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
        row.selectionLayer->setIsInWindow(isInWindow);
#endif
    }
}

void PDFDiscretePresentationController::updateDebugBorders(bool showDebugBorders, bool showRepaintCounters)
{
    auto propagateSettingsToLayer = [&] (GraphicsLayer& layer) {
        layer.setShowDebugBorder(showDebugBorders);
        layer.setShowRepaintCounter(showRepaintCounters);
    };

    for (auto& row : m_rows) {
        propagateSettingsToLayer(*row.containerLayer);

        propagateSettingsToLayer(*row.leftPageContainerLayer);
        propagateSettingsToLayer(*row.leftPageBackgroundLayer());

        if (row.rightPageContainerLayer) {
            propagateSettingsToLayer(*row.rightPageContainerLayer);
            propagateSettingsToLayer(*row.rightPageBackgroundLayer());
        }

        propagateSettingsToLayer(*row.contentsLayer);
#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
        propagateSettingsToLayer(*row.selectionLayer);
#endif
    }

    if (RefPtr asyncRenderer = asyncRendererIfExists())
        asyncRenderer->setShowDebugBorders(showDebugBorders);
}

void PDFDiscretePresentationController::updateForCurrentScrollability(OptionSet<TiledBackingScrollability> scrollability)
{
    if (m_visibleRowIndex >= m_rows.size())
        return;

    auto& visibleRow = m_rows[m_visibleRowIndex];
    if (auto* tiledBacking = visibleRow.contentsLayer->tiledBacking())
        tiledBacking->setScrollability(scrollability);
}

void PDFDiscretePresentationController::repaintForIncrementalLoad()
{
    for (auto& row : m_rows) {
        if (RefPtr leftBackgroundLayer = row.leftPageBackgroundLayer())
            leftBackgroundLayer->setNeedsDisplay();

        if (RefPtr rightBackgroundLayer = row.leftPageBackgroundLayer())
            rightBackgroundLayer->setNeedsDisplay();

        row.protectedContentsLayer()->setNeedsDisplay();
#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
        row.protectedSelectionLayer()->setNeedsDisplay();
#endif
    }
}

void PDFDiscretePresentationController::setNeedsRepaintInDocumentRect(OptionSet<RepaintRequirement> repaintRequirements, const FloatRect& rectInDocumentCoordinates, std::optional<PDFLayoutRow> layoutRow)
{
    ASSERT(layoutRow);
    if (!layoutRow)
        return;

    auto rowIndex = m_plugin->documentLayout().rowIndexForPageIndex(layoutRow->pages[0]);
    if (rowIndex >= m_rows.size())
        return;

    auto& row = m_rows[rowIndex];

    auto contentsRect = m_plugin->convertUp(UnifiedPDFPlugin::CoordinateSpace::PDFDocumentLayout, UnifiedPDFPlugin::CoordinateSpace::Contents, rectInDocumentCoordinates);
    contentsRect = convertFromContentsToPainting(contentsRect, row.pages.pages[0]);

    if (repaintRequirements.contains(RepaintRequirement::PDFContent)) {
        if (RefPtr asyncRenderer = asyncRendererIfExists())
            asyncRenderer->pdfContentChangedInRect(row.contentsLayer.get(), contentsRect, layoutRow);
    }

#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    if (repaintRequirements.contains(RepaintRequirement::Selection) && m_plugin->canPaintSelectionIntoOwnedLayer()) {
        RefPtr { row.selectionLayer }->setNeedsDisplayInRect(contentsRect);
        if (repaintRequirements.hasExactlyOneBitSet())
            return;
    }
#endif

    RefPtr { row.contentsLayer.get() }->setNeedsDisplayInRect(contentsRect);
}

void PDFDiscretePresentationController::didGeneratePreviewForPage(PDFDocumentLayout::PageIndex pageIndex)
{
    auto rowIndex = m_plugin->documentLayout().rowIndexForPageIndex(pageIndex);
    if (rowIndex >= m_rows.size())
        return;

    auto& row = m_rows[rowIndex];
    if (RefPtr backgroundLayer = row.backgroundLayerForPageIndex(pageIndex))
        backgroundLayer->setNeedsDisplay();
}

#pragma mark -

auto PDFDiscretePresentationController::pdfPositionForCurrentView(bool preservePosition) const -> std::optional<VisiblePDFPosition>
{
    if (!preservePosition)
        return { };

    auto& documentLayout = m_plugin->documentLayout();
    if (!documentLayout.hasLaidOutPDFDocument())
        return { };

    auto visibleRow = this->visibleRow();
    if (!visibleRow)
        return { };

    auto pageIndex = visibleRow->pages[0];
    auto pageBounds = documentLayout.layoutBoundsForPageAtIndex(pageIndex);
    auto topLeftInDocumentSpace = m_plugin->convertDown(UnifiedPDFPlugin::CoordinateSpace::Plugin, UnifiedPDFPlugin::CoordinateSpace::PDFDocumentLayout, FloatPoint { });
    auto pagePoint = documentLayout.documentToPDFPage(FloatPoint { pageBounds.center().x(), topLeftInDocumentSpace.y() }, pageIndex);

    return VisiblePDFPosition { pageIndex, pagePoint };
}

void PDFDiscretePresentationController::restorePDFPosition(const VisiblePDFPosition& info)
{
    // FIXME: This needs to respect the point as well, in order for transitions from
    // scrolling mode to discrete mode to not lose your place in the page.
    ensurePageIsVisible(info.pageIndex);
}

void PDFDiscretePresentationController::ensurePageIsVisible(PDFDocumentLayout::PageIndex pageIndex)
{
    auto rowIndex = m_plugin->documentLayout().rowIndexForPageIndex(pageIndex);
    setVisibleRow(rowIndex);
}

#pragma mark -

void PDFDiscretePresentationController::notifyFlushRequired(const GraphicsLayer*)
{
    m_plugin->scheduleRenderingUpdate();
}

// This is a GraphicsLayerClient function. The return value is used to compute layer contentsScale, so we don't
// want to use the normalized scale factor.
float PDFDiscretePresentationController::pageScaleFactor() const
{
    return m_plugin->nonNormalizedScaleFactor();
}

float PDFDiscretePresentationController::deviceScaleFactor() const
{
    return m_plugin->deviceScaleFactor();
}

std::optional<float> PDFDiscretePresentationController::customContentsScale(const GraphicsLayer* layer) const
{
    auto* rowData = rowDataForLayerID(*layer->primaryLayerID());
    if (!rowData)
        return { };

    if (rowData->isPageBackgroundLayer(layer))
        return m_plugin->scaleForPagePreviews();

    return { };
}

bool PDFDiscretePresentationController::layerNeedsPlatformContext(const GraphicsLayer* layer) const
{
    if (m_plugin->canPaintSelectionIntoOwnedLayer())
        return false;

    // We need a platform context if the plugin can not paint selections into its own layer,
    // since we would then have to vend a platform context that PDFKit can paint into.
    // However, this constraint only applies for the contents layer. No other layer needs to be WP-backed.
    auto* rowData = rowDataForLayerID(*layer->primaryLayerID());
    if (!rowData)
        return false;

    return layer == rowData->contentsLayer.get();
}

void PDFDiscretePresentationController::tiledBackingUsageChanged(const GraphicsLayer* layer, bool usingTiledBacking)
{
    if (usingTiledBacking)
        layer->tiledBacking()->setIsInWindow(m_plugin->isInWindow());
}

void PDFDiscretePresentationController::paintBackgroundLayerForRow(const GraphicsLayer* layer, GraphicsContext& context, const FloatRect& clipRect, unsigned rowIndex)
{
    if (rowIndex >= m_rows.size())
        return;

    auto& row = m_rows[rowIndex];

    auto paintOnePageBackground = [&](PDFDocumentLayout::PageIndex pageIndex) {
        auto destinationRect = layoutBoundsForPageAtIndex(pageIndex);
        destinationRect.setLocation({ });

        if (RefPtr asyncRenderer = asyncRendererIfExists())
            asyncRenderer->paintPagePreview(context, clipRect, destinationRect, pageIndex);
    };

    if (layer == row.leftPageBackgroundLayer().get()) {
        paintOnePageBackground(row.pages.pages[0]);
        return;
    }

    if (layer == row.rightPageBackgroundLayer().get() && row.pages.numPages() == 2) {
        paintOnePageBackground(row.pages.pages[1]);
        return;
    }
}

auto PDFDiscretePresentationController::rowDataForLayerID(PlatformLayerIdentifier layerID) const -> const RowData*
{
    auto rowIndex = m_layerIDToRowIndexMap.getOptional(layerID);
    if (!rowIndex)
        return nullptr;

    if (*rowIndex >= m_rows.size())
        return nullptr;

    return &m_rows[*rowIndex];
}

std::optional<PDFLayoutRow> PDFDiscretePresentationController::visibleRow() const
{
    if (m_visibleRowIndex >= m_rows.size())
        return { };

    return m_rows[m_visibleRowIndex].pages;
}

std::optional<PDFLayoutRow> PDFDiscretePresentationController::rowForLayerID(PlatformLayerIdentifier layerID) const
{
    auto* rowData = rowDataForLayerID(layerID);
    if (rowData)
        return rowData->pages;

    return { };
}

void PDFDiscretePresentationController::paintContents(const GraphicsLayer* layer, GraphicsContext& context, const FloatRect& clipRect, OptionSet<GraphicsLayerPaintBehavior>)
{
    auto rowIndex = m_layerIDToRowIndexMap.getOptional(*layer->primaryLayerID());
    if (!rowIndex)
        return;

    if (*rowIndex >= m_rows.size())
        return;

    auto& rowData = m_rows[*rowIndex];
    if (rowData.isPageBackgroundLayer(layer)) {
        paintBackgroundLayerForRow(layer, context, clipRect, *rowIndex);
        return;
    }

    if (layer == rowData.contentsLayer.get()) {
        RefPtr asyncRenderer = asyncRendererIfExists();
        m_plugin->paintPDFContent(layer, context, clipRect, rowData.pages, UnifiedPDFPlugin::PaintingBehavior::All, asyncRenderer.get());
        return;
    }

#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    if (layer == rowData.selectionLayer.get())
        return paintPDFSelection(layer, context, clipRect, rowData.pages);
#endif
}

#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
void PDFDiscretePresentationController::paintPDFSelection(const GraphicsLayer* layer, GraphicsContext& context, const FloatRect& clipRect, std::optional<PDFLayoutRow> row)
{
    m_plugin->paintPDFSelection(layer, context, clipRect, row);
}
#endif

#pragma mark -

bool PDFDiscretePresentationController::RowData::isPageBackgroundLayer(const GraphicsLayer* layer) const
{
    if (!layer)
        return false;

    if (layer == leftPageBackgroundLayer().get())
        return true;

    if (layer == rightPageBackgroundLayer().get())
        return true;

    return false;
}

RefPtr<GraphicsLayer> PDFDiscretePresentationController::RowData::leftPageBackgroundLayer() const
{
    return PDFPresentationController::pageBackgroundLayerForPageContainerLayer(*protectedLeftPageContainerLayer());
}

RefPtr<GraphicsLayer> PDFDiscretePresentationController::RowData::rightPageBackgroundLayer() const
{
    if (!rightPageContainerLayer)
        return nullptr;

    return PDFPresentationController::pageBackgroundLayerForPageContainerLayer(*protectedRightPageContainerLayer());
}

RefPtr<GraphicsLayer> PDFDiscretePresentationController::RowData::backgroundLayerForPageIndex(PDFDocumentLayout::PageIndex pageIndex) const
{
    if (pageIndex == pages.pages[0])
        return leftPageBackgroundLayer();

    if (pages.numPages() == 2 && pageIndex == pages.pages[1])
        return rightPageBackgroundLayer();

    return nullptr;
}

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
