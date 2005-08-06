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

#include "KCanvas.h"
#include "KRenderingStyle.h"
#include "KRenderingDevice.h"
#include "KCanvasContainer.h"

class KCanvasContainer::Private
{
public:
	Private() : drawContents(true), first(0), last(0) { }	
	~Private() { }

	bool drawContents : 1;

	KCanvasItem *first;
	KCanvasItem *last;
};

KCanvasContainer::KCanvasContainer(KCanvas *canvas, KRenderingStyle *style)
: KCanvasItem(canvas, style, 0), d(new Private())
{
}

KCanvasContainer::~KCanvasContainer()
{
	KCanvasItem *next;
	for(KCanvasItem *n = d->first;n;n = next)
	{
		next = n->next();
		delete n;
	}

	delete d;
}

void KCanvasContainer::setDrawContents(bool drawContents)
{
	d->drawContents = drawContents;
}

bool KCanvasContainer::needsTemporaryBuffer() const
{
	bool conditionOne = (style()->opacity() < 255);
	bool conditionTwo = (style()->clipPaths().count() > 0);

	return conditionOne || conditionTwo;
}

void KCanvasContainer::appendItem(KCanvasItem *item)
{
	Q_ASSERT(item);

	KCanvasItem *prev = item->prev();
	KCanvasItem *next = item->next();

	if(prev)
		prev->setNext(next);
	
	if(next)
		next->setPrev(prev);

	item->setPrev(d->last);

	if(d->last)
		d->last->setNext(item);

	d->last = item;

	if(!d->first)
		d->first = item;

	item->setParent(this);
}

void KCanvasContainer::insertItemBefore(KCanvasItem *item, KCanvasItem *reference)
{
	Q_ASSERT(item);

	if(!reference)
		appendItem(item);
	else // We need to correct the existing tree structure...
	{
		KCanvasItem *current = d->first;
		for(; current != 0; current = current->next())
		{
			if(current == reference)
			{
				KCanvasItem *itemPrev = item->prev();
				KCanvasItem *itemNext = item->next();
				KCanvasItem *currentPrev = current->prev();

				if(itemPrev)
					itemPrev->setNext(itemNext);

				if(itemNext)
					itemNext->setPrev(itemPrev);

				if(currentPrev)
					currentPrev->setNext(item);

				item->setPrev(currentPrev);
				item->setNext(current);

				current->setPrev(item);

				if(current == d->first)
					d->first = item;

				return;
			}
		}
	}
}

void KCanvasContainer::removeItem(const KCanvasItem *item)
{
	Q_ASSERT(item);

	KCanvasItem *current = d->first;
	for(; current != 0; current = current->next())
	{
		if(current == item)
		{
			KCanvasItem *currentPrev = current->prev();
			KCanvasItem *currentNext = current->next();

			if(currentPrev)
				currentPrev->setNext(currentNext);

			if(currentNext)
				currentNext->setPrev(currentPrev);

			if(current == d->first)
				d->first = currentNext;

			if(current == d->last)
				d->last = currentPrev;

			delete current;
			return;
		}
	}
}

bool KCanvasContainer::fillContains(const QPoint &p) const
{
	KCanvasItem *current = d->first;
	for(; current != 0; current = current->next())
	{
		if(current->fillContains(p))
			return true;
	}

	return false;
}

bool KCanvasContainer::strokeContains(const QPoint &p) const
{
	KCanvasItem *current = d->first;
	for(; current != 0; current = current->next())
	{
		if(current->strokeContains(p))
			return true;
	}

	return false;
}

void KCanvasContainer::draw(const QRect &rect) const
{
	if(!d->drawContents)
		return;

	KCanvasItem *current = d->first;
	for(; current != 0; current = current->next())
	{
		if(current->isVisible())
			current->draw(rect);
	}
}

void KCanvasContainer::invalidate() const
{
	KCanvasItem *current = d->first;
	for(; current != 0; current = current->next())
		current->invalidate();
}

QRect KCanvasContainer::bbox(bool includeStroke) const
{
	QRect rect;

	KCanvasItem *current = d->first;
	for(; current != 0; current = current->next())
	{
		if(!rect.isValid())
			rect = current->bbox(includeStroke);
		else
			rect = rect.unite(current->bbox(includeStroke));
	}

	return rect;
}

bool KCanvasContainer::raiseItem(KCanvasItem *item)
{
	KCanvasItem *itemPrev = item->prev();
	KCanvasItem *itemNext = item->next();

	if(!itemNext)
		return false;
	
	itemNext->setPrev(itemPrev);
	if(itemPrev)
		itemPrev->setNext(itemNext);
	
	item->setNext(itemNext->next());
	itemNext->setNext(item);

	return true;
}

bool KCanvasContainer::lowerItem(KCanvasItem *item)
{
	KCanvasItem *itemPrev = item->prev();
	KCanvasItem *itemNext = item->next();

	if(!itemPrev)
		return false;
	
	itemPrev->setNext(itemNext);
	if(itemNext)
		itemNext->setPrev(itemPrev);
	
	item->setPrev(itemPrev);
	itemPrev->setPrev(item);

	item->setNext(itemPrev);

	if(d->last == item)
		d->last = itemPrev;
	
	if(d->first == itemPrev)
		d->first = item;
	
	return true;
}

KCanvasItem *KCanvasContainer::first() const
{
    return d->first;
}

KCanvasItem *KCanvasContainer::last() const
{
    return d->last;
}

void KCanvasContainer::collisions(const QPoint &p, KCanvasItemList &hits) const
{
	if(p.x() < 0 || p.y() < 0)
		return;

	KCanvasItem *current = d->last;
	for(; current != 0; current = current->prev())
	{
		if(current->isContainer())
		{
			static_cast<const KCanvasContainer *>(current)->collisions(p, hits);
			return;
		}
		
		QRect rect(current->bbox());

		// TODO: is this logic correct?
		if(((current->fillContains(p) && rect.contains(p))) || current->strokeContains(p))
			hits.append(current);
	}
}

// vim:ts=4:noet
