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
#include "Direct2DUtilities.h"
#include <d2d1.h>

namespace WebCore {

// Encapsulates the additional painting state information we store for each
// pushed graphics state.
class PlatformContextDirect2D::State {
public:
    State() = default;

    COMPtr<ID2D1DrawingStateBlock> m_drawingStateBlock;
    COMPtr<ID2D1Layer> m_activeLayer;
    Vector<Direct2DLayerType> m_clips;
};

PlatformContextDirect2D::PlatformContextDirect2D(ID2D1RenderTarget* renderTarget, WTF::Function<void()>&& preDrawHandler, WTF::Function<void()>&& postDrawHandler)
    : m_renderTarget(renderTarget)
    , m_preDrawHandler(WTFMove(preDrawHandler))
    , m_postDrawHandler(WTFMove(postDrawHandler))
{
    m_stateStack.append(State());
    m_state = &m_stateStack.last();

    m_deviceContext.query(m_renderTarget.get());
    RELEASE_ASSERT(!!m_deviceContext);
}

void PlatformContextDirect2D::setRenderTarget(ID2D1RenderTarget* renderTarget)
{
    m_renderTarget = renderTarget;

    m_deviceContext.query(m_renderTarget.get());
    RELEASE_ASSERT(!!m_deviceContext);
}

ID2D1Layer* PlatformContextDirect2D::clipLayer() const
{
    return m_state->m_activeLayer.get();
}

void PlatformContextDirect2D::clearClips(Vector<Direct2DLayerType>& clips)
{
    for (auto clipType = clips.rbegin(); clipType != clips.rend(); ++clipType) {
        if (*clipType == AxisAlignedClip)
            m_renderTarget->PopAxisAlignedClip();
        else
            m_renderTarget->PopLayer();
    }

    clips.clear();
}

void PlatformContextDirect2D::restore()
{
    ASSERT(m_renderTarget);

    // No need to restore if we don't have a saved element on the stack.
    if (m_stateStack.size() == 1)
        return;

    auto& restoreState = m_stateStack.last();
    if (restoreState.m_drawingStateBlock) {
        m_renderTarget->RestoreDrawingState(restoreState.m_drawingStateBlock.get());
        restoreState.m_drawingStateBlock = nullptr;
    }

    clearClips(restoreState.m_clips);

    m_stateStack.removeLast();
    ASSERT(!m_stateStack.isEmpty());
    m_state = &m_stateStack.last();
}

PlatformContextDirect2D::~PlatformContextDirect2D() = default;

void PlatformContextDirect2D::save()
{
    ASSERT(m_stateStack.size() >= 1); // There should always be one state on the stack.
    ASSERT(m_renderTarget);

    m_stateStack.append(State());
    m_state = &m_stateStack.last();

    GraphicsContext::systemFactory()->CreateDrawingStateBlock(&m_state->m_drawingStateBlock);

    m_renderTarget->SaveDrawingState(m_state->m_drawingStateBlock.get());
}

void PlatformContextDirect2D::pushRenderClip(Direct2DLayerType clipType)
{
    ASSERT(hasSavedState());
    m_state->m_clips.append(clipType);
}

void PlatformContextDirect2D::setActiveLayer(COMPtr<ID2D1Layer>&& layer)
{
    ASSERT(hasSavedState());
    m_state->m_activeLayer = layer;
}

COMPtr<ID2D1SolidColorBrush> PlatformContextDirect2D::brushWithColor(const D2D1_COLOR_F& color)
{
    auto colorKey = convertToComponentBytes(SRGBA { color.r, color.g, color.b, color.a });

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

    if (m_stateStack.size() > 1)
        restore();

    clearClips(m_state->m_clips);

    ASSERT(m_stateStack.size() >= 1);

    D2D1_TAG first, second;
    HRESULT hr = m_renderTarget->EndDraw(&first, &second);

    if (!SUCCEEDED(hr))
        WTFLogAlways("Failed in PlatformContextDirect2D::endDraw: hr=%xd, first=%ld, second=%ld", hr, first, second);

    --beginDrawCount;

    compositeIfNeeded();
}

void PlatformContextDirect2D::setTags(D2D1_TAG tag1, D2D1_TAG tag2)
{
#if ASSERT_ENABLED
    m_renderTarget->SetTags(tag1, tag2);
#else
    UNUSED_PARAM(tag1);
    UNUSED_PARAM(tag2);
#endif
}

void PlatformContextDirect2D::notifyPreDrawObserver()
{
    m_preDrawHandler();
}

void PlatformContextDirect2D::notifyPostDrawObserver()
{
    m_postDrawHandler();
}

void PlatformContextDirect2D::pushClip(Direct2DLayerType clipType)
{
    m_state->m_clips.append(clipType);
}

void PlatformContextDirect2D::compositeIfNeeded()
{
    if (!m_compositeSource)
        return;

    COMPtr<ID2D1BitmapRenderTarget> bitmapTarget(Query, m_renderTarget.get());
    if (!bitmapTarget)
        return;

    COMPtr<ID2D1Bitmap> currentCanvas = Direct2D::createBitmapCopyFromContext(bitmapTarget.get());
    if (!currentCanvas)
        return;

    COMPtr<ID2D1Effect> effect;

    if (m_compositeMode != D2D1_COMPOSITE_MODE_FORCE_DWORD) {
        m_deviceContext->CreateEffect(CLSID_D2D1Composite, &effect);
        effect->SetInput(0, m_compositeSource.get());
        effect->SetInput(1, currentCanvas.get());
        effect->SetValue(D2D1_COMPOSITE_PROP_MODE, m_compositeMode);
    } else if (m_blendMode != D2D1_BLEND_MODE_FORCE_DWORD) {
        m_deviceContext->CreateEffect(CLSID_D2D1Blend, &effect);
        effect->SetInput(0, currentCanvas.get());
        effect->SetInput(1, m_compositeSource.get());
        effect->SetValue(D2D1_BLEND_PROP_MODE, m_blendMode);
    }

    if (effect) {
        m_deviceContext->BeginDraw();
        m_deviceContext->Clear(D2D1::ColorF(0, 0, 0, 0));
        m_deviceContext->DrawImage(effect.get());
        m_deviceContext->EndDraw();
    }

    m_compositeSource = nullptr;
}

void PlatformContextDirect2D::setBlendAndCompositeMode(D2D1_BLEND_MODE blend, D2D1_COMPOSITE_MODE mode)
{
    if (mode == m_compositeMode && blend == m_blendMode)
        return;

    // Direct2D handles compositing based on bitmaps. If we are changing the
    // compositing mode, we need to capture the current state of the rendering
    // in a bitmap, perform drawing operations until we finish, then handle
    // the composting of the two layers.
    COMPtr<ID2D1BitmapRenderTarget> bitmapTarget(Query, m_renderTarget.get());
    if (!bitmapTarget)
        return;

    endDraw();

    m_compositeMode = mode;
    m_blendMode = blend;

    m_compositeSource = Direct2D::createBitmapCopyFromContext(bitmapTarget.get());

    beginDraw();

    m_deviceContext->Clear(D2D1::ColorF(0, 0, 0, 0));
}

} // namespace WebCore

#endif // USE(DIRECT2D)
