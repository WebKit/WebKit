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

#include "PropertySetCSSDescriptors.h"

namespace WebCore {

class CSSPositionTryRule;
struct CSSParserContext;

class CSSPositionTryDescriptors final : public PropertySetCSSDescriptors {
    WTF_MAKE_TZONE_ALLOCATED(CSSPositionTryDescriptors);
public:
    static Ref<CSSPositionTryDescriptors> create(MutableStyleProperties& propertySet, CSSPositionTryRule& parentRule)
    {
        return adoptRef(*new CSSPositionTryDescriptors(propertySet, parentRule));
    }
    virtual ~CSSPositionTryDescriptors();

    StyleDeclarationType styleDeclarationType() const final { return StyleDeclarationType::PositionTry; }

    RefPtr<StringImpl> margin() const;
    ExceptionOr<void> setMargin(StringImpl*);
    RefPtr<StringImpl> marginTop() const;
    ExceptionOr<void> setMarginTop(StringImpl*);
    RefPtr<StringImpl> marginRight() const;
    ExceptionOr<void> setMarginRight(StringImpl*);
    RefPtr<StringImpl> marginBottom() const;
    ExceptionOr<void> setMarginBottom(StringImpl*);
    RefPtr<StringImpl> marginLeft() const;
    ExceptionOr<void> setMarginLeft(StringImpl*);
    RefPtr<StringImpl> marginBlock() const;
    ExceptionOr<void> setMarginBlock(StringImpl*);
    RefPtr<StringImpl> marginBlockStart() const;
    ExceptionOr<void> setMarginBlockStart(StringImpl*);
    RefPtr<StringImpl> marginBlockEnd() const;
    ExceptionOr<void> setMarginBlockEnd(StringImpl*);
    RefPtr<StringImpl> marginInline() const;
    ExceptionOr<void> setMarginInline(StringImpl*);
    RefPtr<StringImpl> marginInlineStart() const;
    ExceptionOr<void> setMarginInlineStart(StringImpl*);
    RefPtr<StringImpl> marginInlineEnd() const;
    ExceptionOr<void> setMarginInlineEnd(StringImpl*);
    RefPtr<StringImpl> inset() const;
    ExceptionOr<void> setInset(StringImpl*);
    RefPtr<StringImpl> insetBlock() const;
    ExceptionOr<void> setInsetBlock(StringImpl*);
    RefPtr<StringImpl> insetBlockStart() const;
    ExceptionOr<void> setInsetBlockStart(StringImpl*);
    RefPtr<StringImpl> insetBlockEnd() const;
    ExceptionOr<void> setInsetBlockEnd(StringImpl*);
    RefPtr<StringImpl> insetInline() const;
    ExceptionOr<void> setInsetInline(StringImpl*);
    RefPtr<StringImpl> insetInlineStart() const;
    ExceptionOr<void> setInsetInlineStart(StringImpl*);
    RefPtr<StringImpl> insetInlineEnd() const;
    ExceptionOr<void> setInsetInlineEnd(StringImpl*);
    RefPtr<StringImpl> top() const;
    ExceptionOr<void> setTop(StringImpl*);
    RefPtr<StringImpl> left() const;
    ExceptionOr<void> setLeft(StringImpl*);
    RefPtr<StringImpl> right() const;
    ExceptionOr<void> setRight(StringImpl*);
    RefPtr<StringImpl> bottom() const;
    ExceptionOr<void> setBottom(StringImpl*);
    RefPtr<StringImpl> width() const;
    ExceptionOr<void> setWidth(StringImpl*);
    RefPtr<StringImpl> minWidth() const;
    ExceptionOr<void> setMinWidth(StringImpl*);
    RefPtr<StringImpl> maxWidth() const;
    ExceptionOr<void> setMaxWidth(StringImpl*);
    RefPtr<StringImpl> height() const;
    ExceptionOr<void> setHeight(StringImpl*);
    RefPtr<StringImpl> minHeight() const;
    ExceptionOr<void> setMinHeight(StringImpl*);
    RefPtr<StringImpl> maxHeight() const;
    ExceptionOr<void> setMaxHeight(StringImpl*);
    RefPtr<StringImpl> blockSize() const;
    ExceptionOr<void> setBlockSize(StringImpl*);
    RefPtr<StringImpl> minBlockSize() const;
    ExceptionOr<void> setMinBlockSize(StringImpl*);
    RefPtr<StringImpl> maxBlockSize() const;
    ExceptionOr<void> setMaxBlockSize(StringImpl*);
    RefPtr<StringImpl> inlineSize() const;
    ExceptionOr<void> setInlineSize(StringImpl*);
    RefPtr<StringImpl> minInlineSize() const;
    ExceptionOr<void> setMinInlineSize(StringImpl*);
    RefPtr<StringImpl> maxInlineSize() const;
    ExceptionOr<void> setMaxInlineSize(StringImpl*);
    RefPtr<StringImpl> placeSelf() const;
    ExceptionOr<void> setPlaceSelf(StringImpl*);
    RefPtr<StringImpl> alignSelf() const;
    ExceptionOr<void> setAlignSelf(StringImpl*);
    RefPtr<StringImpl> justifySelf() const;
    ExceptionOr<void> setJustifySelf(StringImpl*);
    RefPtr<StringImpl> positionAnchor() const;
    ExceptionOr<void> setPositionAnchor(StringImpl*);
    RefPtr<StringImpl> positionArea() const;
    ExceptionOr<void> setPositionArea(StringImpl*);

private:
    CSSPositionTryDescriptors(MutableStyleProperties&, CSSPositionTryRule&);

    // Override this to invalidate all remembered last-successful position options
    // when property value changes.
    ExceptionOr<void> setPropertyInternal(CSSPropertyID, StringImpl* value, IsImportant) override;

    NODELETE StyleRuleType ruleType() const final;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_STYLE_DECLARATION(CSSPositionTryDescriptors, StyleDeclarationType::PositionTry)
