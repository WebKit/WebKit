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

#include "FormattingConstraints.h"
#include "InlineFormattingConstraints.h"
#include "LayoutIntegrationBoxTree.h"
#include "LayoutState.h"

namespace WebCore {

class RenderBox;
class RenderBlock;
class RenderLineBreak;
class RenderInline;
class RenderTable;
class RenderListItem;
class RenderListMarker;

namespace LayoutIntegration {

class BoxGeometryUpdater {
public:
    BoxGeometryUpdater(BoxTree&, Layout::LayoutState&);

    void setGeometriesForLayout();
    void setGeometriesForIntrinsicWidth(Layout::IntrinsicWidthMode);

    Layout::ConstraintsForInlineContent updateInlineContentConstraints();
    HashMap<const Layout::ElementBox*, LayoutUnit> takeNestedListMarkerOffsets() { return WTFMove(m_nestedListMarkerOffsets); }

private:
    void updateLayoutBoxDimensions(const RenderBox&, std::optional<Layout::IntrinsicWidthMode> = std::nullopt);
    void updateLineBreakBoxDimensions(const RenderLineBreak&);
    void updateInlineBoxDimensions(const RenderInline&, std::optional<Layout::IntrinsicWidthMode> = std::nullopt);
    void updateListMarkerDimensions(const RenderListMarker&, std::optional<Layout::IntrinsicWidthMode> = std::nullopt);

    BoxTree& boxTree() { return *m_boxTree; }
    const BoxTree& boxTree() const { return *m_boxTree; }
    Layout::LayoutState& layoutState() { return *m_layoutState; }
    const Layout::LayoutState& layoutState() const { return *m_layoutState; }
    const Layout::ElementBox& rootLayoutBox() const;
    Layout::ElementBox& rootLayoutBox();

private:
    WeakPtr<BoxTree> m_boxTree;
    WeakPtr<Layout::LayoutState> m_layoutState;
    HashMap<const Layout::ElementBox*, LayoutUnit> m_nestedListMarkerOffsets;
};

}
}

