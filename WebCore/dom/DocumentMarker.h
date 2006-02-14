/*
 * This file is part of the DOM implementation for WebCore.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
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
 */

#ifndef DOM_DocumentMarker_h
#define DOM_DocumentMarker_h

namespace WebCore {

// A range of a node within a document that is "marked", such as being misspelled
struct DocumentMarker
{
    enum MarkerType {
        AllMarkers = -1,
        Spelling = 0,
        TextMatch,
    };
    
    enum MarkerType type;
    unsigned startOffset, endOffset;
    
    bool operator == (const DocumentMarker &o) const {
        return type == o.type && startOffset == o.startOffset && endOffset == o.endOffset;
    }
    bool operator != (const DocumentMarker &o) const {
        return !(*this == o);
    }
};

}

#endif
