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

#include "config.h"
#include <qrect.h>
#include <kdebug.h>

#include "KCanvas.h"
#include "KCanvasItem.h"
#include "KCanvasMatrix.h"
#include "KCanvasContainer.h"
#include "KCanvasResources.h"
#include "KRenderingDevice.h"
#include "KCanvasResourceListener.h"

#include <qtextstream.h>
#include "KCanvasTreeDebug.h"

QTextStream &operator<<(QTextStream &ts, const KCanvasResource &r) 
{ 
    return r.externalRepresentation(ts); 
}

// KCanvasResource
KCanvasResource::KCanvasResource()
{
}

KCanvasResource::~KCanvasResource()
{
}

void KCanvasResource::addClient(KCanvasItem *item)
{
    if(m_clients.find(item) != m_clients.end())
        return;

    m_clients.append(item);
}

const KCanvasItemList &KCanvasResource::clients() const
{
    return m_clients;
}

void KCanvasResource::invalidate()
{
    KCanvasItemList::ConstIterator it = m_clients.begin();
    KCanvasItemList::ConstIterator end = m_clients.end();

    for(; it != end; ++it)
    {
        const KCanvasItem *current = (*it);

        Q_ASSERT(current->canvas());
        current->canvas()->invalidate(current);
    }
}

QString KCanvasResource::idInRegistry() const
{
    return registryId;
}

void KCanvasResource::setIdInRegistry(const QString& newId)
{
    registryId = newId;
} 

QTextStream& KCanvasResource::externalRepresentation(QTextStream &ts) const
{
    return ts;
}

// KCanvasClipper
KCanvasClipper::KCanvasClipper() : KCanvasResource()
{
    m_viewportMode = false;
}

KCanvasClipper::~KCanvasClipper()
{
}

bool KCanvasClipper::viewportClipper() const
{
    return m_viewportMode;
}

void KCanvasClipper::setViewportClipper(bool viewport)
{
    m_viewportMode = viewport;
}

void KCanvasClipper::resetClipData()
{
    m_clipData.clear();
}

void KCanvasClipper::addClipData(const KCPathDataList &path, KCWindRule rule, bool bbox)
{
    m_clipData.addPath(path, rule, bbox, viewportClipper());
}

KCClipDataList KCanvasClipper::clipData() const
{
    return m_clipData;
}

QTextStream& KCanvasClipper::externalRepresentation(QTextStream &ts) const
{
    ts << "[type=CLIPPER]";
    if (viewportClipper())    
       ts << " [viewport clipped=" << viewportClipper() << "]";
    ts << " [clip data=" << clipData() << "]";
    return ts;
}

// KCanvasMarker
KCanvasMarker::KCanvasMarker(KCanvasItem *marker) : KCanvasResource()
{
    m_refX = 0;
    m_refY = 0;
    m_marker = marker;
    setAutoAngle();
}

KCanvasMarker::~KCanvasMarker()
{
}

void KCanvasMarker::setMarker(KCanvasItem *marker)
{
    m_marker = marker;
}

void KCanvasMarker::setRefX(double refX)
{
    m_refX = refX;
}

double KCanvasMarker::refX() const
{
    return m_refX;
}

void KCanvasMarker::setRefY(double refY)
{
    m_refY = refY;
}

double KCanvasMarker::refY() const
{
    return m_refY;
}

void KCanvasMarker::setAngle(float angle)
{
    m_angle = angle;
}

float KCanvasMarker::angle() const
{
    return m_angle;
}

void KCanvasMarker::setAutoAngle()
{
    m_angle = -1;
}

void KCanvasMarker::draw(double x, double y, double angle)
{
    if(m_marker && m_marker->style())
    {
        KCanvasMatrix translation;
        translation.translate(x, y);

        KCanvasMatrix rotation;
        rotation.setOperationMode(OPS_POSTMUL);
        rotation.translate(-m_refX, -m_refY);
        rotation.rotate(m_angle > -1 ? m_angle : angle);

        m_marker->draw(QRect());
    }
}

QTextStream& KCanvasMarker::externalRepresentation(QTextStream &ts) const
{
    ts << "[type=MARKER]"
       << " [angle=";
    if (angle() == -1) 
        ts << "auto" << "]";
    else 
        ts << angle() << "]";        
    ts << " [ref x=" << refX() << " y=" << refY() << "]";
    return ts;
}

// vim:ts=4:noet
