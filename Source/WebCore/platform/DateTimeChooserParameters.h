/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)

#include "IntRect.h"
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct DateTimeChooserParameters {
    AtomString type;
    IntRect anchorRectInRootView;
    // Locale name for which the chooser should be localized.
    // This might be an invalid name because it comes from HTML lang attributes.
    AtomString locale;
    String currentValue;
    Vector<String> suggestionValues;
    Vector<String> localizedSuggestionValues;
    Vector<String> suggestionLabels;
    double minimum { 0 };
    double maximum { 0 };
    double step { 0 };
    double stepBase { 0 };
    bool required { false };
    bool isAnchorElementRTL { false };
    bool useDarkAppearance { false };
    bool hasSecondField { false };
    bool hasMillisecondField { false };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<DateTimeChooserParameters> decode(Decoder&);
};

template<class Encoder>
void DateTimeChooserParameters::encode(Encoder& encoder) const
{
    encoder << type;
    encoder << anchorRectInRootView;
    encoder << locale;
    encoder << currentValue;
    encoder << suggestionValues;
    encoder << localizedSuggestionValues;
    encoder << suggestionLabels;
    encoder << minimum;
    encoder << maximum;
    encoder << step;
    encoder << stepBase;
    encoder << required;
    encoder << isAnchorElementRTL;
    encoder << useDarkAppearance;
    encoder << hasSecondField;
    encoder << hasMillisecondField;
}

template<class Decoder>
std::optional<DateTimeChooserParameters> DateTimeChooserParameters::decode(Decoder& decoder)
{
    AtomString type;
    if (!decoder.decode(type))
        return std::nullopt;

    IntRect anchorRectInRootView;
    if (!decoder.decode(anchorRectInRootView))
        return std::nullopt;

    AtomString locale;
    if (!decoder.decode(locale))
        return std::nullopt;

    String currentValue;
    if (!decoder.decode(currentValue))
        return std::nullopt;

    Vector<String> suggestionValues;
    if (!decoder.decode(suggestionValues))
        return std::nullopt;

    Vector<String> localizedSuggestionValues;
    if (!decoder.decode(localizedSuggestionValues))
        return std::nullopt;

    Vector<String> suggestionLabels;
    if (!decoder.decode(suggestionLabels))
        return std::nullopt;

    double minimum;
    if (!decoder.decode(minimum))
        return std::nullopt;

    double maximum;
    if (!decoder.decode(maximum))
        return std::nullopt;

    double step;
    if (!decoder.decode(step))
        return std::nullopt;

    double stepBase;
    if (!decoder.decode(stepBase))
        return std::nullopt;

    bool required;
    if (!decoder.decode(required))
        return std::nullopt;

    bool isAnchorElementRTL;
    if (!decoder.decode(isAnchorElementRTL))
        return std::nullopt;

    bool useDarkAppearance;
    if (!decoder.decode(useDarkAppearance))
        return std::nullopt;

    bool hasSecondField;
    if (!decoder.decode(hasSecondField))
        return std::nullopt;

    bool hasMillisecondField;
    if (!decoder.decode(hasMillisecondField))
        return std::nullopt;

    return {{ WTFMove(type), anchorRectInRootView, WTFMove(locale), WTFMove(currentValue), WTFMove(suggestionValues), WTFMove(localizedSuggestionValues), WTFMove(suggestionLabels), minimum, maximum, step, stepBase, required, isAnchorElementRTL, useDarkAppearance, hasSecondField, hasMillisecondField }};
}

} // namespace WebCore

#endif
