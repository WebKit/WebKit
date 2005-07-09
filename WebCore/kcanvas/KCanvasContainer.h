/*
	Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>

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

#ifndef KCanvasContainer_H
#define KCanvasContainer_H

#include <kcanvas/KCanvasItem.h>

class KCanvas;
class KRenderingStyle;
class KCanvasContainer : public KCanvasItem
{
public:
	KCanvasContainer(KCanvas *canvas, KRenderingStyle *style);
	virtual ~KCanvasContainer();

	// Some containers do not want it's children
	// to be drawn, because they may be 'referenced'
	// Example: <marker> children in SVG
	void setDrawContents(bool drawContents);

	virtual bool needsTemporaryBuffer() const;
	virtual bool isContainer() const { return true; }

	// Container logic
	virtual void appendItem(KCanvasItem *item);
	virtual void insertItemBefore(KCanvasItem *item, KCanvasItem *reference);
	virtual void removeItem(const KCanvasItem *item);

	// 'KCanvasItem' functions
	virtual bool fillContains(const QPoint &p) const;
	virtual bool strokeContains(const QPoint &p) const;

	virtual void draw(const QRect &rect) const;
	
	virtual void invalidate() const;
	virtual QRect bbox(bool includeStroke = true) const;

private:
	friend class KCanvasItem;

	virtual bool raiseItem(KCanvasItem *item);
	virtual bool lowerItem(KCanvasItem *item);

private:
	friend class KCanvas;

	void collisions(const QPoint &p, KCanvasItemList &hits) const;

private:
	class Private;
	Private *d;
};

#endif

// vim:ts=4:noet
