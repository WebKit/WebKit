/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann   <wildfox@kde.org>
                  2004, 2005 Rob Buis             <buis@kde.org>

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
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "KDOMPart.h"
#include "KDOMView.h"
#include "KDOMSettings.h"
#include "DocumentImpl.h"

using namespace KDOM;

struct KDOMPart::Private
{
	KDOMView *view;
	KDOMSettings *settings;

	DocumentImpl *doc;
};

KDOMPart::KDOMPart(KDOMView *view, QObject *parent, const char *name)
: KParts::ReadOnlyPart(parent, name), d(new Private())
{
	d->doc = 0;
	/* FIXME
	d->settings = new KDOMSettings(*KDOMFactory::defaultSettings());;
	*/
	d->settings = new KDOMSettings();
	d->view = view;
}

KDOMPart::~KDOMPart()
{
	delete d;
}

KDOMView *KDOMPart::view() const
{
	return d->view;
}

KDOMSettings *KDOMPart::settings() const
{
	return d->settings;
}

DocumentImpl *KDOMPart::document() const
{
	return d->doc;
}

void KDOMPart::setDocument(DocumentImpl *doc)
{
	d->doc = doc;
}

void KDOMPart::setView(KDOMView *view)
{
	d->view = view;
}

void KDOMPart::setSettings(KDOMSettings *settings)
{
	d->settings = settings;
}

// This executes in the active part on a click or other url selection action in
// that active part.
bool KDOMPart::urlSelected(const QString &url, int button, int state, const QString &_target, KParts::URLArgs args)
{
	return true;
}

// vim:ts=4:noet
