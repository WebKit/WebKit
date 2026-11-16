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

WTF_MAKE_TZONE_ALLOCATED_IMPL(CSSFontFaceDescriptors);

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
    return getPropertyValueInternal(CSSPropertyID::Src);
}

ExceptionOr<void> CSSFontFaceDescriptors::setSrc(const String& value)
{
    return setPropertyInternal(CSSPropertyID::Src, value, IsImportant::No);
}

// @font-face 'fontFamily'
String CSSFontFaceDescriptors::fontFamily() const
{
    return getPropertyValueInternal(CSSPropertyID::FontFamily);
}

ExceptionOr<void> CSSFontFaceDescriptors::setFontFamily(const String& value)
{
    return setPropertyInternal(CSSPropertyID::FontFamily, value, IsImportant::No);
}

// @font-face 'font-style'
String CSSFontFaceDescriptors::fontStyle() const
{
    return getPropertyValueInternal(CSSPropertyID::FontStyle);
}

ExceptionOr<void> CSSFontFaceDescriptors::setFontStyle(const String& value)
{
    return setPropertyInternal(CSSPropertyID::FontStyle, value, IsImportant::No);
}

// @font-face 'font-weight'
String CSSFontFaceDescriptors::fontWeight() const
{
    return getPropertyValueInternal(CSSPropertyID::FontWeight);
}

ExceptionOr<void> CSSFontFaceDescriptors::setFontWeight(const String& value)
{
    return setPropertyInternal(CSSPropertyID::FontWeight, value, IsImportant::No);
}

// @font-face 'font-stretch'
String CSSFontFaceDescriptors::fontStretch() const
{
    return getPropertyValueInternal(CSSPropertyID::FontWidth); // NOTE: 'font-stretch' is an alias for 'font-width'.
}

ExceptionOr<void> CSSFontFaceDescriptors::setFontStretch(const String& value)
{
    return setPropertyInternal(CSSPropertyID::FontWidth, value, IsImportant::No); // NOTE: 'font-stretch' is an alias for 'font-width'.
}

// @font-face 'font-width'
String CSSFontFaceDescriptors::fontWidth() const
{
    return getPropertyValueInternal(CSSPropertyID::FontWidth);
}

ExceptionOr<void> CSSFontFaceDescriptors::setFontWidth(const String& value)
{
    return setPropertyInternal(CSSPropertyID::FontWidth, value, IsImportant::No);
}

// @font-face 'size-adjust'
String CSSFontFaceDescriptors::sizeAdjust() const
{
    return getPropertyValueInternal(CSSPropertyID::SizeAdjust);
}

ExceptionOr<void> CSSFontFaceDescriptors::setSizeAdjust(const String& value)
{
    return setPropertyInternal(CSSPropertyID::SizeAdjust, value, IsImportant::No);
}

// @font-face 'unicode-range'
String CSSFontFaceDescriptors::unicodeRange() const
{
    return getPropertyValueInternal(CSSPropertyID::UnicodeRange);
}

ExceptionOr<void> CSSFontFaceDescriptors::setUnicodeRange(const String& value)
{
    return setPropertyInternal(CSSPropertyID::UnicodeRange, value, IsImportant::No);
}

// @font-face 'font-feature-settings'
String CSSFontFaceDescriptors::fontFeatureSettings() const
{
    return getPropertyValueInternal(CSSPropertyID::FontFeatureSettings);
}

ExceptionOr<void> CSSFontFaceDescriptors::setFontFeatureSettings(const String& value)
{
    return setPropertyInternal(CSSPropertyID::FontFeatureSettings, value, IsImportant::No);
}

// @font-face 'font-display'
String CSSFontFaceDescriptors::fontDisplay() const
{
    return getPropertyValueInternal(CSSPropertyID::FontDisplay);
}

ExceptionOr<void> CSSFontFaceDescriptors::setFontDisplay(const String& value)
{
    return setPropertyInternal(CSSPropertyID::FontDisplay, value, IsImportant::No);
}

// @font-face 'ascent-override'
String CSSFontFaceDescriptors::ascentOverride() const
{
    return getPropertyValueInternal(CSSPropertyID::AscentOverride);
}

ExceptionOr<void> CSSFontFaceDescriptors::setAscentOverride(const String& value)
{
    return setPropertyInternal(CSSPropertyID::AscentOverride, value, IsImportant::No);
}

// @font-face 'descent-override'
String CSSFontFaceDescriptors::descentOverride() const
{
    return getPropertyValueInternal(CSSPropertyID::DescentOverride);
}

ExceptionOr<void> CSSFontFaceDescriptors::setDescentOverride(const String& value)
{
    return setPropertyInternal(CSSPropertyID::DescentOverride, value, IsImportant::No);
}

// @font-face 'line-gap-override'
String CSSFontFaceDescriptors::lineGapOverride() const
{
    return getPropertyValueInternal(CSSPropertyID::LineGapOverride);
}

ExceptionOr<void> CSSFontFaceDescriptors::setLineGapOverride(const String& value)
{
    return setPropertyInternal(CSSPropertyID::LineGapOverride, value, IsImportant::No);
}

} // namespace WebCore
