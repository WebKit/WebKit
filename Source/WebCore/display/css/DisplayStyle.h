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

#include "BorderValue.h"
#include "Color.h"
#include "FontCascade.h"
#include "NinePieceImage.h"
#include "RenderStyleConstants.h"
#include "TabSize.h"
#include <wtf/IsoMalloc.h>
#include <wtf/OptionSet.h>
#include <wtf/Optional.h>

namespace WebCore {

class RenderStyle;

namespace Display {

// Style information needed to paint a Display::Box.
// All colors should be resolved to their painted values [visitedDependentColorWithColorFilter()].
// Should contain only absolute float values; no Lengths (which can contain calc values).

class Style {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Style);
public:

    enum class Flags : uint8_t {
        Positioned  = 1 << 0,
        Floating    = 1 << 1,
    };

    explicit Style(const RenderStyle&);

    const Color& color() const { return m_color; }

    const Color& backgroundColor() const { return m_backgroundColor; }
    bool hasBackground() const;
    bool hasBackgroundImage() const { return false; } // FIXME

    bool hasVisibleBorder() const;
    const BorderValue& borderLeft() const { return m_border.left; }
    const BorderValue& borderRight() const { return m_border.right; }
    const BorderValue& borderTop() const { return m_border.top; }
    const BorderValue& borderBottom() const { return m_border.bottom; }

    Optional<int> zIndex() const { return m_zIndex; }
    bool isStackingContext() const { return m_zIndex.hasValue(); }
    
    bool isPositioned() const { return m_flags.contains(Flags::Positioned); }
    bool isFloating() const { return m_flags.contains(Flags::Floating); }

    bool participatesInZOrderSorting() const { return isPositioned() || isStackingContext(); }

    const FontCascade& fontCascade() const { return m_fontCascade; }
    const FontMetrics& fontMetrics() const { return m_fontCascade.fontMetrics(); }

    WhiteSpace whiteSpace() const { return m_whiteSpace; }
    bool autoWrap() const;
    bool preserveNewline() const;
    bool collapseWhiteSpace() const;

    const TabSize& tabSize() const { return m_tabSize; }

private:
    void setIsPositioned(bool value) { m_flags.set({ Flags::Positioned }, value); }
    void setIsFloating(bool value) { m_flags.set({ Flags::Floating }, value); }

    Color m_color;
    Color m_backgroundColor;

    struct {
        BorderValue left;
        BorderValue right;
        BorderValue top;
        BorderValue bottom;

        NinePieceImage image;
    } m_border;

    FontCascade m_fontCascade;
    WhiteSpace m_whiteSpace;
    TabSize m_tabSize;

    Optional<int> m_zIndex;
    OptionSet<Flags> m_flags;
};

} // namespace Display
} // namespace WebCore

#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
