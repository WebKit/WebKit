/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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

#include "PseudoElementIdentifier.h"
#include "StyleExtractorState.h"
#include <span>
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSPrimitiveValue;
class CSSValue;
class CSSValuePool;
class Element;
class MutableStyleProperties;
class Node;
class RenderElement;
class RenderStyle;

enum CSSPropertyID : uint16_t;
enum CSSValueID : uint16_t;

namespace CSS {
struct SerializationContext;
}

namespace Style {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(Extractor);
class Extractor {
public:
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Extractor, Extractor);
public:
    Extractor(Node*, bool allowVisitedStyle = false);
    Extractor(Node*, bool allowVisitedStyle, const std::optional<Style::PseudoElementIdentifier>&);
    Extractor(Element*, bool allowVisitedStyle = false);
    Extractor(Element*, bool allowVisitedStyle, const std::optional<Style::PseudoElementIdentifier>&);

    enum class UpdateLayout : bool { No, Yes };

    // Extract a CSSValue for the specified property.
    RefPtr<CSSValue> propertyValue(CSSPropertyID, UpdateLayout = UpdateLayout::Yes, ExtractorState::PropertyValueType = ExtractorState::PropertyValueType::Resolved) const;

    // Extract a serialized value for the specified property.
    String propertyValueSerialization(CSSPropertyID, const CSS::SerializationContext&, UpdateLayout = UpdateLayout::Yes, ExtractorState::PropertyValueType = ExtractorState::PropertyValueType::Resolved) const;

    // Extract a CSSValue for the specified property using the provided RenderStyle and RenderElement.
    RefPtr<CSSValue> propertyValueInStyle(const RenderStyle&, CSSPropertyID, CSSValuePool&, const RenderElement* = nullptr, ExtractorState::PropertyValueType = ExtractorState::PropertyValueType::Resolved) const;

    // Extract a serialized value for the specified property using the provided RenderStyle and RenderElement.
    String propertyValueSerializationInStyle(const RenderStyle&, CSSPropertyID, const CSS::SerializationContext&, CSSValuePool&, const RenderElement* = nullptr, ExtractorState::PropertyValueType = ExtractorState::PropertyValueType::Resolved) const;

    // Extract a CSSValue for the specified custom property.
    RefPtr<CSSValue> customPropertyValue(const AtomString& propertyName) const;

    // Extract a serialized value for the specified custom property.
    String customPropertyValueSerialization(const AtomString& propertyName, const CSS::SerializationContext&) const;

    // Extract a serialized value for the specified custom property using the provided RenderStyle and RenderElement.
    String customPropertyValueSerializationInStyle(const RenderStyle&, const AtomString& propertyName, const CSS::SerializationContext&) const;

    // Helper methods for HTML editing.
    Ref<MutableStyleProperties> copyProperties(std::span<const CSSPropertyID>) const;
    Ref<MutableStyleProperties> copyProperties() const;
    RefPtr<CSSPrimitiveValue> getFontSizeCSSValuePreferringKeyword() const;
    bool useFixedFontDefaultSize() const;
    bool propertyMatches(CSSPropertyID, const CSSValue*) const;
    bool propertyMatches(CSSPropertyID, CSSValueID) const;

    static bool updateStyleIfNeededForProperty(Element&, CSSPropertyID);

private:
    // The renderer we should use for resolving layout-dependent properties.
    const RenderElement* computeRenderer() const;

    // The RenderStyle we should use for resolving non-layout-dependent properties.
    const RenderStyle* computeStyle(CSSPropertyID, UpdateLayout, std::unique_ptr<RenderStyle>&) const;

    // The RenderStyle we should use for resolving custom properties.
    const RenderStyle* computeStyleForCustomProperty(std::unique_ptr<RenderStyle>&) const;

    RefPtr<Element> m_element;
    std::optional<Style::PseudoElementIdentifier> m_pseudoElementIdentifier;
    bool m_allowVisitedStyle;
};

} // namespace Style
} // namespace WebCore
