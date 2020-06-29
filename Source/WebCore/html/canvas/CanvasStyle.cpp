/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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
#include "CanvasStyle.h"

#include "CSSParser.h"
#include "CSSPropertyNames.h"
#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "GraphicsContext.h"
#include "HTMLCanvasElement.h"
#if ENABLE(OFFSCREEN_CANVAS)
#include "OffscreenCanvas.h"
#endif
#include "StyleProperties.h"

#if USE(CG)
#include <CoreGraphics/CGContext.h>
#endif

namespace WebCore {

bool isCurrentColorString(const String& colorString)
{
    return equalLettersIgnoringASCIICase(colorString, "currentcolor");
}

Color parseColor(const String& colorString, CanvasBase& canvasBase)
{
#if ENABLE(OFFSCREEN_CANVAS)
    if (canvasBase.isOffscreenCanvas())
        return CSSParser::parseColorWorkerSafe(colorString);
#else
    UNUSED_PARAM(canvasBase);
#endif
    Color color = CSSParser::parseColor(colorString);
    if (color.isValid())
        return color;
    return CSSParser::parseSystemColor(colorString);
}

Color currentColor(CanvasBase& canvasBase)
{
    if (!is<HTMLCanvasElement>(canvasBase))
        return Color::black;

    auto& canvas = downcast<HTMLCanvasElement>(canvasBase);
    if (!canvas.isConnected() || !canvas.inlineStyle())
        return Color::black;
    Color color = CSSParser::parseColor(canvas.inlineStyle()->getPropertyValue(CSSPropertyColor));
    if (!color.isValid())
        return Color::black;
    return color;
}

Color parseColorOrCurrentColor(const String& colorString, CanvasBase& canvasBase)
{
    if (isCurrentColorString(colorString))
        return currentColor(canvasBase);

    return parseColor(colorString, canvasBase);
}

CanvasStyle::CanvasStyle(Color color)
    : m_style(color)
{
}

CanvasStyle::CanvasStyle(const SRGBA<float>& colorComponents)
    : m_style(makeSimpleColor(colorComponents))
{
}

CanvasStyle::CanvasStyle(const CMYKA<float>& colorComponents)
    : m_style(CMYKAColor { makeSimpleColor(toSRGBA(colorComponents)), colorComponents })
{
}

CanvasStyle::CanvasStyle(CanvasGradient& gradient)
    : m_style(makeRefPtr(gradient))
{
}

CanvasStyle::CanvasStyle(CanvasPattern& pattern)
    : m_style(makeRefPtr(pattern))
{
}

inline CanvasStyle::CanvasStyle(CurrentColor color)
    : m_style(color)
{
}

CanvasStyle CanvasStyle::createFromString(const String& colorString, CanvasBase& canvasBase)
{
    if (isCurrentColorString(colorString))
        return CurrentColor { WTF::nullopt };

    Color color = parseColor(colorString, canvasBase);
    if (!color.isValid())
        return { };

    return color;
}

CanvasStyle CanvasStyle::createFromStringWithOverrideAlpha(const String& colorString, float alpha, CanvasBase& canvasBase)
{
    if (isCurrentColorString(colorString))
        return CurrentColor { alpha };

    Color color = parseColor(colorString, canvasBase);
    if (!color.isValid())
        return { };

    return color.colorWithAlpha(alpha);
}

bool CanvasStyle::isEquivalentColor(const CanvasStyle& other) const
{
    if (WTF::holds_alternative<Color>(m_style) && WTF::holds_alternative<Color>(other.m_style))
        return WTF::get<Color>(m_style) == WTF::get<Color>(other.m_style);

    if (WTF::holds_alternative<CMYKAColor>(m_style) && WTF::holds_alternative<CMYKAColor>(other.m_style))
        return WTF::get<CMYKAColor>(m_style).components == WTF::get<CMYKAColor>(other.m_style).components;

    return false;
}

bool CanvasStyle::isEquivalent(const SRGBA<float>& components) const
{
    return WTF::holds_alternative<Color>(m_style) && WTF::get<Color>(m_style) == makeSimpleColor(components);
}

bool CanvasStyle::isEquivalent(const CMYKA<float>& components) const
{
    return WTF::holds_alternative<CMYKAColor>(m_style) && WTF::get<CMYKAColor>(m_style).components == components;
}

void CanvasStyle::applyStrokeColor(GraphicsContext& context) const
{
    WTF::switchOn(m_style,
        [&context] (const Color& color) {
            context.setStrokeColor(color);
        },
        [&context] (const CMYKAColor& color) {
            // FIXME: Do this through platform-independent GraphicsContext API.
            // We'll need a fancier Color abstraction to support CMYKA correctly
#if USE(CG)
            CGContextSetCMYKStrokeColor(context.platformContext(), color.components.cyan, color.components.magenta, color.components.yellow, color.components.black, color.components.alpha);
#else
            context.setStrokeColor(color.colorConvertedToSRGBA);
#endif
        },
        [&context] (const RefPtr<CanvasGradient>& gradient) {
            context.setStrokeGradient(gradient->gradient());
        },
        [&context] (const RefPtr<CanvasPattern>& pattern) {
            context.setStrokePattern(pattern->pattern());
        },
        [] (const CurrentColor&) {
            ASSERT_NOT_REACHED();
        },
        [] (const Invalid&) {
            ASSERT_NOT_REACHED();
        }
    );
}

void CanvasStyle::applyFillColor(GraphicsContext& context) const
{
    WTF::switchOn(m_style,
        [&context] (const Color& color) {
            context.setFillColor(color);
        },
        [&context] (const CMYKAColor& color) {
            // FIXME: Do this through platform-independent GraphicsContext API.
            // We'll need a fancier Color abstraction to support CMYKA correctly
#if USE(CG)
            CGContextSetCMYKFillColor(context.platformContext(), color.components.cyan, color.components.magenta, color.components.yellow, color.components.black, color.components.alpha);
#else
            context.setFillColor(color.colorConvertedToSRGBA);
#endif
        },
        [&context] (const RefPtr<CanvasGradient>& gradient) {
            context.setFillGradient(gradient->gradient());
        },
        [&context] (const RefPtr<CanvasPattern>& pattern) {
            context.setFillPattern(pattern->pattern());
        },
        [] (const CurrentColor&) {
            ASSERT_NOT_REACHED();
        },
        [] (const Invalid&) {
            ASSERT_NOT_REACHED();
        }
    );
}

}
