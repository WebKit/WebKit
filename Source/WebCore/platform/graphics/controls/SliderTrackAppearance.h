/*
 * Copyright (C) 2023 Apple Inc. All Rights Reserved.
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

#include "ControlStyle.h"
#include "IntRect.h"
#include "StyleAppearance.h"

namespace WebCore {

class ControlFactory;
class ControlPart;
class GraphicsContext;
class PlatformControl;

enum class SliderTrackStyleType : uint8_t {
    Horizontal,
    Vertical
};

class SliderTrackAppearance {
public:
    SliderTrackAppearance() = default;
    WEBCORE_EXPORT SliderTrackAppearance(const IntSize& thumbSize, const IntRect& trackBounds, Vector<double>&& tickRatios);

    std::unique_ptr<PlatformControl> createPlatformControl(ControlPart&, ControlFactory&);

    IntSize thumbSize() const { return m_thumbSize; }
    void setThumbSize(IntSize thumbSize) { m_thumbSize = thumbSize; }

    IntRect trackBounds() const { return m_trackBounds; }
    void setTrackBounds(IntRect trackBounds) { m_trackBounds = trackBounds; }

    const Vector<double>& tickRatios() const { return m_tickRatios; }
    void setTickRatios(Vector<double>&& tickRatios) { m_tickRatios = WTFMove(tickRatios); }

protected:
    void drawTicks(GraphicsContext&, const FloatRect&, const ControlStyle&, StyleAppearance) const;

    IntSize m_thumbSize;
    IntRect m_trackBounds;
    Vector<double> m_tickRatios;
};

class SliderTrackHorizontalAppearance final : public SliderTrackAppearance {
public:
    using SliderTrackAppearance::SliderTrackAppearance;
    static constexpr StyleAppearance appearance = StyleAppearance::SliderHorizontal;

#if ENABLE(DATALIST_ELEMENT)
    void drawTicks(GraphicsContext&, const FloatRect&, const ControlStyle&) const;
#endif
};

class SliderTrackVerticalAppearance final : public SliderTrackAppearance {
public:
    using SliderTrackAppearance::SliderTrackAppearance;
    static constexpr StyleAppearance appearance = StyleAppearance::SliderVertical;

#if ENABLE(DATALIST_ELEMENT)
    void drawTicks(GraphicsContext&, const FloatRect&, const ControlStyle&) const;
#endif
};

} // namespace WebCore
