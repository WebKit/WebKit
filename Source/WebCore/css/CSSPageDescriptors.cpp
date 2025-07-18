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

#include "config.h"
#include "CSSPageDescriptors.h"

#include "CSSPageRule.h"
#include "ExceptionOr.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(CSSPageDescriptors);

CSSPageDescriptors::CSSPageDescriptors(MutableStyleProperties& propertySet, CSSPageRule& parentRule)
    : PropertySetCSSDescriptors(propertySet, parentRule)
{
}

CSSPageDescriptors::~CSSPageDescriptors() = default;

StyleRuleType CSSPageDescriptors::ruleType() const
{
    return StyleRuleType::Page;
}

// MARK: - Descriptors

// 'margin'
String CSSPageDescriptors::margin() const
{
    return getPropertyValueInternal(CSSPropertyMargin);
}

ExceptionOr<void> CSSPageDescriptors::setMargin(const String& value)
{
    return setPropertyInternal(CSSPropertyMargin, value, IsImportant::No);
}

// 'margin-top'
String CSSPageDescriptors::marginTop() const
{
    return getPropertyValueInternal(CSSPropertyMarginTop);
}

ExceptionOr<void> CSSPageDescriptors::setMarginTop(const String& value)
{
    return setPropertyInternal(CSSPropertyMarginTop, value, IsImportant::No);
}

// 'margin-right'
String CSSPageDescriptors::marginRight() const
{
    return getPropertyValueInternal(CSSPropertyMarginRight);
}

ExceptionOr<void> CSSPageDescriptors::setMarginRight(const String& value)
{
    return setPropertyInternal(CSSPropertyMarginRight, value, IsImportant::No);
}

// 'margin-bottom'
String CSSPageDescriptors::marginBottom() const
{
    return getPropertyValueInternal(CSSPropertyMarginBottom);
}

ExceptionOr<void> CSSPageDescriptors::setMarginBottom(const String& value)
{
    return setPropertyInternal(CSSPropertyMarginBottom, value, IsImportant::No);
}

// 'margin-left'
String CSSPageDescriptors::marginLeft() const
{
    return getPropertyValueInternal(CSSPropertyMarginLeft);
}

ExceptionOr<void> CSSPageDescriptors::setMarginLeft(const String& value)
{
    return setPropertyInternal(CSSPropertyMarginLeft, value, IsImportant::No);
}

// @page 'size'
String CSSPageDescriptors::size() const
{
    return getPropertyValueInternal(CSSPropertySize);
}

ExceptionOr<void> CSSPageDescriptors::setSize(const String& value)
{
    return setPropertyInternal(CSSPropertySize, value, IsImportant::No);
}

} // namespace WebCore
