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
#include "TransformOperations.h"
#include <wtf/IsoMalloc.h>
#include <wtf/OptionSet.h>
#include <wtf/Optional.h>

namespace WebCore {

class FillLayer;
class RenderStyle;
class ShadowData;

namespace Display {

// Style information needed to paint a Display::Box.
// All colors should be resolved to their painted values [visitedDependentColorWithColorFilter()].
// Should contain only absolute float values; no Lengths (which can contain calc values).
// Should be sharable between boxes with different geometry but the same style.
class Style {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Style);
public:

    enum class Flags : uint8_t {
        Positioned      = 1 << 0,
        Floating        = 1 << 1,
        HasTransform    = 1 << 2,
    };

    explicit Style(const RenderStyle&);
    explicit Style(const RenderStyle&, const RenderStyle* styleForBackground);

    const Color& color() const { return m_color; }

    const Color& backgroundColor() const { return m_backgroundColor; }
    bool hasBackground() const;
    bool hasBackgroundImage() const;

    const FillLayer* backgroundLayers() const { return m_backgroundLayers.get(); }
    bool backgroundHasOpaqueTopLayer() const;
    
    const ShadowData* boxShadow() const { return m_boxShadow.get(); }

    Optional<int> zIndex() const { return m_zIndex; }
    bool isStackingContext() const { return m_zIndex.hasValue(); }
    
    bool isPositioned() const { return m_flags.contains(Flags::Positioned); }
    bool isFloating() const { return m_flags.contains(Flags::Floating); }

    // Just the transform property (not translate, rotate, scale).
    bool hasTransform() const { return m_flags.contains(Flags::HasTransform); }

    bool participatesInZOrderSorting() const { return isPositioned() || isStackingContext(); }

    const FontCascade& fontCascade() const { return m_fontCascade; }
    const FontMetrics& fontMetrics() const { return m_fontCascade.fontMetrics(); }
    
    float opacity() const { return m_opacity; }

    Overflow overflowX() const;
    Overflow overflowY() const;
    bool hasClippedOverflow() const { return m_overflowX != Overflow::Visible || m_overflowY != Overflow::Visible; }

    WhiteSpace whiteSpace() const { return m_whiteSpace; }
    bool autoWrap() const;
    bool preserveNewline() const;
    bool collapseWhiteSpace() const;

    const TabSize& tabSize() const { return m_tabSize; }

private:
    void setupBackground(const RenderStyle&);

    void setIsPositioned(bool value) { m_flags.set({ Flags::Positioned }, value); }
    void setIsFloating(bool value) { m_flags.set({ Flags::Floating }, value); }
    void setHasTransform(bool value) { m_flags.set({ Flags::HasTransform }, value); }

    Color m_color;
    Color m_backgroundColor;

    RefPtr<FillLayer> m_backgroundLayers;
    std::unique_ptr<ShadowData> m_boxShadow;

    Overflow m_overflowX;
    Overflow m_overflowY;

    FontCascade m_fontCascade;
    WhiteSpace m_whiteSpace;
    TabSize m_tabSize;
    
    float m_opacity;

    Optional<int> m_zIndex;
    OptionSet<Flags> m_flags;
};

} // namespace Display
} // namespace WebCore

#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
