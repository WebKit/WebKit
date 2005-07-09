/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
                  2004 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric.seidel@kdemail.net>

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
#include "KCanvasRegistry.h"
#include "KRenderingDevice.h"
#include "KCanvasContainer.h"
#include "KRenderingFillPainter.h"
#include "KRenderingStrokePainter.h"

class KCanvasItem::Private
{
public:
	Private()
	{
		path = 0;
		style = 0;
		canvas = 0;
		parent = 0;
		prev = 0;
		next = 0;
		userData = 0;

		// Set to invalid. '0' is reserved
		// for the root container.
		zIndex = -1;
	}

	~Private()
	{
		if(path && canvas && canvas->renderingDevice())
			canvas->renderingDevice()->deletePath(path);

		delete style;
	}

	KCanvas *canvas;	
	KRenderingStyle *style;

	int zIndex;
	KCanvasUserData path, userData;

	QRect bbox;

	KCanvasContainer *parent;
	KCanvasItem *prev;
	KCanvasItem *next;
};		

// KCanvasItem
KCanvasItem::KCanvasItem(KCanvas *canvas, KRenderingStyle *style, KCanvasUserData path) : d(new Private())
{
	Q_ASSERT(style != 0);
	Q_ASSERT(canvas != 0);
	
	d->path = path;
	d->style = style;
	d->canvas = canvas;
}

KCanvasItem::~KCanvasItem()
{
	delete d;
}

int KCanvasItem::zIndex() const
{
	return d->zIndex;
}

bool KCanvasItem::isVisible() const
{
	if(!d->style || !d->style->visible())
		return false;

	return true;
}

bool KCanvasItem::fillContains(const QPoint &p) const
{
	if(d->path && d->style && canvas() && canvas()->renderingDevice())
		hitsPath(p, true);

	return false;
}

bool KCanvasItem::strokeContains(const QPoint &p) const
{
	if(d->path && d->style && canvas() && canvas()->renderingDevice())
		return hitsPath(p, false);

	return false;
}

QRect KCanvasItem::bbox(bool includeStroke) const
{
	if(!d->bbox.isValid())
	{
		if(d->path && canvas() && canvas()->renderingDevice())
		{
			QRect bbox = bboxPath(includeStroke);
			if(!includeStroke)
				return bbox;

			d->bbox = bbox; // cache it
		}
	}

	return d->bbox;
}

bool KCanvasItem::hitsPath(const QPoint &hitPoint, bool fill) const
{
	return false;
}

QRect KCanvasItem::bboxPath(bool includeStroke, bool includeTransforms) const
{
	return QRect();
}

void KCanvasItem::draw(const QRect &rect) const
{
	if(d->path && d->style && canvas() && canvas()->renderingDevice())
	{
		if(!d->style->visible())
			return;

		if(d->style->fillPainter() && d->style->fillPainter()->paintServer())
			d->style->fillPainter()->paintServer()->setActiveClient(this);

		if(d->style->strokePainter() && d->style->strokePainter()->paintServer())
			d->style->strokePainter()->paintServer()->setActiveClient(this);
	}
}

void KCanvasItem::changePath(KCanvasUserData newPath)
{
	if(d->path && canvas() && canvas()->renderingDevice())
	{
		canvas()->renderingDevice()->setCurrentPath(newPath);
		canvas()->renderingDevice()->deletePath(d->path);

		d->path = newPath;
	}
}

KCanvas *KCanvasItem::canvas() const
{
	return d->canvas;
}

KCanvasUserData KCanvasItem::path() const
{
	return d->path;
}

KRenderingStyle *KCanvasItem::style() const
{
	return d->style;
}

void KCanvasItem::invalidate() const
{
	if(d->canvas)
	{
		d->canvas->invalidate(this);
		d->bbox = QRect();
	}
}

KCanvasUserData KCanvasItem::userData() const
{
	return d->userData;
}

void KCanvasItem::setUserData(KCanvasUserData userData)
{
	d->userData = userData;
}

bool KCanvasItem::raise()
{
	Q_ASSERT(d->parent);
	return d->parent->raiseItem(this);
}

bool KCanvasItem::lower()
{
	Q_ASSERT(d->parent);
	return d->parent->lowerItem(this);
}

KCanvasContainer *KCanvasItem::parent() const
{
	return d->parent;
}

void KCanvasItem::setParent(KCanvasContainer *parent)
{
	d->parent = parent;
}

KCanvasItem *KCanvasItem::prev() const
{
	return d->prev;
}

void KCanvasItem::setPrev(KCanvasItem *prev)
{
	d->prev = prev;
}

KCanvasItem *KCanvasItem::next() const
{
	return d->next;
}

void KCanvasItem::setNext(KCanvasItem *next)
{
	d->next = next;
}

const KCanvasCommonArgs KCanvasItem::commonArgs() const
{
	KCanvasCommonArgs args;
	args.setPath(path());
	args.setStyle(style());
	args.setCanvas(canvas());
	return args;
}

// vim:ts=4:noet
