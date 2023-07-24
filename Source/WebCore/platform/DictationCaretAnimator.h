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

#pragma once

#include "CaretAnimator.h"

#if HAVE(REDESIGNED_TEXT_CURSOR)

namespace WebCore {

class Path;

class DictationCaretAnimator final : public CaretAnimator {
public:
    explicit DictationCaretAnimator(CaretAnimationClient&);

private:
    void updateAnimationProperties() final;
    void start() final;
    FloatRect tailRect() const;

    String debugDescription() const final;

    void setVisible(bool visible) final { setOpacity(visible ? 1.0 : 0.0); }

    void setOpacity(float opacity)
    {
        if (m_presentationProperties.opacity == opacity)
            return;

        m_presentationProperties.opacity = opacity;
        m_client.caretAnimationDidUpdate(*this);
    }

    void setBlinkingSuspended(bool) final;

    void stop(CaretAnimatorStopReason) final;

    Seconds keyframeTimeDelta() const;
    size_t keyframeCount() const;
    void updateGlowTail(float caretPosition, Seconds elapsedTime);
    void resetGlowTail(FloatRect);
    void updateGlowTail(Seconds elapsedTime);
    void paint(GraphicsContext&, const FloatRect&, const Color&, const LayoutPoint&) const final;
    LayoutRect caretRepaintRectForLocalRect(LayoutRect repaintRect) const final;
    Path makeDictationTailConePath(const FloatRect&) const;
    void fillCaretTail(const FloatRect&, GraphicsContext&, const Color&) const;

    FloatRect computeTailRect() const;
    bool isLeftToRightLayout() const;
    FloatRoundedRect expandedCaretRect(const FloatRect&, bool fillTail) const;
    int computeScrollLeft() const;

    MonotonicTime m_lastUpdateTime;
    size_t m_currentKeyframeIndex { 1 };
    FloatRect m_localCaretRect;
    FloatRect m_tailRect, m_previousTailRect;
    float m_animationSpeed { 0 };
    float m_glowStart { 0 };
    float m_initialScale { 0 };
    float m_scrollLeft { 0 };
};

} // namespace WebCore

#endif
