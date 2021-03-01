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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "DisplayBox.h"
#include <wtf/RefPtr.h>

namespace WebCore {
namespace Display {

class BoxClip;
class BoxDecorationData;
class BoxRareGeometry;

// A box in the sense of the CSS Box Model.
// This box can draw backgrounds and borders.
DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(BoxModelBox);
class BoxModelBox : public Box {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(BoxModelBox);
    friend class BoxFactory;
public:
    BoxModelBox(Tree&, AbsoluteFloatRect borderBox, Style&&, OptionSet<TypeFlags> = { });
    virtual ~BoxModelBox();

    AbsoluteFloatRect absoluteBorderBoxRect() const { return absoluteBoxRect(); }
    AbsoluteFloatRect absolutePaddingBoxRect() const { return m_paddingBoxRect; }
    AbsoluteFloatRect absoluteContentBoxRect() const { return m_contentBoxRect; }

    AbsoluteFloatRect absolutePaintingExtent() const override;

    const BoxDecorationData* boxDecorationData() const { return m_boxDecorationData.get(); }
    const BoxRareGeometry* rareGeometry() const { return m_boxRareGeometry.get(); }

    const BoxClip* ancestorClip() const { return m_ancestorClip.get(); }
    bool hasAncestorClip() const;

    bool hasBorderRadius() const;
    FloatRoundedRect borderRoundedRect() const;
    FloatRoundedRect innerBorderRoundedRect() const;

    String debugDescription() const override;

private:
    const char* boxName() const override;

    void setAbsolutePaddingBoxRect(const AbsoluteFloatRect& box) { m_paddingBoxRect = box; }
    void setAbsoluteContentBoxRect(const AbsoluteFloatRect& box) { m_contentBoxRect = box; }

    void setBoxDecorationData(std::unique_ptr<BoxDecorationData>&&);
    void setBoxRareGeometry(std::unique_ptr<BoxRareGeometry>&&);

    void setAncestorClip(RefPtr<BoxClip>&&);
    RefPtr<BoxClip> clipForDescendants() const;

    AbsoluteFloatRect m_paddingBoxRect;
    AbsoluteFloatRect m_contentBoxRect;

    std::unique_ptr<BoxDecorationData> m_boxDecorationData;
    std::unique_ptr<BoxRareGeometry> m_boxRareGeometry;
    RefPtr<BoxClip> m_ancestorClip;
};

} // namespace Display
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_DISPLAY_BOX(BoxModelBox, isBoxModelBox())

#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
