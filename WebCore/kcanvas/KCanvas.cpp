/*
	Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

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

#include <kdebug.h>
#include <kglobal.h>

#include <qvaluelist.h>

#include "KCanvas.h"
#include "KCanvasView.h"
#include "KCanvasRegistry.h"
#include "KCanvasContainer.h"
#include "KRenderingDevice.h"

class KCanvas::Private
{
public:
	Private(KRenderingDevice *device)
	{
		rootContainer = 0;
		registry = new KCanvasRegistry();
		renderingDevice = device;
	}

	~Private()
	{
		delete registry;
		delete rootContainer;
		//delete renderingDevice;  // Should not delete the rendering device!
	}

	QSize canvasSize;

	KCanvasContainer *rootContainer;
	
	KCanvasRegistry *registry;
	KRenderingDevice *renderingDevice;

	QValueList<const KCanvasView *> viewList;
};

KCanvas::KCanvas(KRenderingDevice *device) : d(new Private(device))
{
}

KCanvas::~KCanvas()
{
	reset();
	delete d;
}

void KCanvas::setRootContainer(KCanvasContainer *root)
{
	d->rootContainer = root;
}

void KCanvas::addView(const KCanvasView *view)
{
	Q_ASSERT(view);
	d->viewList.append(view);
}

void KCanvas::removeView(const KCanvasView *view)
{
	Q_ASSERT(view);
	d->viewList.remove(view);
}

KRenderingDevice *KCanvas::renderingDevice() const
{
	Q_ASSERT(d->renderingDevice);
	return d->renderingDevice;
}

KCanvasRegistry *KCanvas::registry() const
{
	Q_ASSERT(d->registry);
	return d->registry;
}

QSize KCanvas::canvasSize() const
{
	return d->canvasSize;
}

void KCanvas::setCanvasSize(const QSize &size)
{
	d->canvasSize = size;

	// Resize all views...
	QValueListConstIterator<const KCanvasView *> it = d->viewList.constBegin();
	QValueListConstIterator<const KCanvasView *> end = d->viewList.constEnd();

	for(; it != end; ++it)
		const_cast<KCanvasView *>(*it)->canvasSizeChanged(size.width(), size.height());
}

void KCanvas::reset()
{
	registry()->cleanup();
}

void KCanvas::collisions(const QPoint &p, KCanvasItemList &hits) const
{
	Q_ASSERT(d->rootContainer);
	return d->rootContainer->collisions(p, hits);
}

// Helpers
KCanvasContainer *KCanvas::rootContainer() const
{
	return d->rootContainer;
}

void KCanvas::invalidate(const KCanvasItem *item)
{
	// Invalidate the item in all views
	QValueListConstIterator<const KCanvasView *> it = d->viewList.constBegin();
	QValueListConstIterator<const KCanvasView *> end = d->viewList.constEnd();

	for(; it != end; ++it)
		(*it)->invalidateCanvasItem(item);
}

// vim:ts=4:noet
