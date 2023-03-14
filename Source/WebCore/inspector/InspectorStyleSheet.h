/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 * Copyright (C) 2022, Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSPropertySourceData.h"
#include "CSSStyleDeclaration.h"
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <wtf/HashMap.h>
#include <wtf/JSONValues.h>
#include <wtf/Vector.h>

namespace WebCore {

class CSSRuleList;
class CSSSelector;
class CSSStyleDeclaration;
class CSSStyleRule;
class CSSStyleSheet;
class Document;
class Element;
class InspectorPageAgent;
class InspectorStyleSheet;
class ParsedStyleSheet;

class InspectorCSSId {
public:
    InspectorCSSId() = default;

    explicit InspectorCSSId(const JSON::Object& value)
    {
        m_styleSheetId = value.getString("styleSheetId"_s);
        if (!m_styleSheetId)
            return;

        if (auto ordinal = value.getInteger("ordinal"_s))
            m_ordinal = *ordinal;
        else
            m_styleSheetId = String();
    }

    InspectorCSSId(const String& styleSheetId, unsigned ordinal)
        : m_styleSheetId(styleSheetId)
        , m_ordinal(ordinal)
    {
    }

    bool isEmpty() const { return m_styleSheetId.isEmpty(); }

    const String& styleSheetId() const { return m_styleSheetId; }
    unsigned ordinal() const { return m_ordinal; }

    // ID type is either Inspector::Protocol::CSS::CSSStyleId or Inspector::Protocol::CSS::CSSRuleId.
    template<typename ID>
    RefPtr<ID> asProtocolValue() const
    {
        if (isEmpty())
            return nullptr;

        return ID::create()
            .setStyleSheetId(m_styleSheetId)
            .setOrdinal(m_ordinal)
            .release();
    }

private:
    String m_styleSheetId;
    unsigned m_ordinal = {0};
};

struct InspectorStyleProperty {
    InspectorStyleProperty()
        : hasSource(false)
        , disabled(false)
    {
    }

    InspectorStyleProperty(CSSPropertySourceData sourceData, bool hasSource, bool disabled)
        : sourceData(sourceData)
        , hasSource(hasSource)
        , disabled(disabled)
    {
    }

    void setRawTextFromStyleDeclaration(const String& styleDeclaration)
    {
        unsigned start = sourceData.range.start;
        unsigned end = sourceData.range.end;
        ASSERT_WITH_SECURITY_IMPLICATION(start < end);
        ASSERT(end <= styleDeclaration.length());
        rawText = styleDeclaration.substring(start, end - start);
    }

    bool hasRawText() const { return !rawText.isEmpty(); }

    CSSPropertySourceData sourceData;
    bool hasSource;
    bool disabled;
    String rawText;
};

class InspectorStyle final : public RefCounted<InspectorStyle> {
public:
    static Ref<InspectorStyle> create(const InspectorCSSId& styleId, Ref<CSSStyleDeclaration>&&, InspectorStyleSheet* parentStyleSheet);
    ~InspectorStyle();

    CSSStyleDeclaration& cssStyle() const { return m_style.get(); }
    Ref<Inspector::Protocol::CSS::CSSStyle> buildObjectForStyle() const;
    Ref<JSON::ArrayOf<Inspector::Protocol::CSS::CSSComputedStyleProperty>> buildArrayForComputedStyle() const;

    ExceptionOr<String> text() const;
    ExceptionOr<void> setText(const String&);

private:
    InspectorStyle(const InspectorCSSId& styleId, Ref<CSSStyleDeclaration>&&, InspectorStyleSheet* parentStyleSheet);

    Vector<InspectorStyleProperty> collectProperties(bool includeAll) const;
    Ref<Inspector::Protocol::CSS::CSSStyle> styleWithProperties() const;
    RefPtr<CSSRuleSourceData> extractSourceData() const;
    String shorthandValue(const String& shorthandProperty) const;
    String shorthandPriority(const String& shorthandProperty) const;
    Vector<String> longhandProperties(const String& shorthandProperty) const;

    InspectorCSSId m_styleId;
    Ref<CSSStyleDeclaration> m_style;
    InspectorStyleSheet* m_parentStyleSheet;
};

class InspectorStyleSheet : public RefCounted<InspectorStyleSheet> {
public:
    class Listener {
    public:
        Listener() = default;
        virtual ~Listener() = default;
        virtual void styleSheetChanged(InspectorStyleSheet*) = 0;
    };

    using StyleDeclarationOrCSSRule = std::variant<CSSStyleDeclaration*, CSSRule*>;

    static Ref<InspectorStyleSheet> create(InspectorPageAgent*, const String& id, RefPtr<CSSStyleSheet>&& pageStyleSheet, Inspector::Protocol::CSS::StyleSheetOrigin, const String& documentURL, Listener*);
    static String styleSheetURL(CSSStyleSheet* pageStyleSheet);

    virtual ~InspectorStyleSheet();

    String id() const { return m_id; }
    String finalURL() const;
    CSSStyleSheet* pageStyleSheet() const { return m_pageStyleSheet.get(); }
    void reparseStyleSheet(const String&);
    ExceptionOr<void> setText(const String&);
    ExceptionOr<String> ruleHeaderText(const InspectorCSSId&);
    ExceptionOr<void> setRuleHeaderText(const InspectorCSSId&, const String& selector);
    ExceptionOr<CSSStyleRule*> addRule(const String& selector);
    ExceptionOr<void> deleteRule(const InspectorCSSId&);
    CSSRule* ruleForId(const InspectorCSSId&) const;
    RefPtr<Inspector::Protocol::CSS::CSSStyleSheetBody> buildObjectForStyleSheet();
    RefPtr<Inspector::Protocol::CSS::CSSStyleSheetHeader> buildObjectForStyleSheetInfo();
    RefPtr<Inspector::Protocol::CSS::CSSRule> buildObjectForRule(CSSStyleRule*);
    Ref<Inspector::Protocol::CSS::CSSStyle> buildObjectForStyle(CSSStyleDeclaration*);
    RefPtr<Inspector::Protocol::CSS::Grouping> buildObjectForGrouping(CSSRule*);
    ExceptionOr<void> setStyleText(const InspectorCSSId&, const String& text, String* oldText);

    virtual ExceptionOr<String> text() const;
    virtual CSSStyleDeclaration* styleForId(const InspectorCSSId&) const;
    void fireStyleSheetChanged();

    InspectorCSSId ruleOrStyleId(StyleDeclarationOrCSSRule) const;

protected:
    InspectorStyleSheet(InspectorPageAgent*, const String& id, RefPtr<CSSStyleSheet>&& pageStyleSheet, Inspector::Protocol::CSS::StyleSheetOrigin, const String& documentURL, Listener*);

    bool canBind() const { return m_origin != Inspector::Protocol::CSS::StyleSheetOrigin::UserAgent && m_origin != Inspector::Protocol::CSS::StyleSheetOrigin::User; }
    virtual Document* ownerDocument() const;
    virtual RefPtr<CSSRuleSourceData> ruleSourceDataFor(CSSStyleDeclaration*) const;
    virtual RefPtr<CSSRuleSourceData> ruleSourceDataFor(CSSRule*) const;
    virtual unsigned ruleIndexByStyle(StyleDeclarationOrCSSRule, bool combineSplitRules = false) const;

    virtual bool ensureParsedDataReady();
    virtual RefPtr<InspectorStyle> inspectorStyleForId(const InspectorCSSId&);

    // Also accessed by friend class InspectorStyle.
    virtual ExceptionOr<void> setStyleText(CSSStyleDeclaration*, const String&);
    virtual Vector<size_t> lineEndings() const;

private:
    friend class InspectorStyle;

    static void collectFlatRules(RefPtr<CSSRuleList>&&, Vector<RefPtr<CSSRule>>* result);
    bool styleSheetMutated() const;
    bool ensureText() const;
    bool ensureSourceData();
    void ensureFlatRules() const;
    bool styleSheetTextWithChangedStyle(CSSStyleDeclaration*, const String& newStyleText, String* result);
    bool originalStyleSheetText(String* result) const;
    bool resourceStyleSheetText(String* result) const;
    bool inlineStyleSheetText(String* result) const;
    bool extensionStyleSheetText(String* result) const;
    Ref<JSON::ArrayOf<Inspector::Protocol::CSS::Grouping>> buildArrayForGroupings(CSSRule&);
    Ref<JSON::ArrayOf<Inspector::Protocol::CSS::CSSRule>> buildArrayForRuleList(CSSRuleList*);
    Ref<Inspector::Protocol::CSS::CSSSelector> buildObjectForSelector(const CSSSelector*);
    Ref<Inspector::Protocol::CSS::SelectorList> buildObjectForSelectorList(CSSStyleRule*, int& endingLine);

    Vector<Ref<CSSStyleRule>> cssStyleRulesSplitFromSameRule(CSSStyleRule&);
    Vector<const CSSSelector*> selectorsForCSSStyleRule(CSSStyleRule&);

    InspectorPageAgent* m_pageAgent;
    String m_id;
    RefPtr<CSSStyleSheet> m_pageStyleSheet;
    Inspector::Protocol::CSS::StyleSheetOrigin m_origin;
    String m_documentURL;
    ParsedStyleSheet* m_parsedStyleSheet;
    mutable Vector<RefPtr<CSSRule>> m_flatRules;
    Listener* m_listener;
};

class InspectorStyleSheetForInlineStyle final : public InspectorStyleSheet {
public:
    static Ref<InspectorStyleSheetForInlineStyle> create(InspectorPageAgent*, const String& id, Ref<StyledElement>&&, Inspector::Protocol::CSS::StyleSheetOrigin, Listener*);

    void didModifyElementAttribute();
    ExceptionOr<String> text() const final;
    CSSStyleDeclaration* styleForId(const InspectorCSSId& id) const final { ASSERT_UNUSED(id, !id.ordinal()); return &inlineStyle(); }

private:
    InspectorStyleSheetForInlineStyle(InspectorPageAgent*, const String& id, Ref<StyledElement>&&, Inspector::Protocol::CSS::StyleSheetOrigin, Listener*);

    Document* ownerDocument() const final;
    RefPtr<CSSRuleSourceData> ruleSourceDataFor(CSSStyleDeclaration* style) const final { ASSERT_UNUSED(style, style == &inlineStyle()); return m_ruleSourceData; }
    RefPtr<CSSRuleSourceData> ruleSourceDataFor(CSSRule* rule) const final { ASSERT_UNUSED(rule, rule); return m_ruleSourceData; }
    unsigned ruleIndexByStyle(StyleDeclarationOrCSSRule, bool) const final { return 0; }
    bool ensureParsedDataReady() final;
    RefPtr<InspectorStyle> inspectorStyleForId(const InspectorCSSId&) final;

    // Also accessed by friend class InspectorStyle.
    ExceptionOr<void> setStyleText(CSSStyleDeclaration*, const String&) final;
    Vector<size_t> lineEndings() const final;

    CSSStyleDeclaration& inlineStyle() const;
    const String& elementStyleText() const;
    Ref<CSSRuleSourceData> ruleSourceData() const;

    Ref<StyledElement> m_element;
    RefPtr<CSSRuleSourceData> m_ruleSourceData;
    RefPtr<InspectorStyle> m_inspectorStyle;

    // Contains "style" attribute value.
    mutable String m_styleText;
    mutable bool m_isStyleTextValid;
};

} // namespace WebCore
