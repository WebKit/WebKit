/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleVisitedLinkColorData.h"

#include "RenderStyleInlines.h"

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleVisitedLinkColorData);

StyleVisitedLinkColorData::StyleVisitedLinkColorData()
    : background(RenderStyle::initialBackgroundColor())
    // borderLeft
    // borderRight
    // borderTop
    // borderBottom
    // textDecoration
    // outline
{
}

StyleVisitedLinkColorData::StyleVisitedLinkColorData(const StyleVisitedLinkColorData& o)
    : RefCounted<StyleVisitedLinkColorData>()
    , background(o.background)
    , borderLeft(o.borderLeft)
    , borderRight(o.borderRight)
    , borderTop(o.borderTop)
    , borderBottom(o.borderBottom)
    , textDecoration(o.textDecoration)
    , outline(o.outline)
{
}

StyleVisitedLinkColorData::~StyleVisitedLinkColorData() = default;

Ref<StyleVisitedLinkColorData> StyleVisitedLinkColorData::copy() const
{
    return adoptRef(*new StyleVisitedLinkColorData(*this));
}

bool StyleVisitedLinkColorData::operator==(const StyleVisitedLinkColorData& o) const
{
    return background == o.background
        && borderLeft == o.borderLeft
        && borderRight == o.borderRight
        && borderTop == o.borderTop
        && borderBottom == o.borderBottom
        && textDecoration == o.textDecoration
        && outline == o.outline;
}

} // namespace WebCore
