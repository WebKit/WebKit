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

#include "CSSToLengthConversionData.h"
#include "CSSToStyleMap.h"
#include "CascadeLevel.h"
#include "PropertyCascade.h"
#include "RuleSet.h"
#include "SelectorChecker.h"
#include <wtf/BitSet.h>

namespace WebCore {

class FilterOperations;
class FontCascadeDescription;
class RenderStyle;
class StyleImage;
class StyleResolver;

namespace Style {

class Builder;
class BuilderState;

void maybeUpdateFontForLetterSpacing(BuilderState&, CSSValue&);

enum class ForVisitedLink : bool { No, Yes };

struct BuilderContext {
    Ref<const Document> document;
    const RenderStyle& parentStyle;
    const RenderStyle* rootElementStyle = nullptr;
    RefPtr<const Element> element = nullptr;
};

class BuilderState {
public:
    BuilderState(Builder&, RenderStyle&, BuilderContext&&);

    Builder& builder() { return m_builder; }

    RenderStyle& style() { return m_style; }
    const RenderStyle& parentStyle() const { return m_context.parentStyle; }
    const RenderStyle* rootElementStyle() const { return m_context.rootElementStyle; }

    const Document& document() const { return m_context.document.get(); }
    const Element* element() const { return m_context.element.get(); }

    inline void setFontDescription(FontCascadeDescription&&);
    void setFontSize(FontCascadeDescription&, float size);
    inline void setZoom(float);
    inline void setEffectiveZoom(float);
    inline void setWritingMode(WritingMode);
    inline void setTextOrientation(TextOrientation);

    bool fontDirty() const { return m_fontDirty; }
    void setFontDirty() { m_fontDirty = true; }

    inline const FontCascadeDescription& fontDescription();
    inline const FontCascadeDescription& parentFontDescription();

    bool applyPropertyToRegularStyle() const { return m_linkMatch != SelectorChecker::MatchVisited; }
    bool applyPropertyToVisitedLinkStyle() const { return m_linkMatch != SelectorChecker::MatchLink; }

    bool useSVGZoomRules() const;
    bool useSVGZoomRulesForLength() const;
    ScopeOrdinal styleScopeOrdinal() const { return m_currentProperty->styleScopeOrdinal; }

    RefPtr<StyleImage> createStyleImage(const CSSValue&);
    std::optional<FilterOperations> createFilterOperations(const CSSValue&);

    static bool isColorFromPrimitiveValueDerivedFromElement(const CSSPrimitiveValue&);
    StyleColor colorFromPrimitiveValue(const CSSPrimitiveValue&, ForVisitedLink = ForVisitedLink::No) const;

    const Vector<AtomString>& registeredContentAttributes() const { return m_registeredContentAttributes; }
    void registerContentAttribute(const AtomString& attributeLocalName);

    const CSSToLengthConversionData& cssToLengthConversionData() const { return m_cssToLengthConversionData; }
    CSSToStyleMap& styleMap() { return m_styleMap; }

    void setIsBuildingKeyframeStyle() { m_isBuildingKeyframeStyle = true; }

    bool isAuthorOrigin() const
    {
        return m_currentProperty && m_currentProperty->cascadeLevel == CascadeLevel::Author;
    }

private:
    // See the comment in maybeUpdateFontForLetterSpacing() about why this needs to be a friend.
    friend void maybeUpdateFontForLetterSpacing(BuilderState&, CSSValue&);
    friend class Builder;

    void adjustStyleForInterCharacterRuby();

    void updateFont();
#if ENABLE(TEXT_AUTOSIZING)
    void updateFontForTextSizeAdjust();
#endif
    void updateFontForZoomChange();
    void updateFontForGenericFamilyChange();
    void updateFontForOrientationChange();

    Builder& m_builder;

    CSSToStyleMap m_styleMap;

    RenderStyle& m_style;
    const BuilderContext m_context;

    const CSSToLengthConversionData m_cssToLengthConversionData;

    HashSet<AtomString> m_appliedCustomProperties;
    HashSet<AtomString> m_inProgressCustomProperties;
    HashSet<AtomString> m_inCycleCustomProperties;
    WTF::BitSet<numCSSProperties> m_inProgressProperties;
    WTF::BitSet<numCSSProperties> m_inUnitCycleProperties;

    const PropertyCascade::Property* m_currentProperty { nullptr };
    SelectorChecker::LinkMatchMask m_linkMatch { };

    bool m_fontDirty { false };
    Vector<AtomString> m_registeredContentAttributes;

    bool m_isBuildingKeyframeStyle { false };
};

} // namespace Style
} // namespace WebCore
