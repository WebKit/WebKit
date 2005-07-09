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

#ifndef KSVG_SVGPathSeg_H
#define KSVG_SVGPathSeg_H

#include <kdom/DOMString.h>
#include <ksvg2/ecma/SVGLookup.h>

namespace KSVG
{
	class SVGPathSegImpl;
	class SVGPathSeg 
	{ 
	public:
		SVGPathSeg();
		explicit SVGPathSeg(SVGPathSegImpl *);
		SVGPathSeg(const SVGPathSeg &);
		virtual ~SVGPathSeg();

		// Operators
		SVGPathSeg &operator=(const SVGPathSeg &other);
		bool operator==(const SVGPathSeg &other) const;
		bool operator!=(const SVGPathSeg &other) const;

		unsigned short pathSegType() const;
		KDOM::DOMString pathSegTypeAsLetter() const;

		// Internal
		KSVG_INTERNAL(SVGPathSeg)
		SVGPathSegImpl *handle() const { return impl; }

	protected:
		SVGPathSegImpl *impl;

	public: // EcmaScript section
		KDOM_BASECLASS_GET
		KDOM_CAST

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
	};

	KDOM_DEFINE_CAST(SVGPathSeg)

#define KSVG_PATHSEG_DERIVED_ASSIGN_OP(T, PATHSEG_TYPE) \
T &T::operator=(const SVGPathSeg &other) \
{ \
	SVGPathSegImpl *ohandle = static_cast<SVGPathSegImpl *>(other.handle()); \
	if(impl != ohandle) { \
		if(!ohandle || ohandle->pathSegType() != PATHSEG_TYPE) { \
			if(impl) impl->deref(); \
			impl = 0; \
		} \
		else \
			SVGPathSeg::operator=(other); \
	} \
	return *this; \
}

};

#endif

// vim:ts=4:noet
