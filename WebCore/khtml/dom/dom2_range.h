/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Gunnstein Lye (gunnstein@netcom.no)
 * (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2004 Apple Computer, Inc.
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
#ifndef _dom2_range_h_
#define _dom2_range_h_

#include <dom/dom_node.h>

namespace DOM {

// Introduced in DOM Level 2:
class RangeException {
public:

    /**
     * An integer indicating the type of error generated.
     *
     */
    enum RangeExceptionCode {
        BAD_BOUNDARYPOINTS_ERR         = 1,
        INVALID_NODE_TYPE_ERR          = 2,
        _EXCEPTION_OFFSET              = 2000,
        _EXCEPTION_MAX                 = 2999
    };
};

class Range
{

public:

    enum CompareHow {
	START_TO_START = 0,
	START_TO_END = 1,
	END_TO_END = 2,
	END_TO_START = 3
    };
};

// Used determine how to interpret the offsets used in DOM Ranges.
inline bool offsetInCharacters(unsigned short type)
{
    switch (type) {
        case Node::ATTRIBUTE_NODE:
        case Node::DOCUMENT_FRAGMENT_NODE:
        case Node::DOCUMENT_NODE:
        case Node::ELEMENT_NODE:
        case Node::ENTITY_REFERENCE_NODE:
            return false;

        case Node::CDATA_SECTION_NODE:
        case Node::COMMENT_NODE:
        case Node::PROCESSING_INSTRUCTION_NODE:
        case Node::TEXT_NODE:
            return true;

        case Node::DOCUMENT_TYPE_NODE:
        case Node::ENTITY_NODE:
        case Node::NOTATION_NODE:
            assert(false); // should never be reached
            return false;
    }

    assert(false); // should never be reached
    return false;
}

} // namespace

#endif
