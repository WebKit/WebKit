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

#include <WebCore/PropertySetCSSDescriptors.h>

namespace WebCore {

class CSSPageRule;
struct CSSParserContext;

class CSSPageDescriptors final : public PropertySetCSSDescriptors {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(CSSPageDescriptors);
public:
    static Ref<CSSPageDescriptors> create(MutableStyleProperties& propertySet, CSSPageRule& parentRule)
    {
        return adoptRef(*new CSSPageDescriptors(propertySet, parentRule));
    }
    virtual ~CSSPageDescriptors();

    StyleDeclarationType styleDeclarationType() const final { return StyleDeclarationType::Page; }

    String margin() const;
    ExceptionOr<void> setMargin(const String&);
    String marginTop() const;
    ExceptionOr<void> setMarginTop(const String&);
    String marginRight() const;
    ExceptionOr<void> setMarginRight(const String&);
    String marginBottom() const;
    ExceptionOr<void> setMarginBottom(const String&);
    String marginLeft() const;
    ExceptionOr<void> setMarginLeft(const String&);
    String size() const;
    ExceptionOr<void> setSize(const String&);

private:
    CSSPageDescriptors(MutableStyleProperties&, CSSPageRule&);

    StyleRuleType ruleType() const final;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_STYLE_DECLARATION(CSSPageDescriptors, StyleDeclarationType::Page)
