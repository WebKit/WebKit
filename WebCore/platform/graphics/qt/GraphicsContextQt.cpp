/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2006 Allan Sandfeld Jensen <sandfeld@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "AffineTransform.h"
#include "Path.h"
#include "Color.h"
#include "GraphicsContext.h"
#include "Font.h"
#include "Pen.h"
#include "NotImplemented.h"

#include <QStack>
#include <QPainter>
#include <QPolygonF>
#include <QPainterPath>
#include <QPaintDevice>
#include <QDebug>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace WebCore {

static inline QPainter::CompositionMode toQtCompositionMode(CompositeOperator op)
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
            return QPainter::CompositionMode_SourceOver;
        case CompositeHighlight:
            return QPainter::CompositionMode_SourceOver;
        case CompositePlusLighter:
            return QPainter::CompositionMode_SourceOver;
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

struct TransparencyLayer
{
    TransparencyLayer(const QPainter* p, const QRect &rect)
        : pixmap(rect.width(), rect.height())
    {
        offset = rect.topLeft();
        pixmap.fill(Qt::transparent);
        painter.begin(&pixmap);
        painter.translate(-offset);
        painter.setPen(p->pen());
        painter.setBrush(p->brush());
        painter.setTransform(p->transform(), true);
        painter.setOpacity(p->opacity());
        painter.setFont(p->font());
        painter.setCompositionMode(p->compositionMode());
        painter.setClipPath(p->clipPath());
    }

    TransparencyLayer()
    {
    }

    QPixmap pixmap;
    QPoint offset;
    QPainter painter;
    qreal opacity;
private:
    TransparencyLayer(const TransparencyLayer &) {}
    TransparencyLayer & operator=(const TransparencyLayer &) { return *this; }
};

struct TextShadow
{
    TextShadow()
        : x(0)
        , y(0)
        , blur(0)
    {
    }

    bool isNull() { return !x && !y && !blur; }

    int x;
    int y;
    int blur;

    Color color;
};

class GraphicsContextPlatformPrivate
{
public:
    GraphicsContextPlatformPrivate(QPainter* painter);
    ~GraphicsContextPlatformPrivate();

    inline QPainter* p()
    {
        if (layers.isEmpty()) {
            if (redirect)
                return redirect;

            return painter;
        } else
            return &layers.top()->painter;
    }

    QPaintDevice* device;

    QStack<TransparencyLayer *> layers;
    QPainter* redirect;

    TextShadow shadow;

    // Only used by SVG for now.
    QPainterPath currentPath;

private:
    QPainter* painter;
};


GraphicsContextPlatformPrivate::GraphicsContextPlatformPrivate(QPainter* p)
{
    painter = p;
    device = painter ? painter->device() : 0;
    redirect = 0;

    // FIXME: Maybe only enable in SVG mode?
    if (painter)
        painter->setRenderHint(QPainter::Antialiasing);
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
        setPlatformFillColor(fillColor());
        setPlatformStrokeColor(strokeColor());
    }
}

GraphicsContext::~GraphicsContext()
{
    while(!m_data->layers.isEmpty())
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
    return platformContext()->combinedMatrix();
}

void GraphicsContext::savePlatformState()
{
    m_data->p()->save();
}

void GraphicsContext::restorePlatformState()
{
    m_data->p()->restore();
}

/* FIXME: DISABLED WHILE MERGING BACK FROM UNITY
void GraphicsContext::drawTextShadow(const TextRun& run, const IntPoint& point, const FontStyle& style)
{
    if (paintingDisabled())
        return;

    if (m_data->shadow.isNull())
        return;

    TextShadow* shadow = &m_data->shadow;

    if (shadow->blur <= 0) {
        Pen p = pen();
        setPen(shadow->color);
        font().drawText(this, run, style, IntPoint(point.x() + shadow->x, point.y() + shadow->y));
        setPen(p);
    } else {
        const int thickness = shadow->blur;
        // FIXME: OPTIMIZE: limit the area to only the actually painted area + 2*thickness
        const int w = m_data->p()->device()->width();
        const int h = m_data->p()->device()->height();
        const QRgb color = qRgb(255, 255, 255);
        const QRgb bgColor = qRgb(0, 0, 0);
        QImage image(QSize(w, h), QImage::Format_ARGB32);
        image.fill(bgColor);
        QPainter p;

        Pen curPen = pen();
        p.begin(&image);
        setPen(color);
        m_data->redirect = &p;
        font().drawText(this, run, style, IntPoint(point.x() + shadow->x, point.y() + shadow->y));
        m_data->redirect = 0;
        p.end();
        setPen(curPen);

        int md = thickness * thickness; // max-dist^2

        // blur map/precalculated shadow-decay
        float* bmap = (float*) alloca(sizeof(float) * (md + 1));
        for (int n = 0; n <= md; n++) {
            float f;
            f = n / (float) (md + 1);
            f = 1.0 - f * f;
            bmap[n] = f;
        }

        float factor = 0.0; // maximal potential opacity-sum
        for (int n = -thickness; n <= thickness; n++) {
            for (int m = -thickness; m <= thickness; m++) {
                int d = n * n + m * m;
                if (d <= md)
                    factor += bmap[d];
            }
        }

        // alpha map
        float* amap = (float*) alloca(sizeof(float) * (h * w));
        memset(amap, 0, h * w * (sizeof(float)));

        for (int j = thickness; j<h-thickness; j++) {
            for (int i = thickness; i<w-thickness; i++) {
                QRgb col = image.pixel(i,j);
                if (col == bgColor)
                    continue;

                float g = qAlpha(col);
                g = g / 255;

                for (int n = -thickness; n <= thickness; n++) {
                    for (int m = -thickness; m <= thickness; m++) {
                        int d = n * n + m * m;
                        if (d > md)
                            continue;

                        float f = bmap[d];
                        amap[(i + m) + (j + n) * w] += (g * f);
                    }
                }
            }
        }

        QImage res(QSize(w,h),QImage::Format_ARGB32);
        int r = shadow->color.red();
        int g = shadow->color.green();
        int b = shadow->color.blue();
        int a1 = shadow->color.alpha();

        // arbitratry factor adjustment to make shadows more solid.
        factor = 1.333 / factor;

        for (int j = 0; j < h; j++) {
            for (int i = 0; i < w; i++) {
                int a = (int) (amap[i + j * w] * factor * a1);
                if (a > 255)
                    a = 255;

                res.setPixel(i,j, qRgba(r, g, b, a));
            }
        }

        m_data->p()->drawImage(0, 0, res, 0, 0, -1, -1, Qt::DiffuseAlphaDither | Qt::ColorOnly | Qt::PreferDither);
    }
}
*/

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    m_data->p()->drawRect(rect);
}

// FIXME: Now that this is refactored, it should be shared by all contexts.
static void adjustLineToPixelBoundaries(FloatPoint& p1, FloatPoint& p2, float strokeWidth,
                                        const StrokeStyle& penStyle)
{
    // For odd widths, we add in 0.5 to the appropriate x/y so that the float arithmetic
    // works out.  For example, with a border width of 3, KHTML will pass us (y1+y2)/2, e.g.,
    // (50+53)/2 = 103/2 = 51 when we want 51.5.  It is always true that an even width gave
    // us a perfect position, but an odd width gave us a position that is off by exactly 0.5.
    if (penStyle == DottedStroke || penStyle == DashedStroke) {
        if (p1.x() == p2.x()) {
            p1.setY(p1.y() + strokeWidth);
            p2.setY(p2.y() - strokeWidth);
        } else {
            p1.setX(p1.x() + strokeWidth);
            p2.setX(p2.x() - strokeWidth);
        }
    }

    if (((int) strokeWidth) % 2) {
        if (p1.x() == p2.x()) {
            // We're a vertical line.  Adjust our x.
            p1.setX(p1.x() + 0.5);
            p2.setX(p2.x() + 0.5);
        } else {
            // We're a horizontal line. Adjust our y.
            p1.setY(p1.y() + 0.5);
            p2.setY(p2.y() + 0.5);
        }
    }
}

// This is only used to draw borders.
void GraphicsContext::drawLine(const IntPoint& point1, const IntPoint& point2)
{
    if (paintingDisabled())
        return;

    FloatPoint p1 = point1;
    FloatPoint p2 = point2;

    adjustLineToPixelBoundaries(p1, p2, strokeThickness(), strokeStyle());
    m_data->p()->drawLine(p1, p2);
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
    if (paintingDisabled() || strokeStyle() == NoStroke || strokeThickness() <= 0.0f || !strokeColor().alpha())
        return;

    m_data->p()->drawArc(rect, startAngle * 16, angleSpan * 16);
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

    QPainter *p = m_data->p();
    p->save();
    p->setRenderHint(QPainter::Antialiasing, shouldAntialias);
    p->drawConvexPolygon(polygon);
    p->restore();
}

void GraphicsContext::fillRect(const IntRect& rect, const Color& c)
{
    if (paintingDisabled())
        return;

    m_data->p()->fillRect(rect, QColor(c));
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& c)
{
    if (paintingDisabled())
        return;

    m_data->p()->fillRect(rect, QColor(c));
}

void GraphicsContext::fillRoundedRect(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight, const Color& color)
{
    if (paintingDisabled() || !color.alpha())
        return;

    Path path = Path::createRoundedRectangle(rect, topLeft, topRight, bottomLeft, bottomRight);
    m_data->p()->fillPath(*path.platformPath(), QColor(color));
}

void GraphicsContext::beginPath()
{
    m_data->currentPath = QPainterPath();
}

void GraphicsContext::addPath(const Path& path)
{
    m_data->currentPath = *(path.platformPath());
}

void GraphicsContext::setFillRule(WindRule rule)
{
    m_data->currentPath.setFillRule(rule == RULE_EVENODD ? Qt::OddEvenFill : Qt::WindingFill);
}

PlatformPath* GraphicsContext::currentPath()
{
    return &m_data->currentPath;
}

void GraphicsContext::clip(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    QPainter *p = m_data->p();
    if (p->clipRegion().isEmpty())
        p->setClipRect(rect);
    else p->setClipRect(rect, Qt::IntersectClip);
}

/**
 * Focus ring handling is not handled here. Qt style in 
 * RenderTheme handles drawing focus on widgets which 
 * need it.
 */
void setFocusRingColorChangeFunction(void (*)()) { }
Color focusRingColor() { return Color(0, 0, 0); }
void GraphicsContext::drawFocusRing(const Color& color)
{
    if (paintingDisabled())
        return;

    const Vector<IntRect>& rects = focusRingRects();
    unsigned rectCount = rects.size();

    if (rects.size() == 0)
        return;

    QPainter *p = m_data->p();

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
    for (int i = 0; i < rectCount; ++i)
        p->drawRect(QRectF(rects[i]));
#endif
    p->setPen(oldPen);
    p->setBrush(oldBrush);
}

void GraphicsContext::drawLineForText(const IntPoint& origin, int width, bool printing)
{
    if (paintingDisabled())
        return;

    IntPoint endPoint = origin + IntSize(width, 0);
    drawLine(origin, endPoint);
}

void GraphicsContext::drawLineForMisspellingOrBadGrammar(const IntPoint&,
                                                         int width, bool grammar)
{
    if (paintingDisabled())
        return;

    notImplemented();
}

FloatRect GraphicsContext::roundToDevicePixels(const FloatRect& frect)
{
    QRectF rect(frect);
    rect = m_data->p()->deviceMatrix().mapRect(rect);

    QRect result = rect.toRect(); //round it
    return FloatRect(QRectF(result));
}

void GraphicsContext::setShadow(const IntSize& pos, int blur, const Color &color)
{
    if (paintingDisabled())
        return;

    m_data->shadow.x = pos.width();
    m_data->shadow.y = pos.height();
    m_data->shadow.blur = blur;
    m_data->shadow.color = color;
}

void GraphicsContext::clearShadow()
{
    if (paintingDisabled())
        return;

    m_data->shadow = TextShadow();
}

void GraphicsContext::beginTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;

    int x, y, w, h;
    x = y = 0;
    w = m_data->device->width();
    h = m_data->device->height();

    QPainter *p = m_data->p();
    QRectF clip = p->clipPath().boundingRect();
    bool ok;
    QTransform transform = p->transform().inverted(&ok);
    if (ok) {
        QRectF deviceClip = transform.mapRect(clip);
        x = int(qBound(qreal(0), deviceClip.x(), (qreal)w));
        y = int(qBound(qreal(0), deviceClip.y(), (qreal)h));
        w = int(qBound(qreal(0), deviceClip.width(), (qreal)w) + 2);
        h = int(qBound(qreal(0), deviceClip.height(), (qreal)h) + 2);
    }
    TransparencyLayer * layer = new TransparencyLayer(m_data->p(), QRect(x, y, w, h));

    layer->opacity = opacity;
    m_data->layers.push(layer);
}

void GraphicsContext::endTransparencyLayer()
{
    if (paintingDisabled())
        return;

    TransparencyLayer *layer = m_data->layers.pop();
    layer->painter.end();

    QPainter *p = m_data->p();
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

    QPainter *p = m_data->p();
    QPainter::CompositionMode currentCompositionMode = p->compositionMode();
    p->setCompositionMode(QPainter::CompositionMode_Source);
    p->eraseRect(rect);
    p->setCompositionMode(currentCompositionMode);
}

void GraphicsContext::strokeRect(const FloatRect& rect, float width)
{
    if (paintingDisabled())
        return;

    QPainter *p = m_data->p();
    QPainterPath path;
    path.addRect(rect);
    QPen nPen = p->pen();
    nPen.setWidthF(width);
    p->strokePath(path, nPen);
}

void GraphicsContext::setLineCap(LineCap lc)
{
    if (paintingDisabled())
        return;

    QPainter *p = m_data->p();
    QPen nPen = p->pen();
    nPen.setCapStyle(toQtLineCap(lc));
    p->setPen(nPen);
}

void GraphicsContext::setLineJoin(LineJoin lj)
{
    if (paintingDisabled())
        return;

    QPainter *p = m_data->p();
    QPen nPen = p->pen();
    nPen.setJoinStyle(toQtLineJoin(lj));
    p->setPen(nPen);
}

void GraphicsContext::setMiterLimit(float limit)
{
    if (paintingDisabled())
        return;

    QPainter *p = m_data->p();
    QPen nPen = p->pen();
    nPen.setMiterLimit(limit);
    p->setPen(nPen);
}

void GraphicsContext::setAlpha(float opacity)
{
    if (paintingDisabled())
        return;
    QPainter *p = m_data->p();
    p->setOpacity(opacity);
}

void GraphicsContext::setCompositeOperation(CompositeOperator op)
{
    if (paintingDisabled())
        return;

    m_data->p()->setCompositionMode(toQtCompositionMode(op));
}

void GraphicsContext::clip(const Path& path)
{
    if (paintingDisabled())
        return;

    m_data->p()->setClipPath(*path.platformPath(), Qt::IntersectClip);
}

void GraphicsContext::clipOut(const Path& path)
{
    if (paintingDisabled())
        return;

    QPainter *p = m_data->p();
    QRectF clipBounds = p->clipPath().boundingRect();
    QPainterPath clippedOut = *path.platformPath();
    QPainterPath newClip;
    newClip.setFillRule(Qt::OddEvenFill);
    newClip.addRect(clipBounds);
    newClip.addPath(clippedOut);

    p->setClipPath(newClip, Qt::IntersectClip);
}

void GraphicsContext::translate(float x, float y)
{
    if (paintingDisabled())
        return;

    m_data->p()->translate(x, y);
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

    m_data->p()->rotate(radians);
}

void GraphicsContext::scale(const FloatSize& s)
{
    if (paintingDisabled())
        return;

    m_data->p()->scale(s.width(), s.height());
}

void GraphicsContext::clipOut(const IntRect& rect)
{
    if (paintingDisabled())
        return;
        
    QPainter *p = m_data->p();
    QRectF clipBounds = p->clipPath().boundingRect();
    QPainterPath newClip;
    newClip.setFillRule(Qt::OddEvenFill);
    newClip.addRect(clipBounds);
    newClip.addRect(QRect(rect));

    p->setClipPath(newClip, Qt::IntersectClip);
}

void GraphicsContext::clipOutEllipseInRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;
    
    QPainter *p = m_data->p();
    QRectF clipBounds = p->clipPath().boundingRect();
    QPainterPath newClip;
    newClip.setFillRule(Qt::OddEvenFill);
    newClip.addRect(clipBounds);
    newClip.addEllipse(QRect(rect));

    p->setClipPath(newClip, Qt::IntersectClip);
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
    m_data->p()->setClipPath(path, Qt::IntersectClip);
}

void GraphicsContext::concatCTM(const AffineTransform& transform)
{
    if (paintingDisabled())
        return;

    m_data->p()->setMatrix(transform, true);
}

void GraphicsContext::setURLForRect(const KURL& link, const IntRect& destRect)
{
    notImplemented();
}

void GraphicsContext::setPlatformFont(const Font& aFont)
{
    if (paintingDisabled())
        return;
    m_data->p()->setFont(aFont.font());
}

void GraphicsContext::setPlatformStrokeColor(const Color& color)
{
    if (paintingDisabled())
        return;
    QPainter *p = m_data->p();
    QPen newPen(p->pen());
    newPen.setColor(color);
    p->setPen(newPen);
}

void GraphicsContext::setPlatformStrokeStyle(const StrokeStyle& strokeStyle)
{   
    if (paintingDisabled())
        return;
    QPainter *p = m_data->p();
    QPen newPen(p->pen());
    newPen.setStyle(toQPenStyle(strokeStyle));
    p->setPen(newPen);
}

void GraphicsContext::setPlatformStrokeThickness(float thickness)
{
    if (paintingDisabled())
        return;
    QPainter *p = m_data->p();
    QPen newPen(p->pen());
    newPen.setWidthF(thickness);
    p->setPen(newPen);
}

void GraphicsContext::setPlatformFillColor(const Color& color)
{
    if (paintingDisabled())
        return;
    m_data->p()->setBrush(QBrush(color));
}

void GraphicsContext::setUseAntialiasing(bool enable)
{
    if (paintingDisabled())
        return;
    m_data->p()->setRenderHint(QPainter::Antialiasing, enable);
}

}

// vim: ts=4 sw=4 et
