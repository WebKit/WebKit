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
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KSVG_SVGStyleElement_H
#define KSVG_SVGStyleElement_H

#include <SVGElement.h>
#include <kdom/css/LinkStyle.h>

namespace KSVG
{
	class SVGStyleElementImpl;
	class SVGStyleElement : public SVGElement,	
							public KDOM::LinkStyle
	{
	public:
		SVGStyleElement();
		explicit SVGStyleElement(SVGStyleElementImpl *i);
		SVGStyleElement(const SVGStyleElement &other);
		SVGStyleElement(const KDOM::Node &other);
		virtual ~SVGStyleElement();

		// Operators
		SVGStyleElement &operator=(const SVGStyleElement &other);
		SVGStyleElement &operator=(const KDOM::Node &other);

		// 'SVGStyleElement' functions
		KDOM::DOMString xmlspace() const;
		void setXmlspace(const KDOM::DOMString &);

		KDOM::DOMString type() const;
		void setType(const KDOM::DOMString &);

		KDOM::DOMString media() const;
		void setMedia(const KDOM::DOMString &);

		KDOM::DOMString title() const;
		void setTitle(const KDOM::DOMString &);

		// LinkStyle
		virtual KDOM::StyleSheet sheet() const;

		// Internal
		KSVG_INTERNAL(SVGStyleElement)

	public: // EcmaScript section
		KDOM_GET
		KDOM_PUT

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
		void putValueProperty(KJS::ExecState *exec, int token, KJS::ValueImp *value, int attr);
	};
};

#endif

// vim:ts=4:noet
