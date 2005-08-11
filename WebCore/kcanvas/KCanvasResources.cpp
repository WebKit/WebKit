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

#include <qrect.h>
#include <kdebug.h>

#include "KCanvas.h"
#include "KCanvasItem.h"
#include "KCanvasMatrix.h"
#include "KCanvasResources.h"
#include "KRenderingDevice.h"
#include "KCanvasResourceListener.h"

// KCanvasResource
KCanvasResource::KCanvasResource()
{
	m_changed = true;

	m_listener = 0;
}

KCanvasResource::~KCanvasResource()
{
}

bool KCanvasResource::changed() const
{
	return m_changed;
}

void KCanvasResource::setChanged(bool changed)
{
	m_changed = changed;
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

KCanvasResourceListener *KCanvasResource::listener() const
{
	return m_listener;
}

void KCanvasResource::setListener(KCanvasResourceListener *listener)
{
	m_listener = listener;
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
	kdDebug() << "Drawing marker at : " << x << " , " << y << endl;
	if(m_marker && m_marker->style())
	{
		setChanged(false);

		KCanvasMatrix translation;
		translation.translate(x, y);

		KCanvasMatrix rotation;
		rotation.setOperationMode(OPS_POSTMUL);
		rotation.translate(-m_refX, -m_refY);
		rotation.rotate(m_angle > -1 ? m_angle : angle);

		m_marker->draw(QRect());
	}
}

// vim:ts=4:noet
