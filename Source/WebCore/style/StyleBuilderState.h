/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "RenderStyle.h"
#include "SelectorChecker.h"
#include <wtf/Bitmap.h>

namespace WebCore {

class StyleImage;
class StyleResolver;

namespace Style {

class Builder;

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

    void setFontDescription(FontCascadeDescription&& fontDescription) { m_fontDirty |= m_style.setFontDescription(WTFMove(fontDescription)); }
    void setFontSize(FontCascadeDescription&, float size);
    void setZoom(float f) { m_fontDirty |= m_style.setZoom(f); }
    void setEffectiveZoom(float f) { m_fontDirty |= m_style.setEffectiveZoom(f); }
    void setWritingMode(WritingMode writingMode) { m_fontDirty |= m_style.setWritingMode(writingMode); }
    void setTextOrientation(TextOrientation textOrientation) { m_fontDirty |= m_style.setTextOrientation(textOrientation); }

    bool fontDirty() const { return m_fontDirty; }
    void setFontDirty() { m_fontDirty = true; }

    const FontCascadeDescription& fontDescription() { return m_style.fontDescription(); }
    const FontCascadeDescription& parentFontDescription() { return parentStyle().fontDescription(); }

    // FIXME: These are mutually exclusive, clean up the code to take that into account.
    bool applyPropertyToRegularStyle() const { return m_linkMatch != SelectorChecker::MatchVisited; }
    bool applyPropertyToVisitedLinkStyle() const { return m_linkMatch == SelectorChecker::MatchVisited; }

    bool useSVGZoomRules() const;
    bool useSVGZoomRulesForLength() const;
    ScopeOrdinal styleScopeOrdinal() const { return m_styleScopeOrdinal; }

    Ref<CSSValue> resolveImageStyles(CSSValue&);
    RefPtr<StyleImage> createStyleImage(CSSValue&);
    bool createFilterOperations(const CSSValue&, FilterOperations& outOperations);

    static bool isColorFromPrimitiveValueDerivedFromElement(const CSSPrimitiveValue&);
    Color colorFromPrimitiveValue(const CSSPrimitiveValue&, bool forVisitedLink = false) const;

    const Vector<AtomString>& registeredContentAttributes() const { return m_registeredContentAttributes; }
    void registerContentAttribute(const AtomString& attributeLocalName);

    const CSSToLengthConversionData& cssToLengthConversionData() const { return m_cssToLengthConversionData; }
    CSSToStyleMap& styleMap() { return m_styleMap; }

private:
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

    Bitmap<numCSSProperties> m_appliedProperties;
    HashSet<String> m_appliedCustomProperties;
    Bitmap<numCSSProperties> m_inProgressProperties;
    HashSet<String> m_inProgressPropertiesCustom;

    CascadeLevel m_cascadeLevel { };
    ScopeOrdinal m_styleScopeOrdinal { };
    SelectorChecker::LinkMatchMask m_linkMatch { };

    bool m_fontDirty { false };
    Vector<AtomString> m_registeredContentAttributes;
};

}
}
