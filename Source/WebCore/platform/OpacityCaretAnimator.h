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

class OpacityCaretAnimator final : public CaretAnimator {
public:
    explicit OpacityCaretAnimator(CaretAnimationClient&, std::optional<LayoutRect> = std::nullopt);

private:
    void updateAnimationProperties() final;
    void start() final;
    void paint(GraphicsContext&, const FloatRect&, const Color&, const LayoutPoint&) const final;

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

    Seconds keyframeTimeDelta() const;
    LayoutRect caretRepaintRectForLocalRect(LayoutRect) const final;

    MonotonicTime m_lastTimeCaretOpacityWasToggled;
    size_t m_currentKeyframeIndex { 1 };
    std::optional<LayoutRect> m_overrideRepaintRect;
};

} // namespace WebCore

#endif
