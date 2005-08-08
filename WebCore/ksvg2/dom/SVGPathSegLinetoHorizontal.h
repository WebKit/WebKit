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

#ifndef KSVG_SVGPathSegLinetoHorizontal_H
#define KSVG_SVGPathSegLinetoHorizontal_H

#include <ksvg2/dom/SVGPathSeg.h>

namespace KSVG
{
	class SVGPathSegLinetoHorizontalAbsImpl;
	class SVGPathSegLinetoHorizontalAbs : public SVGPathSeg
	{
	public:
		SVGPathSegLinetoHorizontalAbs();
		explicit SVGPathSegLinetoHorizontalAbs(SVGPathSegLinetoHorizontalAbsImpl *);
		SVGPathSegLinetoHorizontalAbs(const SVGPathSegLinetoHorizontalAbs &);
		SVGPathSegLinetoHorizontalAbs(const SVGPathSeg &);
		virtual ~SVGPathSegLinetoHorizontalAbs();

		// Operators
		SVGPathSegLinetoHorizontalAbs &operator=(const SVGPathSegLinetoHorizontalAbs &other);
		SVGPathSegLinetoHorizontalAbs &operator=(const SVGPathSeg &other);

		void setX(float);
		float x() const;

		// Internal
		KSVG_INTERNAL(SVGPathSegLinetoHorizontalAbs)

	public: // EcmaScript section
		KDOM_GET
		KDOM_PUT

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
		void putValueProperty(KJS::ExecState *exec, int token, KJS::ValueImp *value, int attr);
	};

	class SVGPathSegLinetoHorizontalRelImpl;
	class SVGPathSegLinetoHorizontalRel : public SVGPathSeg
	{
	public:
		SVGPathSegLinetoHorizontalRel();
		explicit SVGPathSegLinetoHorizontalRel(SVGPathSegLinetoHorizontalRelImpl *);
		SVGPathSegLinetoHorizontalRel(const SVGPathSegLinetoHorizontalRel &);
		SVGPathSegLinetoHorizontalRel(const SVGPathSeg &);
		virtual ~SVGPathSegLinetoHorizontalRel();

		// Operators
		SVGPathSegLinetoHorizontalRel &operator=(const SVGPathSegLinetoHorizontalRel &other);
		SVGPathSegLinetoHorizontalRel &operator=(const SVGPathSeg &other);

		void setX(float);
		float x() const;

		// Internal
		KSVG_INTERNAL(SVGPathSegLinetoHorizontalRel)

	public: // EcmaScript section
		KDOM_GET
		KDOM_PUT

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
		void putValueProperty(KJS::ExecState *exec, int token, KJS::ValueImp *value, int attr);
	};
};

#endif

// vim:ts=4:noet
