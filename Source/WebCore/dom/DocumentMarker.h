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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef DocumentMarker_h
#define DocumentMarker_h

#include "PlatformString.h"
#include <wtf/Forward.h>

namespace WebCore {

// A range of a node within a document that is "marked", such as the range of a misspelled word.
// It optionally includes a description that could be displayed in the user interface.
// It also optionally includes a flag specifying whether the match is active, which is ignored
// for all types other than type TextMatch.
struct DocumentMarker {
    enum MarkerType {
        Spelling = 1 << 0,
        Grammar = 1 << 1,
        TextMatch = 1 << 2,
        Replacement = 1 << 3,
        CorrectionIndicator = 1 << 4,
        RejectedCorrection = 1 << 5,
        AllMarkers = Spelling | Grammar | TextMatch | Replacement | CorrectionIndicator | RejectedCorrection
    };
    MarkerType type;
    typedef unsigned MarkerTypes;
    unsigned startOffset;
    unsigned endOffset;
    String description;
    bool activeMatch;

    bool operator==(const DocumentMarker& o) const
    {
        return type == o.type && startOffset == o.startOffset && endOffset == o.endOffset;
    }

    bool operator!=(const DocumentMarker& o) const
    {
        return !(*this == o);
    }
};

} // namespace WebCore

#endif // DocumentMarker_h
