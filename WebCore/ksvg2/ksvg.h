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

#ifndef KSVG_H
#define KSVG_H
#ifdef SVG_SUPPORT

#include "SVGException.h"

/**
 * @short General namespace specific definitions.
 */
namespace WebCore {
    /**
     * All SVG constants
     */
    enum SVGCSSRuleType {
        COLOR_PROFILE_RULE = 7
    };

    enum SVGTextPathMethodType {
        TEXTPATH_METHODTYPE_UNKNOWN    = 0,
        TEXTPATH_METHODTYPE_ALIGN      = 1,
        TEXTPATH_METHODTYPE_STRETCH    = 2
    };

    enum SVGTextPathSpacingType {
        TEXTPATH_SPACINGTYPE_UNKNOWN    = 0,
        TEXTPATH_SPACINGTYPE_AUTO       = 1,
        TEXTPATH_SPACINGTYPE_EXACT      = 2
    };

    enum SVGEdgeModes {
        SVG_EDGEMODE_UNKNOWN   = 0,
        SVG_EDGEMODE_DUPLICATE = 1,
        SVG_EDGEMODE_WRAP      = 2,
        SVG_EDGEMODE_NONE      = 3
    };

    enum SVGMorphologyOperators {
        SVG_MORPHOLOGY_OPERATOR_UNKNOWN = 0,
        SVG_MORPHOLOGY_OPERATOR_ERODE   = 1,
        SVG_MORPHOLOGY_OPERATOR_DILATE  = 2
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif // KSVG_H

// vim:ts=4:noet
