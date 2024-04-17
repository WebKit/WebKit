/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "RenderFrameBase.h"

#include "HTMLFrameElementBase.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderView.h"
#include "ScriptDisallowedScope.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderFrameBase);

RenderFrameBase::RenderFrameBase(Type type, HTMLFrameElementBase& element, RenderStyle&& style)
    : RenderWidget(type, element, WTFMove(style))
{
}

inline bool shouldExpandFrame(LayoutUnit width, LayoutUnit height, bool hasFixedWidth, bool hasFixedHeight)
{
    // If the size computed to zero never expand.
    if (!width || !height)
        return false;
    // Really small fixed size frames can't be meant to be scrolled and are there probably by mistake. Avoid expanding.
    static const unsigned smallestUsefullyScrollableDimension = 8;
    if (hasFixedWidth && width < LayoutUnit(smallestUsefullyScrollableDimension))
        return false;
    if (hasFixedHeight && height < LayoutUnit(smallestUsefullyScrollableDimension))
        return false;
    return true;
}

}
