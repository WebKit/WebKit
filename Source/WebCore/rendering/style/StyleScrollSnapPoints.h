/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(CSS_SCROLL_SNAP)

#include "LengthBox.h"
#include "LengthSize.h"
#include "RenderStyleConstants.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

struct ScrollSnapType {
    ScrollSnapAxis axis { ScrollSnapAxis::Both };
    ScrollSnapStrictness strictness { ScrollSnapStrictness::None };
};

inline bool operator==(const ScrollSnapType& a, const ScrollSnapType& b)
{
    return a.axis == b.axis && a.strictness == b.strictness;
}

inline bool operator!=(const ScrollSnapType& a, const ScrollSnapType& b) { return !(a == b); }

class StyleScrollSnapPort : public RefCounted<StyleScrollSnapPort> {
public:
    static Ref<StyleScrollSnapPort> create() { return adoptRef(*new StyleScrollSnapPort); }
    Ref<StyleScrollSnapPort> copy() const;

    ScrollSnapType type;
    LengthBox scrollPadding { 0, 0, 0, 0 };

private:
    StyleScrollSnapPort();
    StyleScrollSnapPort(const StyleScrollSnapPort&);
};

inline bool operator==(const StyleScrollSnapPort& a, const StyleScrollSnapPort& b)
{
    return a.type == b.type && a.scrollPadding == b.scrollPadding;
}

inline bool operator!=(const StyleScrollSnapPort& a, const StyleScrollSnapPort& b) { return !(a == b); }

struct ScrollSnapAlign {
    ScrollSnapAxisAlignType x { ScrollSnapAxisAlignType::None };
    ScrollSnapAxisAlignType y { ScrollSnapAxisAlignType::None };
};

inline bool operator==(const ScrollSnapAlign& a, const ScrollSnapAlign& b)
{
    return a.x == b.x && a.y == b.y;
}

inline bool operator!=(const ScrollSnapAlign& a, const ScrollSnapAlign& b) { return !(a == b); }

class StyleScrollSnapArea : public RefCounted<StyleScrollSnapArea> {
public:
    static Ref<StyleScrollSnapArea> create() { return adoptRef(*new StyleScrollSnapArea); }
    Ref<StyleScrollSnapArea> copy() const;
    bool hasSnapPosition() const { return alignment.x != ScrollSnapAxisAlignType::None || alignment.y != ScrollSnapAxisAlignType::None; }

    ScrollSnapAlign alignment;
    LengthBox scrollSnapMargin { 0, 0, 0, 0 };

private:
    StyleScrollSnapArea();
    StyleScrollSnapArea(const StyleScrollSnapArea&);
};

inline bool operator==(const StyleScrollSnapArea& a, const StyleScrollSnapArea& b)
{
    return a.alignment == b.alignment && a.scrollSnapMargin == b.scrollSnapMargin;
}

inline bool operator!=(const StyleScrollSnapArea& a, const StyleScrollSnapArea& b) { return !(a == b); }

} // namespace WebCore

#endif // ENABLE(CSS_SCROLL_SNAP)
