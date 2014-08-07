/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef StyleScrollSnapPoints_h
#define StyleScrollSnapPoints_h

#if ENABLE(CSS_SCROLL_SNAP)

#include "Length.h"

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

typedef std::pair<Length, Length> SnapCoordinate;

class StyleScrollSnapPoints : public RefCounted<StyleScrollSnapPoints> {
public:
    static PassRef<StyleScrollSnapPoints> create() { return adoptRef(*new StyleScrollSnapPoints); }
    
    static Length defaultRepeatOffset()
    {
        return Length(100, Percent);
    }
    
    static Length defaultDestinationOffset()
    {
        return Length(0, Fixed);
    }

    PassRef<StyleScrollSnapPoints> copy() const;
    
    bool operator==(const StyleScrollSnapPoints&) const;
    bool operator!=(const StyleScrollSnapPoints& o) const
    {
        return !(*this == o);
    }
    
    Length repeatOffsetX;
    Length repeatOffsetY;
    Length destinationX;
    Length destinationY;
    bool hasRepeatX;
    bool hasRepeatY;
    bool usesElementsX;
    bool usesElementsY;
    Vector<Length> offsetsX;
    Vector<Length> offsetsY;
    Vector<SnapCoordinate> coordinates;
    
private:
    StyleScrollSnapPoints();
    StyleScrollSnapPoints(const StyleScrollSnapPoints&);
};

} // namespace WebCore

#endif /* ENABLE(CSS_SCROLL_SNAP) */

#endif // StyleScrollSnapPoints_h
