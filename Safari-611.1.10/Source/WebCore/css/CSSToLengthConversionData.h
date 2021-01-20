/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSPropertyNames.h"
#include <wtf/Assertions.h>
#include <wtf/Optional.h>

namespace WebCore {

class RenderStyle;
class RenderView;

class CSSToLengthConversionData {
public:
    CSSToLengthConversionData(const RenderStyle* style, const RenderStyle* rootStyle, const RenderStyle* parentStyle, const RenderView* renderView, float zoom, Optional<CSSPropertyID> propertyToCompute = WTF::nullopt)
        : m_style(style)
        , m_rootStyle(rootStyle)
        , m_parentStyle(parentStyle)
        , m_renderView(renderView)
        , m_zoom(zoom)
        , m_useEffectiveZoom(false)
        , m_propertyToCompute(propertyToCompute)
    {
        ASSERT(zoom > 0);
    }

    CSSToLengthConversionData(const RenderStyle* style, const RenderStyle* rootStyle, const RenderStyle* parentStyle, const RenderView* renderView, Optional<CSSPropertyID> propertyToCompute = WTF::nullopt)
        : m_style(style)
        , m_rootStyle(rootStyle)
        , m_parentStyle(parentStyle)
        , m_renderView(renderView)
        , m_zoom(1)
        , m_useEffectiveZoom(true)
        , m_propertyToCompute(propertyToCompute)
    {
    }

    CSSToLengthConversionData()
        : CSSToLengthConversionData(nullptr, nullptr, nullptr, nullptr)
    {
    }

    const RenderStyle* style() const { return m_style; }
    const RenderStyle* rootStyle() const { return m_rootStyle; }
    const RenderStyle* parentStyle() const { return m_parentStyle; }
    float zoom() const;
    bool computingFontSize() const { return m_propertyToCompute == CSSPropertyFontSize; }
    bool computingLineHeight() const { return m_propertyToCompute == CSSPropertyLineHeight; }

    double viewportWidthFactor() const;
    double viewportHeightFactor() const;
    double viewportMinFactor() const;
    double viewportMaxFactor() const;

    CSSToLengthConversionData copyWithAdjustedZoom(float newZoom) const
    {
        return CSSToLengthConversionData(m_style, m_rootStyle, m_parentStyle, m_renderView, newZoom, m_propertyToCompute);
    }

    CSSToLengthConversionData copyWithAdjustedZoomAndPropertyToCompute(float zoom, Optional<CSSPropertyID> propertyToCompute) const
    {
        return CSSToLengthConversionData(m_style, m_rootStyle, m_parentStyle, m_renderView, zoom, propertyToCompute);
    }

private:
    const RenderStyle* m_style;
    const RenderStyle* m_rootStyle;
    const RenderStyle* m_parentStyle;
    const RenderView* m_renderView;
    float m_zoom;
    bool m_useEffectiveZoom;
    Optional<CSSPropertyID> m_propertyToCompute;
};

} // namespace WebCore
