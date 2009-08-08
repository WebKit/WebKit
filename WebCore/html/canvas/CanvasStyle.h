/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef CanvasStyle_h
#define CanvasStyle_h

#include "PlatformString.h"

namespace WebCore {

    class CanvasGradient;
    class CanvasPattern;
    class GraphicsContext;

    class CanvasStyle : public RefCounted<CanvasStyle> {
    public:
        static PassRefPtr<CanvasStyle> create(const String& color) { return adoptRef(new CanvasStyle(color)); }
        static PassRefPtr<CanvasStyle> create(float grayLevel) { return adoptRef(new CanvasStyle(grayLevel)); }
        static PassRefPtr<CanvasStyle> create(const String& color, float alpha) { return adoptRef(new CanvasStyle(color, alpha)); }
        static PassRefPtr<CanvasStyle> create(float grayLevel, float alpha) { return adoptRef(new CanvasStyle(grayLevel, alpha)); }
        static PassRefPtr<CanvasStyle> create(float r, float g, float b, float a) { return adoptRef(new CanvasStyle(r, g, b, a)); }
        static PassRefPtr<CanvasStyle> create(float c, float m, float y, float k, float a) { return adoptRef(new CanvasStyle(c, m, y, k, a)); }
        static PassRefPtr<CanvasStyle> create(PassRefPtr<CanvasGradient> gradient) { return adoptRef(new CanvasStyle(gradient)); }
        static PassRefPtr<CanvasStyle> create(PassRefPtr<CanvasPattern> pattern) { return adoptRef(new CanvasStyle(pattern)); }

        String color() const { return m_color; }
        CanvasGradient* canvasGradient() const { return m_gradient.get(); }
        CanvasPattern* canvasPattern() const { return m_pattern.get(); }

        void applyFillColor(GraphicsContext*);
        void applyStrokeColor(GraphicsContext*);

    private:
        CanvasStyle(const String& color);
        CanvasStyle(float grayLevel);
        CanvasStyle(const String& color, float alpha);
        CanvasStyle(float grayLevel, float alpha);
        CanvasStyle(float r, float g, float b, float a);
        CanvasStyle(float c, float m, float y, float k, float a);
        CanvasStyle(PassRefPtr<CanvasGradient>);
        CanvasStyle(PassRefPtr<CanvasPattern>);

        enum Type { ColorString, ColorStringWithAlpha, GrayLevel, RGBA, CMYKA, Gradient, ImagePattern };

        Type m_type;

        String m_color;
        RefPtr<CanvasGradient> m_gradient;
        RefPtr<CanvasPattern> m_pattern;

        float m_alpha;

        float m_grayLevel;

        float m_red;
        float m_green;
        float m_blue;

        float m_cyan;
        float m_magenta;
        float m_yellow;
        float m_black;
    };

} // namespace WebCore

#endif
