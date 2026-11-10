/*
 * Copyright (C) 2020-2026 Apple Inc. All rights reserved.
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

#include <WebCore/FontCascade.h>
#include <WebCore/LayoutBox.h>
#include <wtf/OptionSet.h>

namespace WebCore {

enum class SynthesizedGlyph : uint8_t;

namespace Layout {

class InlineTextBox : public Box {
public:
    enum class ContentCharacteristic : uint8_t {
        CanUseSimplifiedContentMeasuring,
        CanUseSimpleFontCodepath,
        ShouldUseSimpleGlyphOverflowCodePath,
        HasPositionDependentContentWidth,
        HasStrongDirectionalityContent,
        HasSynthesizedGlyph, // A list marker's disc, circle or square: the text is the character the counter style produced.
        HighestEnumValue = HasSynthesizedGlyph
    };
    const RefPtr<StringImpl>& content() const LIFETIME_BOUND { return m_content; }

    // text-combine-upright content that fits in 1em is drawn horizontally with a font of its own.
    struct CombinedText {
        WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(CombinedText);

        FontCascade font;
        float width { 0 };
        float ascent { 0 };
        float descent { 0 };
    };
    bool isCombined() const { return m_isCombinedText; }
    NODELETE const CombinedText& combinedText() const LIFETIME_BOUND;
    // Text in a display: contents element has a style of its own instead of inheriting from the parent box.
    bool inheritsStyleFromDisplayContentsParent() const { return m_textInheritsStyleFromDisplayContentsParent; }
    // FIXME: This should not be a box's property.
    bool canUseSimplifiedContentMeasuring() const { return contentCharacteristics().contains(ContentCharacteristic::CanUseSimplifiedContentMeasuring); }
    bool canUseSimpleFontCodePath() const { return contentCharacteristics().contains(ContentCharacteristic::CanUseSimpleFontCodepath); }
    bool shouldUseSimpleGlyphOverflowCodePath() const { return contentCharacteristics().contains(ContentCharacteristic::ShouldUseSimpleGlyphOverflowCodePath); }
    bool hasPositionDependentContentWidth() const { return contentCharacteristics().contains(ContentCharacteristic::HasPositionDependentContentWidth); }
    bool hasStrongDirectionalityContent() const { return contentCharacteristics().contains(ContentCharacteristic::HasStrongDirectionalityContent); }
    bool hasSynthesizedGlyph() const { return contentCharacteristics().contains(ContentCharacteristic::HasSynthesizedGlyph); }
    // A glyph from 'content' (e.g. ::picker-icon) that the UA draws itself rather than rendering the text.
    std::optional<SynthesizedGlyph> contentGlyph() const;
    static constexpr float contentGlyphAdvanceRatio = 0.52f;

    // BoxTreeUpdater sets these from the renderer.
    void setContent(RefPtr<StringImpl> newContent, EnumSet<ContentCharacteristic>);
    void setCanUseSimplifiedContentMeasuring(bool);
    void setCombinedText(std::unique_ptr<CombinedText>&&);

protected:
    InlineTextBox(Style::ComputedStyle&&, std::optional<SynthesizedGlyph>);
    ~InlineTextBox();

    void setInheritsStyleFromDisplayContentsParent(bool value) { m_textInheritsStyleFromDisplayContentsParent = value; }

private:
    EnumSet<ContentCharacteristic> contentCharacteristics() const { return EnumSet<ContentCharacteristic>::fromRaw(m_textContentCharacteristics); }

    // The rest of the state is in Box bits.
    RefPtr<StringImpl> m_content;
};

inline std::optional<SynthesizedGlyph> InlineTextBox::contentGlyph() const
{
    if (!m_textContentGlyph)
        return { };
    return static_cast<SynthesizedGlyph>(m_textContentGlyph - 1);
}

inline void InlineTextBox::setContent(RefPtr<StringImpl> newContent, EnumSet<ContentCharacteristic> contentCharacteristicSet)
{
    m_content = WTF::move(newContent);
    m_textContentCharacteristics = contentCharacteristicSet.toRaw();
}

inline void InlineTextBox::setCanUseSimplifiedContentMeasuring(bool value)
{
    auto contentCharacteristicSet = contentCharacteristics();
    contentCharacteristicSet.set(ContentCharacteristic::CanUseSimplifiedContentMeasuring, value);
    m_textContentCharacteristics = contentCharacteristicSet.toRaw();
}

}
}

SPECIALIZE_TYPE_TRAITS_LAYOUT_BOX(InlineTextBox, isInlineTextBox())

