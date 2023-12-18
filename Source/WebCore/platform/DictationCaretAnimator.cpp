/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DictationCaretAnimator.h"

#if HAVE(REDESIGNED_TEXT_CURSOR)

#include "Editing.h"
#include "FloatRoundedRect.h"
#include "Path.h"
#include "RenderBlock.h"
#include "VisibleSelection.h"

namespace WebCore {

static constexpr size_t dictationCaretAnimatorUpdateRate = 60;

static constexpr KeyFrame keyframe(size_t i)
{
    constexpr auto updateRate = 40;
    i %= updateRate;
    constexpr float inverseFrameRate = 1.f / static_cast<float>(updateRate);
    return KeyFrame { Seconds(i * inverseFrameRate), fabs(sinf(static_cast<float>(M_PI * i * inverseFrameRate))) };
}

constexpr auto tailBlurRadius(float cursorHeight)
{
    return (10.f * cursorHeight) / 12.f;
}

constexpr auto caretBlurRadius(float cursorHeight)
{
    return (8.f * cursorHeight) / 12.f;
}

constexpr auto coneStart(float cursorHeight)
{
    return (4.f * cursorHeight) / 38.f;
}

constexpr auto coneEnd(float cursorHeight)
{
    return (152.f * cursorHeight) / 38.f;
}

size_t DictationCaretAnimator::keyframeCount() const
{
    return 2 * dictationCaretAnimatorUpdateRate;
}

DictationCaretAnimator::DictationCaretAnimator(CaretAnimationClient& client)
    : CaretAnimator(client)
{
}

Seconds DictationCaretAnimator::keyframeTimeDelta() const
{
    return Seconds(1.f / static_cast<float>(dictationCaretAnimatorUpdateRate));
}

void DictationCaretAnimator::setBlinkingSuspended(bool suspended)
{
    if (suspended == isBlinkingSuspended())
        return;

    if (!suspended) {
        m_currentKeyframeIndex = 1;
        m_blinkTimer.startOneShot(keyframeTimeDelta());
        resetGlowTail(m_client.localCaretRect());
    }

    CaretAnimator::setBlinkingSuspended(suspended);
}

FloatRect DictationCaretAnimator::computeTailRect() const
{
    auto caretX = m_localCaretRect.x();
    auto cursorHeight = m_localCaretRect.height();

    return { std::min(m_glowStart, caretX), m_localCaretRect.y(), fabs(caretX - m_glowStart), cursorHeight };
}

int DictationCaretAnimator::computeScrollLeft() const
{
    auto document = m_client.document();
    if (!document)
        return 0;

    if (auto* caretNode = m_client.caretNode()) {
        if (auto* rendererForCaret = rendererForCaretPainting(caretNode))
            return rendererForCaret->scrollLeft();
    }

    return 0;
}

void DictationCaretAnimator::updateGlowTail(Seconds elapsedTime)
{
    auto document = m_client.document();
    if (!document)
        return;

    m_previousTailRect = computeTailRect();

    auto previousScrollLeft = m_scrollLeft;
    m_scrollLeft = computeScrollLeft();
    auto deltaScrollLeft = m_scrollLeft - previousScrollLeft;
    m_glowStart -= deltaScrollLeft;

    auto caretRect = m_client.localCaretRect();

    float caretPosition = caretRect.x();
    if (caretRect.y() != m_localCaretRect.y())
        resetGlowTail(caretRect);

    m_localCaretRect = caretRect;

    if (caretPosition != m_glowStart)
        updateGlowTail(caretPosition, elapsedTime);
    else
        resetGlowTail(caretRect);

    m_tailRect = computeTailRect();
}

void DictationCaretAnimator::updateAnimationProperties()
{
    auto currentTime = MonotonicTime::now();
    auto elapsedTime = currentTime - m_lastUpdateTime;
    if (elapsedTime >= keyframeTimeDelta()) {
        setOpacity(keyframe(m_currentKeyframeIndex).value);
        m_lastUpdateTime = currentTime;

        if (m_currentKeyframeIndex >= keyframeCount() - 1)
            m_currentKeyframeIndex = 0;

        m_currentKeyframeIndex++;
        updateGlowTail(elapsedTime);
        constexpr auto scaleAnimationSpeed = 4.f;
        m_initialScale = std::max(0.f, m_initialScale - scaleAnimationSpeed * static_cast<float>(elapsedTime.value()));

        m_blinkTimer.startOneShot(keyframeTimeDelta());
    }
}

void DictationCaretAnimator::start()
{
    // The default/start value of `m_currentKeyframeIndex` should be `1` since the keyframe
    // delta is the difference between `m_currentKeyframeIndex` and `m_currentKeyframeIndex - 1`
    m_currentKeyframeIndex = 1;
    m_lastUpdateTime = MonotonicTime::now();
    m_initialScale = M_PI_2;
    didStart(m_lastUpdateTime, keyframeTimeDelta());

    resetGlowTail(m_client.localCaretRect());
    m_previousTailRect = computeTailRect();
}

String DictationCaretAnimator::debugDescription() const
{
    TextStream textStream;
    textStream << "DictationCaretAnimator " << this << " glowStart " << m_glowStart << " glowEnd = " << m_localCaretRect.x();
    return textStream.release();
}

void DictationCaretAnimator::updateGlowTail(float caretPosition, Seconds elapsedSeconds)
{
    auto distanceFromPrevious = caretPosition - m_glowStart;

    auto elapsedTime = static_cast<float>(elapsedSeconds.value());

    constexpr auto easeInMultiplier = .12f;
    constexpr auto maxAnimationSpeed = .2f;
    constexpr auto minimumVelocityMultiplier = 1.0f;
    constexpr auto acceleration = .1f;
    m_animationSpeed = std::min(maxAnimationSpeed, m_animationSpeed + acceleration * elapsedTime);
    auto direction = distanceFromPrevious > 0.f ? 1.f : -1.f;
    distanceFromPrevious *= direction;
    if (distanceFromPrevious > 0) {
        if (elapsedTime <= 0)
            m_glowStart = caretPosition;
        else {
            m_glowStart += direction * std::max(1.f, elapsedTime * m_animationSpeed * std::max(minimumVelocityMultiplier, distanceFromPrevious * distanceFromPrevious * easeInMultiplier));
            if (direction * (m_glowStart - caretPosition) > 0)
                m_glowStart = caretPosition;
        }
    }
}

void DictationCaretAnimator::resetGlowTail(FloatRect localCaretRect)
{
    m_animationSpeed = 0;
    m_glowStart = localCaretRect.x();
    m_localCaretRect = localCaretRect;
    m_tailRect = computeTailRect();

    m_scrollLeft = computeScrollLeft();
}

FloatRect DictationCaretAnimator::tailRect() const
{
    return m_tailRect;
}

Path DictationCaretAnimator::makeDictationTailConePath(const FloatRect& rect) const
{
    const auto width = rect.width();
    const auto height = rect.height();
    const float minimumTailWidth = coneStart(height);
    const auto nonTruncatedTailWidth = coneEnd(height);
    const auto coneRectangleMorphConstant = (width - minimumTailWidth) / (nonTruncatedTailWidth - minimumTailWidth);
    auto verticalOffset = -height * .3f + height * 0.5f * std::min(1.f, std::max(0.f, coneRectangleMorphConstant));
    bool isLTR = isLeftToRightLayout();

    const auto verticalOffsetLTR = isLTR ? verticalOffset : 0.f;
    const auto verticalOffsetRTL = isLTR ? 0.f : verticalOffset;

    return Path(Vector<FloatPoint> {
        { rect.x(), rect.y() + verticalOffsetLTR },
        { rect.x(), rect.maxY() - verticalOffsetLTR },
        { rect.maxX(), rect.maxY() - verticalOffsetRTL },
        { rect.maxX(), rect.y() + verticalOffsetRTL }
    });
}

void DictationCaretAnimator::fillCaretTail(const FloatRect& rect, GraphicsContext& context, const Color& tailColor) const
{
    ASSERT(!rect.isZero());

    float height = rect.height();
    float width = rect.width();
    float midY = rect.y() + 0.5f * height;

    auto gradient = Gradient::create(Gradient::LinearData { FloatPoint(rect.x(), midY), FloatPoint(rect.x() + width, midY) }, { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied });
    constexpr auto glowOpacity = .75f;
    bool isLTR = isLeftToRightLayout();
    gradient->addColorStop({ isLTR ? 0.f : 1.f, tailColor.colorWithAlpha(0.1f * glowOpacity) });
    gradient->addColorStop({ isLTR ? 1.f : 0.f, tailColor.colorWithAlpha(0.35f * glowOpacity) });
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=253139 - this should be computed based on the render area
    constexpr auto shadowOffset = 10000.f;
    GraphicsDropShadow dropShadow = {
        .offset = { shadowOffset, shadowOffset },
        .radius = tailBlurRadius(rect.height()),
        .color = tailColor.colorWithAlpha(glowOpacity),
        .radiusMode = ShadowRadiusMode::Default,
    };
    context.setDropShadow(dropShadow);
    context.translate(-dropShadow.offset);
    context.setFillGradient(WTFMove(gradient));
    context.fillPath(makeDictationTailConePath(rect));
}

FloatRoundedRect DictationCaretAnimator::expandedCaretRect(const FloatRect& rect, bool fillTail) const
{
    if (m_initialScale <= 0.f && fillTail)
        return FloatRoundedRect { rect, FloatRoundedRect::Radii { 1.f } };

    auto extraScaleFactor = 1.f;
    auto pulseExpansion = 1.f;
    if (m_initialScale > 0.f)
        extraScaleFactor = 1.f + 1.4f * sinf(2.f * m_initialScale);
    else
        pulseExpansion = 0.75f * m_presentationProperties.opacity * extraScaleFactor;

    float horizontalPulseExpansion = 0.5f * pulseExpansion;
    float verticalPulseExpansion = pulseExpansion * (1.f + 3.f * (extraScaleFactor - 1.f)) - 1.f;
    FloatRect expandedRect = rect;
    expandedRect.expand(FloatBoxExtent { verticalPulseExpansion, horizontalPulseExpansion, verticalPulseExpansion, horizontalPulseExpansion });
    return FloatRoundedRect { expandedRect, FloatRoundedRect::Radii(1.f + std::max(0.f, .5f * horizontalPulseExpansion), 1.f + std::max(0.f, .5f * horizontalPulseExpansion)) };
}

void DictationCaretAnimator::paint(GraphicsContext& context, const FloatRect& rect, const Color& caretColor, const LayoutPoint& paintOffset) const
{
    auto tailRect = computeTailRect();
    bool blinkingSuspended = isBlinkingSuspended();
    bool fillTail = !blinkingSuspended && tailRect.width() > 0.f;

    GraphicsContextStateSaver stateSaver(context);
    context.resetClip();
    const auto targetOpacity = (fillTail ? 1.0 : m_presentationProperties.opacity) - m_initialScale;
    if (targetOpacity > 0 && !blinkingSuspended)
        context.setDropShadow({ { }, caretBlurRadius(rect.height()), caretColor.colorWithAlpha(targetOpacity), ShadowRadiusMode::Default });

    context.fillRoundedRect(expandedCaretRect(rect, fillTail), caretColor);

    if (fillTail) {
        tailRect.moveBy(paintOffset);
        fillCaretTail(tailRect, context, caretColor);
    }
}

bool DictationCaretAnimator::isLeftToRightLayout() const
{
    return m_glowStart <= m_localCaretRect.x();
}

LayoutRect DictationCaretAnimator::caretRepaintRectForLocalRect(LayoutRect repaintRect) const
{
    auto tailRect = unionRect(this->tailRect(), m_previousTailRect);
    tailRect = expandedCaretRect(tailRect, !isBlinkingSuspended() && tailRect.width() > 0.f).rect();
    auto height = static_cast<float>(tailRect.height());
    const auto maxBlurDiameter = 2.f * std::max(caretBlurRadius(height), tailBlurRadius(height));
    if (isLeftToRightLayout())
        repaintRect.move(LayoutSize(FloatSize(-tailRect.width() - 2 * maxBlurDiameter, -maxBlurDiameter)));
    else
        repaintRect.move(LayoutSize(FloatSize(-2 * maxBlurDiameter, -maxBlurDiameter)));

    auto heightOffset = std::max(0.f, height - static_cast<float>(repaintRect.height()));
    repaintRect.expand(LayoutSize(FloatSize(tailRect.width() + 4 * maxBlurDiameter, heightOffset + 2 * maxBlurDiameter)));

    return repaintRect;
}

void DictationCaretAnimator::stop(CaretAnimatorStopReason reason)
{
    if (reason == CaretAnimatorStopReason::Default)
        CaretAnimator::stop();
}

} // namespace WebCore

#endif
