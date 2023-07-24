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
#include "OpacityCaretAnimator.h"

#if HAVE(REDESIGNED_TEXT_CURSOR)

#include "FloatRoundedRect.h"
#include "VisibleSelection.h"

namespace WebCore {

static constexpr std::array keyframes = {
    KeyFrame { 0.0_s   , 1.00 },
    KeyFrame { 0.5_s   , 1.00 },
    KeyFrame { 0.5375_s, 0.75 },
    KeyFrame { 0.575_s , 0.50 },
    KeyFrame { 0.6125_s, 0.25 },
    KeyFrame { 0.65_s  , 0.00 },
    KeyFrame { 0.85_s  , 0.00 },
    KeyFrame { 0.8875_s, 0.25 },
    KeyFrame { 0.925_s , 0.50 },
    KeyFrame { 0.9625_s, 0.75 },
    KeyFrame { 1.0_s   , 1.00 },
};

OpacityCaretAnimator::OpacityCaretAnimator(CaretAnimationClient& client, std::optional<LayoutRect> repaintExpansionRect)
    : CaretAnimator(client)
    , m_overrideRepaintRect(repaintExpansionRect)
{
}

Seconds OpacityCaretAnimator::keyframeTimeDelta() const
{
    ASSERT(m_currentKeyframeIndex > 0 && m_currentKeyframeIndex < keyframes.size());
    return keyframes[m_currentKeyframeIndex].time - keyframes[m_currentKeyframeIndex - 1].time;
}

void OpacityCaretAnimator::setBlinkingSuspended(bool suspended)
{
    if (suspended == isBlinkingSuspended())
        return;

    if (!suspended) {
        m_currentKeyframeIndex = 1;
        m_blinkTimer.startOneShot(keyframeTimeDelta());
    }

    CaretAnimator::setBlinkingSuspended(suspended);
}

void OpacityCaretAnimator::updateAnimationProperties()
{
    if (isBlinkingSuspended() && m_presentationProperties.opacity >= 1.0)
        return;

    auto currentTime = MonotonicTime::now();
    if (currentTime - m_lastTimeCaretOpacityWasToggled >= keyframeTimeDelta()) {
        setOpacity(keyframes[m_currentKeyframeIndex].value);
        m_lastTimeCaretOpacityWasToggled = currentTime;

        if (m_currentKeyframeIndex == keyframes.size() - 1)
            m_currentKeyframeIndex = 0;

        m_currentKeyframeIndex++;

        m_blinkTimer.startOneShot(keyframeTimeDelta());

        m_overrideRepaintRect = std::nullopt;
    }
}

void OpacityCaretAnimator::start()
{
    // The default/start value of `m_currentKeyframeIndex` should be `1` since the keyframe
    // delta is the difference between `m_currentKeyframeIndex` and `m_currentKeyframeIndex - 1`
    m_currentKeyframeIndex = 1;
    m_lastTimeCaretOpacityWasToggled = MonotonicTime::now();
    didStart(m_lastTimeCaretOpacityWasToggled, keyframeTimeDelta());
}

String OpacityCaretAnimator::debugDescription() const
{
    TextStream textStream;
    textStream << "OpacityCaretAnimator " << this << " active " << isActive() << " opacity = " << m_presentationProperties.opacity;
    return textStream.release();
}

void OpacityCaretAnimator::paint(GraphicsContext& context, const FloatRect& rect, const Color& caretColor, const LayoutPoint&) const
{
    auto caretPresentationProperties = presentationProperties();

    auto caretColorWithOpacity = caretColor;
    if (caretColor != Color::transparentBlack)
        caretColorWithOpacity = caretColor.colorWithAlpha(caretPresentationProperties.opacity);

    context.fillRoundedRect(FloatRoundedRect { rect, FloatRoundedRect::Radii { 1.0 } }, caretColorWithOpacity);
}

LayoutRect OpacityCaretAnimator::caretRepaintRectForLocalRect(LayoutRect repaintRect) const
{
    if (!m_overrideRepaintRect)
        return CaretAnimator::caretRepaintRectForLocalRect(repaintRect);

    auto rect = *m_overrideRepaintRect;
    repaintRect.moveBy(rect.location());
    repaintRect.expand(rect.size());

    return repaintRect;
}

} // namespace WebCore

#endif
