/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "COMPtr.h"
#include "Color.h"
#include <d2d1.h>
#include <d2d1_1.h>
#include <d2d1effects.h>
#include <d2d1helper.h>
#include <windows.h>
#include <wtf/TinyLRUCache.h>

namespace WebCore {

class GraphicsContextPlatformPrivate {
    WTF_MAKE_FAST_ALLOCATED;
public:
    GraphicsContextPlatformPrivate(ID2D1RenderTarget*);
    ~GraphicsContextPlatformPrivate();

    enum Direct2DLayerType { AxisAlignedClip, LayerClip };

    void beginTransparencyLayer(float opacity);
    void endTransparencyLayer();

    void clip(const FloatRect&);
    void clip(const Path&);
    void clip(ID2D1Geometry*);

    void beginDraw();
    void endDraw();
    void flush();
    void save();
    void scale(const FloatSize&);
    void restore();
    void rotate(float);
    void translate(float, float);
    void concatCTM(const AffineTransform&);
    void setCTM(const AffineTransform&);

    void setLineCap(LineCap);
    void setLineJoin(LineJoin);
    void setStrokeStyle(StrokeStyle);
    void setMiterLimit(float);
    void setDashOffset(float);
    void setPatternWidth(float);
    void setPatternOffset(float);
    void setStrokeThickness(float);
    void setDashes(const DashArray&);
    void setAlpha(float);

    ID2D1RenderTarget* renderTarget();
    ID2D1Layer* clipLayer() const { return m_renderStates.last().m_activeLayer.get(); }
    ID2D1StrokeStyle* strokeStyle();

    COMPtr<ID2D1SolidColorBrush> brushWithColor(const D2D1_COLOR_F&);

    HDC m_hdc { nullptr };
    D2D1_BLEND_MODE m_blendMode { D2D1_BLEND_MODE_MULTIPLY };
    D2D1_COMPOSITE_MODE m_compositeMode { D2D1_COMPOSITE_MODE_SOURCE_OVER };
    bool m_shouldIncludeChildWindows { false };
    bool m_strokeSyleIsDirty { false };

    COMPtr<ID2D1SolidColorBrush> m_solidStrokeBrush;
    COMPtr<ID2D1SolidColorBrush> m_solidFillBrush;
    COMPtr<ID2D1BitmapBrush> m_patternStrokeBrush;
    COMPtr<ID2D1BitmapBrush> m_patternFillBrush;

    float currentGlobalAlpha() const;

    D2D1_STROKE_STYLE_PROPERTIES strokeStyleProperties() const;

private:
    void recomputeStrokeStyle();

    COMPtr<ID2D1RenderTarget> m_renderTarget;
    HashMap<uint32_t, COMPtr<ID2D1SolidColorBrush>> m_solidColoredBrushCache;
    COMPtr<ID2D1SolidColorBrush> m_whiteBrush;
    COMPtr<ID2D1SolidColorBrush> m_zeroBrush;
    COMPtr<ID2D1StrokeStyle> m_d2dStrokeStyle;

    struct RenderState {
        COMPtr<ID2D1DrawingStateBlock> m_drawingStateBlock;
        COMPtr<ID2D1Layer> m_activeLayer;
        Vector<Direct2DLayerType> m_clips;
    };

    Vector<RenderState> m_renderStates;

    struct TransparencyLayerState {
        COMPtr<ID2D1BitmapRenderTarget> renderTarget;
        Color shadowColor;
        FloatSize shadowOffset;
        float opacity { 1.0 };
        float shadowBlur { 0 };
        bool hasShadow { false };
    };
    Vector<TransparencyLayerState> m_transparencyLayerStack;

    D2D1_CAP_STYLE m_lineCap { D2D1_CAP_STYLE_FLAT };
    D2D1_LINE_JOIN m_lineJoin { D2D1_LINE_JOIN_MITER };
    StrokeStyle m_strokeStyle { SolidStroke };
    DashArray m_dashes;

    float m_miterLimit { 10.0f };
    float m_dashOffset { 0 };
    float m_patternWidth { 1.0f };
    float m_patternOffset { 0 };
    float m_strokeThickness { 0 };
    float m_alpha { 1.0 };
};

class D2DContextStateSaver {
public:
    D2DContextStateSaver(GraphicsContextPlatformPrivate& context, bool saveAndRestore = true)
        : m_context(context)
        , m_saveAndRestore(saveAndRestore)
    {
        if (m_saveAndRestore)
            m_context.save();
    }

    ~D2DContextStateSaver()
    {
        if (m_saveAndRestore)
            m_context.restore();
    }

    void save()
    {
        ASSERT(!m_saveAndRestore);
        m_context.save();
        m_saveAndRestore = true;
    }

    void restore()
    {
        ASSERT(m_saveAndRestore);
        m_context.restore();
        m_saveAndRestore = false;
    }

    bool didSave() const
    {
        return m_saveAndRestore;
    }

private:
    GraphicsContextPlatformPrivate& m_context;
    bool m_saveAndRestore { false };
};

}
