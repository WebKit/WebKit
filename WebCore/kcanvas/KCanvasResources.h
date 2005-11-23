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

#ifndef KCanvasResources_H
#define KCanvasResources_H

#include <qstring.h>
#include <q3valuelist.h>

#include <kcanvas/RenderPath.h>
#include <kcanvas/KCanvasPath.h>
#include <kcanvas/KCanvasResourceListener.h>

class QTextStream;

// Enumerations
typedef enum
{
    // Painting mode
    RS_CLIPPER = 0,
    RS_MARKER = 1,
    RS_IMAGE = 2,
    RS_FILTER = 3
} KCResourceType;

class KCanvasMatrix;
class KRenderingPaintServer;

class KCanvasResource
{
public:
    KCanvasResource();
    virtual ~KCanvasResource();

    virtual void invalidate();
    void addClient(RenderPath *item);

    const KCanvasItemList &clients() const;
    
    QString idInRegistry() const;
    void setIdInRegistry(const QString& newId);
    
    virtual bool isPaintServer() const { return false; }
    virtual bool isFilter() const { return false; }
    virtual bool isClipper() const { return false; }
    virtual bool isMarker() const { return false; }
    
    virtual QTextStream& externalRepresentation(QTextStream &) const; 
private:
    KCanvasItemList m_clients;
    QString registryId;
};

class KCanvasClipper : public KCanvasResource
{
public:
    KCanvasClipper();
    virtual ~KCanvasClipper();
    
    virtual bool isClipper() const { return true; }

    void resetClipData();
    void addClipData(const KCPathDataList &path, KCWindRule rule, bool bbox);

    KCClipDataList clipData() const;

    QTextStream& externalRepresentation(QTextStream &) const; 
protected:
    KCClipDataList m_clipData;
};

class KCanvasMarker : public KCanvasResource
{
public:
    KCanvasMarker(khtml::RenderObject *marker = 0);
    virtual ~KCanvasMarker();
    
    virtual bool isMarker() const { return true; }

    void setMarker(khtml::RenderObject *marker);
    
    void setRefX(double refX);
    double refX() const;
    
    void setRefY(double refY);
    double refY() const;
    
    void setAngle(float angle);
    void setAutoAngle();
    float angle() const;

    void setUseStrokeWidth(bool useStrokeWidth = true);
    bool useStrokeWidth() const;

    void setScaleX(float scaleX);
    float scaleX() const;

    void setScaleY(float scaleY);
    float scaleY() const;

     // Draw onto the canvas
    void draw(const QRect &rect, const KCanvasMatrix &objectMatrix, double x, double y, double strokeWidth = 1., double angle = 0.0);

    QTextStream& externalRepresentation(QTextStream &) const; 
private:
    double m_refX, m_refY;
    float m_angle, m_scaleX, m_scaleY;
    khtml::RenderObject *m_marker;
    bool m_useStrokeWidth;
};

QTextStream &operator<<(QTextStream &ts, const KCanvasResource &r);

KCanvasResource *getResourceById(KDOM::DocumentImpl *document, const KDOM::DOMString &id);
KCanvasMarker *getMarkerById(KDOM::DocumentImpl *document, const KDOM::DOMString &id);
KCanvasClipper *getClipperById(KDOM::DocumentImpl *document, const KDOM::DOMString &id);
KRenderingPaintServer *getPaintServerById(KDOM::DocumentImpl *document, const KDOM::DOMString &id);

#endif

// vim:ts=4:noet
