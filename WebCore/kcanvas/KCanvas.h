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

#ifndef KCanvas_H
#define KCanvas_H

#include <kcanvas/KCanvasItem.h>

class KCanvasView;
class KCanvasRegistry;
class KRenderingDevice;
class KCanvas
{
public:
	KCanvas(KRenderingDevice *device);
	virtual ~KCanvas();

	KCanvasContainer *rootContainer() const;
	
	// Assign the topmost rendering item.
	// It has to be a KCanvasContainer.
	void setRootContainer(KCanvasContainer *root);

	// Register/deregister KCanvasViews with the canvas.
	// The views will be notified anytime they
	// need to update their contents...
	void addView(const KCanvasView *view);
	void removeView(const KCanvasView *view);

	// Returns active painting backend
	KRenderingDevice *renderingDevice() const;

	// Returns the registry of our canvas
	KCanvasRegistry *registry() const;

	// Returns/sets the canvas size
	QSize canvasSize() const;
	void setCanvasSize(const QSize &size);
	
	// Deletes all canvas items
	void reset();

	// TODO: maybe add insertItemBefore, as convenience function
	// void insertItemBefore(KCanvasItem *item, KCanvasItem *other);
	
	// Hit detection
	void collisions(const QPoint &p, KCanvasItemList &hits) const;

private:
	friend class KCanvasView;
	friend class KCanvasItem;
	friend class KCanvasResource;

	void invalidate(const KCanvasItem *item); // Helper

private:

	class Private;
	Private *d;
};

#endif

// vim:ts=4:noet
