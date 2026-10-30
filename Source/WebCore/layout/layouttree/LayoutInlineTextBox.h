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

#include <WebCore/LayoutBox.h>
#include <wtf/OptionSet.h>

namespace WebCore {

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
    const String& content() const LIFETIME_BOUND { return m_content; }
    bool isCombined() const { return m_isCombined; }
    // FIXME: This should not be a box's property.
    bool canUseSimplifiedContentMeasuring() const { return m_contentCharacteristicSet.contains(ContentCharacteristic::CanUseSimplifiedContentMeasuring); }
    bool canUseSimpleFontCodePath() const { return m_contentCharacteristicSet.contains(ContentCharacteristic::CanUseSimpleFontCodepath); }
    bool shouldUseSimpleGlyphOverflowCodePath() const { return m_contentCharacteristicSet.contains(ContentCharacteristic::ShouldUseSimpleGlyphOverflowCodePath); }
    bool hasPositionDependentContentWidth() const { return m_contentCharacteristicSet.contains(ContentCharacteristic::HasPositionDependentContentWidth); }
    bool hasStrongDirectionalityContent() const { return m_contentCharacteristicSet.contains(ContentCharacteristic::HasStrongDirectionalityContent); }
    bool hasSynthesizedGlyph() const { return m_contentCharacteristicSet.contains(ContentCharacteristic::HasSynthesizedGlyph); }

    // BoxTreeUpdater sets these from the renderer.
    void setContent(String newContent, EnumSet<ContentCharacteristic>);
    void setCanUseSimplifiedContentMeasuring(bool value) { m_contentCharacteristicSet.set(ContentCharacteristic::CanUseSimplifiedContentMeasuring, value); }
    void setIsCombined(bool isCombined) { m_isCombined = isCombined; }

protected:
    explicit InlineTextBox(Style::ComputedStyle&&);
    ~InlineTextBox() = default;

private:
    String m_content;
    bool m_isCombined { false };
    EnumSet<ContentCharacteristic> m_contentCharacteristicSet;
};

inline void InlineTextBox::setContent(String newContent, EnumSet<ContentCharacteristic> contentCharacteristicSet)
{
    m_content = WTF::move(newContent);
    m_contentCharacteristicSet = contentCharacteristicSet;
}

}
}

SPECIALIZE_TYPE_TRAITS_LAYOUT_BOX(InlineTextBox, isInlineTextBox())

