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

#ifndef KSVG_SVGLengthList_H
#define KSVG_SVGLengthList_H

#include <ksvg2/ecma/SVGLookup.h>

namespace KSVG
{
	class SVGMatrix;
	class SVGLength;
	class SVGLengthListImpl;
	class SVGLengthList
	{
	public:
		SVGLengthList();
		explicit SVGLengthList(SVGLengthListImpl *i);
		SVGLengthList(const SVGLengthList &other);
		virtual ~SVGLengthList();

		// Operators
		SVGLengthList &operator=(const SVGLengthList &other);
		bool operator==(const SVGLengthList &other) const;
		bool operator!=(const SVGLengthList &other) const;

		// 'SVGLengthList' functions
		unsigned long numberOfItems() const;
		void clear();

		SVGLength initialize(const SVGLength &newItem);
		SVGLength getItem(unsigned long index);
		SVGLength insertItemBefore(const SVGLength &newItem, unsigned long index);
		SVGLength replaceItem(const SVGLength &newItem, unsigned long index);
		SVGLength removeItem(unsigned long index);
		SVGLength appendItem(const SVGLength &newItem);

		// Internal
		KSVG_INTERNAL_BASE(SVGLengthList)

	protected:
		SVGLengthListImpl *impl;

	public: // EcmaScript section
		KDOM_BASECLASS_GET

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KSVG_DEFINE_PROTOTYPE(SVGLengthListProto)
KSVG_IMPLEMENT_PROTOFUNC(SVGLengthListProtoFunc, SVGLengthList)

#endif

// vim:ts=4:noet
