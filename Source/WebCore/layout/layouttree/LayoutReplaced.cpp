/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "LayoutReplaced.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutBox.h"
#include "RenderStyle.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(Replaced);

Replaced::Replaced(const Box& layoutBox)
    : m_layoutBox(makeWeakPtr(layoutBox))
{
}

bool Replaced::hasIntrinsicWidth() const
{
    return m_intrinsicSize || m_layoutBox->style().logicalWidth().isIntrinsic();
}

bool Replaced::hasIntrinsicHeight() const
{
    return m_intrinsicSize || m_layoutBox->style().logicalHeight().isIntrinsic();
}

bool Replaced::hasIntrinsicRatio() const
{
    if (!hasAspectRatio())
        return false;
    return m_intrinsicSize || m_intrinsicRatio;
}

LayoutUnit Replaced::intrinsicWidth() const
{
    ASSERT(hasIntrinsicWidth());
    if (m_intrinsicSize)
        return m_intrinsicSize->width();
    return m_layoutBox->style().logicalWidth().value();
}

LayoutUnit Replaced::intrinsicHeight() const
{
    ASSERT(hasIntrinsicHeight());
    if (m_intrinsicSize)
        return m_intrinsicSize->height();
    return m_layoutBox->style().logicalHeight().value();
}

LayoutUnit Replaced::intrinsicRatio() const
{
    ASSERT(hasIntrinsicRatio() || (hasIntrinsicWidth() && hasIntrinsicHeight()));
    if (m_intrinsicRatio)
        return *m_intrinsicRatio;
    if (m_intrinsicSize)
        return m_intrinsicSize->width() / m_intrinsicSize->height();
    return 1;
}

bool Replaced::hasAspectRatio() const
{
    return m_layoutBox->isImage() || m_layoutBox->style().aspectRatioType() == AspectRatioType::FromIntrinsic;
}

}
}
#endif
