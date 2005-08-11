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
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KSVG_SVGElementInstance_H
#define KSVG_SVGElementInstance_H

#include <ksvg2/ecma/SVGLookup.h>
#include <kdom/events/EventTarget.h>

namespace KSVG
{
	class SVGElement;
	class SVGUseElement;
	class SVGElementInstanceImpl;
	class SVGElementInstanceList;
	class SVGElementInstance : public KDOM::EventTarget
	{
	public:
		SVGElementInstance();
		SVGElementInstance(SVGElementInstanceImpl *p);
		SVGElementInstance(const SVGElementInstance &other);
		virtual ~SVGElementInstance();

		// Operators
		SVGElementInstance &operator=(const SVGElementInstance &other);

		// 'SVGElementInstance' functions
		SVGElement correspondingElement() const;
		SVGUseElement correspondingUseElement() const;

		SVGElementInstance parentNode() const;
		SVGElementInstanceList childNodes() const;
		SVGElementInstance firstChild() const;
		SVGElementInstance lastChild() const;
		SVGElementInstance previousSibling() const;
		SVGElementInstance nextSibling() const;

		// Internal
		KDOM_INTERNAL(SVGElementInstance)

	public: // EcmaScript section
		KDOM_BASECLASS_GET
		KDOM_FORWARDPUT

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

#endif

// vim:ts=4:noet
