/*
 * Copyright (C) 2015-2025 Apple Inc. All rights reserved.
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

#include <WebCore/ClipRect.h>

namespace WebCore {

class LayerFragment {
public:
    class Rects {
    public:
        Rects() = default;

        Rects(const LayoutRect& layerBounds, const LayoutRect& paintDirtyRect, const ClipRect& backgroundRect, const ClipRect& foregroundRect, const std::optional<LayoutRect>& boundingBox = std::nullopt)
            : m_layerBounds(layerBounds)
            , m_paintDirtyRect(paintDirtyRect)
            , m_backgroundRect(backgroundRect)
            , m_foregroundRect(foregroundRect)
            , m_boundingBox(boundingBox)
        {
        }

        Rects(const Rects& other, const LayoutRect& boundingBox)
            : Rects(other.m_layerBounds, other.m_paintDirtyRect, other. m_backgroundRect, other.m_foregroundRect, boundingBox)
        {
        }

        LayoutRect layerBounds() const { return m_layerBounds; }

        ClipRect backgroundRect() const { return m_backgroundRect; }
        ClipRect dirtyBackgroundRect() const { return intersection(m_paintDirtyRect, m_backgroundRect); }
        ClipRect dirtyForegroundRect() const { return intersection(m_paintDirtyRect, m_foregroundRect); }

        std::optional<LayoutRect> boundingBox() const { return m_boundingBox; }

        void moveBy(const LayoutPoint& offset)
        {
            m_layerBounds.moveBy(offset);
            m_paintDirtyRect.moveBy(offset);
            m_backgroundRect.moveBy(offset);
            m_foregroundRect.moveBy(offset);
            if (m_boundingBox)
                m_boundingBox->moveBy(offset);
        }

        void intersect(const LayoutRect& rect)
        {
            m_backgroundRect.intersect(rect);
            m_foregroundRect.intersect(rect);
            if (m_boundingBox)
                m_boundingBox->intersect(rect);
        }

        void intersect(const ClipRect& clipRect)
        {
            m_backgroundRect.intersect(clipRect);
            m_foregroundRect.intersect(clipRect);
        }

        LayoutRect m_layerBounds;
        LayoutRect m_paintDirtyRect;
        ClipRect m_backgroundRect;
        ClipRect m_foregroundRect;
        std::optional<LayoutRect> m_boundingBox;
    };

    LayerFragment() = default;

    LayerFragment(Rects&& rects) { this->rects = WTFMove(rects); }

    LayoutRect layerBounds() const { return rects.layerBounds(); }

    ClipRect backgroundRect() const { return rects.backgroundRect(); }
    ClipRect dirtyBackgroundRect() const { return rects.dirtyBackgroundRect(); }
    ClipRect dirtyForegroundRect() const { return rects.dirtyForegroundRect(); }

    std::optional<LayoutRect> boundingBox() const { return rects.boundingBox(); }

    void moveBy(const LayoutPoint& offset)
    {
        rects.moveBy(offset);
        paginationClip.moveBy(offset);
    }

    void intersect(const LayoutRect& rect) { rects.intersect(rect); }
    void intersect(const ClipRect& clipRect) { rects.intersect(clipRect); }

    bool shouldPaintContent = false;

    Rects rects;

    // Unique to paginated fragments. The physical translation to apply to shift the layer when painting/hit-testing.
    LayoutSize paginationOffset;

    // Also unique to paginated fragments. An additional clip that applies to the layer. It is in layer-local
    // (physical) coordinates.
    LayoutRect paginationClip;
};

typedef Vector<LayerFragment, 1> LayerFragments;

} // namespace WebCore
