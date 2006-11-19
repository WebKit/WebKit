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

#ifndef KRenderingDeviceQt_H
#define KRenderingDeviceQt_H

#include <QPainter>
#include <QPainterPath>

#include "KRenderingDevice.h"

namespace WebCore {

class KRenderingDeviceContextQt : public KRenderingDeviceContext
{
public:
    KRenderingDeviceContextQt(QPainter*);
    virtual ~KRenderingDeviceContextQt();

    virtual AffineTransform concatCTM(const AffineTransform&);
    virtual AffineTransform ctm() const;

    virtual IntRect mapFromVisual(const IntRect&);
    virtual IntRect mapToVisual(const IntRect&);

    virtual void clearPath();
    virtual void addPath(const Path&);

    virtual GraphicsContext* createGraphicsContext();
    
    // Qt specific stuff
    QPainter& painter();
    QRectF pathBBox() const;

    void setFillRule(WindRule);

    void fillPath();
    void strokePath();

private:
    QPainter* m_painter;
    QPainterPath m_path;
};

class KRenderingDeviceQt : public KRenderingDevice
{
public:
    KRenderingDeviceQt();
    virtual ~KRenderingDeviceQt();

    virtual bool isBuffered() const { return false; }

    virtual KRenderingDeviceContext* popContext();
    virtual void pushContext(KRenderingDeviceContext*);

    // context management.
    KRenderingDeviceContextQt* qtContext() const;
    virtual KRenderingDeviceContext* contextForImage(SVGResourceImage*) const;

    // Resource creation
    virtual PassRefPtr<SVGResource> createResource(const SVGResourceType&) const;
    virtual PassRefPtr<KRenderingPaintServer> createPaintServer(const KCPaintServerType&) const;
    virtual SVGFilterEffect* createFilterEffect(const SVGFilterEffectType&) const;
};

}

#endif

// vim:ts=4:noet
