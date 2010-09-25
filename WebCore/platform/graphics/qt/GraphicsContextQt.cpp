/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2006 Allan Sandfeld Jensen <sandfeld@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2008 Dirk Schulze <vbs85@gmx.de>
 * Copyright (C) 2010 Sencha, Inc.
 *
 * All rights reserved.
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
#include "GraphicsContext.h"

#ifdef Q_WS_WIN
#include <windows.h>
#endif

#include "AffineTransform.h"
#include "Color.h"
#include "ContextShadow.h"
#include "FloatConversion.h"
#include "Font.h"
#include "GraphicsContextPrivate.h"
#include "ImageBuffer.h"
#include "NotImplemented.h"
#include "Path.h"
#include "Pattern.h"
#include "Pen.h"
#include "TransparencyLayer.h"

#include <QBrush>
#include <QDebug>
#include <QGradient>
#include <QPaintDevice>
#include <QPaintEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QStack>
#include <QVector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace WebCore {

QPainter::CompositionMode GraphicsContext::toQtCompositionMode(CompositeOperator op)
{
    switch (op) {
    case CompositeClear:
        return QPainter::CompositionMode_Clear;
    case CompositeCopy:
        return QPainter::CompositionMode_Source;
    case CompositeSourceOver:
        return QPainter::CompositionMode_SourceOver;
    case CompositeSourceIn:
        return QPainter::CompositionMode_SourceIn;
    case CompositeSourceOut:
        return QPainter::CompositionMode_SourceOut;
    case CompositeSourceAtop:
        return QPainter::CompositionMode_SourceAtop;
    case CompositeDestinationOver:
        return QPainter::CompositionMode_DestinationOver;
    case CompositeDestinationIn:
        return QPainter::CompositionMode_DestinationIn;
    case CompositeDestinationOut:
        return QPainter::CompositionMode_DestinationOut;
    case CompositeDestinationAtop:
        return QPainter::CompositionMode_DestinationAtop;
    case CompositeXOR:
        return QPainter::CompositionMode_Xor;
    case CompositePlusDarker:
        // there is no exact match, but this is the closest
        return QPainter::CompositionMode_Darken;
    case CompositeHighlight:
        return QPainter::CompositionMode_SourceOver;
    case CompositePlusLighter:
        return QPainter::CompositionMode_Plus;
    }

    return QPainter::CompositionMode_SourceOver;
}

static inline Qt::PenCapStyle toQtLineCap(LineCap lc)
{
    switch (lc) {
    case ButtCap:
        return Qt::FlatCap;
    case RoundCap:
        return Qt::RoundCap;
    case SquareCap:
        return Qt::SquareCap;
    }

    return Qt::FlatCap;
}

static inline Qt::PenJoinStyle toQtLineJoin(LineJoin lj)
{
    switch (lj) {
    case MiterJoin:
        return Qt::SvgMiterJoin;
    case RoundJoin:
        return Qt::RoundJoin;
    case BevelJoin:
        return Qt::BevelJoin;
    }

    return Qt::MiterJoin;
}

static Qt::PenStyle toQPenStyle(StrokeStyle style)
{
    switch (style) {
    case NoStroke:
        return Qt::NoPen;
        break;
    case SolidStroke:
        return Qt::SolidLine;
        break;
    case DottedStroke:
        return Qt::DotLine;
        break;
    case DashedStroke:
        return Qt::DashLine;
        break;
    }
    qWarning("couldn't recognize the pen style");
    return Qt::NoPen;
}

static inline Qt::FillRule toQtFillRule(WindRule rule)
{
    switch (rule) {
    case RULE_EVENODD:
        return Qt::OddEvenFill;
    case RULE_NONZERO:
        return Qt::WindingFill;
    }
    qDebug("Qt: unrecognized wind rule!");
    return Qt::OddEvenFill;
}

class GraphicsContextPlatformPrivate : public Noncopyable {
public:
    GraphicsContextPlatformPrivate(QPainter* painter);
    ~GraphicsContextPlatformPrivate();

    inline QPainter* p() const
    {
        if (layers.isEmpty())
            return painter;
        return &layers.top()->painter;
    }

    bool antiAliasingForRectsAndLines;

    QStack<TransparencyLayer*> layers;
    // Counting real layers. Required by inTransparencyLayer() calls
    // For example, layers with valid alphaMask are not real layers
    int layerCount;

    // reuse this brush for solid color (to prevent expensive QBrush construction)
    QBrush solidColor;

    InterpolationQuality imageInterpolationQuality;

    // Only used by SVG for now.
    QPainterPath currentPath;

    ContextShadow shadow;
    QStack<ContextShadow> shadowStack;

    bool hasShadow() const
    {
        return shadow.m_type != ContextShadow::NoShadow;
    }

    QRectF clipBoundingRect() const
    {
#if QT_VERSION >= QT_VERSION_CHECK(4, 8, 0)
        return p()->clipBoundingRect();
#else
        return p()->clipRegion().boundingRect();
#endif
    }

private:
    QPainter* painter;
};


GraphicsContextPlatformPrivate::GraphicsContextPlatformPrivate(QPainter* p)
{
    painter = p;
    layerCount = 0;

    solidColor = QBrush(Qt::black);

    imageInterpolationQuality = InterpolationDefault;

    if (painter) {
        // use the default the QPainter was constructed with
        antiAliasingForRectsAndLines = painter->testRenderHint(QPainter::Antialiasing);
        // FIXME: Maybe only enable in SVG mode?
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    } else
        antiAliasingForRectsAndLines = false;
}

GraphicsContextPlatformPrivate::~GraphicsContextPlatformPrivate()
{
}

GraphicsContext::GraphicsContext(PlatformGraphicsContext* context)
    : m_common(createGraphicsContextPrivate())
    , m_data(new GraphicsContextPlatformPrivate(context))
{
    setPaintingDisabled(!context);
    if (context) {
        // Make sure the context starts in sync with our state.
        setPlatformFillColor(fillColor(), DeviceColorSpace);
        setPlatformStrokeColor(strokeColor(), DeviceColorSpace);

        // Make sure we start with the correct join mode.
        setLineJoin(MiterJoin);
    }
}

GraphicsContext::~GraphicsContext()
{
    while (!m_data->layers.isEmpty())
        endTransparencyLayer();

    destroyGraphicsContextPrivate(m_common);
    delete m_data;
}

PlatformGraphicsContext* GraphicsContext::platformContext() const
{
    return m_data->p();
}

AffineTransform GraphicsContext::getCTM() const
{
    QTransform matrix(platformContext()->combinedTransform());
    return AffineTransform(matrix.m11(), matrix.m12(), matrix.m21(),
                           matrix.m22(), matrix.dx(), matrix.dy());
}

void GraphicsContext::savePlatformState()
{
    if (!m_data->layers.isEmpty() && !m_data->layers.top()->alphaMask.isNull())
        ++m_data->layers.top()->saveCounter;
    m_data->p()->save();
    m_data->shadowStack.push(m_data->shadow);
}

void GraphicsContext::restorePlatformState()
{
    if (!m_data->layers.isEmpty() && !m_data->layers.top()->alphaMask.isNull())
        if (!--m_data->layers.top()->saveCounter)
            endTransparencyLayer();

    m_data->p()->restore();

    if (!m_data->currentPath.isEmpty() && m_common->state.pathTransform.isInvertible()) {
        QTransform matrix = m_common->state.pathTransform;
        m_data->currentPath = m_data->currentPath * matrix;
    }

    if (m_data->shadowStack.isEmpty())
        m_data->shadow = ContextShadow();
    else
        m_data->shadow = m_data->shadowStack.pop();
}

// Draws a filled rectangle with a stroked border.
// This is only used to draw borders (real fill is done via fillRect), and
// thus it must not cast any shadow.
void GraphicsContext::drawRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    QPainter* p = m_data->p();
    const bool antiAlias = p->testRenderHint(QPainter::Antialiasing);
    p->setRenderHint(QPainter::Antialiasing, m_data->antiAliasingForRectsAndLines);

    p->drawRect(rect);

    p->setRenderHint(QPainter::Antialiasing, antiAlias);
}

// This is only used to draw borders.
// Must not cast any shadow.
void GraphicsContext::drawLine(const IntPoint& point1, const IntPoint& point2)
{
    if (paintingDisabled())
        return;

    StrokeStyle style = strokeStyle();
    Color color = strokeColor();
    if (style == NoStroke)
        return;

    float width = strokeThickness();

    FloatPoint p1 = point1;
    FloatPoint p2 = point2;
    bool isVerticalLine = (p1.x() == p2.x());

    QPainter* p = m_data->p();
    const bool antiAlias = p->testRenderHint(QPainter::Antialiasing);
    p->setRenderHint(QPainter::Antialiasing, m_data->antiAliasingForRectsAndLines);
    adjustLineToPixelBoundaries(p1, p2, width, style);

    int patWidth = 0;
    switch (style) {
    case NoStroke:
    case SolidStroke:
        break;
    case DottedStroke:
        patWidth = static_cast<int>(width);
        break;
    case DashedStroke:
        patWidth = 3 * static_cast<int>(width);
        break;
    }

    if (patWidth) {
        p->save();

        // Do a rect fill of our endpoints.  This ensures we always have the
        // appearance of being a border.  We then draw the actual dotted/dashed line.
        if (isVerticalLine) {
            p->fillRect(FloatRect(p1.x() - width / 2, p1.y() - width, width, width), QColor(color));
            p->fillRect(FloatRect(p2.x() - width / 2, p2.y(), width, width), QColor(color));
        } else {
            p->fillRect(FloatRect(p1.x() - width, p1.y() - width / 2, width, width), QColor(color));
            p->fillRect(FloatRect(p2.x(), p2.y() - width / 2, width, width), QColor(color));
        }

        // Example: 80 pixels with a width of 30 pixels.
        // Remainder is 20.  The maximum pixels of line we could paint
        // will be 50 pixels.
        int distance = (isVerticalLine ? (point2.y() - point1.y()) : (point2.x() - point1.x())) - 2*(int)width;
        int remainder = distance % patWidth;
        int coverage = distance - remainder;
        int numSegments = coverage / patWidth;

        float patternOffset = 0.0f;
        // Special case 1px dotted borders for speed.
        if (patWidth == 1)
            patternOffset = 1.0f;
        else {
            bool evenNumberOfSegments = !(numSegments % 2);
            if (remainder)
                evenNumberOfSegments = !evenNumberOfSegments;
            if (evenNumberOfSegments) {
                if (remainder) {
                    patternOffset += patWidth - remainder;
                    patternOffset += remainder / 2;
                } else
                    patternOffset = patWidth / 2;
            } else {
                if (remainder)
                    patternOffset = (patWidth - remainder) / 2;
            }
        }

        QVector<qreal> dashes;
        dashes << qreal(patWidth) / width << qreal(patWidth) / width;

        QPen pen = p->pen();
        pen.setWidthF(width);
        pen.setCapStyle(Qt::FlatCap);
        pen.setDashPattern(dashes);
        pen.setDashOffset(patternOffset / width);
        p->setPen(pen);
    }

    p->drawLine(p1, p2);

    if (patWidth)
        p->restore();

    p->setRenderHint(QPainter::Antialiasing, antiAlias);
}

// This method is only used to draw the little circles used in lists.
void GraphicsContext::drawEllipse(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    m_data->p()->drawEllipse(rect);
}

void GraphicsContext::strokeArc(const IntRect& rect, int startAngle, int angleSpan)
{
    if (paintingDisabled() || strokeStyle() == NoStroke || strokeThickness() <= 0.0f)
        return;

    QPainter* p = m_data->p();
    const bool antiAlias = p->testRenderHint(QPainter::Antialiasing);
    p->setRenderHint(QPainter::Antialiasing, true);

    startAngle *= 16;
    angleSpan *= 16;

    if (m_data->hasShadow()) {
        p->save();
        p->translate(m_data->shadow.offset());
        QPen pen(p->pen());
        pen.setColor(m_data->shadow.m_color);
        p->setPen(pen);
        p->drawArc(rect, startAngle, angleSpan);
        p->restore();
    }
    p->drawArc(rect, startAngle, angleSpan);

    p->setRenderHint(QPainter::Antialiasing, antiAlias);
}

void GraphicsContext::drawConvexPolygon(size_t npoints, const FloatPoint* points, bool shouldAntialias)
{
    if (paintingDisabled())
        return;

    if (npoints <= 1)
        return;

    QPolygonF polygon(npoints);

    for (size_t i = 0; i < npoints; i++)
        polygon[i] = points[i];

    QPainter* p = m_data->p();
    p->save();
    p->setRenderHint(QPainter::Antialiasing, shouldAntialias);
    if (m_data->hasShadow()) {
        p->save();
        p->translate(m_data->shadow.offset());
        if (p->brush().style() != Qt::NoBrush)
            p->setBrush(QBrush(m_data->shadow.m_color));
        QPen pen(p->pen());
        if (pen.style() != Qt::NoPen) {
            pen.setColor(m_data->shadow.m_color);
            p->setPen(pen);
        }
        p->drawConvexPolygon(polygon);
        p->restore();
    }
    p->drawConvexPolygon(polygon);
    p->restore();
}

void GraphicsContext::clipConvexPolygon(size_t numPoints, const FloatPoint* points, bool antialiased)
{
    if (paintingDisabled())
        return;

    if (numPoints <= 1)
        return;

    QPainterPath path(points[0]);
    for (size_t i = 1; i < numPoints; ++i)
        path.lineTo(points[i]);
    path.setFillRule(Qt::WindingFill);
    m_data->p()->setClipPath(path, Qt::IntersectClip);
}

QPen GraphicsContext::pen()
{
    if (paintingDisabled())
        return QPen();

    QPainter* p = m_data->p();
    return p->pen();
}

void GraphicsContext::fillPath()
{
    if (paintingDisabled())
        return;

    QPainter* p = m_data->p();
    QPainterPath& path = m_data->currentPath; // Avoid detaching the QPainterPath
    path.setFillRule(toQtFillRule(fillRule()));

    if (m_data->hasShadow()) {
        p->translate(m_data->shadow.offset());
        p->fillPath(path, QColor(m_data->shadow.m_color));
        p->translate(-m_data->shadow.offset());
    }
    if (m_common->state.fillPattern) {
        AffineTransform affine;
        p->fillPath(path, QBrush(m_common->state.fillPattern->createPlatformPattern(affine)));
    } else if (m_common->state.fillGradient) {
        QBrush brush(*m_common->state.fillGradient->platformGradient());
        brush.setTransform(m_common->state.fillGradient->gradientSpaceTransform());
        p->fillPath(path, brush);
    } else
        p->fillPath(path, p->brush());

    m_data->currentPath = QPainterPath();
}

void GraphicsContext::strokePath()
{
    if (paintingDisabled())
        return;

    QPainter* p = m_data->p();
    QPen pen(p->pen());
    QPainterPath& path = m_data->currentPath; // Avoid detaching the QPainterPath
    path.setFillRule(toQtFillRule(fillRule()));

    if (m_data->hasShadow()) {
        p->translate(m_data->shadow.offset());
        QPen shadowPen(pen);
        shadowPen.setColor(m_data->shadow.m_color);
        p->strokePath(path, shadowPen);
        p->translate(-m_data->shadow.offset());
    }
    if (m_common->state.strokePattern) {
        AffineTransform affine;
        pen.setBrush(QBrush(m_common->state.strokePattern->createPlatformPattern(affine)));
        p->setPen(pen);
        p->strokePath(path, pen);
    } else if (m_common->state.strokeGradient) {
        QBrush brush(*m_common->state.strokeGradient->platformGradient());
        brush.setTransform(m_common->state.strokeGradient->gradientSpaceTransform());
        pen.setBrush(brush);
        p->setPen(pen);
        p->strokePath(path, pen);
    } else
        p->strokePath(path, pen);
    m_data->currentPath = QPainterPath();
}

static inline void drawRepeatPattern(QPainter* p, QPixmap* image, const FloatRect& rect, const bool repeatX, const bool repeatY)
{
    // Patterns must be painted so that the top left of the first image is anchored at
    // the origin of the coordinate space
    if (image) {
        int w = image->width();
        int h = image->height();
        int startX, startY;
        QRect r(static_cast<int>(rect.x()), static_cast<int>(rect.y()), static_cast<int>(rect.width()), static_cast<int>(rect.height()));

        // startX, startY is the coordinate of the first image we need to put on the left-top of the rect
        if (repeatX && repeatY) {
            // repeat
            // startX, startY is at the left top side of the left-top of the rect
            startX = r.x() >=0 ? r.x() - (r.x() % w) : r.x() - (w - qAbs(r.x()) % w);
            startY = r.y() >=0 ? r.y() - (r.y() % h) : r.y() - (h - qAbs(r.y()) % h);
        } else {
           if (!repeatX && !repeatY) {
               // no-repeat
               // only draw the image once at orgin once, check if need to draw
               QRect imageRect(0, 0, w, h);
               if (imageRect.intersects(r)) {
                   startX = 0;
                   startY = 0;
               } else
                   return;   
           } else if (repeatX && !repeatY) {
               // repeat-x
               // startY is fixed, but startX change based on the left-top of the rect
               QRect imageRect(r.x(), 0, r.width(), h);
               if (imageRect.intersects(r)) {
                   startX = r.x() >=0 ? r.x() - (r.x() % w) : r.x() - (w - qAbs(r.x()) % w);
                   startY = 0;
               } else
                   return;
           } else {
               // repeat-y
               // startX is fixed, but startY change based on the left-top of the rect
               QRect imageRect(0, r.y(), w, r.height());
               if (imageRect.intersects(r)) {
                   startX = 0;
                   startY = r.y() >=0 ? r.y() - (r.y() % h) : r.y() - (h - qAbs(r.y()) % h);
               } else
                   return;
           }
        }

        int x = startX;
        int y = startY; 
        do {
            // repeat Y
            do {
                // repeat X
                QRect   imageRect(x, y, w, h);
                QRect   intersectRect = imageRect.intersected(r);
                QPoint  destStart(intersectRect.x(), intersectRect.y());
                QRect   sourceRect(intersectRect.x() - imageRect.x(), intersectRect.y() - imageRect.y(), intersectRect.width(), intersectRect.height());

                p->drawPixmap(destStart, *image, sourceRect);
                x += w;
            } while (repeatX && x < r.x() + r.width());
            x = startX;
            y += h;
        } while (repeatY && y < r.y() + r.height());
    }
}

void GraphicsContext::fillRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    QPainter* p = m_data->p();
    QRectF normalizedRect = rect.normalized();
    ContextShadow* shadow = contextShadow();

    if (m_common->state.fillPattern) {
        AffineTransform affine;
        QBrush brush(m_common->state.fillPattern->createPlatformPattern(affine));
        QPixmap* image = m_common->state.fillPattern->tileImage()->nativeImageForCurrentFrame();
        QPainter* shadowPainter = m_data->hasShadow() ? shadow->beginShadowLayer(p, normalizedRect) : 0;
        if (shadowPainter) {
            drawRepeatPattern(shadowPainter, image, normalizedRect, m_common->state.fillPattern->repeatX(), m_common->state.fillPattern->repeatY());
            shadowPainter->setCompositionMode(QPainter::CompositionMode_SourceIn);
            shadowPainter->fillRect(normalizedRect, shadow->m_color);
            shadow->endShadowLayer(p);
        }
        drawRepeatPattern(p, image, normalizedRect, m_common->state.fillPattern->repeatX(), m_common->state.fillPattern->repeatY());
    } else if (m_common->state.fillGradient) {
        QBrush brush(*m_common->state.fillGradient->platformGradient());
        brush.setTransform(m_common->state.fillGradient->gradientSpaceTransform());
        QPainter* shadowPainter = m_data->hasShadow() ? shadow->beginShadowLayer(p, normalizedRect) : 0;
        if (shadowPainter) {
            shadowPainter->fillRect(normalizedRect, brush);
            shadowPainter->setCompositionMode(QPainter::CompositionMode_SourceIn);
            shadowPainter->fillRect(normalizedRect, shadow->m_color);
            shadow->endShadowLayer(p);
        }
        p->fillRect(normalizedRect, brush);
    } else {
        if (m_data->hasShadow()) {
            if (shadow->m_type == ContextShadow::BlurShadow) {
                QPainter* shadowPainter = shadow->beginShadowLayer(p, normalizedRect);
                if (shadowPainter) {
                    shadowPainter->fillRect(normalizedRect, p->brush());
                    shadow->endShadowLayer(p);
                }
            } else {
                // Solid rectangle fill with no blur shadow can be done faster
                // without using the shadow layer at all.
                QColor shadowColor = shadow->m_color;
                shadowColor.setAlphaF(shadowColor.alphaF() * p->brush().color().alphaF());
                p->fillRect(normalizedRect.translated(shadow->offset()), shadowColor);
            }
        }
        p->fillRect(normalizedRect, p->brush());
    }
}


void GraphicsContext::fillRect(const FloatRect& rect, const Color& color, ColorSpace colorSpace)
{
    if (paintingDisabled() || !color.isValid())
        return;

    m_data->solidColor.setColor(color);
    QPainter* p = m_data->p();
    QRectF normalizedRect = rect.normalized();

    if (m_data->hasShadow()) {
        ContextShadow* shadow = contextShadow();

        if (shadow->m_type != ContextShadow::BlurShadow) {
            // We do not need any layer for simple shadow.
            p->fillRect(normalizedRect.translated(shadow->offset()), shadow->m_color);
        } else {
            QPainter* shadowPainter = shadow->beginShadowLayer(p, normalizedRect);
            if (shadowPainter) {
                shadowPainter->setCompositionMode(QPainter::CompositionMode_Source);
                shadowPainter->fillRect(normalizedRect, shadow->m_color);
                shadow->endShadowLayer(p);
            }
        }
    }

    p->fillRect(normalizedRect, m_data->solidColor);
}

void GraphicsContext::fillRoundedRect(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight, const Color& color, ColorSpace colorSpace)
{
    if (paintingDisabled() || !color.isValid())
        return;

    Path path = Path::createRoundedRectangle(rect, topLeft, topRight, bottomLeft, bottomRight);
    QPainter* p = m_data->p();
    if (m_data->hasShadow()) {
        p->translate(m_data->shadow.offset());
        p->fillPath(path.platformPath(), QColor(m_data->shadow.m_color));
        p->translate(-m_data->shadow.offset());
    }
    p->fillPath(path.platformPath(), QColor(color));
}

void GraphicsContext::beginPath()
{
    m_data->currentPath = QPainterPath();
}

void GraphicsContext::addPath(const Path& path)
{
    if (!m_data->currentPath.elementCount()) {
        m_data->currentPath = path.platformPath();
        return;
    }
    m_data->currentPath.addPath(path.platformPath());
}

bool GraphicsContext::inTransparencyLayer() const
{
    return m_data->layerCount;
}

PlatformPath* GraphicsContext::currentPath()
{
    return &m_data->currentPath;
}

ContextShadow* GraphicsContext::contextShadow()
{
    return &m_data->shadow;
}

void GraphicsContext::clip(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    m_data->p()->setClipRect(rect, Qt::IntersectClip);
}

void GraphicsContext::clipPath(WindRule clipRule)
{
    if (paintingDisabled())
        return;

    QPainter* p = m_data->p();
    QPainterPath newPath = m_data->currentPath;
    newPath.setFillRule(clipRule == RULE_EVENODD ? Qt::OddEvenFill : Qt::WindingFill);
    p->setClipPath(newPath);
}

void GraphicsContext::drawFocusRing(const Vector<Path>& paths, int width, int offset, const Color& color)
{
    // FIXME: implement
}

/**
 * Focus ring handling is not handled here. Qt style in 
 * RenderTheme handles drawing focus on widgets which 
 * need it.
 */
void GraphicsContext::drawFocusRing(const Vector<IntRect>& rects, int /* width */, int /* offset */, const Color& color)
{
    if (paintingDisabled() || !color.isValid())
        return;

    unsigned rectCount = rects.size();

    if (!rects.size())
        return;

    QPainter* p = m_data->p();
    const bool antiAlias = p->testRenderHint(QPainter::Antialiasing);
    p->setRenderHint(QPainter::Antialiasing, m_data->antiAliasingForRectsAndLines);

    const QPen oldPen = p->pen();
    const QBrush oldBrush = p->brush();

    QPen nPen = p->pen();
    nPen.setColor(color);
    p->setBrush(Qt::NoBrush);
    nPen.setStyle(Qt::DotLine);
    p->setPen(nPen);
#if 0
    // FIXME How do we do a bounding outline with Qt?
    QPainterPath path;
    for (int i = 0; i < rectCount; ++i)
        path.addRect(QRectF(rects[i]));
    QPainterPathStroker stroker;
    QPainterPath newPath = stroker.createStroke(path);
    p->strokePath(newPath, nPen);
#else
    for (unsigned i = 0; i < rectCount; ++i)
        p->drawRect(QRectF(rects[i]));
#endif
    p->setPen(oldPen);
    p->setBrush(oldBrush);

    p->setRenderHint(QPainter::Antialiasing, antiAlias);
}

void GraphicsContext::drawLineForText(const IntPoint& origin, int width, bool)
{
    if (paintingDisabled())
        return;

    IntPoint endPoint = origin + IntSize(width, 0);
    drawLine(origin, endPoint);
}

void GraphicsContext::drawLineForTextChecking(const IntPoint&, int, TextCheckingLineStyle)
{
    if (paintingDisabled())
        return;

    notImplemented();
}

FloatRect GraphicsContext::roundToDevicePixels(const FloatRect& frect)
{
    // It is not enough just to round to pixels in device space. The rotation part of the
    // affine transform matrix to device space can mess with this conversion if we have a
    // rotating image like the hands of the world clock widget. We just need the scale, so
    // we get the affine transform matrix and extract the scale.
    QPainter* painter = platformContext();
    QTransform deviceTransform = painter->deviceTransform();
    if (deviceTransform.isIdentity())
        return frect;

    qreal deviceScaleX = sqrtf(deviceTransform.m11() * deviceTransform.m11() + deviceTransform.m12() * deviceTransform.m12());
    qreal deviceScaleY = sqrtf(deviceTransform.m21() * deviceTransform.m21() + deviceTransform.m22() * deviceTransform.m22());

    QPoint deviceOrigin(frect.x() * deviceScaleX, frect.y() * deviceScaleY);
    QPoint deviceLowerRight(frect.right() * deviceScaleX, frect.bottom() * deviceScaleY);

    // Don't let the height or width round to 0 unless either was originally 0
    if (deviceOrigin.y() == deviceLowerRight.y() && frect.height())
        deviceLowerRight.setY(deviceLowerRight.y() + 1);
    if (deviceOrigin.x() == deviceLowerRight.x() && frect.width())
        deviceLowerRight.setX(deviceLowerRight.x() + 1);

    FloatPoint roundedOrigin = FloatPoint(deviceOrigin.x() / deviceScaleX, deviceOrigin.y() / deviceScaleY);
    FloatPoint roundedLowerRight = FloatPoint(deviceLowerRight.x() / deviceScaleX, deviceLowerRight.y() / deviceScaleY);
    return FloatRect(roundedOrigin, roundedLowerRight - roundedOrigin);
}

void GraphicsContext::setPlatformShadow(const FloatSize& size, float blur, const Color& color, ColorSpace)
{
    // Qt doesn't support shadows natively, they are drawn manually in the draw*
    // functions

    if (m_common->state.shadowsIgnoreTransforms) {
        // Meaning that this graphics context is associated with a CanvasRenderingContext
        // We flip the height since CG and HTML5 Canvas have opposite Y axis
        m_common->state.shadowOffset = FloatSize(size.width(), -size.height());
        m_data->shadow = ContextShadow(color, blur, FloatSize(size.width(), -size.height()));
    } else {
        m_data->shadow = ContextShadow(color, blur, FloatSize(size.width(), size.height()));
    }
}

void GraphicsContext::clearPlatformShadow()
{
    m_data->shadow.clear();
}

void GraphicsContext::pushTransparencyLayerInternal(const QRect &rect, qreal opacity, QPixmap& alphaMask)
{
   m_data->layers.push(new TransparencyLayer(m_data->p(), m_data->p()->transform().mapRect(rect), 1.0, alphaMask));
}

void GraphicsContext::beginTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;

    int x, y, w, h;
    x = y = 0;
    QPainter* p = m_data->p();
    const QPaintDevice* device = p->device();
    w = device->width();
    h = device->height();

    QRectF clip = m_data->clipBoundingRect();
    QRectF deviceClip = p->transform().mapRect(clip);
    x = int(qBound(qreal(0), deviceClip.x(), (qreal)w));
    y = int(qBound(qreal(0), deviceClip.y(), (qreal)h));
    w = int(qBound(qreal(0), deviceClip.width(), (qreal)w) + 2);
    h = int(qBound(qreal(0), deviceClip.height(), (qreal)h) + 2);

    QPixmap emptyAlphaMask;
    m_data->layers.push(new TransparencyLayer(m_data->p(), QRect(x, y, w, h), opacity, emptyAlphaMask));
    ++m_data->layerCount;
}

void GraphicsContext::endTransparencyLayer()
{
    if (paintingDisabled())
        return;

    TransparencyLayer* layer = m_data->layers.pop();
    if (!layer->alphaMask.isNull()) {
        layer->painter.resetTransform();
        layer->painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        layer->painter.drawPixmap(QPoint(), layer->alphaMask);
    } else
        --m_data->layerCount; // see the comment for layerCount
    layer->painter.end();

    QPainter* p = m_data->p();
    p->save();
    p->resetTransform();
    p->setOpacity(layer->opacity);
    p->drawPixmap(layer->offset, layer->pixmap);
    p->restore();

    delete layer;
}

void GraphicsContext::clearRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    QPainter* p = m_data->p();
    QPainter::CompositionMode currentCompositionMode = p->compositionMode();
    if (p->paintEngine()->hasFeature(QPaintEngine::PorterDuff))
        p->setCompositionMode(QPainter::CompositionMode_Source);
    p->fillRect(rect, Qt::transparent);
    if (p->paintEngine()->hasFeature(QPaintEngine::PorterDuff))
        p->setCompositionMode(currentCompositionMode);
}

void GraphicsContext::strokeRect(const FloatRect& rect, float width)
{
    if (paintingDisabled())
        return;

    QPainterPath path;
    path.addRect(rect);
    setStrokeThickness(width);
    m_data->currentPath = path;

    strokePath();
}

void GraphicsContext::setLineCap(LineCap lc)
{
    if (paintingDisabled())
        return;

    QPainter* p = m_data->p();
    QPen nPen = p->pen();
    nPen.setCapStyle(toQtLineCap(lc));
    p->setPen(nPen);
}

void GraphicsContext::setLineDash(const DashArray& dashes, float dashOffset)
{
    QPainter* p = m_data->p();
    QPen pen = p->pen();
    unsigned dashLength = dashes.size();
    if (dashLength) {
        QVector<qreal> pattern;
        unsigned count = dashLength;
        if (dashLength % 2)
            count *= 2;

        float penWidth = narrowPrecisionToFloat(double(pen.widthF()));
        for (unsigned i = 0; i < count; i++)
            pattern.append(dashes[i % dashLength] / penWidth);

        pen.setDashPattern(pattern);
        pen.setDashOffset(dashOffset / penWidth);
    } else
        pen.setStyle(Qt::SolidLine);
    p->setPen(pen);
}

void GraphicsContext::setLineJoin(LineJoin lj)
{
    if (paintingDisabled())
        return;

    QPainter* p = m_data->p();
    QPen nPen = p->pen();
    nPen.setJoinStyle(toQtLineJoin(lj));
    p->setPen(nPen);
}

void GraphicsContext::setMiterLimit(float limit)
{
    if (paintingDisabled())
        return;

    QPainter* p = m_data->p();
    QPen nPen = p->pen();
    nPen.setMiterLimit(limit);
    p->setPen(nPen);
}

void GraphicsContext::setAlpha(float opacity)
{
    if (paintingDisabled())
        return;
    QPainter* p = m_data->p();
    p->setOpacity(opacity);
}

void GraphicsContext::setCompositeOperation(CompositeOperator op)
{
    if (paintingDisabled())
        return;

    if (m_data->p()->paintEngine()->hasFeature(QPaintEngine::PorterDuff))
        m_data->p()->setCompositionMode(toQtCompositionMode(op));
}

void GraphicsContext::clip(const Path& path)
{
    if (paintingDisabled())
        return;

    m_data->p()->setClipPath(path.platformPath(), Qt::IntersectClip);
}

void GraphicsContext::canvasClip(const Path& path)
{
    if (paintingDisabled())
        return;

    QPainterPath clipPath = path.platformPath();
    clipPath.setFillRule(Qt::WindingFill);
    m_data->p()->setClipPath(clipPath, Qt::IntersectClip);
}

void GraphicsContext::clipOut(const Path& path)
{
    if (paintingDisabled())
        return;

    QPainter* p = m_data->p();
    QPainterPath clippedOut = path.platformPath();
    QPainterPath newClip;
    newClip.setFillRule(Qt::OddEvenFill);
    if (p->hasClipping()) {
        newClip.addRect(m_data->clipBoundingRect());
        newClip.addPath(clippedOut);
        p->setClipPath(newClip, Qt::IntersectClip);
    } else {
        QRect windowRect = p->transform().inverted().mapRect(p->window());
        newClip.addRect(windowRect);
        newClip.addPath(clippedOut.intersected(newClip));
        p->setClipPath(newClip);
    }
}

void GraphicsContext::translate(float x, float y)
{
    if (paintingDisabled())
        return;

    m_data->p()->translate(x, y);

    if (!m_data->currentPath.isEmpty()) {
        QTransform matrix;
        m_data->currentPath = m_data->currentPath * matrix.translate(-x, -y);
        m_common->state.pathTransform.translate(x, y);
    }
}

IntPoint GraphicsContext::origin()
{
    if (paintingDisabled())
        return IntPoint();
    const QTransform &transform = m_data->p()->transform();
    return IntPoint(qRound(transform.dx()), qRound(transform.dy()));
}

void GraphicsContext::rotate(float radians)
{
    if (paintingDisabled())
        return;

    m_data->p()->rotate(180 / M_PI*radians);

    if (!m_data->currentPath.isEmpty()) {
        QTransform matrix;
        m_data->currentPath = m_data->currentPath * matrix.rotate(-180 / M_PI*radians);
        m_common->state.pathTransform.rotate(radians);
    }
}

void GraphicsContext::scale(const FloatSize& s)
{
    if (paintingDisabled())
        return;

    m_data->p()->scale(s.width(), s.height());

    if (!m_data->currentPath.isEmpty()) {
        QTransform matrix;
        m_data->currentPath = m_data->currentPath * matrix.scale(1 / s.width(), 1 / s.height());
        m_common->state.pathTransform.scaleNonUniform(s.width(), s.height());
    }
}

void GraphicsContext::clipOut(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    QPainter* p = m_data->p();
    QPainterPath newClip;
    newClip.setFillRule(Qt::OddEvenFill);
    if (p->hasClipping()) {
        newClip.addRect(m_data->clipBoundingRect());
        newClip.addRect(QRect(rect));
        p->setClipPath(newClip, Qt::IntersectClip);
    } else {
        QRect clipOutRect(rect);
        QRect window = p->transform().inverted().mapRect(p->window());
        clipOutRect &= window;
        newClip.addRect(window);
        newClip.addRect(clipOutRect);
        p->setClipPath(newClip);
    }
}

void GraphicsContext::clipOutEllipseInRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    QPainter* p = m_data->p();
    QPainterPath newClip;
    newClip.setFillRule(Qt::OddEvenFill);
    if (p->hasClipping()) {
        newClip.addRect(m_data->clipBoundingRect());
        newClip.addEllipse(QRect(rect));
        p->setClipPath(newClip, Qt::IntersectClip);
    } else {
        QRect clipOutRect(rect);
        QRect window(p->window());
        clipOutRect &= window;
        newClip.addRect(window);
        newClip.addEllipse(clipOutRect);
        p->setClipPath(newClip);
    }
}

void GraphicsContext::addInnerRoundedRectClip(const IntRect& rect,
                                              int thickness)
{
    if (paintingDisabled())
        return;

    clip(rect);
    QPainterPath path;

    // Add outer ellipse
    path.addEllipse(QRectF(rect.x(), rect.y(), rect.width(), rect.height()));

    // Add inner ellipse.
    path.addEllipse(QRectF(rect.x() + thickness, rect.y() + thickness,
                           rect.width() - (thickness * 2), rect.height() - (thickness * 2)));

    path.setFillRule(Qt::OddEvenFill);

    QPainter* p = m_data->p();

    const bool antiAlias = p->testRenderHint(QPainter::Antialiasing);
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setClipPath(path, Qt::IntersectClip);
    p->setRenderHint(QPainter::Antialiasing, antiAlias);
}

void GraphicsContext::concatCTM(const AffineTransform& transform)
{
    if (paintingDisabled())
        return;

    m_data->p()->setWorldTransform(transform, true);

    // Transformations to the context shouldn't transform the currentPath.
    // We have to undo every change made to the context from the currentPath
    // to avoid wrong drawings.
    if (!m_data->currentPath.isEmpty() && transform.isInvertible()) {
        QTransform matrix = transform.inverse();
        m_data->currentPath = m_data->currentPath * matrix;
        m_common->state.pathTransform.multiply(transform.toTransformationMatrix());
    }
}

void GraphicsContext::setURLForRect(const KURL&, const IntRect&)
{
    notImplemented();
}

void GraphicsContext::setPlatformStrokeColor(const Color& color, ColorSpace colorSpace)
{
    if (paintingDisabled() || !color.isValid())
        return;

    QPainter* p = m_data->p();
    QPen newPen(p->pen());
    m_data->solidColor.setColor(color);
    newPen.setBrush(m_data->solidColor);
    p->setPen(newPen);
}

void GraphicsContext::setPlatformStrokeStyle(const StrokeStyle& strokeStyle)
{
    if (paintingDisabled())
        return;
    QPainter* p = m_data->p();
    QPen newPen(p->pen());
    newPen.setStyle(toQPenStyle(strokeStyle));
    p->setPen(newPen);
}

void GraphicsContext::setPlatformStrokeThickness(float thickness)
{
    if (paintingDisabled())
        return;
    QPainter* p = m_data->p();
    QPen newPen(p->pen());
    newPen.setWidthF(thickness);
    p->setPen(newPen);
}

void GraphicsContext::setPlatformFillColor(const Color& color, ColorSpace colorSpace)
{
    if (paintingDisabled() || !color.isValid())
        return;

    m_data->solidColor.setColor(color);
    m_data->p()->setBrush(m_data->solidColor);
}

void GraphicsContext::setPlatformShouldAntialias(bool enable)
{
    if (paintingDisabled())
        return;
    m_data->p()->setRenderHint(QPainter::Antialiasing, enable);
}

#ifdef Q_WS_WIN

HDC GraphicsContext::getWindowsContext(const IntRect& dstRect, bool supportAlphaBlend, bool mayCreateBitmap)
{
    // painting through native HDC is only supported for plugin, where mayCreateBitmap is always true
    Q_ASSERT(mayCreateBitmap);

    if (dstRect.isEmpty())
        return 0;

    // Create a bitmap DC in which to draw.
    BITMAPINFO bitmapInfo;
    bitmapInfo.bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth         = dstRect.width();
    bitmapInfo.bmiHeader.biHeight        = dstRect.height();
    bitmapInfo.bmiHeader.biPlanes        = 1;
    bitmapInfo.bmiHeader.biBitCount      = 32;
    bitmapInfo.bmiHeader.biCompression   = BI_RGB;
    bitmapInfo.bmiHeader.biSizeImage     = 0;
    bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
    bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
    bitmapInfo.bmiHeader.biClrUsed       = 0;
    bitmapInfo.bmiHeader.biClrImportant  = 0;

    void* pixels = 0;
    HBITMAP bitmap = ::CreateDIBSection(0, &bitmapInfo, DIB_RGB_COLORS, &pixels, 0, 0);
    if (!bitmap)
        return 0;

    HDC displayDC = ::GetDC(0);
    HDC bitmapDC = ::CreateCompatibleDC(displayDC);
    ::ReleaseDC(0, displayDC);

    ::SelectObject(bitmapDC, bitmap);

    // Fill our buffer with clear if we're going to alpha blend.
    if (supportAlphaBlend) {
        BITMAP bmpInfo;
        GetObject(bitmap, sizeof(bmpInfo), &bmpInfo);
        int bufferSize = bmpInfo.bmWidthBytes * bmpInfo.bmHeight;
        memset(bmpInfo.bmBits, 0, bufferSize);
    }

#if !OS(WINCE)
    // Make sure we can do world transforms.
    SetGraphicsMode(bitmapDC, GM_ADVANCED);

    // Apply a translation to our context so that the drawing done will be at (0,0) of the bitmap.
    XFORM xform;
    xform.eM11 = 1.0f;
    xform.eM12 = 0.0f;
    xform.eM21 = 0.0f;
    xform.eM22 = 1.0f;
    xform.eDx = -dstRect.x();
    xform.eDy = -dstRect.y();
    ::SetWorldTransform(bitmapDC, &xform);
#endif

    return bitmapDC;
}

void GraphicsContext::releaseWindowsContext(HDC hdc, const IntRect& dstRect, bool supportAlphaBlend, bool mayCreateBitmap)
{
    // painting through native HDC is only supported for plugin, where mayCreateBitmap is always true
    Q_ASSERT(mayCreateBitmap);

    if (hdc) {

        if (!dstRect.isEmpty()) {

            HBITMAP bitmap = static_cast<HBITMAP>(GetCurrentObject(hdc, OBJ_BITMAP));
            BITMAP info;
            GetObject(bitmap, sizeof(info), &info);
            ASSERT(info.bmBitsPixel == 32);

            QPixmap pixmap = QPixmap::fromWinHBITMAP(bitmap, supportAlphaBlend ? QPixmap::PremultipliedAlpha : QPixmap::NoAlpha);
            m_data->p()->drawPixmap(dstRect, pixmap);

            ::DeleteObject(bitmap);
        }

        ::DeleteDC(hdc);
    }
}
#endif

void GraphicsContext::setImageInterpolationQuality(InterpolationQuality quality)
{
    m_data->imageInterpolationQuality = quality;

    switch (quality) {
    case InterpolationNone:
    case InterpolationLow:
        // use nearest-neigbor
        m_data->p()->setRenderHint(QPainter::SmoothPixmapTransform, false);
        break;

    case InterpolationDefault:
    case InterpolationMedium:
    case InterpolationHigh:
    default:
        // use the filter
        m_data->p()->setRenderHint(QPainter::SmoothPixmapTransform, true);
        break;
    };
}

InterpolationQuality GraphicsContext::imageInterpolationQuality() const
{
    return m_data->imageInterpolationQuality;
}

}

// vim: ts=4 sw=4 et
