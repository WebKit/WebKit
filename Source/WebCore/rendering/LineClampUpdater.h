/**
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "RenderLayoutState.h"
#include <wtf/CheckedRef.h>
#include <wtf/Variant.h>

namespace WebCore {

class RenderBlock;

class LineClampUpdater {
public:
    LineClampUpdater(const RenderBlock& blockContainer);
    ~LineClampUpdater();

    bool isLineClampRoot() const { return m_isLineClampRoot; }
    bool isAutoLineClampRoot() const;
    // The clamp point is either after this many lines, or between this block and its next sibling (with no line box right before it).
    using AutoClampPoint = Variant<size_t, CheckedRef<const RenderBox>>;
    // The possible auto clamp points in render tree order (none when the content fits).
    Vector<AutoClampPoint> autoClampPoints() const;
    void setAutoClampPoint(const AutoClampPoint&);
    bool fitsAutoClampBlockSizeLimit() const;
    bool hasConstrainedTextWrapContent() const;
    void setIsStableTextWrap(bool);
    void setMaximumLines(size_t);
    void setClampAfterBox(const RenderBox&);
    void resetLineClamp();

    // Whether content laid out now comes after the clamp point (the line-clamp budget is used up).
    static bool isAfterClampPoint(const RenderObject&);
    static bool isInsideLineClampContainer(const RenderObject&);
    // Floats of a line-clamp container are clipped to its block-end content edge (and nowhere else). The clip rect is in the renderer's coordinates.
    static std::optional<LayoutRect> blockEndClipRect(const RenderBox& floatBox, const RenderElement&);
    static void setIsForcedHidden(RenderElement&, bool);
    static void skipLayoutForForcedHidden(RenderBox&);

private:
    const CheckedRef<const RenderBlock> m_blockContainer;
    bool m_isLineClampRoot { false };
    std::optional<RenderLayoutState::LineClamp> m_previousLineClamp { };
    std::optional<RenderLayoutState::LegacyLineClamp> m_skippedLegacyLineClampToRestore { };
};

}
