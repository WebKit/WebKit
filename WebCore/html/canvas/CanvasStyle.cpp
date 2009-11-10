/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "config.h"
#include "CanvasStyle.h"

#include "CSSParser.h"
#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "GraphicsContext.h"
#include <wtf/PassRefPtr.h>

#if PLATFORM(CG)
#include <CoreGraphics/CGContext.h>
#endif

#if PLATFORM(QT)
#include <QPainter>
#include <QBrush>
#include <QPen>
#include <QColor>
#endif

namespace WebCore {

CanvasStyle::CanvasStyle(const String& color)
    : m_type(ColorString)
    , m_color(color)
{
}

CanvasStyle::CanvasStyle(float grayLevel)
    : m_type(GrayLevel)
    , m_alpha(1)
    , m_grayLevel(grayLevel)
{
}

CanvasStyle::CanvasStyle(const String& color, float alpha)
    : m_type(ColorStringWithAlpha)
    , m_color(color)
    , m_alpha(alpha)
{
}

CanvasStyle::CanvasStyle(float grayLevel, float alpha)
    : m_type(GrayLevel)
    , m_alpha(alpha)
    , m_grayLevel(grayLevel)
{
}

CanvasStyle::CanvasStyle(float r, float g, float b, float a)
    : m_type(RGBA)
    , m_alpha(a)
    , m_red(r)
    , m_green(g)
    , m_blue(b)
{
}

CanvasStyle::CanvasStyle(float c, float m, float y, float k, float a)
    : m_type(CMYKA)
    , m_alpha(a)
    , m_cyan(c)
    , m_magenta(m)
    , m_yellow(y)
    , m_black(k)
{
}

CanvasStyle::CanvasStyle(PassRefPtr<CanvasGradient> gradient)
    : m_type(gradient ? Gradient : ColorString)
    , m_gradient(gradient)
{
}

CanvasStyle::CanvasStyle(PassRefPtr<CanvasPattern> pattern)
    : m_type(pattern ? ImagePattern : ColorString)
    , m_pattern(pattern)
{
}

void CanvasStyle::applyStrokeColor(GraphicsContext* context)
{
    if (!context)
        return;
    switch (m_type) {
        case ColorString: {
            Color c = Color(m_color);
            if (c.isValid()) {
                context->setStrokeColor(c.rgb(), DeviceColorSpace);
                break;
            }
            RGBA32 color = 0; // default is transparent black
            if (CSSParser::parseColor(color, m_color))
                context->setStrokeColor(color, DeviceColorSpace);
            break;
        }
        case ColorStringWithAlpha: {
            Color c = Color(m_color);
            if (c.isValid()) {
                context->setStrokeColor(colorWithOverrideAlpha(c.rgb(), m_alpha), DeviceColorSpace);
                break;
            }
            RGBA32 color = 0; // default is transparent black
            if (CSSParser::parseColor(color, m_color))
                context->setStrokeColor(colorWithOverrideAlpha(color, m_alpha), DeviceColorSpace);
            break;
        }
        case GrayLevel:
            // We're only supporting 255 levels of gray here.  Since this isn't
            // even part of HTML5, I don't expect anyone will care.  If they do
            // we'll make a fancier Color abstraction.
            context->setStrokeColor(Color(m_grayLevel, m_grayLevel, m_grayLevel, m_alpha), DeviceColorSpace);
            break;
        case RGBA:
            context->setStrokeColor(Color(m_red, m_green, m_blue, m_alpha), DeviceColorSpace);
            break;
        case CMYKA: {
            // FIXME: Do this through platform-independent GraphicsContext API.
            // We'll need a fancier Color abstraction to support CYMKA correctly
#if PLATFORM(CG)
            CGContextSetCMYKStrokeColor(context->platformContext(), m_cyan, m_magenta, m_yellow, m_black, m_alpha);
#elif PLATFORM(QT)
            QPen currentPen = context->platformContext()->pen();
            QColor clr;
            clr.setCmykF(m_cyan, m_magenta, m_yellow, m_black, m_alpha);
            currentPen.setColor(clr);
            context->platformContext()->setPen(currentPen);
#else
            context->setStrokeColor(Color(m_cyan, m_magenta, m_yellow, m_black, m_alpha), DeviceColorSpace);
#endif
            break;
        }
        case Gradient:
            context->setStrokeGradient(canvasGradient()->gradient());
            break;
        case ImagePattern:
            context->setStrokePattern(canvasPattern()->pattern());
            break;
    }
}

void CanvasStyle::applyFillColor(GraphicsContext* context)
{
    if (!context)
        return;
    switch (m_type) {
        case ColorString: {
            Color c = Color(m_color);
            if (c.isValid()) {
                context->setFillColor(c.rgb(), DeviceColorSpace);
                break;
            }
            RGBA32 rgba = 0; // default is transparent black
            if (CSSParser::parseColor(rgba, m_color))
                context->setFillColor(rgba, DeviceColorSpace);
            break;
        }
        case ColorStringWithAlpha: {
            Color c = Color(m_color);
            if (c.isValid()) {
                context->setFillColor(colorWithOverrideAlpha(c.rgb(), m_alpha), DeviceColorSpace);
                break;
            }
            RGBA32 color = 0; // default is transparent black
            if (CSSParser::parseColor(color, m_color))
                context->setFillColor(colorWithOverrideAlpha(color, m_alpha), DeviceColorSpace);
            break;
        }
        case GrayLevel:
            // We're only supporting 255 levels of gray here.  Since this isn't
            // even part of HTML5, I don't expect anyone will care.  If they do
            // we'll make a fancier Color abstraction.
            context->setFillColor(Color(m_grayLevel, m_grayLevel, m_grayLevel, m_alpha), DeviceColorSpace);
            break;
        case RGBA:
            context->setFillColor(Color(m_red, m_green, m_blue, m_alpha), DeviceColorSpace);
            break;
        case CMYKA: {
            // FIXME: Do this through platform-independent GraphicsContext API.
            // We'll need a fancier Color abstraction to support CYMKA correctly
#if PLATFORM(CG)
            CGContextSetCMYKFillColor(context->platformContext(), m_cyan, m_magenta, m_yellow, m_black, m_alpha);
#elif PLATFORM(QT)
            QBrush currentBrush = context->platformContext()->brush();
            QColor clr;
            clr.setCmykF(m_cyan, m_magenta, m_yellow, m_black, m_alpha);
            currentBrush.setColor(clr);
            context->platformContext()->setBrush(currentBrush);
#else
            context->setFillColor(Color(m_cyan, m_magenta, m_yellow, m_black, m_alpha), DeviceColorSpace);
#endif
            break;
        }
        case Gradient:
            context->setFillGradient(canvasGradient()->gradient());
            break;
        case ImagePattern:
            context->setFillPattern(canvasPattern()->pattern());
            break;
    }
}

}
