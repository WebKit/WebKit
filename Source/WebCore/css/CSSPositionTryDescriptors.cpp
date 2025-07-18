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
#include "ExceptionOr.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(CSSPositionTryDescriptors);

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
String CSSPositionTryDescriptors::margin() const
{
    return getPropertyValueInternal(CSSPropertyMargin);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMargin(const String& value)
{
    return setPropertyInternal(CSSPropertyMargin, value, IsImportant::No);
}

// @position-try 'margin-top'
String CSSPositionTryDescriptors::marginTop() const
{
    return getPropertyValueInternal(CSSPropertyMarginTop);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMarginTop(const String& value)
{
    return setPropertyInternal(CSSPropertyMarginTop, value, IsImportant::No);
}

// @position-try 'margin-right'
String CSSPositionTryDescriptors::marginRight() const
{
    return getPropertyValueInternal(CSSPropertyMarginRight);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMarginRight(const String& value)
{
    return setPropertyInternal(CSSPropertyMarginRight, value, IsImportant::No);
}

// @position-try 'margin-bottom'
String CSSPositionTryDescriptors::marginBottom() const
{
    return getPropertyValueInternal(CSSPropertyMarginBottom);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMarginBottom(const String& value)
{
    return setPropertyInternal(CSSPropertyMarginBottom, value, IsImportant::No);
}

// @position-try 'margin-left'
String CSSPositionTryDescriptors::marginLeft() const
{
    return getPropertyValueInternal(CSSPropertyMarginLeft);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMarginLeft(const String& value)
{
    return setPropertyInternal(CSSPropertyMarginLeft, value, IsImportant::No);
}

// @position-try 'margin-block'
String CSSPositionTryDescriptors::marginBlock() const
{
    return getPropertyValueInternal(CSSPropertyMarginBlock);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMarginBlock(const String& value)
{
    return setPropertyInternal(CSSPropertyMarginBlock, value, IsImportant::No);
}

// @position-try 'margin-block-start'
String CSSPositionTryDescriptors::marginBlockStart() const
{
    return getPropertyValueInternal(CSSPropertyMarginBlockStart);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMarginBlockStart(const String& value)
{
    return setPropertyInternal(CSSPropertyMarginBlockStart, value, IsImportant::No);
}

// @position-try 'margin-block-end'
String CSSPositionTryDescriptors::marginBlockEnd() const
{
    return getPropertyValueInternal(CSSPropertyMarginBlockEnd);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMarginBlockEnd(const String& value)
{
    return setPropertyInternal(CSSPropertyMarginBlockEnd, value, IsImportant::No);
}

// @position-try 'marginp-inline'
String CSSPositionTryDescriptors::marginInline() const
{
    return getPropertyValueInternal(CSSPropertyMarginInline);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMarginInline(const String& value)
{
    return setPropertyInternal(CSSPropertyMarginInline, value, IsImportant::No);
}

// @position-try 'margin-inline-start'
String CSSPositionTryDescriptors::marginInlineStart() const
{
    return getPropertyValueInternal(CSSPropertyMarginInlineStart);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMarginInlineStart(const String& value)
{
    return setPropertyInternal(CSSPropertyMarginInlineStart, value, IsImportant::No);
}

// @position-try 'margin-inline-end'
String CSSPositionTryDescriptors::marginInlineEnd() const
{
    return getPropertyValueInternal(CSSPropertyMarginInlineEnd);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMarginInlineEnd(const String& value)
{
    return setPropertyInternal(CSSPropertyMarginInlineEnd, value, IsImportant::No);
}

// @position-try 'inset'
String CSSPositionTryDescriptors::inset() const
{
    return getPropertyValueInternal(CSSPropertyInset);
}

ExceptionOr<void> CSSPositionTryDescriptors::setInset(const String& value)
{
    return setPropertyInternal(CSSPropertyInset, value, IsImportant::No);
}

// @position-try 'inset-block'
String CSSPositionTryDescriptors::insetBlock() const
{
    return getPropertyValueInternal(CSSPropertyInsetBlock);
}

ExceptionOr<void> CSSPositionTryDescriptors::setInsetBlock(const String& value)
{
    return setPropertyInternal(CSSPropertyInsetBlock, value, IsImportant::No);
}

// @position-try 'inset-block-start'
String CSSPositionTryDescriptors::insetBlockStart() const
{
    return getPropertyValueInternal(CSSPropertyInsetBlockStart);
}

ExceptionOr<void> CSSPositionTryDescriptors::setInsetBlockStart(const String& value)
{
    return setPropertyInternal(CSSPropertyInsetBlockStart, value, IsImportant::No);
}

// @position-try 'inset-block-end'
String CSSPositionTryDescriptors::insetBlockEnd() const
{
    return getPropertyValueInternal(CSSPropertyInsetBlockEnd);
}

ExceptionOr<void> CSSPositionTryDescriptors::setInsetBlockEnd(const String& value)
{
    return setPropertyInternal(CSSPropertyInsetBlockEnd, value, IsImportant::No);
}

// @position-try 'inset-inline'
String CSSPositionTryDescriptors::insetInline() const
{
    return getPropertyValueInternal(CSSPropertyInsetInline);
}

ExceptionOr<void> CSSPositionTryDescriptors::setInsetInline(const String& value)
{
    return setPropertyInternal(CSSPropertyInsetInline, value, IsImportant::No);
}

// @position-try 'inset-inline-start'
String CSSPositionTryDescriptors::insetInlineStart() const
{
    return getPropertyValueInternal(CSSPropertyInsetInlineStart);
}

ExceptionOr<void> CSSPositionTryDescriptors::setInsetInlineStart(const String& value)
{
    return setPropertyInternal(CSSPropertyInsetInlineStart, value, IsImportant::No);
}

// @position-try 'inset-inline-end'
String CSSPositionTryDescriptors::insetInlineEnd() const
{
    return getPropertyValueInternal(CSSPropertyInsetInlineEnd);
}

ExceptionOr<void> CSSPositionTryDescriptors::setInsetInlineEnd(const String& value)
{
    return setPropertyInternal(CSSPropertyInsetInlineEnd, value, IsImportant::No);
}

// @position-try 'top'
String CSSPositionTryDescriptors::top() const
{
    return getPropertyValueInternal(CSSPropertyTop);
}

ExceptionOr<void> CSSPositionTryDescriptors::setTop(const String& value)
{
    return setPropertyInternal(CSSPropertyTop, value, IsImportant::No);
}

// @position-try 'left'
String CSSPositionTryDescriptors::left() const
{
    return getPropertyValueInternal(CSSPropertyLeft);
}

ExceptionOr<void> CSSPositionTryDescriptors::setLeft(const String& value)
{
    return setPropertyInternal(CSSPropertyLeft, value, IsImportant::No);
}

// @position-try 'right'
String CSSPositionTryDescriptors::right() const
{
    return getPropertyValueInternal(CSSPropertyRight);
}

ExceptionOr<void> CSSPositionTryDescriptors::setRight(const String& value)
{
    return setPropertyInternal(CSSPropertyRight, value, IsImportant::No);
}

// @position-try 'bottom'
String CSSPositionTryDescriptors::bottom() const
{
    return getPropertyValueInternal(CSSPropertyBottom);
}

ExceptionOr<void> CSSPositionTryDescriptors::setBottom(const String& value)
{
    return setPropertyInternal(CSSPropertyBottom, value, IsImportant::No);
}

// @position-try 'width'
String CSSPositionTryDescriptors::width() const
{
    return getPropertyValueInternal(CSSPropertyWidth);
}

ExceptionOr<void> CSSPositionTryDescriptors::setWidth(const String& value)
{
    return setPropertyInternal(CSSPropertyWidth, value, IsImportant::No);
}

// @position-try 'min-width'
String CSSPositionTryDescriptors::minWidth() const
{
    return getPropertyValueInternal(CSSPropertyMinWidth);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMinWidth(const String& value)
{
    return setPropertyInternal(CSSPropertyMinWidth, value, IsImportant::No);
}

// @position-try 'max-width'
String CSSPositionTryDescriptors::maxWidth() const
{
    return getPropertyValueInternal(CSSPropertyMaxWidth);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMaxWidth(const String& value)
{
    return setPropertyInternal(CSSPropertyMaxWidth, value, IsImportant::No);
}

// @position-try 'height'
String CSSPositionTryDescriptors::height() const
{
    return getPropertyValueInternal(CSSPropertyHeight);
}

ExceptionOr<void> CSSPositionTryDescriptors::setHeight(const String& value)
{
    return setPropertyInternal(CSSPropertyHeight, value, IsImportant::No);
}

// @position-try 'min-height'
String CSSPositionTryDescriptors::minHeight() const
{
    return getPropertyValueInternal(CSSPropertyMinHeight);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMinHeight(const String& value)
{
    return setPropertyInternal(CSSPropertyMinHeight, value, IsImportant::No);
}

// @position-try 'max-height'
String CSSPositionTryDescriptors::maxHeight() const
{
    return getPropertyValueInternal(CSSPropertyMaxHeight);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMaxHeight(const String& value)
{
    return setPropertyInternal(CSSPropertyMaxHeight, value, IsImportant::No);
}

// @position-try 'block-size'
String CSSPositionTryDescriptors::blockSize() const
{
    return getPropertyValueInternal(CSSPropertyBlockSize);
}

ExceptionOr<void> CSSPositionTryDescriptors::setBlockSize(const String& value)
{
    return setPropertyInternal(CSSPropertyBlockSize, value, IsImportant::No);
}

// @position-try 'min-block-size'
String CSSPositionTryDescriptors::minBlockSize() const
{
    return getPropertyValueInternal(CSSPropertyMinBlockSize);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMinBlockSize(const String& value)
{
    return setPropertyInternal(CSSPropertyMinBlockSize, value, IsImportant::No);
}

// @position-try 'max-block-size'
String CSSPositionTryDescriptors::maxBlockSize() const
{
    return getPropertyValueInternal(CSSPropertyMaxBlockSize);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMaxBlockSize(const String& value)
{
    return setPropertyInternal(CSSPropertyMaxBlockSize, value, IsImportant::No);
}

// @position-try 'inline-size'
String CSSPositionTryDescriptors::inlineSize() const
{
    return getPropertyValueInternal(CSSPropertyInlineSize);
}

ExceptionOr<void> CSSPositionTryDescriptors::setInlineSize(const String& value)
{
    return setPropertyInternal(CSSPropertyInlineSize, value, IsImportant::No);
}

// @position-try 'min-inline-size'
String CSSPositionTryDescriptors::minInlineSize() const
{
    return getPropertyValueInternal(CSSPropertyMinInlineSize);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMinInlineSize(const String& value)
{
    return setPropertyInternal(CSSPropertyMinInlineSize, value, IsImportant::No);
}

// @position-try 'max-inline-size'
String CSSPositionTryDescriptors::maxInlineSize() const
{
    return getPropertyValueInternal(CSSPropertyMaxInlineSize);
}

ExceptionOr<void> CSSPositionTryDescriptors::setMaxInlineSize(const String& value)
{
    return setPropertyInternal(CSSPropertyMaxInlineSize, value, IsImportant::No);
}

// @position-try 'place-self'
String CSSPositionTryDescriptors::placeSelf() const
{
    return getPropertyValueInternal(CSSPropertyPlaceSelf);
}

ExceptionOr<void> CSSPositionTryDescriptors::setPlaceSelf(const String& value)
{
    return setPropertyInternal(CSSPropertyPlaceSelf, value, IsImportant::No);
}

// @position-try 'align-self'
String CSSPositionTryDescriptors::alignSelf() const
{
    return getPropertyValueInternal(CSSPropertyAlignSelf);
}

ExceptionOr<void> CSSPositionTryDescriptors::setAlignSelf(const String& value)
{
    return setPropertyInternal(CSSPropertyAlignSelf, value, IsImportant::No);
}

// @position-try 'justify-self'
String CSSPositionTryDescriptors::justifySelf() const
{
    return getPropertyValueInternal(CSSPropertyJustifySelf);
}

ExceptionOr<void> CSSPositionTryDescriptors::setJustifySelf(const String& value)
{
    return setPropertyInternal(CSSPropertyJustifySelf, value, IsImportant::No);
}

// @position-try 'position-anchor'
String CSSPositionTryDescriptors::positionAnchor() const
{
    return getPropertyValueInternal(CSSPropertyPositionAnchor);
}

ExceptionOr<void> CSSPositionTryDescriptors::setPositionAnchor(const String& value)
{
    return setPropertyInternal(CSSPropertyPositionAnchor, value, IsImportant::No);
}

// @position-try 'position-area'
String CSSPositionTryDescriptors::positionArea() const
{
    return getPropertyValueInternal(CSSPropertyPositionArea);
}

ExceptionOr<void> CSSPositionTryDescriptors::setPositionArea(const String& value)
{
    return setPropertyInternal(CSSPropertyPositionArea, value, IsImportant::No);
}

} // namespace WebCore
