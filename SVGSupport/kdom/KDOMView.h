/*
    Copyright (C) 2004 Nikolas Zimmermann   <wildfox@kde.org>
                  2004 Rob Buis             <buis@kde.org>

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

#ifndef KDOM_KDOMView_H
#define KDOM_KDOMView_H

#include <qstring.h>

namespace KDOM
{
	class KDOMPart;

	/**
	 * A generic view class for any layer-on-top of kdom
	 * which can perform rendering... (ie. khtml2/ksvg2)
	 */
	class KDOMView
	{
	public:
		/**
		 * Constructs a KDOMView.
		 */
		KDOMView(KDOMPart *part);
		virtual ~KDOMView();

		/**
		 * Returns a pointer to the KDOMPart that is
		 * rendering the page.
		 */
		KDOMPart *part() const;

		/**
		* Get/set the CSS Media Type.
		*
		* Media type is set to "screen" for on-screen rendering and "print"
		* during printing. Other media types lack the proper support in the
		* renderer and are not activated. The DOM tree and the parser itself,
		* however, properly handle other media types. To make them actually work
		* you only need to enable the media type in the view and if necessary
		* add the media type dependent changes to the renderer.
		*/
		QString mediaType() const;
		void setMediaType(const QString &medium);

	private:
		struct KDOMViewPrivate;
		KDOMViewPrivate *d;
	};
};

#endif

// vim:ts=4:noet
