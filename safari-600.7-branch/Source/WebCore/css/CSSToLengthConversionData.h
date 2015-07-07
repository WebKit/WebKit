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

#ifndef CSSToLengthConversionData_h
#define CSSToLengthConversionData_h

#include <wtf/Assertions.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class RenderStyle;
class RenderView;

class CSSToLengthConversionData {
public:
    CSSToLengthConversionData(RenderStyle* style, const RenderStyle* rootStyle, const RenderView* renderView, float zoom, bool computingFontSize = false)
        : m_style(style)
        , m_rootStyle(rootStyle)
        , m_renderView(renderView)
        , m_zoom(zoom)
        , m_useEffectiveZoom(false)
        , m_computingFontSize(computingFontSize)
    {
        ASSERT(zoom > 0);
    }

    CSSToLengthConversionData(RenderStyle* style, const RenderStyle* rootStyle, const RenderView* renderView, bool computingFontSize = false)
        : m_style(style)
        , m_rootStyle(rootStyle)
        , m_renderView(renderView)
        , m_zoom(1)
        , m_useEffectiveZoom(true)
        , m_computingFontSize(computingFontSize)
    {
    }

    CSSToLengthConversionData()
        : CSSToLengthConversionData(nullptr, nullptr, nullptr)
    {
    }

    RenderStyle* style() const { return m_style; }
    const RenderStyle* rootStyle() const { return m_rootStyle; }
    float zoom() const;
    bool computingFontSize() const { return m_computingFontSize; }

    double viewportWidthFactor() const;
    double viewportHeightFactor() const;
    double viewportMinFactor() const;
    double viewportMaxFactor() const;

    CSSToLengthConversionData copyWithAdjustedZoom(float newZoom) const
    {
        return CSSToLengthConversionData(m_style, m_rootStyle, m_renderView, newZoom, m_computingFontSize);
    }

private:
    RenderStyle* m_style;
    const RenderStyle* m_rootStyle;
    const RenderView* m_renderView;
    float m_zoom;
    bool m_useEffectiveZoom;
    bool m_computingFontSize;
};

} // namespace WebCore

#endif
