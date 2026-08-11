/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2023-2025 Apple Inc. All rights reserved.
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
#include "CSSStyleRule.h"
#include "ExceptionOr.h"
#include "InspectorHistory.h"
#include "Settings.h"
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <wtf/CheckedPtr.h>
#include <wtf/CheckedRef.h>
#include <wtf/HashMap.h>
#include <wtf/JSONValues.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/TypeCasts.h>
#include <wtf/Vector.h>
#include <wtf/text/MakeString.h>

namespace Inspector {
class IdentifierRegistry;
}

namespace WebCore {

class CSSRuleList;
class CSSSelector;
class CSSStyleDeclaration;
class CSSStyleRule;
class CSSStyleSheet;
class Document;
class Element;
class InspectorStyleSheet;
class ParsedStyleSheet;

class InspectorCSSId {
public:
    InspectorCSSId() = default;

    explicit InspectorCSSId(const JSON::Object& value)
        : m_styleSheetId(value.getString("styleSheetId"_s))
    {
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

    const String& styleSheetId() const LIFETIME_BOUND { return m_styleSheetId; }
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
    Ref<Inspector::Protocol::CSS::CSSStyle> buildObjectForStyle();
    Ref<JSON::ArrayOf<Inspector::Protocol::CSS::CSSComputedStyleProperty>> buildArrayForComputedStyle();

    ExceptionOr<String> text();

private:
    InspectorStyle(const InspectorCSSId& styleId, Ref<CSSStyleDeclaration>&&, InspectorStyleSheet* parentStyleSheet);

    Vector<InspectorStyleProperty> collectProperties(bool includeAll);
    Ref<Inspector::Protocol::CSS::CSSStyle> styleWithProperties();
    RefPtr<CSSRuleSourceData> extractSourceData() const;
    String shorthandValue(const String& shorthandProperty) const;
    String shorthandPriority(const String& shorthandProperty) const;
    Vector<String> longhandProperties(const String& shorthandProperty) const;

    InspectorCSSId m_styleId;
    const Ref<CSSStyleDeclaration> m_style;
    WeakPtr<InspectorStyleSheet> m_parentStyleSheet;
};

class InspectorStyleSheet : public RefCountedAndCanMakeWeakPtr<InspectorStyleSheet> {
public:
    class Listener {
    public:
        Listener() = default;
        virtual ~Listener() = default;
        virtual void styleSheetChanged(InspectorStyleSheet*) = 0;
    };

    using StyleDeclarationOrCSSRule = Variant<CSSStyleDeclaration*, CSSRule*>;

    static Ref<InspectorStyleSheet> create(Inspector::IdentifierRegistry&, const String& id, RefPtr<CSSStyleSheet>&& pageStyleSheet, Inspector::Protocol::CSS::StyleSheetOrigin, const String& documentURL, Listener*);
    static String NODELETE styleSheetURL(CSSStyleSheet* pageStyleSheet);

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

    virtual ExceptionOr<void> setRuleStyleText(const InspectorCSSId&, const String& newStyleDeclarationText, String* outOldStyleDeclarationText, const String* newRuleText, String* outOldRuleText);

    virtual ExceptionOr<String> text();
    virtual CSSStyleDeclaration* styleForId(const InspectorCSSId&) const;
    void fireStyleSheetChanged();

    InspectorCSSId ruleOrStyleId(StyleDeclarationOrCSSRule) const;

protected:
    InspectorStyleSheet(Inspector::IdentifierRegistry&, const String& id, RefPtr<CSSStyleSheet>&& pageStyleSheet, Inspector::Protocol::CSS::StyleSheetOrigin, const String& documentURL, Listener*);

    bool canBind() const { return m_origin != Inspector::Protocol::CSS::StyleSheetOrigin::UserAgent && m_origin != Inspector::Protocol::CSS::StyleSheetOrigin::User; }
    virtual Document* ownerDocument() const;
    virtual RefPtr<CSSRuleSourceData> ruleSourceDataFor(CSSStyleDeclaration*) const;
    virtual RefPtr<CSSRuleSourceData> ruleSourceDataFor(CSSRule*) const;
    virtual unsigned ruleIndexByStyle(StyleDeclarationOrCSSRule, bool combineSplitRules = false) const;

    virtual bool ensureParsedDataReady();
    virtual RefPtr<InspectorStyle> inspectorStyleForId(const InspectorCSSId&);

    // Also accessed by friend class InspectorStyle.
    virtual Vector<size_t> lineEndings() const;

private:
    friend class InspectorStyle;

    static void collectFlatRules(RefPtr<CSSRuleList>&&, Vector<RefPtr<CSSRule>>* result);
    bool ensureText();
    bool ensureSourceData();
    void ensureFlatRules() const;
    bool originalStyleSheetText(String* result) const;
    bool resourceStyleSheetText(String* result) const;
    bool inlineStyleSheetText(String* result) const;
    bool extensionStyleSheetText(String* result) const;
    bool styleSheetTextFromCSSRuleSerialization(String* result) const;
    Ref<JSON::ArrayOf<Inspector::Protocol::CSS::Grouping>> buildArrayForGroupings(CSSRule&);
    Ref<JSON::ArrayOf<Inspector::Protocol::CSS::CSSRule>> buildArrayForRuleList(CSSRuleList*);
    Ref<Inspector::Protocol::CSS::CSSSelector> buildObjectForSelector(const CSSSelector*);
    Ref<Inspector::Protocol::CSS::SelectorList> buildObjectForSelectorList(CSSStyleRule*, int& endingLine);

    Vector<Ref<CSSStyleRule>> cssStyleRulesSplitFromSameRule(CSSStyleRule&);
    Vector<const CSSSelector*> selectorsForCSSStyleRule(CSSStyleRule&);

    WeakRef<Inspector::IdentifierRegistry> m_identifierRegistry;
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
    static Ref<InspectorStyleSheetForInlineStyle> create(Inspector::IdentifierRegistry&, const String& id, Ref<StyledElement>&&, Inspector::Protocol::CSS::StyleSheetOrigin, Listener*);

    void didModifyElementAttribute();
    ExceptionOr<String> text() final;
    CSSStyleDeclaration* styleForId(const InspectorCSSId& id) const final { ASSERT_UNUSED(id, !id.ordinal()); return &inlineStyle(); }
    ExceptionOr<void> setRuleStyleText(const InspectorCSSId&, const String& newStyleDeclarationText, String* outOldStyleDeclarationText, const String* newRuleText, String* outOldRuleText);

private:
    InspectorStyleSheetForInlineStyle(Inspector::IdentifierRegistry&, const String& id, Ref<StyledElement>&&, Inspector::Protocol::CSS::StyleSheetOrigin, Listener*);

    Document* ownerDocument() const final;
    RefPtr<CSSRuleSourceData> ruleSourceDataFor(CSSStyleDeclaration* style) const final { ASSERT_UNUSED(style, style == &inlineStyle()); return m_ruleSourceData; }
    RefPtr<CSSRuleSourceData> ruleSourceDataFor(CSSRule* rule) const final { ASSERT_UNUSED(rule, rule); return m_ruleSourceData; }
    unsigned ruleIndexByStyle(StyleDeclarationOrCSSRule, bool) const final { return 0; }
    bool ensureParsedDataReady() final;
    RefPtr<InspectorStyle> inspectorStyleForId(const InspectorCSSId&) final;

    // Also accessed by friend class InspectorStyle.
    Vector<size_t> lineEndings() const final;

    CSSStyleDeclaration& inlineStyle() const;
    const String& elementStyleText() const;
    Ref<CSSRuleSourceData> ruleSourceData() const;

    const Ref<StyledElement> m_element;
    RefPtr<CSSRuleSourceData> m_ruleSourceData;
    RefPtr<InspectorStyle> m_inspectorStyle;

    // Contains "style" attribute value.
    mutable String m_styleText;
    mutable bool m_isStyleTextValid;
};

class StyleSheetAction : public InspectorHistory::Action {
    WTF_MAKE_NONCOPYABLE(StyleSheetAction);
public:
    explicit StyleSheetAction(InspectorStyleSheet* styleSheet)
        : InspectorHistory::Action()
        , m_styleSheet(styleSheet)
    {
    }

protected:
    WeakPtr<InspectorStyleSheet> m_styleSheet;
};

class SetStyleSheetTextAction final : public StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(SetStyleSheetTextAction);
public:
    SetStyleSheetTextAction(InspectorStyleSheet* styleSheet, const String& text)
        : StyleSheetAction(styleSheet)
        , m_text(text)
    {
    }

private:
    bool isSetStyleSheetTextAction() const final { return true; }

    ExceptionOr<void> perform() final
    {
        RefPtr styleSheet = m_styleSheet.get();
        if (!styleSheet)
            return { };
        auto result = styleSheet->text();
        if (result.hasException())
            return result.releaseException();
        m_oldText = result.releaseReturnValue();
        return redo();
    }

    ExceptionOr<void> undo() final
    {
        RefPtr styleSheet = m_styleSheet.get();
        if (!styleSheet)
            return { };
        auto result = styleSheet->setText(m_oldText);
        if (result.hasException())
            return result.releaseException();
        styleSheet->reparseStyleSheet(m_oldText);
        return { };
    }

    ExceptionOr<void> redo() final
    {
        RefPtr styleSheet = m_styleSheet.get();
        if (!styleSheet)
            return { };
        auto result = styleSheet->setText(m_text);
        if (result.hasException())
            return result.releaseException();
        styleSheet->reparseStyleSheet(m_text);
        return { };
    }

    String mergeId() final
    {
        RefPtr styleSheet = m_styleSheet.get();
        if (!styleSheet)
            return { };
        return makeString("SetStyleSheetText "_s, styleSheet->id());
    }

    void merge(std::unique_ptr<Action> action) override
    {
        ASSERT(action->mergeId() == mergeId());
        m_text = downcast<SetStyleSheetTextAction>(*action).m_text;
    }

    String m_text;
    String m_oldText;
};

class SetStyleTextAction final : public StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(SetStyleTextAction);
public:
    SetStyleTextAction(InspectorStyleSheet* styleSheet, const InspectorCSSId& cssId, const String& text)
        : StyleSheetAction(styleSheet)
        , m_cssId(cssId)
        , m_newStyleDeclarationText(text)
    {
    }

    ExceptionOr<void> perform() override
    {
        return redo();
    }

    ExceptionOr<void> undo() override
    {
        RefPtr styleSheet = m_styleSheet.get();
        if (!styleSheet)
            return { };
        return styleSheet->setRuleStyleText(
            m_cssId,
            m_oldStyleDeclarationText,
            nullptr, /* outOldStyleDeclarationText */
            &m_oldRuleText,
            nullptr /* outOldRuleText */);
    }

    ExceptionOr<void> redo() override
    {
        RefPtr styleSheet = m_styleSheet.get();
        if (!styleSheet)
            return { };
        return styleSheet->setRuleStyleText(
            m_cssId,
            m_newStyleDeclarationText,
            &m_oldStyleDeclarationText,
            nullptr, /* newRuleText */
            &m_oldRuleText);
    }

    String mergeId() override
    {
        RefPtr styleSheet = m_styleSheet.get();
        if (!styleSheet)
            return { };
        ASSERT(styleSheet->id() == m_cssId.styleSheetId());
        return makeString("SetStyleText "_s, styleSheet->id(), ':', m_cssId.ordinal());
    }

    void merge(std::unique_ptr<Action> action) override
    {
        ASSERT(action->mergeId() == mergeId());

        SetStyleTextAction* other = static_cast<SetStyleTextAction*>(action.get());
        m_newStyleDeclarationText = other->m_newStyleDeclarationText;
    }

private:
    InspectorCSSId m_cssId;
    String m_newStyleDeclarationText;
    String m_oldStyleDeclarationText;
    String m_oldRuleText;
};

class SetRuleHeaderTextAction final : public StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(SetRuleHeaderTextAction);
public:
    SetRuleHeaderTextAction(InspectorStyleSheet* styleSheet, const InspectorCSSId& cssId, const String& newHeaderText)
        : StyleSheetAction(styleSheet)
        , m_cssId(cssId)
        , m_newHeaderText(newHeaderText)
    {
    }

private:
    ExceptionOr<void> perform() final
    {
        RefPtr styleSheet = m_styleSheet.get();
        if (!styleSheet)
            return { };
        auto result = styleSheet->ruleHeaderText(m_cssId);
        if (result.hasException())
            return result.releaseException();
        m_oldHeaderText = result.releaseReturnValue();
        return redo();
    }

    ExceptionOr<void> undo() final
    {
        RefPtr styleSheet = m_styleSheet.get();
        if (!styleSheet)
            return { };
        return styleSheet->setRuleHeaderText(m_cssId, m_oldHeaderText);
    }

    ExceptionOr<void> redo() final
    {
        RefPtr styleSheet = m_styleSheet.get();
        if (!styleSheet)
            return { };
        return styleSheet->setRuleHeaderText(m_cssId, m_newHeaderText);
    }

    InspectorCSSId m_cssId;
    String m_newHeaderText;
    String m_oldHeaderText;
};

class AddRuleAction final : public StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(AddRuleAction);
public:
    AddRuleAction(InspectorStyleSheet* styleSheet, const String& selector)
        : StyleSheetAction(styleSheet)
        , m_selector(selector)
    {
    }

    InspectorCSSId NODELETE newRuleId() const { return m_newId; }

private:
    ExceptionOr<void> perform() final
    {
        return redo();
    }

    ExceptionOr<void> undo() final
    {
        RefPtr styleSheet = m_styleSheet.get();
        if (!styleSheet)
            return { };
        return styleSheet->deleteRule(m_newId);
    }

    ExceptionOr<void> redo() final
    {
        RefPtr styleSheet = m_styleSheet.get();
        if (!styleSheet)
            return { };
        auto result = styleSheet->addRule(m_selector);
        if (result.hasException())
            return result.releaseException();
        m_newId = styleSheet->ruleOrStyleId(result.releaseReturnValue());
        return { };
    }

    InspectorCSSId m_newId;
    String m_selector;
    String m_oldSelector;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SetStyleSheetTextAction)
    static bool isType(const WebCore::InspectorHistory::Action& action)
    {
        return action.isSetStyleSheetTextAction();
    }
SPECIALIZE_TYPE_TRAITS_END()
