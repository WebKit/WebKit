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

#ifndef KDOM_CSSHelper_H
#define KDOM_CSSHelper_H

#include <kdom/DOMString.h>
#include <kdom/css/kdomcss.h>
#include <kdom/css/impl/RenderStyleDefs.h>

namespace KDOM
{
	class CSSHelper
	{
	public:
		static DOMStringImpl *unitTypeToString(unsigned short unitType)
		{
			switch(unitType)
			{
				case CSS_EMS: return new DOMStringImpl("em");
				case CSS_EXS: return new DOMStringImpl("ex");
				case CSS_PX: return new DOMStringImpl("px");
				case CSS_IN: return new DOMStringImpl("in");
				case CSS_CM: return new DOMStringImpl("cm");
				case CSS_MM: return new DOMStringImpl("mm");
				case CSS_PT: return new DOMStringImpl("pt");
				case CSS_PC: return new DOMStringImpl("pc");
				case CSS_DEG: return new DOMStringImpl("deg");
				case CSS_RAD: return new DOMStringImpl("rad");
				case CSS_GRAD: return new DOMStringImpl("grad");
				case CSS_MS: return new DOMStringImpl("ms");
				case CSS_S: return new DOMStringImpl("s");
				case CSS_HZ: return new DOMStringImpl("hz");
				case CSS_KHZ: return new DOMStringImpl("khz");
				case CSS_PERCENTAGE: return new DOMStringImpl("%");
				default: break;
			}

			return new DOMStringImpl("");
		}

		static DOMStringImpl *stringForListStyleType(EListStyleType type)
		{
			switch(type)
			{
				case LT_DISC: return new DOMStringImpl("disc");
				case LT_CIRCLE: return new DOMStringImpl("circle");
				case LT_SQUARE: return new DOMStringImpl("square");
				case LT_BOX: return new DOMStringImpl("box");
				case LT_DIAMOND: return new DOMStringImpl("-khtml-diamond");
				case LT_DECIMAL: return new DOMStringImpl("decimal");
				case LT_DECIMAL_LEADING_ZERO: return new DOMStringImpl("decimal-leading-zero");
				case LT_ARABIC_INDIC: return new DOMStringImpl("-khtml-arabic-indic");
				case LT_PERSIAN: return new DOMStringImpl("-khtml-persian");
				case LT_URDU: return new DOMStringImpl("-khtml-urdu");
				case LT_LOWER_ROMAN: return new DOMStringImpl("lower-roman");
				case LT_UPPER_ROMAN: return new DOMStringImpl("upper-roman");
				case LT_HEBREW: return new DOMStringImpl("hebrew");
				case LT_ARMENIAN: return new DOMStringImpl("armenian");
				case LT_GEORGIAN: return new DOMStringImpl("georgian");
				case LT_CJK_IDEOGRAPHIC: return new DOMStringImpl("cjk-ideographic");
				case LT_LOWER_GREEK: return new DOMStringImpl("lower-greek");
				case LT_UPPER_GREEK: return new DOMStringImpl("-khtml-upper-greek");
				case LT_LOWER_ALPHA: return new DOMStringImpl("lower-alpha");
				case LT_UPPER_ALPHA: return new DOMStringImpl("upper-alpha");
				case LT_LOWER_LATIN: return new DOMStringImpl("lower-latin");
				case LT_UPPER_LATIN: return new DOMStringImpl("upper-latin");
				case LT_HIRAGANA: return new DOMStringImpl("hiragana");
				case LT_KATAKANA: return new DOMStringImpl("katakana");
				case LT_HIRAGANA_IROHA: return new DOMStringImpl("hiragana-iroha");
				case LT_KATAKANA_IROHA: return new DOMStringImpl("katakana_iroha");
				case LT_OPEN_QUOTE: return new DOMStringImpl("-khtml-open-quote");
				case LT_CLOSE_QUOTE: return new DOMStringImpl("-khtml-close-quote");
				case LT_NONE: return new DOMStringImpl("none");
				default: Q_ASSERT(0);
			}
		
			return 0;
		}
	};
};

#endif

// vim:ts=4:noet
