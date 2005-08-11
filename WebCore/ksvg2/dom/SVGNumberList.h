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

#ifndef KSVG_SVGNumberList_H
#define KSVG_SVGNumberList_H

#include <ksvg2/ecma/SVGLookup.h>

namespace KSVG
{
	class SVGMatrix;
	class SVGNumber;
	class SVGNumberListImpl;
	class SVGNumberList
	{
	public:
		SVGNumberList();
		explicit SVGNumberList(SVGNumberListImpl *i);
		SVGNumberList(const SVGNumberList &other);
		virtual ~SVGNumberList();

		// Operators
		SVGNumberList &operator=(const SVGNumberList &other);
		bool operator==(const SVGNumberList &other) const;
		bool operator!=(const SVGNumberList &other) const;

		// 'SVGNumberList' functions
		unsigned long numberOfItems() const;
		void clear();

		SVGNumber initialize(const SVGNumber &newItem);
		SVGNumber getItem(unsigned long index);
		SVGNumber insertItemBefore(const SVGNumber &newItem, unsigned long index);
		SVGNumber replaceItem(const SVGNumber &newItem, unsigned long index);
		SVGNumber removeItem(unsigned long index);
		SVGNumber appendItem(const SVGNumber &newItem);

		// Internal
		KSVG_INTERNAL_BASE(SVGNumberList)

	protected:
		SVGNumberListImpl *impl;

	public: // EcmaScript section
		KDOM_BASECLASS_GET

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KSVG_DEFINE_PROTOTYPE(SVGNumberListProto)
KSVG_IMPLEMENT_PROTOFUNC(SVGNumberListProtoFunc, SVGNumberList)

#endif

// vim:ts=4:noet
