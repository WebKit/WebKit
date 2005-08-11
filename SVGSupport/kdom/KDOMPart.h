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

#ifndef KDOM_PART_H
#define KDOM_PART_H

#include <kparts/part.h>
#include <kparts/browserextension.h>

namespace KDOM
{
	class KDOMView;
	class KDOMSettings;
	class DocumentImpl;

	class KDOMPart : public KParts::ReadOnlyPart
	{
	public:
		/**
		 * Constructs a new KDOMPart.
		 *
		 */
		KDOMPart(KDOMView *view, QObject *parent = 0, const char *name = 0);
		virtual ~KDOMPart();

		/**
		 * Returns a pointer to the internal Document implementation.
		 */
		DocumentImpl *document() const;
	
		/**
		 * Assign a DocumentImpl pointer to the part.
		 */	
		void setDocument(DocumentImpl *doc);

		/**
		 * Returns a pointer to the DOM document view.
		 */
		KDOMView *view() const;

		/**
		 * Returns a pointer to the DOM document settings.
		 */
		KDOMSettings *settings() const;

		virtual bool urlSelected(const QString &url, int button, int state,
								 const QString &_target,
								 KParts::URLArgs args = KParts::URLArgs());
	protected:
		void setView(KDOMView *view);
		void setSettings(KDOMSettings *settings);

	private:
		struct Private;
		Private *d;
	};
};

#endif

// vim:ts=4:noet
