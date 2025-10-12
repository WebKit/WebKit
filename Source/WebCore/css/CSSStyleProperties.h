/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/CSSProperty.h>
#include <WebCore/CSSPropertyNames.h>
#include <WebCore/CSSStyleDeclaration.h>
#include <WebCore/StyleRuleType.h>
#include <WebCore/StyledElement.h>
#include <wtf/HashMap.h>
#include <wtf/OptionalOrReference.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class StyleSheetContents;
struct CSSParserContext;

class CSSStyleProperties : public CSSStyleDeclaration {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(CSSStyleProperties);
public:
    StyleDeclarationType styleDeclarationType() const final { return StyleDeclarationType::Style; }

    String cssFloat();
    ExceptionOr<void> setCssFloat(const String&);

    // CSSPropertyID versions of the CSSOM functions to support bindings and editing.
    virtual String getPropertyValueInternal(CSSPropertyID) = 0;
    virtual ExceptionOr<void> setPropertyInternal(CSSPropertyID, const String& value, IsImportant) = 0;
    virtual Ref<MutableStyleProperties> copyProperties() const = 0;

    // FIXME: It would be more efficient, by virtue of avoiding the text transformation and hash lookup currently
    // required in the implementation, if we could could smuggle the CSSPropertyID through the bindings, perhaps
    // by encoding it into the HashTableValue and then passing it together with the PropertyName.

    // Shared implementation for all properties that match https://drafts.csswg.org/cssom/#dom-cssstyledeclaration-camel_cased_attribute.
    String propertyValueForCamelCasedIDLAttribute(const AtomString&);
    ExceptionOr<void> setPropertyValueForCamelCasedIDLAttribute(const AtomString&, const String&);

    // Shared implementation for all properties that match https://drafts.csswg.org/cssom/#dom-cssstyledeclaration-webkit_cased_attribute.
    String propertyValueForWebKitCasedIDLAttribute(const AtomString&);
    ExceptionOr<void> setPropertyValueForWebKitCasedIDLAttribute(const AtomString&, const String&);

    // Shared implementation for all properties that match https://drafts.csswg.org/cssom/#dom-cssstyledeclaration-dashed_attribute.
    String propertyValueForDashedIDLAttribute(const AtomString&);
    ExceptionOr<void> setPropertyValueForDashedIDLAttribute(const AtomString&, const String&);

    // Shared implementation for all properties that match non-standard Epub-cased.
    String propertyValueForEpubCasedIDLAttribute(const AtomString&);
    ExceptionOr<void> setPropertyValueForEpubCasedIDLAttribute(const AtomString&, const String&);

    // FIXME: This needs to pass in a Settings& to work correctly.
    static CSSPropertyID getCSSPropertyIDFromJavaScriptPropertyName(const AtomString&);
};

class PropertySetCSSStyleProperties : public CSSStyleProperties {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(PropertySetCSSStyleProperties);
public:
    explicit PropertySetCSSStyleProperties(MutableStyleProperties& propertySet)
        : m_propertySet(&propertySet)
    {
    }

    void ref() const override;
    void deref() const override;

    StyleSheetContents* contextStyleSheet() const;

protected:
    enum class MutationType : uint8_t { NoChanges, StyleAttributeChanged, PropertyChanged };

    virtual OptionalOrReference<CSSParserContext> cssParserContext() const;

    MutableStyleProperties* m_propertySet;
    HashMap<CSSValue*, WeakPtr<DeprecatedCSSOMValue>> m_cssomValueWrappers;

private:
    CSSRule* parentRule() const override { return nullptr; }
    // FIXME: To implement.
    CSSRule* cssRules() const override { return nullptr; }
    unsigned length() const final;
    String item(unsigned index) const final;
    RefPtr<DeprecatedCSSOMValue> getPropertyCSSValue(const String& propertyName) final;
    String getPropertyValue(const String& propertyName) final;
    String getPropertyPriority(const String& propertyName) final;
    String getPropertyShorthand(const String& propertyName) final;
    bool isPropertyImplicit(const String& propertyName) final;
    ExceptionOr<void> setProperty(const String& propertyName, const String& value, const String& priority) final;
    ExceptionOr<String> removeProperty(const String& propertyName) final;
    String cssText() const final;
    ExceptionOr<void> setCssText(const String&) final;
    String getPropertyValueInternal(CSSPropertyID) final;
    ExceptionOr<void> setPropertyInternal(CSSPropertyID, const String& value, IsImportant) final;
    Ref<MutableStyleProperties> copyProperties() const final;

    bool isExposed(CSSPropertyID) const;

    RefPtr<DeprecatedCSSOMValue> wrapForDeprecatedCSSOM(CSSValue*);

    virtual bool willMutate() WARN_UNUSED_RETURN { return true; }
    virtual void didMutate(MutationType) { }
};

class StyleRuleCSSStyleProperties final : public PropertySetCSSStyleProperties, public RefCounted<StyleRuleCSSStyleProperties> {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(StyleRuleCSSStyleProperties);
public:
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    static Ref<StyleRuleCSSStyleProperties> create(MutableStyleProperties& propertySet, CSSRule& parentRule)
    {
        return adoptRef(*new StyleRuleCSSStyleProperties(propertySet, parentRule));
    }
    virtual ~StyleRuleCSSStyleProperties();

    void clearParentRule() { m_parentRule = nullptr; }

    void reattach(MutableStyleProperties&);

private:
    StyleRuleCSSStyleProperties(MutableStyleProperties&, CSSRule&);

    CSSStyleSheet* parentStyleSheet() const final;

    CSSRule* parentRule() const final;

    bool willMutate() final WARN_UNUSED_RETURN;
    void didMutate(MutationType) final;
    OptionalOrReference<CSSParserContext> cssParserContext() const final;

    StyleRuleType m_parentRuleType;
    WeakPtr<CSSRule> m_parentRule;
};

class InlineCSSStyleProperties final : public PropertySetCSSStyleProperties {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(InlineCSSStyleProperties);
public:
    InlineCSSStyleProperties(MutableStyleProperties& propertySet, StyledElement& parentElement)
        : PropertySetCSSStyleProperties(propertySet)
        , m_parentElement(parentElement)
    {
    }

private:
    CSSStyleSheet* parentStyleSheet() const final;
    StyledElement* parentElement() const final { return m_parentElement.get(); }

    bool willMutate() final WARN_UNUSED_RETURN;
    void didMutate(MutationType) final;
    OptionalOrReference<CSSParserContext> cssParserContext() const final;

    WeakPtr<StyledElement, WeakPtrImplWithEventTargetData> m_parentElement;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_STYLE_DECLARATION(CSSStyleProperties, StyleDeclarationType::Style)
