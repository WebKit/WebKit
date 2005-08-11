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

#ifndef KSVG_SVGException_H
#define KSVG_SVGException_H

#include <kdom/ecma/Ecma.h>
#include <ksvg2/ecma/SVGLookup.h>

namespace KSVG
{
	class SVGExceptionImpl;
	class SVGException
	{
	public:
		SVGException();
		explicit SVGException(SVGExceptionImpl *i);
		SVGException(const SVGException &other);
		virtual ~SVGException();

		// Operators
		SVGException &operator=(const SVGException &other);
		bool operator==(const SVGException &other) const;
		bool operator!=(const SVGException &other) const;

		// 'SVGException' functions
		unsigned short code() const;

		// Internal
		KSVG_INTERNAL(SVGException)

	protected:
		SVGExceptionImpl *impl;

	public: // EcmaScript section
		KDOM_GET

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

#endif

// vim:ts=4:noet
