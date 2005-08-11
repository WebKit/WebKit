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

#ifndef KSVG_SVGTransformList_H
#define KSVG_SVGTransformList_H

#include <ksvg2/ecma/SVGLookup.h>

namespace KSVG
{
	class SVGMatrix;
	class SVGTransform;
	class SVGTransformListImpl;
	class SVGTransformList
	{
	public:
		SVGTransformList();
		explicit SVGTransformList(SVGTransformListImpl *i);
		SVGTransformList(const SVGTransformList &other);
		virtual ~SVGTransformList();

		// Operators
		SVGTransformList &operator=(const SVGTransformList &other);
		bool operator==(const SVGTransformList &other) const;
		bool operator!=(const SVGTransformList &other) const;

		// 'SVGTransformList' functions
		unsigned long numberOfItems() const;
		void clear();

		SVGTransform initialize(const SVGTransform &newItem);
		SVGTransform getItem(unsigned long index);
		SVGTransform insertItemBefore(const SVGTransform &newItem, unsigned long index);
		SVGTransform replaceItem(const SVGTransform &newItem, unsigned long index);
		SVGTransform removeItem(unsigned long index);
		SVGTransform appendItem(const SVGTransform &newItem);

		SVGTransform createSVGTransformFromMatrix(const SVGMatrix &matrix) const;
		SVGTransform consolidate();

		// Internal
		KSVG_INTERNAL_BASE(SVGTransformList)

	protected:
		SVGTransformListImpl *impl;

	public: // EcmaScript section
		KDOM_BASECLASS_GET

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KSVG_DEFINE_PROTOTYPE(SVGTransformListProto)
KSVG_IMPLEMENT_PROTOFUNC(SVGTransformListProtoFunc, SVGTransformList)

#endif

// vim:ts=4:noet
