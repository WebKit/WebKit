/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#pragma once

#include "CSSStyleDeclaration.h"
#include "RenderStyleConstants.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

class ComputedStyleExtractor;
class Element;
class MutableStyleProperties;

enum ComputedStyleExtractorOption : uint8_t;

class CSSComputedStyleDeclaration final : public CSSStyleDeclaration {
    WTF_MAKE_ISO_ALLOCATED_EXPORT(CSSComputedStyleDeclaration, WEBCORE_EXPORT);
public:
    WEBCORE_EXPORT static Ref<CSSComputedStyleDeclaration> create(Element&, StringView pseudoElementName);
    WEBCORE_EXPORT static Ref<CSSComputedStyleDeclaration> createAllowingVisitedLinkColoring(Element&);
    virtual ~CSSComputedStyleDeclaration();

    WEBCORE_EXPORT void ref() final;
    WEBCORE_EXPORT void deref() final;

    String getPropertyValue(CSSPropertyID) const;

private:
    CSSComputedStyleDeclaration(Element&, PseudoId, OptionSet<ComputedStyleExtractorOption>);

    CSSRule* parentRule() const final;
    CSSRule* cssRules() const final;
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
    ExceptionOr<void> setPropertyInternal(CSSPropertyID, const String& value, bool important) final;
    Ref<MutableStyleProperties> copyProperties() const final;
    RefPtr<CSSValue> getPropertyCSSValue(CSSPropertyID) const;
    const Settings* settings() const final;

    const FixedVector<CSSPropertyID>& exposedComputedCSSPropertyIDs() const;
    ComputedStyleExtractor extractor() const;

    Ref<Element> m_element;
    PseudoId m_pseudoElementSpecifier { };
    OptionSet<ComputedStyleExtractorOption> m_options;
    unsigned m_refCount { 1 };
};

} // namespace WebCore
