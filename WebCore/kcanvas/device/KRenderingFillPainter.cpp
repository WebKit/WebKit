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

#include "KRenderingDevice.h"
#include "KRenderingFillPainter.h"
#include "KRenderingPaintServer.h"

class KRenderingFillPainter::Private
{
public:
	Private()
	{
		pserver = 0;
		opacity = 1.0;
		fillRule = RULE_NONZERO;
	}

	~Private() { }

	float opacity;
	KCWindRule fillRule;

	KRenderingPaintServer *pserver;
};

KRenderingFillPainter::KRenderingFillPainter() : d(new Private())
{
}

KRenderingFillPainter::~KRenderingFillPainter()
{
	delete d;
}

KRenderingPaintServer *KRenderingFillPainter::paintServer() const
{
	return d->pserver;
}

void KRenderingFillPainter::setPaintServer(KRenderingPaintServer *pserver)
{
	d->pserver = pserver;
}

KCWindRule KRenderingFillPainter::fillRule() const
{
	return d->fillRule;
}

void KRenderingFillPainter::setFillRule(KCWindRule rule)
{
	d->fillRule = rule;
}

float KRenderingFillPainter::opacity() const
{
	return d->opacity;
}

void KRenderingFillPainter::setOpacity(float opacity)
{
	d->opacity = opacity;
}

void KRenderingFillPainter::draw(KRenderingDeviceContext *context, const KCanvasCommonArgs &args) const
{
	if(d->pserver)
	{
		KCanvasResource *res = dynamic_cast<KCanvasResource *>(d->pserver);
		if(res && res->listener())		
			res->listener()->resourceNotification();

		d->pserver->draw(context, args, APPLY_TO_FILL);
	}
}

// vim:ts=4:noet
