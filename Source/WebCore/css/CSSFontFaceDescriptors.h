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

class CSSFontFaceRule;

class CSSFontFaceDescriptors final : public PropertySetCSSDescriptors {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(CSSFontFaceDescriptors);
public:
    static Ref<CSSFontFaceDescriptors> create(MutableStyleProperties& propertySet, CSSFontFaceRule& parentRule)
    {
        return adoptRef(*new CSSFontFaceDescriptors(propertySet, parentRule));
    }
    virtual ~CSSFontFaceDescriptors();

    StyleDeclarationType styleDeclarationType() const final { return StyleDeclarationType::FontFace; }

    String src() const;
    ExceptionOr<void> setSrc(const String&);
    String fontFamily() const;
    ExceptionOr<void> setFontFamily(const String&);
    String fontStyle() const;
    ExceptionOr<void> setFontStyle(const String&);
    String fontWeight() const;
    ExceptionOr<void> setFontWeight(const String&);
    String fontStretch() const;
    ExceptionOr<void> setFontStretch(const String&);
    String fontWidth() const;
    ExceptionOr<void> setFontWidth(const String&);
    String sizeAdjust() const;
    ExceptionOr<void> setSizeAdjust(const String&);
    String unicodeRange() const;
    ExceptionOr<void> setUnicodeRange(const String&);
    String fontFeatureSettings() const;
    ExceptionOr<void> setFontFeatureSettings(const String&);
    String fontDisplay() const;
    ExceptionOr<void> setFontDisplay(const String&);

private:
    CSSFontFaceDescriptors(MutableStyleProperties&, CSSFontFaceRule&);

    StyleRuleType ruleType() const final;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_STYLE_DECLARATION(CSSFontFaceDescriptors, StyleDeclarationType::FontFace)
