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

#include "DisplayStyle.h"
#include "FloatRect.h"
#include <wtf/IsoMalloc.h>
#include <wtf/OptionSet.h>

namespace WebCore {
namespace Display {

// FIXME: Make this a strong type.
using AbsoluteFloatRect = FloatRect;

class Box {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Box);
public:
    enum class Flags : uint8_t {
        ContainerBox    = 1 << 0,
        ImageBox        = 1 << 1,
    };

    Box(AbsoluteFloatRect borderBox, Style&&, OptionSet<Flags> = { });
    virtual ~Box() = default;

    const Style& style() const { return m_style; }

    AbsoluteFloatRect borderBoxFrame() const { return m_borderBoxFrame; }

    bool isContainerBox() const { return m_flags.contains(Flags::ContainerBox); }
    bool isImageBox() const { return m_flags.contains(Flags::ImageBox); }
    bool isReplacedBox() const { return m_flags.contains(Flags::ImageBox); /* and other types later. */ }

    const Box* nextSibling() const { return m_nextSibling.get(); }
    void setNextSibling(std::unique_ptr<Box>&&);

    virtual String debugDescription() const;

private:
    AbsoluteFloatRect m_borderBoxFrame;
    Style m_style;
    std::unique_ptr<Box> m_nextSibling;
    OptionSet<Flags> m_flags;
};

} // namespace Display
} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_DISPLAY_BOX(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Display::ToValueTypeName) \
    static bool isType(const WebCore::Display::Box& box) { return box.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
