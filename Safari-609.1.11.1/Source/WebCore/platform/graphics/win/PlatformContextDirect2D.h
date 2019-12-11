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

#pragma once

#if USE(DIRECT2D)

#include "COMPtr.h"
#include "GraphicsContext.h"
#include <d2d1.h>
#include <d2d1_1.h>
#include <d2d1effects.h>
#include <d3d11_1.h>

namespace WebCore {

class GraphicsContextPlatformPrivate;
struct GraphicsContextState;

namespace Direct2D {
struct FillSource;
struct StrokeSource;
struct ShadowState;
}

class PlatformContextDirect2D {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(PlatformContextDirect2D);
public:
    PlatformContextDirect2D(ID2D1RenderTarget*, WTF::Function<void()>&& preDrawHandler = []() { }, WTF::Function<void()>&& postDrawHandler = []() { });
    ~PlatformContextDirect2D();

    ID2D1RenderTarget* renderTarget() { return m_renderTarget.get(); }
    void setRenderTarget(ID2D1RenderTarget*);

    ID2D1DeviceContext* deviceContext() { return m_deviceContext.get(); }

    ID3D11Device1* d3dDevice() const { return m_d3dDevice.get(); }
    void setD3DDevice(ID3D11Device1* device) { m_d3dDevice = device; }

    GraphicsContextPlatformPrivate* graphicsContextPrivate() { return m_graphicsContextPrivate; }
    void setGraphicsContextPrivate(GraphicsContextPlatformPrivate* graphicsContextPrivate) { m_graphicsContextPrivate = graphicsContextPrivate; }

    ID2D1Layer* clipLayer() const;
    ID2D1StrokeStyle* strokeStyle();
    D2D1_STROKE_STYLE_PROPERTIES strokeStyleProperties() const;
    ID2D1StrokeStyle* platformStrokeStyle() const;

    COMPtr<ID2D1SolidColorBrush> brushWithColor(const D2D1_COLOR_F&);

    void save();
    void restore();

    bool hasSavedState() const { return !m_stateStack.isEmpty(); }

    void beginDraw();
    void endDraw();

    enum Direct2DLayerType { AxisAlignedClip, LayerClip };
    void pushRenderClip(Direct2DLayerType);
    void setActiveLayer(COMPtr<ID2D1Layer>&&);

    void recomputeStrokeStyle();
    float strokeThickness() const { return m_strokeThickness; }

    struct TransparencyLayerState {
        COMPtr<ID2D1BitmapRenderTarget> renderTarget;
        Color shadowColor;
        FloatSize shadowOffset;
        float opacity { 1.0 };
        float shadowBlur { 0 };
        bool hasShadow { false };
    };
    Vector<TransparencyLayerState> m_transparencyLayerStack;

    void setLineCap(D2D1_CAP_STYLE);
    void setLineJoin(D2D1_LINE_JOIN);
    void setStrokeStyle(StrokeStyle);
    void setMiterLimit(float);
    void setDashOffset(float);
    void setPatternWidth(float);
    void setPatternOffset(float);
    void setStrokeThickness(float);
    void setDashes(const DashArray&);

    void setBlendAndCompositeMode(D2D1_BLEND_MODE, D2D1_COMPOSITE_MODE);

    void setTags(D2D1_TAG tag1, D2D1_TAG tag2);
    void notifyPreDrawObserver();
    void notifyPostDrawObserver();

    void pushClip(Direct2DLayerType);

private:
    void clearClips(Vector<Direct2DLayerType>&);
    void compositeIfNeeded();

    GraphicsContextPlatformPrivate* m_graphicsContextPrivate { nullptr };

    COMPtr<ID2D1RenderTarget> m_renderTarget;
    COMPtr<ID2D1DeviceContext> m_deviceContext;
    COMPtr<ID2D1StrokeStyle> m_d2dStrokeStyle;

    HashMap<uint32_t, COMPtr<ID2D1SolidColorBrush>> m_solidColoredBrushCache;
    COMPtr<ID2D1SolidColorBrush> m_whiteBrush;
    COMPtr<ID2D1SolidColorBrush> m_zeroBrush;
    COMPtr<ID2D1SolidColorBrush> m_solidStrokeBrush;
    COMPtr<ID2D1SolidColorBrush> m_solidFillBrush;
    COMPtr<ID2D1BitmapBrush> m_patternStrokeBrush;
    COMPtr<ID2D1BitmapBrush> m_patternFillBrush;

    COMPtr<ID3D11Device1> m_d3dDevice;

    COMPtr<ID2D1Bitmap> m_compositeSource;

    WTF::Function<void()> m_preDrawHandler;
    WTF::Function<void()> m_postDrawHandler;

    class State;
    State* m_state { nullptr };
    Vector<State> m_stateStack;

    D2D1_CAP_STYLE m_lineCap { D2D1_CAP_STYLE_FLAT };
    D2D1_LINE_JOIN m_lineJoin { D2D1_LINE_JOIN_MITER };
    StrokeStyle m_strokeStyle { SolidStroke };
    DashArray m_dashes;

    unsigned beginDrawCount { 0 };

    float m_miterLimit { 10.0f };
    float m_dashOffset { 0 };
    float m_patternWidth { 1.0f };
    float m_patternOffset { 0 };
    float m_strokeThickness { 0 };
    float m_alpha { 1.0 };

    bool m_strokeSyleIsDirty { false };

    D2D1_BLEND_MODE m_blendMode { D2D1_BLEND_MODE_MULTIPLY };
    D2D1_COMPOSITE_MODE m_compositeMode { D2D1_COMPOSITE_MODE_SOURCE_OVER };

    friend class GraphicsContext;
    friend class GraphicsContextPlatformPrivate;
};

class PlatformContextStateSaver {
public:
    PlatformContextStateSaver(PlatformContextDirect2D& context, bool saveAndRestore = true)
        : m_context(context)
        , m_saveAndRestore(saveAndRestore)
    {
        if (m_saveAndRestore)
            m_context.save();
    }

    ~PlatformContextStateSaver()
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
    PlatformContextDirect2D& m_context;
    bool m_saveAndRestore { false };
};

} // namespace WebCore

#endif // USE(DIRECT2D)
