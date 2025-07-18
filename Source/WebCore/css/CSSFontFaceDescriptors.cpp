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
#include "CSSFontFaceDescriptors.h"
#include "ExceptionOr.h"

#include "CSSFontFaceRule.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(CSSFontFaceDescriptors);

CSSFontFaceDescriptors::CSSFontFaceDescriptors(MutableStyleProperties& propertySet, CSSFontFaceRule& parentRule)
    : PropertySetCSSDescriptors(propertySet, parentRule)
{
}

CSSFontFaceDescriptors::~CSSFontFaceDescriptors() = default;

StyleRuleType CSSFontFaceDescriptors::ruleType() const
{
    return StyleRuleType::FontFace;
}

// MARK: - Descriptors

// @font-face 'src'
String CSSFontFaceDescriptors::src() const
{
    return getPropertyValueInternal(CSSPropertySrc);
}

ExceptionOr<void> CSSFontFaceDescriptors::setSrc(const String& value)
{
    return setPropertyInternal(CSSPropertySrc, value, IsImportant::No);
}

// @font-face 'fontFamily'
String CSSFontFaceDescriptors::fontFamily() const
{
    return getPropertyValueInternal(CSSPropertyFontFamily);
}

ExceptionOr<void> CSSFontFaceDescriptors::setFontFamily(const String& value)
{
    return setPropertyInternal(CSSPropertyFontFamily, value, IsImportant::No);
}

// @font-face 'font-style'
String CSSFontFaceDescriptors::fontStyle() const
{
    return getPropertyValueInternal(CSSPropertyFontStyle);
}

ExceptionOr<void> CSSFontFaceDescriptors::setFontStyle(const String& value)
{
    return setPropertyInternal(CSSPropertyFontStyle, value, IsImportant::No);
}

// @font-face 'font-weight'
String CSSFontFaceDescriptors::fontWeight() const
{
    return getPropertyValueInternal(CSSPropertyFontWeight);
}

ExceptionOr<void> CSSFontFaceDescriptors::setFontWeight(const String& value)
{
    return setPropertyInternal(CSSPropertyFontWeight, value, IsImportant::No);
}

// @font-face 'font-stretch'
String CSSFontFaceDescriptors::fontStretch() const
{
    return getPropertyValueInternal(CSSPropertyFontWidth); // NOTE: 'font-stretch' is an alias for 'font-width'.
}

ExceptionOr<void> CSSFontFaceDescriptors::setFontStretch(const String& value)
{
    return setPropertyInternal(CSSPropertyFontWidth, value, IsImportant::No); // NOTE: 'font-stretch' is an alias for 'font-width'.
}

// @font-face 'font-width'
String CSSFontFaceDescriptors::fontWidth() const
{
    return getPropertyValueInternal(CSSPropertyFontWidth);
}

ExceptionOr<void> CSSFontFaceDescriptors::setFontWidth(const String& value)
{
    return setPropertyInternal(CSSPropertyFontWidth, value, IsImportant::No);
}

// @font-face 'size-adjust'
String CSSFontFaceDescriptors::sizeAdjust() const
{
    return getPropertyValueInternal(CSSPropertySizeAdjust);
}

ExceptionOr<void> CSSFontFaceDescriptors::setSizeAdjust(const String& value)
{
    return setPropertyInternal(CSSPropertySizeAdjust, value, IsImportant::No);
}

// @font-face 'unicode-range'
String CSSFontFaceDescriptors::unicodeRange() const
{
    return getPropertyValueInternal(CSSPropertyUnicodeRange);
}

ExceptionOr<void> CSSFontFaceDescriptors::setUnicodeRange(const String& value)
{
    return setPropertyInternal(CSSPropertyUnicodeRange, value, IsImportant::No);
}

// @font-face 'font-feature-settings'
String CSSFontFaceDescriptors::fontFeatureSettings() const
{
    return getPropertyValueInternal(CSSPropertyFontFeatureSettings);
}

ExceptionOr<void> CSSFontFaceDescriptors::setFontFeatureSettings(const String& value)
{
    return setPropertyInternal(CSSPropertyFontFeatureSettings, value, IsImportant::No);
}

// @font-face 'font-display'
String CSSFontFaceDescriptors::fontDisplay() const
{
    return getPropertyValueInternal(CSSPropertyFontDisplay);
}

ExceptionOr<void> CSSFontFaceDescriptors::setFontDisplay(const String& value)
{
    return setPropertyInternal(CSSPropertyFontDisplay, value, IsImportant::No);
}

} // namespace WebCore
