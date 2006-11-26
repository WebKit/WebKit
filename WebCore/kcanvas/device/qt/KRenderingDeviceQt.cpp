/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; see the file COPYING.LIB. If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#include "IntRect.h"
#include "RenderPath.h"
#include "AffineTransform.h"
#include "GraphicsContext.h"
#include "SVGResourceClipper.h"
#include "SVGResourceMarker.h"
#include "KRenderingDeviceQt.h"
#include "SVGPaintServerSolid.h"
#include "SVGPaintServerLinearGradient.h"
#include "SVGPaintServerRadialGradient.h"
#include "SVGPaintServerPattern.h"

namespace WebCore {

KRenderingDeviceContextQt::KRenderingDeviceContextQt(QPainter* painter)
    : m_painter(painter)
    , m_path()
{
    Q_ASSERT(m_painter != 0);
}

KRenderingDeviceContextQt::~KRenderingDeviceContextQt()
{
}

AffineTransform KRenderingDeviceContextQt::concatCTM(const AffineTransform& worldMatrix)
{
    AffineTransform ret = ctm();
    m_painter->setMatrix(worldMatrix, true);
    return ret;
}

AffineTransform KRenderingDeviceContextQt::ctm() const
{
    return AffineTransform(m_painter->matrix());
}

IntRect KRenderingDeviceContextQt::mapFromVisual(const IntRect& rect)
{
    return IntRect();
}

IntRect KRenderingDeviceContextQt::mapToVisual(const IntRect& rect)
{
    return IntRect();
}

void KRenderingDeviceContextQt::clearPath()
{
    m_path = QPainterPath();
}

void KRenderingDeviceContextQt::addPath(const Path& path)
{
    m_path.addPath(*(path.platformPath()));
}

GraphicsContext* KRenderingDeviceContextQt::createGraphicsContext()
{
    return new GraphicsContext(m_painter);
}

QPainter& KRenderingDeviceContextQt::painter()
{
    return *m_painter;
}

QRectF KRenderingDeviceContextQt::pathBBox() const
{
    return m_path.boundingRect();
}

void KRenderingDeviceContextQt::setFillRule(WindRule rule)
{
    m_path.setFillRule(rule == RULE_EVENODD ? Qt::OddEvenFill : Qt::WindingFill);
}

void KRenderingDeviceContextQt::fillPath()
{
    m_painter->fillPath(m_path, m_painter->brush());
}

void KRenderingDeviceContextQt::strokePath()
{
    m_painter->strokePath(m_path, m_painter->pen());
}

// KRenderingDeviceQt
KRenderingDeviceQt::KRenderingDeviceQt() : KRenderingDevice()
{
}

KRenderingDeviceQt::~KRenderingDeviceQt()
{
}

KRenderingDeviceContext* KRenderingDeviceQt::popContext()
{
    // Any special things needed?
    return KRenderingDevice::popContext();
}

void KRenderingDeviceQt::pushContext(KRenderingDeviceContext* context)
{
    // Any special things needed?
    KRenderingDevice::pushContext(context);
}

// context management.
KRenderingDeviceContextQt* KRenderingDeviceQt::qtContext() const
{
    return static_cast<KRenderingDeviceContextQt*>(currentContext());
}

KRenderingDeviceContext* KRenderingDeviceQt::contextForImage(SVGResourceImage* image) const
{
    qDebug("KRenderingDeviceQt::contextForImage() TODO!");
    return 0;
}

// Resource creation
PassRefPtr<SVGResource> KRenderingDeviceQt::createResource(const SVGResourceType& type) const
{
    switch (type)
    {
        case ClipperResourceType:
            return new SVGResourceClipper();
        case MarkerResourceType:
            return new SVGResourceMarker(); // Use default implementation...
        case ImageResourceType:
            // return new SVGResourceImageQt();
        case FilterResourceType:
            // return new KCanvasFilterQt();
        case MaskerResourceType:
            // return new KCanvasMaskerQt();
        default:
            return 0;
    }
}

PassRefPtr<SVGPaintServer> KRenderingDeviceQt::createPaintServer(const SVGPaintServerType& type) const
{
    switch (type)
    {
        case SolidPaintServer:
            return new SVGPaintServerSolid();
        case PatternPaintServer:
            return new SVGPaintServerPattern();
        case LinearGradientPaintServer:
            return new SVGPaintServerLinearGradient();
        case RadialGradientPaintServer:
            return new SVGPaintServerRadialGradient();
        default:
            return 0;
    }
}

SVGFilterEffect* KRenderingDeviceQt::createFilterEffect(const SVGFilterEffectType& type) const
{
    qDebug("KRenderingDeviceQt::createFilterEffect() TODO!");
    return 0;
}

KRenderingDevice* renderingDevice()
{
    static KRenderingDevice *sharedRenderingDevice = new KRenderingDeviceQt();
    return sharedRenderingDevice;
}

}

// vim:ts=4:noet
