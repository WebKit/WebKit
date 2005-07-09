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
		static DOMString unitTypeToString(unsigned short unitType)
		{
			switch(unitType)
			{
				case CSS_EMS: return "em";
				case CSS_EXS: return "ex";
				case CSS_PX: return "px";
				case CSS_IN: return "in";
				case CSS_CM: return "cm";
				case CSS_MM: return "mm";
				case CSS_PT: return "pt";
				case CSS_PC: return "pc";
				case CSS_DEG: return "deg";
				case CSS_RAD: return "rad";
				case CSS_GRAD: return "grad";
				case CSS_MS: return "ms";
				case CSS_S: return "s";
				case CSS_HZ: return "hz";
				case CSS_KHZ: return "khz";
				case CSS_PERCENTAGE: return "%";
				default: break;
			}

			return KDOM::DOMString("");
		}

		static DOMString stringForListStyleType(EListStyleType type)
		{
			switch(type)
			{
				case LT_DISC: return "disc";
				case LT_CIRCLE: return "circle";
				case LT_SQUARE: return "square";
				case LT_BOX: return "box";
				case LT_DIAMOND: return "-khtml-diamond";
				case LT_DECIMAL: return "decimal";
				case LT_DECIMAL_LEADING_ZERO: return "decimal-leading-zero";
				case LT_ARABIC_INDIC: return "-khtml-arabic-indic";
				case LT_PERSIAN: return "-khtml-persian";
				case LT_URDU: return "-khtml-urdu";
				case LT_LOWER_ROMAN: return "lower-roman";
				case LT_UPPER_ROMAN: return "upper-roman";
				case LT_HEBREW: return "hebrew";
				case LT_ARMENIAN: return "armenian";
				case LT_GEORGIAN: return "georgian";
				case LT_CJK_IDEOGRAPHIC: return "cjk-ideographic";
				case LT_LOWER_GREEK: return "lower-greek";
				case LT_UPPER_GREEK: return "-khtml-upper-greek";
				case LT_LOWER_ALPHA: return "lower-alpha";
				case LT_UPPER_ALPHA: return "upper-alpha";
				case LT_LOWER_LATIN: return "lower-latin";
				case LT_UPPER_LATIN: return "upper-latin";
				case LT_HIRAGANA: return "hiragana";
				case LT_KATAKANA: return "katakana";
				case LT_HIRAGANA_IROHA: return "hiragana-iroha";
				case LT_KATAKANA_IROHA: return "katakana_iroha";
				case LT_OPEN_QUOTE: return "-khtml-open-quote";
				case LT_CLOSE_QUOTE: return "-khtml-close-quote";
				case LT_NONE: return "none";
				default: Q_ASSERT(0);
			}
		
			return KDOM::DOMString("");
		}
	};
};

#endif

// vim:ts=4:noet
