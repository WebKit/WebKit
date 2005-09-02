/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KRenderingPaintServerGradient_H
#define KRenderingPaintServerGradient_H

#include <qcolor.h>
#include <q3ptrlist.h>

#include <kcanvas/KCanvasResources.h>
#include <kcanvas/device/KRenderingPaintServer.h>

typedef enum
{
    SPREADMETHOD_PAD = 1,
    SPREADMETHOD_REPEAT = 2,
    SPREADMETHOD_REFLECT = 4
} KCGradientSpreadMethod;

QTextStream &operator<<(QTextStream &ts, KCGradientSpreadMethod m);

struct KCGradientOffsetPair
{
    float offset;
    QColor color;
};

class KCSortedGradientStopList : public Q3PtrList<KCGradientOffsetPair>
{
public:
    KCSortedGradientStopList();
    void addStop(float offset, const QColor &color);

    typedef Q3PtrListIterator<KCGradientOffsetPair> Iterator;

    
protected:
    virtual int compareItems(Q3PtrCollection::Item item1, Q3PtrCollection::Item item2);
private:
    friend QTextStream &operator<<(QTextStream &, const KCSortedGradientStopList &);
};

QTextStream &operator<<(QTextStream &, const KCSortedGradientStopList &);

class KCanvasMatrix;
class KRenderingPaintServerGradient : public KRenderingPaintServer,
                                      public KCanvasResource
{
public:
    KRenderingPaintServerGradient();
    virtual ~KRenderingPaintServerGradient();

    // 'Gradient' interface
    KCSortedGradientStopList &gradientStops() const;
    void setGradientStops(const KCSortedGradientStopList &stops);

    KCGradientSpreadMethod spreadMethod() const;
    void setGradientSpreadMethod(const KCGradientSpreadMethod &method);

    // Gradient start and end points are percentages when used in boundingBox mode.
    // For instance start point with value (0,0) is top-left and end point with value (100, 100) is 
    // bottom-right.
    // BoundingBox mode is true by default.
    bool boundingBoxMode() const;
    void setBoundingBoxMode(bool mode = true);

    KCanvasMatrix gradientTransform() const;
    void setGradientTransform(const KCanvasMatrix &mat);

    QTextStream &externalRepresentation(QTextStream &) const;
private:
    class Private;
    Private *d;
};

class KRenderingPaintServerLinearGradient : public KRenderingPaintServerGradient
{
public:
    KRenderingPaintServerLinearGradient();
    virtual ~KRenderingPaintServerLinearGradient();

    virtual KCPaintServerType type() const;

    // 'Linear Gradient' interface
    QPoint gradientStart() const;
    void setGradientStart(const QPoint &start);

    QPoint gradientEnd() const;
    void setGradientEnd(const QPoint &end);

    QTextStream &externalRepresentation(QTextStream &) const;
private:
    class Private;
    Private *d;
};

class KRenderingPaintServerRadialGradient : public KRenderingPaintServerGradient
{
public:
    KRenderingPaintServerRadialGradient();
    virtual ~KRenderingPaintServerRadialGradient();

    virtual KCPaintServerType type() const;

    // 'Radial Gradient' interface
    QPoint gradientCenter() const;
    void setGradientCenter(const QPoint &center);

    QPoint gradientFocal() const;
    void setGradientFocal(const QPoint &focal);

    float gradientRadius() const;
    void setGradientRadius(float radius);

    QTextStream &externalRepresentation(QTextStream &) const;
private:
    class Private;
    Private *d;
};

#endif

// vim:ts=4:noet
