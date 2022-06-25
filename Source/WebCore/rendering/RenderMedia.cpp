/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#include "RenderFragmentedFlow.h"
#include "RenderView.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderMedia);

RenderMedia::RenderMedia(HTMLMediaElement& element, RenderStyle&& style)
    : RenderImage(element, WTFMove(style))
{
    setHasShadowControls(true);
}

RenderMedia::RenderMedia(HTMLMediaElement& element, RenderStyle&& style, const IntSize& intrinsicSize)
    : RenderImage(element, WTFMove(style))
{
    setIntrinsicSize(intrinsicSize);
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
        mediaElement().layoutSizeChanged();
}

void RenderMedia::styleDidChange(StyleDifference difference, const RenderStyle* oldStyle)
{
    RenderImage::styleDidChange(difference, oldStyle);
    if (!oldStyle || style().visibility() != oldStyle->visibility())
        mediaElement().visibilityDidChange();
}

} // namespace WebCore

#endif
