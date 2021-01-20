/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FloatRoundedRect.h"

namespace WebCore {

class FillLayer;
class GraphicsContext;
enum class ShadowStyle : uint8_t;

namespace Display {

class BoxModelBox;
class FillLayerImageGeometry;
struct PaintingContext;

enum class BackgroundBleedAvoidance : uint8_t {
    None,
    ShrinkBackground,
    UseTransparencyLayer,
    BackgroundOverBorder
};

class BoxDecorationPainter {
public:
    BoxDecorationPainter(const BoxModelBox&, PaintingContext&, bool includeLeftEdge = true, bool includeRightEdge = true);
    void paintBackgroundAndBorders(PaintingContext&) const;

private:
    friend class BorderPainter;

    static BackgroundBleedAvoidance determineBackgroundBleedAvoidance(const BoxModelBox&, PaintingContext&);

    void paintBorders(PaintingContext&) const;
    void paintBoxShadow(PaintingContext&, ShadowStyle) const;
    void paintBackground(PaintingContext&) const;
    void paintBackgroundImages(PaintingContext&) const;

    void paintFillLayer(PaintingContext&, const FillLayer&, const FillLayerImageGeometry&) const;
    
    FloatRoundedRect borderRoundedRect() const { return m_borderRect; }
    FloatRoundedRect innerBorderRoundedRect() const;
    
    FloatRoundedRect backgroundRoundedRectAdjustedForBleedAvoidance(const PaintingContext&) const;

    bool includeLeftEdge() const { return m_includeLeftEdge; }
    bool includeRightEdge() const { return m_includeRightEdge; }

    const BoxModelBox& m_box;
    const FloatRoundedRect m_borderRect;
    const BackgroundBleedAvoidance m_bleedAvoidance { BackgroundBleedAvoidance::None };
    bool m_includeLeftEdge { true };
    bool m_includeRightEdge { true };
};

} // namespace Display
} // namespace WebCore

#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
