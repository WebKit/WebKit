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

#include "FloatRect.h"
#include "LayoutRect.h"
#include "RenderStyleConstants.h"

namespace WebCore {
class FillLayer;
class RenderStyle;
class StyleImage;

namespace Layout {
class Box;
class BoxGeometry;
}

namespace Display {
class Box;

// Pixel-snapped image geometry for a single fill layer (used for backgrounds and masks).
class FillLayerImageGeometry {
public:
    FillLayerImageGeometry(const FloatRect& destinationRect, const FloatSize& tileSize, const FloatSize& phase, const FloatSize& space, FillAttachment attachment)
        : m_destRect(destinationRect)
        , m_destOrigin(m_destRect.location())
        , m_tileSize(tileSize)
        , m_phase(phase)
        , m_space(space)
        , m_hasNonLocalGeometry(attachment == FillAttachment::FixedBackground)
    {
    }

    FloatRect destRect() const { return m_destRect; }
    FloatSize phase() const { return m_phase; }
    FloatSize tileSize() const { return m_tileSize; }
    FloatSize spaceSize() const { return m_space; }
    bool hasNonLocalGeometry() const { return m_hasNonLocalGeometry; }

    FloatSize relativePhase() const
    {
        FloatSize phase = m_phase;
        phase += m_destRect.location() - m_destOrigin;
        return phase;
    }

    void clip(const FloatRect& clipRect) { m_destRect.intersect(clipRect); }

private:
    FloatRect m_destRect;
    FloatPoint m_destOrigin;
    FloatSize m_tileSize;
    FloatSize m_phase;
    FloatSize m_space;
    bool m_hasNonLocalGeometry { false }; // Has background-attachment: fixed. Implies that we can't always cheaply compute destRect.
};

Vector<FillLayerImageGeometry, 1> calculateFillLayerImageGeometry(const RenderStyle&, const Layout::BoxGeometry&, LayoutSize offsetFromRoot, float pixelSnappingFactor);

} // namespace Display
} // namespace WebCore

