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

#include "FlexFormattingConstraints.h"
#include "FlexFormattingGeometry.h"
#include "FlexFormattingState.h"
#include "FlexLayout.h"
#include "FlexRect.h"
#include "FormattingQuirks.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {
namespace Layout {

// This class implements the layout logic for flex formatting contexts.
// https://www.w3.org/TR/css-flexbox-1/
class FlexFormattingContext final : public FormattingContext {
    WTF_MAKE_ISO_ALLOCATED(FlexFormattingContext);
public:
    FlexFormattingContext(const ElementBox& formattingContextRoot, FlexFormattingState&);

    void layout(const ConstraintsForFlexContent&);
    IntrinsicWidthConstraints computedIntrinsicWidthConstraints();

    const FlexFormattingGeometry& formattingGeometry() const { return m_flexFormattingGeometry; }
    const FormattingQuirks& formattingQuirks() const { return m_flexFormattingQuirks; }

private:
    FlexLayout::LogicalFlexItems convertFlexItemsToLogicalSpace(const ConstraintsForFlexContent&);
    void setFlexItemsGeometry(const FlexLayout::LogicalFlexItems&, const FlexLayout::LogicalFlexItemRects&, const ConstraintsForFlexContent&);

    std::optional<LayoutUnit> computedAutoMarginValueForFlexItems(const ConstraintsForFlexContent&);

    const FlexFormattingState& formattingState() const { return m_flexFormattingState; }
    FlexFormattingState& formattingState() { return m_flexFormattingState; }

private:
    FlexFormattingState& m_flexFormattingState;
    const FlexFormattingGeometry m_flexFormattingGeometry;
    const FormattingQuirks m_flexFormattingQuirks;
};

}
}

SPECIALIZE_TYPE_TRAITS_LAYOUT_FORMATTING_CONTEXT(FlexFormattingContext, isFlexFormattingContext())

