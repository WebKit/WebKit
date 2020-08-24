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
    double minimum;
    double maximum;
    double step;
    double stepBase;
    bool required;
    bool isAnchorElementRTL;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<DateTimeChooserParameters> decode(Decoder&);
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
}

template<class Decoder>
Optional<DateTimeChooserParameters> DateTimeChooserParameters::decode(Decoder& decoder)
{
    AtomString type;
    if (!decoder.decode(type))
        return WTF::nullopt;

    IntRect anchorRectInRootView;
    if (!decoder.decode(anchorRectInRootView))
        return WTF::nullopt;

    AtomString locale;
    if (!decoder.decode(locale))
        return WTF::nullopt;

    String currentValue;
    if (!decoder.decode(currentValue))
        return WTF::nullopt;

    Vector<String> suggestionValues;
    if (!decoder.decode(suggestionValues))
        return WTF::nullopt;

    Vector<String> localizedSuggestionValues;
    if (!decoder.decode(localizedSuggestionValues))
        return WTF::nullopt;

    Vector<String> suggestionLabels;
    if (!decoder.decode(suggestionLabels))
        return WTF::nullopt;

    double minimum;
    if (!decoder.decode(minimum))
        return WTF::nullopt;

    double maximum;
    if (!decoder.decode(maximum))
        return WTF::nullopt;

    double step;
    if (!decoder.decode(step))
        return WTF::nullopt;

    double stepBase;
    if (!decoder.decode(stepBase))
        return WTF::nullopt;

    bool required;
    if (!decoder.decode(required))
        return WTF::nullopt;

    bool isAnchorElementRTL;
    if (!decoder.decode(isAnchorElementRTL))
        return WTF::nullopt;

    return {{ WTFMove(type), anchorRectInRootView, WTFMove(locale), WTFMove(currentValue), WTFMove(suggestionValues), WTFMove(localizedSuggestionValues), WTFMove(suggestionLabels), minimum, maximum, step, stepBase, required, isAnchorElementRTL }};
}

} // namespace WebCore

#endif
