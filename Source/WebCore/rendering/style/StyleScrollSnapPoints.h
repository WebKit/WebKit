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

#include "LengthSize.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

struct ScrollSnapPoints {
    Length repeatOffset;
    bool hasRepeat;
    bool usesElements;
    Vector<Length> offsets;

    ScrollSnapPoints();
};

bool operator==(const ScrollSnapPoints&, const ScrollSnapPoints&);
inline bool operator!=(const ScrollSnapPoints& a, const ScrollSnapPoints& b) { return !(a == b); }

LengthSize defaultScrollSnapDestination();

class StyleScrollSnapPoints : public RefCounted<StyleScrollSnapPoints> {
public:
    static Ref<StyleScrollSnapPoints> create() { return adoptRef(*new StyleScrollSnapPoints); }
    Ref<StyleScrollSnapPoints> copy() const;

    std::unique_ptr<ScrollSnapPoints> xPoints;
    std::unique_ptr<ScrollSnapPoints> yPoints;
    LengthSize destination;
    Vector<LengthSize> coordinates;

private:
    StyleScrollSnapPoints();
    StyleScrollSnapPoints(const StyleScrollSnapPoints&);
};

bool operator==(const StyleScrollSnapPoints&, const StyleScrollSnapPoints&);
inline bool operator!=(const StyleScrollSnapPoints& a, const StyleScrollSnapPoints& b) { return !(a == b); }

} // namespace WebCore

#endif // ENABLE(CSS_SCROLL_SNAP)

#endif // StyleScrollSnapPoints_h
