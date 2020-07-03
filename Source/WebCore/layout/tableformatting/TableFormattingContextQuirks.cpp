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
#include "TableFormattingContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutBox.h"
#include "LayoutContainerBox.h"
#include "LayoutState.h"

namespace WebCore {
namespace Layout {

bool TableFormattingContext::Quirks::shouldIgnoreChildContentVerticalMargin(const ContainerBox& cellBox) const
{
    // Normally BFC root content height takes the margin box of the child content as vertical margins don't collapse with BFC roots,
    // but table cell boxes do collapse their (non-existing) margins with child quirk margins (so much quirk), so here we check
    // if the content height should include margins or not.
    // e.g <table><tr><td><p>text content</td></tr></table> <- <p>'s quirk margin collapses with the <td> so its content
    // height should not include vertical margins.
    if (!layoutState().inQuirksMode())
        return false;
    if (cellBox.establishesInlineFormattingContext())
        return false;
    if (!cellBox.hasInFlowChild())
        return false;
    return cellBox.firstInFlowChild()->style().hasMarginBeforeQuirk() || cellBox.lastInFlowChild()->style().hasMarginAfterQuirk();
}

}
}

#endif
