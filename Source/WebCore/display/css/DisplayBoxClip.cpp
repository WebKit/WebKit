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

#include "config.h"
#include "DisplayBoxClip.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "RenderStyle.h"

namespace WebCore {
namespace Display {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(BoxClip);

BoxClip::BoxClip() = default;
BoxClip::~BoxClip() = default;

BoxClip::BoxClip(const BoxClip& other)
    : m_clipRect(other.clipRect())
    , m_clipStack(other.clipStack())
    , m_affectedByBorderRadius(other.affectedByBorderRadius())
{
}

Ref<BoxClip> BoxClip::copy() const
{
    return adoptRef(*new BoxClip(*this));
}

void BoxClip::pushClip(const UnadjustedAbsoluteFloatRect& rect)
{
    if (m_clipRect) {
        m_clipRect->intersect(rect);
        return;
    }
    
    m_clipRect = rect;
}

void BoxClip::pushRoundedClip(const FloatRoundedRect& roundedRect)
{
    ASSERT(roundedRect.isRounded());
    pushClip(UnadjustedAbsoluteFloatRect { roundedRect.rect() });

    m_affectedByBorderRadius = true;
    m_clipStack.append(roundedRect);
}



} // namespace Display
} // namespace WebCore

#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
