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

#pragma once

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutUnit.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

namespace Layout {

class Box;

// This class implements margin collapsing for block formatting context.
class BlockMarginCollapse {
    WTF_MAKE_ISO_ALLOCATED(BlockMarginCollapse);
public:
    BlockMarginCollapse();

    LayoutUnit marginTop(const Box&) const;
    LayoutUnit marginBottom(const Box&) const;

private:
    bool isMarginTopCollapsedWithSibling(const Box&) const;
    bool isMarginBottomCollapsedWithSibling(const Box&) const;
    bool isMarginTopCollapsedWithParent(const Box&) const;
    bool isMarginBottomCollapsedWithParent(const Box&) const;

    LayoutUnit nonCollapsedMarginTop(const Box&) const;
    LayoutUnit nonCollapsedMarginBottom(const Box&) const;
    LayoutUnit collapsedMarginTopFromFirstChild(const Box&) const;
    LayoutUnit collapsedMarginBottomFromLastChild(const Box&) const;

    bool hasAdjoiningMarginTopAndBottom(const Box&) const;
};

}
}
#endif
