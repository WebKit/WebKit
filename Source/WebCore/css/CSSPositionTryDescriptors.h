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

class CSSPositionTryRule;
struct CSSParserContext;

class CSSPositionTryDescriptors final : public PropertySetCSSDescriptors {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(CSSPositionTryDescriptors);
public:
    static Ref<CSSPositionTryDescriptors> create(MutableStyleProperties& propertySet, CSSPositionTryRule& parentRule)
    {
        return adoptRef(*new CSSPositionTryDescriptors(propertySet, parentRule));
    }
    virtual ~CSSPositionTryDescriptors();

    StyleDeclarationType styleDeclarationType() const final { return StyleDeclarationType::PositionTry; }

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
    String marginBlock() const;
    ExceptionOr<void> setMarginBlock(const String&);
    String marginBlockStart() const;
    ExceptionOr<void> setMarginBlockStart(const String&);
    String marginBlockEnd() const;
    ExceptionOr<void> setMarginBlockEnd(const String&);
    String marginInline() const;
    ExceptionOr<void> setMarginInline(const String&);
    String marginInlineStart() const;
    ExceptionOr<void> setMarginInlineStart(const String&);
    String marginInlineEnd() const;
    ExceptionOr<void> setMarginInlineEnd(const String&);
    String inset() const;
    ExceptionOr<void> setInset(const String&);
    String insetBlock() const;
    ExceptionOr<void> setInsetBlock(const String&);
    String insetBlockStart() const;
    ExceptionOr<void> setInsetBlockStart(const String&);
    String insetBlockEnd() const;
    ExceptionOr<void> setInsetBlockEnd(const String&);
    String insetInline() const;
    ExceptionOr<void> setInsetInline(const String&);
    String insetInlineStart() const;
    ExceptionOr<void> setInsetInlineStart(const String&);
    String insetInlineEnd() const;
    ExceptionOr<void> setInsetInlineEnd(const String&);
    String top() const;
    ExceptionOr<void> setTop(const String&);
    String left() const;
    ExceptionOr<void> setLeft(const String&);
    String right() const;
    ExceptionOr<void> setRight(const String&);
    String bottom() const;
    ExceptionOr<void> setBottom(const String&);
    String width() const;
    ExceptionOr<void> setWidth(const String&);
    String minWidth() const;
    ExceptionOr<void> setMinWidth(const String&);
    String maxWidth() const;
    ExceptionOr<void> setMaxWidth(const String&);
    String height() const;
    ExceptionOr<void> setHeight(const String&);
    String minHeight() const;
    ExceptionOr<void> setMinHeight(const String&);
    String maxHeight() const;
    ExceptionOr<void> setMaxHeight(const String&);
    String blockSize() const;
    ExceptionOr<void> setBlockSize(const String&);
    String minBlockSize() const;
    ExceptionOr<void> setMinBlockSize(const String&);
    String maxBlockSize() const;
    ExceptionOr<void> setMaxBlockSize(const String&);
    String inlineSize() const;
    ExceptionOr<void> setInlineSize(const String&);
    String minInlineSize() const;
    ExceptionOr<void> setMinInlineSize(const String&);
    String maxInlineSize() const;
    ExceptionOr<void> setMaxInlineSize(const String&);
    String placeSelf() const;
    ExceptionOr<void> setPlaceSelf(const String&);
    String alignSelf() const;
    ExceptionOr<void> setAlignSelf(const String&);
    String justifySelf() const;
    ExceptionOr<void> setJustifySelf(const String&);
    String positionAnchor() const;
    ExceptionOr<void> setPositionAnchor(const String&);
    String positionArea() const;
    ExceptionOr<void> setPositionArea(const String&);

private:
    CSSPositionTryDescriptors(MutableStyleProperties&, CSSPositionTryRule&);

    StyleRuleType ruleType() const final;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_STYLE_DECLARATION(CSSPositionTryDescriptors, StyleDeclarationType::PositionTry)
