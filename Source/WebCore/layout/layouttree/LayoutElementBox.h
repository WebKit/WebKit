/*
 * Copyright (C) 2018-2026 Apple Inc. All rights reserved.
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

#include <WebCore/CachedImage.h>
#include <WebCore/LayoutBox.h>

namespace WebCore {

class RenderElement;

namespace Style {
class ComputedStyle;
}

namespace Layout {

class ElementBox : public Box {
public:
    enum class IsListMarkerImage : bool { No, Yes };

    // The render tree children, without excluded list markers. RenderView's box is the only child of the initial containing block.
    // Defined in RenderElement.h.
    inline const Box* firstChild() const;
    const Box* LEGACY_NODELETE firstInFlowChild() const;
    const Box* LEGACY_NODELETE firstInFlowOrFloatingChild() const;
    const Box* LEGACY_NODELETE firstOutOfFlowChild() const;
    inline const Box* lastChild() const;
    const Box* LEGACY_NODELETE lastInFlowChild() const;
    const Box* LEGACY_NODELETE lastInFlowOrFloatingChild() const;
    const Box* LEGACY_NODELETE lastOutOfFlowChild() const;

    bool hasChild() const { return firstChild(); }
    bool hasInFlowChild() const { return firstInFlowChild(); }
    bool hasInFlowOrFloatingChild() const { return firstInFlowOrFloatingChild(); }
    bool LEGACY_NODELETE hasOutOfFlowChild() const;

    void setBaselineForIntegration(LayoutUnit baseline) { m_baselineForIntegration = baseline; }
    std::optional<LayoutUnit> baselineForIntegration() const { return m_baselineForIntegration; }

    bool LEGACY_NODELETE hasIntrinsicWidth() const;
    bool LEGACY_NODELETE hasIntrinsicHeight() const;
    bool LEGACY_NODELETE hasIntrinsicRatio() const;
    bool LEGACY_NODELETE hasNaturalWidth() const;
    bool LEGACY_NODELETE hasNaturalHeight() const;
    LayoutUnit LEGACY_NODELETE naturalWidth() const;
    LayoutUnit LEGACY_NODELETE naturalHeight() const;
    LayoutUnit LEGACY_NODELETE intrinsicWidth() const;
    LayoutUnit LEGACY_NODELETE intrinsicHeight() const;
    LayoutUnit LEGACY_NODELETE intrinsicRatio() const;
    bool LEGACY_NODELETE hasAspectRatio() const;

    void setIsListMarkerImage(IsListMarkerImage);

    bool isListMarkerImage() const { return m_replacedData && m_replacedData->isListMarkerImage; }

    // FIXME: This is temporary until after list marker content is accessible by IFC (webkit.org/b/294342)
    void setListMarkerLayoutBounds(std::pair<float, float> layoutBounds) { m_replacedData->layoutBounds = layoutBounds; }
    std::pair<float, float> layoutBoundsForListMarker() const { return m_replacedData ? m_replacedData->layoutBounds : std::pair<float, float>(); }

    // FIXME: This doesn't belong.
    CachedImage* cachedImage() const { return m_replacedData ? m_replacedData->cachedImage : nullptr; }

    // Null only for the initial containing block. Defined in RenderElement.h.
    inline RenderElement* rendererForIntegration() const;

protected:
    explicit ElementBox(Style::ComputedStyle&&, EnumSet<BaseTypeFlag> = { });
    ~ElementBox();

private:
    friend class Box;

    const Box& LEGACY_NODELETE initialContainingBlockChild() const;

    struct ReplacedData {
        WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(ReplacedData);

        bool isListMarkerImage { false };
        std::pair<float, float> layoutBounds;

        std::optional<LayoutSize> intrinsicSize;
        std::optional<LayoutUnit> intrinsicRatio;
        WeakPtr<CachedImage> cachedImage;
    };

    std::unique_ptr<ReplacedData> m_replacedData;
    std::optional<LayoutUnit> m_baselineForIntegration;
};

}
}

SPECIALIZE_TYPE_TRAITS_LAYOUT_BOX(ElementBox, isElementBox())

// For the inline functions that read the renderer. RenderElement.h includes this header first.
#include <WebCore/RenderElement.h>
