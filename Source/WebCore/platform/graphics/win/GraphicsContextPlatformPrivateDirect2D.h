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
#include "PlatformContextDirect2D.h"
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
    GraphicsContextPlatformPrivate(PlatformContextDirect2D&, GraphicsContext::BitmapRenderingContextType);
    GraphicsContextPlatformPrivate(std::unique_ptr<PlatformContextDirect2D>&&, GraphicsContext::BitmapRenderingContextType);
    ~GraphicsContextPlatformPrivate();

    void syncContext(PlatformContextDirect2D&);

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

    PlatformContextDirect2D& platformContext() { return m_platformContext; }
    ID2D1RenderTarget* renderTarget();

    HDC m_hdc { nullptr };

    float currentGlobalAlpha() const;

    D2D1_STROKE_STYLE_PROPERTIES strokeStyleProperties() const;

private:
    std::unique_ptr<PlatformContextDirect2D> m_ownedPlatformContext;
    PlatformContextDirect2D& m_platformContext;
    const GraphicsContext::BitmapRenderingContextType m_rendererType;

    unsigned beginDrawCount { 0 };

    float m_alpha { 1.0 };

    friend class D2DContextStateSaver;
};

class D2DContextStateSaver {
public:
    D2DContextStateSaver(GraphicsContextPlatformPrivate& context, bool saveAndRestore = true)
        : m_context(context)
        , m_saveAndRestore(saveAndRestore)
    {
        if (m_saveAndRestore)
            m_context.m_platformContext.save();
    }

    ~D2DContextStateSaver()
    {
        if (m_saveAndRestore)
            m_context.m_platformContext.restore();
    }

    void save()
    {
        ASSERT(!m_saveAndRestore);
        m_context.m_platformContext.save();
        m_saveAndRestore = true;
    }

    void restore()
    {
        ASSERT(m_saveAndRestore);
        m_context.m_platformContext.restore();
        m_saveAndRestore = false;
    }

    bool didSave() const
    {
        return m_saveAndRestore;
    }

private:
    std::unique_ptr<PlatformContextDirect2D> ownedPlatformContext;
    GraphicsContextPlatformPrivate& m_context;
    bool m_saveAndRestore { false };
};

}
