/*
 * Copyright (C) 2006, 2008, 2013 Apple Inc. All rights reserved.
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
#include "StyleProperties.h"

#if USE(CG)
#include <CoreGraphics/CGContext.h>
#endif

namespace WebCore {

static bool isCurrentColorString(const String& colorString)
{
    return equalLettersIgnoringASCIICase(colorString, "currentcolor");
}

static Color parseColor(const String& colorString, Document* document = nullptr)
{
    Color color = CSSParser::parseColor(colorString);
    if (color.isValid())
        return color;

    return CSSParser::parseSystemColor(colorString, document);
}

Color currentColor(HTMLCanvasElement* canvas)
{
    if (!canvas || !canvas->inDocument() || !canvas->inlineStyle())
        return Color::black;
    Color color = CSSParser::parseColor(canvas->inlineStyle()->getPropertyValue(CSSPropertyColor));
    if (!color.isValid())
        return Color::black;
    return color;
}

Color parseColorOrCurrentColor(const String& colorString, HTMLCanvasElement* canvas)
{
    if (isCurrentColorString(colorString))
        return currentColor(canvas);

    return parseColor(colorString, canvas ? &canvas->document() : nullptr);
}

CanvasStyle::CanvasStyle(Color color)
    : m_color(color)
    , m_type(RGBA)
{
}

CanvasStyle::CanvasStyle(float grayLevel, float alpha)
    : m_color(Color(grayLevel, grayLevel, grayLevel, alpha))
    , m_type(RGBA)
{
}

CanvasStyle::CanvasStyle(float r, float g, float b, float a)
    : m_color(Color(r, g, b, a))
    , m_type(RGBA)
{
}

CanvasStyle::CanvasStyle(float c, float m, float y, float k, float a)
    : m_cmyka(new CMYKAValues(Color(c, m, y, k, a), c, m, y, k, a))
    , m_type(CMYKA)
{
}

CanvasStyle::CanvasStyle(PassRefPtr<CanvasGradient> gradient)
    : m_gradient(gradient.leakRef())
    , m_type(Gradient)
{
    if (!m_gradient)
        m_type = Invalid;
}

CanvasStyle::CanvasStyle(PassRefPtr<CanvasPattern> pattern)
    : m_pattern(pattern.leakRef())
    , m_type(ImagePattern)
{
    if (!m_pattern)
        m_type = Invalid;
}

CanvasStyle::~CanvasStyle()
{
    if (m_type == Gradient)
        m_gradient->deref();
    else if (m_type == ImagePattern)
        m_pattern->deref();
    else if (m_type == CMYKA)
        delete m_cmyka;
}

CanvasStyle CanvasStyle::createFromString(const String& colorString, Document* document)
{
    if (isCurrentColorString(colorString))
        return CanvasStyle(ConstructCurrentColor);

    Color color = parseColor(colorString, document);
    if (color.isValid())
        return CanvasStyle(color);

    return CanvasStyle();
}

CanvasStyle CanvasStyle::createFromStringWithOverrideAlpha(const String& colorString, float alpha)
{
    if (isCurrentColorString(colorString))
        return CanvasStyle(CurrentColorWithOverrideAlpha, alpha);

    Color color = parseColor(colorString);
    if (color.isValid())
        return CanvasStyle(colorWithOverrideAlpha(color.rgb(), alpha));

    return CanvasStyle();
}

bool CanvasStyle::isEquivalentColor(const CanvasStyle& other) const
{
    if (m_type != other.m_type)
        return false;

    switch (m_type) {
    case RGBA:
        return m_color == other.m_color;
    case CMYKA:
        return m_cmyka->c == other.m_cmyka->c
            && m_cmyka->m == other.m_cmyka->m
            && m_cmyka->y == other.m_cmyka->y
            && m_cmyka->k == other.m_cmyka->k
            && m_cmyka->a == other.m_cmyka->a;
    case Gradient:
    case ImagePattern:
    case CurrentColor:
    case CurrentColorWithOverrideAlpha:
        return false;
    case Invalid:
        break;
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool CanvasStyle::isEquivalentRGBA(float r, float g, float b, float a) const
{
    if (m_type != RGBA)
        return false;

    return m_color == Color(r, g, b, a);
}

bool CanvasStyle::isEquivalentCMYKA(float c, float m, float y, float k, float a) const
{
    if (m_type != CMYKA)
        return false;

    return c == m_cmyka->c
        && m == m_cmyka->m
        && y == m_cmyka->y
        && k == m_cmyka->k
        && a == m_cmyka->a;
}

CanvasStyle::CanvasStyle(const CanvasStyle& other)
{
    memcpy(this, &other, sizeof(CanvasStyle));
    if (m_type == Gradient)
        m_gradient->ref();
    else if (m_type == ImagePattern)
        m_pattern->ref();
    else if (m_type == CMYKA)
        m_cmyka = new CMYKAValues(other.m_cmyka->color, other.m_cmyka->c, other.m_cmyka->m, other.m_cmyka->y, other.m_cmyka->k, other.m_cmyka->a);
}

CanvasStyle& CanvasStyle::operator=(const CanvasStyle& other)
{
    if (this != &other) {
        this->~CanvasStyle();
        new (this) CanvasStyle(other);
    }
    return *this;
}

void CanvasStyle::applyStrokeColor(GraphicsContext* context) const
{
    if (!context)
        return;
    switch (m_type) {
    case RGBA:
        context->setStrokeColor(m_color);
        break;
    case CMYKA: {
        // FIXME: Do this through platform-independent GraphicsContext API.
        // We'll need a fancier Color abstraction to support CMYKA correctly
#if USE(CG)
        CGContextSetCMYKStrokeColor(context->platformContext(), m_cmyka->c, m_cmyka->m, m_cmyka->y, m_cmyka->k, m_cmyka->a);
#else
        context->setStrokeColor(m_cmyka->color);
#endif
        break;
    }
    case Gradient:
        context->setStrokeGradient(canvasGradient()->gradient());
        break;
    case ImagePattern:
        context->setStrokePattern(canvasPattern()->pattern());
        break;
    case CurrentColor:
    case CurrentColorWithOverrideAlpha:
    case Invalid:
        ASSERT_NOT_REACHED();
        break;
    }
}

void CanvasStyle::applyFillColor(GraphicsContext* context) const
{
    if (!context)
        return;
    switch (m_type) {
    case RGBA:
        context->setFillColor(m_color);
        break;
    case CMYKA: {
        // FIXME: Do this through platform-independent GraphicsContext API.
        // We'll need a fancier Color abstraction to support CMYKA correctly
#if USE(CG)
        CGContextSetCMYKFillColor(context->platformContext(), m_cmyka->c, m_cmyka->m, m_cmyka->y, m_cmyka->k, m_cmyka->a);
#else
        context->setFillColor(m_cmyka->color);
#endif
        break;
    }
    case Gradient:
        context->setFillGradient(canvasGradient()->gradient());
        break;
    case ImagePattern:
        context->setFillPattern(canvasPattern()->pattern());
        break;
    case CurrentColor:
    case CurrentColorWithOverrideAlpha:
    case Invalid:
        ASSERT_NOT_REACHED();
        break;
    }
}

}
