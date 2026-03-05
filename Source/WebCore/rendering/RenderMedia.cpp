/*
 * Copyright (C) 2007-2025 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO)
#include "RenderMedia.h"

#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderFragmentedFlow.h"
#include "RenderMediaInlines.h"
#include "RenderView.h"
#include <wtf/StackStats.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderMedia);

RenderMedia::RenderMedia(Type type, HTMLMediaElement& element, RenderStyle&& style)
    : RenderImage(type, element, WTF::move(style), ReplacedFlag::IsMedia)
{
    setHasShadowControls(true);
}

RenderMedia::~RenderMedia() = default;

void RenderMedia::paintReplaced(PaintInfo&, const LayoutPoint&)
{
}

void RenderMedia::layout()
{
    LayoutSize oldSize = size();
    RenderImage::layout();
    if (oldSize != size())
        protect(mediaElement())->layoutSizeChanged();
}

void RenderMedia::styleDidChange(Style::Difference difference, const RenderStyle* oldStyle)
{
    RenderImage::styleDidChange(difference, oldStyle);
    if (!oldStyle || style().usedVisibility() != oldStyle->usedVisibility())
        protect(mediaElement())->visibilityDidChange();

    if (!oldStyle || style().dynamicRangeLimit() != oldStyle->dynamicRangeLimit())
        protect(mediaElement())->dynamicRangeLimitDidChange(style().dynamicRangeLimit().toPlatformDynamicRangeLimit());
}

} // namespace WebCore

#endif
