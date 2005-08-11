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

#ifndef KSVG_SVGPreserveAspectRatio_H
#define KSVG_SVGPreserveAspectRatio_H

#include <ksvg2/ecma/SVGLookup.h>

namespace KSVG
{
	class SVGPreserveAspectRatioImpl;
	class SVGPreserveAspectRatio 
	{ 
	public:
		SVGPreserveAspectRatio();
		explicit SVGPreserveAspectRatio(SVGPreserveAspectRatioImpl *);
		SVGPreserveAspectRatio(const SVGPreserveAspectRatio &);
		virtual ~SVGPreserveAspectRatio();

		// 'SVGPreserveAspectRatio' functions
		SVGPreserveAspectRatio &operator=(const SVGPreserveAspectRatio &other);
		bool operator==(const SVGPreserveAspectRatio &other) const;
		bool operator!=(const SVGPreserveAspectRatio &other) const;

		void setAlign(unsigned short);
		unsigned short align() const;

		void setMeetOrSlice(unsigned short);
		unsigned short meetOrSlice() const;

		// Internal
		KSVG_INTERNAL_BASE(SVGPreserveAspectRatio)

	protected:
		SVGPreserveAspectRatioImpl *impl;

	public: // EcmaScript section
		KDOM_BASECLASS_GET
		KDOM_PUT

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
		void putValueProperty(KJS::ExecState *exec, int token, KJS::ValueImp *value, int attr);
	};
};

#endif

// vim:ts=4:noet
