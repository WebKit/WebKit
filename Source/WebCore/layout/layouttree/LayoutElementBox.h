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

#include <WebCore/CachedImage.h>
#include <WebCore/LayoutBox.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class RenderElement;
class RenderStyle;

namespace Layout {

class ElementBox : public Box {
    WTF_MAKE_TZONE_ALLOCATED(ElementBox);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ElementBox);
public:
    ElementBox(ElementAttributes&&, RenderStyle&&, std::unique_ptr<RenderStyle>&& firstLineStyle = nullptr, EnumSet<BaseTypeFlag> = { ElementBoxFlag });

    enum class ListMarkerAttribute : bool {
        Image,
        Outside,
    };
    ElementBox(ElementAttributes&&, EnumSet<ListMarkerAttribute>, RenderStyle&&, std::unique_ptr<RenderStyle>&& firstLineStyle = nullptr);

    struct ReplacedAttributes {
        LayoutSize intrinsicSize;
        std::optional<LayoutUnit> intrinsicRatio { };
        WeakPtr<CachedImage> cachedImage { };
    };
    ElementBox(ElementAttributes&&, ReplacedAttributes&&, RenderStyle&&, std::unique_ptr<RenderStyle>&& firstLineStyle = nullptr);

    ~ElementBox();

    const Box* firstChild() const { return m_firstChild.get(); }
    const Box* NODELETE firstInFlowChild() const;
    const Box* NODELETE firstInFlowOrFloatingChild() const;
    const Box* NODELETE firstOutOfFlowChild() const;
    const Box* lastChild() const { return m_lastChild.get(); }
    const Box* NODELETE lastInFlowChild() const;
    const Box* NODELETE lastInFlowOrFloatingChild() const;
    const Box* NODELETE lastOutOfFlowChild() const;

    // FIXME: This is currently needed for style updates.
    Box* firstChild() { return m_firstChild.get(); }

    bool hasChild() const { return firstChild(); }
    bool hasInFlowChild() const { return firstInFlowChild(); }
    bool hasInFlowOrFloatingChild() const { return firstInFlowOrFloatingChild(); }
    bool NODELETE hasOutOfFlowChild() const;

    void appendChild(UniqueRef<Box>);
    void insertChild(UniqueRef<Box>, Box* beforeChild = nullptr);
    void destroyChildren();

    void setBaselineForIntegration(LayoutUnit baseline) { m_baselineForIntegration = baseline; }
    std::optional<LayoutUnit> baselineForIntegration() const { return m_baselineForIntegration; }

    bool NODELETE hasIntrinsicWidth() const;
    bool NODELETE hasIntrinsicHeight() const;
    bool NODELETE hasIntrinsicRatio() const;
    LayoutUnit NODELETE intrinsicWidth() const;
    LayoutUnit NODELETE intrinsicHeight() const;
    LayoutUnit NODELETE intrinsicRatio() const;
    bool NODELETE hasAspectRatio() const;

    void setListMarkerAttributes(EnumSet<ListMarkerAttribute> listMarkerAttributes) { m_replacedData->listMarkerAttributes = listMarkerAttributes; }

    bool isListMarkerImage() const { return m_replacedData && m_replacedData->listMarkerAttributes.contains(ListMarkerAttribute::Image); }
    bool isListMarkerOutside() const { return m_replacedData && m_replacedData->listMarkerAttributes.contains(ListMarkerAttribute::Outside); }

    // FIXME: This is temporary until after list marker content is accessible by IFC (webkit.org/b/294342)
    void setListMarkerLayoutBounds(std::pair<float, float> layoutBounds) { m_replacedData->layoutBounds = layoutBounds; }
    std::pair<float, float> layoutBoundsForListMarker() const { return m_replacedData ? m_replacedData->layoutBounds : std::pair<float, float>(); }

    // FIXME: This doesn't belong.
    CachedImage* cachedImage() const { return m_replacedData ? m_replacedData->cachedImage : nullptr; }

    RenderElement* NODELETE rendererForIntegration() const;

private:
    friend class Box;

    struct ReplacedData {
        WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(ReplacedData);

        EnumSet<ListMarkerAttribute> listMarkerAttributes;
        std::pair<float, float> layoutBounds;

        std::optional<LayoutSize> intrinsicSize;
        std::optional<LayoutUnit> intrinsicRatio;
        WeakPtr<CachedImage> cachedImage;
    };

    std::unique_ptr<Box> m_firstChild;
    CheckedPtr<Box> m_lastChild;

    std::unique_ptr<ReplacedData> m_replacedData;
    std::optional<LayoutUnit> m_baselineForIntegration;
};

}
}

SPECIALIZE_TYPE_TRAITS_LAYOUT_BOX(ElementBox, isElementBox())

