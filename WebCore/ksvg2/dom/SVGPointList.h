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

#ifndef KSVG_SVGPointList_H
#define KSVG_SVGPointList_H

#include <ksvg2/ecma/SVGLookup.h>

namespace KSVG
{
	class SVGPoint;
	class SVGPointListImpl;
	class SVGPointList
	{
	public:
		SVGPointList();
		explicit SVGPointList(SVGPointListImpl *i);
		SVGPointList(const SVGPointList &other);
		virtual ~SVGPointList();

		// Operators
		SVGPointList &operator=(const SVGPointList &other);
		bool operator==(const SVGPointList &other) const;
		bool operator!=(const SVGPointList &other) const;

		// 'SVGPointList' functions
		unsigned long numberOfItems() const;
		void clear();

		SVGPoint initialize(const SVGPoint &newItem);
		SVGPoint getItem(unsigned long index);
		SVGPoint insertItemBefore(const SVGPoint &newItem, unsigned long index);
		SVGPoint replaceItem(const SVGPoint &newItem, unsigned long index);
		SVGPoint removeItem(unsigned long index);
		SVGPoint appendItem(const SVGPoint &newItem);

		// Internal
		KSVG_INTERNAL_BASE(SVGPointList)

	protected:
		SVGPointListImpl *impl;

	public: // EcmaScript section
		KDOM_BASECLASS_GET

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KSVG_DEFINE_PROTOTYPE(SVGPointListProto)
KSVG_IMPLEMENT_PROTOFUNC(SVGPointListProtoFunc, SVGPointList)

#endif

// vim:ts=4:noet
