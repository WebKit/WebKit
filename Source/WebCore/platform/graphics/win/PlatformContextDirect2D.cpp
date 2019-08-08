/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "PlatformContextDirect2D.h"

#if USE(DIRECT2D)

#include "Direct2DOperations.h"
#include <d2d1.h>

namespace WebCore {

// Encapsulates the additional painting state information we store for each
// pushed graphics state.
class PlatformContextDirect2D::State {
public:
    State() = default;
};

PlatformContextDirect2D::PlatformContextDirect2D(ID2D1RenderTarget* renderTarget)
    : m_renderTarget(renderTarget)
{
    m_stateStack.append(State());
    m_state = &m_stateStack.last();
}

void PlatformContextDirect2D::restore()
{
    ASSERT(m_renderTarget);

    auto restoreState = m_renderStates.takeLast();
    m_renderTarget->RestoreDrawingState(restoreState.m_drawingStateBlock.get());

    for (auto clipType = restoreState.m_clips.rbegin(); clipType != restoreState.m_clips.rend(); ++clipType) {
        if (*clipType == AxisAlignedClip)
            m_renderTarget->PopAxisAlignedClip();
        else
            m_renderTarget->PopLayer();
    }

    m_stateStack.removeLast();
    ASSERT(!m_stateStack.isEmpty());
    m_state = &m_stateStack.last();
}

PlatformContextDirect2D::~PlatformContextDirect2D() = default;

void PlatformContextDirect2D::save()
{
    ASSERT(m_renderTarget);

    m_stateStack.append(State());
    m_state = &m_stateStack.last();

    RenderState currentState;
    GraphicsContext::systemFactory()->CreateDrawingStateBlock(&currentState.m_drawingStateBlock);

    m_renderTarget->SaveDrawingState(currentState.m_drawingStateBlock.get());

    m_renderStates.append(currentState);
}

void PlatformContextDirect2D::pushRenderClip(Direct2DLayerType clipType)
{
    ASSERT(hasSavedState());
    m_renderStates.last().m_clips.append(clipType);
}

void PlatformContextDirect2D::setActiveLayer(COMPtr<ID2D1Layer>&& layer)
{
    ASSERT(hasSavedState());
    m_renderStates.last().m_activeLayer = layer;
}

COMPtr<ID2D1SolidColorBrush> PlatformContextDirect2D::brushWithColor(const D2D1_COLOR_F& color)
{
    RGBA32 colorKey = makeRGBA32FromFloats(color.r, color.g, color.b, color.a);

    if (!colorKey) {
        if (!m_zeroBrush)
            m_renderTarget->CreateSolidColorBrush(color, &m_zeroBrush);
        return m_zeroBrush;
    }

    if (colorKey == 0xFFFFFFFF) {
        if (!m_whiteBrush)
            m_renderTarget->CreateSolidColorBrush(color, &m_whiteBrush);
        return m_whiteBrush;
    }

    auto existingBrush = m_solidColoredBrushCache.ensure(colorKey, [this, color] {
        COMPtr<ID2D1SolidColorBrush> colorBrush;
        m_renderTarget->CreateSolidColorBrush(color, &colorBrush);
        return colorBrush;
    });

    return existingBrush.iterator->value;
}

void PlatformContextDirect2D::recomputeStrokeStyle()
{
    if (!m_strokeSyleIsDirty)
        return;

    m_d2dStrokeStyle = nullptr;

    DashArray dashes;
    float patternOffset = 0;
    auto dashStyle = D2D1_DASH_STYLE_SOLID;

    if ((m_strokeStyle != SolidStroke) && (m_strokeStyle != NoStroke)) {
        dashStyle = D2D1_DASH_STYLE_CUSTOM;
        patternOffset = m_patternOffset / m_strokeThickness;
        dashes = m_dashes;

        // In Direct2D, dashes and dots are defined in terms of the ratio of the dash length to the line thickness.
        for (auto& dash : dashes)
            dash /= m_strokeThickness;
    }

    auto strokeStyleProperties = D2D1::StrokeStyleProperties(m_lineCap, m_lineCap, m_lineCap, m_lineJoin, m_miterLimit, dashStyle, patternOffset);
    GraphicsContext::systemFactory()->CreateStrokeStyle(&strokeStyleProperties, dashes.data(), dashes.size(), &m_d2dStrokeStyle);

    m_strokeSyleIsDirty = false;
}

ID2D1StrokeStyle* PlatformContextDirect2D::strokeStyle()
{
    recomputeStrokeStyle();
    return m_d2dStrokeStyle.get();
}

D2D1_STROKE_STYLE_PROPERTIES PlatformContextDirect2D::strokeStyleProperties() const
{
    return D2D1::StrokeStyleProperties(m_lineCap, m_lineCap, m_lineCap, m_lineJoin, m_miterLimit, D2D1_DASH_STYLE_SOLID, 0.0f);
}

void PlatformContextDirect2D::setLineCap(D2D1_CAP_STYLE lineCap)
{
    if (m_lineCap == lineCap)
        return;

    m_lineCap = lineCap;
    m_strokeSyleIsDirty = true;
}

void PlatformContextDirect2D::setLineJoin(D2D1_LINE_JOIN join)
{
    if (m_lineJoin == join)
        return;

    m_lineJoin = join;
    m_strokeSyleIsDirty = true;
}

void PlatformContextDirect2D::setStrokeStyle(StrokeStyle strokeStyle)
{
    if (m_strokeStyle == strokeStyle)
        return;

    m_strokeStyle = strokeStyle;
    m_strokeSyleIsDirty = true;
}

void PlatformContextDirect2D::setStrokeThickness(float thickness)
{
    if (WTF::areEssentiallyEqual(thickness, m_strokeThickness))
        return;

    m_strokeThickness = thickness;
    m_strokeSyleIsDirty = true;
}

void PlatformContextDirect2D::setMiterLimit(float canvasMiterLimit)
{
    // Direct2D miter limit is in terms of HALF the line thickness.
    float miterLimit = 0.5f * canvasMiterLimit;
    if (WTF::areEssentiallyEqual(miterLimit, m_miterLimit))
        return;

    m_miterLimit = miterLimit;
    m_strokeSyleIsDirty = true;
}

void PlatformContextDirect2D::setDashOffset(float dashOffset)
{
    if (WTF::areEssentiallyEqual(dashOffset, m_dashOffset))
        return;

    m_dashOffset = dashOffset;
    m_strokeSyleIsDirty = true;
}

void PlatformContextDirect2D::setDashes(const DashArray& dashes)
{
    if (m_dashes == dashes)
        return;

    m_dashes = dashes;
    m_strokeSyleIsDirty = true;
}

void PlatformContextDirect2D::setPatternWidth(float patternWidth)
{
    if (WTF::areEssentiallyEqual(patternWidth, m_patternWidth))
        return;

    m_patternWidth = patternWidth;
    m_strokeSyleIsDirty = true;
}

void PlatformContextDirect2D::setPatternOffset(float patternOffset)
{
    if (WTF::areEssentiallyEqual(patternOffset, m_patternOffset))
        return;

    m_patternOffset = patternOffset;
    m_strokeSyleIsDirty = true;
}

void PlatformContextDirect2D::beginDraw()
{
    ASSERT(m_renderTarget);
    m_renderTarget->BeginDraw();
    ++beginDrawCount;
}

void PlatformContextDirect2D::endDraw()
{
    ASSERT(m_renderTarget);
    D2D1_TAG first, second;
    HRESULT hr = m_renderTarget->EndDraw(&first, &second);

    if (!SUCCEEDED(hr))
        WTFLogAlways("Failed in PlatformContextDirect2D::endDraw: hr=%xd, first=%ld, second=%ld", hr, first, second);

    --beginDrawCount;
}

} // namespace WebCore

#endif // USE(DIRECT2D)
