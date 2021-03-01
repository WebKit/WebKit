/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "DisplayBox.h"
#include "FloatRoundedRect.h"
#include "RectEdges.h"
#include <utility>
#include <wtf/IsoMalloc.h>
#include <wtf/Optional.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class RenderStyle;

namespace Layout {
class Box;
class BoxGeometry;
}

namespace Display {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(BoxClip);
class BoxClip : public RefCounted<BoxClip> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(BoxClip);
public:
    ~BoxClip();

    static Ref<BoxClip> create() { return adoptRef(*new BoxClip); }
    Ref<BoxClip> copy() const;

    Optional<AbsoluteFloatRect> clipRect() const { return m_clipRect; }
    
    bool affectedByBorderRadius() const { return m_affectedByBorderRadius; }
    const Vector<FloatRoundedRect>& clipStack() const { return m_clipStack; }

    void pushClip(const AbsoluteFloatRect&);
    void pushRoundedClip(const FloatRoundedRect&);

private:
    BoxClip();
    BoxClip(const BoxClip&);

    Optional<AbsoluteFloatRect> m_clipRect;
    Vector<FloatRoundedRect> m_clipStack;
    bool m_affectedByBorderRadius { false };
};

} // namespace Display
} // namespace WebCore


#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
