/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * This file includes excerpts from the Document Object Model (DOM)
 * Level 2 Specification (Style)
 * http://www.w3.org/TR/DOM-Level-2-Style/
 * Copyright © 2000 W3C® (MIT, INRIA, Keio), All Rights Reserved.
 *
 */
#ifndef _CSS_css_value_h_
#define _CSS_css_value_h_

#include <dom/dom_string.h>

#include <qcolor.h>

namespace DOM {

class CSSStyleDeclarationImpl;
class CSSRule;
class CSSValue;

/**
 * The <code> CSSValue </code> interface represents a simple or a
 * complexe value.
 *
 */
class CSSValue
{

public:

    /**
     * An integer indicating which type of unit applies to the value.
     *
     *  All CSS2 constants are not supposed to be required by the
     * implementation since all CSS2 interfaces are optionals.
     *
     */
    enum UnitTypes {
	CSS_INHERIT = 0,
        CSS_PRIMITIVE_VALUE = 1,
        CSS_VALUE_LIST = 2,
        CSS_CUSTOM = 3,
        CSS_INITIAL = 4
    };
};

/**
 * The <code> CSSPrimitiveValue </code> interface represents a single
 * <a href="http://www.w3.org/TR/REC-CSS2/syndata.html#values"> CSS
 * value </a> . This interface may be used to determine the value of a
 * specific style property currently set in a block or to set a
 * specific style properties explicitly within the block. An instance
 * of this interface can be obtained from the <code>
 * getPropertyCSSValue </code> method of the <code>
 * CSSStyleDeclaration </code> interface.
 *
 */
class CSSPrimitiveValue : public CSSValue
{

public:

    /**
     * An integer indicating which type of unit applies to the value.
     *
     */
    enum UnitTypes {
        CSS_UNKNOWN = 0,
        CSS_NUMBER = 1,
        CSS_PERCENTAGE = 2,
        CSS_EMS = 3,
        CSS_EXS = 4,
        CSS_PX = 5,
        CSS_CM = 6,
        CSS_MM = 7,
        CSS_IN = 8,
        CSS_PT = 9,
        CSS_PC = 10,
        CSS_DEG = 11,
        CSS_RAD = 12,
        CSS_GRAD = 13,
        CSS_MS = 14,
        CSS_S = 15,
        CSS_HZ = 16,
        CSS_KHZ = 17,
        CSS_DIMENSION = 18,
        CSS_STRING = 19,
        CSS_URI = 20,
        CSS_IDENT = 21,
        CSS_ATTR = 22,
        CSS_COUNTER = 23,
        CSS_RECT = 24,
        CSS_RGBCOLOR = 25,
        CSS_PAIR = 26, // We envision this being exposed as a means of getting computed style values for pairs (border-spacing/radius, background-position, etc.)
        CSS_DASHBOARD_REGION = 27, // FIXME: What on earth is this doing as a primitive value? This is insane.
        CSS_HTML_RELATIVE = 255
    };
};
} // namespace

#endif
