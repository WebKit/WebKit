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
 * Level 2 Specification (Candidate Recommendation)
 * http://www.w3.org/TR/2000/CR-DOM-Level-2-20000510/
 * Copyright © 2000 W3C® (MIT, INRIA, Keio), All Rights Reserved.
 *
 */
#ifndef _CSS_css_rule_h_
#define _CSS_css_rule_h_

namespace DOM {

/**
 * The <code> CSSRule </code> interface is the abstract base interface
 * for any type of CSS <a
 * href="http://www.w3.org/TR/REC-CSS2/syndata.html#q5"> statement
 * </a> . This includes both <a
 * href="http://www.w3.org/TR/REC-CSS2/syndata.html#q8"> rule sets
 * </a> and <a
 * href="http://www.w3.org/TR/REC-CSS2/syndata.html#at-rules">
 * at-rules </a> . An implementation is expected to preserve all rules
 * specified in a CSS style sheet, even if it is not recognized.
 * Unrecognized rules are represented using the <code> CSSUnknownRule
 * </code> interface.
 *
 */
class CSSRule
{

public:

    /**
     * An integer indicating which type of rule this is.
     *
     */
    enum RuleType {
        UNKNOWN_RULE = 0,
        STYLE_RULE = 1,
        CHARSET_RULE = 2,
        IMPORT_RULE = 3,
        MEDIA_RULE = 4,
        FONT_FACE_RULE = 5,
        PAGE_RULE = 6,
        QUIRKS_RULE = 100 // Not part of the official DOM
    };
};
} // namespace

#endif
