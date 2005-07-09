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

#ifndef KSVG_SVGElementInstanceList_H
#define KSVG_SVGElementInstanceList_H

#include <ksvg2/ecma/SVGLookup.h>

namespace KSVG
{
	class SVGElementInstance;
	class SVGElementInstanceListImpl;
	class SVGElementInstanceList
	{
	public:
		SVGElementInstanceList();
		explicit SVGElementInstanceList(SVGElementInstanceListImpl *i);
		SVGElementInstanceList(const SVGElementInstanceList &other);
		virtual ~SVGElementInstanceList();

		// Operators
		SVGElementInstanceList &operator=(const SVGElementInstanceList &other);
		bool operator==(const SVGElementInstanceList &other) const;
		bool operator!=(const SVGElementInstanceList &other) const;

		// 'SVGElementInstanceList' functions
		unsigned long length() const;
		SVGElementInstance item(unsigned long index);

		// Internal
		KSVG_INTERNAL_BASE(SVGElementInstanceList)

	protected:
		SVGElementInstanceListImpl *impl;

	public: // EcmaScript section
		KDOM_BASECLASS_GET

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KSVG_DEFINE_PROTOTYPE(SVGElementInstanceListProto)
KSVG_IMPLEMENT_PROTOFUNC(SVGElementInstanceListProtoFunc, SVGElementInstanceList)

#endif

// vim:ts=4:noet
