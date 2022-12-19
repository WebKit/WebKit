/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "CSSParserContext.h"
#include "CSSStyleDeclaration.h"
#include "DeprecatedCSSOMValue.h"
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class CSSRule;
class CSSProperty;
class CSSValue;
class MutableStyleProperties;
class StyleSheetContents;
class StyledElement;

class PropertySetCSSStyleDeclaration : public CSSStyleDeclaration {
    WTF_MAKE_ISO_ALLOCATED(PropertySetCSSStyleDeclaration);
public:
    explicit PropertySetCSSStyleDeclaration(MutableStyleProperties& propertySet)
        : m_propertySet(&propertySet)
    { }

    virtual void clearParentElement() { ASSERT_NOT_REACHED(); }

    StyleSheetContents* contextStyleSheet() const;

protected:
    enum MutationType { NoChanges, PropertyChanged };

    virtual CSSParserContext cssParserContext() const;

    MutableStyleProperties* m_propertySet;
    HashMap<CSSValue*, WeakPtr<DeprecatedCSSOMValue>> m_cssomValueWrappers;

private:
    void ref() override;
    void deref() override;

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
    RefPtr<CSSValue> getPropertyCSSValueInternal(CSSPropertyID) final;
    String getPropertyValueInternal(CSSPropertyID) final;
    ExceptionOr<void> setPropertyInternal(CSSPropertyID, const String& value, bool important) final;
    
    Ref<MutableStyleProperties> copyProperties() const final;
    bool isExposed(CSSPropertyID) const;

    RefPtr<DeprecatedCSSOMValue> wrapForDeprecatedCSSOM(CSSValue*);
    
    virtual bool willMutate() WARN_UNUSED_RETURN { return true; }
    virtual void didMutate(MutationType) { }
};

class StyleRuleCSSStyleDeclaration final : public PropertySetCSSStyleDeclaration {
    WTF_MAKE_ISO_ALLOCATED(StyleRuleCSSStyleDeclaration);
public:
    static Ref<StyleRuleCSSStyleDeclaration> create(MutableStyleProperties& propertySet, CSSRule& parentRule)
    {
        return adoptRef(*new StyleRuleCSSStyleDeclaration(propertySet, parentRule));
    }
    virtual ~StyleRuleCSSStyleDeclaration();

    void clearParentRule() { m_parentRule = nullptr; }
    
    void ref() final;
    void deref() final;

    void reattach(MutableStyleProperties&);

private:
    StyleRuleCSSStyleDeclaration(MutableStyleProperties&, CSSRule&);

    CSSStyleSheet* parentStyleSheet() const final;

    CSSRule* parentRule() const final { return m_parentRule; }

    bool willMutate() final WARN_UNUSED_RETURN;
    void didMutate(MutationType) final;
    CSSParserContext cssParserContext() const final;

    unsigned m_refCount;
    StyleRuleType m_parentRuleType;
    CSSRule* m_parentRule;
};

class InlineCSSStyleDeclaration final : public PropertySetCSSStyleDeclaration {
    WTF_MAKE_ISO_ALLOCATED(InlineCSSStyleDeclaration);
public:
    InlineCSSStyleDeclaration(MutableStyleProperties& propertySet, StyledElement& parentElement)
        : PropertySetCSSStyleDeclaration(propertySet)
        , m_parentElement(&parentElement)
    {
    }

private:
    CSSStyleSheet* parentStyleSheet() const final;
    StyledElement* parentElement() const final { return m_parentElement; }
    void clearParentElement() final { m_parentElement = nullptr; }

    bool willMutate() final WARN_UNUSED_RETURN;
    void didMutate(MutationType) final;
    CSSParserContext cssParserContext() const final;

    StyledElement* m_parentElement;
};

} // namespace WebCore
