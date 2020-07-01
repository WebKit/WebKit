/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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

#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "Color.h"
#include "ColorSerialization.h"
#include <wtf/Variant.h>

namespace WebCore {

class Document;
class GraphicsContext;
class CanvasBase;

class CanvasStyle {
public:
    CanvasStyle();
    CanvasStyle(Color);
    CanvasStyle(const SRGBA<float>&);
    CanvasStyle(const CMYKA<float>&);
    CanvasStyle(CanvasGradient&);
    CanvasStyle(CanvasPattern&);

    static CanvasStyle createFromString(const String& color, CanvasBase&);
    static CanvasStyle createFromStringWithOverrideAlpha(const String& color, float alpha, CanvasBase&);

    bool isValid() const { return !WTF::holds_alternative<Invalid>(m_style); }
    bool isCurrentColor() const { return WTF::holds_alternative<CurrentColor>(m_style); }
    Optional<float> overrideAlpha() const { return WTF::get<CurrentColor>(m_style).overrideAlpha; }

    String color() const;
    RefPtr<CanvasGradient> canvasGradient() const;
    RefPtr<CanvasPattern> canvasPattern() const;

    void applyFillColor(GraphicsContext&) const;
    void applyStrokeColor(GraphicsContext&) const;

    bool isEquivalentColor(const CanvasStyle&) const;
    bool isEquivalent(const SRGBA<float>&) const;
    bool isEquivalent(const CMYKA<float>&) const;

private:
    struct Invalid { };

    struct CMYKAColor {
        Color colorConvertedToSRGBA;
        CMYKA<float> components;
    };

    struct CurrentColor {
        Optional<float> overrideAlpha;
    };

    CanvasStyle(CurrentColor);

    Variant<Invalid, Color, CMYKAColor, RefPtr<CanvasGradient>, RefPtr<CanvasPattern>, CurrentColor> m_style;
};

bool isCurrentColorString(const String& colorString);

Color currentColor(CanvasBase&);
Color parseColor(const String& colorString, CanvasBase&);
Color parseColorOrCurrentColor(const String& colorString, CanvasBase&);

inline CanvasStyle::CanvasStyle()
    : m_style(Invalid { })
{
}

inline RefPtr<CanvasGradient> CanvasStyle::canvasGradient() const
{
    if (!WTF::holds_alternative<RefPtr<CanvasGradient>>(m_style))
        return nullptr;
    return WTF::get<RefPtr<CanvasGradient>>(m_style);
}

inline RefPtr<CanvasPattern> CanvasStyle::canvasPattern() const
{
    if (!WTF::holds_alternative<RefPtr<CanvasPattern>>(m_style))
        return nullptr;
    return WTF::get<RefPtr<CanvasPattern>>(m_style);
}

inline String CanvasStyle::color() const
{
    return serializationForHTML(WTF::holds_alternative<Color>(m_style) ? WTF::get<Color>(m_style) : WTF::get<CMYKAColor>(m_style).colorConvertedToSRGBA);
}

} // namespace WebCore
