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

#include "DisplayStyle.h"
#include "FloatRect.h"
#include <wtf/IsoMalloc.h>
#include <wtf/OptionSet.h>

namespace WebCore {
namespace Display {

class ContainerBox;
class Tree;

// FIXME: Make this a strong type.
using AbsoluteFloatRect = FloatRect;

class Box {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Box);
public:
    enum class TypeFlags : uint8_t {
        BoxModelBox     = 1 << 0,
        ContainerBox    = 1 << 1,
        ImageBox        = 1 << 2,
        TextBox         = 1 << 3,
        LineBreakBox    = 1 << 4, // FIXME: Workaround for webkit.org/b/219335
    };

    Box(Tree&, AbsoluteFloatRect, Style&&, OptionSet<TypeFlags> = { });
    virtual ~Box();

    const Style& style() const { return m_style; }

    AbsoluteFloatRect absoluteBoxRect() const { return m_absoluteBoxRect; }

    virtual AbsoluteFloatRect absolutePaintingExtent() const { return m_absoluteBoxRect; }

    bool isBoxModelBox() const { return m_typeFlags.contains(TypeFlags::BoxModelBox); }
    bool isContainerBox() const { return m_typeFlags.contains(TypeFlags::ContainerBox); }
    bool isImageBox() const { return m_typeFlags.contains(TypeFlags::ImageBox); }
    bool isReplacedBox() const { return m_typeFlags.contains(TypeFlags::ImageBox); /* and other types later. */ }
    bool isTextBox() const { return m_typeFlags.contains(TypeFlags::TextBox); }
    bool isLineBreakBox() const { return m_typeFlags.contains(TypeFlags::LineBreakBox); }

    bool participatesInZOrderSorting() const;

    ContainerBox* parent() const { return m_parent; }
    void setParent(ContainerBox* parent) { m_parent = parent; }

    const Box* nextSibling() const { return m_nextSibling.get(); }
    void setNextSibling(std::unique_ptr<Box>&&);

    void setNeedsDisplay(Optional<AbsoluteFloatRect> subrect = WTF::nullopt);

    virtual String debugDescription() const;

private:
    virtual const char* boxName() const;

    const Tree& m_tree;
    AbsoluteFloatRect m_absoluteBoxRect;
    Style m_style;
    ContainerBox* m_parent { nullptr };
    std::unique_ptr<Box> m_nextSibling;
    OptionSet<TypeFlags> m_typeFlags;
};

} // namespace Display
} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_DISPLAY_BOX(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Display::ToValueTypeName) \
    static bool isType(const WebCore::Display::Box& box) { return box.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
