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

#ifndef KSVG_SVGTests_H
#define KSVG_SVGTests_H

#include <ksvg2/ecma/SVGLookup.h>

namespace KSVG
{
	class SVGStringList;
	class SVGTestsImpl;

	class SVGTests
	{
	public:
		SVGTests();
		explicit SVGTests(SVGTestsImpl *i);
		SVGTests(const SVGTests &other);
		virtual ~SVGTests();

		// Operators
		SVGTests &operator=(const SVGTests &other);
		SVGTests &operator=(SVGTestsImpl *other);

		// 'SVGTests' functions
		SVGStringList requiredFeatures() const;
		SVGStringList requiredExtensions() const;
		SVGStringList systemLanguage() const;

		bool hasExtension(const KDOM::DOMString &extension) const;

		// Internal
		KSVG_INTERNAL_BASE(SVGTests)

	protected:
		SVGTestsImpl *impl;

	public: // EcmaScript section
		KDOM_BASECLASS_GET

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KSVG_DEFINE_PROTOTYPE(SVGTestsProto)
KSVG_IMPLEMENT_PROTOFUNC(SVGTestsProtoFunc, SVGTests)

#endif

// vim:ts=4:noet
