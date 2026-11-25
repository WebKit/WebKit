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
 * PROFITS;OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSPositionTryDescriptors.h"

#include "CSSPositionTryRule.h"
#include "CSSStyleSheet.h"
#include "ExceptionOr.h"
#include "StyleDocumentScope.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CSSPositionTryDescriptors);

CSSPositionTryDescriptors::CSSPositionTryDescriptors(MutableStyleProperties& propertySet, CSSPositionTryRule& parentRule)
    : PropertySetCSSDescriptors(propertySet, parentRule)
{
}

CSSPositionTryDescriptors::~CSSPositionTryDescriptors() = default;

StyleRuleType CSSPositionTryDescriptors::ruleType() const
{
    return StyleRuleType::PositionTry;
}

// MARK: - Descriptors

// @position-try 'margin'
RefPtr<StringImpl> CSSPositionTryDescriptors::margin() const
{
    return getPropertyValueInternal(CSSPropertyID::Margin);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMargin(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::Margin, value, IsImportant::No);
}

// @position-try 'margin-top'
RefPtr<StringImpl> CSSPositionTryDescriptors::marginTop() const
{
    return getPropertyValueInternal(CSSPropertyID::MarginTop);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMarginTop(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::MarginTop, value, IsImportant::No);
}

// @position-try 'margin-right'
RefPtr<StringImpl> CSSPositionTryDescriptors::marginRight() const
{
    return getPropertyValueInternal(CSSPropertyID::MarginRight);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMarginRight(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::MarginRight, value, IsImportant::No);
}

// @position-try 'margin-bottom'
RefPtr<StringImpl> CSSPositionTryDescriptors::marginBottom() const
{
    return getPropertyValueInternal(CSSPropertyID::MarginBottom);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMarginBottom(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::MarginBottom, value, IsImportant::No);
}

// @position-try 'margin-left'
RefPtr<StringImpl> CSSPositionTryDescriptors::marginLeft() const
{
    return getPropertyValueInternal(CSSPropertyID::MarginLeft);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMarginLeft(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::MarginLeft, value, IsImportant::No);
}

// @position-try 'margin-block'
RefPtr<StringImpl> CSSPositionTryDescriptors::marginBlock() const
{
    return getPropertyValueInternal(CSSPropertyID::MarginBlock);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMarginBlock(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::MarginBlock, value, IsImportant::No);
}

// @position-try 'margin-block-start'
RefPtr<StringImpl> CSSPositionTryDescriptors::marginBlockStart() const
{
    return getPropertyValueInternal(CSSPropertyID::MarginBlockStart);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMarginBlockStart(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::MarginBlockStart, value, IsImportant::No);
}

// @position-try 'margin-block-end'
RefPtr<StringImpl> CSSPositionTryDescriptors::marginBlockEnd() const
{
    return getPropertyValueInternal(CSSPropertyID::MarginBlockEnd);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMarginBlockEnd(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::MarginBlockEnd, value, IsImportant::No);
}

// @position-try 'marginp-inline'
RefPtr<StringImpl> CSSPositionTryDescriptors::marginInline() const
{
    return getPropertyValueInternal(CSSPropertyID::MarginInline);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMarginInline(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::MarginInline, value, IsImportant::No);
}

// @position-try 'margin-inline-start'
RefPtr<StringImpl> CSSPositionTryDescriptors::marginInlineStart() const
{
    return getPropertyValueInternal(CSSPropertyID::MarginInlineStart);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMarginInlineStart(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::MarginInlineStart, value, IsImportant::No);
}

// @position-try 'margin-inline-end'
RefPtr<StringImpl> CSSPositionTryDescriptors::marginInlineEnd() const
{
    return getPropertyValueInternal(CSSPropertyID::MarginInlineEnd);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMarginInlineEnd(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::MarginInlineEnd, value, IsImportant::No);
}

// @position-try 'inset'
RefPtr<StringImpl> CSSPositionTryDescriptors::inset() const
{
    return getPropertyValueInternal(CSSPropertyID::Inset);
}

ExceptionOr<void> CSSPositionTryDescriptors::setInset(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::Inset, value, IsImportant::No);
}

// @position-try 'inset-block'
RefPtr<StringImpl> CSSPositionTryDescriptors::insetBlock() const
{
    return getPropertyValueInternal(CSSPropertyID::InsetBlock);
}

ExceptionOr<void> CSSPositionTryDescriptors::setInsetBlock(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::InsetBlock, value, IsImportant::No);
}

// @position-try 'inset-block-start'
RefPtr<StringImpl> CSSPositionTryDescriptors::insetBlockStart() const
{
    return getPropertyValueInternal(CSSPropertyID::InsetBlockStart);
}

ExceptionOr<void> CSSPositionTryDescriptors::setInsetBlockStart(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::InsetBlockStart, value, IsImportant::No);
}

// @position-try 'inset-block-end'
RefPtr<StringImpl> CSSPositionTryDescriptors::insetBlockEnd() const
{
    return getPropertyValueInternal(CSSPropertyID::InsetBlockEnd);
}

ExceptionOr<void> CSSPositionTryDescriptors::setInsetBlockEnd(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::InsetBlockEnd, value, IsImportant::No);
}

// @position-try 'inset-inline'
RefPtr<StringImpl> CSSPositionTryDescriptors::insetInline() const
{
    return getPropertyValueInternal(CSSPropertyID::InsetInline);
}

ExceptionOr<void> CSSPositionTryDescriptors::setInsetInline(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::InsetInline, value, IsImportant::No);
}

// @position-try 'inset-inline-start'
RefPtr<StringImpl> CSSPositionTryDescriptors::insetInlineStart() const
{
    return getPropertyValueInternal(CSSPropertyID::InsetInlineStart);
}

ExceptionOr<void> CSSPositionTryDescriptors::setInsetInlineStart(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::InsetInlineStart, value, IsImportant::No);
}

// @position-try 'inset-inline-end'
RefPtr<StringImpl> CSSPositionTryDescriptors::insetInlineEnd() const
{
    return getPropertyValueInternal(CSSPropertyID::InsetInlineEnd);
}

ExceptionOr<void> CSSPositionTryDescriptors::setInsetInlineEnd(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::InsetInlineEnd, value, IsImportant::No);
}

// @position-try 'top'
RefPtr<StringImpl> CSSPositionTryDescriptors::top() const
{
    return getPropertyValueInternal(CSSPropertyID::Top);
}

ExceptionOr<void> CSSPositionTryDescriptors::setTop(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::Top, value, IsImportant::No);
}

// @position-try 'left'
RefPtr<StringImpl> CSSPositionTryDescriptors::left() const
{
    return getPropertyValueInternal(CSSPropertyID::Left);
}

ExceptionOr<void> CSSPositionTryDescriptors::setLeft(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::Left, value, IsImportant::No);
}

// @position-try 'right'
RefPtr<StringImpl> CSSPositionTryDescriptors::right() const
{
    return getPropertyValueInternal(CSSPropertyID::Right);
}

ExceptionOr<void> CSSPositionTryDescriptors::setRight(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::Right, value, IsImportant::No);
}

// @position-try 'bottom'
RefPtr<StringImpl> CSSPositionTryDescriptors::bottom() const
{
    return getPropertyValueInternal(CSSPropertyID::Bottom);
}

ExceptionOr<void> CSSPositionTryDescriptors::setBottom(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::Bottom, value, IsImportant::No);
}

// @position-try 'width'
RefPtr<StringImpl> CSSPositionTryDescriptors::width() const
{
    return getPropertyValueInternal(CSSPropertyID::Width);
}

ExceptionOr<void> CSSPositionTryDescriptors::setWidth(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::Width, value, IsImportant::No);
}

// @position-try 'min-width'
RefPtr<StringImpl> CSSPositionTryDescriptors::minWidth() const
{
    return getPropertyValueInternal(CSSPropertyID::MinWidth);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMinWidth(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::MinWidth, value, IsImportant::No);
}

// @position-try 'max-width'
RefPtr<StringImpl> CSSPositionTryDescriptors::maxWidth() const
{
    return getPropertyValueInternal(CSSPropertyID::MaxWidth);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMaxWidth(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::MaxWidth, value, IsImportant::No);
}

// @position-try 'height'
RefPtr<StringImpl> CSSPositionTryDescriptors::height() const
{
    return getPropertyValueInternal(CSSPropertyID::Height);
}

ExceptionOr<void> CSSPositionTryDescriptors::setHeight(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::Height, value, IsImportant::No);
}

// @position-try 'min-height'
RefPtr<StringImpl> CSSPositionTryDescriptors::minHeight() const
{
    return getPropertyValueInternal(CSSPropertyID::MinHeight);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMinHeight(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::MinHeight, value, IsImportant::No);
}

// @position-try 'max-height'
RefPtr<StringImpl> CSSPositionTryDescriptors::maxHeight() const
{
    return getPropertyValueInternal(CSSPropertyID::MaxHeight);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMaxHeight(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::MaxHeight, value, IsImportant::No);
}

// @position-try 'block-size'
RefPtr<StringImpl> CSSPositionTryDescriptors::blockSize() const
{
    return getPropertyValueInternal(CSSPropertyID::BlockSize);
}

ExceptionOr<void> CSSPositionTryDescriptors::setBlockSize(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::BlockSize, value, IsImportant::No);
}

// @position-try 'min-block-size'
RefPtr<StringImpl> CSSPositionTryDescriptors::minBlockSize() const
{
    return getPropertyValueInternal(CSSPropertyID::MinBlockSize);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMinBlockSize(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::MinBlockSize, value, IsImportant::No);
}

// @position-try 'max-block-size'
RefPtr<StringImpl> CSSPositionTryDescriptors::maxBlockSize() const
{
    return getPropertyValueInternal(CSSPropertyID::MaxBlockSize);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMaxBlockSize(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::MaxBlockSize, value, IsImportant::No);
}

// @position-try 'inline-size'
RefPtr<StringImpl> CSSPositionTryDescriptors::inlineSize() const
{
    return getPropertyValueInternal(CSSPropertyID::InlineSize);
}

ExceptionOr<void> CSSPositionTryDescriptors::setInlineSize(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::InlineSize, value, IsImportant::No);
}

// @position-try 'min-inline-size'
RefPtr<StringImpl> CSSPositionTryDescriptors::minInlineSize() const
{
    return getPropertyValueInternal(CSSPropertyID::MinInlineSize);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMinInlineSize(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::MinInlineSize, value, IsImportant::No);
}

// @position-try 'max-inline-size'
RefPtr<StringImpl> CSSPositionTryDescriptors::maxInlineSize() const
{
    return getPropertyValueInternal(CSSPropertyID::MaxInlineSize);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMaxInlineSize(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::MaxInlineSize, value, IsImportant::No);
}

// @position-try 'place-self'
RefPtr<StringImpl> CSSPositionTryDescriptors::placeSelf() const
{
    return getPropertyValueInternal(CSSPropertyID::PlaceSelf);
}

ExceptionOr<void> CSSPositionTryDescriptors::setPlaceSelf(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::PlaceSelf, value, IsImportant::No);
}

// @position-try 'align-self'
RefPtr<StringImpl> CSSPositionTryDescriptors::alignSelf() const
{
    return getPropertyValueInternal(CSSPropertyID::AlignSelf);
}

ExceptionOr<void> CSSPositionTryDescriptors::setAlignSelf(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::AlignSelf, value, IsImportant::No);
}

// @position-try 'justify-self'
RefPtr<StringImpl> CSSPositionTryDescriptors::justifySelf() const
{
    return getPropertyValueInternal(CSSPropertyID::JustifySelf);
}

ExceptionOr<void> CSSPositionTryDescriptors::setJustifySelf(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::JustifySelf, value, IsImportant::No);
}

// @position-try 'position-anchor'
RefPtr<StringImpl> CSSPositionTryDescriptors::positionAnchor() const
{
    return getPropertyValueInternal(CSSPropertyID::PositionAnchor);
}

ExceptionOr<void> CSSPositionTryDescriptors::setPositionAnchor(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::PositionAnchor, value, IsImportant::No);
}

// @position-try 'position-area'
RefPtr<StringImpl> CSSPositionTryDescriptors::positionArea() const
{
    return getPropertyValueInternal(CSSPropertyID::PositionArea);
}

ExceptionOr<void> CSSPositionTryDescriptors::setPositionArea(StringImpl* value)
{
    return setPropertyInternal(CSSPropertyID::PositionArea, value, IsImportant::No);
}

ExceptionOr<void> CSSPositionTryDescriptors::setPropertyInternal(CSSPropertyID propertyID, StringImpl* value, IsImportant isImportant)
{
    auto result = PropertySetCSSDescriptors::setPropertyInternal(propertyID, value, isImportant);

    auto invalidateAllRememberedLastSuccessfulPositionOptions = [&] () {
        RefPtr strongParentRule = m_parentRule.get();
        ASSERT(strongParentRule);
        if (!strongParentRule)
            return;

        RefPtr parentStyleSheet = strongParentRule->parentStyleSheet();
        ASSERT(parentStyleSheet);
        if (!parentStyleSheet)
            return;

        if (CheckedPtr parentStyleSheetScope = parentStyleSheet->styleScope()) {
            // FIXME: only invalidate elements that use this position-try rule in position-try-fallbacks
            protect(parentStyleSheetScope->documentScope())->setLastSuccessfulPositionOptionIndexMap({ });
        }
    };

    invalidateAllRememberedLastSuccessfulPositionOptions();

    return result;
}

} // namespace WebCore
