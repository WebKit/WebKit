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

#ifndef KSVG_SVGLengthImpl_H
#define KSVG_SVGLengthImpl_H

#include <kdom/Shared.h>
#include <kdom/DOMString.h>

#include <ksvg2/impl/SVGHelper.h>

class KCanvasItem;

namespace KSVG
{
	class SVGElementImpl;
	class SVGStyledElementImpl;
	class SVGLengthImpl : public KDOM::Shared
	{
	public:
		SVGLengthImpl(const SVGStyledElementImpl *context, LengthMode mode = LM_UNKNOWN, const SVGElementImpl *viewport = 0);
		virtual ~SVGLengthImpl();

		// 'SVGLength' functions
		unsigned short unitType() const;

		float value() const;
		void setValue(float value);

		float valueInSpecifiedUnits() const;
		void setValueInSpecifiedUnits(float valueInSpecifiedUnits);

		KDOM::DOMString valueAsString() const;
		void setValueAsString(const KDOM::DOMString &valueAsString);

		void newValueSpecifiedUnits(unsigned short unitType, float valueInSpecifiedUnits);
		void convertToSpecifiedUnits(unsigned short unitType);

		// Helpers
		bool bboxRelative() const;
		void setBboxRelative(bool relative);

		const SVGStyledElementImpl *context() const;
		void setContext(const SVGStyledElementImpl *context);

	private:
		bool updateValueInSpecifiedUnits();
		void updateValue();

		double dpi() const;

		float m_value;
		float m_valueInSpecifiedUnits;

		LengthMode m_mode : 2;
		bool m_bboxRelative : 1;
		unsigned short m_unitType : 4;

		const SVGStyledElementImpl *m_context;
		const SVGElementImpl *m_viewportElement;
	};
};

#endif

// vim:ts=4:noet
