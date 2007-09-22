/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Trolltech ASA
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

#if PLATFORM(QT)
#include <QPainter>
#include <QBrush>
#include <QPen>
#include <QColor>
#endif

namespace WebCore {

CanvasStyle::CanvasStyle(const String& color)
    : m_type(ColorString), m_color(color)
{
}

CanvasStyle::CanvasStyle(float grayLevel)
    : m_type(GrayLevel), m_alpha(1), m_grayLevel(grayLevel)
{
}

CanvasStyle::CanvasStyle(const String& color, float alpha)
    : m_type(ColorStringWithAlpha), m_color(color), m_alpha(alpha)
{
}

CanvasStyle::CanvasStyle(float grayLevel, float alpha)
    : m_type(GrayLevel), m_alpha(alpha), m_grayLevel(grayLevel)
{
}

CanvasStyle::CanvasStyle(float r, float g, float b, float a)
    : m_type(RGBA), m_alpha(a), m_red(r), m_green(g), m_blue(b)
{
}

CanvasStyle::CanvasStyle(float c, float m, float y, float k, float a)
    : m_type(CMYKA), m_alpha(a), m_cyan(c), m_magenta(m), m_yellow(y), m_black(k)
{
}

CanvasStyle::CanvasStyle(PassRefPtr<CanvasGradient> gradient)
    : m_type(gradient ? Gradient : ColorString), m_gradient(gradient)
{
}

CanvasStyle::CanvasStyle(PassRefPtr<CanvasPattern> pattern)
    : m_type(pattern ? ImagePattern : ColorString), m_pattern(pattern)
{
}

void CanvasStyle::applyStrokeColor(GraphicsContext* context)
{
    if (!context)
        return;
#if PLATFORM(QT)
    QPainter* p = static_cast<QPainter*>(context->platformContext());
#endif
    switch (m_type) {
        case ColorString: {
            RGBA32 color = CSSParser::parseColor(m_color);
            // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
            CGContextSetRGBStrokeColor(context->platformContext(),
                ((color >> 16) & 0xFF) / 255.0f,
                ((color >> 8) & 0xFF) / 255.0f,
                (color & 0xFF) / 255.0f,
                ((color >> 24) & 0xFF) / 255.0f);
#elif PLATFORM(QT)
            QPen currentPen = p->pen();
            currentPen.setColor((QColor(QRgb(color))));
            p->setPen(currentPen);
#endif
            break;
        }
        case ColorStringWithAlpha: {
            RGBA32 color = CSSParser::parseColor(m_color);
            // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
            CGContextSetRGBStrokeColor(context->platformContext(),
                ((color >> 16) & 0xFF) / 255.0f,
                ((color >> 8) & 0xFF) / 255.0f,
                (color & 0xFF) / 255.0f,
                m_alpha);
#elif PLATFORM(QT)
            QPen currentPen = p->pen();
            QColor clr = QColor(QRgb(color));
            clr.setAlphaF(m_alpha);
            currentPen.setColor(clr);
            p->setPen(currentPen);
#endif
            break;
        }
        case GrayLevel: {
            // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
            CGContextSetGrayStrokeColor(context->platformContext(), m_grayLevel, m_alpha);
#elif PLATFORM(QT)
            QColor clr;
            QPen currentPen = p->pen();
            clr.setRgbF(m_grayLevel, m_grayLevel, m_grayLevel, m_alpha);
            currentPen.setColor(clr);
            p->setPen(currentPen);
#endif
            break;
        }
        case RGBA: {
            // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
            CGContextSetRGBStrokeColor(context->platformContext(), m_red, m_green, m_blue, m_alpha);
#elif PLATFORM(QT)
            QPen currentPen = p->pen();
            QColor clr;
            clr.setRgbF(m_red, m_green, m_blue, m_alpha);
            currentPen.setColor(clr);
            p->setPen(currentPen);
#endif
            break;
        }
        case CMYKA: {
            // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
            CGContextSetCMYKStrokeColor(context->platformContext(), m_cyan, m_magenta, m_yellow, m_black, m_alpha);
#elif PLATFORM(QT)
            QPen currentPen = p->pen();
            QColor clr;
            clr.setCmykF(m_cyan, m_magenta, m_yellow, m_black, m_alpha);
            currentPen.setColor(clr);
            p->setPen(currentPen);
#endif
            break;
        }
        case Gradient:
        case ImagePattern:
            break;
    }
}

void CanvasStyle::applyFillColor(GraphicsContext* context)
{
    if (!context)
        return;
#if PLATFORM(QT)
    QPainter* p = static_cast<QPainter*>(context->platformContext());
#endif
    switch (m_type) {
        case ColorString: {
            RGBA32 color = CSSParser::parseColor(m_color);
            // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
            CGContextSetRGBFillColor(context->platformContext(),
                ((color >> 16) & 0xFF) / 255.0f,
                ((color >> 8) & 0xFF) / 255.0f,
                (color & 0xFF) / 255.0f,
                ((color >> 24) & 0xFF) / 255.0f);
#elif PLATFORM(QT)
            QBrush currentBrush = p->brush();
            QColor clr;
            clr.setRgb(QRgb(color));
            currentBrush.setColor(clr);
            p->setBrush(currentBrush);
#endif
            break;
        }
        case ColorStringWithAlpha: {
            RGBA32 color = CSSParser::parseColor(m_color);
            // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
            CGContextSetRGBFillColor(context->platformContext(),
                ((color >> 16) & 0xFF) / 255.0f,
                ((color >> 8) & 0xFF) / 255.0f,
                (color & 0xFF) / 255.0f,
                m_alpha);
#elif PLATFORM(QT)
            QBrush currentBrush = p->brush();
            QColor clr;
            clr.setRgb(QRgb(color));
            clr.setAlphaF(m_alpha);
            currentBrush.setColor(clr);
            p->setBrush(currentBrush);
#endif
            break;
        }
        case GrayLevel: {
            // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
            CGContextSetGrayFillColor(context->platformContext(), m_grayLevel, m_alpha);
#elif PLATFORM(QT)
            QBrush currentBrush = p->brush();
            QColor clr;
            clr.setRgbF(m_grayLevel, m_grayLevel, m_grayLevel, m_alpha);
            currentBrush.setColor(clr);
            p->setBrush(currentBrush);
#endif
            break;
        }
        case RGBA: {
            // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
            CGContextSetRGBFillColor(context->platformContext(), m_red, m_green, m_blue, m_alpha);
#elif PLATFORM(QT)
            QBrush currentBrush = p->brush();
            QColor clr;
            clr.setRgbF(m_red, m_green, m_blue, m_alpha);
            currentBrush.setColor(clr);
            p->setBrush(currentBrush);
#endif
            break;
        }
        case CMYKA: {
            // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
            CGContextSetCMYKFillColor(context->platformContext(), m_cyan, m_magenta, m_yellow, m_black, m_alpha);
#elif PLATFORM(QT)
            QBrush currentBrush = p->brush();
            QColor clr;
            clr.setCmykF(m_cyan, m_magenta, m_yellow, m_black, m_alpha);
            currentBrush.setColor(clr);
            p->setBrush(currentBrush);
#endif
            break;
        }
        case Gradient:
        case ImagePattern:
            break;
    }
}

}
